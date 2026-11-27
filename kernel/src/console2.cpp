// SPDX-License-Identifier: GPL-2.0-only
module;

#include <string_view>
#include <string.h>
#include <mutex>
#include <span>
#include <algorithm>

module console;

#ifdef __x86_64__
import inout;
import text_console;
#endif

import nt;
import print;
import mem;
import config;
import rbtree;
import visit;
import vt100;
import cp437;
import cmdline;
import thread;
import bsod;
import chardev;
import pipe;
import scheduler;
import list;
import disable_interrupts;
import cpu_block;
import spinlock;
import framebuffer;
import handle;
import objects;
import freetype;
import section;
import smbios_parser;
import vec;

using namespace std;

#ifdef __x86_64__
enum class console_colour : uint8_t {
    black = 0,
    blue = 1,
    green = 2,
    cyan = 3,
    red = 4,
    magenta = 5,
    brown = 6,
    light_grey = 7,
    dark_grey = 8,
    light_blue = 9,
    light_green = 10,
    light_cyan = 11,
    light_red = 12,
    light_magenta = 13,
    yellow = 14,
    white = 15
};
#endif

static object_ref kmsg;
static const unsigned int console_buffer_pages = 4;

static FT_Library ft = nullptr;
static unsigned int font_width, font_height, font_descender;
static bool fb_console_started = false;
static bool show_cursor = true;

#ifdef __x86_64__
static bool text_console_started = false;;
#endif

spinlock early_print_lock;
char early_print_buf[65536];
size_t early_print_buf_size = 0;

static uint32_t console_output_mode = ENABLE_PROCESSED_OUTPUT;

// font.S
extern "C" void* console_font_data;
extern "C" uint64_t console_font_size;

#ifdef __x86_64__
// x86/framebuffer.S
extern "C" void copy_scanline_avx512(void* dst, const void* src, size_t n);
extern "C" void copy_scanline_avx2(void* dst, const void* src, size_t n);
extern "C" void copy_scanline_mirrored_avx512(uint32_t* dst, const uint32_t* src,
                                              uint64_t x, uint64_t length);
extern "C" void copy_scanline_mirrored_avx2(uint32_t* dst, const uint32_t* src,
                                            uint64_t x, uint64_t length);
#elifdef __aarch64__
// aarch64/framebuffer.S
extern "C" void copy_scanline_neon(void* dst, const void* src, size_t n);
extern "C" void copy_scanline_mirrored_neon(uint32_t* dst, const uint32_t* src,
                                            uint64_t x, uint64_t length);
#endif

struct glyph_info {
    rbtree_node<FT_UInt> tree;
    FT_Int bitmap_left;
    FT_Int bitmap_top;
    FT_Bitmap bitmap;
};

// FIXME - LRU for glyphs, and free these occasionally?
struct font_info {
    FT_Face face = nullptr;
    rbtree_node<FT_UInt>* glyphs = nullptr; // FIXME - destructor for this
};

static font_info face;

class console : public pipe {
public:
    console(unsigned int buffer_pages) : pipe(buffer_pages) { }

    nt_status flush(bool go_readonly) override;
};

struct kmsg_buf_page {
    list_entry le;
    uint64_t pa;
    uint16_t read_pos = 0;
    uint16_t write_pos = 0;
};

class kmsg_buf : public chardev {
public:
    kmsg_buf() {
        signalled = false;
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        return do_read<false>(v, read_bytes);
    }

    nt_status read_nonblock(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        return do_read<true>(v, read_bytes);
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;

    spinlock lock;
    list_entry pages = { &pages, &pages };

private:
    template<bool NONBLOCK>
    nt_status do_read(span<const span<uint8_t>> v, uint64_t& read_bytes);
};

struct console_info {
    uint32_t bg;
    uint32_t fg;
    bool bold;
    FT_UInt glyph;
    bool changed;
};

static console_info* console;

static const uint32_t vt100_colours[] = {
    0x000000,
    0xb21818,
    0x18b218,
    0xb26818,
    0x1818b2,
    0xb218b2,
    0x18b2b2,
    0xb2b2b2,
};

static const uint32_t vt100_bright_colours[] = {
    0x686868,
    0xff5454,
    0x54ff54,
    0xffff54,
    0x5454ff,
    0xff54ff,
    0x54ffff,
    0xffffff,
};

#ifdef __x86_64__
static const enum console_colour vt100_colours_text[] = {
    console_colour::black,
    console_colour::red,
    console_colour::green,
    console_colour::brown,
    console_colour::blue,
    console_colour::magenta,
    console_colour::cyan,
    console_colour::light_grey,
};

static const enum console_colour vt100_bright_colours_text[] = {
    console_colour::dark_grey,
    console_colour::light_red,
    console_colour::light_green,
    console_colour::yellow,
    console_colour::light_blue,
    console_colour::light_magenta,
    console_colour::light_cyan,
    console_colour::white,
};
#endif

static const unsigned int DEFAULT_BG_COLOUR_INDEX = 0;
static const unsigned int DEFAULT_FG_COLOUR_INDEX = 7;
static const uint32_t DEFAULT_BG_COLOUR = vt100_colours[DEFAULT_BG_COLOUR_INDEX];
static const uint32_t DEFAULT_FG_COLOUR = vt100_colours[DEFAULT_FG_COLOUR_INDEX];
static uint32_t bg_colour_index = 0, fg_colour_index = 7;
static bool bg_bright = false, fg_bright = false, bold = false;
static uint32_t bg_colour = DEFAULT_BG_COLOUR, fg_colour = DEFAULT_FG_COLOUR;
#ifdef __x86_64__
static const auto DEFAULT_BG_COLOUR_TEXT = console_colour::black;
static const auto DEFAULT_FG_COLOUR_TEXT = console_colour::light_grey;
static enum console_colour bg_colour_text = DEFAULT_BG_COLOUR_TEXT;
static enum console_colour fg_colour_text = DEFAULT_FG_COLOUR_TEXT;
#endif

struct console_fb {
    void console_overflow(unsigned int rows = 1);
    static void handle_sgr_part_num(unsigned int num);
    void print_chars(span<const char32_t> s);
    void cursor_changed();
    void scroll_down(unsigned int rows);
    static void enable_cursor(bool enable);
    void move_cursor() { }
    void erase_in_line(unsigned int param);
    void erase_in_display(unsigned int param);
    void delete_chars(unsigned int) { }
    void erase_chars(unsigned int) { }
    void insert_chars(unsigned int) { }
    void insert_lines(unsigned int) { }
    void delete_lines(unsigned int) { }
    void enter_alt_screen() { }
    void exit_alt_screen() { }
    void write_input_data(string_view) { }
    static uint32_t get_bg_colour() { return 0x000000; }

    unsigned int cursor_x, cursor_y;
    unsigned int console_rows, console_cols;
    unsigned int scroll_top = 0, scroll_bottom;
    bool decckm = false;
    bool auto_wrap = true;
    bool insert_mode = false;
};

static_assert(vt100_console<console_fb>);

static console_fb console_fb_inst;

#ifdef __x86_64__

struct console_text {
    void console_overflow(unsigned int rows = 1);
    static void handle_sgr_part_num(unsigned int num);
    void print_chars(span<const char32_t> s);
    void cursor_changed() { }
    void scroll_down(unsigned int rows);
    static void enable_cursor(bool enable);
    void move_cursor();
    void erase_in_line(unsigned int param) const;
    void erase_in_display(unsigned int param) const;
    void delete_chars(unsigned int) { }
    void erase_chars(unsigned int) { }
    void insert_chars(unsigned int) { }
    void insert_lines(unsigned int) { }
    void delete_lines(unsigned int) { }
    void enter_alt_screen() { }
    void exit_alt_screen() { }
    void write_input_data(string_view) { }
    static uint32_t get_bg_colour() { return 0x000000; }

    unsigned int cursor_x, cursor_y;
    unsigned int console_rows, console_cols;
    unsigned int scroll_top = 0, scroll_bottom;
    bool decckm = false;
    bool auto_wrap = true;
    bool insert_mode = false;
};

static_assert(vt100_console<console_text>);

static console_text console_text_inst;

static constexpr uint16_t text_console_val(uint8_t c, enum console_colour bg,
                                           enum console_colour fg) {
    return (uint16_t)(((uint16_t)bg << 12) | ((uint16_t)fg << 8) | c);
}

static void text_console_putc(uint8_t x, uint8_t y, char32_t cp,
                              enum console_colour bg, enum console_colour fg) {
    auto c = cp437(cp);

    text_console[(y * console_text_inst.console_cols) + x] = text_console_val((uint8_t)c, bg, fg);
}

void console_text::move_cursor() {
    auto pos = (uint16_t)(((uint8_t)cursor_y * console_cols) + (uint8_t)cursor_x);

    {
        lock_guard lock(text_cursor_lock);

        outb(0x3d4, 0xf);
        outb(0x3d5, (uint8_t)(pos & 0xff));
        outb(0x3d4, 0xe);
        outb(0x3d5, (uint8_t)((pos >> 8) & 0xff));
    }
}

void console_text::console_overflow(unsigned int rows) {
    memcpy(text_console.data(), &text_console[console_cols * rows],
            (console_rows - rows) * console_cols * sizeof(uint16_t));

    for (uint8_t y = 0; y < rows; y++) {
        for (uint8_t x = 0; x < console_cols; x++) {
            text_console_putc(x, (uint8_t)(console_rows - rows + y), ' ',
                                bg_colour_text, fg_colour_text);
        }
    }
}

void console_text::print_chars(span<const char32_t> s) {
    for (auto c : s) {
        if (c == '\n') {
            cursor_x = 0;
            cursor_y++;

            if (cursor_y == console_rows) {
                console_overflow();
                cursor_y = console_rows - 1;
            }
        } else if (c == '\b') {
            if (cursor_x == 0 && cursor_y == 0)
                continue;

            if (cursor_x > 0) { // FIXME
                cursor_x--;

                if (console_output_mode & ENABLE_PROCESSED_OUTPUT) {
                    text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, ' ',
                                    bg_colour_text, fg_colour_text);
                }
            }
        } else {
            if (c < 0x20)
                continue;

            text_console_putc((uint8_t)cursor_x, (uint8_t)cursor_y, c,
                              bg_colour_text, fg_colour_text);

            cursor_x++;

            if (cursor_x == console_cols) {
                cursor_x = 0;
                cursor_y++;

                if (cursor_y == console_rows) {
                    console_overflow();
                    cursor_y = console_rows - 1;
                }
            }
        }
    }
}

static void recalc_colours_text(bool fg) {
    bool bright = fg ? fg_bright : bg_bright;
    unsigned int index = fg ? fg_colour_index : bg_colour_index;
    enum console_colour* colour = fg ? &fg_colour_text : &bg_colour_text;

    if (bright || (fg && bold))
        *colour = vt100_bright_colours_text[index];
    else
        *colour = vt100_colours_text[index];
}

void console_text::handle_sgr_part_num(unsigned int num) {
    switch (num) {
        case 0: // reset
            fg_colour_index = DEFAULT_FG_COLOUR_INDEX;
            fg_bright = false;
            fg_colour_text = DEFAULT_FG_COLOUR_TEXT;

            bg_colour_index = DEFAULT_BG_COLOUR_INDEX;
            bg_bright = false;
            bg_colour_text = DEFAULT_BG_COLOUR_TEXT;

            bold = false;
            break;

        case 1:
            bold = true;
            recalc_colours_text(true);
            break;

        // FIXME - 5 - blink
        // FIXME - 7 - reverse video

        case 22:
            bold = false;
            recalc_colours_text(false);
            break;

        // FIXME - 25 - not blinking

        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            fg_colour_index = num - 30;
            fg_bright = false;
            recalc_colours_text(true);
            break;

        case 39:
            fg_colour_index = DEFAULT_FG_COLOUR_INDEX;
            fg_bright = false;
            fg_colour_text = DEFAULT_FG_COLOUR_TEXT;
            break;

        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
            bg_colour_index = num - 40;
            bg_bright = false;
            recalc_colours_text(false);
            break;

        case 49:
            bg_colour_index = DEFAULT_BG_COLOUR_INDEX;
            bg_colour_text = DEFAULT_BG_COLOUR_TEXT;
            bg_bright = false;
            break;

        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
            fg_colour_index = num - 90;
            fg_bright = true;
            recalc_colours_text(true);
            break;

        case 100:
        case 101:
        case 102:
        case 103:
        case 104:
        case 105:
        case 106:
        case 107:
            bg_colour_index = num - 100;
            bg_bright = true;
            recalc_colours_text(false);
            break;
    }
}

void console_text::enable_cursor(bool enabled) {
    if (enabled) {
        uint8_t cursor_start = 14, cursor_end = 15;

        outb(0x3d4, 0x0a);
        outb(0x3d5, (inb(0x3d5) & 0xc0) | cursor_start);

        outb(0x3d4, 0x0b);
        outb(0x3d5, (inb(0x3d5) & 0xe0) | cursor_end);
    } else {
        outb(0x3d4, 0xa);
        outb(0x3d5, 0x20);
    }
}

void console_text::scroll_down(unsigned int rows) {
    memmove(&text_console[console_cols * rows], text_console.data(),
            (console_rows - rows) * console_cols * sizeof(uint16_t));

    for (uint8_t y = 0; y < rows; y++) {
        for (uint8_t x = 0; x < console_cols; x++) {
            text_console_putc(x, y, ' ', bg_colour_text, fg_colour_text);
        }
    }
}

void console_text::erase_in_line(unsigned int param) const {
    switch (param) {
        case 0: { // clear from cursor to end of line
            if (cursor_x == console_cols)
                break;

            for (uint8_t x = (uint8_t)cursor_x; x < console_cols; x++) {
                text_console_putc(x, (uint8_t)cursor_y, ' ', bg_colour_text,
                                  fg_colour_text);
            }

            break;
        }

        case 1: { // clear from beginning of line to cursor
            for (uint8_t x = 0; x <= cursor_x; x++) {
                text_console_putc(x, (uint8_t)cursor_y, ' ', bg_colour_text,
                                  fg_colour_text);
            }

            break;
        }

        case 2: { // clear entire line
            for (uint8_t x = 0; x < console_cols; x++) {
                text_console_putc(x, (uint8_t)cursor_y, ' ', bg_colour_text,
                                  fg_colour_text);
            }

            break;
        }
    }
}

void console_text::erase_in_display(unsigned int param) const {
    switch (param) {
        case 0: // clear from cursor to end of screen
            if (cursor_y == console_rows)
                break;

            for (uint8_t x = (uint8_t)cursor_x; x < console_cols; x++) {
                text_console_putc(x, (uint8_t)cursor_y, ' ', bg_colour_text,
                                  fg_colour_text);
            }

            for (uint8_t y = (uint8_t)(cursor_y + 1); y < console_rows; y++) {
                for (uint8_t x = 0; x < console_cols; x++) {
                    text_console_putc(x, y, ' ', bg_colour_text,
                                      fg_colour_text);
                }
            }

            break;

        case 1: // clear from beginning of screen to cursor
            for (uint8_t y = 0; y < cursor_y; y++) {
                for (uint8_t x = 0; x < console_cols; x++) {
                    text_console_putc(x, y, ' ', bg_colour_text,
                                      fg_colour_text);
                }
            }

            for (uint8_t x = 0; x < cursor_x; x++) {
                text_console_putc(x, (uint8_t)cursor_y, ' ', bg_colour_text,
                                  fg_colour_text);
            }

            break;

        case 2: // clear entire screen
        case 3:
            for (uint8_t y = 0; y < console_rows; y++) {
                for (uint8_t x = 0; x < console_cols; x++) {
                    text_console_putc(x, y, ' ', bg_colour_text,
                                      fg_colour_text);
                }
            }

            break;
    }
}

#endif

void console_fb::console_overflow(unsigned int rows) {
    memcpy(console, &console[console_cols * rows], (console_rows - rows) * console_cols * sizeof(console_info));

    auto c = &console[(console_rows - rows) * console_cols];

    auto glyph = FT_Get_Char_Index(face.face, ' ');

    for (unsigned int i = 0; i < console_cols * console_rows; i++) {
        console[i].changed = true;
    }

    for (unsigned int i = 0; i < console_cols * rows; i++) {
        c->bg = bg_colour;
        c->fg = fg_colour;
        c->glyph = glyph;
        c->bold = bold;
        c++;
    }
}

void console_fb::print_chars(span<const char32_t> s) {
    if (in_bsod && cursor_y == console_rows)
        return;

    for (auto c : s) {
        if (c == '\n') {
            if (console) {
                auto glyph = FT_Get_Char_Index(face.face, ' ');

                for (unsigned int i = cursor_x; i < console_cols; i++) {
                    if (console[(cursor_y * console_cols) + i].glyph != glyph)
                        continue;

                    console[(cursor_y * console_cols) + i].glyph = glyph;
                    console[(cursor_y * console_cols) + i].changed = true;
                }
            }

            cursor_x = 0;
            cursor_y++;

            if (cursor_y == console_rows) {
                if (in_bsod)
                    break;

                console_overflow();
                cursor_y = console_rows - 1;
            }
        } else if (c == '\b') {
            if (cursor_x == 0 && cursor_y == 0)
                continue;

            if (cursor_x > 0)
                cursor_x--;
            else {
                cursor_x = console_cols - 1;
                cursor_y--;

                if (cursor_x != console_cols - 1)
                    cursor_x++;
            }

            if (console && console_output_mode & ENABLE_PROCESSED_OUTPUT) {
                auto glyph = FT_Get_Char_Index(face.face, ' ');

                if (console[(cursor_y * console_cols) + cursor_x].glyph != glyph) {
                    console[(cursor_y * console_cols) + cursor_x].glyph = glyph;
                    console[(cursor_y * console_cols) + cursor_x].changed = true;
                }
            }
        } else {
            if (c < 0x20)
                continue;

            if (console) {
                auto& p = console[(cursor_y * console_cols) + cursor_x];

                auto glyph = FT_Get_Char_Index(face.face, c);

                p.glyph = glyph;
                p.bg = bg_colour;
                p.fg = fg_colour;
                p.bold = bold;
                p.changed = true;
            }

            cursor_x++;

            if (cursor_x == console_cols) {
                cursor_x = 0;
                cursor_y++;

                if (cursor_y == console_rows) {
                    if (in_bsod)
                        break;

                    console_overflow();
                    cursor_y = console_rows - 1;
                }
            }
        }
    }
}

void console_fb::cursor_changed() {
    if (show_cursor) {
        auto& p = console[(cursor_y * console_cols) + cursor_x];

        p.changed = true;
    }
}

static void recalc_colours(bool fg) {
    bool bright = fg ? fg_bright : bg_bright;
    unsigned int index = fg ? fg_colour_index : bg_colour_index;
    uint32_t* colour = fg ? &fg_colour : &bg_colour;

    if (bright || (fg && bold))
        *colour = vt100_bright_colours[index];
    else
        *colour = vt100_colours[index];
}

void console_fb::handle_sgr_part_num(unsigned int num) {
    switch (num) {
        case 0: // reset
            fg_colour_index = DEFAULT_FG_COLOUR_INDEX;
            fg_bright = false;
            fg_colour = DEFAULT_FG_COLOUR;

            bg_colour_index = DEFAULT_BG_COLOUR_INDEX;
            bg_bright = false;
            bg_colour = DEFAULT_BG_COLOUR;

            bold = false;
            break;

        case 1:
            bold = true;
            recalc_colours(true);
            break;

        // FIXME - 3 - italic
        // FIXME - 4 - underline
        // FIXME - 5 - blink
        // FIXME - 7 - reverse video
        // FIXME - 9 - strikethrough
        // FIXME - 21 - double underline

        case 22:
            bold = false;
            recalc_colours(false);
            break;

        // FIXME - 23 - not italic
        // FIXME - 24 - not underlined
        // FIXME - 25 - not blinking

        case 30:
        case 31:
        case 32:
        case 33:
        case 34:
        case 35:
        case 36:
        case 37:
            fg_colour_index = num - 30;
            fg_bright = false;
            recalc_colours(true);
            break;

        // FIXME - 38 - custom foreground colour

        case 39:
            fg_colour_index = DEFAULT_FG_COLOUR_INDEX;
            fg_bright = false;
            fg_colour = DEFAULT_FG_COLOUR;
            break;

        case 40:
        case 41:
        case 42:
        case 43:
        case 44:
        case 45:
        case 46:
        case 47:
            bg_colour_index = num - 40;
            bg_bright = false;
            recalc_colours(false);
            break;

        // FIXME - 48 - custom background colour

        case 49:
            bg_colour_index = DEFAULT_BG_COLOUR_INDEX;
            bg_colour = DEFAULT_BG_COLOUR;
            bg_bright = false;
            break;

        case 90:
        case 91:
        case 92:
        case 93:
        case 94:
        case 95:
        case 96:
        case 97:
            fg_colour_index = num - 90;
            fg_bright = true;
            recalc_colours(true);
            break;

        case 100:
        case 101:
        case 102:
        case 103:
        case 104:
        case 105:
        case 106:
        case 107:
            bg_colour_index = num - 100;
            bg_bright = true;
            recalc_colours(false);
            break;
    }
}

void console_fb::enable_cursor(bool enabled) {
    show_cursor = enabled;
}

void console_fb::scroll_down(unsigned int rows) {
    memmove(&console[console_cols * rows], console,
            (console_rows - rows) * console_cols * sizeof(console_info));

    auto c = &console[0];

    auto glyph = FT_Get_Char_Index(face.face, ' ');

    for (unsigned int i = 0; i < console_cols * console_rows; i++) {
        console[i].changed = true;
    }

    for (unsigned int i = 0; i < console_cols * rows; i++) {
        c->bg = bg_colour;
        c->fg = fg_colour;
        c->glyph = glyph;
        c->bold = bold;
        c++;
    }
}

void console_fb::erase_in_line(unsigned int param) {
    switch (param) {
        case 0: // clear from cursor to end of line
            if (cursor_x == console_cols)
                return;

            if (console) {
                auto glyph = FT_Get_Char_Index(face.face, ' ');

                for (unsigned int x = cursor_x; x < console_cols; x++) {
                    auto& p = console[(cursor_y * console_cols) + x];

                    p.glyph = glyph;
                    p.bg = bg_colour;
                    p.fg = fg_colour;
                    p.bold = bold;
                    p.changed = true;
                }
            }

        break;

        case 1: // clear from beginning of line to cursor
            if (cursor_x == 0)
                return;

            if (console) {
                auto glyph = FT_Get_Char_Index(face.face, ' ');

                for (unsigned int x = 0; x < cursor_x; x++) {
                    auto& p = console[(cursor_y * console_cols) + x];

                    p.glyph = glyph;
                    p.bg = bg_colour;
                    p.fg = fg_colour;
                    p.bold = bold;
                    p.changed = true;
                }
            }

        break;

        case 2: // clear entire line
            if (console) {
                auto glyph = FT_Get_Char_Index(face.face, ' ');

                for (unsigned int x = 0; x < console_rows; x++) {
                    auto& p = console[(cursor_y * console_cols) + x];

                    p.glyph = glyph;
                    p.bg = bg_colour;
                    p.fg = fg_colour;
                    p.bold = bold;
                    p.changed = true;
                }
            }

        break;
    }
}

void console_fb::erase_in_display(unsigned int param) {
    auto glyph = FT_Get_Char_Index(face.face, ' ');

    switch (param) {
        case 0: // clear from cursor to end of screen
            if (cursor_y == console_rows)
                break;

            for (unsigned int x = cursor_x; x < console_cols; x++) {
                auto& p = console[(cursor_y * console_cols) + x];

                p.glyph = glyph;
                p.bg = bg_colour;
                p.fg = fg_colour;
                p.bold = bold;
                p.changed = true;
            }

            for (unsigned int y = cursor_y + 1; y < console_rows; y++) {
                for (unsigned int x = 0; x < console_cols; x++) {
                    auto& p = console[(y * console_cols) + x];

                    p.glyph = glyph;
                    p.bg = bg_colour;
                    p.fg = fg_colour;
                    p.bold = bold;
                    p.changed = true;
                }
            }

            break;

        case 1: // clear from beginning of screen to cursor
            for (unsigned int y = 0; y < cursor_y; y++) {
                for (unsigned int x = 0; x < console_cols; x++) {
                    auto& p = console[(y * console_cols) + x];

                    p.glyph = glyph;
                    p.bg = bg_colour;
                    p.fg = fg_colour;
                    p.bold = bold;
                    p.changed = true;
                }
            }

            for (unsigned int x = 0; x < cursor_x; x++) {
                auto& p = console[(cursor_y * console_cols) + x];

                p.glyph = glyph;
                p.bg = bg_colour;
                p.fg = fg_colour;
                p.bold = bold;
                p.changed = true;
            }

            break;

        case 2: // clear entire screen
        case 3:
            for (unsigned int y = 0; y < console_rows; y++) {
                for (unsigned int x = 0; x < console_cols; x++) {
                    auto& p = console[(y * console_cols) + x];

                    p.glyph = glyph;
                    p.bg = bg_colour;
                    p.fg = fg_colour;
                    p.bold = bold;
                    p.changed = true;
                }
            }

            break;
    }
}

static glyph_info* load_glyph(font_info& font, FT_UInt glyph) {
    auto* tree = find_rbtree_node(font.glyphs, glyph);
    if (tree)
        return &rbtree_containing_record<glyph_info>(tree);

    auto error = FT_Load_Glyph(font.face, glyph, FT_LOAD_RENDER | FT_LOAD_TARGET_NORMAL);
    if (error) // FIXME - report somehow
        return nullptr;

    auto gi = new glyph_info;
    if (!gi)
        return nullptr;

    gi->tree.key = glyph;
    gi->bitmap_left = font.face->glyph->bitmap_left;
    gi->bitmap_top = font.face->glyph->bitmap_top;

    FT_Bitmap_Init(&gi->bitmap);

    error = FT_Bitmap_Copy(ft, &font.face->glyph->bitmap, &gi->bitmap);
    if (error) {
        FT_Bitmap_Done(ft, &gi->bitmap);
        delete gi;
    }

    insert_into_rbtree(font.glyphs, gi->tree);

    return gi;
}

static void draw_ft_char(const console_info& c, unsigned int fb_x, unsigned int fb_y,
                         bool is_cursor) {
    span<uint32_t> fb;
    uint32_t pixels_per_scan_line;

    if (console_type == console_type::boot_framebuffer) {
        fb = shadow_framebuffer;
        pixels_per_scan_line = shadow_fb_pixels_per_scan_line;
    } else {
        fb = framebuffer;
        pixels_per_scan_line = fb_pixels_per_scan_line;
    }

    auto gi = load_glyph(face, c.glyph);
    if (!gi)
        return;

    auto& bitmap = gi->bitmap;
    auto buf = bitmap.buffer;

    auto width = (unsigned int)bitmap.width;
    if ((int)fb_x + gi->bitmap_left + (int)width > (int)shadow_fb_width)
        width = (unsigned int)((int)shadow_fb_width - (int)fb_x - gi->bitmap_left);

    int y = (int)fb_y - gi->bitmap_top;
    int bottom = y + (int)bitmap.rows;

    if (y < 0) {
        buf += bitmap.width * ((unsigned int)gi->bitmap_top - fb_y);
        y = 0;
    }

    if (y >= (int)shadow_fb_height)
        return;

    if (bottom > (int)shadow_fb_height)
        bottom = (int)shadow_fb_height;

    auto bg = c.bg;
    auto fg = c.fg;

    if (is_cursor)
        swap(bg, fg);

    uint32_t bg_colour_r = bg >> 16;
    uint32_t fg_colour_r = fg >> 16;
    uint32_t bg_colour_g = (bg & 0xff00) >> 8;
    uint32_t fg_colour_g = (fg & 0xff00) >> 8;
    uint32_t bg_colour_b = bg & 0xff;
    uint32_t fg_colour_b = fg & 0xff;

    auto base = fb.data() + ((uint32_t)y * pixels_per_scan_line) + fb_x + gi->bitmap_left;

    while (y < bottom) {
        for (unsigned int x = 0; x < width; x++) {
            if (*buf == 255)
                base[x] = fg;
            else if (*buf != 0) {
                auto r = (uint8_t)(((bg_colour_r * (255 - (uint32_t)*buf)) + (fg_colour_r * (uint32_t)*buf)) / 255);
                auto g = (uint8_t)(((bg_colour_g * (255 - (uint32_t)*buf)) + (fg_colour_g * (uint32_t)*buf)) / 255);
                auto b = (uint8_t)(((bg_colour_b * (255 - (uint32_t)*buf)) + (fg_colour_b * (uint32_t)*buf)) / 255);

                base[x] = (unsigned int)((r << 16) | (g << 8) | b);
            }

            buf++;
        }

        buf += bitmap.width - width;

        base += pixels_per_scan_line;
        y++;
    }
}

static void rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h, uint32_t col) {
    span<uint32_t> fb;
    uint32_t pixels_per_scan_line;

    if (console_type == console_type::boot_framebuffer) {
        fb = shadow_framebuffer;
        pixels_per_scan_line = shadow_fb_pixels_per_scan_line;
    } else {
        fb = framebuffer;
        pixels_per_scan_line = fb_pixels_per_scan_line;
    }

    auto dest = fb.data() + (pixels_per_scan_line * y) + x;

    for (unsigned int i = 0; i < h; i++) {
        for (unsigned int j = 0; j < w; j++) {
            *dest = col;
            dest++;
        }

        dest += pixels_per_scan_line - w;
    }
}

static void refresh_framebuffer() {
    auto* c = &console[0];

    for (unsigned int y = 0; y < console_fb_inst.console_rows; y++) {
        for (unsigned int x = 0; x < console_fb_inst.console_cols; x++) {
            if (!c->changed) {
                c++;
                continue;
            }

            bool is_cursor = show_cursor && x == console_fb_inst.cursor_x && y == console_fb_inst.cursor_y;

            rect(x * font_width, y * font_height, font_width, font_height, is_cursor ? c->fg : c->bg);
            draw_ft_char(*c, x * font_width, ((y + 1) * font_height) - font_descender, is_cursor);

            // don't leave black lines at edge of screen

            if (x == console_fb_inst.console_cols - 1 && font_width * console_fb_inst.console_cols < shadow_fb_width) {
                rect((x + 1) * font_width, y * font_height, shadow_fb_width - (font_width * console_fb_inst.console_cols),
                     font_height, c->bg);
            }

            if (y == console_fb_inst.console_rows - 1 && font_height * console_fb_inst.console_rows < shadow_fb_height) {
                rect(x * font_width, (y + 1) * font_height, font_width,
                     shadow_fb_height - (font_height * console_fb_inst.console_rows), c->bg);

                if (x == console_fb_inst.console_cols - 1 && font_width * console_fb_inst.console_cols < shadow_fb_width) {
                    rect((x + 1) * font_width, (y + 1) * font_height,
                         shadow_fb_width - (font_width * console_fb_inst.console_cols),
                         shadow_fb_height - (font_height * console_fb_inst.console_rows), c->bg);
                }
            }

            c++;
        }
    }
}

static void copy_shadow_framebuffer() {
    auto* c = &console[0];
    auto& sect = static_cast<section&>(framebuffer_section.get());

    unsigned int dirty_run_start = 0, dirty_run_length = 0;

    auto flush_lines = [&sect](unsigned int start, unsigned int length) {
        size_t start_off = shadow_fb_pixels_per_scan_line * start * font_height * sizeof(uint32_t);
        size_t end_off = shadow_fb_pixels_per_scan_line * (start + length + 1) * font_height * sizeof(uint32_t);

        size_t start_page = start_off >> PAGE_SHIFT;
        size_t end_page = (end_off + PAGE_SIZE - 1) >> PAGE_SHIFT;

        if (start_page >= sect.size >> PAGE_SHIFT)
            return;

        if (end_page > sect.size >> PAGE_SHIFT)
            end_page = sect.size >> PAGE_SHIFT;

        sect.custom_cache->flush_cache(start_page, end_page - start_page + 1);
    };

    for (unsigned int y = 0; y < console_fb_inst.console_rows; y++) {
        bool line_dirty = false;

        for (unsigned int x = 0; x < console_fb_inst.console_cols; x++) {
            if (!c->changed) {
                c++;
                continue;
            }

            line_dirty = true;
            break;
        }

        if (line_dirty) {
            if (dirty_run_start + dirty_run_length == y)
                dirty_run_length++;
            else {
                dirty_run_start = y;
                dirty_run_length = 1;
            }
        } else if (y != 0 && y == dirty_run_start + dirty_run_length)
            flush_lines(dirty_run_start, dirty_run_length);
    }

    if (dirty_run_start + dirty_run_length == console_fb_inst.console_rows)
        flush_lines(dirty_run_start, dirty_run_length);
}

template<bool no_lock>
void add_to_early_print_buf(string_view s) {
    auto func = [&]() {
        if (early_print_buf_size == sizeof(early_print_buf))
            return;

        auto to_copy = min(s.size(), sizeof(early_print_buf) - early_print_buf_size);

        memcpy(early_print_buf + early_print_buf_size, s.data(), to_copy);

        early_print_buf_size += to_copy;
    };

    if constexpr (no_lock)
        func();
    else {
        lock_guard lock(early_print_lock);
        func();
    }
}

void very_early_print(string_view s) {
    add_to_early_print_buf<true>(s);
}

void early_print(string_view s) {
    if (userspace_started && !in_bsod)
        return;

#ifdef __x86_64__
    if (console_type == console_type::text) {
        if (!text_console_started) {
            add_to_early_print_buf<false>(s);
            return;
        }

        do_print(console_text_inst, s);
        return;
    }
#endif

    if (!fb_console_started) {
        add_to_early_print_buf<false>(s);
        return;
    }

    disable_preempt();

    do_print(console_fb_inst, s);

    refresh_framebuffer();

    if (flush_framebuffer)
        flush_framebuffer(nullptr, 0, 0, shadow_fb_width, shadow_fb_height);
    else
        copy_shadow_framebuffer();

    enable_preempt();
}

void safe_print(string_view s) {
    uint64_t written_bytes;

    auto buf = span((const uint8_t*)s.data(), s.size());

    // FIXME - how can we print errors if write fails?

    object_write_without_offset(console_chardev.get(), span(&buf, 1),
                                written_bytes);

    object_write_without_offset(kmsg.get(), span(&buf, 1), written_bytes);

    // FIXME - this should be hidden behind a cmdline option
    if (console_type != console_type::other && com1_pipe_handle) {
        object_write_without_offset(com1_pipe_handle.get(), span(&buf, 1),
                                    written_bytes);
    }
}

static void console_event_recv() {
    auto buf_size = console_buffer_pages << PAGE_SHIFT;

    auto buf = (uint8_t*)vmalloc(buf_size);
    if (!buf)
        return;

    do {
        uint64_t read_bytes;

        wait_for_object(console_chardev.get(), false);

        auto sp = span(buf, buf_size);
        auto status = static_cast<class console&>(console_chardev.get()).read(span(&sp, 1), read_bytes);

        // FIXME - watch out for partial UTF-8 characters, partial escape sequences, etc.

        if (success(status) && read_bytes != 0)
            early_print(string_view((char*)buf, read_bytes));
    } while (true);

    vfree(buf, buf_size);
}

static void flush_early_print() {
    if (early_print_buf_size == 0)
        return;

    auto sp = span(early_print_buf, early_print_buf_size);
    auto page = allocate_folio(0);
    if (!page)
        bsod("allocate_folio failed in flush_early_print");

    while (!sp.empty()) {
        auto to_print = min(PAGE_SIZE, sp.size());

        memcpy(phys_address(page), sp.data(), to_print);

        safe_print(string_view((char*)phys_address(page), to_print));

        if (to_print == sp.size())
            break;

        sp = span(sp.data() + to_print, sp.size() - to_print);
    }

    free_folio(page);
}

static bool create_kmsg() {
    auto pptr = new kmsg_buf;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    kmsg = object_ref{*pptr};

    auto status = name_object(kmsg.get(), "kmsg");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

#ifdef __x86_64__
bool init_console(const loader_block_framebuffer& fb,
                  const loader_block_text_console& tc)
#else
bool init_console(const loader_block_framebuffer& fb)
#endif
{
    string_view console_opt, transform_opt;

    loop_through_cmdline([&](string_view s) {
        static const string_view console_prefix = "console=";
        static const string_view transform_prefix = "transform=";

        if (s.size() >= console_prefix.size() && string_view(s.data(), console_prefix.size()) == console_prefix)
            console_opt = string_view(s.data() + console_prefix.size(), s.size() - console_prefix.size());

        if (s.size() >= transform_prefix.size() && string_view(s.data(), transform_prefix.size()) == transform_prefix)
            transform_opt = string_view(s.data() + transform_prefix.size(), s.size() - transform_prefix.size());

        return true;
    });

    if (transform_opt.empty()) {
#ifdef __x86_64__
        // quirk for the Steam Deck, which has its framebuffer at 90 degrees
        if (smbios_manufacturer == "Valve" && (smbios_product_name == "Jupiter" || smbios_product_name == "Galileo"))
            fb_transform = fb_transform::rotated90;
#endif
    } else {
        if (transform_opt == "normal")
            fb_transform = fb_transform::normal;
        else if (transform_opt == "flipped")
            fb_transform = fb_transform::flipped;
        else if (transform_opt == "mirrored")
            fb_transform = fb_transform::mirrored;
        else if (transform_opt == "rotated90")
            fb_transform = fb_transform::rotated90;
        else if (transform_opt == "rotated180")
            fb_transform = fb_transform::rotated180;
        else if (transform_opt == "rotated270")
            fb_transform = fb_transform::rotated270;
        else
            print("unrecognized transform option \"{}\"\n", transform_opt);
    }

    if (!console_opt.empty()) {
        static const char prefix[] = "/dev/";

        // FIXME - these BSODs won't actually get displayed; do we need to initialize the FB in this case?

        if (console_opt.size() < sizeof(prefix) - 1 || memcmp(prefix, console_opt.data(), sizeof(prefix) - 1))
            bsod_format("unable to parse console option {}", console_opt);

        auto sv = string_view(console_opt.data() + sizeof(prefix) - 1,
                              console_opt.size() - sizeof(prefix) + 1);

        auto status = open_object(sv, console_chardev);
        if (!success(status))
            bsod_format("open_object returned {} trying to open console {}", status, console_opt);

        if (console_chardev.get().type != object_type::chardev)
            bsod_format("console option {} did not point to a character device", console_opt);

#ifdef __x86_64__
        console_text_inst.console_cols = 80; // FIXME
        console_text_inst.console_rows = 25; // FIXME
        console_text_inst.scroll_bottom = console_text_inst.console_rows - 1;
        console_cols = console_text_inst.console_cols;
#else
        console_cols = 80;
#endif

        console_type = console_type::other;

        if (!create_kmsg())
            return false;

        flush_early_print();

        return true;
    }

#ifdef __x86_64__
    if (console_type == console_type::text) {
        console_text_inst.console_cols = tc.cols;
        console_text_inst.console_rows = tc.rows;
        console_text_inst.scroll_bottom = console_text_inst.console_rows - 1;
        console_cols = console_text_inst.console_cols;

        text_console = span((uint16_t*)phys_address(tc.phys_address),
                            console_text_inst.console_cols * console_text_inst.console_rows);

        // clear console (foreground colour needs to be initialized for cursor to show up)
        for (uint8_t y = 0; y < console_text_inst.console_rows; y++) {
            for (uint8_t x = 0; x < console_text_inst.console_cols; x++) {
                text_console_putc(x, y, ' ', console_colour::black, console_colour::light_grey);
            }
        }

        console_text_inst.cursor_x = 0;
        console_text_inst.cursor_y = 0;

        text_console_cols = console_text_inst.console_cols;
        text_console_rows = console_text_inst.console_rows;

        text_console_started = true;

        create_text_console_section();
    } else {
#endif
        if (!framebuffer.empty()) { // not using boot framebuffer
            console_type = console_type::other_framebuffer;
            create_other_framebuffer_section();
        } else {
            uint32_t size = fb.pixels_per_scan_line * fb.height * sizeof(uint32_t);

            if (size == 0) {
                print("Framebuffer size was 0.\n");
                return false;
            }

            auto num_pages = bytes_to_pages(size);
            auto va = allocate_va(num_pages);
            if (!va) {
                print("allocate_va failed\n");
                return false;
            }

            framebuffer = span((uint32_t*)va, fb.pixels_per_scan_line * fb.height);

            for (unsigned int i = 0; i < num_pages; i++) {
                if (!map_page((uintptr_t)fb.phys_address + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::write_combine)) {
                    print("map_page failed\n");
                    return false;
                }
            }

            unsigned int shadow_fb_height_alloc;

            fb_width = fb.width;
            fb_height = fb.height;
            fb_pixels_per_scan_line = fb.pixels_per_scan_line;

            if (fb_transform == fb_transform::rotated90 || fb_transform == fb_transform::rotated270) {
                shadow_fb_width = fb_height;
                shadow_fb_height = fb_width;

                shadow_fb_height_alloc = shadow_fb_height;
#ifdef __x86_64__
                if (avx512_supported)
                    shadow_fb_height_alloc += 16;
                else
                    shadow_fb_height_alloc += 8;
#endif
            } else {
                shadow_fb_width = fb_width;
                shadow_fb_height = shadow_fb_height_alloc = fb_height;
            }

            // for AVX512, but also cache line
            static const size_t SCANLINE_ALIGNMENT_BYTES = 64;
            static const size_t SCANLINE_ALIGNMENT_PIXELS = SCANLINE_ALIGNMENT_BYTES / sizeof(uint32_t);

            shadow_fb_pixels_per_scan_line = shadow_fb_width;

            if (shadow_fb_pixels_per_scan_line % SCANLINE_ALIGNMENT_PIXELS)
                shadow_fb_pixels_per_scan_line += (uint32_t)(SCANLINE_ALIGNMENT_PIXELS - (shadow_fb_pixels_per_scan_line % SCANLINE_ALIGNMENT_PIXELS));

            auto ptr = (uint32_t*)vmalloc(shadow_fb_pixels_per_scan_line * shadow_fb_height_alloc * sizeof(uint32_t));
            if (!ptr) {
                print("vmalloc failed\n");
                return false;
            }

            shadow_framebuffer = span(ptr, shadow_fb_pixels_per_scan_line * shadow_fb_height_alloc);

            memset(framebuffer.data(), 0, framebuffer.size_bytes());
            memset(shadow_framebuffer.data(), 0, shadow_framebuffer.size_bytes());

            create_boot_framebuffer_section();

#ifdef __x86_64__
            if (avx512_supported && (fb_pixels_per_scan_line % (64 / sizeof(uint32_t)) == 0)) {
                copy_scanline = copy_scanline_avx512;
                copy_scanline_mirrored = copy_scanline_mirrored_avx512;
                copy_scanline_granularity = 64;

                flush_scanlines_rotated90 = flush_scanlines_rotated90_avx512;
                flush_scanlines_rotated270 = flush_scanlines_rotated270_avx512;
            } else if (fb_pixels_per_scan_line % (32 / sizeof(uint32_t)) == 0) {
                copy_scanline = copy_scanline_avx2;
                copy_scanline_mirrored = copy_scanline_mirrored_avx2;
                copy_scanline_granularity = 32;

                flush_scanlines_rotated90 = flush_scanlines_rotated90_avx2;
                flush_scanlines_rotated270 = flush_scanlines_rotated270_avx2;
            } else
                print("warning: framebuffer will be slow as fb_pixels_per_scan_line is {}, not a multiple of 8\n", fb_pixels_per_scan_line);
#elifdef __aarch64__
            if (fb_pixels_per_scan_line % (16 / sizeof(uint32_t)) == 0) {
                copy_scanline = copy_scanline_neon;
                copy_scanline_mirrored = copy_scanline_mirrored_neon;
                copy_scanline_granularity = 16;
            } else
                print("warning: framebuffer will be slow as fb_pixels_per_scan_line is {}, not a multiple of 4\n", fb_pixels_per_scan_line);
#endif
        }

        auto status = open_object("framebuffer", framebuffer_section);
        if (!success(status)) {
            print("open_object returned {} for /dev/framebuffer\n", status);
            return false;
        }
#ifdef __x86_64__
    }
#endif

    {
        auto pptr = new class console(console_buffer_pages);
        if (!pptr) {
            print("new failed\n");
            return false;
        }

        console_chardev = object_ref{*pptr};
    }

    if (!create_kmsg())
        return false;

#ifdef __x86_64__
    if (console_type == console_type::text)
        flush_early_print();
#endif

    if (framebuffer.empty()) { // gets started later if we have framebuffer
        if (!add_kernel_thread(console_event_recv)) {
            print("add_kernel_thread failed\n");
            return false;
        }
    }

    return true;
}

static void* ft_alloc(FT_Memory, long size) {
    return malloc((size_t)size);
}

static void* ft_realloc(FT_Memory, long, long new_size, void* block) {
    return realloc(block, (size_t)new_size);
}

static void ft_free(FT_Memory, void* block) {
    free(block);
}

extern "C"
FT_Memory FT_New_Memory() {
    auto ptr = (FT_MemoryRec_*)malloc(sizeof(FT_MemoryRec_));
    if (!ptr)
        return nullptr;

    auto& memory = *ptr;

    memory.user = nullptr;
    memory.alloc = ft_alloc;
    memory.realloc = ft_realloc;
    memory.free = ft_free;

    return &memory;
}

extern "C"
void FT_Done_Memory(FT_Memory memory) {
    free(memory);
}

bool init_fb_console() {
    FT_Error error;

    // FIXME - allow setting on cmdline
    static const unsigned int font_size = 12;

    error = FT_Init_FreeType(&ft);
    if (error) {
        print("FT_Init_FreeType failed ({:x})\n", error);
        return false;
    }

    // ignore EDID if it has an invalid manufacturer ID
    if (have_edid && edid[8] == 0 && edid[9] == 0) {
        print("ignoring EDID with manufacturer ID of 0\n");
        have_edid = false;
    }

    if (have_edid && edid[21] != 0) {
        uint8_t screen_x_cm = edid[21];

        fb_dpi = (fb_width * 254) / ((uint16_t)screen_x_cm * 100);
    } else
        fb_dpi = 96;

    error = FT_New_Memory_Face(ft, (uint8_t*)&console_font_data, (FT_Long)console_font_size,
                               0, &face.face);
    if (error) {
        print("FT_New_Memory_Face failed ({:x})\n", error);
        return false;
    }

    error = FT_Set_Char_Size(face.face, font_size * 64, 0, fb_dpi, 0);
    if (error) {
        print("FT_Set_Char_Size failed ({:x})\n", error);
        return false;
    }

    font_width = (unsigned int)(face.face->size->metrics.max_advance / 64);
    font_height = (unsigned int)(face.face->size->metrics.height / 64);
    font_descender = (unsigned int)(-face.face->size->metrics.descender / 64);

    console_fb_inst.console_cols = shadow_fb_width / font_width;
    console_fb_inst.console_rows = shadow_fb_height / font_height;
    console_fb_inst.scroll_bottom = console_fb_inst.console_rows - 1;
    console_cols = console_fb_inst.console_cols;

    console_fb_inst.cursor_x = 0;
    console_fb_inst.cursor_y = 0;

    console = (console_info*)vmalloc(console_fb_inst.console_cols * console_fb_inst.console_rows * sizeof(console_info));
    if (!console) {
        print("out of memory\n");
        return false;
    }

    auto glyph = FT_Get_Char_Index(face.face, ' ');

    for (unsigned int i = 0; i < console_fb_inst.console_cols * console_fb_inst.console_rows; i++) {
        console[i].bg = DEFAULT_BG_COLOUR;
        console[i].fg = DEFAULT_FG_COLOUR;
        console[i].bold = false;
        console[i].glyph = glyph;
        console[i].changed = false;
    }

    if (!add_kernel_thread(console_event_recv)) {
        print("add_kernel_thread failed\n");
        return false;
    }

    fb_console_started = true;

    flush_early_print();

    return true;
}

void copy_edid(loader_block* lb) {
    memcpy(edid.data(), (void*)lb->edid, edid.size());
    have_edid = true;
}

nt_status console::flush(bool go_readonly) {
    if (go_readonly)
        readonly = true;

    sync_lock sl;
    lock_guard l(sl);
    lock_guard l2(lock);

    if (buf_unread == 0)
        return nt_status::SUCCESS;

    size_t buf_read_pos = buf_write_pos + (buf_pages << PAGE_SHIFT) - buf_unread;

    while (buf_read_pos >= buf_pages << PAGE_SHIFT) {
        buf_read_pos -= buf_pages << PAGE_SHIFT;
    }

    if (buf_read_pos + buf_unread > buf_pages << PAGE_SHIFT) {
        auto first_bit = (buf_pages << PAGE_SHIFT) - buf_read_pos;

        early_print(string_view((char*)&buf[buf_read_pos], first_bit));

        buf_read_pos = 0;
        buf_unread -= first_bit;
    }

    early_print(string_view((char*)&buf[buf_read_pos], buf_unread));
    buf_unread = 0;

    signalled = false;

    if (!not_full_event.signalled)
        not_full_event.set_holding_sync_lock();

    return nt_status::SUCCESS;
}

template<bool NONBLOCK>
nt_status kmsg_buf::do_read(span<const span<uint8_t>> v, uint64_t& read_bytes) {
    list_entry del_list = { &del_list, &del_list };
    size_t vec_size = get_vec_size(v);

    if (vec_size == 0) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<uint8_t> top = v.front();

    {
        sync_lock sl;
        lock_guard lg1(sl);
        lock_guard lg2(lock);

        read_bytes = 0;

        while (vec_size > 0 && !is_list_empty(pages)) {
            auto& p = containing_record<kmsg_buf_page>(pages.next);
            auto page = (uint8_t*)phys_address(p.pa);
            auto to_copy = min(vec_size, (size_t)(p.write_pos - p.read_pos));

            if (to_copy == 0)
                break;

            memcpy_vec(v, top, page + p.read_pos, to_copy);

            p.read_pos += (uint16_t)to_copy;
            read_bytes += to_copy;

            vec_size -= to_copy;

            if (p.read_pos < PAGE_SIZE)
                break;

            remove_entry_list(p.le);
            insert_tail_list(del_list, p.le);
        }

        if (!is_list_empty(pages)) {
            auto& p = containing_record<kmsg_buf_page>(pages.prev);

            if (p.write_pos == p.read_pos)
                signalled = false;
        }
    }

    while (!is_list_empty(del_list)) {
        auto& p = containing_record<kmsg_buf_page>(del_list.next);

        remove_entry_list(p.le);

        free_folio(p.pa);
        delete &p;
    }

    return nt_status::SUCCESS;
}

nt_status kmsg_buf::write(span<const span<const uint8_t>> v,
                          uint64_t& written_bytes) {
    size_t vec_size = get_vec_size(v);

    if (vec_size == 0) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<const uint8_t> top = v.front();

    {
        lock_guard lg(lock);

        written_bytes = 0;

        if (!is_list_empty(pages) && containing_record<kmsg_buf_page>(pages.prev).write_pos < PAGE_SIZE) {
            auto& p = containing_record<kmsg_buf_page>(pages.prev);
            auto page = (uint8_t*)phys_address(p.pa);
            size_t to_copy = min(vec_size, PAGE_SIZE - p.write_pos);

            memcpy_vec(page + p.write_pos, v, top, to_copy);

            p.write_pos += (uint16_t)to_copy;
            written_bytes = to_copy;
            vec_size -= to_copy;
        }

        while (vec_size > 0) {
            auto p = new kmsg_buf_page;
            if (!p) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            p->pa = allocate_folio(0);
            if (!p->pa) {
                delete p;
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            insert_tail_list(pages, p->le);

            auto page = (uint8_t*)phys_address(p->pa);
            size_t to_copy = min(vec_size, PAGE_SIZE);

            memcpy_vec(page, v, top, to_copy);

            p->write_pos += (uint16_t)to_copy;
            written_bytes += to_copy;
            vec_size -= to_copy;
        }
    }

    if (written_bytes > 0) {
        sync_lock sl;
        lock_guard l(sl);

        if (!signalled) {
            signalled = true;

            wake_up_waiters(waiters, WAIT_TYPE_READING);
        }
    }

    return nt_status::SUCCESS;
}
