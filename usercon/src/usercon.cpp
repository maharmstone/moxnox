// SPDX-License-Identifier: GPL-2.0-only
#include <string_view>
#include <tuple>
#include <span>
#include <memory>
#include <array>
#include <optional>
#include <algorithm>
#include <mutex>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <pwd.h>
#include <cmath>
#include <xkbcommon/xkbcommon.h>

import nt;
import ntdll;
import format;
import rbtree;
import unique_handle;
import vt100;
import print;
import unicode;
import cp437;
import text;
import evdev;
import keyboard;
import pagesize;
import timepage;
import harfbuzz;
import freetype;
import emoji;
import moxnox_ioctls;

using namespace std;

static const string_view welcome_message1 = "\r\nWelcome to \e[1mMoxnox\e[m \U0001f98a (version ";
static const string_view welcome_message2 = ")\r\n\r\n";

static const char32_t TEXT_PRESENTATION = 0xfe0e;
static const char32_t EMOJI_PRESENTATION = 0xfe0f;
static const char32_t ZWJ = 0x200d;
static const char32_t KEYCAP = 0x20e3;
static const char32_t SKIN_TONE_1 = 0x1f3fb;
static const char32_t SKIN_TONE_6 = 0x1f3ff;
static const char32_t FLAG_LETTER_A = 0x1f1e6;
static const char32_t FLAG_LETTER_Z = 0x1f1ff;

extern uint64_t __CTOR_LIST__;

struct glyph_info {
    // FIXME - destructor

    rbtree_node<FT_UInt> tree;
    FT_Int bitmap_left;
    FT_Int bitmap_top;
    FT_Bitmap bitmap;
};

struct font_info {
    // FIXME - destructor

    FT_Face face;
    hb_font_t* hb_font;
    rbtree_node<FT_UInt>* glyphs = nullptr; // FIXME - destructor for this
    float scale;
};

struct console_cell {
    hb_glyph_position_t pos;
    FT_UInt glyph;
    uint32_t fg_colour;
    uint32_t bg_colour;
    font_info* font;
    bool ignore;
};

class hb_buffer_closer {
public:
    using pointer = hb_buffer_t*;

    void operator()(hb_buffer_t* buf) {
        hb_buffer_destroy(buf);
    }
};

using unique_hb_buffer = unique_ptr<hb_buffer_t*, hb_buffer_closer>;

struct text_attribute {
    font_info* font;
    uint32_t fg_colour;
    uint32_t bg_colour;
};

struct console_line {
    ~console_line() {
        if (text)
            free(text);

        if (atts)
            free(atts);
    }

    char32_t* text = nullptr;
    text_attribute* atts = nullptr;
    size_t len = 0;
    unsigned int num_cells = 0;
    bool changed = false;
    console_cell cells[0];
};

static FT_Library ft;
static font_info normal_font, bold_font, italic_font, bold_italic_font, emoji_font;
static MN_SECTION_FRAMEBUFFER_INFORMATION fb_info;
static span<uint32_t> framebuffer;
static unsigned int font_width, font_height, font_descender;
static console_line* curline = nullptr;
static console_line** lines = nullptr;
static console_line** normal_lines = nullptr;
static console_line** alt_lines = nullptr;

static array<char32_t, 32> prev_emoji_cps;
static size_t prev_emoji_cp_len = 0;
static unsigned int prev_emoji_cursor_x = 0;
static unsigned int prev_emoji_num_cells = 0;
static console_line* prev_emoji_line = nullptr;

struct console_fb {
    void console_overflow(unsigned int rows = 1);
    static void handle_sgr_part_num(unsigned int num);
    void print_chars(span<const char32_t> s);
    void cursor_changed();
    void scroll_down(unsigned int rows);
    void enable_cursor(bool enable);
    void move_cursor() { }
    void erase_in_line(unsigned int param);
    void erase_in_display(unsigned int param);
    void delete_chars(unsigned int num);
    void erase_chars(unsigned int num);
    void insert_chars(unsigned int num);
    void insert_lines(unsigned int num);
    void delete_lines(unsigned int num);
    void enter_alt_screen();
    void exit_alt_screen();
    void write_input_data(string_view s);
    static uint32_t get_bg_colour() { return 0x000000; }

    unsigned int cursor_x, cursor_y;
    unsigned int console_rows, console_cols;
    unsigned int scroll_top = 0, scroll_bottom;
    unsigned int normal_cursor_x = 0, normal_cursor_y = 0;
    unsigned int normal_scroll_top = 0, normal_scroll_bottom = 0;
    bool* tab_stops;
    unsigned int tab_stops_count;
    bool decckm = false;
    bool auto_wrap = true;
    bool insert_mode = false;
    bool alt_screen_active = false;
    bool show_cursor = true;
    HANDLE server_handle = 0;
};

static_assert(vt100_console_with_tab_stops<console_fb>);

static console_fb console_fb_inst;

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

static const unsigned int DEFAULT_BG_COLOUR_INDEX = 0;
static const unsigned int DEFAULT_FG_COLOUR_INDEX = 7;
static const uint32_t DEFAULT_BG_COLOUR = vt100_colours[DEFAULT_BG_COLOUR_INDEX];
static const uint32_t DEFAULT_FG_COLOUR = vt100_colours[DEFAULT_FG_COLOUR_INDEX];
static uint32_t bg_colour_index = 0, fg_colour_index = 7;
static bool bg_bright = false, fg_bright = false, bold = false, italic = false;
static uint32_t bg_colour = DEFAULT_BG_COLOUR, fg_colour = DEFAULT_FG_COLOUR;
static bool reverse_video = false;

void do_print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardError, 0, nullptr, nullptr, &iosb, s.data(),
                (uint32_t)s.size(), nullptr, nullptr);
}

static void reset_line(console_line& line) {
    if (line.text)
        free(line.text);

    if (line.atts)
        free(line.atts);

    line.text = nullptr;
    line.atts = nullptr;
    line.len = 0;
    line.num_cells = 0;

    for (unsigned int i = 0; i < console_fb_inst.console_cols; i++) {
        line.cells[i].fg_colour = !reverse_video ? fg_colour : bg_colour;
        line.cells[i].bg_colour = !reverse_video ? bg_colour : fg_colour;
    }

    line.changed = true;
}

void console_fb::console_overflow(unsigned int rows) {
    rows = min(rows, scroll_bottom - scroll_top + 1);

    for (unsigned int i = 0; i < rows; i++) {
        auto* recycled = lines[scroll_top];
        memmove(&lines[scroll_top], &lines[scroll_top + 1],
                (scroll_bottom - scroll_top) * sizeof(console_line*));
        lines[scroll_bottom] = recycled;
        reset_line(*recycled);
    }

    for (unsigned int i = scroll_top; i <= scroll_bottom; i++) {
        lines[i]->changed = true;
    }

    curline = lines[scroll_bottom];
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
            italic = false;
            reverse_video = false;
            break;

        case 1:
            bold = true;
            recalc_colours(true);
            break;

        case 3:
            italic = true;
            break;

        // FIXME - 4 - underline
        // FIXME - 5 - blink

        case 7:
            reverse_video = true;
            break;

        // FIXME - 9 - strikethrough
        // FIXME - 21 - double underline

        case 22:
            bold = false;
            recalc_colours(false);
            break;

        case 23:
            italic = false;
            break;

        // FIXME - 24 - not underlined
        // FIXME - 25 - not blinking

        case 27:
            reverse_video = false;
            break;

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

void console_fb::cursor_changed() {
    if (show_cursor)
        curline->changed = true;

    curline = lines[cursor_y];

    if (show_cursor)
        curline->changed = true;
}

void console_fb::scroll_down(unsigned int rows) {
    rows = min(scroll_bottom - scroll_top + 1, rows);

    for (unsigned int i = 0; i < rows; i++) {
        auto* recycled = lines[scroll_bottom];
        memmove(&lines[scroll_top + 1], &lines[scroll_top],
                (scroll_bottom - scroll_top) * sizeof(console_line*));
        lines[scroll_top] = recycled;
        reset_line(*recycled);
    }

    for (unsigned int i = scroll_top; i <= scroll_bottom; i++) {
        lines[i]->changed = true;
    }

    curline = lines[cursor_y];
}

void console_fb::enable_cursor(bool enable) {
    show_cursor = enable;
}

static bool init_font(span<const uint8_t> buf, font_info& font,
                      bool main_font) {
    FT_Error error;
    unsigned int font_size = 12; // FIXME - allow setting somehow

    error = FT_New_Memory_Face(ft, buf.data(), (FT_Long)buf.size(),
                               0, &font.face);
    if (error) {
        print("FT_New_Memory_Face failed ({}, {})\n", error, FT_Error_String(error));
        return false;
    }

    if (!(font.face->face_flags & FT_FACE_FLAG_SCALABLE)) {
        optional<FT_Int> size;

        auto px = (font_size * fb_info.dpi) / 72;

        // choose smallest larger than or equal to px
        for (FT_Int i = 0; i < font.face->num_fixed_sizes; i++) {
            auto& sz = font.face->available_sizes[i];

            if ((unsigned int)sz.height >= px && (!size.has_value() || font.face->available_sizes[*size].height > sz.height))
                size = i;
        }

        // failing that choose largest
        if (!size.has_value()) {
            for (FT_Int i = 0; i < font.face->num_fixed_sizes; i++) {
                auto& sz = font.face->available_sizes[i];

                if (!size.has_value() || font.face->available_sizes[*size].height < sz.height)
                    size = i;
            }
        }

        if (!size.has_value()) {
            print("could not choose size for bitmap font\n");
            return false;
        }

        error = FT_Select_Size(font.face, *size);
        if (error) {
            print("FT_Select_Size failed ({}, {})\n", error, FT_Error_String(error));
            return false;
        }

        font.scale = (float)px / (float)font.face->available_sizes[*size].height;
    } else {
        error = FT_Set_Char_Size(font.face, (FT_F26Dot6)(font_size * 64), 0,
                                fb_info.dpi, 0);
        if (error) {
            print("FT_Set_Char_Size failed ({}, {})\n", error, FT_Error_String(error));
            return false;
        }
    }

    font.hb_font = hb_ft_font_create(font.face, nullptr);
    if (!font.hb_font) {
        print("hb_ft_font_create failed\n");
        FT_Done_Face(font.face);
        return false;
    }

    if (main_font) {
        font_width = (unsigned int)(font.face->size->metrics.max_advance / 64);
        font_height = (unsigned int)(font.face->size->metrics.height / 64);
        font_descender = (unsigned int)(-font.face->size->metrics.descender / 64);

        console_fb_inst.console_cols = fb_info.width / font_width;
        console_fb_inst.console_rows = fb_info.height / font_height;
        console_fb_inst.scroll_bottom = console_fb_inst.console_rows - 1;

        console_fb_inst.tab_stops = (bool*)malloc(console_fb_inst.console_cols * sizeof(bool));
        if (!console_fb_inst.tab_stops) {
            print("malloc failed\n");
            FT_Done_Face(font.face);
            FT_Done_FreeType(ft);
            return false;
        }

        console_fb_inst.tab_stops_count = console_fb_inst.console_cols;

        for (unsigned int i = 0; i < console_fb_inst.console_cols; i++) {
            console_fb_inst.tab_stops[i] = i > 0 && i % 8 == 0;
        }

        auto line_size = offsetof(console_line, cells) + (sizeof(console_cell) * console_fb_inst.console_cols);

        auto rows = console_fb_inst.console_rows;

        normal_lines = (console_line**)malloc(rows * sizeof(console_line*));
        if (!normal_lines) {
            print("malloc failed\n");
            free(console_fb_inst.tab_stops);
            FT_Done_Face(font.face);
            FT_Done_FreeType(ft);
            return false;
        }

        alt_lines = (console_line**)malloc(rows * sizeof(console_line*));
        if (!alt_lines) {
            print("malloc failed\n");
            free(console_fb_inst.tab_stops);
            free(normal_lines);
            FT_Done_Face(font.face);
            FT_Done_FreeType(ft);
            return false;
        }

        for (unsigned int i = 0; i < rows; i++) {
            auto* line = (console_line*)malloc(line_size);
            if (!line) {
                print("malloc failed\n");

                for (unsigned int j = 0; j < i; j++) {
                    free(normal_lines[j]);
                }

                free(console_fb_inst.tab_stops);
                free(normal_lines);
                free(alt_lines);

                FT_Done_Face(font.face);
                FT_Done_FreeType(ft);
                return false;
            }

            new (line) console_line;

            for (unsigned int j = 0; j < console_fb_inst.console_cols; j++) {
                line->cells[j].fg_colour = !reverse_video ? fg_colour : bg_colour;
                line->cells[j].bg_colour = !reverse_video ? bg_colour : fg_colour;
            }

            normal_lines[i] = line;
        }

        for (unsigned int i = 0; i < rows; i++) {
            auto* line = (console_line*)malloc(line_size);
            if (!line) {
                print("malloc failed\n");

                for (unsigned int j = 0; j < rows; j++) {
                    free(normal_lines[j]);
                }

                for (unsigned int j = 0; j < i; j++) {
                    free(alt_lines[j]);
                }

                free(console_fb_inst.tab_stops);
                free(normal_lines);
                free(alt_lines);

                FT_Done_Face(font.face);
                FT_Done_FreeType(ft);
                return false;
            }

            new (line) console_line;

            for (unsigned int j = 0; j < console_fb_inst.console_cols; j++) {
                line->cells[j].fg_colour = !reverse_video ? fg_colour : bg_colour;
                line->cells[j].bg_colour = !reverse_video ? bg_colour : fg_colour;
            }

            alt_lines[i] = line;
        }

        lines = normal_lines;
        curline = lines[0];
    }

    return true;
}

static span<uint32_t> init_framebuffer(bool& not_found) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;
    HANDLE sect;
    void* addr;
    size_t size, return_length;

    static const u16string_view fn = u"\\dev\\framebuffer";

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenSection(&sect, SECTION_MAP_READ | SECTION_MAP_WRITE, &oa);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        not_found = true;
        return {};
    } else if (!success(status)) {
        print("NtOpenSection returned {}\n", status);
        return {};
    }

    status = NtQuerySection(sect, SECTION_INFORMATION_CLASS::MoxnoxFramebufferInformation,
                            &fb_info, sizeof(fb_info), &return_length);
    if (!success(status)) {
        print("NtQuerySection returned {}\n", status);
        NtClose(sect);
        return {};
    }

    addr = nullptr;
    size = 0;

    status = NtMapViewOfSection(sect, NtCurrentProcess(), &addr, 0, 0, nullptr,
                                &size, SECTION_INHERIT::ViewUnmap, 0,
                                PAGE_READWRITE);
    if (!success(status)) {
        print("NtMapViewOfSection returned {}\n", status);
        NtClose(sect);
        return {};
    }

    // FIXME - unmap this at end of program

    NtClose(sect);

    return span((uint32_t*)addr, size / sizeof(uint32_t));
}

static span<const uint8_t> load_font(u16string_view fn) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE f, sect;
    FILE_STANDARD_INFORMATION fsi;
    void* addr;
    size_t size;

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&f, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print("NtCreateFile returned {}\n", status);
        return {};
    }

    status = NtQueryInformationFile(f, &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print("NtQueryInformationFile returned {}\n", status);
        NtClose(f);
        return {};
    }

    status = NtCreateSection(&sect, SECTION_MAP_READ, nullptr, nullptr,
                             PAGE_READONLY, SEC_COMMIT, f);
    if (!success(status)) {
        print("NtCreateSection returned {}\n", status);
        NtClose(f);
        return {};
    }

    NtClose(f);

    addr = nullptr;
    size = 0;

    // FIXME - unmap this at end of program

    status = NtMapViewOfSection(sect, NtCurrentProcess(), &addr, 0, 0, nullptr,
                                &size, SECTION_INHERIT::ViewUnmap, 0, PAGE_READONLY);
    if (!success(status)) {
        print("NtMapViewOfSection returned {}\n", status);
        NtClose(sect);
        return {};
    }

    NtClose(sect);

    return span((uint8_t*)addr, (size_t)fsi.EndOfFile.QuadPart);
}

static glyph_info* load_glyph(font_info& font, FT_UInt glyph) {
    auto* tree = find_rbtree_node(font.glyphs, glyph);
    if (tree)
        return &rbtree_containing_record<glyph_info>(tree);

    int32_t flags = FT_LOAD_TARGET_NORMAL;
    bool colour = !(font.face->face_flags & FT_FACE_FLAG_SCALABLE) &&
                  font.face->face_flags & FT_FACE_FLAG_COLOR;

    if (colour)
        flags |= FT_LOAD_COLOR;
    else
        flags |= FT_LOAD_RENDER;

    auto error = FT_Load_Glyph(font.face, glyph, flags);
    if (error) {
        print("FT_Load_Glyph returned {} ({})\n", error, FT_Error_String(error));
        return nullptr;
    }

    auto gi = new glyph_info;
    if (!gi)
        return nullptr;

    gi->tree.key = glyph;

    FT_Bitmap_Init(&gi->bitmap);

    if (!(font.face->face_flags & FT_FACE_FLAG_SCALABLE)) {
        auto& src = font.face->glyph->bitmap;

        gi->bitmap_left = (FT_Int)((float)font.face->glyph->bitmap_left * font.scale);
        gi->bitmap_top = (FT_Int)((float)font.face->glyph->bitmap_top * font.scale);

        // FIXME - call FT_Bitmap_Convert if not FT_PIXEL_MODE_GRAY (and not colour)

        gi->bitmap.width = (unsigned int)((float)src.width * font.scale);
        gi->bitmap.rows = (unsigned int)((float)src.rows * font.scale);
        gi->bitmap.pixel_mode = src.pixel_mode;
        gi->bitmap.num_grays = src.num_grays;

        unsigned int bytes = colour ? 4 : 1;

        gi->bitmap.pitch = (int)(gi->bitmap.width * bytes);

        if (!src.buffer)
            gi->bitmap.buffer = nullptr;
        else {
            gi->bitmap.buffer = (uint8_t*)malloc((unsigned int)gi->bitmap.pitch * gi->bitmap.rows);
            if (!gi->bitmap.buffer) {
                print("malloc failed\n");
                delete gi;
                return nullptr;
            }

            auto src_buf = src.buffer;
            auto* buf = gi->bitmap.buffer;

            for (unsigned int y = 0; y < gi->bitmap.rows; y++) {
                for (unsigned int x = 0; x < gi->bitmap.width; x++) {
                    float src_x0 = (float)x / font.scale;
                    float src_y0 = (float)y / font.scale;
                    float src_x1 = (float)(x + 1) / font.scale;
                    float src_y1 = (float)(y + 1) / font.scale;

                    unsigned int x0 = (unsigned int)src_x0;
                    unsigned int y0 = (unsigned int)src_y0;
                    unsigned int x1 = min((unsigned int)ceilf(src_x1), src.width);
                    unsigned int y1 = min((unsigned int)ceilf(src_y1), src.rows);

                    for (unsigned int i = 0; i < bytes; i++) {
                        float sum = 0;
                        unsigned int count = 0;

                        for (unsigned int sy = y0; sy < y1; sy++) {
                            auto* row = &src_buf[sy * (unsigned int)src.pitch];
                            for (unsigned int sx = x0; sx < x1; sx++) {
                                sum += row[(sx * bytes) + i];
                                count++;
                            }
                        }

                        buf[(y * (unsigned int)gi->bitmap.pitch) + (x * bytes) + i] =
                            (uint8_t)(sum / (float)count);
                    }
                }
            }
        }
    } else {
        gi->bitmap_left = font.face->glyph->bitmap_left;
        gi->bitmap_top = font.face->glyph->bitmap_top;

        error = FT_Bitmap_Copy(ft, &font.face->glyph->bitmap, &gi->bitmap);
        if (error) {
            FT_Bitmap_Done(ft, &gi->bitmap);
            delete gi;
            return nullptr;
        }
    }

    insert_into_rbtree(font.glyphs, gi->tree);

    return gi;
}

static void draw_ft_char(const console_cell& cell, unsigned int fb_x, unsigned int fb_y,
                         bool is_cursor, font_info& font) {
    auto gi = load_glyph(font, cell.glyph);
    if (!gi)
        return;

    auto& bitmap = gi->bitmap;
    auto buf = bitmap.buffer;

    auto width = (unsigned int)bitmap.width;
    if ((int)fb_x + gi->bitmap_left + (int)width > (int)fb_info.width)
        width = (unsigned int)((int)fb_info.width - (int)fb_x - gi->bitmap_left);

    int y = (int)fb_y - gi->bitmap_top;
    int bottom = y + (int)bitmap.rows;

    if (y < 0) {
        if (font.face->face_flags & FT_FACE_FLAG_COLOR)
            buf += bitmap.width * ((unsigned int)gi->bitmap_top - fb_y) * sizeof(uint32_t);
        else
            buf += bitmap.width * ((unsigned int)gi->bitmap_top - fb_y);
        y = 0;
    }

    if (y >= (int)fb_info.height)
        return;

    if (bottom > (int)fb_info.height)
        bottom = (int)fb_info.height;

    auto bg = is_cursor ? cell.fg_colour : cell.bg_colour;
    uint8_t bg_colour_r = (uint8_t)(bg >> 16);
    uint8_t bg_colour_g = (bg & 0xff00) >> 8;
    uint8_t bg_colour_b = bg & 0xff;

    auto base = framebuffer.data() + ((uint32_t)y * fb_info.pixels_per_scan_line) + fb_x + gi->bitmap_left;

    auto byte_lerp = [](uint8_t a, uint8_t b, uint8_t t) {
        return (uint8_t)lerp((float)a, (float)b, (float)t / 255.0f);
    };

    if (font.face->face_flags & FT_FACE_FLAG_COLOR) {
        while (y < bottom) {
            for (unsigned int x = 0; x < width; x++) {
                uint32_t val = ((uint32_t)buf[2] << 16) | ((uint32_t)buf[1] << 8) | (uint32_t)buf[0];
                uint8_t alpha = buf[3];

                if (alpha == 0)
                    base[x] = bg;
                else if (alpha == 0xff)
                    base[x] = val;
                else {
                    auto r = byte_lerp(bg_colour_r, buf[2], alpha);
                    auto g = byte_lerp(bg_colour_g, buf[1], alpha);
                    auto b = byte_lerp(bg_colour_b, buf[0], alpha);

                    base[x] = (unsigned int)((r << 16) | (g << 8) | b);
                }

                buf += sizeof(uint32_t);
            }

            buf += (bitmap.width - width) * sizeof(uint32_t);

            base += fb_info.pixels_per_scan_line;
            y++;
        }
    } else {
        auto fg = is_cursor ? cell.bg_colour : cell.fg_colour;

        uint8_t fg_colour_r = (uint8_t)(fg >> 16);
        uint8_t fg_colour_g = (fg & 0xff00) >> 8;
        uint8_t fg_colour_b = fg & 0xff;

        while (y < bottom) {
            for (unsigned int x = 0; x < width; x++) {
                if (*buf == 255)
                    base[x] = fg;
                else if (*buf != 0) {
                    auto r = byte_lerp(bg_colour_r, fg_colour_r, *buf);
                    auto g = byte_lerp(bg_colour_g, fg_colour_g, *buf);
                    auto b = byte_lerp(bg_colour_b, fg_colour_b, *buf);

                    base[x] = (unsigned int)((r << 16) | (g << 8) | b);
                }

                buf++;
            }

            buf += bitmap.width - width;

            base += fb_info.pixels_per_scan_line;
            y++;
        }
    }
}

static void rect(unsigned int x, unsigned int y, unsigned int w, unsigned int h,
                 uint32_t col) {
    auto dest = framebuffer.data() + (fb_info.pixels_per_scan_line * y) + x;

    for (unsigned int i = 0; i < h; i++) {
        for (unsigned int j = 0; j < w; j++) {
            *dest = col;
            dest++;
        }

        dest += fb_info.pixels_per_scan_line - w;
    }
}

static void print_line(console_line& line, unsigned int sy) {
    for (unsigned int i = 0; i < line.num_cells; i++) {
        double x_offset = line.cells[i].pos.x_offset / 64.0;
        double y_offset = line.cells[i].pos.y_offset / 64.0;

        bool is_cursor = console_fb_inst.show_cursor &&
            i == console_fb_inst.cursor_x &&
            sy == console_fb_inst.cursor_y;

        auto bg = is_cursor ? line.cells[i].fg_colour : line.cells[i].bg_colour;

        if (line.cells[i].ignore)
            continue;

        if (i < line.num_cells - 1 && line.cells[i + 1].ignore)
            rect(i * font_width, sy * font_height, font_width * 2, font_height, bg);
        else
            rect(i * font_width, sy * font_height, font_width, font_height, bg);

        draw_ft_char(line.cells[i],
                     (i * font_width) + (unsigned int)x_offset,
                     ((sy + 1) * font_height) + (unsigned int)y_offset - font_descender,
                     is_cursor, *line.cells[i].font);
    }

    for (unsigned int i = line.num_cells; i < console_fb_inst.console_cols; i++) {
        bool is_cursor = console_fb_inst.show_cursor &&
            i == console_fb_inst.cursor_x &&
            sy == console_fb_inst.cursor_y;

        auto bg = is_cursor ? line.cells[i].fg_colour : line.cells[i].bg_colour;

        rect(i * font_width, sy * font_height, font_width, font_height, bg);
    }
}

static void flush_framebuffer() {
    void* flush_addr = framebuffer.data();
    size_t flush_size = framebuffer.size_bytes();
    IO_STATUS_BLOCK iosb;

    auto status = NtFlushVirtualMemory(NtCurrentProcess(), &flush_addr,
                                       &flush_size, &iosb);
    if (!success(status)) {
        print("NtFlushVirtualMemory returned {}\n", status);
        return;
    }
}

static bool is_emoji_continuation(char32_t c) {
    if (c == ZWJ || c == EMOJI_PRESENTATION || c == KEYCAP)
        return true;

    if (c >= SKIN_TONE_1 && c <= SKIN_TONE_6)
        return true;

    if (prev_emoji_cp_len > 0 && prev_emoji_cps[prev_emoji_cp_len - 1] == ZWJ &&
        is_emoji(c)) {
        return true;
    }

    if (c >= FLAG_LETTER_A && c <= FLAG_LETTER_Z &&
        prev_emoji_cp_len == 1 &&
        prev_emoji_cps[0] >= FLAG_LETTER_A && prev_emoji_cps[0] <= FLAG_LETTER_Z) {
        return true;
    }

    return false;
}

static bool is_emoji_char(char32_t c, char32_t next) {
    if (!is_emoji(c))
        return false;

    if (next == EMOJI_PRESENTATION)
        return true;

    if (next == TEXT_PRESENTATION)
        return false;

    return has_emoji_presentation(c);
}

static bool is_cluster_wide(span<const char32_t> s) {

    for (size_t i = 0; i < s.size(); i++) {
        auto next = i < s.size() - 1 ? s[i + 1] : 0;

        if (is_emoji_char(s[i], next))
            return true;
    }

    return false;
}

static span<const char32_t> find_font_run(span<const char32_t> s,
                                          font_info*& font) {
    if (is_emoji_char(s[0], s.size() > 1 ? s[1] : 0)) {
        size_t len = 1;

        if (s.size() > 1 && s[1] == EMOJI_PRESENTATION)
            len++;

        while (len < s.size()) {
            auto next = len < s.size() - 1 ? s[len + 1] : 0;
            bool after_zwj = len > 0 && s[len - 1] == ZWJ;

            if (s[len] == ZWJ || s[len] == EMOJI_PRESENTATION || s[len] == KEYCAP) {
                len++;
                continue;
            }

            if (!is_emoji(s[len]))
                break;

            if (!after_zwj && next != EMOJI_PRESENTATION &&
                (next == TEXT_PRESENTATION || !has_emoji_presentation(s[len]))) {
                break;
            }

            len++;
        }

        font = &emoji_font;
        return s.subspan(0, len);
    }

    font = &normal_font;

    for (size_t i = 0; i < s.size(); i++) {
        if (is_emoji_char(s[i], i < s.size() - 1 ? s[i + 1] : 0))
            return s.subspan(0, i);
    }

    return s;
}

static span<const char32_t> hb_clus(span<const char32_t> s,
                                    span<hb_glyph_info_t> info, size_t i) {
    size_t num_cp;

    if (i == info.size() - 1)
        num_cp = s.size() - info[i].cluster;
    else
        num_cp = info[i + 1].cluster - info[i].cluster;

    if (info[i].cluster + num_cp < s.size() &&
        (s[info[i].cluster + num_cp] == TEXT_PRESENTATION || s[info[i].cluster + num_cp] == EMOJI_PRESENTATION)) {
        num_cp++;
    }

    return span(&s[info[i].cluster], num_cp);
}

static unsigned int find_text_pos(unsigned int cursor_x) {
    unsigned int ret = 0, num_cells = 0;
    auto text = span(curline->text, curline->len);

    while (true) {
        font_info* font;
        auto s = find_font_run(text, font);

        unique_hb_buffer buf{hb_buffer_create()};

        hb_buffer_add_codepoints(buf.get(), (const hb_codepoint_t*)s.data(),
                                 (int)s.size(), 0, -1);
        hb_buffer_guess_segment_properties(buf.get());

        hb_shape(font->hb_font, buf.get(), nullptr, 0);

        auto len = hb_buffer_get_length(buf.get());
        auto info = hb_buffer_get_glyph_infos(buf.get(), nullptr);

        for (unsigned int i = 0; i < len; i++) {
            auto clus = hb_clus(s, span(info, len), i);
            bool wide = is_cluster_wide(clus);

            if (num_cells == cursor_x || (wide && num_cells + 1 == cursor_x))
                return ret + info[i].cluster;

            if (wide)
                num_cells += 2;
            else
                num_cells++;
        }

        ret += (unsigned int)s.size();

        if (text.size() == s.size())
            break;

        text = span(text.data() + s.size(), text.size() - s.size());
    }

    return ret;
}

static void print_chars2(console_fb& con, font_info& font,
                         span<const char32_t> s) {
    unsigned int text_pos;
    font_info* cur_font;

    // get current position in string

    auto orig_cursor_x = con.cursor_x;

    if (curline->len == 0)
        text_pos = orig_cursor_x;
    else
        text_pos = find_text_pos(con.cursor_x);

    if (&font == &normal_font) {
        if (bold && italic)
            cur_font = &bold_italic_font;
        else if (bold)
            cur_font = &bold_font;
        else if (italic)
            cur_font = &italic_font;
        else
            cur_font = &normal_font;
    } else
        cur_font = &font;

    auto fgc = !reverse_video ? fg_colour : bg_colour;
    auto bgc = !reverse_video ? bg_colour : fg_colour;

    // append to existing buffer

    if (text_pos + s.size() < curline->len) {
        memcpy(&curline->text[text_pos], s.data(), s.size() * sizeof(char32_t));

        for (size_t i = 0; i < s.size(); i++) {
            curline->atts[text_pos + i].font = cur_font;
            curline->atts[text_pos + i].fg_colour = fgc;
            curline->atts[text_pos + i].bg_colour = bgc;
        }
    } else {
        auto new_len = max(text_pos + s.size(), curline->len);
        auto t = (char32_t*)malloc(new_len * sizeof(char32_t));
        if (!t) {
            print("malloc failed\n");
            return;
        }

        auto a = (text_attribute*)malloc(new_len * sizeof(text_attribute));
        if (!a) {
            print("malloc failed\n");
            free(t);
            return;
        }

        // preserve existing text before insertion point
        if (curline->len > 0 && text_pos > 0) {
            auto copy_len = min((size_t)text_pos, curline->len);

            memcpy(t, curline->text, copy_len * sizeof(char32_t));
            memcpy(a, curline->atts, copy_len * sizeof(text_attribute));
        }

        // fill any gap between end of existing content and insertion point with spaces
        for (size_t i = curline->len; i < text_pos; i++) {
            t[i] = U' ';
            a[i].font = cur_font;
            a[i].fg_colour = curline->cells[i].fg_colour;
            a[i].bg_colour = curline->cells[i].bg_colour;
        }

        memcpy(t + text_pos, s.data(), s.size() * sizeof(char32_t));

        for (size_t i = 0; i < s.size(); i++) {
            a[text_pos + i].font = cur_font;
            a[text_pos + i].fg_colour = fgc;
            a[text_pos + i].bg_colour = bgc;
        }

        if (curline->text)
            free(curline->text);

        curline->text = t;
        curline->atts = a;
        curline->len = new_len;
    }

    // lay out using harfbuzz

    size_t s_offset = 0;
    unsigned int final_cursor_x = con.cursor_x;

    do {
        // FIXME - keep HB buffer around?
        // FIXME - cap how much data we send to hb_buffer_add_utf8?

        unique_hb_buffer buf{hb_buffer_create()};

        auto s_rem = s.subspan(s_offset);

        hb_buffer_add_codepoints(buf.get(), (const hb_codepoint_t*)s_rem.data(),
                                 (int)s_rem.size(), 0, -1);
        hb_buffer_guess_segment_properties(buf.get());

        hb_shape(font.hb_font, buf.get(), nullptr, 0);

        auto len = hb_buffer_get_length(buf.get());
        auto info = hb_buffer_get_glyph_infos(buf.get(), nullptr);
        auto pos = hb_buffer_get_glyph_positions(buf.get(), nullptr);

        if (orig_cursor_x > curline->num_cells) {
            auto space_glyph = FT_Get_Char_Index(cur_font->face, U' ');

            for (auto i = curline->num_cells; i < orig_cursor_x; i++) {
                auto& cell = curline->cells[i];

                cell.pos = {};
                cell.glyph = space_glyph;
                cell.font = cur_font;
                cell.ignore = false;
            }

            curline->num_cells = orig_cursor_x;
        }

        auto cells_at_start = curline->num_cells;
        if (orig_cursor_x < curline->num_cells)
            curline->num_cells = orig_cursor_x;

        auto curlen = curline->num_cells;
        auto c = &curline->cells[curline->num_cells];
        unsigned int hb_len_done = 0;

        for (unsigned int i = 0; i < len; i++) {
            if (curline->num_cells == console_fb_inst.console_cols)
                break;

            auto clus = hb_clus(s_rem, span(info, len), i);
            bool wide = is_cluster_wide(clus);

            if (wide && curline->num_cells + 2 == console_fb_inst.console_cols)
                break;

            if (pos[i].x_advance == 0) {
                hb_len_done++;
                continue;
            }

            c->pos = pos[i];
            c->glyph = info[i].codepoint;
            c->ignore = false;

            if (curline->num_cells >= orig_cursor_x) {
                c->fg_colour = fgc;
                c->bg_colour = bgc;

                if (&font == &normal_font) {
                    if (bold && italic)
                        c->font = &bold_italic_font;
                    else if (bold)
                        c->font = &bold_font;
                    else if (italic)
                        c->font = &italic_font;
                    else
                        c->font = &normal_font;
                } else
                    c->font = &font;
            }

            c++;
            curline->num_cells++;
            hb_len_done++;

            if (wide) {
                c->ignore = true;
                c->fg_colour = fgc;
                c->bg_colour = bgc;
                c->font = &normal_font;

                c++;
                curline->num_cells++;
            }
        }

        final_cursor_x = curline->num_cells;

        if (curline->num_cells < cells_at_start)
            curline->num_cells = cells_at_start;

        curline->changed = true;

        if (hb_len_done == len)
            break;

        if (!con.auto_wrap) {
            final_cursor_x = console_fb_inst.console_cols - 1;
            break;
        }

        auto trunc = curlen + info[hb_len_done].cluster;
        char32_t* t = nullptr;
        text_attribute* a = nullptr;
        size_t left = curline->len - trunc;

        if (left > 0) {
            t = (char32_t*)malloc(left * sizeof(char32_t));
            if (!t) {
                print("malloc failed\n");
                return;
            }

            a = (text_attribute*)malloc(left * sizeof(text_attribute));
            if (!a) {
                print("malloc failed\n");
                free(t);
                return;
            }

            memcpy(t, curline->text + trunc, left * sizeof(char32_t));
            memcpy(a, curline->atts + trunc, left * sizeof(text_attribute));
        }

        auto oldline = curline;

        con.console_overflow();

        curline->text = t;
        curline->atts = a;
        curline->len = oldline->len - trunc;
        curline->changed = true;

        oldline->text = (char32_t*)realloc(oldline->text, trunc * sizeof(char32_t));
        oldline->atts = (text_attribute*)realloc(oldline->atts, trunc * sizeof(text_attribute));
        oldline->len = trunc;

        s_offset += info[hb_len_done].cluster;
        orig_cursor_x = 0;
    } while (true);

    con.cursor_x = final_cursor_x;
}

void console_fb::print_chars(span<const char32_t> s) {
    while (!s.empty()) {
        if (s[0] == '\r') {
            cursor_x = 0;
            s = span(s.data() + 1, s.size() - 1);
        } else if (s[0] == '\n') {
            curline->changed = true;
            cursor_y++;

            if (cursor_y == scroll_bottom + 1) {
                console_overflow();
                cursor_y = scroll_bottom;
            } else
                curline = lines[cursor_y];

            s = span(s.data() + 1, s.size() - 1);
        } else if (s[0] == '\b') {
            if (cursor_x > 0)
                cursor_x--;

            s = span(s.data() + 1, s.size() - 1);
        } else if (s[0] < ' ') {
            // nop
            s = span(s.data() + 1, s.size() - 1);
        } else {
            span<const char32_t> sp;
            font_info* font;
            array<char32_t, 64> combined_buf;
            size_t old_emoji_len = 0;

            if (prev_emoji_cp_len > 0 && prev_emoji_line == curline && is_emoji_continuation(s[0])) {
                // backtracking - combine with previous emoji and re-render
                old_emoji_len = prev_emoji_cp_len;
                memcpy(combined_buf.data(), prev_emoji_cps.data(),
                       old_emoji_len * sizeof(char32_t));

                auto new_count = min(s.size(), combined_buf.size() - old_emoji_len);

                memcpy(combined_buf.data() + old_emoji_len, s.data(),
                       new_count * sizeof(char32_t));

                // roll-back to previous emoji position
                cursor_x = prev_emoji_cursor_x;
                curline->num_cells = prev_emoji_num_cells;
                curline->len -= old_emoji_len;
                prev_emoji_cp_len = 0;

                sp = span(combined_buf.data(), old_emoji_len + new_count);
                sp = find_font_run(sp, font);
            } else
                sp = find_font_run(s, font);

            for (size_t i = 0; i < sp.size(); i++) {
                if (sp[i] == '\n' || sp[i] == '\b' || sp[i] < ' ') {
                    sp = sp.subspan(0, i);
                    break;
                }
            }

            // FIXME - insert mode breaks emoji ZWJs?

            if (insert_mode && sp.size() > 1)
                sp = sp.subspan(0, 1);

            if (insert_mode)
                insert_chars(1);

            // save emoji state for potential future backtracking
            if (font == &emoji_font) {
                prev_emoji_cursor_x = cursor_x;
                prev_emoji_num_cells = curline->num_cells;

                auto save_len = min(sp.size(), prev_emoji_cps.size());

                memcpy(prev_emoji_cps.data(), sp.data(), save_len * sizeof(char32_t));
                prev_emoji_cp_len = save_len;
                prev_emoji_line = curline;
            }

            print_chars2(*this, *font, sp);

            s = span(s.data() + sp.size() - old_emoji_len,
                     s.size() - sp.size() + old_emoji_len);

            if (font != &emoji_font) {
                if (!sp.empty() && is_emoji(sp.back())) {
                    prev_emoji_cps[0] = sp.back();
                    prev_emoji_cp_len = 1;
                    prev_emoji_cursor_x = cursor_x - 1;
                    prev_emoji_num_cells = curline->num_cells - 1;
                    prev_emoji_line = curline;
                } else
                    prev_emoji_cp_len = 0;
            }
        }
    }
}

static void redo_line2(span<const char32_t> s, console_line& line,
                       font_info& font, size_t text_offset) {
    // FIXME - keep HB buffer around?
    // FIXME - cap how much data we send to hb_buffer_add_utf8?

    unique_hb_buffer buf{hb_buffer_create()};

    hb_buffer_add_codepoints(buf.get(), (const hb_codepoint_t*)s.data(),
                             (int)s.size(), 0, -1);

    hb_buffer_guess_segment_properties(buf.get());

    hb_shape(font.hb_font, buf.get(), nullptr, 0);

    auto len = hb_buffer_get_length(buf.get());
    auto info = hb_buffer_get_glyph_infos(buf.get(), nullptr);
    auto pos = hb_buffer_get_glyph_positions(buf.get(), nullptr);

    auto c = &line.cells[line.num_cells];

    for (unsigned int i = 0; i < len; i++) {
        auto clus = hb_clus(s, span(info, len), i);
        bool wide = is_cluster_wide(clus);
        auto& att = line.atts[text_offset + info[i].cluster];

        c->pos = pos[i];
        c->glyph = info[i].codepoint;

        c->font = &font == &emoji_font ? &font : att.font;
        c->fg_colour = att.fg_colour;
        c->bg_colour = att.bg_colour;

        c->ignore = false;

        c++;
        line.num_cells++;

        if (wide) {
            c->ignore = true;
            c->fg_colour = att.fg_colour;
            c->bg_colour = att.bg_colour;
            c->font = &font == &emoji_font ? &font : att.font;

            c++;
            line.num_cells++;
        }
    }
}

static void redo_line(console_line& line) {
    line.num_cells = 0;

    if (line.len > 0) {
        // lay out using harfbuzz

        auto s = span(line.text, line.len);
        size_t text_offset = 0;

        while (!s.empty()) {
            font_info* font;
            auto sp = find_font_run(s, font);

            redo_line2(sp, line, *font, text_offset);

            s = span(s.data() + sp.size(), s.size() - sp.size());
            text_offset += sp.size();
        }
    }

    line.changed = true;
}

void console_fb::erase_in_line(unsigned int param) {
    unsigned int text_pos;

    // get current position in string

    if (curline->len == 0)
        text_pos = 0;
    else
        text_pos = find_text_pos(cursor_x);

    switch (param) {
        case 0: { // clear from cursor to end of line
            if (cursor_x == console_cols)
                break;

            // append to existing buffer

            if (text_pos == 0) {
                if (curline->text)
                    free(curline->text);

                if (curline->atts)
                    free(curline->atts);

                curline->text = nullptr;
                curline->atts = nullptr;
                curline->len = 0;
            } else if (text_pos < curline->len) {
                auto t = (char32_t*)malloc(text_pos * sizeof(char32_t));
                if (!t) {
                    print("malloc failed\n");
                    return;
                }

                auto a = (text_attribute*)malloc(text_pos * sizeof(text_attribute));
                if (!a) {
                    print("malloc failed\n");
                    free(t);
                    return;
                }

                if (curline->len != 0) {
                    memcpy(t, curline->text, text_pos * sizeof(char32_t));
                    memcpy(a, curline->atts, text_pos * sizeof(text_attribute));
                }

                if (curline->text)
                    free(curline->text);

                if (curline->atts)
                    free(curline->atts);

                curline->text = t;
                curline->atts = a;
                curline->len = text_pos;
            }

            curline->num_cells = min(cursor_x, curline->num_cells);

            for (unsigned int i = cursor_x; i < console_cols; i++) {
                curline->cells[i].bg_colour = !reverse_video ? bg_colour : fg_colour;
                curline->cells[i].fg_colour = !reverse_video ? fg_colour : bg_colour;
            }

            break;
        }

        case 1: { // clear from beginning of line to cursor
            auto newlen = cursor_x + (text_pos < curline->len ? curline->len - text_pos : 1);
            char32_t* t = nullptr;
            text_attribute* a = nullptr;

            font_info* def_font;
            if (bold && italic)
                def_font = &bold_italic_font;
            else if (bold)
                def_font = &bold_font;
            else if (italic)
                def_font = &italic_font;
            else
                def_font = &normal_font;

            auto def_fg = !reverse_video ? fg_colour : bg_colour;
            auto def_bg = !reverse_video ? bg_colour : fg_colour;

            if (newlen > 0) {
                t = (char32_t*)malloc(newlen * sizeof(char32_t));
                if (!t) {
                    print("malloc failed\n");
                    return;
                }

                a = (text_attribute*)malloc(newlen * sizeof(text_attribute));
                if (!a) {
                    print("malloc failed\n");
                    free(t);
                    return;
                }

                for (unsigned int i = 0; i <= cursor_x; i++) {
                    t[i] = U' ';

                    if (curline->atts && i < curline->len) {
                        a[i].font = curline->atts[i].font;
                        a[i].fg_colour = curline->atts[i].fg_colour;
                        a[i].bg_colour = curline->atts[i].bg_colour;
                    } else {
                        a[i].font = def_font;
                        a[i].fg_colour = def_fg;
                        a[i].bg_colour = def_bg;
                    }
                }

                if (text_pos < curline->len) {
                    memcpy(&t[cursor_x + 1], &curline->text[text_pos + 1],
                           (curline->len - text_pos - 1) * sizeof(char32_t));
                    memcpy(&a[cursor_x + 1], &curline->atts[text_pos + 1],
                           (curline->len - text_pos - 1) * sizeof(text_attribute));
                }
            }

            if (curline->text)
                free(curline->text);

            if (curline->atts)
                free(curline->atts);

            curline->text = t;
            curline->atts = a;
            curline->len = newlen;

            {
                auto space_glyph = FT_Get_Char_Index(def_font->face, U' ');

                for (unsigned int i = 0; i <= cursor_x; i++) {
                    curline->cells[i].pos = {};
                    curline->cells[i].glyph = space_glyph;
                    curline->cells[i].font = def_font;
                    curline->cells[i].ignore = false;
                    curline->cells[i].fg_colour = def_fg;
                    curline->cells[i].bg_colour = def_bg;
                }
            }

            curline->num_cells = max(cursor_x + 1, curline->num_cells);

            break;
        }

        case 2: { // clear entire line
            if (curline->text)
                free(curline->text);

            if (curline->atts)
                free(curline->atts);

            curline->text = nullptr;
            curline->atts = nullptr;
            curline->len = 0;
            curline->num_cells = 0;

            for (unsigned int i = 0; i < console_cols; i++) {
                curline->cells[i].bg_colour = !reverse_video ? bg_colour : fg_colour;
                curline->cells[i].fg_colour = !reverse_video ? fg_colour : bg_colour;
            }

            break;
        }
    }

    curline->changed = true;
}

void console_fb::erase_in_display(unsigned int param) {
    switch (param) {
        case 0: // clear from cursor to end of screen
            erase_in_line(0);

            for (unsigned int i = cursor_y + 1; i < console_rows; i++) {
                auto& l = *lines[i];

                if (l.text)
                    free(l.text);

                if (l.atts)
                    free(l.atts);

                l.text = nullptr;
                l.atts = nullptr;
                l.len = 0;

                redo_line(l);
            }

            break;

        case 1: // clear from beginning of screen to cursor
            erase_in_line(1);

            for (unsigned int i = 0; i < cursor_y; i++) {
                auto& l = *lines[i];

                if (l.text)
                    free(l.text);

                if (l.atts)
                    free(l.atts);

                l.text = nullptr;
                l.atts = nullptr;
                l.len = 0;

                redo_line(l);
            }

            break;

        case 2: // clear entire screen
        case 3:
            for (unsigned int i = 0; i < console_rows; i++) {
                auto& l = *lines[i];

                if (l.text)
                    free(l.text);

                if (l.atts)
                    free(l.atts);

                l.text = nullptr;
                l.atts = nullptr;
                l.len = 0;

                redo_line(l);
            }

            break;
    }
}

void console_fb::delete_chars(unsigned int num) {
    if (curline->len == 0)
        return;

    auto text_start = find_text_pos(cursor_x);
    if (text_start >= curline->len)
        return;

    auto text_end = find_text_pos(cursor_x + num);
    auto count = text_end - text_start;

    memmove(&curline->text[text_start], &curline->text[text_end],
            (curline->len - text_end) * sizeof(char32_t));
    memmove(&curline->atts[text_start], &curline->atts[text_end],
            (curline->len - text_end) * sizeof(text_attribute));

    curline->len -= count;
    redo_line(*curline);
}

void console_fb::erase_chars(unsigned int num) {
    if (cursor_x >= console_cols || num == 0)
        return;

    auto end_x = min(cursor_x + num, console_cols);

    font_info* def_font;
    if (bold && italic)
        def_font = &bold_italic_font;
    else if (bold)
        def_font = &bold_font;
    else if (italic)
        def_font = &italic_font;
    else
        def_font = &normal_font;

    auto fgc = !reverse_video ? fg_colour : bg_colour;
    auto bgc = !reverse_video ? bg_colour : fg_colour;

    auto space_glyph = FT_Get_Char_Index(def_font->face, U' ');

    for (unsigned int i = cursor_x; i < end_x; i++) {
        curline->cells[i].pos = {};
        curline->cells[i].glyph = space_glyph;
        curline->cells[i].font = def_font;
        curline->cells[i].ignore = false;
        curline->cells[i].fg_colour = fgc;
        curline->cells[i].bg_colour = bgc;
    }

    if (end_x > curline->num_cells)
        curline->num_cells = end_x;

    curline->changed = true;
}

void console_fb::insert_chars(unsigned int num) {
    if (cursor_x >= console_cols || num == 0)
        return;

    if (num > console_cols - cursor_x)
        num = console_cols - cursor_x;

    font_info* def_font;
    if (bold && italic)
        def_font = &bold_italic_font;
    else if (bold)
        def_font = &bold_font;
    else if (italic)
        def_font = &italic_font;
    else
        def_font = &normal_font;

    auto fgc = !reverse_video ? fg_colour : bg_colour;
    auto bgc = !reverse_video ? bg_colour : fg_colour;

    auto space_glyph = FT_Get_Char_Index(def_font->face, U' ');

    if (curline->num_cells > cursor_x) {
        unsigned int shift_count = min(curline->num_cells - cursor_x,
                                       console_cols - cursor_x - num);
        if (shift_count > 0) {
            memmove(&curline->cells[cursor_x + num], &curline->cells[cursor_x],
                    shift_count * sizeof(console_cell));
        }
        curline->num_cells = min(curline->num_cells + num, console_cols);
    }

    for (unsigned int i = cursor_x; i < cursor_x + num; i++) {
        curline->cells[i].pos = {};
        curline->cells[i].glyph = space_glyph;
        curline->cells[i].font = def_font;
        curline->cells[i].ignore = false;
        curline->cells[i].fg_colour = fgc;
        curline->cells[i].bg_colour = bgc;
    }

    curline->changed = true;
}

void console_fb::insert_lines(unsigned int num) {
    num = min(num, scroll_bottom - cursor_y + 1);

    for (unsigned int i = 0; i < num; i++) {
        auto* recycled = lines[scroll_bottom];
        memmove(&lines[cursor_y + 1], &lines[cursor_y],
                (scroll_bottom - cursor_y) * sizeof(console_line*));
        lines[cursor_y] = recycled;
        reset_line(*recycled);
    }

    for (unsigned int i = cursor_y; i <= scroll_bottom; i++)
        lines[i]->changed = true;

    curline = lines[cursor_y];
}

void console_fb::delete_lines(unsigned int num) {
    num = min(num, scroll_bottom - cursor_y + 1);

    for (unsigned int i = 0; i < num; i++) {
        auto* recycled = lines[cursor_y];
        memmove(&lines[cursor_y], &lines[cursor_y + 1],
                (scroll_bottom - cursor_y) * sizeof(console_line*));
        lines[scroll_bottom] = recycled;
        reset_line(*recycled);
    }

    for (unsigned int i = cursor_y; i <= scroll_bottom; i++)
        lines[i]->changed = true;

    curline = lines[cursor_y];
}

void console_fb::enter_alt_screen() {
    if (alt_screen_active)
        return;

    normal_cursor_x = cursor_x;
    normal_cursor_y = cursor_y;
    normal_scroll_top = scroll_top;
    normal_scroll_bottom = scroll_bottom;

    lines = alt_lines;

    for (unsigned int i = 0; i < console_rows; i++) {
        reset_line(*lines[i]);
    }

    cursor_x = 0;
    cursor_y = 0;
    scroll_top = 0;
    scroll_bottom = console_rows - 1;

    curline = lines[0];
    alt_screen_active = true;
}

void console_fb::exit_alt_screen() {
    if (!alt_screen_active)
        return;

    cursor_x = normal_cursor_x;
    cursor_y = normal_cursor_y;
    scroll_top = normal_scroll_top;
    scroll_bottom = normal_scroll_bottom;

    lines = normal_lines;

    for (unsigned int i = 0; i < console_rows; i++) {
        lines[i]->changed = true;
    }

    curline = lines[cursor_y];
    alt_screen_active = false;
}

void console_fb::write_input_data(string_view s) {
    IO_STATUS_BLOCK iosb;

    if (server_handle == 0)
        return;

    while (!s.empty()) {
        auto status = NtWriteFile(server_handle, 0, nullptr, nullptr, &iosb,
                                  s.data(), (uint32_t)s.size(), nullptr,
                                  nullptr);
        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status)) {
            print_stderr("NtWriteFile returned {}\n", status);
            return;
        }

        if (iosb.Information >= s.size())
            break;

        s = string_view(s.data() + iosb.Information,
                        s.size() - iosb.Information);
    }
}

static void recv_data_fb(string_view s) {
    do_print(console_fb_inst, s);

    // print to FB

    for (unsigned int y = 0; y < console_fb_inst.console_rows; y++) {
        auto& l = *lines[y];

        if (l.changed) {
            print_line(l, y);
            l.changed = false;
        }
    }
}

static constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

static size_t calc_environment_size(const char16_t* env) {
    size_t ret = 0;

    while (*env) {
        auto sv = u16string_view(env);

        ret += sv.size() * sizeof(char16_t);
        ret += sizeof(char16_t);

        env += sv.size() + 1;
    }

    ret += sizeof(char16_t);

    return ret;
}

static nt_status create_process(u16string_view exe, u16string_view cmdline,
                                const char16_t* environment, optional<HANDLE> orig_stdin,
                                optional<HANDLE> orig_stdout, optional<HANDLE> orig_stderr,
                                bool create_suspended, optional<u16string_view> curdir,
                                unique_handle& proc, unique_handle& thread) {
    HANDLE h, stdinh = 0, stdouth, stderrh, curdirh = 0;
    unique_handle file, sect, curdiruh;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    SECTION_IMAGE_INFORMATION sii;
    PROCESS_BASIC_INFORMATION pbi;
    RTL_USER_PROCESS_PARAMETERS params;
    PEB newpeb;
    INITIAL_TEB initial_teb;
    CONTEXT context;
    CLIENT_ID client_id;
    nt_status status;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    // open file

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    us.Length = us.MaximumLength = (uint16_t)(exe.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)exe.data();

    status = NtOpenFile(&h, FILE_READ_DATA, &oa, &iosb, 0,
                        FILE_NON_DIRECTORY_FILE);
    if (!success(status))
        return status;

    file.reset(h);

    // create section

    status = NtCreateSection(&h, 0, nullptr, nullptr, PAGE_READONLY, SEC_IMAGE,
                             file.get());
    if (!success(status))
        return status;

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::ImageInformation,
                            &sii, sizeof(SECTION_IMAGE_INFORMATION), nullptr);
    if (!success(status))
        return status;

    // create process

    status = NtCreateProcess(&h, MAXIMUM_ALLOWED, nullptr, NtCurrentProcess(),
                             false, sect.get(), 0, 0);
    if (!success(status))
        return status;

    proc.reset(h);

    // duplicate handles

    if (!orig_stdin.has_value())
        orig_stdin = peb->ProcessParameters->StandardInput;

    if (!orig_stdout.has_value())
        orig_stdout = peb->ProcessParameters->StandardOutput;

    if (!orig_stderr.has_value())
        orig_stderr = peb->ProcessParameters->StandardError;

    status = NtDuplicateObject(NtCurrentProcess(), *orig_stdin,
                               proc.get(), &stdinh, FILE_READ_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return status;

    status = NtDuplicateObject(NtCurrentProcess(), *orig_stdout,
                               proc.get(), &stdouth, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return status;

    status = NtDuplicateObject(NtCurrentProcess(), *orig_stderr,
                               proc.get(), &stderrh, FILE_WRITE_DATA, 0,
                               DUPLICATE_SAME_ATTRIBUTES);
    if (!success(status))
        return status;

    if (curdir.has_value()) {
        us.Length = us.MaximumLength = (uint16_t)(curdir->size() * sizeof(char16_t));
        us.Buffer = (char16_t*)curdir->data();

        status = NtCreateFile(&h, FILE_TRAVERSE, &oa, &iosb, nullptr,
                              FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                              FILE_DIRECTORY_FILE, nullptr, 0);
        if (!success(status))
            return status;

        curdiruh.reset(h);

        status = NtDuplicateObject(NtCurrentProcess(), curdiruh.get(),
                                   proc.get(), &curdirh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
        if (!success(status))
            return status;
    } else {
        status = NtDuplicateObject(NtCurrentProcess(), peb->ProcessParameters->CurrentDirectory.Handle,
                                   proc.get(), &curdirh, 0, 0,
                                   DUPLICATE_SAME_ATTRIBUTES | DUPLICATE_SAME_ACCESS);
        if (!success(status))
            return status;
    }

    // take copy of PEB

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        return status;

    status = NtReadVirtualMemory(proc.get(), pbi.PebBaseAddress, &newpeb,
                                 sizeof(PEB), nullptr);
    if (!success(status))
        return status;

    // take copy of ProcessParameters

    status = NtReadVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                 sizeof(params), nullptr);
    if (!success(status))
        return status;

    // change handles and set params

    params.StandardInput = stdinh;
    params.StandardOutput = stdouth;
    params.StandardError = stderrh;
    params.CurrentDirectory.Handle = curdirh;
    params.StartingX = 0;
    params.StartingY = 0;
    params.CountX = 0;
    params.CountY = 0;
    params.CountCharsX = 0;
    params.CountCharsY = 0;
    params.FillAttribute = 0;
    params.WindowFlags = 0;
    params.ShowWindowFlags = 0;

    if (curdir.has_value()) {
        FILE_NAME_INFORMATION fni;

        status = NtQueryInformationFile(curdiruh.get(), &iosb, &fni, sizeof(fni),
                                        FILE_INFORMATION_CLASS::NameInformation);
        if (!success(status) && status != nt_status::BUFFER_OVERFLOW)
            return status;

        size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

        unique_ptr<uint8_t[]> buf;

        buf.reset(new uint8_t[len]);
        if (!buf) {
            print_stderr("new failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        auto& fni2 = *(FILE_NAME_INFORMATION*)buf.get();

        status = NtQueryInformationFile(curdiruh.get(), &iosb, &fni2, (uint32_t)len,
                                        FILE_INFORMATION_CLASS::NameInformation);
        if (!success(status))
            return status;

        auto sv = u16string_view(fni2.FileName, fni2.FileNameLength / sizeof(char16_t));

        static const u16string_view prefix = u"C:";

        for (size_t i = 0; i < sv.size(); i++) {
            if (fni2.FileName[i] == u'/')
                fni2.FileName[i] = u'\\';
        }

        status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer,
                                      prefix.data(), (uint32_t)(prefix.size() * sizeof(char16_t)),
                                      nullptr);
        if (!success(status))
            return status;

        status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer + prefix.size(),
                                      fni2.FileName,
                                      (uint32_t)min(sv.size() * sizeof(char16_t), (size_t)params.CurrentDirectory.DosPath.MaximumLength - (prefix.size() * sizeof(char16_t))),
                                      nullptr);
        if (!success(status))
            return status;

        params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength, (uint16_t)((prefix.size() * sizeof(char16_t)) + fni2.FileNameLength));
    } else {
        mn_peb_lock lock;
        lock_guard lg(lock);

        // copy current directory string

        params.CurrentDirectory.DosPath.Length = min(params.CurrentDirectory.DosPath.MaximumLength,
                                                     peb->ProcessParameters->CurrentDirectory.DosPath.Length);

        status = NtWriteVirtualMemory(proc.get(), params.CurrentDirectory.DosPath.Buffer,
                                      peb->ProcessParameters->CurrentDirectory.DosPath.Buffer,
                                      params.CurrentDirectory.DosPath.Length, nullptr);
        if (!success(status))
            return status;
    }

    // allocate misc block

    size_t misc_block_size = (cmdline.size() + 1) * sizeof(char16_t);
    unique_ptr<uint8_t[]> misc_block;
    void* proc_misc_block = nullptr;

    {
        mn_peb_lock lock;
        lock_guard lg(lock);

        size_t environment_size;

        if (environment)
            environment_size = calc_environment_size(environment);
        else
            environment_size = peb->ProcessParameters->EnvironmentSize;

        misc_block_size += environment_size;

        misc_block.reset(new uint8_t[misc_block_size]);
        if (!misc_block) {
            print_stderr("new failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        // allocate misc block in process

        size_t proc_misc_block_size = bytes_to_pages(misc_block_size) << PAGE_SHIFT;

        status = NtAllocateVirtualMemory(proc.get(), &proc_misc_block, 0,
                                         &proc_misc_block_size,
                                         MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!success(status))
            return status;

        // copy cmdline

        auto ptr = misc_block.get();

        memcpy(misc_block.get(), cmdline.data(), cmdline.size() * sizeof(char16_t));
        ptr += cmdline.size() * sizeof(char16_t);

        *ptr = 0;
        ptr++;

        params.CommandLine.Length = (uint16_t)(cmdline.size() * sizeof(char16_t));
        params.CommandLine.MaximumLength = (uint16_t)((cmdline.size() + 1) * sizeof(char16_t));
        params.CommandLine.Buffer = (char16_t*)proc_misc_block;

        if (environment)
            memcpy(ptr, environment, environment_size);
        else {
            memcpy(ptr, peb->ProcessParameters->Environment,
                   peb->ProcessParameters->EnvironmentSize);
        }

        params.Environment = (uint8_t*)proc_misc_block + (ptr - misc_block.get());
        params.EnvironmentSize = environment_size;
    }

    // copy misc block

    status = NtWriteVirtualMemory(proc.get(), proc_misc_block, misc_block.get(),
                                  (uint32_t)misc_block_size, nullptr);
    if (!success(status))
        return status;

    // write ProcessParameters back

    status = NtWriteVirtualMemory(proc.get(), newpeb.ProcessParameters, &params,
                                  sizeof(params), nullptr);
    if (!success(status))
        return status;

    // create thread

    memset(&initial_teb, 0, sizeof(initial_teb));

    void* stack = nullptr;
    auto stack_size = max((size_t)0x100000, sii.MaximumStackSize);

    status = NtAllocateVirtualMemory(proc.get(), &stack, 0, &stack_size,
                                     MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!success(status))
        return status;

    initial_teb.OldStackBase = nullptr;
    initial_teb.OldStackLimit = nullptr;
    initial_teb.StackBase = (uint8_t*)stack + stack_size;
    initial_teb.StackLimit = (uint8_t*)stack + stack_size;
    initial_teb.StackAllocationBase = stack;

    memset(&context, 0, sizeof(context));
#ifdef __aarch64__
    context.Pc = (uintptr_t)sii.TransferAddress;
    context.Sp = (uintptr_t)initial_teb.StackBase - 0x10;
#elifdef __x86_64__
    context.Rip = (uintptr_t)sii.TransferAddress;
    context.Rsp = (uintptr_t)initial_teb.StackBase - 0x28; // 0x20 bytes for MS ABI, 0x8 for return pointer
    context.EFlags = 0x200;
#endif
    context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

    status = NtCreateThread(&h, THREAD_ALL_ACCESS, nullptr, proc.get(),
                            &client_id, &context, &initial_teb, true);
    if (!success(status))
        return status;

    thread.reset(h);

    if (!create_suspended) {
        status = NtResumeThread(thread.get(), nullptr);
        if (!success(status))
            return status;
    }

    return nt_status::SUCCESS;
}

static void handle_ctrl_alt_delete(HANDLE client_handle) {
    nt_status status;
    unique_handle proc, thread;

    status = create_process(u"/bin/reboot.exe", u"reboot.exe", nullptr,
                            client_handle, client_handle, client_handle,
                            false, nullopt, proc, thread);
    if (!success(status))
        print_stderr("create_process returned {} for reboot.exe\n", status);
}

static string_view get_os_version() {
    const auto& time_page = *(time_page_struct*)TIME_PAGE_BASE;

    auto sv = string_view(time_page.os_version);

    if (auto sp = sv.find(' '); sp != string_view::npos)
        sv = string_view(sv.data() + sp + 1, sv.size() - sp - 1);

    return sv;
}

static void run_shell(const char16_t* exe, void (*recv_func)(string_view),
                      void (*flush_func)(), bool& decckm, uint16_t cols,
                      uint16_t rows, HANDLE& console_server_handle) {
    HANDLE h;
    unique_handle server_handle, client_handle, proc, kmsg, ptyctl, keyboard_pipe;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY_IN ptyparams;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY_OUT ptyinfo;

    auto kmsg_recv_func = [recv_func](string_view msg) {
        while (!msg.empty()) {
            auto nl = msg.find('\n');

            if (nl == string_view::npos) {
                recv_func(msg);
                break;
            }

            if (nl != 0) {
                recv_func(string_view(msg.data(), nl));
                msg = string_view(msg.data() + nl, msg.size() - nl);
            }

            recv_func("\r\n");
            msg = string_view(msg.data() + 1, msg.size() - 1);
        }
    };

    unique_xkb_context xkb{xkb_context_new(XKB_CONTEXT_NO_FLAGS)};
    if (!xkb) {
        print_stderr("xkb_context_new failed\n");
        return;
    }

    xkb_rule_names xkb_rules = {
        .rules = "evdev",
        .model = nullptr,
        .layout = "gb", // FIXME - allow setting
        .variant = nullptr,
        .options = nullptr
    };

    unique_xkb_keymap keymap{xkb_keymap_new_from_names(xkb.get(), &xkb_rules,
                                                       XKB_KEYMAP_COMPILE_NO_FLAGS)};
    if (!keymap) {
        print_stderr("xkb_keymap_new_from_names failed\n");
        return;
    }

    // open kmsg

    static const u16string_view kmsg_fn = u"\\dev\\kmsg";
    static const u16string_view ptyctl_fn = u"\\dev\\ptyctl";
    static const u16string_view keyboard_pipe_fn = u"\\dev\\keyboard";

    us.Length = us.MaximumLength = (uint16_t)(kmsg_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)kmsg_fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return;
    }

    kmsg.reset(h);

    // open ptyctl

    us.Length = us.MaximumLength = (uint16_t)(ptyctl_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)ptyctl_fn.data();

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return;
    }

    ptyctl.reset(h);

    // create pty

    ptyparams.cols = cols;
    ptyparams.rows = rows;

    status = NtDeviceIoControlFile(ptyctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTYCTL_CREATE_PTY, &ptyparams,
                                   sizeof(ptyparams), &ptyinfo, sizeof(ptyinfo));
    if (!success(status)) {
        print("IOCTL_MOXNOX_PTYCTL_CREATE_PTY returned {}\n", status);
        return;
    }

    ptyctl.reset();

    server_handle.reset(ptyinfo.server_handle);
    client_handle.reset(ptyinfo.client_handle);

    // open keyboard pipe

    us.Length = us.MaximumLength = (uint16_t)(keyboard_pipe_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)keyboard_pipe_fn.data();

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return;
    }

    keyboard_pipe.reset(h);

    // print pending kmsg buffer first

    while (true) {
        LARGE_INTEGER timeout;
        char s[PAGE_SIZE];

        timeout.QuadPart = 0;

        status = NtWaitForSingleObject(kmsg.get(), true, &timeout);
        if (status == nt_status::ALERTED)
            continue;
        else if (status == nt_status::TIMEOUT)
            break;
        else if (!success(status)) {
            print("NtWaitForSingleObject returned {}\n", status);
            return;
        }

        status = NtReadFile(kmsg.get(), 0, nullptr, nullptr,
                            &iosb, s, sizeof(s), nullptr, nullptr);
        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status)) {
            print("NtReadFile returned {}\n", status);
            return;
        }

        if (iosb.Information != 0)
            kmsg_recv_func(string_view(s, iosb.Information));
    }

    recv_func(welcome_message1);
    recv_func(get_os_version());
    recv_func(welcome_message2);
    flush_func();

    auto do_run_shell = [&exe, &proc, &client_handle]() -> bool {
        unique_ptr<char16_t[]> u16home;
        size_t u16homelen;
        unique_handle thread;
        nt_status status;

        auto home = getenv("HOME");
        if (home && home[0]) {
            u16homelen = utf8_to_utf16_len(home);

            u16home.reset(new char16_t[u16homelen]);
            if (!u16home) {
                print_stderr("new failed\n");
                return false;
            }

            utf8_to_utf16(span(u16home.get(), u16homelen), home);
        }

        status = create_process(exe, exe, nullptr, client_handle.get(),
                                client_handle.get(), client_handle.get(), true,
                                u16home ? u16string_view(u16home.get(), u16homelen) : optional<u16string_view>{nullopt},
                                proc, thread);
        if (!success(status)) {
            print_stderr("create_process returned {}\n", status);
            return false;
        }

        status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxNewSession,
                                         nullptr, 0);
        if (!success(status)) {
            print_stderr("NtSetInformationProcess returned {} for MoxnoxNewSession\n", status);
            NtTerminateProcess(proc.get(), status);
            return false;
        }

        HANDLE pty = client_handle.get();

        status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxSetControllingTerminal,
                                         &pty, sizeof(pty));
        if (!success(status)) {
            print_stderr("NtSetInformationProcess returned {} for MoxnoxSetControllingTerminal\n", status);
            NtTerminateProcess(proc.get(), status);
            return false;
        }

        status = NtResumeThread(thread.get(), nullptr);
        if (!success(status)) {
            print_stderr("NtResumeThread returned {}\n", status);
            NtTerminateProcess(proc.get(), status);
            return false;
        }

        return true;
    };

    if (!do_run_shell())
        return;

    console_server_handle = server_handle.get();

    do {
        array<HANDLE, 4> objs;

        objs[0] = kmsg.get();
        objs[1] = keyboard_pipe.get();
        objs[2] = server_handle.get();
        objs[3] = proc.get();

        status = NtWaitForMultipleObjects(objs.size(), objs.data(), true,
                                          true, nullptr);
        if (status == nt_status::ALERTED)
            continue;

        if (!success(status)) {
            print("NtWaitForMultipleObjects returned {}\n", status);
            return;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: {
                char s[PAGE_SIZE];

                // print kmsg data

                status = NtReadFile(kmsg.get(), 0, nullptr, nullptr,
                                    &iosb, s, sizeof(s), nullptr, nullptr);
                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    return;
                }

                if (iosb.Information != 0) {
                    kmsg_recv_func(string_view(s, iosb.Information));
                    flush_func();
                }

                break;
            }

            case 1: {
                input_event_with_id ev;
                bool cad = false;

                status = NtReadFile(keyboard_pipe.get(), 0, nullptr, nullptr,
                                    &iosb, &ev, sizeof(ev), nullptr, nullptr);

                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    return;
                }

                span<const char8_t> sp;
                char8_t k[64];

                if (ev.ev.type == evdev_type::EV_KEY) {
                    sp = handle_keyboard_event(ev, keymap.get(), keyboard_pipe.get(),
                                               cad, decckm, k);
                }

                if (cad)
                    handle_ctrl_alt_delete(client_handle.get());

                if (!sp.empty()) {
                    IO_STATUS_BLOCK iosb;

                    while (!sp.empty()) {
                        auto status = NtWriteFile(server_handle.get(), 0, nullptr,
                                                  nullptr, &iosb, sp.data(),
                                                  (uint32_t)sp.size(), nullptr,
                                                  nullptr);
                        if (status == nt_status::ALERTED)
                            continue;
                        else if (!success(status)) {
                            print("NtWriteFile returned {}\n", status);
                            return;
                        }

                        if (iosb.Information >= sp.size())
                            break;

                        sp = span(sp.data() + iosb.Information,
                                  sp.size() - iosb.Information);
                    }
                }

                break;
            }

            case 2: {
                do {
                    char s[PAGE_SIZE];
                    LARGE_INTEGER timeout;

                    // print pipe data

                    status = NtReadFile(server_handle.get(), 0, nullptr, nullptr,
                                        &iosb, s, sizeof(s), nullptr, nullptr);
                    if (status == nt_status::ALERTED)
                        break;
                    else if (!success(status)) {
                        print("NtReadFile returned {}\n", status);
                        return;
                    }

                    recv_func(string_view(s, iosb.Information));

                    timeout.QuadPart = 0;

                    // go again if still data pending
                    status = NtWaitForSingleObject(server_handle.get(), true,
                                                   &timeout);
                    if (status == nt_status::ALERTED || status == nt_status::TIMEOUT)
                        break;
                    else if (!success(status)) {
                        print("NtWaitForSingleObject returned {}\n", status);
                        return;
                    }
                } while (false);

                flush_func();

                if (status == nt_status::ALERTED)
                    continue;

                break;
            }

            case 3: {
                // process terminated, start again

                if (!do_run_shell())
                    return;

                break;
            }
        }
    } while (true);

    console_server_handle = 0;
}

#ifdef __x86_64__
static int try_text_console(const char16_t* shell) {
    text_console = init_text_console();
    if (text_console.empty()) {
        print("init_text_console failed\n");
        return 1;
    }

    run_shell(shell, recv_data_text, flush_text_console,
              console_text_inst.decckm, (uint16_t)console_text_inst.console_cols,
              (uint16_t)console_text_inst.console_rows,
              console_text_inst.server_handle);

    return 0;
}
#endif

static int do_console(const char16_t* shell) {
    bool no_framebuffer = false;

    setenv("TERM", "xterm-16color", 1);
    setenv("LANG", "C.UTF-8", 1);

    framebuffer = init_framebuffer(no_framebuffer);
#ifdef __x86_64__
    if (no_framebuffer)
        return try_text_console(shell);
    else if (framebuffer.empty()) {
        print("init_framebuffer failed\n");
        return 1;
    }
#else
    if (framebuffer.empty()) {
        print("init_framebuffer failed\n");
        return 1;
    }
#endif

    auto error = FT_Init_FreeType(&ft); // FIXME - free at end
    if (error) {
        print("FT_Init_FreeType failed ({}, {})\n", error, FT_Error_String(error));
        return 1;
    }

    auto fontbuf = load_font(u"/share/fonts/console.ttf");
    if (fontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    auto boldfontbuf = load_font(u"/share/fonts/console-bold.ttf");
    if (boldfontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    auto italicfontbuf = load_font(u"/share/fonts/console-italic.ttf");
    if (italicfontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    auto bolditalicfontbuf = load_font(u"/share/fonts/console-bold-italic.ttf");
    if (italicfontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    auto emojifontbuf = load_font(u"/share/fonts/emoji.ttf");
    if (emojifontbuf.empty()) {
        print("load_font failed\n");
        return 1;
    }

    if (!init_font(fontbuf, normal_font, true)) {
        print("init_font failed\n");
        return 1;
    }

    if (!init_font(boldfontbuf, bold_font, false)) {
        print("init_font failed\n");
        return 1;
    }

    if (!init_font(italicfontbuf, italic_font, false)) {
        print("init_font failed\n");
        return 1;
    }

    if (!init_font(bolditalicfontbuf, bold_italic_font, false)) {
        print("init_font failed\n");
        return 1;
    }

    if (!init_font(emojifontbuf, emoji_font, false)) {
        print("init_font failed\n");
        return 1;
    }

    rect(0, 0, fb_info.width, fb_info.height, 0x000000);
    flush_framebuffer();

    run_shell(shell, recv_data_fb, flush_framebuffer,
              console_fb_inst.decckm, (uint16_t)console_fb_inst.console_cols,
              (uint16_t)console_fb_inst.console_rows,
              console_fb_inst.server_handle);

    return 0;
}

static void serial_print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    if (s.size() == 0)
        return;

    while (true) {
        auto status = NtWriteFile(peb->ProcessParameters->StandardOutput, 0,
                                  nullptr, nullptr, &iosb, s.data(),
                                  (uint32_t)s.size(), nullptr, nullptr);
        if (!success(status)) {
            print("NtWriteFile returned {}\n", status);
            return;
        }

        if (iosb.Information == s.size())
            break;

        s = string_view(s.data() + iosb.Information, s.size() - iosb.Information);
    }
}

static int do_serial_console(const char16_t* exe) {
    HANDLE h;
    unique_handle server_handle, client_handle, proc, ptyctl;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY_IN ptyparams;
    MOXNOX_IOCTL_PTYCTL_CREATE_PTY_OUT ptyinfo;

    static const u16string_view ptyctl_fn = u"\\dev\\ptyctl";

    setenv("TERM", "xterm-16color", 1);
    setenv("LANG", "C.UTF-8", 1);

    // open ptyctl

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(ptyctl_fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)ptyctl_fn.data();

    status = NtOpenFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, 0, 0);
    if (!success(status)) {
        print("NtOpenFile returned {}\n", status);
        return 1;
    }

    ptyctl.reset(h);

    ptyparams.cols = 80;
    ptyparams.rows = 25;

    status = NtDeviceIoControlFile(ptyctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_PTYCTL_CREATE_PTY, &ptyparams,
                                   sizeof(ptyparams), &ptyinfo, sizeof(ptyinfo));
    if (!success(status)) {
        print("IOCTL_MOXNOX_PTYCTL_CREATE_PTY returned {}\n", status);
        return 1;
    }

    ptyctl.reset();

    server_handle.reset(ptyinfo.server_handle);
    client_handle.reset(ptyinfo.client_handle);

    serial_print(welcome_message1);
    serial_print(get_os_version());
    serial_print(welcome_message2);

    auto do_run_shell = [&proc, &client_handle, &exe]() -> bool {
        unique_ptr<char16_t[]> u16home;
        size_t u16homelen;
        unique_handle thread;
        nt_status status;

        auto home = getenv("HOME");
        if (home && home[0]) {
            u16homelen = utf8_to_utf16_len(home);

            u16home.reset(new char16_t[u16homelen]);
            if (!u16home) {
                print_stderr("new failed\n");
                return false;
            }

            utf8_to_utf16(span(u16home.get(), u16homelen), home);
        }

        status = create_process(exe, exe, nullptr, client_handle.get(),
                                client_handle.get(), client_handle.get(), true,
                                u16home ? u16string_view(u16home.get(), u16homelen) : optional<u16string_view>{nullopt},
                                proc, thread);
        if (!success(status)) {
            print_stderr("create_process returned {}\n", status);
            return false;
        }

        status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxNewSession,
                                         nullptr, 0);
        if (!success(status)) {
            print_stderr("NtSetInformationProcess returned {} for MoxnoxNewSession\n", status);
            NtTerminateProcess(proc.get(), status);
            return false;
        }

        HANDLE pty = client_handle.get();

        status = NtSetInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::MoxnoxSetControllingTerminal,
                                         &pty, sizeof(pty));
        if (!success(status)) {
            print_stderr("NtSetInformationProcess returned {} for MoxnoxSetControllingTerminal\n", status);
            NtTerminateProcess(proc.get(), status);
            return false;
        }

        status = NtResumeThread(thread.get(), nullptr);
        if (!success(status)) {
            print_stderr("NtResumeThread returned {}\n", status);
            NtTerminateProcess(proc.get(), status);
            return false;
        }

        return true;
    };

    if (!do_run_shell())
        return 1;

    do {
        array<HANDLE, 3> objs;

        objs[0] = peb->ProcessParameters->StandardInput;
        objs[1] = server_handle.get();
        objs[2] = proc.get();

        status = NtWaitForMultipleObjects(objs.size(), objs.data(), true,
                                          true, nullptr);
        if (status == nt_status::ALERTED)
            continue;

        if (!success(status)) {
            print("NtWaitForMultipleObjects returned {}\n", status);
            return 1;
        }

        switch ((unsigned int)status - (unsigned int)nt_status::SUCCESS) {
            case 0: {
                char buf[1024];

                // forward data on stdin

                status = NtReadFile(peb->ProcessParameters->StandardInput, 0,
                                    nullptr, nullptr, &iosb, buf, sizeof(buf),
                                    nullptr, nullptr);

                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    return 1;
                }

                span read(buf, iosb.Information);

                auto do_write = [&server_handle](span<const char> buf) {
                    IO_STATUS_BLOCK iosb;

                    while (!buf.empty()) {
                        auto status = NtWriteFile(server_handle.get(), 0, nullptr,
                                                  nullptr, &iosb, buf.data(),
                                                  (uint32_t)buf.size(),
                                                  nullptr, nullptr);
                        if (status == nt_status::ALERTED)
                            continue;
                        else if (!success(status)) {
                            print("NtWriteFile returned {}\n", status);
                            return;
                        }

                        if (iosb.Information >= buf.size())
                            break;

                        buf = string_view(buf.data() + iosb.Information,
                                          buf.size() - iosb.Information);
                    }
                };

                do_write(read);

                break;
            }

            case 1: {
                char s[PAGE_SIZE];

                // print pipe data

                status = NtReadFile(server_handle.get(), 0, nullptr, nullptr,
                                    &iosb, s, sizeof(s), nullptr, nullptr);
                if (status == nt_status::ALERTED)
                    continue;
                else if (!success(status)) {
                    print("NtReadFile returned {}\n", status);
                    return 1;
                }

                serial_print(string_view(s, iosb.Information));
                break;
            }

            case 2: {
                // process terminated, start again

                if (!do_run_shell())
                    return 1;

                break;
            }
        }
    } while (true);

    return 0;
}

int main(int argc, char** argv) {
    struct passwd pwd, *pwdres;
    char pwdbuf[1024];
    bool serial_mode = false;
    int ret;

    for (int i = 1; i < argc; i++) {
        auto arg = string_view(argv[i]);

        if (arg == "serial")
            serial_mode = true;
        else {
            print("unhandled command line option \"{}\"\n", arg);
            return 1;
        }
    }

    if (getpwuid_r(0, &pwd, pwdbuf, sizeof(pwdbuf), &pwdres) != 0 || !pwdres) {
        print_stderr("getpwuid_r failed for root\n");
        return 1;
    }

    if (!pwdres->pw_shell[0]) {
        print_stderr("shell not set for root\n");
        return 1;
    }

    auto u16shelllen = utf8_to_utf16_len(pwdres->pw_shell);

    auto shell = (char16_t*)malloc((u16shelllen + 1) * sizeof(char16_t));
    if (!shell) {
        print_stderr("malloc failed\n");
        return 1;
    }

    utf8_to_utf16(span(shell, u16shelllen), pwdres->pw_shell);
    shell[u16shelllen] = 0;

    if (serial_mode)
        ret = do_serial_console(shell);
    else
        ret = do_console(shell);

    free(shell);

    return ret;
}
