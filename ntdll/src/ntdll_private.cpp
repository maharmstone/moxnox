// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <stddef.h>
#include <stdint.h>
#include <string_view>
#include <span>

export module ntdll_private;

import ntdll;
import crt;
import pagesize;

export {

constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

struct ldr_entry {
    LDR_DATA_TABLE_ENTRY ldr;
    uint64_t offset_debug_info;
    uint64_t size_debug_info;
    uint64_t offset_debug_abbrev;
    uint64_t size_debug_abbrev;
    uint64_t offset_debug_rnglists;
    uint64_t size_debug_rnglists;
    uint64_t offset_debug_str;
    uint64_t size_debug_str;
    uint64_t offset_debug_addr;
    uint64_t size_debug_addr;
    uint64_t offset_debug_str_offsets;
    uint64_t size_debug_str_offsets;
    uint64_t offset_debug_line;
    uint64_t size_debug_line;
    uint64_t offset_debug_line_str;
    uint64_t size_debug_line_str;
};

using DLL_ENTRY_PROC = bool (*)(void*, uint32_t, void*);

// ntdll.cpp
extern "C++" void call_tls_callbacks(const LDR_DATA_TABLE_ENTRY& ent, uint32_t reason);
extern "C++" void call_process_detach_img(LDR_DATA_TABLE_ENTRY& ent,
                                          bool process_exit);

// time.cpp
extern "C++" void clear_alarm_on_fork();

// heap.cpp
extern "C++" void clear_heap_lock_on_fork();

}
