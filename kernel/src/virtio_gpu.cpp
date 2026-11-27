// SPDX-License-Identifier: GPL-2.0-only
#include <span>
#include <atomic>
#include <string_view>
#include <variant>
#include <optional>
#include <mutex>

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
import framebuffer;
import cmdline;
import console;
import handle;
import object_ref;
import crt;
import dma;
import rwsem;

using namespace std;

static const uint64_t VIRTIO_GPU_F_VIRGL = 1 << 0;
static const uint64_t VIRTIO_GPU_F_EDID = 1 << 1;
static const uint64_t VIRTIO_GPU_F_RESOURCE_UUID = 1 << 2;
static const uint64_t VIRTIO_GPU_F_RESOURCE_BLOB = 1 << 3;
static const uint64_t VIRTIO_GPU_F_CONTEXT_INIT = 1 << 4;

static const uint64_t SUPPORTED_FEATURES = VIRTIO_RING_F_INDIRECT_DESC |
                                           VIRTIO_GPU_F_EDID;

struct virtio_gpu_config {
    uint32_t events_read;
    uint32_t events_clear;
    uint32_t num_scanouts;
    uint32_t num_capsets;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_config) == 16);

enum class VIRTIO_GPU_CMD : uint32_t {
    GET_DISPLAY_INFO = 0x0100,
    RESOURCE_CREATE_2D,
    RESOURCE_UNREF,
    SET_SCANOUT,
    RESOURCE_FLUSH,
    TRANSFER_TO_HOST_2D,
    RESOURCE_ATTACH_BACKING,
    RESOURCE_DETACH_BACKING,
    GET_CAPSET_INFO,
    GET_CAPSET,
    GET_EDID,
    RESOURCE_ASSIGN_UUID,
    RESOURCE_CREATE_BLOB,
    SET_SCANOUT_BLOB,

    CTX_CREATE = 0x0200,
    CTX_DESTROY,
    CTX_ATTACH_RESOURCE,
    CTX_DETACH_RESOURCE,
    RESOURCE_CREATE_3D,
    TRANSFER_TO_HOST_3D,
    TRANSFER_FROM_HOST_3D,
    SUBMIT_3D,
    RESOURCE_MAP_BLOB,
    RESOURCE_UNMAP_BLOB,

    UPDATE_CURSOR = 0x0300,
    MOVE_CURSOR,
};

enum class VIRTIO_GPU_RESP : uint32_t {
    OK_NODATA = 0x1100,
    OK_DISPLAY_INFO,
    OK_CAPSET_INFO,
    OK_CAPSET,
    OK_EDID,
    OK_RESOURCE_UUID,
    OK_MAP_INFO,

    ERR_UNSPEC = 0x1200,
    ERR_OUT_OF_MEMORY,
    ERR_INVALID_SCANOUT_ID,
    ERR_INVALID_RESOURCE_ID,
    ERR_INVALID_CONTEXT_ID,
    ERR_INVALID_PARAMETER,
};

static const uint32_t VIRTIO_GPU_FLAG_FENCE = 1 << 0;
static const uint32_t VIRTIO_GPU_FLAG_INFO_RING_IDX = 1 << 1;

struct virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint8_t ring_idx;
    uint8_t padding[3];
} __attribute((packed));

static_assert(sizeof(virtio_gpu_ctrl_hdr) == 24);

static const size_t VIRTIO_GPU_MAX_SCANOUTS = 16;

struct virtio_gpu_rect {
    uint32_t x;
    uint32_t y;
    uint32_t width;
    uint32_t height;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_rect) == 16);

struct virtio_gpu_resp_display_info {
    virtio_gpu_ctrl_hdr hdr;
    struct {
        virtio_gpu_rect r;
        uint32_t enabled;
        uint32_t flags;
    } pmodes[VIRTIO_GPU_MAX_SCANOUTS];
} __attribute((packed));

static_assert(sizeof(virtio_gpu_resp_display_info) == 408);

enum class VIRTIO_GPU_FORMAT : uint32_t {
    B8G8R8A8_UNORM = 1,
    B8G8R8X8_UNORM = 2,
    A8R8G8B8_UNORM = 3,
    X8R8G8B8_UNORM = 4,
    R8G8B8A8_UNORM = 67,
    X8B8G8R8_UNORM = 68,
    A8B8G8R8_UNORM = 121,
    R8G8B8X8_UNORM = 134,
};

struct virtio_gpu_resource_create_2d {
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    VIRTIO_GPU_FORMAT format;
    uint32_t width;
    uint32_t height;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_resource_create_2d) == 40);

struct virtio_gpu_resource_unref {
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t padding;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_resource_unref) == 32);

struct virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_mem_entry) == 16);

struct virtio_gpu_resource_attach_backing {
    virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
    virtio_gpu_mem_entry entries[1];
} __attribute((packed));

static_assert(offsetof(virtio_gpu_resource_attach_backing, entries) == 32);

struct virtio_gpu_set_scanout {
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint32_t scanout_id;
    uint32_t resource_id;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_set_scanout) == 48);

struct virtio_gpu_transfer_to_host_2d {
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint64_t offset;
    uint32_t resource_id;
    uint32_t padding;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_transfer_to_host_2d) == 56);

struct virtio_gpu_resource_flush {
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_rect r;
    uint32_t resource_id;
    uint32_t padding;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_resource_flush) == 48);

struct virtio_gpu_get_edid {
    virtio_gpu_ctrl_hdr hdr;
    uint32_t scanout;
    uint32_t padding;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_get_edid) == 32);

struct virtio_gpu_resp_edid {
    virtio_gpu_ctrl_hdr hdr;
    uint32_t size;
    uint32_t padding;
    uint8_t edid[1024];
} __attribute((packed));

static_assert(sizeof(virtio_gpu_resp_edid) == 1056);

struct virtio_gpu_cursor_pos {
    uint32_t scanout_id;
    uint32_t x;
    uint32_t y;
    uint32_t padding;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_cursor_pos) == 16);

struct virtio_gpu_update_cursor {
    virtio_gpu_ctrl_hdr hdr;
    virtio_gpu_cursor_pos pos;
    uint32_t resource_id;
    uint32_t hot_x;
    uint32_t hot_y;
    uint32_t padding;
} __attribute((packed));

static_assert(sizeof(virtio_gpu_update_cursor) == 56);

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

static void found_virtio_pci_gpu(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1050, true, found_virtio_pci_gpu },
};

static bool found_virtio_mmio_gpu(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 16, SUPPORTED_FEATURES, 2, found_virtio_mmio_gpu
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_gpu(init_virtio_pci);

template<virtio_device T>
class virtio_gpu : public chardev {
public:
    virtio_gpu(T& dev);

    ~virtio_gpu() {
        delete &dev;
    }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status read_nonblock(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    void start();

    bool get_display_info(virtio_gpu_resp_display_info& disp_info);
    bool get_edid(uint32_t scanout, span<uint8_t> ret);
    optional<uint32_t> create_resource(VIRTIO_GPU_FORMAT format,
                                       uint32_t width, uint32_t height);
    bool unref_resource(uint32_t resource);
    bool attach_backing(uint32_t resource, span<uint8_t> buf);
    bool set_scanout(const virtio_gpu_rect& r, uint32_t scanout,
                     uint32_t resource);
    bool transfer_to_host(const virtio_gpu_rect& r, uint64_t offset,
                          uint32_t resource, optional<uint64_t> fence_num = nullopt);
    bool resource_flush(const virtio_gpu_rect& r, uint32_t resource);
    bool move_cursor(uint32_t scanout, uint32_t x, uint32_t y);
    bool update_cursor(uint32_t scanout, uint32_t x, uint32_t y,
                       uint32_t resource, uint32_t hot_x,
                       uint32_t hot_y, optional<uint64_t> fence_num = nullopt);

    T& dev;
    uint32_t num_scanouts;
    rwsem cursor_lock;
    uint32_t mouse_resource = 0;
    atomic<uint64_t> fence = 1;

private:
    virt_queue* control_queue = nullptr;
    virt_queue* cursor_queue = nullptr;
    atomic<uint32_t> resource_num = 1;
};

static variant<virtio_gpu<virtio_pci_device>*, virtio_gpu<virtio_mmio_device>*> flush_framebuffer_device;
static uint32_t flush_framebuffer_resource;

template<virtio_device T>
virtio_gpu<T>::virtio_gpu(T& dev) : dev(dev) {
    if constexpr (is_same_v<T, virtio_pci_device>) {
        if (dev.device_cfg->length < sizeof(virtio_gpu_config)) {
            print("virtio_gpu: device cfg length was {}, expected at least {}\n",
                +dev.device_cfg->length, sizeof(virtio_gpu_config));
            return;
        }

        auto& cfg = *(volatile virtio_gpu_config*)(dev.dev.bar[dev.device_cfg->bar].data() + dev.device_cfg->offset);

        num_scanouts = cfg.num_scanouts;
    } else if constexpr (is_same_v<T, virtio_mmio_device>) {
        auto& cfg = *(volatile virtio_gpu_config*)(dev.config);

        num_scanouts = cfg.num_scanouts;
    }

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            control_queue = &vq;
        else if (vq.num == 1)
            cursor_queue = &vq;
    }
}

template<>
consteval bool format_spec_valid<enum VIRTIO_GPU_RESP>(string_view fmt) {
    return fmt.empty();
}

static span<char> do_format_arg(span<char> sp, string_view, enum VIRTIO_GPU_RESP r) {
    switch (r) {
        using enum VIRTIO_GPU_RESP;

        case OK_NODATA:
            return do_format_arg(sp, "", "OK_NODATA");
        case OK_DISPLAY_INFO:
            return do_format_arg(sp, "", "OK_DISPLAY_INFO");
        case OK_CAPSET_INFO:
            return do_format_arg(sp, "", "OK_CAPSET_INFO");
        case OK_CAPSET:
            return do_format_arg(sp, "", "OK_CAPSET");
        case OK_EDID:
            return do_format_arg(sp, "", "OK_EDID");
        case OK_RESOURCE_UUID:
            return do_format_arg(sp, "", "OK_RESOURCE_UUID");
        case OK_MAP_INFO:
            return do_format_arg(sp, "", "OK_MAP_INFO");
        case ERR_UNSPEC:
            return do_format_arg(sp, "", "ERR_UNSPEC");
        case ERR_OUT_OF_MEMORY:
            return do_format_arg(sp, "", "ERR_OUT_OF_MEMORY");
        case ERR_INVALID_SCANOUT_ID:
            return do_format_arg(sp, "", "ERR_INVALID_SCANOUT_ID");
        case ERR_INVALID_RESOURCE_ID:
            return do_format_arg(sp, "", "ERR_INVALID_RESOURCE_ID");
        case ERR_INVALID_CONTEXT_ID:
            return do_format_arg(sp, "", "ERR_INVALID_CONTEXT_ID");
        case ERR_INVALID_PARAMETER:
            return do_format_arg(sp, "", "ERR_INVALID_PARAMETER");
        default:
            return do_format_arg(sp, "", (uint32_t)r);
    }
}

#if 0
template<>
consteval bool format_spec_valid<virtio_gpu_rect>(string_view fmt) {
    return fmt.empty();
}

static span<char> do_format_arg(span<char> sp, string_view, const virtio_gpu_rect& r) {
    sp = do_format_arg(sp, "", "(");
    sp = do_format_arg(sp, "", r.x);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, "", r.y);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, "", r.width);
    sp = do_format_arg(sp, "", ",");
    sp = do_format_arg(sp, "", r.height);
    sp = do_format_arg(sp, "", ")");

    return sp;
}
#endif

template<virtio_device T>
bool virtio_gpu<T>::get_display_info(virtio_gpu_resp_display_info& disp_info) {
    virtio_gpu_ctrl_hdr req;
    virtq_desc descs[2];
    uint64_t read_bytes;
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;

    memset(&req, 0, sizeof(req));
    req.type = (uint32_t)VIRTIO_GPU_CMD::GET_DISPLAY_INFO;

    auto pa1 = dma_setup(span((uint8_t*)&req, sizeof(req)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return false;
    }

    auto pa2 = dma_setup(span((uint8_t*)&disp_info, sizeof(disp_info)), false,
                         bounce2);
    if (!pa2) {
        print("dma_setup failed\n");
        dma_finish_write(bounce1);
        return false;
    }

    descs[0].addr = pa1;
    descs[0].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = sizeof(virtio_gpu_resp_display_info);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    auto status = control_queue->indirect(descs, read_bytes);

    dma_finish_read(span((uint8_t*)&disp_info, sizeof(disp_info)), bounce2);
    dma_finish_write(bounce1);

    if (!success(status))
        return false;

    auto resp_err = (enum VIRTIO_GPU_RESP)disp_info.hdr.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_DISPLAY_INFO) {
        print("VIRTIO_GPU_CMD_GET_DISPLAY_INFO returned {}\n", resp_err);
        return false;
    }

    return true;
}

template<virtio_device T>
bool virtio_gpu<T>::get_edid(uint32_t scanout, span<uint8_t> ret) {
    virtio_gpu_get_edid req;
    virtio_gpu_resp_edid resp;
    virtq_desc descs[2];
    uint64_t read_bytes;
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;

    memset(&req, 0, sizeof(req));
    req.hdr.type = (uint32_t)VIRTIO_GPU_CMD::GET_EDID;
    req.scanout = scanout;

    auto pa1 = dma_setup(span((uint8_t*)&req, sizeof(req)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return false;
    }

    auto pa2 = dma_setup(span((uint8_t*)&resp, sizeof(resp)), false, bounce2);
    if (!pa2) {
        print("dma_setup failed\n");
        dma_finish_write(bounce1);
        return false;
    }

    descs[0].addr = pa1;
    descs[0].len = sizeof(virtio_gpu_get_edid);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = sizeof(virtio_gpu_resp_edid);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    auto status = control_queue->indirect(descs, read_bytes);

    dma_finish_read(span((uint8_t*)&resp, sizeof(resp)), bounce2);
    dma_finish_write(bounce1);

    if (!success(status))
        return false;

    auto resp_err = (enum VIRTIO_GPU_RESP)resp.hdr.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_EDID) {
        print("VIRTIO_GPU_CMD_GET_EDID returned {}\n", resp_err);
        return false;
    }

    memcpy(ret.data(), resp.edid, min(ret.size(), sizeof(resp.edid)));

    return true;
}

template<virtio_device T>
optional<uint32_t> virtio_gpu<T>::create_resource(VIRTIO_GPU_FORMAT format,
                                                  uint32_t width,
                                                  uint32_t height) {
    virtio_gpu_resource_create_2d req;
    virtio_gpu_ctrl_hdr resp;
    virtq_desc descs[2];
    uint64_t read_bytes;
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;

    uint32_t num = resource_num.fetch_add(1);

    memset(&req, 0, sizeof(req));
    req.hdr.type = (uint32_t)VIRTIO_GPU_CMD::RESOURCE_CREATE_2D;
    req.resource_id = num;
    req.format = format;
    req.width = width;
    req.height = height;

    auto pa1 = dma_setup(span((uint8_t*)&req, sizeof(req)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return false;
    }

    auto pa2 = dma_setup(span((uint8_t*)&resp, sizeof(resp)), false, bounce2);
    if (!pa2) {
        print("dma_setup failed\n");
        dma_finish_write(bounce1);
        return false;
    }

    descs[0].addr = pa1;
    descs[0].len = sizeof(virtio_gpu_resource_create_2d);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    auto status = control_queue->indirect(descs, read_bytes);

    dma_finish_read(span((uint8_t*)&resp, sizeof(resp)), bounce2);
    dma_finish_write(bounce1);

    if (!success(status))
        return nullopt;

    auto resp_err = (enum VIRTIO_GPU_RESP)resp.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_NODATA) {
        print("VIRTIO_GPU_CMD_RESOURCE_CREATE_2D returned {}\n", resp_err);
        return nullopt;
    }

    return num;
}

template<virtio_device T>
bool virtio_gpu<T>::unref_resource(uint32_t resource) {
    virtio_gpu_resource_unref req;
    virtio_gpu_ctrl_hdr resp;
    virtq_desc descs[2];
    uint64_t read_bytes;
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;

    memset(&req, 0, sizeof(req));
    req.hdr.type = (uint32_t)VIRTIO_GPU_CMD::RESOURCE_UNREF;
    req.resource_id = resource;

    auto pa1 = dma_setup(span((uint8_t*)&req, sizeof(req)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return false;
    }

    auto pa2 = dma_setup(span((uint8_t*)&resp, sizeof(resp)), false, bounce2);
    if (!pa2) {
        print("dma_setup failed\n");
        dma_finish_write(bounce1);
        return false;
    }

    descs[0].addr = pa1;
    descs[0].len = sizeof(virtio_gpu_resource_unref);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    auto status = control_queue->indirect(descs, read_bytes);

    dma_finish_read(span((uint8_t*)&resp, sizeof(resp)), bounce2);
    dma_finish_write(bounce1);

    if (!success(status))
        return false;

    auto resp_err = (enum VIRTIO_GPU_RESP)resp.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_NODATA) {
        print("VIRTIO_GPU_CMD_RESOURCE_UNREF returned {}\n", resp_err);
        return false;
    }

    return true;
}

static unsigned int calc_num_mem_entries(span<const uint8_t> buf) {
    unsigned int ret = 0;

    // buf must come from vmalloc

    while (!buf.empty()) {
        auto pa = va_to_pa_lookup((uintptr_t)buf.data());
        auto order = get_folio_order(pa);

        ret++;

        if (buf.size() <= (size_t)1 << (PAGE_SHIFT + order))
            break;

        buf = buf.subspan((size_t)1 << (PAGE_SHIFT + order));
    }

    return ret;
}

static void populate_entries(span<virtio_gpu_mem_entry> entries, span<const uint8_t> buf) {
    while (!buf.empty()) {
        auto pa = va_to_pa_lookup((uintptr_t)buf.data());
        auto order = get_folio_order(pa);
        auto& e = entries.front();

        e.addr = pa;
        e.length = (uint32_t)min(buf.size(), (size_t)1 << (PAGE_SHIFT + order));
        e.padding = 0;

        if (buf.size() <= (size_t)1 << (PAGE_SHIFT + order))
            break;

        buf = buf.subspan((size_t)1 << (PAGE_SHIFT + order));
        entries = entries.subspan(1);
    }
}

static void populate_descs(span<virtq_desc> descs, span<const uint8_t> buf) {
    uint16_t i = 0;

    while (!buf.empty()) {
        auto pa = va_to_pa_lookup((uintptr_t)buf.data());
        auto order = get_folio_order(pa);
        auto& d = descs.front();

        d.addr = pa;
        d.len = (uint32_t)min(buf.size(), (size_t)1 << (PAGE_SHIFT + order));
        d.flags = VIRTQ_DESC_F_NEXT;
        d.next = i + 1;

        if (buf.size() <= (size_t)1 << (PAGE_SHIFT + order))
            break;

        buf = buf.subspan((size_t)1 << (PAGE_SHIFT + order));
        descs = descs.subspan(1);
        i++;
    }
}

template<virtio_device T>
bool virtio_gpu<T>::attach_backing(uint32_t resource, span<uint8_t> buf) {
    virtio_gpu_ctrl_hdr resp;
    uint64_t read_bytes;
    uint8_t* bounce = nullptr;

    auto num_entries = calc_num_mem_entries(buf);

    auto req_size = (uint32_t)(offsetof(virtio_gpu_resource_attach_backing, entries) +
                               (num_entries * sizeof(virtio_gpu_mem_entry)));

    auto req = (virtio_gpu_resource_attach_backing*)vmalloc(req_size);
    if (!req) {
        print("vmalloc failed\n");
        return false;
    }

    memset(req, 0, req_size);
    req->hdr.type = (uint32_t)VIRTIO_GPU_CMD::RESOURCE_ATTACH_BACKING;
    req->resource_id = resource;
    req->nr_entries = num_entries;

    populate_entries(span(req->entries, num_entries), buf);

    auto req_entries = calc_num_mem_entries(span((uint8_t*)req, req_size));

    auto descs = (virtq_desc*)malloc(sizeof(virtq_desc) * (req_entries + 1));
    if (!descs) {
        print("malloc failed\n");
        vfree(req, req_size);
        return false;
    }

    populate_descs(span(descs, num_entries), span((uint8_t*)req, req_size));

    auto pa = dma_setup(span((uint8_t*)&resp, sizeof(resp)), false, bounce);
    if (!pa) {
        print("dma_setup failed\n");
        free(descs);
        vfree(req, req_size);
        return false;
    }

    descs[req_entries].addr = pa;
    descs[req_entries].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[req_entries].flags = VIRTQ_DESC_F_WRITE;
    descs[req_entries].next = 0;

    flush_cache(span((uint8_t*)req, req_size), false);

    auto status = control_queue->indirect(span(descs, req_entries + 1),
                                          read_bytes);

    dma_finish_read(span((uint8_t*)&resp, sizeof(resp)), bounce);

    if (!success(status)) {
        free(descs);
        vfree(req, req_size);
        return false;
    }

    free(descs);
    vfree(req, req_size);

    auto resp_err = (enum VIRTIO_GPU_RESP)resp.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_NODATA) {
        print("VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING returned {}\n", resp_err);
        return false;
    }

    return true;
}

template<virtio_device T>
bool virtio_gpu<T>::set_scanout(const virtio_gpu_rect& r, uint32_t scanout,
                                uint32_t resource) {
    virtio_gpu_set_scanout req;
    virtio_gpu_ctrl_hdr resp;
    virtq_desc descs[2];
    uint64_t read_bytes;
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;

    memset(&req, 0, sizeof(req));
    req.hdr.type = (uint32_t)VIRTIO_GPU_CMD::SET_SCANOUT;
    req.r = r;
    req.scanout_id = scanout;
    req.resource_id = resource;

    auto pa1 = dma_setup(span((uint8_t*)&req, sizeof(req)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return false;
    }

    auto pa2 = dma_setup(span((uint8_t*)&resp, sizeof(resp)), false, bounce2);
    if (!pa2) {
        print("dma_setup failed\n");
        dma_finish_write(bounce1);
        return false;
    }

    descs[0].addr = pa1;
    descs[0].len = sizeof(virtio_gpu_set_scanout);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    auto status = control_queue->indirect(descs, read_bytes);

    dma_finish_read(span((uint8_t*)&resp, sizeof(resp)), bounce2);
    dma_finish_write(bounce1);

    if (!success(status))
        return false;

    auto resp_err = (enum VIRTIO_GPU_RESP)resp.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_NODATA) {
        print("VIRTIO_GPU_CMD_SET_SCANOUT returned {}\n", resp_err);
        return false;
    }

    return true;
}

template<virtio_device T>
bool virtio_gpu<T>::transfer_to_host(const virtio_gpu_rect& r, uint64_t offset,
                                     uint32_t resource, optional<uint64_t> fence_num) {
    virtio_gpu_transfer_to_host_2d req;
    virtio_gpu_ctrl_hdr resp;
    virtq_desc descs[2];
    uint64_t read_bytes;
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;
    nt_status status;

    memset(&req, 0, sizeof(req));
    req.hdr.type = (uint32_t)VIRTIO_GPU_CMD::TRANSFER_TO_HOST_2D;

    if (fence_num.has_value()) {
        req.hdr.flags = VIRTIO_GPU_FLAG_FENCE;
        req.hdr.fence_id = *fence_num;
    }

    req.r = r;
    req.offset = offset;
    req.resource_id = resource;

    auto pa1 = dma_setup(span((uint8_t*)&req, sizeof(req)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return false;
    }

    auto pa2 = dma_setup(span((uint8_t*)&resp, sizeof(resp)), false, bounce2);
    if (!pa2) {
        print("dma_setup failed\n");
        dma_finish_write(bounce1);
        return false;
    }

    descs[0].addr = pa1;
    descs[0].len = sizeof(virtio_gpu_transfer_to_host_2d);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    if (in_bsod)
        status = control_queue->indirect_no_sleep(descs, read_bytes);
    else
        status = control_queue->indirect(descs, read_bytes);

    dma_finish_read(span((uint8_t*)&resp, sizeof(resp)), bounce2);
    dma_finish_write(bounce1);

    if (!success(status))
        return false;

    auto resp_err = (enum VIRTIO_GPU_RESP)resp.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_NODATA) {
        print("VIRTIO_GPU_CMD_TRANSFER_TO_HOST_2D returned {}\n", resp_err);
        return false;
    }

    return true;
}

template<virtio_device T>
bool virtio_gpu<T>::resource_flush(const virtio_gpu_rect& r, uint32_t resource) {
    virtio_gpu_resource_flush req;
    virtio_gpu_ctrl_hdr resp;
    virtq_desc descs[2];
    uint64_t read_bytes;
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;
    nt_status status;

    memset(&req, 0, sizeof(req));
    req.hdr.type = (uint32_t)VIRTIO_GPU_CMD::RESOURCE_FLUSH;
    req.r = r;
    req.resource_id = resource;

    auto pa1 = dma_setup(span((uint8_t*)&req, sizeof(req)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return false;
    }

    auto pa2 = dma_setup(span((uint8_t*)&resp, sizeof(resp)), false, bounce2);
    if (!pa2) {
        print("dma_setup failed\n");
        dma_finish_write(bounce1);
        return false;
    }

    descs[0].addr = pa1;
    descs[0].len = sizeof(virtio_gpu_resource_flush);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = sizeof(virtio_gpu_ctrl_hdr);
    descs[1].flags = VIRTQ_DESC_F_WRITE;
    descs[1].next = 0;

    if (in_bsod)
        status = control_queue->indirect_no_sleep(descs, read_bytes);
    else
        status = control_queue->indirect(descs, read_bytes);

    dma_finish_read(span((uint8_t*)&resp, sizeof(resp)), bounce2);
    dma_finish_write(bounce1);

    if (!success(status))
        return false;

    auto resp_err = (enum VIRTIO_GPU_RESP)resp.type;

    if (resp_err != VIRTIO_GPU_RESP::OK_NODATA) {
        print("VIRTIO_GPU_CMD_RESOURCE_FLUSH returned {}\n", resp_err);
        return false;
    }

    return true;
}

template<virtio_device T>
bool virtio_gpu<T>::move_cursor(uint32_t scanout, uint32_t x, uint32_t y) {
    virtio_gpu_update_cursor req;

    memset(&req, 0, sizeof(req));
    req.hdr.type = (uint32_t)VIRTIO_GPU_CMD::MOVE_CURSOR;
    req.pos.scanout_id = scanout;
    req.pos.x = x;
    req.pos.y = y;

    // no response, contrary to what the specs imply
    auto status = cursor_queue->write(span((uint8_t*)&req, sizeof(req)));

    if (!success(status))
        return false;

    return true;
}

template<virtio_device T>
bool virtio_gpu<T>::update_cursor(uint32_t scanout, uint32_t x, uint32_t y,
                                  uint32_t resource, uint32_t hot_x,
                                  uint32_t hot_y, optional<uint64_t> fence_num) {
    virtio_gpu_update_cursor req;

    memset(&req, 0, sizeof(req));
    req.hdr.type = (uint32_t)VIRTIO_GPU_CMD::UPDATE_CURSOR;

    if (fence_num.has_value())
        req.hdr.fence_id = *fence_num;

    req.pos.scanout_id = scanout;
    req.pos.x = x;
    req.pos.y = y;
    req.resource_id = resource;
    req.hot_x = hot_x;
    req.hot_y = hot_y;

    // no response, contrary to what the specs imply
    auto status = cursor_queue->write(span((uint8_t*)&req, sizeof(req)));

    if (!success(status))
        return false;

    return true;
}

static void virtio_gpu_flush_framebuffer(void* ctx, unsigned int x, unsigned int y,
                                         unsigned int width, unsigned int height) {
    auto resource = ctx ? (uint32_t)(uintptr_t)ctx : flush_framebuffer_resource;

    auto sp = span((uint8_t*)(framebuffer.data() + (y * fb_width)),
                   height * fb_width * sizeof(uint32_t));
    flush_cache(sp, false);

    visit([&](auto&& v) {
        virtio_gpu_rect r;

        r.x = x;
        r.y = y;
        r.width = width;
        r.height = height;

        if (!v->transfer_to_host(r, ((r.y * fb_width) + r.x) * sizeof(uint32_t),
                                 resource)) {
            print("virtio_gpu::transfer_to_host failed\n");
            return;
        }

        if (!v->resource_flush(r, resource)) {
            print("virtio_gpu::resource_flush failed\n");
            return;
        }
    }, flush_framebuffer_device);
}

static nt_status virtio_gpu_new_framebuffer_created(span<uint32_t> buffer,
                                                    unsigned int width,
                                                    unsigned int height,
                                                    void*& ctx) {
    return visit([&](auto&& v) -> nt_status {
        optional<uint64_t> fence_num;
        uint32_t resource = 0;

        auto opt_res = v->create_resource(VIRTIO_GPU_FORMAT::B8G8R8A8_UNORM,
                                          width, height);
        if (!opt_res.has_value()) {
            print("virtio_gpu::create_resource failed\n");
            return nt_status::INTERNAL_ERROR;
        }

        resource = *opt_res;

        if (!v->attach_backing(resource, span((uint8_t*)buffer.data(), buffer.size_bytes()))) {
            print("virtio_gpu::attach_backing failed\n");
            v->unref_resource(resource);
            return nt_status::INTERNAL_ERROR;
        }

        virtio_gpu_rect r;

        r.x = 0;
        r.y = 0;
        r.width = width;
        r.height = height;

        fence_num = v->fence.fetch_add(1);

        if (!v->transfer_to_host(r, 0, resource, fence_num)) {
            print("virtio_gpu::transfer_to_host failed\n");
            v->unref_resource(resource);
            return nt_status::INTERNAL_ERROR;
        }

        ctx = (void*)(uintptr_t)resource;

        return nt_status::SUCCESS;
    }, flush_framebuffer_device);
}

static nt_status virtio_gpu_framebuffer_set_active(void* ctx) {
    auto resource = ctx ? (uint32_t)(uintptr_t)ctx : flush_framebuffer_resource;

    return visit([&](auto&& v) -> nt_status {
        virtio_gpu_rect r;

        r.x = 0;
        r.y = 0;
        r.width = fb_width;
        r.height = fb_height;

        if (!v->set_scanout(r, 0, resource)) {
            print("virtio_gpu::set_scanout failed\n");
            return nt_status::INTERNAL_ERROR;
        }

        return nt_status::SUCCESS;
    }, flush_framebuffer_device);
}

static void virtio_gpu_framebuffer_deleted(void* ctx) {
    auto resource = (uint32_t)(uintptr_t)ctx;

    visit([&](auto&& v) {
        v->unref_resource(resource);
    }, flush_framebuffer_device);
}

static void virtio_gpu_move_cursor(uint32_t x, uint32_t y) {
    visit([&](auto&& v) {
        if (!v->move_cursor(0, x, y)) {
            print("virtio_gpu::move_cursor failed\n");
            return;
        }
    }, flush_framebuffer_device);
}

static void virtio_gpu_set_cursor(uint32_t x, uint32_t y,
                                  span<const uint32_t> buf, uint32_t hot_x,
                                  uint32_t hot_y) {
    visit([&](auto&& v) {
        optional<uint64_t> fence_num;
        uint32_t resource = 0;

        if (!buf.empty()) {
            static const uint32_t CURSOR_WIDTH = 64;
            static const uint32_t CURSOR_HEIGHT = 64;

            auto opt_res = v->create_resource(VIRTIO_GPU_FORMAT::B8G8R8A8_UNORM,
                                            CURSOR_WIDTH, CURSOR_HEIGHT);
            if (!opt_res.has_value()) {
                print("virtio_gpu::create_resource failed\n");
                return;
            }

            resource = *opt_res;

            if (!v->attach_backing(resource, span((uint8_t*)buf.data(), buf.size_bytes()))) {
                print("virtio_gpu::attach_backing failed\n");
                v->unref_resource(resource);
                return;
            }

            virtio_gpu_rect r;

            r.x = 0;
            r.y = 0;
            r.width = CURSOR_WIDTH;
            r.height = CURSOR_HEIGHT;

            fence_num = v->fence.fetch_add(1);

            if (!v->transfer_to_host(r, 0, resource, fence_num)) {
                print("virtio_gpu::transfer_to_host failed\n");
                v->unref_resource(resource);
                return;
            }
        }

        {
            lock_guard lock(v->cursor_lock);

            if (!v->update_cursor(0, x, y, resource, hot_x, hot_y, fence_num)) {
                print("virtio_gpu::update_cursor failed\n");

                if (resource != 0)
                    v->unref_resource(resource);

                return;
            }

            if (v->mouse_resource != 0) {
                if (!v->unref_resource(v->mouse_resource))
                    print("virtio_gpu::unref_resource failed\n");
            }

            v->mouse_resource = resource;
        }
    }, flush_framebuffer_device);
}

static void parse_video_option(string_view s, virtio_gpu_rect& r) {
    string_view wsv, hsv;

    if (auto x = s.find('x'); x == string_view::npos)
        return;
    else {
        wsv = string_view(s.data(), x);
        hsv = string_view(s.data() + x + 1, s.size() - x - 1);
    }

    if (wsv.empty() || hsv.empty())
        return;

    unsigned int w = 0;
    for (auto c : wsv) {
        if (c >= '0' && c <= '9') {
            w *= 10;
            w += (unsigned int)(c - '0');
        } else
            return;
    }

    unsigned int h = 0;
    for (auto c : hsv) {
        if (c >= '0' && c <= '9') {
            h *= 10;
            h += (unsigned int)(c - '0');
        } else
            return;
    }

    if (w == 0 || h == 0)
        return;

    r.width = w;
    r.height = h;
}

template<virtio_device T>
void virtio_gpu<T>::start() {
    virtio_gpu_resp_display_info disp_info;

    if (!get_display_info(disp_info)) {
        print("virtio_gpu::get_display_info failed\n");
        return;
    }

    if (!disp_info.pmodes[0].enabled) {
        print("virtio_gpu::start: scanout 0 not enabled\n");
        return;
    }

    auto& r = disp_info.pmodes[0].r;

    if (dev.negotiated_features & VIRTIO_GPU_F_EDID) {
        if (!get_edid(0, edid))
            print("virtio_gpu::get_edid failed\n");
        else
            have_edid = true;
    }

    // check video cmdline option

    string_view video_opt;

    loop_through_cmdline([&](string_view s) {
        static const string_view prefix = "video=";

        if (s.size() >= prefix.size() && string_view(s.data(), prefix.size()) == prefix) {
            video_opt = string_view(s.data() + prefix.size(), s.size() - prefix.size());
            return false;
        }

        return true;
    });

    if (!video_opt.empty())
        parse_video_option(video_opt, r);

    // FIXME - multiple scanouts

    // FIXME - unref resource in destructor
    auto res = create_resource(VIRTIO_GPU_FORMAT::B8G8R8A8_UNORM, r.width,
                               r.height);
    if (!res.has_value()) {
        print("virtio_gpu::create_resource failed\n");
        return;
    }

    // FIXME - vfree in destructor
    auto backing = (uint32_t*)vmalloc(sizeof(uint32_t) * r.width * r.height);
    if (!backing) {
        print("vmalloc failed\n");
        // FIXME - unref resource
        return;
    }

    if (!attach_backing(*res, span((uint8_t*)backing, sizeof(uint32_t) * r.width * r.height))) {
        print("virtio_gpu::attach_backing failed\n");
        // FIXME - vfree
        // FIXME - unref resource
    }

    if (!set_scanout(r, 0, *res)) {
        print("virtio_gpu::set_scanout failed\n");
        // FIXME - vfree
        // FIXME - unref resource
    }

    memset(backing, 0, sizeof(uint32_t) * r.width * r.height);
    flush_cache(span((uint8_t*)backing, sizeof(uint32_t) * r.width * r.height),
                false);

    flush_framebuffer_device = decltype(flush_framebuffer_device)(this);
    flush_framebuffer_resource = *res;
    flush_framebuffer = virtio_gpu_flush_framebuffer;
    new_framebuffer_created = virtio_gpu_new_framebuffer_created;
    framebuffer_set_active = virtio_gpu_framebuffer_set_active;
    framebuffer_deleted = virtio_gpu_framebuffer_deleted;
    ::move_cursor = virtio_gpu_move_cursor;
    set_cursor = virtio_gpu_set_cursor;

    if (!transfer_to_host(r, 0, *res)) {
        print("virtio_gpu::transfer_to_host failed\n");
        // FIXME - vfree
        // FIXME - unref resource
    }

    if (!resource_flush(r, *res)) {
        print("virtio_gpu::resource_flush failed\n");
        // FIXME - vfree
        // FIXME - unref resource
    }

    framebuffer = span(backing, r.width * r.height);
    fb_width = fb_pixels_per_scan_line = r.width;
    fb_height = r.height;
    fb_dpi = 72; // FIXME!

    shadow_fb_width = fb_width;
    shadow_fb_height = fb_height;

    console_type = console_type::other_framebuffer;
}

static atomic<uint32_t> virtio_gpu_num = 0;

static void found_virtio_pci_gpu(pci_device& dev) {
    auto d = new virtio_pci_device(dev, SUPPORTED_FEATURES, 2);
    if (!d) {
        print("new failed\n");
        return;
    }

    auto pv = new virtio_gpu(*d);
    if (!pv) {
        print("new failed\n");
        delete d;
        return;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return;

    v.dev.finalize();

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "virtgpu{}", virtio_gpu_num.fetch_add(1));

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status))
            print("name_object returned {}\n", status);
    }

    v.start();
}

static bool found_virtio_mmio_gpu(virtio_mmio_device& dev) {
    auto pv = new virtio_gpu(dev);
    if (!pv) {
        print("new failed\n");
        return false;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return false;

    v.dev.finalize();

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "virtgpu{}", virtio_gpu_num.fetch_add(1));

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    v.start();

    return true;
}
