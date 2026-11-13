// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <chrono>
#include <stddef.h>
#include <stdint.h>

export module efi;

import loaderblock;
import bsod;
import config;
import mem;
import print;
import timer;
import format;
import kernel_time_page;
import gnu_efi;

using namespace std;

static const uint64_t UEFI_VABLOCK = 0xfffffa6000000000;
export EFI_GET_TIME GetTime = nullptr;
export EFI_SET_TIME SetTime = nullptr;
export EFI_RESET_SYSTEM ResetSystem = nullptr;
export EFI_SET_VARIABLE SetVariable = nullptr;

extern "C++" void (*_printk)(string_view s);

// print not setup properly by the time map_uefi runs
template<typename... Args>
void do_print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];

    auto sp = span(buf, PAGE_SIZE);

    sp = format_to(sp, fmt.sv, make_tuple(args...));

    _printk(string_view(buf, (size_t)(sp.data() - buf)));
}

export bool map_uefi(const loader_block& lb) {
    uint64_t va = UEFI_VABLOCK;

#ifdef __aarch64__
    // work round buggy firmware trying to fixup non-writable code
    mark_lower_half_writable();
#endif

    auto d = (EFI_MEMORY_DESCRIPTOR*)lb.runtime_map_ptr;

    for (unsigned int i = 0; i < lb.num_runtime_map_entries; i++) {
        enum mapping_type mt;

        d->VirtualStart = va;

        // FIXME - read-only and NX flags

        if (d->Attribute & EFI_MEMORY_WB)
            mt = mapping_type::read_write_execute;
        else if (d->Attribute & EFI_MEMORY_UC)
            mt = mapping_type::no_cache;
        else
            mt = mapping_type::read_write_execute;

        for (unsigned int i = 0; i < d->NumberOfPages; i++) {
            if (!map_page<true>(d->PhysicalStart + (i << PAGE_SHIFT),
                                d->VirtualStart + (i << PAGE_SHIFT), mt)) {
                do_print("map_page failed\n");
                return false;
            }
        }

        if (!GetTime && (uintptr_t)lb.GetTime >= d->PhysicalStart && (uintptr_t)lb.GetTime < d->PhysicalStart + (d->NumberOfPages << PAGE_SHIFT))
            GetTime = (EFI_GET_TIME)((uintptr_t)lb.GetTime - d->PhysicalStart + va);

        if (!SetTime && (uintptr_t)lb.SetTime >= d->PhysicalStart && (uintptr_t)lb.SetTime < d->PhysicalStart + (d->NumberOfPages << PAGE_SHIFT))
            SetTime = (EFI_SET_TIME)((uintptr_t)lb.SetTime - d->PhysicalStart + va);

        if (!ResetSystem && (uintptr_t)lb.ResetSystem >= d->PhysicalStart && (uintptr_t)lb.ResetSystem < d->PhysicalStart + (d->NumberOfPages << PAGE_SHIFT))
            ResetSystem = (EFI_RESET_SYSTEM)((uintptr_t)lb.ResetSystem - d->PhysicalStart + va);

        if (!SetVariable && (uintptr_t)lb.SetVariable >= d->PhysicalStart && (uintptr_t)lb.SetVariable < d->PhysicalStart + (d->NumberOfPages << PAGE_SHIFT))
            SetVariable = (EFI_SET_VARIABLE)((uintptr_t)lb.SetVariable - d->PhysicalStart + va);

        va += d->NumberOfPages << PAGE_SHIFT;

        d = (EFI_MEMORY_DESCRIPTOR*)((uint8_t*)d + lb.runtime_map_entry_size);
    }

    auto ret = lb.SetVirtualAddressMap(lb.num_runtime_map_entries * lb.runtime_map_entry_size,
                                       lb.runtime_map_entry_size,
                                       EFI_MEMORY_DESCRIPTOR_VERSION,
                                       (EFI_MEMORY_DESCRIPTOR*)lb.runtime_map_ptr);
    if (ret != EFI_SUCCESS) { // FIXME - enum
        do_print("SetVirtualAddressMap returned {}\n", ret);
        return false;
    }

    return true;
}

export constexpr int64_t ymd_to_num(const chrono::year_month_day& d) noexcept {
    int m2 = ((int)(unsigned int)d.month() - 14) / 12;
    int64_t n;

    n = (1461 * ((int)d.year() + 4800 + m2)) / 4;
    n += (367 * ((int)(unsigned int)d.month() - 2 - (12 * m2))) / 12;
    n -= (3 * (((int)d.year() + 4900 + m2)/100)) / 4;
    n += (unsigned int)d.day();
    n -= 2337889;

    return n;
}

export void get_time_from_efi() {
    EFI_TIME now;

    auto ret = GetTime(&now, nullptr);
    if (ret != EFI_SUCCESS) { // FIXME - enum
        print("GetTime returned {:x}\n", ret);
        return;
    }

    auto ymd = chrono::year_month_day{chrono::year{now.Year}, chrono::month{now.Month},
                                      chrono::day{now.Day}};

    auto day_num = ymd_to_num(ymd);

    kernel_time_page.system_time_offset = (uint64_t)((((day_num * 86400) + (now.Hour * 3600) + (now.Minute * 60) + now.Second) * 10000000) - kernel_time_page.system_time);

    // FIXME - do something with timezone field
}
