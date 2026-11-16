// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stddef.h>
#include <stdint.h>
#include <new>
#include <algorithm>
#include <array>

import nt;
import print;
import list;
import ntdll;
import ntdll_private;
import crt;
import pagesize;

using namespace std;

struct alloc_header {
    uint32_t size : 31;
    uint32_t free : 1;
    uint32_t align_shift;
    uint8_t _pad[8];
};

static_assert(sizeof(alloc_header) == 16);

struct free_block : alloc_header {
    list_entry le;
};

static constexpr size_t NUM_BINS = 76;

class subheap {
public:
    subheap(size_t reserve_size, size_t commit_size,
            array<list_entry, NUM_BINS>& free_bins);

    list_entry le;
    size_t reserve_size;
    size_t commit_size;
    uint8_t data[0];
};

class heap {
public:
    heap(size_t reserve_size, size_t commit_size);
    ~heap();

    void* allocate(uint32_t flags, size_t size);
    void* reallocate(uint32_t flags, void* ptr, size_t size);
    void* allocate_aligned(size_t alignment, size_t size);
    bool free(uint32_t flags, void* base_address);

    list_entry subheaps = { &subheaps, &subheaps };
    RTL_CRITICAL_SECTION cs;
    size_t reserve_size, commit_size;
    array<list_entry, NUM_BINS> free_bins;
};

static size_t get_bin_number(size_t size) {
    if (size > 1024) {
        if (size > 32768)
            return 75;
        else if (size > 16384)
            return 74;
        else if (size > 8192)
            return 73;
        else if (size > 4096)
            return 72;
        else if (size > 3584)
            return 71;
        else if (size > 3072)
            return 70;
        else if (size > 2560)
            return 69;
        else if (size > 2048)
            return 68;
        else if (size > 1792)
            return 67;
        else if (size > 1536)
            return 66;
        else if (size > 1280)
            return 65;
        else
            return 64;
    }

    return (size - 1) / 16;
}

subheap::subheap(size_t reserve_size, size_t commit_size, array<list_entry, NUM_BINS>& free_bins) : reserve_size(reserve_size), commit_size(commit_size) {
    auto& fb = *(free_block*)data;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    fb.size = (uint32_t)(commit_size - offsetof(subheap, data) - sizeof(alloc_header));
    fb.size -= sizeof(alloc_header); // for sentinel
#pragma GCC diagnostic pop
    fb.free = 1;
    insert_tail_list(free_bins[get_bin_number(fb.size)], fb.le);

    auto& sentinel = *(alloc_header*)((uint8_t*)&fb + sizeof(alloc_header) + fb.size);
    sentinel.size = 0;
    sentinel.free = 0;
    sentinel.align_shift = 0;
}

heap::heap(size_t reserve_size, size_t commit_size) : reserve_size(reserve_size), commit_size(commit_size) {
    void* addr = nullptr;

    for (auto& b : free_bins) {
        b.next = &b;
        b.prev = &b;
    }

    auto ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &reserve_size,
                                       MEM_RESERVE, PAGE_READWRITE);

    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned {}\n", ret);
        // FIXME - return error
        return;
    }

    ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &commit_size,
                                  MEM_COMMIT, PAGE_READWRITE);

    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned {}\n", ret);
        // FIXME - return error
        return;
    }

    auto sh = new (addr) subheap(reserve_size, commit_size, free_bins);

    insert_tail_list(subheaps, sh->le);

    RtlInitializeCriticalSection(&cs);
}

heap::~heap() {
    RtlDeleteCriticalSection(&cs);
    auto le = subheaps.next;

    while (le != &subheaps) {
        auto& sh = containing_record<subheap>(le);
        auto next = le->next;

        void* addr = &sh;
        size_t region_size = 0;

        NtFreeVirtualMemory(NtCurrentProcess(), &addr, &region_size, MEM_RELEASE);

        le = next;
    }
}

static void mark_block_allocated(free_block& fb, size_t size,
                                 array<list_entry, NUM_BINS>& free_bins) {
    if (fb.size >= size + sizeof(free_block)) {
        auto& nh = *(free_block*)((uint8_t*)&fb + sizeof(alloc_header) + size);

        nh.free = 1;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        nh.size = fb.size - size - sizeof(alloc_header);
#pragma GCC diagnostic pop

        insert_tail_list(free_bins[get_bin_number(nh.size)], nh.le);
    }

    fb.free = 0;
    fb.align_shift = 0;

    remove_entry_list(fb.le);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
    fb.size = size;
#pragma GCC diagnostic pop
}

void* heap::allocate(uint32_t flags, size_t size) {
    // FIXME - flags

    // round to multiple of 16
    if (size & 15)
        size = ((size >> 4) + 1) << 4;
    else if (size == 0)
        size = 16;

    auto bin = get_bin_number(size);

    if (size >= 0x80000000)
        return nullptr;

    RtlEnterCriticalSection(&cs);

    while (bin < free_bins.size()) {
        for (auto le = free_bins[bin].next; le != &free_bins[bin]; le = le->next) {
            auto& fb = containing_record<free_block>(le);

            if (fb.size < size)
                continue;

            mark_block_allocated(fb, size, free_bins);

            auto ret = (uint8_t*)&fb + sizeof(alloc_header);

            if (flags & HEAP_ZERO_MEMORY)
                memset(ret, 0, size);

            RtlLeaveCriticalSection(&cs);

            return ret;
        }

        bin++;
    }

    // FIXME - return nullptr if heap not growable
    // FIXME - separate logic if "large" allocation?

    void* sh_addr = nullptr;
    size_t subheap_size = offsetof(subheap, data) + sizeof(alloc_header) +
                          size + sizeof(alloc_header);
    size_t sh_reserve = max(reserve_size, subheap_size);
    size_t sh_commit  = max(commit_size, subheap_size);

    auto status = NtAllocateVirtualMemory(NtCurrentProcess(), &sh_addr, 0, &sh_reserve,
                                          MEM_RESERVE, PAGE_READWRITE);

    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        RtlLeaveCriticalSection(&cs);
        return nullptr;
    }

    status = NtAllocateVirtualMemory(NtCurrentProcess(), &sh_addr, 0, &sh_commit,
                                     MEM_COMMIT, PAGE_READWRITE);
    if (!success(status)) {
        print("NtAllocateVirtualMemory returned {}\n", status);
        RtlLeaveCriticalSection(&cs);
        return nullptr;
    }

    auto sh = new (sh_addr) subheap(sh_reserve, sh_commit, free_bins);

    insert_tail_list(subheaps, sh->le);

    auto& shfb = *(free_block*)sh->data;

    mark_block_allocated(shfb, size, free_bins);

    auto ret = (uint8_t*)&shfb + sizeof(alloc_header);

    if (flags & HEAP_ZERO_MEMORY)
        memset(ret, 0, size);

    RtlLeaveCriticalSection(&cs);
    return ret;
}

void* heap::reallocate(uint32_t flags, void* ptr, size_t size) {
    // FIXME - flags?

    if (!ptr)
        return allocate(flags, size);

    if (size == 0) {
        free(flags, ptr);
        return nullptr;
    }

    // reallocating an aligned allocation doesn't preserve the alignment

    // round to multiple of 16, same as allocate
    size_t rounded_size = size;
    if (rounded_size & 15)
        rounded_size = ((rounded_size >> 4) + 1) << 4;

    auto ah = (alloc_header*)((uint8_t*)ptr - sizeof(alloc_header));

    if (ah->align_shift != 0)
        ah = (alloc_header*)((uint8_t*)ah - ah->align_shift);

    RtlEnterCriticalSection(&cs);

    auto old_size = (size_t)ah->size;

    RtlLeaveCriticalSection(&cs);

    if (rounded_size <= old_size)
        return ptr;

    auto ret = allocate(flags, size);
    if (!ret)
        return nullptr;

    memcpy(ret, ptr, old_size);

    free(flags, ptr);

    return ret;
}

bool heap::free(uint32_t, void* base_address) {
    // FIXME - flags

    auto* fb = (free_block*)((uint8_t*)base_address - sizeof(alloc_header));

    if (fb->align_shift != 0)
        fb = (free_block*)((uint8_t*)fb - fb->align_shift);

    auto bin = get_bin_number(fb->size);

    RtlEnterCriticalSection(&cs);

    fb->free = 1;
    insert_tail_list(free_bins[bin], fb->le);

    // FIXME - decommit part of heap if last page(s) now clear?

    RtlLeaveCriticalSection(&cs);

    return true;
}

static constexpr size_t round_up(size_t val, uint64_t div) {
    if (val & (div - 1)) {
        val &= ~(div - 1);
        val += div;
    }

    return val;
}

extern "C"
void* RtlCreateHeap(uint32_t, void*, size_t reserve_size, size_t commit_size,
                    void*, RTL_HEAP_PARAMETERS*) {
    void* addr = nullptr;

    // FIXME - flags
    // FIXME - heap_base
    // FIXME - parameters

    reserve_size = round_up(reserve_size, PAGE_SIZE);
    commit_size = round_up(commit_size, PAGE_SIZE);

    // see https://docs.microsoft.com/en-us/windows-hardware/drivers/ddi/ntifs/nf-ntifs-rtlcreateheap
    if (reserve_size == 0) {
        if (commit_size == 0) {
            reserve_size = 64 << PAGE_SHIFT;
            commit_size = PAGE_SIZE;
        } else
            reserve_size = round_up(commit_size, 16 * PAGE_SIZE);
    } else {
        if (commit_size == 0)
            commit_size = PAGE_SIZE;
        else if (commit_size > reserve_size)
            commit_size = reserve_size;
    }

    size_t heap_obj_size = sizeof(heap);

    auto ret = NtAllocateVirtualMemory(NtCurrentProcess(), &addr, 0, &heap_obj_size,
                                       MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (!success(ret)) {
        print("NtAllocateVirtualMemory returned {}\n", ret);
        return nullptr;
    }

    auto h = new (addr) heap(reserve_size, commit_size);

    return h;
}

extern "C"
void* RtlAllocateHeap(void* heap_handle, uint32_t flags, size_t size) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.allocate(flags, size);
}

extern "C"
void* RtlReAllocateHeap(void* heap_handle, uint32_t flags, void* ptr,
                        size_t size) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.reallocate(flags, ptr, size);
}

extern "C"
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address) {
    if (!base_address)
        return true;

    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.free(flags, base_address);
}

extern "C"
void* RtlDestroyHeap(void* heap_handle) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    h.heap::~heap();

    size_t region_size = 0;

    auto ret = NtFreeVirtualMemory(NtCurrentProcess(), &heap_handle, &region_size, MEM_RELEASE);

    if (!success(ret)) {
        print("NtFreeVirtualMemory returned {}\n", ret);
        return nullptr;
    }

    return nullptr;
}

extern "C"
void* malloc(size_t size) {
    return RtlAllocateHeap((void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap((void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap, 0, ptr);
}

extern "C"
void* calloc(size_t nmemb, size_t size) {
    // FIXME - should test for overflow of nmemb * size

    auto ptr = malloc(nmemb * size);
    if (!ptr)
        return nullptr;

    memset(ptr, 0, nmemb * size);

    return ptr;
}

void* operator new(size_t count) {
    return malloc(count);
}

void operator delete(void* ptr) noexcept {
    free(ptr);
}

void operator delete(void* ptr, size_t) noexcept {
    free(ptr);
}

void* operator new[](size_t count) {
    return malloc(count);
}

void operator delete[](void* ptr) noexcept {
    free(ptr);
}

void operator delete[](void* ptr, size_t) noexcept {
    free(ptr);
}

extern "C"
void* RtlSecureZeroMemory(void* ptr, size_t count) {
    auto p = (volatile uint8_t*)ptr;

    while (count > 0) {
        *p = 0;
        p++;
        count--;
    }

    return ptr;
}

extern "C"
void* mn_allocate_aligned(void* heap_handle, size_t alignment, size_t size) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    return h.allocate_aligned(alignment, size);
}

void* heap::allocate_aligned(size_t alignment, size_t size) {
    alignment = max(alignment, sizeof(alloc_header));

    auto m = (uint8_t*)this->allocate(0, sizeof(alloc_header) + size + alignment - 1);
    if (!m)
        return nullptr;

    auto ret = m;

    if ((uintptr_t)ret % alignment)
        ret += alignment - ((uintptr_t)ret % alignment);

    if ((size_t)(ret - m) < sizeof(alloc_header))
        ret += alignment;

    auto& ah = *(alloc_header*)(ret - sizeof(alloc_header));

    ah.free = 0;
    ah.align_shift = (uint32_t)(ret - m);

    return ret;
}

extern "C"
bool RtlLockHeap(void* heap_handle) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    RtlEnterCriticalSection(&h.cs);

    return true;
}

extern "C"
bool RtlUnlockHeap(void* heap_handle) {
    auto& h = *reinterpret_cast<heap*>(heap_handle);

    RtlLeaveCriticalSection(&h.cs);

    return true;
}

void clear_heap_lock_on_fork() {
    auto& h = *reinterpret_cast<heap*>((void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap);

    RtlDeleteCriticalSection(&h.cs);
    RtlInitializeCriticalSection(&h.cs);
}
