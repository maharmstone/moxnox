// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <mutex>
#include <shared_mutex>
#include <algorithm>
#include <stddef.h>
#include <stdint.h>

module framebuffer;

import nt;
import print;
import mem;
import config;
import handle;
import section;
import page_cache;
import moxnox_ioctls;
import crt;
import thread;
import spinlock;
import disable_interrupts;
import bsod;
import rwsem;
import rbtree;
import console;

using namespace std;

static uint32_t* cursor_buffer;
static uint16_t cursor_width, cursor_height, cursor_hotspot_x, cursor_hotspot_y;
static uint16_t cursor_x, cursor_y;
static rwsem active_fb_lock;
static custom_section_cache* active_fb;

template<bool BOOT>
class framebuffer_cache : public custom_section_cache {
public:
    framebuffer_cache(span<uint32_t> buffer) : buffer(buffer) { }
    ~framebuffer_cache();

    NTSTATUS read_into_cache(uint64_t page_num, uint64_t num_pages,
                             uint64_t& read_pages) override;
    NTSTATUS flush_cache(uint64_t page_num, uint64_t num_pages) override;
    NTSTATUS query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                        span<uint8_t> buf, size_t* return_length) override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written,
                    ACCESS_MASK access_mask) override;

private:
    nt_status ioctl_set_cursor(span<const uint8_t> inbuf);
    nt_status ioctl_move_cursor(span<const uint8_t> inbuf);
    nt_status ioctl_create_framebuffer(span<uint8_t> outbuf, uintptr_t& written);
    nt_status ioctl_set_active();
    void flush_scanlines(int16_t top, int16_t bottom);
    span<uint32_t> buffer;
    void* gpu_ctx = nullptr;
};

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::read_into_cache(uint64_t page_num, uint64_t num_pages,
                                                   uint64_t& read_pages) {
    auto status = cache.pin(page_num, num_pages, nullptr);
    if (!success(status)) {
        print("page_cache::pin returned {}\n", status);
        return status;
    }

    page_cache_entry* p;

    for (uint64_t i = 0; i < num_pages; i++) {
        {
            lock_guard lg(cache.base.cache_lock);

            if (i == 0)
                p = cache.find_page_cache_entry(page_num);
            else
                p = cache.next_page_cache_entry(*p);
        }

        if (p->pfn)
            continue;

        auto va = (uintptr_t)((uint8_t*)buffer.data() + ((page_num + i) << PAGE_SHIFT));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
        p->pfn = va_to_pa_lookup(va) >> PAGE_SHIFT;
#pragma GCC diagnostic pop
    }

    cache.unpin(page_num, num_pages);

    read_pages = num_pages;
    return nt_status::SUCCESS;
}

void copy_scanline_mirrored_slow(uint32_t* dst, const uint32_t* src,
                                 uint64_t x, uint64_t length) {
    for (uint64_t i = x; i < x + length; i++) {
        dst[fb_pixels_per_scan_line - i - 1] = src[i];
    }
}

static void flush_scanline(span<const uint32_t> source, uint16_t y,
                           uint32_t* fb_line) {
    auto cursor_top = (int16_t)((int16_t)cursor_y - (int16_t)cursor_hotspot_y);
    auto cursor_bottom = (int16_t)(cursor_top + cursor_height);
    auto cursor_left = (int16_t)((int16_t)cursor_x - (int16_t)cursor_hotspot_x);
    auto draw_x_start = (uint16_t)max((int16_t)0, cursor_left);
    auto draw_x_end = (uint16_t)min((int16_t)fb_width, (int16_t)(cursor_left + cursor_width));

    auto shadow_line = &source[y * shadow_fb_pixels_per_scan_line];

    if (y >= cursor_top && y < cursor_bottom && !in_bsod) {
        auto cursor_line = &cursor_buffer[((int16_t)y - cursor_top) * cursor_width];

        size_t scanline_x_granularity = copy_scanline_granularity / sizeof(uint32_t);

        size_t fast_pixels_start = (draw_x_start / scanline_x_granularity) * scanline_x_granularity;

        if (fast_pixels_start > 0)
            copy_scanline(fb_line, shadow_line, fast_pixels_start * sizeof(uint32_t));

        memcpy(fb_line + fast_pixels_start, shadow_line + fast_pixels_start,
                (draw_x_start - fast_pixels_start) * sizeof(uint32_t));

        for (uint16_t x = draw_x_start; x < draw_x_end; x++) {
            auto& fb_pixel = fb_line[x];
            auto& shadow_pixel = shadow_line[x];
            auto& cursor_pixel = cursor_line[(int16_t)x - cursor_left];
            auto alpha = (uint8_t)(cursor_pixel >> 24);

            if (alpha == 0)
                fb_pixel = shadow_pixel;
            else if (alpha == 255)
                fb_pixel = cursor_pixel;
            else {
                uint8_t cursor_r = (uint8_t)(cursor_pixel >> 16);
                uint8_t cursor_g = (uint8_t)(cursor_pixel >> 8);
                uint8_t cursor_b = (uint8_t)cursor_pixel;
                uint8_t shadow_r = (uint8_t)(shadow_pixel >> 16);
                uint8_t shadow_g = (uint8_t)(shadow_pixel >> 8);
                uint8_t shadow_b = (uint8_t)shadow_pixel;

                auto fb_r = (uint8_t)(cursor_r + (((255 - alpha) * shadow_r) / 255));
                auto fb_g = (uint8_t)(cursor_g + (((255 - alpha) * shadow_g) / 255));
                auto fb_b = (uint8_t)(cursor_b + (((255 - alpha) * shadow_b) / 255));

                fb_pixel = (uint32_t)(fb_r << 16) | (uint32_t)(fb_g << 8) | (uint32_t)fb_b;
            }
        }

        size_t fast_pixels_end = ((draw_x_end + scanline_x_granularity - 1) / scanline_x_granularity) * scanline_x_granularity;

        if (fast_pixels_end < shadow_fb_pixels_per_scan_line) {
            copy_scanline(fb_line + fast_pixels_end,
                            shadow_line + fast_pixels_end,
                            (shadow_fb_pixels_per_scan_line - fast_pixels_end) * sizeof(uint32_t));
        }

        memcpy(fb_line + draw_x_end, shadow_line + draw_x_end,
               (fast_pixels_end - draw_x_end) * sizeof(uint32_t));
    } else {
        copy_scanline(fb_line, shadow_line,
                      shadow_fb_pixels_per_scan_line * sizeof(uint32_t));
    }
}

static void flush_scanline_mirrored(span<const uint32_t> source, uint16_t y,
                                    uint32_t* fb_line) {
    auto cursor_top = (int16_t)((int16_t)cursor_y - (int16_t)cursor_hotspot_y);
    auto cursor_bottom = (int16_t)(cursor_top + cursor_height);
    auto cursor_left = (int16_t)((int16_t)cursor_x - (int16_t)cursor_hotspot_x);
    auto draw_x_start = (uint16_t)max((int16_t)0, cursor_left);
    auto draw_x_end = (uint16_t)min((int16_t)fb_width, (int16_t)(cursor_left + cursor_width));

    auto shadow_line = &source[y * shadow_fb_pixels_per_scan_line];

    if (y >= cursor_top && y < cursor_bottom && !in_bsod) {
        auto cursor_line = &cursor_buffer[((int16_t)y - cursor_top) * cursor_width];

        size_t scanline_x_granularity = copy_scanline_granularity / sizeof(uint32_t);

        size_t fast_pixels_start = (draw_x_start / scanline_x_granularity) * scanline_x_granularity;

        if (fast_pixels_start > 0)
            copy_scanline_mirrored(fb_line, shadow_line, 0, (unsigned int)fast_pixels_start);

        for (unsigned int x = (unsigned int)fast_pixels_start; x < draw_x_start; x++) {
            fb_line[fb_pixels_per_scan_line - x - 1] = shadow_line[x];
        }

        for (uint16_t x = draw_x_start; x < draw_x_end; x++) {
            auto& fb_pixel = fb_line[fb_pixels_per_scan_line - x - 1];
            auto& shadow_pixel = shadow_line[x];
            auto& cursor_pixel = cursor_line[(int16_t)x - cursor_left];
            auto alpha = (uint8_t)(cursor_pixel >> 24);

            if (alpha == 0)
                fb_pixel = shadow_pixel;
            else if (alpha == 255)
                fb_pixel = cursor_pixel;
            else {
                uint8_t cursor_r = (uint8_t)(cursor_pixel >> 16);
                uint8_t cursor_g = (uint8_t)(cursor_pixel >> 8);
                uint8_t cursor_b = (uint8_t)cursor_pixel;
                uint8_t shadow_r = (uint8_t)(shadow_pixel >> 16);
                uint8_t shadow_g = (uint8_t)(shadow_pixel >> 8);
                uint8_t shadow_b = (uint8_t)shadow_pixel;

                auto fb_r = (uint8_t)(cursor_r + (((255 - alpha) * shadow_r) / 255));
                auto fb_g = (uint8_t)(cursor_g + (((255 - alpha) * shadow_g) / 255));
                auto fb_b = (uint8_t)(cursor_b + (((255 - alpha) * shadow_b) / 255));

                fb_pixel = (uint32_t)(fb_r << 16) | (uint32_t)(fb_g << 8) | (uint32_t)fb_b;
            }
        }

        size_t fast_pixels_end = ((draw_x_end + scanline_x_granularity - 1) / scanline_x_granularity) * scanline_x_granularity;

        if (fast_pixels_end < shadow_fb_pixels_per_scan_line) {
            copy_scanline_mirrored(fb_line, shadow_line, (unsigned int)fast_pixels_end,
                                   (unsigned int)(shadow_fb_pixels_per_scan_line - fast_pixels_end));
        }

        for (unsigned int x = draw_x_end; x < fast_pixels_end; x++) {
            fb_line[fb_pixels_per_scan_line - x - 1] = shadow_line[x];
        }
    } else {
        copy_scanline_mirrored(fb_line, shadow_line, 0,
                               shadow_fb_pixels_per_scan_line);
    }
}

void flush_scanlines_rotated90_slow(uint32_t* buffer, uint32_t min_y,
                                    uint32_t max_y) {
    static const size_t BLOCK = 16;

    min_y &= (uint32_t)~(BLOCK - 1);
    max_y = min(shadow_fb_height - 1, (uint32_t)((max_y + BLOCK - 1) & ~(BLOCK - 1)));

    for (uint32_t by = min_y; by <= max_y; by += BLOCK) {
        for (uint32_t bx = 0; bx < shadow_fb_width; bx += BLOCK) {
            uint32_t bh = (by + BLOCK <= shadow_fb_height) ? BLOCK : (shadow_fb_height - by);
            uint32_t bw = (bx + BLOCK <= shadow_fb_width) ? BLOCK : (shadow_fb_width - bx);

            for (uint32_t y = 0; y < bh; y++) {
                auto shadow_line = &buffer[(by + y) * shadow_fb_pixels_per_scan_line];

                for (uint32_t x = 0; x < bw; x++) {
                    uint32_t dest_y = bx + x;
                    uint32_t dest_x = shadow_fb_height - 1 - (by + y);

                    auto fb_line = &framebuffer[dest_y  * fb_pixels_per_scan_line];

                    fb_line[dest_x] = shadow_line[bx + x];
                }
            }
        }
    }
}

void flush_scanlines_rotated270_slow(uint32_t* buffer, uint32_t min_y,
                                     uint32_t max_y) {
    static const size_t BLOCK = 16;

    min_y &= (uint32_t)~(BLOCK - 1);
    max_y = min(shadow_fb_height - 1, (uint32_t)((max_y + BLOCK - 1) & ~(BLOCK - 1)));

    for (uint32_t by = min_y; by <= max_y; by += BLOCK) {
        for (uint32_t bx = 0; bx < shadow_fb_width; bx += BLOCK) {
            uint32_t bh = (by + BLOCK <= shadow_fb_height) ? BLOCK : (shadow_fb_height - by);
            uint32_t bw = (bx + BLOCK <= shadow_fb_width) ? BLOCK : (shadow_fb_width - bx);

            for (uint32_t y = 0; y < bh; y++) {
                auto shadow_line = &buffer[(by + y) * shadow_fb_pixels_per_scan_line];

                for (uint32_t x = 0; x < bw; x++) {
                    uint32_t dest_y = shadow_fb_width - 1 - (bx + x);
                    uint32_t dest_x = by + y;

                    auto fb_line = &framebuffer[dest_y  * fb_pixels_per_scan_line];

                    fb_line[dest_x] = shadow_line[bx + x];
                }
            }
        }
    }
}

template<bool BOOT>
NTSTATUS framebuffer_cache<BOOT>::flush_cache(uint64_t page_num, uint64_t num_pages) {
    auto start = page_num << PAGE_SHIFT;

    if constexpr (BOOT) {
        if (start >= shadow_framebuffer.size_bytes())
            return nt_status::SUCCESS;
    } else {
        if (start >= framebuffer.size_bytes())
            return nt_status::SUCCESS;
    }

    auto do_flush = [&]() {
        if constexpr (BOOT) {
            lock_guard lock(cursor_lock);

            auto min_y = (uint16_t)((page_num << PAGE_SHIFT) / (shadow_fb_pixels_per_scan_line * sizeof(uint32_t)));
            auto max_y = min((uint16_t)(((page_num + num_pages + 1) << PAGE_SHIFT) / (shadow_fb_pixels_per_scan_line * sizeof(uint32_t))), (uint16_t)(shadow_fb_height - 1));

            if (fb_transform == fb_transform::rotated90)
                flush_scanlines_rotated90(buffer.data(), min_y, max_y);
            else if (fb_transform == fb_transform::rotated270)
                flush_scanlines_rotated270(buffer.data(), min_y, max_y);
            else {
                for (uint16_t y = min_y; y <= max_y; y++) {
                    uint32_t* fb_line;

                    if (fb_transform == fb_transform::flipped || fb_transform == fb_transform::rotated180)
                        fb_line = &framebuffer[(fb_height - 1 - y) * fb_pixels_per_scan_line];
                    else
                        fb_line = &framebuffer[y * fb_pixels_per_scan_line];

                    if (fb_transform == fb_transform::mirrored || fb_transform == fb_transform::rotated180)
                        flush_scanline_mirrored(buffer, y, fb_line);
                    else
                        flush_scanline(buffer, y, fb_line);
                }
            }
        } else {
            auto y1 = (unsigned int)(((page_num << PAGE_SHIFT) / sizeof(uint32_t)) / fb_width);
            auto y2 = (unsigned int)((((page_num + num_pages) << PAGE_SHIFT) / sizeof(uint32_t)) / fb_width);

            flush_framebuffer(gpu_ctx, 0, y1, fb_width, min(fb_height - y1, y2 - y1 + 1));
        }
    };

    if (in_bsod)
        do_flush();
    else {
        shared_lock lock(active_fb_lock);

        if (active_fb != static_cast<custom_section_cache*>(this))
            return nt_status::SUCCESS;

        do_flush();
    }

    return nt_status::SUCCESS;
}

template<bool BOOT>
NTSTATUS framebuffer_cache<BOOT>::query_info(enum SECTION_INFORMATION_CLASS section_information_class,
                                             span<uint8_t> buf, size_t* return_length) {
    switch (section_information_class) {
        case SECTION_INFORMATION_CLASS::MoxnoxFramebufferInformation: {
            if (buf.size() < sizeof(MN_SECTION_FRAMEBUFFER_INFORMATION))
                return nt_status::BUFFER_TOO_SMALL;

            auto& info = *(MN_SECTION_FRAMEBUFFER_INFORMATION*)buf.data();

            if constexpr (BOOT) {
                info.width = shadow_fb_width;
                info.height = shadow_fb_height;
            } else {
                info.width = fb_width;
                info.height = fb_height;
            }

            if constexpr (BOOT)
                info.pixels_per_scan_line = shadow_fb_pixels_per_scan_line;
            else
                info.pixels_per_scan_line = fb_pixels_per_scan_line;

            info.dpi = fb_dpi;

            *return_length = sizeof(MN_SECTION_FRAMEBUFFER_INFORMATION);

            return nt_status::SUCCESS;
        }

        default:
            return nt_status::INVALID_INFO_CLASS;
    }
}

void create_boot_framebuffer_section() {
    nt_status status;

    auto s = new section;
    if (!s) {
        print("new failed\n");
        return;
    }

    s->type = section_type::anonymous;
    s->size = (uint32_t)shadow_framebuffer.size_bytes();
    s->num_pages = bytes_to_pages(s->size);
    s->page_protection = PAGE_READWRITE;

    s->custom_cache = new framebuffer_cache<true>(shadow_framebuffer);
    if (!s->custom_cache) {
        print("new failed\n");
        s->release();
        return;
    }

    active_fb = s->custom_cache;

    status = name_object(*s, "framebuffer", true);
    if (!success(status)) {
        print("name_object returned {}\n", status);
        s->release();
        return;
    }
}

void create_other_framebuffer_section() {
    nt_status status;

    auto s = new section;
    if (!s) {
        print("new failed\n");
        return;
    }

    s->type = section_type::anonymous;
    s->size = (uint32_t)framebuffer.size_bytes();
    s->num_pages = bytes_to_pages(s->size);
    s->page_protection = PAGE_READWRITE;

    s->custom_cache = new framebuffer_cache<false>(framebuffer);
    if (!s->custom_cache) {
        print("new failed\n");
        s->release();
        return;
    }

    active_fb = s->custom_cache;

    status = name_object(*s, "framebuffer", true);
    if (!success(status)) {
        print("name_object returned {}\n", status);
        s->release();
        return;
    }
}

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::ioctl_create_framebuffer(span<uint8_t> outbuf,
                                                            uintptr_t& written) {
    auto& t = *get_current_thread();
    nt_status status;
    handle_t h;

    if (outbuf.size() < sizeof(HANDLE))
        return nt_status::BUFFER_TOO_SMALL;

    auto s = new section;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    s->type = section_type::anonymous;

    if constexpr (BOOT)
        s->size = (uint32_t)shadow_framebuffer.size_bytes();
    else
        s->size = (uint32_t)framebuffer.size_bytes();

    s->num_pages = bytes_to_pages(s->size);
    s->page_protection = PAGE_READWRITE;

    auto buf = (uint32_t*)vmalloc(s->num_pages << PAGE_SHIFT);
    if (!buf) {
        print("vmalloc failed\n");
        s->release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memset(buf, 0, s->num_pages << PAGE_SHIFT);

    void* new_ctx;

    if (new_framebuffer_created) {
        status = new_framebuffer_created(span(buf, s->size / sizeof(uint32_t)),
                                         fb_width, fb_height, new_ctx);
        if (!success(status)) {
            vfree(buf, s->size);
            s->release();
            return status;
        }
    }

    auto c = new framebuffer_cache<BOOT>(span(buf, s->size / sizeof(uint32_t)));

    s->custom_cache = c;
    if (!s->custom_cache) {
        print("new failed\n");

        if (framebuffer_deleted)
            framebuffer_deleted(new_ctx);

        vfree(buf, s->size);
        s->release();
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    c->gpu_ctx = new_ctx;

    status = t.proc->add_handle(*s, 0, true, h);
    if (!success(status)) {
        s->release();
        return status;
    }

    s->release();

    *(HANDLE*)outbuf.data() = h;
    written = sizeof(HANDLE);

    return nt_status::SUCCESS;
}

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::ioctl_set_active() {
    auto* c = static_cast<custom_section_cache*>(this);

    {
        lock_guard lock(active_fb_lock);

        if (active_fb == c)
            return nt_status::SUCCESS;

        active_fb = c;

        if (framebuffer_set_active) {
            auto status = framebuffer_set_active(gpu_ctx);
            if (!success(status))
                return status;
        }
    }

    flush_cache(0, bytes_to_pages(buffer.size_bytes()));

    return nt_status::SUCCESS;
}

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::ioctl(uint32_t code, span<const uint8_t> inbuf,
                                         span<uint8_t> outbuf, uintptr_t& written,
                                         ACCESS_MASK /*access_mask*/) {
    switch (code) {
        case IOCTL_MOXNOX_FB_SET_CURSOR:
            return ioctl_set_cursor(inbuf);

        case IOCTL_MOXNOX_FB_MOVE_CURSOR:
            return ioctl_move_cursor(inbuf);

        case IOCTL_MOXNOX_FB_CREATE_FRAMEBUFFER:
            return ioctl_create_framebuffer(outbuf, written);

        case IOCTL_MOXNOX_FB_SET_ACTIVE:
            return ioctl_set_active();

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

template<bool BOOT>
void framebuffer_cache<BOOT>::flush_scanlines(int16_t top, int16_t bottom) {
    uint32_t pixels_per_scan_line;

    if constexpr (BOOT)
        pixels_per_scan_line = shadow_fb_pixels_per_scan_line;
    else
        pixels_per_scan_line = fb_pixels_per_scan_line;

    top = max((int16_t)0, top);
    bottom = min((int16_t)shadow_fb_height, bottom);

    auto first_cursor_page = ((uint16_t)top * pixels_per_scan_line * sizeof(uint32_t)) >> PAGE_SHIFT;
    auto last_cursor_page = (((uint16_t)bottom * pixels_per_scan_line * sizeof(uint32_t)) + PAGE_SIZE - 1) >> PAGE_SHIFT;

    flush_cache(first_cursor_page, last_cursor_page - first_cursor_page);
}

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::ioctl_set_cursor(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(MOXNOX_IOCTL_FB_SET_CURSOR))
        return nt_status::INVALID_PARAMETER;

    auto& sc = *(MOXNOX_IOCTL_FB_SET_CURSOR*)inbuf.data();
    auto& t = *get_current_thread();
    uint32_t* buf = nullptr;

    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    if (sc.width != 0 && sc.height != 0 && sc.data) {
        if (sc.hotspot_x >= sc.width || sc.hotspot_y >= sc.height)
            return nt_status::INVALID_PARAMETER;

        // FIXME - if smaller than 64x64, should be copying and adding padding
        if (sc.width != 64 || sc.height != 64)
            return nt_status::INVALID_PARAMETER;

        size_t bytes = sc.width * sc.height * sizeof(uint32_t);

        buf = (uint32_t*)vmalloc(bytes);
        if (!buf) {
            print("vmalloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        if (!t.proc->copy_from(buf, sc.data, bytes)) {
            print("process::copy_from failed\n");
            vfree(buf, bytes);
            return nt_status::NO_MEMORY;
        }

        if constexpr (BOOT) {
            // premultiply

            for (unsigned int y = 0; y < sc.height; y++) {
                auto* line = &buf[y * sc.width];

                for (unsigned int x = 0; x < sc.width; x++) {
                    auto& pixel = line[x];

                    uint8_t alpha = (uint8_t)(pixel >> 24);

                    if (alpha == 0)
                        pixel = 0;
                    else if (alpha != 255) {
                        uint8_t r = (uint8_t)(pixel >> 16);
                        uint8_t g = (uint8_t)(pixel >> 8);
                        uint8_t b = (uint8_t)pixel;

                        r = (uint8_t)((r * alpha) / 255);
                        g = (uint8_t)((g * alpha) / 255);
                        b = (uint8_t)((b * alpha) / 255);

                        pixel = ((uint32_t)alpha << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | b;
                    }
                }
            }
        }
    } else {
        sc.width = 0;
        sc.height = 0;
        sc.data = nullptr;
    }

    uint32_t* old_cursor_buffer;
    uint32_t old_cursor_width, old_cursor_height;
    int16_t cursor_top, cursor_bottom, old_cursor_top, old_cursor_bottom;
    uint16_t mouse_x, mouse_y;

    {
        lock_guard lock(cursor_lock);

        old_cursor_buffer = cursor_buffer;
        old_cursor_width = cursor_width;
        old_cursor_height = cursor_height;

        old_cursor_top = (int16_t)((int16_t)cursor_y - (int16_t)cursor_hotspot_y);
        old_cursor_bottom = (int16_t)(old_cursor_top + (int16_t)old_cursor_height);

        cursor_buffer = buf;
        cursor_width = sc.width;
        cursor_height = sc.height;
        cursor_hotspot_x = sc.hotspot_x;
        cursor_hotspot_y = sc.hotspot_y;

        mouse_x = cursor_x;
        mouse_y = cursor_y;

        cursor_top = (int16_t)((int16_t)cursor_y - (int16_t)cursor_hotspot_y);
        cursor_bottom = (int16_t)(cursor_top + cursor_height);
    }

    if constexpr (BOOT) {
        flush_scanlines(min(cursor_top, old_cursor_top),
                        max(cursor_bottom, old_cursor_bottom));
    } else {
        if (set_cursor) {
            set_cursor(mouse_x, mouse_y, span(buf, sc.width * sc.height),
                       sc.hotspot_x, sc.hotspot_y);
        }
    }

    if (old_cursor_buffer)
        vfree(old_cursor_buffer, old_cursor_width * old_cursor_height * sizeof(uint32_t));

    return nt_status::SUCCESS;
}

template<bool BOOT>
nt_status framebuffer_cache<BOOT>::ioctl_move_cursor(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(MOXNOX_IOCTL_FB_MOVE_CURSOR))
        return nt_status::INVALID_PARAMETER;

    auto& mc = *(MOXNOX_IOCTL_FB_MOVE_CURSOR*)inbuf.data();

    auto mc_x = min(mc.x, (uint16_t)(shadow_fb_width - 1));
    auto mc_y = min(mc.y, (uint16_t)(shadow_fb_height - 1));

    int16_t cursor_top, cursor_bottom;
    int16_t last_cursor_top, last_cursor_bottom;

    {
        lock_guard lock(cursor_lock);

        last_cursor_top = (int16_t)((int16_t)cursor_y - (int16_t)cursor_hotspot_y);
        last_cursor_bottom = (int16_t)(last_cursor_top + cursor_height);

        cursor_x = mc_x;
        cursor_y = mc_y;

        cursor_top = (int16_t)((int16_t)cursor_y - (int16_t)cursor_hotspot_y);
        cursor_bottom = (int16_t)(cursor_top + cursor_height);
    }

    if constexpr (BOOT) {
        auto higher_cursor_top = cursor_top < last_cursor_top ? cursor_top : last_cursor_top;
        auto higher_cursor_bottom = cursor_top < last_cursor_top ? cursor_bottom : last_cursor_bottom;
        auto lower_cursor_top = cursor_top < last_cursor_top ? last_cursor_top : cursor_top;
        auto lower_cursor_bottom = cursor_top < last_cursor_top ? last_cursor_bottom : cursor_bottom;

        if (lower_cursor_top <= higher_cursor_bottom)
            flush_scanlines(higher_cursor_top, lower_cursor_bottom);
        else {
            flush_scanlines(higher_cursor_top, higher_cursor_bottom);
            flush_scanlines(lower_cursor_top, lower_cursor_bottom);
        }
    } else {
        if (move_cursor)
            move_cursor(mc_x, mc_y);
    }

    return nt_status::SUCCESS;
}

static void zero_cache_entry(rbtree_node<uint64_t>* tree) {
    auto& r = rbtree_containing_record<page_cache_entry>(tree);

    if (tree->left)
        zero_cache_entry(tree->left);

    if (tree->right)
        zero_cache_entry(tree->right);

    r.pfn = 0;
}

template<bool BOOT>
framebuffer_cache<BOOT>::~framebuffer_cache() {
    auto* c = static_cast<custom_section_cache*>(this);
    auto& master_fb = static_cast<section&>(framebuffer_section.get());
    bool changed_active = false;

    {
        lock_guard lock(active_fb_lock);

        if (active_fb == c) {
            active_fb = master_fb.custom_cache;
            changed_active = true;

            if (framebuffer_set_active)
                framebuffer_set_active(nullptr);
        }
    }

    if (changed_active)
        master_fb.custom_cache->flush_cache(0, bytes_to_pages(buffer.size_bytes()));

    if (framebuffer_deleted)
        framebuffer_deleted(gpu_ctx);

    if (buffer.data())
        vfree(buffer.data(), bytes_to_pages(buffer.size_bytes()) << PAGE_SHIFT);

    // make sure page cache doesn't double-free our pages

    {
        lock_guard lock(cache.base.cache_lock);

        if (cache.base.root)
            zero_cache_entry(cache.base.root);
    }
}
