// SPDX-License-Identifier: GPL-2.0-only
module;

#include <optional>
#include <functional>
#include <span>
#include <stddef.h>
#include <stdint.h>
#include <variant>
#include <string_view>

export module section;

import nt;
import object_ref;
import page_cache;
import objects;
import spinlock;
import pe;
import vfs;

using namespace std;

export enum class section_type {
    anonymous,
    file,
    image
};

export class anon_section_cache {
public:
    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages);

    page_cache<anon_section_cache> cache{*this};
};

struct section_info {
    uint64_t offset;
    uint64_t size;
};

struct dwarf_info {
    section_info debug_info;
    section_info debug_abbrev;
    section_info debug_rnglists;
    section_info debug_str;
    section_info debug_addr;
    section_info debug_str_offsets;
    section_info debug_line;
    section_info debug_line_str;
};

static_assert(cacheable<anon_section_cache>);

export class custom_section_cache {
public:
    virtual NTSTATUS read_into_cache(uint64_t page_num, uint64_t num_pages,
                                     uint64_t& read_pages) = 0;
    virtual NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                                span<uint8_t> buf, size_t* return_length) = 0;
    virtual NTSTATUS flush_cache(uint64_t page_num, uint64_t num_pages) = 0;
    virtual nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                            span<uint8_t> outbuf, uintptr_t& written,
                            ACCESS_MASK access_mask) = 0;
    virtual ~custom_section_cache() { }

    page_cache<custom_section_cache> cache{*this};
};

static_assert(cacheable<custom_section_cache>);
static_assert(cacheable_flushable<custom_section_cache>);
static_assert(has_ioctls<custom_section_cache>);

class image_section_cache;

using cache_variant = variant<page_cache<image_section_cache>*, page_cache<anon_section_cache>*,
                                   page_cache<custom_section_cache>*, page_cache<inode>*>;

export class section : public object {
public:
    section() : object(object_type::section) { }
    ~section();

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status load_image();
    uint64_t find_export(string_view name);
    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class, span<uint8_t> buf,
                        size_t* return_length);
    cache_variant get_cache();
    nt_status flush(uint64_t page_num, uint64_t num_pages);
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written,
                    ACCESS_MASK access_mask);

    optional<reference_wrapper<file>> file;
    enum section_type type;
    uint32_t size;
    uint32_t num_pages;
    uintptr_t preferred_base, entry_point;
    uint16_t characteristics, machine;
    uint64_t stack_size;
    uint64_t stack_commit;
    spinlock alloc_lock;
    dwarf_info dwinfo;
    unsigned int num_pe_sections = 0;
    IMAGE_SECTION_HEADER* pe_sections = nullptr;
    uint32_t size_of_headers;
    image_section_cache* image_cache = nullptr;
    anon_section_cache* anon_cache = nullptr;
    custom_section_cache* custom_cache = nullptr;
    uint32_t page_protection;
};

static_assert(has_ioctls<section>);

class image_section_cache {
public:
    image_section_cache(section& sect) : sect(sect) { }

    nt_status read_into_cache(uint64_t page_num, uint64_t num_pages,
                              uint64_t& read_pages);

    page_cache<image_section_cache> cache{*this};

private:
    section& sect;
};

export nt_status create_section(const optional<reference_wrapper<file>>& file, bool image,
                                size_t size, uint32_t page_protection, object_ref& ref);
export uint64_t get_image_size(void* image);
