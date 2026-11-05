// SPDX-License-Identifier: GPL-2.0-only
module;

#include <string.h>
#include <mutex>
#include <variant>
#include <span>
#include <algorithm>
#include <atomic>
#include "assert.h"

module mapping;

import print;
import config;
import vfs;
import section;
import rangelock;
import page_cache;
import mem;
import bsod;

using namespace std;

mapping::mapping(process& proc, section& sect) : proc(proc), sect(sect) {
    sect.acquire();

    visit([&](auto* ptr) {
        auto& cache = *ptr;

        {
            lock_guard lg(cache.base.mappings_lock);
            insert_tail_list(cache.base.mappings, cache_le);
        }
    }, sect.get_cache());
}

#ifdef __x86_64__
void unmap_proc_pages(process& proc, uint64_t va, uint64_t num_pages) {
    span<HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((HARDWARE_PTE*)phys_address(proc.cr3), 512);

    auto redo_level = [&](uint8_t level) {
        pt[level - 1] = span((HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    };

    for (uint8_t level = 3; level > 0; level--) {
        redo_level(level);
    }

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i != 0) {
            index[0]++;

            if (index[0] == 512) {
                index[0] = 0;
                index[1]++;

                if (index[1] == 512) {
                    index[1] = 0;
                    index[2]++;

                    if (index[2] == 512) {
                        index[2] = 0;
                        index[3]++;

                        redo_level(3);
                    }

                    redo_level(2);
                }

                redo_level(1);
            }
        }

        auto& pte = pt[0][index[0]];

        // free COWed folios
        if (pte.p && pte.owned && pte.cow)
            free_folio(pte.pfn << PAGE_SHIFT);

        pte.p = 0;
        pte.owned = 0;
    }
}
#elifdef __aarch64__
void unmap_proc_pages(process& proc, uint64_t va, uint64_t num_pages) {
    span<ARM64_HARDWARE_PTE> pt[4];
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

    pt[3] = span((ARM64_HARDWARE_PTE*)phys_address(proc.ttbr0), 512);

    auto redo_level = [&](uint8_t level) {
        pt[level - 1] = span((ARM64_HARDWARE_PTE*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    };

    for (uint8_t level = 3; level > 0; level--) {
        redo_level(level);
    }

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i != 0) {
            index[0]++;

            if (index[0] == 512) {
                index[0] = 0;
                index[1]++;

                if (index[1] == 512) {
                    index[1] = 0;
                    index[2]++;

                    if (index[2] == 512) {
                        index[2] = 0;
                        index[3]++;

                        redo_level(3);
                    }

                    redo_level(2);
                }

                redo_level(1);
            }
        }

        auto& pte = pt[0][index[0]];

        // free COWed folios
        if (pte.valid && pte.owned && pte.cow)
            free_folio(pte.pfn << PAGE_SHIFT);

        pte.valid = 0;
        pte.owned = 0;
    }
}
#endif

mapping::~mapping() {
    // FIXME - send IPI to unmap on other CPUs by clearing TLB

    if (in_address_space)
        unmap_proc_pages(proc, tree.key, size >> PAGE_SHIFT);

    visit([&](auto* ptr) {
        auto& cache = *ptr;

        {
            lock_guard lg(cache.base.mappings_lock);
            remove_entry_list(cache_le);
        }
    }, sect.get_cache());

    sect.release();
}

bool mapping::map(uint32_t mapping_offset, uint64_t va) {
    uint64_t page_num = mapping_offset >> PAGE_SHIFT;
    uint64_t sect_page_num = page_num + (offset >> PAGE_SHIFT);

    return visit([&](auto* ptr) {
        uint64_t read_pages;
        auto& cache = *ptr;

        auto status = cache.read_into_cache_and_pin(sect_page_num, 1, read_pages);
        if (!success(status)) {
            print("page_cache::read_into_cache_and_pin returned {}\n", status);
            return false;
        }

        auto pte = find_pte(proc, va);
        if (!pte) {
            cache.unpin(sect_page_num, 1);
            return false;
        }

        {
            page_cache_entry* p;
            rangelock_req rlr(cache.base.rl, sect_page_num, 1, rangelock_mode::exclusive);

            {
                lock_guard lg(cache.base.cache_lock);
                p = cache.find_page_cache_entry(sect_page_num);
            }

#ifdef __aarch64__
            pte->pfn = p->pfn;
            pte->AP = ARM64_PTE_AP_EL0RO;
            pte->table_entry = 1;
            pte->SH = 3; // inner shareable
            pte->AF = 1;
            pte->valid = 1;
            pte->AttrIndex = attr_index_normal;
#elifdef __x86_64__
            pte->pfn = p->pfn & PFN_MASK;
            pte->p = 1;
            pte->w = 0;
#endif
        }

        cache.unpin(sect_page_num, 1);

        return true;
    }, sect.get_cache());
}

bool mapping::copy_from(uint32_t offset, void* ks_addr, uint32_t size) {
    uint64_t page_num = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + size + PAGE_SIZE - 1) >> PAGE_SHIFT) - page_num;
    uint64_t page_offset = offset & (PAGE_SIZE - 1);

    {
        rangelock_req rlr(proc.rl, (tree.key >> PAGE_SHIFT) + page_num,
                          num_pages, rangelock_mode::shared);

        for (uint64_t i = 0; i < num_pages; i++) {
            auto* ppte = find_pte(proc, tree.key + ((page_num + i) << PAGE_SHIFT));

            if (!ppte)
                return false;

            auto& pte = *ppte;

            auto to_copy = min((uint32_t)(PAGE_SIZE - page_offset), size);

#ifdef __aarch64__
            bool pte_valid = pte.valid;
#elifdef __x86_64__
            bool pte_valid = pte.p;
#endif

            if (pte_valid) {
                memcpy(ks_addr, (uint8_t*)phys_address(pte.pfn << PAGE_SHIFT) + page_offset,
                       to_copy);
            } else {
                page_cache_entry* p;

                uint64_t sect_page_num = page_num + i + (this->offset >> PAGE_SHIFT);

                auto b = visit([&](auto* ptr) {
                    uint64_t read_pages;
                    auto& cache = *ptr;

                    auto status = cache.read_into_cache_and_pin(sect_page_num, 1,
                                                                read_pages);
                    if (!success(status)) {
                        print("page_cache::read_into_cache_and_pin returned {}\n",
                              status);
                        return false;
                    }

                    {
                        lock_guard lg(cache.base.cache_lock);
                        p = cache.find_page_cache_entry(sect_page_num);
                    }

                    {
                        rangelock_req rlr(cache.base.rl, sect_page_num, 1, rangelock_mode::exclusive);

                        memcpy(ks_addr, (uint8_t*)get_phys_address(*p) + page_offset,
                               to_copy);
                    }

                    cache.unpin(sect_page_num, 1);

                    return true;
                }, sect.get_cache());

                if (!b)
                    return false;
            }

            ks_addr = (uint8_t*)ks_addr + to_copy;
            size -= to_copy;
            page_offset = 0;
        }
    }

    return true;
}

bool mapping::copy_to(uint32_t offset, const void* ks_addr, uint32_t size) {
    uint64_t page_num = offset >> PAGE_SHIFT;
    uint64_t num_pages = ((offset + size + PAGE_SIZE - 1) >> PAGE_SHIFT) - page_num;
    uint64_t page_offset = offset & (PAGE_SIZE - 1);

    {
        rangelock_req rlr(proc.rl, (tree.key >> PAGE_SHIFT) + page_num,
                          num_pages, rangelock_mode::exclusive);

        for (uint64_t i = 0; i < num_pages; i++) {
            uint64_t va = tree.key + ((page_num + i) << PAGE_SHIFT);
            auto* ppte = find_pte(proc, va);

            if (!ppte)
                return false;

            auto& pte = *ppte;
            auto to_copy = min((uint32_t)(PAGE_SIZE - page_offset), size);

            if (!pte.allow_write)
                return false;

#ifdef __aarch64__
            bool pte_valid = pte.valid;
            bool pte_writable = pte.AP == ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
            bool pte_valid = pte.p;
            bool pte_writable = pte.w;
#endif

            if (pte_valid && pte_writable) {
                memcpy((uint8_t*)phys_address(pte.pfn << PAGE_SHIFT) + page_offset,
                       ks_addr, to_copy);
            } else if (pte.cow) {
                uint64_t pa;

                if (!pte_valid) {
                    pa = allocate_folio(0, page_state::process_memory);
                    if (!pa)
                        return false;

                    if (to_copy < PAGE_SIZE) {
                        page_cache_entry* p;
                        uint64_t sect_page_num = page_num + i + (this->offset >> PAGE_SHIFT);

                        auto b = visit([&](auto* ptr) {
                            uint64_t read_pages;
                            auto& cache = *ptr;

                            auto status = cache.read_into_cache_and_pin(sect_page_num, 1,
                                                                        read_pages);
                            if (!success(status)) {
                                print("page_cache::read_into_cache_and_pin returned {}\n",
                                    status);
                                free_folio(pa);
                                return false;
                            }

                            {
                                lock_guard lg(cache.base.cache_lock);
                                p = cache.find_page_cache_entry(sect_page_num);
                            }

                            {
                                rangelock_req rlr(cache.base.rl, sect_page_num, 1, rangelock_mode::exclusive);

                                memcpy(phys_address(pa), get_phys_address(*p),
                                       PAGE_SIZE);
                            }

                            cache.unpin(sect_page_num, 1);

                            return true;
                        }, sect.get_cache());

                        if (!b)
                            return false;
                    }
                } else {
                    if (pte.owned) {
                        pa = cow_folio(pte.pfn << PAGE_SHIFT,
                                       page_state::process_memory);
                        if (!pa) {
                            print("cow_folio failed\n");
                            return false;
                        }
                    } else {
                        pa = allocate_folio(0, page_state::process_memory);
                        if (!pa)
                            return false;

                        memcpy(phys_address(pa), phys_address(pte.pfn << PAGE_SHIFT),
                               PAGE_SIZE);
                    }
                }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                pte.pfn = pa >> PAGE_SHIFT;
#pragma GCC diagnostic pop

                memcpy((uint8_t*)phys_address(pte.pfn << PAGE_SHIFT) + page_offset,
                       ks_addr, to_copy);

#ifdef __aarch64__
                pte.AP = ARM64_PTE_AP_EL0RW;
                pte.owned = 1;

                if (!pte.valid) {
                    pte.table_entry = 1;
                    pte.AttrIndex = attr_index_normal;
                    pte.SH = 3; // inner shareable
                    pte.AF = 1;
                    pte.valid = 1;
                }

                __asm __volatile (
                    "tlbi vae1, %x[res]\n"
                    "dsb ish\n"
                    "isb\n"
                    :
                    : [res] "r" (va >> PAGE_SHIFT)
                    : "memory"
                );
#elifdef __x86_64__
                pte.w = 1;
                pte.owned = 1;

                if (!pte.p)
                    pte.p = 1;

                __asm __volatile (
                    "invlpg [%0]\n"
                    :
                    : "r" (va)
                    : "memory"
                );
#endif

                if (pte_valid)
                    send_page_tlb_ipi(proc, va, 1);
            } else {
                uint64_t sect_page_num = page_num + i + (this->offset >> PAGE_SHIFT);
                page_cache_entry* p;

                auto b = visit([&](auto* ptr) {
                    auto& cache = *ptr;

                    if (to_copy < PAGE_SIZE) {
                        uint64_t read_pages;

                        auto status = cache.read_into_cache_and_pin(sect_page_num, 1,
                                                                    read_pages);
                        if (!success(status)) {
                            print("page_cache::read_into_cache_and_pin returned {}\n",
                                status);
                            return false;
                        }
                    } else
                        cache.pin(sect_page_num, 1, nullptr);

                    {
                        lock_guard lg(cache.base.cache_lock);
                        p = cache.find_page_cache_entry(sect_page_num);
                    }

                    bool send_ipi = false;

                    {
                        rangelock_req rlr(cache.base.rl, sect_page_num, 1, rangelock_mode::exclusive);

                        if (p->pfn == 0) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                            p->pfn = allocate_folio(0, page_state::cache) >> PAGE_SHIFT;
#pragma GCC diagnostic pop

                            if (!p->pfn) {
                                cache.unpin(sect_page_num, 1);
                                return false;
                            }
                        } else if (!p->dirty) {
                            auto pfn = cow_folio(p->pfn << PAGE_SHIFT, page_state::cache) >> PAGE_SHIFT;
                            if (pfn == 0) {
                                print("cow_folio failed\n");
                                cache.unpin(sect_page_num, 1);
                                return false;
                            }

                            send_ipi = p->pfn != pfn;
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                            p->pfn = pfn;
#pragma GCC diagnostic pop
                        }

                        p->dirty = 1;
                        cache.mark_dirty();

                        memcpy((uint8_t*)get_phys_address(*p) + page_offset,
                               ks_addr, to_copy);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                        pte.pfn = p->pfn;
#pragma GCC diagnostic pop
                    }

#ifdef __aarch64__
                    pte.table_entry = 1;
                    pte.AttrIndex = attr_index_normal;
                    pte.AP = ARM64_PTE_AP_EL0RW;
                    pte.SH = 3; // inner shareable
                    pte.AF = 1;
                    pte.valid = 1;

                    __asm __volatile (
                        "tlbi vae1, %x[res]\n"
                        "dsb ish\n"
                        "isb\n"
                        :
                        : [res] "r" (va >> PAGE_SHIFT)
                        : "memory"
                    );
#elifdef __x86_64__
                    pte.w = 1;
                    pte.p = 1;

                    __asm __volatile (
                        "invlpg [%0]\n"
                        :
                        : "r" (va)
                        : "memory"
                    );
#endif

                    if (send_ipi)
                        send_page_tlb_ipi(proc, va, 1);

                    cache.unpin(sect_page_num, 1);

                    return true;
                }, sect.get_cache());

                if (!b)
                    return false;
            }

            ks_addr = (uint8_t*)ks_addr + to_copy;
            size -= to_copy;
            page_offset = 0;
        }
    }

    return true;
}

nt_status mapping::flush(uint64_t offset, uint64_t size) {
    return sect.flush((this->offset + offset) >> PAGE_SHIFT, size >> PAGE_SHIFT);
}

void mapping::clear_write_bit(uint64_t page_num, uint64_t num_pages) {
    span<pte_type> pt[4];
    uint64_t va = tree.key + (page_num << PAGE_SHIFT);
    uint16_t index[4];

    index[0] = (va & 0x1ff000) >> 12;
    index[1] = (va & 0x3fe00000) >> 21;
    index[2] = (va & 0x7fc0000000) >> 30;
    index[3] = (va & 0xff8000000000) >> 39;

#ifdef __aarch64__
    pt[3] = span((pte_type*)phys_address(proc.ttbr0), 512);
#elifdef __x86_64__
    pt[3] = span((pte_type*)phys_address(proc.cr3), 512);
#endif

    auto redo_level = [&](uint8_t level) {
        pt[level - 1] = span((pte_type*)phys_address((uint64_t)((pt[level][index[level]].pfn) << PAGE_SHIFT)), 512);
    };

    for (uint8_t level = 3; level > 0; level--) {
        redo_level(level);
    }

    for (uint64_t i = 0; i < num_pages; i++) {
        if (i != 0) {
            index[0]++;

            if (index[0] == 512) {
                index[0] = 0;
                index[1]++;

                if (index[1] == 512) {
                    index[1] = 0;
                    index[2]++;

                    if (index[2] == 512) {
                        index[2] = 0;
                        index[3]++;

                        redo_level(3);
                    }

                    redo_level(2);
                }

                redo_level(1);
            }
        }

        auto& pte = pt[0][index[0]];

        if (pte.cow || pte.reserved)
            continue;

#ifdef __aarch64__
        pte.AP = ARM64_PTE_AP_EL0RO;

        __asm __volatile (
            "tlbi vae1, %x[res]\n"
            "dsb ish\n"
            "isb\n"
            :
            : [res] "r" ((va >> PAGE_SHIFT) + i)
        );
#elifdef __x86_64__
        pte.w = 0;

        __asm __volatile (
            "invlpg [%0]\n"
            :
            : "r" (va + (i << PAGE_SHIFT))
        );
#endif
    }

    // send IPIs to invalidate TLBs on other processors
    send_page_tlb_ipi(proc, va, num_pages);
}

bool mapping::try_acquire() {
    auto expected = refcount.load(memory_order_relaxed);

    while (expected != 0) {
        if (refcount.compare_exchange_weak(expected, expected + 1,
                                           memory_order_acquire,
                                           memory_order_relaxed)) {
            return true;
        }
    }

    return false;
}

mapping* find_mapping(process& proc, uint64_t address) {
    address &= ~(PAGE_SIZE - 1);

    lock_guard lock(proc.mappings_lock);

    auto n = find_rbtree_node_or_less(proc.mappings, address);

    if (!n)
        return nullptr;

    auto& m = rbtree_containing_record<mapping>(n);

    if (m.tree.key <= address && m.tree.key + m.size > address) {
        if (!m.try_acquire())
            return nullptr;

        return &m;
    }

    return nullptr;
}

bool make_pte_writable(uintptr_t address, mapping& m, pte_type& pte,
                       bool& send_ipi) {
    uint64_t page_num = (address - m.tree.key) >> PAGE_SHIFT;
    uint64_t sect_page_num = page_num + (m.offset >> PAGE_SHIFT);

    if (pte.cow) {
        uint64_t pfn;

        if (pte.owned) {
            pfn = cow_folio(pte.pfn << PAGE_SHIFT, page_state::process_memory) >> PAGE_SHIFT;
            if (pfn == 0) {
                print("cow_folio failed\n");
                return false;
            }

            send_ipi = pfn != pte.pfn;
        } else {
            pfn = allocate_folio(0, page_state::process_memory) >> PAGE_SHIFT;
            if (pfn == 0)
                return false;

            memcpy(phys_address(pfn << PAGE_SHIFT),
                   phys_address(pte.pfn << PAGE_SHIFT), PAGE_SIZE);

            send_ipi = true;
        }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        pte.pfn = pfn;
#pragma GCC diagnostic pop

        pte.owned = 1;
    } else {
        auto ret = visit([&](auto* ptr) {
            auto& cache = *ptr;
            uint64_t read_pages;

            auto status = cache.read_into_cache_and_pin(sect_page_num, 1,
                                                        read_pages);
            if (!success(status)) {
                print("read_into_cache_and_pin returned {}\n", status);
                return false;
            }

            rangelock_req rlr(cache.base.rl, sect_page_num, 1, rangelock_mode::exclusive);

            page_cache_entry* p;

            {
                lock_guard lg(cache.base.cache_lock);

                p = cache.find_page_cache_entry(sect_page_num);
            }

            if (!p->dirty) {
                assert(p->pfn != 0);

                auto new_pfn = cow_folio(p->pfn << PAGE_SHIFT, page_state::cache) >> PAGE_SHIFT;
                if (new_pfn == 0) {
                    print("out of memory\n");
                    cache.unpin(sect_page_num, 1);
                    return false;
                }

                if (new_pfn != p->pfn) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
                    p->pfn = new_pfn;
#pragma GCC diagnostic pop

                    send_ipi = true;
                }

                p->dirty = 1;
                cache.mark_dirty();
            }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
            pte.pfn = p->pfn;
#pragma GCC diagnostic pop

            cache.unpin(sect_page_num, 1); // FIXME?

            return true;
        }, m.sect.get_cache());

        if (!ret)
            return false;
    }

#ifdef __aarch64__
    pte.AP = ARM64_PTE_AP_EL0RW;
#elifdef __x86_64__
    pte.w = 1;
#endif

    return true;
};
