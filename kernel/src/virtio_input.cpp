// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <span>
#include <atomic>
#include <optional>
#include <string_view>
#include <stddef.h>
#include <stdint.h>

import nt;
import print;
import format;
import pci;
import virtio;
import list;
import module_init;
import mem;
import thread;
import handle;
import object_ref;
import evdev_device;
import dma;
import evdev;
import moxnox_ioctls;
import vec;

using namespace std;

static const uint64_t SUPPORTED_FEATURES = 0;

struct virtio_input_event {
    evdev_type type;
    evdev_code code;
    int32_t value;
} __attribute((packed));

static_assert(sizeof(virtio_input_event) == 8);

enum class virtio_input_config_select : uint8_t {
    UNSET = 0x00,
    ID_NAME = 0x01,
    ID_SERIAL = 0x02,
    ID_DEVIDS = 0x03,
    PROP_BITS = 0x10,
    EV_BITS = 0x11,
    ABS_INFO = 0x12,
};

struct virtio_input_absinfo {
    int32_t min;
    int32_t max;
    int32_t fuzz;
    int32_t flat;
    int32_t res;
};

static_assert(sizeof(virtio_input_absinfo) == 20);

struct virtio_input_devids {
    int16_t bustype;
    int16_t vendor;
    int16_t product;
    int16_t version;
};

static_assert(sizeof(virtio_input_devids) == 8);

struct virtio_input_config {
    virtio_input_config_select select;
    uint8_t subsel;
    uint8_t size;
    uint8_t reserved[5];
    union {
        char string[128];
        uint8_t bitmap[128];
        struct virtio_input_absinfo abs;
        struct virtio_input_devids ids;
    };
};

static_assert(sizeof(virtio_input_config) == 136);

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

static void found_virtio_pci_input(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1052, true, found_virtio_pci_input },
};

static bool found_virtio_mmio_input(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 18, SUPPORTED_FEATURES, 2, found_virtio_mmio_input
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_input(init_virtio_pci);

template<virtio_device T>
class virtio_input : public evdev_device {
public:
    virtio_input(T& t);

    ~virtio_input() {
        if (kc)
            kc->unregister();

        delete &dev;
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written,
                    ACCESS_MASK access_mask) override;

    void recv_thread();

    T& dev;
    evdev_context* kc = nullptr;

private:
    void init(volatile virtio_input_config& cfg);
    nt_status ioctl_get_absinfo(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                                uintptr_t& written);

    virt_queue* event_queue = nullptr;
    virt_queue* status_queue = nullptr;
    optional<input_absinfo> absinfo_x, absinfo_y, absinfo_wheel;
};

static atomic<uint32_t> virtio_input_num = 0;

template<virtio_device T>
virtio_input<T>::virtio_input(T& dev) : dev(dev) {
    if constexpr (is_same_v<T, virtio_pci_device>) {
        if (dev.device_cfg->length < sizeof(virtio_input_config)) {
            print("virtio_input: device cfg length was {}, expected at least {}\n",
                  +dev.device_cfg->length, sizeof(virtio_input_config));
            return;
        }
    }

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            event_queue = &vq;
        else if (vq.num == 1)
            status_queue = &vq;
    }

    if constexpr (is_same_v<T, virtio_pci_device>) {
        init(*(virtio_input_config*)(dev.dev.bar[dev.device_cfg->bar].data() + dev.device_cfg->offset));
    } else if constexpr (is_same_v<T, virtio_mmio_device>) {
        init(*(virtio_input_config*)dev.config);
    }

    this->acquire();

    auto t = add_kernel_thread([](void* ctx) {
        auto& dev = *(virtio_input<T>*)ctx;

        dev.recv_thread();
        dev.release();
    }, this);

    if (!t) {
        print("add_kernel_thread failed\n");
        this->release();
        return;
    }

    this->kc = register_evdev_device(*this);
}

template<virtio_device T>
void virtio_input<T>::init(volatile virtio_input_config& cfg) {
    cfg.select = virtio_input_config_select::EV_BITS;
    cfg.subsel = (uint8_t)evdev_type::EV_ABS;

    uint8_t size = cfg.size;

    if (size == 0)
        return;

    auto is_bit_set = [&](uint8_t num) -> bool {
        if (num / 8 >= size)
            return false;

        return cfg.bitmap[num / 8] & (1 << (num % 8));
    };

    bool has_abs_x = is_bit_set((uint8_t)evdev_abs_code::ABS_X);
    bool has_abs_y = is_bit_set((uint8_t)evdev_abs_code::ABS_Y);
    bool has_abs_wheel = is_bit_set((uint8_t)evdev_abs_code::ABS_WHEEL);

    auto read_absinfo = [&](evdev_abs_code code, optional<input_absinfo>& buf) {
        cfg.select = virtio_input_config_select::ABS_INFO;
        cfg.subsel = (uint8_t)evdev_abs_code::ABS_X;

        size = cfg.size;

        if (size < sizeof(virtio_input_absinfo)) {
            print("virtio_input: absinfo for {} was {} bytes, expected {}\n",
                  code, size, sizeof(virtio_input_absinfo));
            return;
        }

        buf = input_absinfo{};
        buf->value = 0;
        buf->minimum = cfg.abs.min;
        buf->maximum = cfg.abs.max;
        buf->fuzz = cfg.abs.fuzz;
        buf->flat = cfg.abs.flat;
        buf->resolution = cfg.abs.res;
    };

    if (has_abs_x)
        read_absinfo(evdev_abs_code::ABS_X, absinfo_x);

    if (has_abs_y)
        read_absinfo(evdev_abs_code::ABS_Y, absinfo_y);

    if (has_abs_wheel)
        read_absinfo(evdev_abs_code::ABS_WHEEL, absinfo_wheel);
}

template<virtio_device T>
void virtio_input<T>::recv_thread() {
    // FIXME - die gracefully if device ever goes away

    static const size_t BUFFER_LENGTH = 16;

    virtio_input_event evs[BUFFER_LENGTH];
    uint8_t* bounce[BUFFER_LENGTH];
    uint8_t slot[BUFFER_LENGTH];
    uintptr_t pa[BUFFER_LENGTH];
    size_t idx = 0;

    for (size_t i = 0; i < BUFFER_LENGTH; i++) {
        pa[i] = dma_setup(span((uint8_t*)&evs[i], sizeof(evs[i])),
                          false, bounce[i]);
        if (!pa[i]) {
            print("dma_setup failed\n");

            for (size_t j = 0; j < i; j++) {
                dma_finish_read(span((uint8_t*)&evs[j], sizeof(evs[j])),
                                bounce[j]);
            }

            return;
        }

        flush_cache(span((uint8_t*)phys_address(pa[i]), sizeof(virtio_input_event)),
                    false);

        slot[i] = event_queue->async_read(pa[i], sizeof(virtio_input_event));
    }

    while (true) {
        auto len = event_queue->wait_read(slot[idx]);

        if (len >= sizeof(virtio_input_event)) {
            if (bounce[idx]) {
                flush_cache(span(bounce[idx], sizeof(virtio_input_event)), true);
                memcpy(&evs[idx], bounce[idx], sizeof(virtio_input_event));
            } else
                flush_cache(span((uint8_t*)&evs[idx], sizeof(virtio_input_event)), true);

#if 0
            print("ev: type = {}, code = {}, value = {}\n",
                  (evdev_type)evs[idx].type, (evdev_code)evs[idx].code,
                  +evs[idx].value);
#endif

            input_event ev;

            ev.type = evs[idx].type;
            ev.code = evs[idx].code;
            ev.value = evs[idx].value;

            post_event(ev, false);
        }

        slot[idx] = event_queue->async_read(pa[idx], sizeof(virtio_input_event));
        idx = (idx + 1) % BUFFER_LENGTH;
    }

    for (size_t i = 0; i < BUFFER_LENGTH; i++) {
        if (bounce[i])
            free(bounce[i]);
    }
}

template<virtio_device T>
nt_status virtio_input<T>::write(span<const span<const uint8_t>> v,
                                 uint64_t& written_bytes) {
    if (get_vec_size(v) < sizeof(input_event))
        return nt_status::INVALID_PARAMETER;

    input_event ev;
    span<const uint8_t> top = v.front();

    memcpy_vec((uint8_t*)&ev, v, top, sizeof(input_event));

    virtio_input_event vie;

    vie.type = ev.type;
    vie.code = ev.code;
    vie.value = ev.value;

    auto status = status_queue->write(span((uint8_t*)&vie, sizeof(vie)));

    if (!success(status))
        return status;

    written_bytes = sizeof(input_event);

    return nt_status::SUCCESS;
}

template<virtio_device T>
nt_status virtio_input<T>::ioctl(uint32_t code, span<const uint8_t> inbuf,
                                 span<uint8_t> outbuf, uintptr_t& written,
                                 ACCESS_MASK /*access_mask*/) {
    switch (code) {
        case IOCTL_MOXNOX_EVDEV_GET_ABSINFO:
            return ioctl_get_absinfo(inbuf, outbuf, written);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

template<virtio_device T>
nt_status virtio_input<T>::ioctl_get_absinfo(span<const uint8_t> inbuf,
                                             span<uint8_t> outbuf,
                                             uintptr_t& written) {
    if (inbuf.size() < sizeof(evdev_abs_code))
        return nt_status::INVALID_PARAMETER;

    if (outbuf.size() < sizeof(input_absinfo))
        return nt_status::INVALID_PARAMETER;

    auto code = *(evdev_abs_code*)inbuf.data();
    auto& ret = *(input_absinfo*)outbuf.data();

    switch (code) {
        case evdev_abs_code::ABS_X:
            if (absinfo_x.has_value()) {
                ret = *absinfo_x;
                written = sizeof(input_absinfo);
                return nt_status::SUCCESS;
            }
        break;

        case evdev_abs_code::ABS_Y:
            if (absinfo_y.has_value()) {
                ret = *absinfo_y;
                written = sizeof(input_absinfo);
                return nt_status::SUCCESS;
            }
        break;

        case evdev_abs_code::ABS_WHEEL:
            if (absinfo_wheel.has_value()) {
                ret = *absinfo_wheel;
                written = sizeof(input_absinfo);
                return nt_status::SUCCESS;
            }
        break;

        default:
            break;
    }

    return nt_status::NOT_FOUND;
}

static void found_virtio_pci_input(pci_device& dev) {
    auto d = new virtio_pci_device(dev, SUPPORTED_FEATURES, 2);
    if (!d) {
        print("new failed\n");
        return;
    }

    auto v = new virtio_input(*d);
    if (!v) {
        print("new failed\n");
        delete d;
        return;
    }

    object_ref ref{*v};

    if (!v->dev.initialized)
        return;

    v->dev.finalize();

    // FIXME - make sure devices are numbered consistently

    auto num = virtio_input_num.fetch_add(1);

    {
        char s[20];
        span sp(s, sizeof(s));

        sp = format(sp, "virtioinput{}", num);

        auto status = name_object(*v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status))
            print("name_object returned {}\n", status);
    }
}

static bool found_virtio_mmio_input(virtio_mmio_device& dev) {
    auto pv = new virtio_input(dev);
    if (!pv) {
        print("new failed\n");
        return false;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return false;

    v.dev.finalize();

    // FIXME - make sure devices are numbered consistently

    auto num = virtio_input_num.fetch_add(1);

    {
        char s[20];
        span sp(s, sizeof(s));

        sp = format(sp, "virtioinput{}", num);

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    return true;
}
