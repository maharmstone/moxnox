// SPDX-License-Identifier: GPL-2.0-only
module;

#include <stdint.h>
#include <string.h>
#include <span>
#include <string_view>
#include <mutex>

module trace;

import nt;
import chardev;
import print;
import handle;
import object_ref;
import moxnox_ioctls;
import kernel_images;
import list;
import rwsem;
import mem;
import config;
import process;

using namespace std;

struct image_trace_point {
    uint32_t rva_name;
    uint32_t rva_addr;
    uint32_t rva_handler;
};

struct active_trace_point {
    active_trace_point(kernel_image& img, image_trace_point& tp) : img(img), tp(tp) { }

    list_entry le;
    kernel_image& img;
    image_trace_point& tp;
};

class tracectl : public chardev {
public:
    tracectl() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written,
                    ACCESS_MASK access_mask) override;
};

static rwsem trace_point_lock;
static list_entry active_trace_points = { &active_trace_points, &active_trace_points };

bool create_dev_tracectl() {
    auto pptr = new tracectl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "tracectl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

static nt_status do_enable_tracepoint(kernel_image& img, image_trace_point& tp) {
    auto atp = new active_trace_point(img, tp);
    if (!atp) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    bool already_done = false;

    {
        lock_guard lock(trace_point_lock);

        for (auto le = active_trace_points.next; le != &active_trace_points; le = le->next) {
            auto& atp2 = containing_record<active_trace_point>(le);

            if (&atp2.tp == &tp) {
                already_done = true;
                break;
            }
        }

        if (!already_done) {
            insert_tail_list(active_trace_points, atp->le);

            auto addr = (uintptr_t)((uint8_t*)img.base_address + tp.rva_addr);

#ifdef __x86_64__
            auto write_byte = [](uintptr_t addr, uint8_t byte) {
                auto pa = va_to_pa_lookup(addr & ~(PAGE_SIZE - 1)) | (addr & (PAGE_SIZE - 1));

                *(uint8_t*)phys_address(pa) = byte;
            };

            int32_t delta = tp.rva_handler - tp.rva_addr - 5;

            write_byte(addr, 0xcc); // int 3
            send_tracepoint_ipi();

            write_byte(addr + 1, (uint8_t)(delta & 0xff));
            write_byte(addr + 2, (uint8_t)((delta >> 8) & 0xff));
            write_byte(addr + 3, (uint8_t)((delta >> 16) & 0xff));
            write_byte(addr + 4, (uint8_t)((delta >> 24) & 0xff));
            send_tracepoint_ipi();

            write_byte(addr, 0xe9); // jmp

            uint32_t eax;

            // serialize
            __asm__ __volatile__ (
                "cpuid\n\t"
                : "=a" (eax)
                : "a" (0)
                : "ebx", "ecx", "edx"
            );
#elifdef __aarch64__
            auto pa = va_to_pa_lookup(addr & ~(PAGE_SIZE - 1)) | (addr & (PAGE_SIZE - 1));
            auto delta = (int32_t)tp.rva_handler - (int32_t)tp.rva_addr;
            *(uint32_t*)phys_address(pa) = 0x14000000 | (((int32_t)delta >> 2) & 0x03ffffff); // b handler

            __asm __volatile (
                "dc cvau, %x[addr]\n\t"
                "dsb ish\n\t"
                "ic ivau, %x[addr]\n\t"
                "dsb ish\n\t"
                "isb\n\t"
                :
                : [addr] "r" (addr)
            );

            send_tracepoint_ipi();
#endif
        }
    }

    if (already_done) {
        delete atp;
        return nt_status::SUCCESS;
    }

    return nt_status::SUCCESS;
}

static nt_status image_enable_tracepoint(kernel_image& img, string_view name) {
    if (img.trace.empty())
        return nt_status::NOT_FOUND;

    auto tps = span((image_trace_point*)img.trace.data(), img.trace.size() / sizeof(image_trace_point));

    for (auto& tp : tps) {
        auto* tp_name = (char*)img.base_address + tp.rva_name;

        if (!memcmp(tp_name, name.data(), name.size()) && tp_name[name.size()] == 0)
            return do_enable_tracepoint(img, tp);
    }

    return nt_status::NOT_FOUND;
}

static nt_status enable_tracepoint(string_view name) {
    nt_status status;

    if (name.empty())
        return nt_status::INVALID_PARAMETER;

    for (auto le = kernel_images.next; le != &kernel_images; le = le->next) {
        auto& img = containing_record<kernel_image>(le);

        status = image_enable_tracepoint(img, name);
        if (status != nt_status::NOT_FOUND)
            return status;
    }

    return nt_status::NOT_FOUND;
}

static nt_status disable_tracepoint(string_view name) {
    if (name.empty())
        return nt_status::INVALID_PARAMETER;

    active_trace_point* atp = nullptr;

    {
        lock_guard lock(trace_point_lock);

        for (auto le = active_trace_points.next; le != &active_trace_points; le = le->next) {
            auto& atp2 = containing_record<active_trace_point>(le);

            auto* tp_name = (char*)atp2.img.base_address + atp2.tp.rva_name;

            if (!memcmp(tp_name, name.data(), name.size()) && tp_name[name.size()] == 0) {
                remove_entry_list(atp2.le);
                atp = &atp2;
                break;
            }
        }

        if (!atp)
            return nt_status::NOT_FOUND;

        auto addr = (uintptr_t)((uint8_t*)atp->img.base_address + atp->tp.rva_addr);

#ifdef __x86_64__
        auto write_byte = [](uintptr_t addr, uint8_t byte) {
            auto pa = va_to_pa_lookup(addr & ~(PAGE_SIZE - 1)) | (addr & (PAGE_SIZE - 1));

            *(uint8_t*)phys_address(pa) = byte;
        };

        write_byte(addr, 0xcc); // int 3
        send_tracepoint_ipi();

        write_byte(addr + 1, 0x1f);
        write_byte(addr + 2, 0x44);
        write_byte(addr + 3, 0x00);
        write_byte(addr + 4, 0x00);
        send_tracepoint_ipi();

        write_byte(addr, 0x0f);

        uint32_t eax;

        // serialize
        __asm__ __volatile__ (
            "cpuid\n\t"
            : "=a" (eax)
            : "a" (0)
            : "ebx", "ecx", "edx"
        );
#elifdef __aarch64__
        auto pa = va_to_pa_lookup(addr & ~(PAGE_SIZE - 1)) | (addr & (PAGE_SIZE - 1));
        *(uint32_t*)phys_address(pa) = 0xd503201f; // nop

        __asm __volatile (
            "dc cvau, %x[addr]\n\t"
            "dsb ish\n\t"
            "ic ivau, %x[addr]\n\t"
            "dsb ish\n\t"
            "isb\n\t"
            :
            : [addr] "r" (addr)
        );

        send_tracepoint_ipi();
#endif
    }

    delete atp;

    return nt_status::SUCCESS;
}

nt_status tracectl::ioctl(uint32_t code, span<const uint8_t> inbuf,
                          span<uint8_t> /*outbuf*/, uintptr_t& /*written*/,
                          ACCESS_MASK /*access_mask*/) {
    switch (code) {
        case IOCTL_MOXNOX_TRACECTL_ENABLE:
            return enable_tracepoint(string_view((char*)inbuf.data(), inbuf.size()));

        case IOCTL_MOXNOX_TRACECTL_DISABLE:
            return disable_tracepoint(string_view((char*)inbuf.data(), inbuf.size()));

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

bool is_trace_point(uintptr_t addr) {
    auto pimg = get_kernel_image_from_address(addr);
    if (!pimg)
        return false;

    auto& img = *pimg;

    if (img.trace.empty())
        return false;

    auto rva = (uint32_t)(addr - (uintptr_t)img.base_address);

    auto tps = span((image_trace_point*)img.trace.data(), img.trace.size() / sizeof(image_trace_point));

    for (auto& tp : tps) {
        if (tp.rva_addr == rva)
            return true;
    }

    return false;
}
