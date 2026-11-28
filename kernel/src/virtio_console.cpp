// SPDX-License-Identifier: GPL-2.0-only
#include <span>
#include <atomic>
#include <string_view>

import nt;
import print;
import bsod;
import config;
import chardev;
import pipe;
import format;
import pci;
import virtio;
import list;
import module_init;
import power;
import mem;
import thread;
import console;
import handle;
import object_ref;
import vec;

using namespace std;

static const unsigned int serial_buffer_pages = 1;

static const uint64_t VIRTIO_CONSOLE_F_SIZE = 1 << 0;
static const uint64_t VIRTIO_CONSOLE_F_MULTIPORT = 1 << 1;
static const uint64_t VIRTIO_CONSOLE_F_EMERG_WRITE = 1 << 2;

static const uint64_t SUPPORTED_FEATURES = VIRTIO_CONSOLE_F_EMERG_WRITE;

struct virtio_console_config {
    uint16_t cols;
    uint16_t rows;
    uint32_t max_nr_ports;
    uint32_t emerg_wr;
} __attribute((packed));

static_assert(sizeof(virtio_console_config) == 12);

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

static void found_virtio_pci_console(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1003, true, found_virtio_pci_console },
    { {}, PCI_VENDOR_ID_REDHAT, 0x1043, true, found_virtio_pci_console },
};

static bool found_virtio_mmio_console(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 3, SUPPORTED_FEATURES, 2, found_virtio_mmio_console
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_console(init_virtio_pci);

template<virtio_device T>
class virtio_console : public chardev {
public:
    virtio_console(T& t, pipe& recv_pipe);

    ~virtio_console() {
        recv_pipe.release();
        delete &dev;
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        return recv_pipe.read(v, read_bytes);
    }

    nt_status read_nonblock(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        return recv_pipe.read_nonblock(v, read_bytes);
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override {
        return do_write<false>(v, written_bytes);
    }

    nt_status write_nonblock(span<const span<const uint8_t>> v,
                             uint64_t& written_bytes) override {
        return do_write<true>(v, written_bytes);
    }

    sync_object& get_sync_object() override {
        return recv_pipe;
    }

    nt_status write(string_view sv, uint64_t& written_bytes) {
        return write(span((uint8_t*)sv.data(), sv.length()), written_bytes);
    }

    void recv_thread();

    T& dev;
    pipe& recv_pipe;

private:
    template<bool NONBLOCK>
    nt_status do_write(span<const span<const uint8_t>> v,
                       uint64_t& written_bytes);

    virt_queue* recv_queue = nullptr;
    virt_queue* send_queue = nullptr;
};

static atomic<uint32_t> virtio_console_num = 0;

template<virtio_device T>
virtio_console<T>::virtio_console(T& dev, pipe& recv_pipe) : dev(dev), recv_pipe(recv_pipe) {
    recv_pipe.acquire();

    if constexpr (is_same_v<T, virtio_pci_device>) {
        if (dev.device_cfg->length < sizeof(virtio_console_config)) {
            print("virtio_console: device cfg length was {}, expected at least {}\n",
                +dev.device_cfg->length, sizeof(virtio_console_config));
            return;
        }
    }

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            recv_queue = &vq;
        else if (vq.num == 1)
            send_queue = &vq;
    }

    this->acquire();

    auto t = add_kernel_thread([](void* ctx) {
        auto& dev = *(virtio_console<T>*)ctx;

        dev.recv_thread();
        dev.release();
    }, this);

    if (!t) {
        print("add_kernel_thread failed\n");
        this->release();
        return;
    }
}

template<virtio_device T>
void virtio_console<T>::recv_thread() {
    // FIXME - die gracefully if device ever goes away

    auto buf_size = PAGE_SIZE;
    auto buf = (uint8_t*)malloc(buf_size);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    while (true) {
        uint64_t buf_read_bytes, written_bytes;

        auto status = recv_queue->read(span(buf, buf_size), buf_read_bytes);

        if (!success(status)) {
            print("recv_queue->read returned {}\n", status);
            continue;
        }

        if (buf_read_bytes == 0)
            continue;

        auto read_buf = span((const uint8_t*)buf, buf_read_bytes);

        while (true) {
            status = recv_pipe.write(span(&read_buf, 1), written_bytes);
            if (!success(status)) {
                print("recv_pipe.write returned {}\n", status);
                break;
            }

            if (written_bytes == read_buf.size())
                break;

            read_buf = span(read_buf.data() + written_bytes,
                            read_buf.size() - written_bytes);
        }
    }
}

template<virtio_device T>
template<bool NONBLOCK>
nt_status virtio_console<T>::do_write(span<const span<const uint8_t>> v,
                                      uint64_t& written_bytes) {
    if (shutting_down_cpu.load(memory_order::acquire) != 0xffffffff)
        return nt_status::SYSTEM_SHUTDOWN;

    if (in_bsod) {
        if (!(dev.negotiated_features & VIRTIO_CONSOLE_F_EMERG_WRITE))
            return nt_status::SYSTEM_SHUTDOWN;

        volatile virtio_console_config* vc;

        if constexpr (is_same_v<T, virtio_pci_device>)
            vc = (volatile virtio_console_config*)(dev.dev.bar[dev.device_cfg->bar].data() + dev.device_cfg->offset);
        else
            vc = (volatile virtio_console_config*)dev.config;

        for (auto s : v) {
            for (auto c : s) {
                vc->emerg_wr = c;
            }
        }

        written_bytes = get_vec_size(v);

        return nt_status::SUCCESS;
    }

    size_t vec_count = 0;
    uint64_t read_bytes;

    // FIXME - this should be capped at queue size
    static const size_t MAX_VEC_COUNT = PAGE_SIZE / sizeof(virtq_desc);

    for (auto s : v) {
        if (s.empty())
            continue;

        vec_count++;

        if (vec_count == MAX_VEC_COUNT)
            break;
    }

    written_bytes = 0;

    if (vec_count == 0)
        return nt_status::SUCCESS;

    auto descs = (virtq_desc*)malloc(vec_count * sizeof(virtq_desc));
    if (!descs) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    size_t idx = 0;
    for (auto s : v) {
        if (s.empty())
            continue;

        auto& d = descs[idx];

        if (idx != 0) {
            descs[idx - 1].flags |= VIRTQ_DESC_F_NEXT;
            descs[idx - 1].next = (uint16_t)idx;
        }

        d.addr = va_to_pa_phys(s.data());
        d.len = (uint32_t)s.size();
        d.flags = 0;

        flush_cache(span((uint8_t*)phys_address(d.addr), d.len), false);

        written_bytes += d.len;

        idx++;

        if (idx == vec_count)
            break;
    }

    // FIXME - non-blocking mode?
    auto status = send_queue->indirect(span(descs, vec_count), read_bytes);

    free(descs);

    return status;
}

static object_ref create_virtio_console_in_pipe(unsigned int buffer_pages) {
    auto pptr = new pipe(buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    return p;
}

static void found_virtio_pci_console(pci_device& dev) {
    auto recv_handle = create_virtio_console_in_pipe(serial_buffer_pages);
    if (!recv_handle) {
        print("create_virtio_console_in_pipe failed\n");
        return;
    }

    auto& recv_pipe = static_cast<pipe&>(recv_handle.get());

    auto d = new virtio_pci_device(dev, SUPPORTED_FEATURES, 2);
    if (!d) {
        print("new failed\n");
        return;
    }

    auto v = new virtio_console(*d, recv_pipe);
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

    auto num = virtio_console_num.fetch_add(1);

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "hvc{}", num);

        auto status = name_object(*v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status))
            print("name_object returned {}\n", status);
    }

    if (num == 0 && !com1_pipe_handle)
        com1_pipe_handle.swap(ref);
}

static bool found_virtio_mmio_console(virtio_mmio_device& dev) {
    auto recv_handle = create_virtio_console_in_pipe(serial_buffer_pages);
    if (!recv_handle) {
        print("create_virtio_console_in_pipe failed\n");
        return false;
    }

    auto& recv_pipe = static_cast<pipe&>(recv_handle.get());

    auto pv = new virtio_console(dev, recv_pipe);
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

    auto num = virtio_console_num.fetch_add(1);

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "hvc{}", num);

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    if (num == 0 && !com1_pipe_handle)
        com1_pipe_handle.swap(ref);

    return true;
}
