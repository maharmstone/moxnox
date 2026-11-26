// SPDX-License-Identifier: GPL-2.0-only
#include <atomic>
#include <string_view>
#include <span>
#include <mutex>
#include <optional>
#include <algorithm>
#include <string.h>

import nt;
import print;
import format;
import xhci;
import usb;
import evdev_device;
import pipe;
import evdev;
import timer;
import module_init;
import thread;
import mem;
import scheduler;
import list;
import spinlock;
import handle;
import object_ref;
import config;
import moxnox_ioctls;
import chardev;
import rwsem;
import vec;

using namespace std;

static const int64_t REPEAT_DELAY_MS = 660;
static const int64_t REPEAT_RATE_HZ = 25;

static const uint16_t USAGE_PAGE_GENERIC = 0x1;
static const uint16_t USAGE_PAGE_KEYBOARD = 0x7;
static const uint16_t USAGE_PAGE_LEDS = 0x8;
static const uint16_t USAGE_PAGE_BUTTONS = 0x9;
static const uint16_t USAGE_PAGE_CONSUMER = 0xc;

static const uint32_t LED_USAGE_NUM_LOCK = 1;
static const uint32_t LED_USAGE_CAPS_LOCK = 2;
static const uint32_t LED_USAGE_SCROLL_LOCK = 3;

static const uint32_t GENERIC_USAGE_X = 0x30;
static const uint32_t GENERIC_USAGE_Y = 0x31;
static const uint32_t GENERIC_USAGE_WHEEL = 0x38;

static const uint32_t CONSUMER_USAGE_AC_PAN = 0x238;

static const size_t evdev_buffer_pages = 1;

static atomic<unsigned int> hid_num = 0;

struct usb_hid_field {
    ~usb_hid_field() {
        if (usages)
            free(usages);
    }

    list_entry le;
    bool is_variable;
    bool is_relative;
    unsigned int count;
    unsigned int size;
    int32_t logical_min;
    int32_t logical_max;
    uint16_t usage_page;
    size_t num_usages = 0;
    uint32_t* usages = nullptr;
    uint32_t usage_min;
    uint32_t usage_max;
    unsigned int bit_offset;
};

struct usb_hid_report {
    ~usb_hid_report() {
        if (old)
            free(old);
    }

    list_entry le;
    uint8_t id;
    unsigned int bit_offset = 0;
    list_entry fields = { &fields, &fields };
    void* old = nullptr;
};

class usb_hid_device : public evdev_device {
public:
    usb_hid_device(usb_device& d, const usb_device_config& config,
                   xhci_queue& queue, uint16_t packet_size,
                   const usb_interface& interface);
    ~usb_hid_device();
    void set_led_field(usb_hid_report& r, usb_hid_field& f, evdev_led_code led,
                       bool on);
    void recv_thread();

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written,
                    ACCESS_MASK access_mask) override;

    bool init = false;

private:
    void post_evdev(evdev_type type, evdev_code code, int32_t value,
                    bool add_syn);
    void keyboard_repeat_thread();
    bool get_report_descriptor(const usb_device_config& cfg);
    bool parse_report_descriptor(span<const uint8_t> r);
    usb_hid_report* find_report(uint8_t id, bool input);
    void handle_input(usb_hid_report& r, span<const uint8_t> sp);
    void handle_input_field(usb_hid_field& f, span<const uint8_t> sp);
    void input_usage(uint16_t usage_page, uint32_t usage, int32_t num,
                     bool is_relative);
    nt_status ioctl_get_absinfo(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                                uintptr_t& written);

    usb_device& d;
    xhci_queue& queue;
    uint16_t packet_size;
    const usb_interface& interface;
    evdev_context* kc = nullptr;
    enum evdev_key_code repeat_key = evdev_key_code::KEY_RESERVED;
    object_ref repeat_timer;
    uint8_t* report_descriptor = nullptr;
    size_t report_descriptor_len;
    bool is_keyboard = false;
    list_entry input_reports = { &input_reports, &input_reports };
    list_entry output_reports = { &output_reports, &output_reports };
    array<uint64_t, 4> keys;
    array<uint64_t, 11> media_keys;
    uint8_t buttons;
    optional<int32_t> relative_x, relative_y, relative_wheel, relative_hwheel;
    optional<int32_t> absolute_x, absolute_y, absolute_wheel;
};

static const evdev_key_code keycodes[] = {
    // 00
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_A,
    evdev_key_code::KEY_B,
    evdev_key_code::KEY_C,
    evdev_key_code::KEY_D,
    evdev_key_code::KEY_E,
    evdev_key_code::KEY_F,
    evdev_key_code::KEY_G,
    evdev_key_code::KEY_H,
    evdev_key_code::KEY_I,
    evdev_key_code::KEY_J,
    evdev_key_code::KEY_K,
    evdev_key_code::KEY_L,
    // 10
    evdev_key_code::KEY_M,
    evdev_key_code::KEY_N,
    evdev_key_code::KEY_O,
    evdev_key_code::KEY_P,
    evdev_key_code::KEY_Q,
    evdev_key_code::KEY_R,
    evdev_key_code::KEY_S,
    evdev_key_code::KEY_T,
    evdev_key_code::KEY_U,
    evdev_key_code::KEY_V,
    evdev_key_code::KEY_W,
    evdev_key_code::KEY_X,
    evdev_key_code::KEY_Y,
    evdev_key_code::KEY_Z,
    evdev_key_code::KEY_1,
    evdev_key_code::KEY_2,
    // 20
    evdev_key_code::KEY_3,
    evdev_key_code::KEY_4,
    evdev_key_code::KEY_5,
    evdev_key_code::KEY_6,
    evdev_key_code::KEY_7,
    evdev_key_code::KEY_8,
    evdev_key_code::KEY_9,
    evdev_key_code::KEY_0,
    evdev_key_code::KEY_ENTER,
    evdev_key_code::KEY_ESC,
    evdev_key_code::KEY_BACKSPACE,
    evdev_key_code::KEY_TAB,
    evdev_key_code::KEY_SPACE,
    evdev_key_code::KEY_MINUS,
    evdev_key_code::KEY_EQUAL,
    evdev_key_code::KEY_LEFTBRACE,
    // 30
    evdev_key_code::KEY_RIGHTBRACE,
    evdev_key_code::KEY_BACKSLASH,
    evdev_key_code::KEY_BACKSLASH,
    evdev_key_code::KEY_SEMICOLON,
    evdev_key_code::KEY_APOSTROPHE,
    evdev_key_code::KEY_GRAVE,
    evdev_key_code::KEY_COMMA,
    evdev_key_code::KEY_DOT,
    evdev_key_code::KEY_SLASH,
    evdev_key_code::KEY_CAPSLOCK,
    evdev_key_code::KEY_F1,
    evdev_key_code::KEY_F2,
    evdev_key_code::KEY_F3,
    evdev_key_code::KEY_F4,
    evdev_key_code::KEY_F5,
    evdev_key_code::KEY_F6,
    // 40
    evdev_key_code::KEY_F7,
    evdev_key_code::KEY_F8,
    evdev_key_code::KEY_F9,
    evdev_key_code::KEY_F10,
    evdev_key_code::KEY_F11,
    evdev_key_code::KEY_F12,
    evdev_key_code::KEY_SYSRQ,
    evdev_key_code::KEY_SCROLLLOCK,
    evdev_key_code::KEY_PAUSE,
    evdev_key_code::KEY_INSERT,
    evdev_key_code::KEY_HOME,
    evdev_key_code::KEY_PAGEUP,
    evdev_key_code::KEY_DELETE,
    evdev_key_code::KEY_END,
    evdev_key_code::KEY_PAGEDOWN,
    evdev_key_code::KEY_RIGHT,
    // 50
    evdev_key_code::KEY_LEFT,
    evdev_key_code::KEY_DOWN,
    evdev_key_code::KEY_UP,
    evdev_key_code::KEY_NUMLOCK,
    evdev_key_code::KEY_KPSLASH,
    evdev_key_code::KEY_KPASTERISK,
    evdev_key_code::KEY_KPMINUS,
    evdev_key_code::KEY_KPPLUS,
    evdev_key_code::KEY_KPENTER,
    evdev_key_code::KEY_KP1,
    evdev_key_code::KEY_KP2,
    evdev_key_code::KEY_KP3,
    evdev_key_code::KEY_KP4,
    evdev_key_code::KEY_KP5,
    evdev_key_code::KEY_KP6,
    evdev_key_code::KEY_KP7,
    // 60
    evdev_key_code::KEY_KP8,
    evdev_key_code::KEY_KP9,
    evdev_key_code::KEY_KP0,
    evdev_key_code::KEY_KPDOT,
    evdev_key_code::KEY_102ND,
    evdev_key_code::KEY_COMPOSE,
    evdev_key_code::KEY_POWER,
    evdev_key_code::KEY_KPEQUAL,
    evdev_key_code::KEY_F13,
    evdev_key_code::KEY_F14,
    evdev_key_code::KEY_F15,
    evdev_key_code::KEY_F16,
    evdev_key_code::KEY_F17,
    evdev_key_code::KEY_F18,
    evdev_key_code::KEY_F19,
    evdev_key_code::KEY_F20,
    // 70
    evdev_key_code::KEY_F21,
    evdev_key_code::KEY_F22,
    evdev_key_code::KEY_F23,
    evdev_key_code::KEY_F24,
    evdev_key_code::KEY_OPEN,
    evdev_key_code::KEY_HELP,
    evdev_key_code::KEY_PROPS,
    evdev_key_code::KEY_FRONT,
    evdev_key_code::KEY_STOP,
    evdev_key_code::KEY_AGAIN,
    evdev_key_code::KEY_UNDO,
    evdev_key_code::KEY_CUT,
    evdev_key_code::KEY_COPY,
    evdev_key_code::KEY_PASTE,
    evdev_key_code::KEY_FIND,
    evdev_key_code::KEY_MUTE,
    // 80
    evdev_key_code::KEY_VOLUMEUP,
    evdev_key_code::KEY_VOLUMEDOWN,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_KPCOMMA,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RO,
    evdev_key_code::KEY_KATAKANAHIRAGANA,
    evdev_key_code::KEY_YEN,
    evdev_key_code::KEY_HENKAN,
    evdev_key_code::KEY_MUHENKAN,
    evdev_key_code::KEY_KPJPCOMMA,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 90
    evdev_key_code::KEY_HANGEUL,
    evdev_key_code::KEY_HANJA,
    evdev_key_code::KEY_KATAKANA,
    evdev_key_code::KEY_HIRAGANA,
    evdev_key_code::KEY_ZENKAKUHANKAKU,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_DELETE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // a0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // b0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_KPLEFTPAREN,
    evdev_key_code::KEY_KPRIGHTPAREN,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // c0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // d0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_DELETE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // e0
    evdev_key_code::KEY_LEFTCTRL,
    evdev_key_code::KEY_LEFTSHIFT,
    evdev_key_code::KEY_LEFTALT,
    evdev_key_code::KEY_LEFTMETA,
    evdev_key_code::KEY_RIGHTCTRL,
    evdev_key_code::KEY_RIGHTSHIFT,
    evdev_key_code::KEY_RIGHTALT,
    evdev_key_code::KEY_RIGHTMETA,
    evdev_key_code::KEY_PLAYPAUSE,
    evdev_key_code::KEY_STOPCD,
    evdev_key_code::KEY_PREVIOUSSONG,
    evdev_key_code::KEY_NEXTSONG,
    evdev_key_code::KEY_EJECTCD,
    evdev_key_code::KEY_VOLUMEUP,
    evdev_key_code::KEY_VOLUMEDOWN,
    evdev_key_code::KEY_MUTE,
    // f0
    evdev_key_code::KEY_WWW,
    evdev_key_code::KEY_BACK,
    evdev_key_code::KEY_FORWARD,
    evdev_key_code::KEY_STOP,
    evdev_key_code::KEY_FIND,
    evdev_key_code::KEY_SCROLLUP,
    evdev_key_code::KEY_SCROLLDOWN,
    evdev_key_code::KEY_EDIT,
    evdev_key_code::KEY_SLEEP,
    evdev_key_code::KEY_COFFEE,
    evdev_key_code::KEY_REFRESH,
    evdev_key_code::KEY_CALC,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
};

static const evdev_key_code media_keycodes[] = {
    // 00
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 10
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 20
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 30
    evdev_key_code::KEY_POWER,
    evdev_key_code::KEY_RESTART,
    evdev_key_code::KEY_SLEEP,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_SLEEP,
    evdev_key_code::KEY_KBDILLUMTOGGLE,
    evdev_key_code::KEY_RESERVED, // FIXME - BTN_MISC
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 40
    evdev_key_code::KEY_MENU,
    evdev_key_code::KEY_SELECT,
    evdev_key_code::KEY_UP,
    evdev_key_code::KEY_DOWN,
    evdev_key_code::KEY_LEFT,
    evdev_key_code::KEY_RIGHT,
    evdev_key_code::KEY_ESC,
    evdev_key_code::KEY_KPPLUS,
    evdev_key_code::KEY_KPMINUS,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 50
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 60
    evdev_key_code::KEY_INFO,
    evdev_key_code::KEY_SUBTITLE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_VCR,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_CAMERA,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RED,
    evdev_key_code::KEY_GREEN,
    evdev_key_code::KEY_BLUE,
    evdev_key_code::KEY_YELLOW,
    evdev_key_code::KEY_ASPECT_RATIO,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_BRIGHTNESSUP,
    // 70
    evdev_key_code::KEY_BRIGHTNESSDOWN,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_DISPLAYTOGGLE,
    evdev_key_code::KEY_BRIGHTNESS_MIN,
    evdev_key_code::KEY_BRIGHTNESS_MAX,
    evdev_key_code::KEY_BRIGHTNESS_AUTO,
    evdev_key_code::KEY_CAMERA_ACCESS_ENABLE,
    evdev_key_code::KEY_CAMERA_ACCESS_DISABLE,
    evdev_key_code::KEY_CAMERA_ACCESS_TOGGLE,
    evdev_key_code::KEY_KBDILLUMUP,
    evdev_key_code::KEY_KBDILLUMDOWN,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_KBDILLUMTOGGLE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 80
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_VIDEO_NEXT,
    evdev_key_code::KEY_LAST,
    evdev_key_code::KEY_ENTER,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_PC,
    evdev_key_code::KEY_TV,
    evdev_key_code::KEY_WWW,
    evdev_key_code::KEY_DVD,
    evdev_key_code::KEY_PHONE,
    evdev_key_code::KEY_PROGRAM,
    evdev_key_code::KEY_VIDEOPHONE,
    evdev_key_code::KEY_GAMES,
    // 90
    evdev_key_code::KEY_MEMO,
    evdev_key_code::KEY_CD,
    evdev_key_code::KEY_VCR,
    evdev_key_code::KEY_TUNER,
    evdev_key_code::KEY_EXIT,
    evdev_key_code::KEY_HELP,
    evdev_key_code::KEY_TAPE,
    evdev_key_code::KEY_TV2,
    evdev_key_code::KEY_SAT,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_PVR,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_CHANNELUP,
    evdev_key_code::KEY_CHANNELDOWN,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // a0
    evdev_key_code::KEY_VCR2,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // b0
    evdev_key_code::KEY_PLAY,
    evdev_key_code::KEY_PAUSE,
    evdev_key_code::KEY_RECORD,
    evdev_key_code::KEY_FASTFORWARD,
    evdev_key_code::KEY_REWIND,
    evdev_key_code::KEY_NEXTSONG,
    evdev_key_code::KEY_PREVIOUSSONG,
    evdev_key_code::KEY_STOPCD,
    evdev_key_code::KEY_EJECTCD,
    evdev_key_code::KEY_SHUFFLE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_MEDIA_REPEAT,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_SLOW,
    // c0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_PLAYPAUSE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_VOICECOMMAND,
    // d0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_DICTATE,
    evdev_key_code::KEY_EMOJI_PICKER,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // e0
    evdev_key_code::KEY_RESERVED, // FIXME - ABS_VOLUME
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_MUTE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_BASSBOOST,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_VOLUMEUP,
    evdev_key_code::KEY_VOLUMEDOWN,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // f0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_SLOW,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 100
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 110
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 120
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 130
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 140
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 150
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 160
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 170
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 180
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_BUTTONCONFIG,
    evdev_key_code::KEY_BOOKMARKS,
    evdev_key_code::KEY_CONFIG,
    evdev_key_code::KEY_WORDPROCESSOR,
    evdev_key_code::KEY_EDITOR,
    evdev_key_code::KEY_SPREADSHEET,
    evdev_key_code::KEY_GRAPHICSEDITOR,
    evdev_key_code::KEY_PRESENTATION,
    evdev_key_code::KEY_DATABASE,
    evdev_key_code::KEY_MAIL,
    evdev_key_code::KEY_NEWS,
    evdev_key_code::KEY_VOICEMAIL,
    evdev_key_code::KEY_ADDRESSBOOK,
    evdev_key_code::KEY_CALENDAR,
    evdev_key_code::KEY_TASKMANAGER,
    // 190
    evdev_key_code::KEY_JOURNAL,
    evdev_key_code::KEY_FINANCE,
    evdev_key_code::KEY_CALC,
    evdev_key_code::KEY_PLAYER,
    evdev_key_code::KEY_FILE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_WWW,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_CHAT,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_LOGOFF,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_COFFEE,
    evdev_key_code::KEY_CONTROLPANEL,
    // 1a0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_APPSELECT,
    evdev_key_code::KEY_NEXT,
    evdev_key_code::KEY_PREVIOUS,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_HELP,
    evdev_key_code::KEY_DOCUMENTS,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_SPELLCHECK,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_KEYBOARD,
    evdev_key_code::KEY_RESERVED,
    // 1b0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_SCREENSAVER,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_FILE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_IMAGES,
    evdev_key_code::KEY_AUDIO,
    evdev_key_code::KEY_VIDEO,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_MESSENGER,
    evdev_key_code::KEY_INFO,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 1c0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_ASSISTANT,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 1d0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 1e0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 1f0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 200
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_NEW,
    evdev_key_code::KEY_OPEN,
    evdev_key_code::KEY_CLOSE,
    evdev_key_code::KEY_EXIT,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_SAVE,
    evdev_key_code::KEY_PRINT,
    evdev_key_code::KEY_PROPS,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 210
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_UNDO,
    evdev_key_code::KEY_COPY,
    evdev_key_code::KEY_CUT,
    evdev_key_code::KEY_PASTE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_FIND,
    // 220
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_SEARCH,
    evdev_key_code::KEY_GOTO,
    evdev_key_code::KEY_HOMEPAGE,
    evdev_key_code::KEY_BACK,
    evdev_key_code::KEY_FORWARD,
    evdev_key_code::KEY_STOP,
    evdev_key_code::KEY_REFRESH,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_BOOKMARKS,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_ZOOMIN,
    evdev_key_code::KEY_ZOOMOUT,
    evdev_key_code::KEY_ZOOMRESET,
    // 230
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_FULL_SCREEN,
    evdev_key_code::KEY_SCROLLUP,
    evdev_key_code::KEY_SCROLLDOWN,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED, // AC Pan, dealt with separately
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_EDIT,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 240
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 250
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_CANCEL,
    // 260
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_INSERT,
    evdev_key_code::KEY_DELETE,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 270
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_REDO,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 280
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_REPLY,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_FORWARDMAIL,
    evdev_key_code::KEY_SEND,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 290
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_KBD_LAYOUT_NEXT,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_SCALE,
    // 2a0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_ALL_APPLICATIONS,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 2b0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    // 2c0
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_KBDINPUTASSIST_PREV,
    evdev_key_code::KEY_KBDINPUTASSIST_NEXT,
    evdev_key_code::KEY_KBDINPUTASSIST_PREVGROUP,
    evdev_key_code::KEY_KBDINPUTASSIST_NEXTGROUP,
    evdev_key_code::KEY_KBDINPUTASSIST_ACCEPT,
    evdev_key_code::KEY_KBDINPUTASSIST_CANCEL,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
    evdev_key_code::KEY_RESERVED,
};

static bool found_usb_hid_device(usb_device& d, const usb_device_config& config,
                                 const usb_interface& interface);

static usb_recognizer mod_usb_interfaces = {
    {}, usb_class_code::hid, found_usb_hid_device
};

static void init_usbhid() {
    insert_tail_list(usb_interfaces, mod_usb_interfaces.le);
}

static module_init module_init_usbhid(init_usbhid);

enum class usb_hid_request : uint8_t {
    GET_REPORT = 0x01,
    GET_IDLE = 0x02,
    GET_PROTOCOL = 0x03,
    SET_REPORT = 0x09,
    SET_IDLE = 0x0a,
    SET_PROTOCOL = 0x0b,
};

enum class usb_hid_report_type : uint8_t {
    INPUT = 0x01,
    OUTPUT = 0x02,
    FEATURE = 0x03,
};

static bool usb_hid_set_report(usb_device& d, span<uint8_t> buf,
                               enum usb_hid_report_type report_type,
                               uint8_t report_id, uint16_t interface) {
    return d.ep0_queue.control_transfer(buf,
                                        USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_INTERFACE,
                                        (enum usb_request)usb_hid_request::SET_REPORT,
                                        (uint16_t)(((uint8_t)report_type << 8) | report_id),
                                        interface);
}

bool usb_hid_device::get_report_descriptor(const usb_device_config& cfg) {
    auto sp = span(cfg.data, cfg.len);
    span<const uint8_t> hid;
    bool ignore = true;

    while (true) {
        if (sp.empty())
            return false;

        auto len = sp[0];

        if (sp.size() < len || len < 2)
            return false;

        auto type = (usb_descriptor)sp[1];

        if (type == usb_descriptor::INTERFACE && len >= sizeof(usb_interface_descriptor)) {
            auto& desc = *(usb_interface_descriptor*)sp.data();

            ignore = desc.bInterfaceNumber != interface.desc.bInterfaceNumber;
        }

        if (type == usb_descriptor::HID && !ignore) {
            hid = sp.subspan(0, len);
            break;
        }

        sp = sp.subspan(len);
    }

    if (hid.empty()) {
        print("HID descriptor not found for interface {} on USB HID device\n",
              interface.desc.bInterfaceNumber);
        return false;
    }

    if (hid.size() < offsetof(usb_hid_descriptor, entries)) {
        print("HID descriptor was {} bytes, expected at least {}\n", hid.size(),
              offsetof(usb_hid_descriptor, entries));
        return false;
    }

    auto& h = *(usb_hid_descriptor*)hid.data();

    if (hid.size() < offsetof(usb_hid_descriptor, entries) + (sizeof(usb_hid_descriptor_entry) * h.bNumDescriptors)) {
        print("HID descriptor was {} bytes, expected {}\n", hid.size(),
              offsetof(usb_hid_descriptor, entries), offsetof(usb_hid_descriptor, entries) + (sizeof(usb_hid_descriptor_entry) * h.bNumDescriptors));
        return false;
    }

    optional<uint16_t> len;

    for (unsigned int i = 0; i < h.bNumDescriptors; i++) {
        if (h.entries[i].bDescriptorType != usb_descriptor::HID_REPORT)
            continue;

        len = +h.entries[i].wDescriptorLength;
        break;
    }

    if (!len.has_value()) {
        print("HID report descriptor not found for interface {} on USB HID device\n",
              interface.desc.bInterfaceNumber);
        return false;
    }

    if (*len == 0) {
        print("HID report descriptor length was 0\n");
        return false;
    }

    report_descriptor_len = *len;

    report_descriptor = (uint8_t*)malloc(*len);
    if (!report_descriptor) {
        print("malloc failed\n");
        return false;
    }

    if (!d.get_descriptor_interface(span(report_descriptor, *len),
                                    usb_descriptor::HID_REPORT, 0,
                                    interface.desc.bInterfaceNumber)) {
        print("usb_device::get_descriptor_interface failed\n");
        return false;
    }

    return true;
}

static int32_t get_signed_num(uint32_t v, unsigned int n) {
    if (n == 1 && v & 0x80)
        return (int32_t)(0xffffff00 | (uint8_t)v);
    else if (n == 2 && v & 0x8000)
        return (int32_t)(0xffff0000 | (uint16_t)v);
    else
        return (int32_t)v;
}

#if 0
static void print_report_descriptor(span<const uint8_t> r) {
    print("report descriptor:\n");

    while (!r.empty()) {
        auto b = r[0];

        if (b == 0xfe) { // long item
            if (r.size() == 1) {
                print("report descriptor is truncated\n");
                return;
            }

            uint8_t size = r[1];

            if (r.size() < (size_t)(size + 3)) {
                print("report descriptor is truncated\n");
                return;
            }

            print("parse_report_descriptor: skipping long item\n");
            r = r.subspan(size + 3);
            continue;
        }

        unsigned int n;

        switch (b & 0x3) {
            case 0:
                n = 0;
                break;

            case 1:
                n = 1;
                break;

            case 2:
                n = 2;
                break;

            case 3:
                n = 4;
                break;
        }

        if (r.size() < n + 1) {
            print("report descriptor is truncated\n");
            return;
        }

        auto sp = span(r.data() + 1, n);

        uint32_t v;

        if (n == 0)
            v = 0;
        else if (n == 1)
            v = sp[0];
        else if (n == 2)
            v = (sp[1] << 8) | sp[0];
        else if (n == 4)
            v = (sp[3] << 24) | (sp[2] << 16) | (sp[1] << 8) | sp[0];

        switch (b & 0xc) {
            case 0x0: // main
                switch (b & 0xf0) {
                    case 0x80: {
                        print("input {:x}\n", v);
                        break;
                    }

                    case 0x90: {
                        print("output {:x}\n", v);
                        break;
                    }

                    case 0xa0: {
                        print("collection {:x}\n", v);
                        break;
                    }

                    case 0xb0: {
                        print("feature {:x}\n", v);
                        break;
                    }

                    case 0xc0:
                        print("end collection\n");
                        break;

                    default:
                        print("unrecognized main item {:02x}, {:x}\n", b, v);
                        break;
                }
            break;

            case 0x4: // global
                switch (b & 0xf0) {
                    case 0x00:
                        print("usage page {:x}\n", v);
                        break;

                    case 0x10:
                        print("logical minimum {}\n", get_signed_num(v, n));
                        break;

                    case 0x20:
                        print("logical maximum {}\n", get_signed_num(v, n));
                        break;

                    case 0x30:
                        print("physical minimum {}\n", get_signed_num(v, n));
                        break;

                    case 0x40:
                        print("physical maximum {}\n", get_signed_num(v, n));
                        break;

                    case 0x50:
                        print("unit exponent {:x}\n", v);
                        break;

                    case 0x60:
                        print("unit {:x}\n", v);
                        break;

                    case 0x70:
                        print("report size {:x}\n", v);
                        break;

                    case 0x80:
                        print("report ID {:x}\n", v);
                        break;

                    case 0x90:
                        print("report count {:x}\n", v);
                        break;

                    case 0xa0:
                        print("push\n");
                        break;

                    case 0xb0:
                        print("pop\n");
                        break;

                    default:
                        print("unrecognized global item {:02x}, {:x}\n", b, v);
                        break;
                }
            break;

            case 0x8: // local
                switch (b & 0xf0) {
                    case 0x00:
                        print("usage {:x}\n", v);
                        break;

                    case 0x10:
                        print("usage minimum {:x}\n", v);
                        break;

                    case 0x20:
                        print("usage maximum {:x}\n", v);
                        break;

                    case 0x30:
                        print("designator index {:x}\n", v);
                        break;

                    case 0x40:
                        print("designator minimum {:x}\n", v);
                        break;

                    case 0x50:
                        print("designator maximum {:x}\n", v);
                        break;

                    case 0x70:
                        print("string index {:x}\n", v);
                        break;

                    case 0x80:
                        print("string minimum {:x}\n", v);
                        break;

                    case 0x90:
                        print("string maximum {:x}\n", v);
                        break;

                    case 0xa0:
                        print("delimiter {:x}\n", v);
                        break;

                    default:
                        print("unrecognized local item {:02x}, {:x}\n", b, v);
                        break;
                }
            break;

            default:
                print("unrecognized item {:02x}, {:x}\n", b, v);
                break;
        }

        r = r.subspan(n + 1);
    }
}
#endif

usb_hid_report* usb_hid_device::find_report(uint8_t id, bool input) {
    auto& list = input ? input_reports : output_reports;

    for (auto le = list.next; le != &list; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

        if (r.id == id)
            return &r;
    }

    auto r = new usb_hid_report;
    if (!r) {
        print("new failed\n");
        return nullptr;
    }

    r->id = id;

    insert_tail_list(list, r->le);

    return r;
}

bool usb_hid_device::parse_report_descriptor(span<const uint8_t> r) {
    uint32_t usage_min = 0, usage_max = 0;
    size_t num_usages = 0;
    uint32_t* usages = nullptr;
    uint8_t report_id = 0;
    uint32_t report_count = 0, report_size = 0;
    uint16_t usage_page = 0;
    uint32_t logical_max_u = 0;
    int32_t logical_min = 0, logical_max_s = 0;

    while (!r.empty()) {
        auto b = r[0];

        if (b == 0xfe) { // long item
            if (r.size() == 1) {
                print("report descriptor is truncated\n");

                if (usages)
                    free(usages);

                return false;
            }

            uint8_t size = r[1];

            if (r.size() < (size_t)(size + 3)) {
                print("report descriptor is truncated\n");

                if (usages)
                    free(usages);

                return false;
            }

            r = r.subspan(size + 3);
            continue;
        }

        unsigned int n;

        switch (b & 0x3) {
            case 0:
                n = 0;
                break;

            case 1:
                n = 1;
                break;

            case 2:
                n = 2;
                break;

            case 3:
                n = 4;
                break;
        }

        if (r.size() < n + 1) {
            print("report descriptor is truncated\n");

            if (usages)
                free(usages);

            return false;
        }

        auto sp = span(r.data() + 1, n);

        uint32_t v;

        if (n == 0)
            v = 0;
        else if (n == 1)
            v = sp[0];
        else if (n == 2)
            v = (uint32_t)(sp[1] << 8) | sp[0];
        else if (n == 4)
            v = (uint32_t)(sp[3] << 24) | (uint32_t)(sp[2] << 16) | (uint32_t)(sp[1] << 8) | sp[0];

        switch (b & 0xc) {
            case 0x0: // main
                switch (b & 0xf0) {
                    case 0x80: // input
                    case 0x90: // output
                    {
                        auto rep = find_report(report_id, (b & 0xf0) == 0x80);
                        if (!rep) {
                            if (usages)
                                free(usages);

                            return false;
                        }

                        if (!(v & 1)) { // skip constant
                            int32_t logical_max;

                            if (logical_min >= 0)
                                logical_max = (int32_t)logical_max_u;
                            else
                                logical_max = logical_max_s;

                            auto f = new usb_hid_field;
                            if (!f) {
                                print("new failed\n");

                                if (usages)
                                    free(usages);

                                return false;
                            }

                            f->is_variable = v & 2;
                            f->is_relative = v & 4;
                            f->count = report_count;
                            f->size = report_size;
                            f->logical_min = logical_min;
                            f->logical_max = logical_max;
                            f->usage_page = usage_page;
                            f->usages = usages;
                            f->num_usages = num_usages;
                            f->usage_min = usage_min;
                            f->usage_max = usage_max;
                            f->bit_offset = rep->bit_offset;

                            insert_tail_list(rep->fields, f->le);

                            usages = nullptr;
                        } else {
                            if (usages) {
                                free(usages);
                                usages = nullptr;
                            }
                        }

                        num_usages = 0;
                        usage_min = 0;
                        usage_max = 0;

                        rep->bit_offset += report_count * report_size;

                        break;
                    }

                    default:
                        usage_min = 0;
                        usage_max = 0;

                        if (usages) {
                            free(usages);
                            usages = nullptr;
                        }

                        num_usages = 0;

                        break;
                }
            break;

            case 0x4: // global
                switch (b & 0xf0) {
                    case 0x00:
                        // FIXME - return error if more than 0xffff?
                        usage_page = (uint16_t)v;
                        break;

                    case 0x10:
                        logical_min = get_signed_num(v, n);
                        break;

                    case 0x20:
                        logical_max_u = v;
                        logical_max_s = get_signed_num(v, n);
                        break;

                    case 0x70:
                        report_size = v;
                        break;

                    case 0x80:
                        // FIXME - return error if more than 0xff?
                        report_id = (uint8_t)v;
                        break;

                    case 0x90:
                        report_count = v;
                        break;

                    default:
                        break;
                }
            break;

            case 0x8: // local
                switch (b & 0xf0) {
                    case 0x00:
                        usages = (decltype(usages))realloc(usages, sizeof(*usages) + (num_usages + 1));
                        if (!usages) {
                            print("realloc failed\n");
                            return false;
                        }

                        usages[num_usages] = v;
                        num_usages++;

                        break;

                    case 0x10:
                        usage_min = v;
                        break;

                    case 0x20:
                        usage_max = v;
                        break;

                    default:
                        break;
                }
            break;

            default:
                break;
        }

        r = r.subspan(n + 1);
    }

    if (usages)
        free(usages);

    return true;
}

#if 0
static void print_field(const usb_hid_field& f) {
    print("is_variable {}, is_relative {}, ", f.is_variable ? "true" : "false",
          f.is_relative ? "true" : "false");

    if (f.num_usages == 0)
        print("usage min = {:x}, usage max = {:x}, ", f.usage_min, f.usage_max);
    else {
        print("usage = ");

        for (size_t i = 0; i < f.num_usages; i++) {
            print("{:x}, ", f.usages[i]);
        }
    }

    print("count {:x}, size {:x}, usage page {:x}, logical min = {}, logical max = {}, bit offset = {}\n",
          f.count, f.size, f.usage_page, f.logical_min, f.logical_max,
          f.bit_offset);
}
#endif

usb_hid_device::usb_hid_device(usb_device& d, const usb_device_config& config,
                               xhci_queue& queue, uint16_t packet_size,
                               const usb_interface& interface) : d(d), queue(queue), interface(interface) {
    d.acquire();

    if (!get_report_descriptor(config)) {
        print("usb_hid_device::get_report_descriptor failed\n");
        return;
    }

#if 0
    print_report_descriptor(span(report_descriptor, report_descriptor_len));
#endif
    parse_report_descriptor(span(report_descriptor, report_descriptor_len));

    for (auto le = input_reports.next; le != &input_reports; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

#if 0
        print("input report {}:\n", r.id);
#endif
        for (auto le2 = r.fields.next; le2 != &r.fields; le2 = le2->next) {
            auto& f = containing_record<usb_hid_field>(le2);

            if (f.usage_page == USAGE_PAGE_KEYBOARD || f.usage_page == USAGE_PAGE_CONSUMER)
                is_keyboard = true;
#if 0
            print_field(f);
#endif
        }
    }

#if 0
    for (auto le = output_reports.next; le != &output_reports; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

        print("output report {}:\n", r.id);

        for (auto le2 = r.fields.next; le2 != &r.fields; le2 = le2->next) {
            auto& f = containing_record<usb_hid_field>(le2);

            print_field(f);
        }
    }
#endif

    this->packet_size = packet_size;

    unsigned int num = hid_num.fetch_add(1);

    {
        char s[50];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "usbhid{}", num);

        auto sv = string_view(s, (size_t)(sp.data() - s));

        // FIXME - put in /dev/input like on Linux?
        auto status = name_object(*this, sv);
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return;
        }
    }

    this->kc = register_evdev_device(*this);

    if (is_keyboard) {
        repeat_timer = create_timer(true);
        if (!repeat_timer) {
            print("create_timer failed\n");
            return;
        }

        acquire();
        if (!add_kernel_thread([](void* ctx) { ((usb_hid_device*)ctx)->keyboard_repeat_thread(); }, this)) {
            release();
            print("add_kernel_thread failed\n");
            return;
        }
    }

    init = true;
}

usb_hid_device::~usb_hid_device() {
    if (kc)
        kc->unregister();

    if (report_descriptor)
        free(report_descriptor);

    while (!is_list_empty(input_reports)) {
        auto& r = containing_record<usb_hid_report>(input_reports.next);

        remove_entry_list(r.le);

        while (!is_list_empty(r.fields)) {
            auto& f = containing_record<usb_hid_field>(r.fields.next);

            remove_entry_list(f.le);

            delete &f;
        }

        delete &r;
    }

    while (!is_list_empty(output_reports)) {
        auto& r = containing_record<usb_hid_report>(output_reports.next);

        remove_entry_list(r.le);

        while (!is_list_empty(r.fields)) {
            auto& f = containing_record<usb_hid_field>(r.fields.next);

            remove_entry_list(f.le);

            delete &f;
        }

        delete &r;
    }

    d.release();
}

void usb_hid_device::set_led_field(usb_hid_report& r, usb_hid_field& f,
                                   evdev_led_code led, bool on) {
    if (!f.is_variable)
        return; // FIXME - handle arrays?

    uint32_t usage;

    switch (led) {
        case evdev_led_code::LED_NUML:
            usage = LED_USAGE_NUM_LOCK;
            break;

        case evdev_led_code::LED_CAPSL:
            usage = LED_USAGE_CAPS_LOCK;
            break;

        case evdev_led_code::LED_SCROLLL:
            usage = LED_USAGE_SCROLL_LOCK;
            break;

        default:
            return;
    }

    if (f.num_usages == 0) {
        if (f.usage_min > usage || f.usage_max < usage)
            return;
    } else {
        bool match = false;

        for (unsigned int i = 0; i < f.num_usages; i++) {
            if (f.usages[i] == usage) {
                match = true;
                break;
            }
        }

        if (!match)
            return;
    }

    size_t report_bits = 0;

    for (auto le = r.fields.next; le != &r.fields; le = le->next) {
        auto& f2 = containing_record<usb_hid_field>(le);

        report_bits = max(report_bits, (size_t)(f2.bit_offset + (f2.count * f2.size)));
    }

    if (r.id != 0)
        report_bits += 8;

    if (report_bits % 8)
        report_bits += 8 - (report_bits % 8);

    if (!r.old) {
        r.old = malloc(report_bits / 8);
        if (!r.old) {
            print("malloc failed\n");
            return;
        }

        if (r.id != 0) {
            *(uint8_t*)r.old = r.id;
            memset((uint8_t*)r.old + 1, 0, (report_bits / 8) - 1);
        } else
            memset(r.old, 0, report_bits / 8);
    }

    auto set_bit = [&r](size_t n, bool set) {
        auto buf = (uint8_t*)r.old;

        if (r.id != 0)
            buf++;

        if (set)
            buf[n / 8] |= (1 << (n % 8));
        else
            buf[n / 8] &= (uint8_t)~(1 << (n % 8));
    };

    if (f.num_usages == 0) {
        for (unsigned int i = f.usage_min; i < f.count && i <= f.usage_max; i++) {
            if (i == usage) {
                set_bit(i - f.usage_min, on);
                break;
            }
        }
    } else {
        for (unsigned int i = 0; i < f.num_usages; i++) {
            if (f.usages[i] == usage) {
                set_bit(i, on);
                break;
            }
        }
    }

    if (!usb_hid_set_report(d, span((uint8_t*)r.old, report_bits / 8),
                            usb_hid_report_type::OUTPUT, r.id,
                            interface.desc.bInterfaceNumber)) {
        print("usb_hid_set_report failed\n");
        return;
    }
}

static bool should_key_repeat(enum evdev_key_code code) {
    switch (code) {
        case evdev_key_code::KEY_RESERVED:
        case evdev_key_code::KEY_LEFTALT:
        case evdev_key_code::KEY_RIGHTALT:
        case evdev_key_code::KEY_LEFTCTRL:
        case evdev_key_code::KEY_RIGHTCTRL:
        case evdev_key_code::KEY_LEFTSHIFT:
        case evdev_key_code::KEY_RIGHTSHIFT:
        case evdev_key_code::KEY_LEFTMETA:
        case evdev_key_code::KEY_RIGHTMETA:
        case evdev_key_code::KEY_CAPSLOCK:
        case evdev_key_code::KEY_NUMLOCK:
        case evdev_key_code::KEY_SCROLLLOCK:
        case evdev_key_code::KEY_BRIGHTNESSUP:
        case evdev_key_code::KEY_BRIGHTNESSDOWN:
        case evdev_key_code::KEY_PLAY:
        case evdev_key_code::KEY_PAUSE:
        case evdev_key_code::KEY_RECORD:
        case evdev_key_code::KEY_FASTFORWARD:
        case evdev_key_code::KEY_REWIND:
        case evdev_key_code::KEY_NEXTSONG:
        case evdev_key_code::KEY_PREVIOUSSONG:
        case evdev_key_code::KEY_STOPCD:
        case evdev_key_code::KEY_EJECTCD:
        case evdev_key_code::KEY_PLAYPAUSE:
        case evdev_key_code::KEY_MUTE:
        case evdev_key_code::KEY_CONFIG:
        case evdev_key_code::KEY_EMAIL:
        case evdev_key_code::KEY_CALC:
        case evdev_key_code::KEY_MEDIA:
        case evdev_key_code::KEY_COFFEE:
        case evdev_key_code::KEY_FILE:
        case evdev_key_code::KEY_SEARCH:
        case evdev_key_code::KEY_HOMEPAGE:
        case evdev_key_code::KEY_BACK:
        case evdev_key_code::KEY_FORWARD:
        case evdev_key_code::KEY_STOP:
        case evdev_key_code::KEY_REFRESH:
        case evdev_key_code::KEY_POWER:
        case evdev_key_code::KEY_RESTART:
        case evdev_key_code::KEY_SLEEP:
        case evdev_key_code::KEY_KBDILLUMTOGGLE:
        case evdev_key_code::KEY_MENU:
        case evdev_key_code::KEY_SELECT:
        case evdev_key_code::KEY_INFO:
        case evdev_key_code::KEY_SUBTITLE:
        case evdev_key_code::KEY_VCR:
        case evdev_key_code::KEY_CAMERA:
        case evdev_key_code::KEY_RED:
        case evdev_key_code::KEY_GREEN:
        case evdev_key_code::KEY_BLUE:
        case evdev_key_code::KEY_YELLOW:
        case evdev_key_code::KEY_ASPECT_RATIO:
        case evdev_key_code::KEY_DISPLAYTOGGLE:
        case evdev_key_code::KEY_BRIGHTNESS_MIN:
        case evdev_key_code::KEY_BRIGHTNESS_MAX:
        case evdev_key_code::KEY_BRIGHTNESS_AUTO:
        case evdev_key_code::KEY_CAMERA_ACCESS_ENABLE:
        case evdev_key_code::KEY_CAMERA_ACCESS_DISABLE:
        case evdev_key_code::KEY_CAMERA_ACCESS_TOGGLE:
        case evdev_key_code::KEY_KBDILLUMUP:
        case evdev_key_code::KEY_KBDILLUMDOWN:
        case evdev_key_code::KEY_VIDEO_NEXT:
        case evdev_key_code::KEY_LAST:
        case evdev_key_code::KEY_PC:
        case evdev_key_code::KEY_TV:
        case evdev_key_code::KEY_WWW:
        case evdev_key_code::KEY_DVD:
        case evdev_key_code::KEY_PHONE:
        case evdev_key_code::KEY_PROGRAM:
        case evdev_key_code::KEY_VIDEOPHONE:
        case evdev_key_code::KEY_GAMES:
        case evdev_key_code::KEY_MEMO:
        case evdev_key_code::KEY_CD:
        case evdev_key_code::KEY_TUNER:
        case evdev_key_code::KEY_EXIT:
        case evdev_key_code::KEY_HELP:
        case evdev_key_code::KEY_TAPE:
        case evdev_key_code::KEY_TV2:
        case evdev_key_code::KEY_SAT:
        case evdev_key_code::KEY_PVR:
        case evdev_key_code::KEY_CHANNELUP:
        case evdev_key_code::KEY_CHANNELDOWN:
        case evdev_key_code::KEY_VCR2:
        case evdev_key_code::KEY_SHUFFLE:
        case evdev_key_code::KEY_MEDIA_REPEAT:
        case evdev_key_code::KEY_SLOW:
        case evdev_key_code::KEY_VOICECOMMAND:
        case evdev_key_code::KEY_DICTATE:
        case evdev_key_code::KEY_EMOJI_PICKER:
        case evdev_key_code::KEY_BASSBOOST:
        case evdev_key_code::KEY_BUTTONCONFIG:
        case evdev_key_code::KEY_BOOKMARKS:
        case evdev_key_code::KEY_WORDPROCESSOR:
        case evdev_key_code::KEY_EDITOR:
        case evdev_key_code::KEY_SPREADSHEET:
        case evdev_key_code::KEY_GRAPHICSEDITOR:
        case evdev_key_code::KEY_PRESENTATION:
        case evdev_key_code::KEY_DATABASE:
        case evdev_key_code::KEY_MAIL:
        case evdev_key_code::KEY_NEWS:
        case evdev_key_code::KEY_VOICEMAIL:
        case evdev_key_code::KEY_ADDRESSBOOK:
        case evdev_key_code::KEY_CALENDAR:
        case evdev_key_code::KEY_TASKMANAGER:
        case evdev_key_code::KEY_JOURNAL:
        case evdev_key_code::KEY_FINANCE:
        case evdev_key_code::KEY_PLAYER:
        case evdev_key_code::KEY_CHAT:
        case evdev_key_code::KEY_LOGOFF:
        case evdev_key_code::KEY_CONTROLPANEL:
        case evdev_key_code::KEY_APPSELECT:
        case evdev_key_code::KEY_NEXT:
        case evdev_key_code::KEY_PREVIOUS:
        case evdev_key_code::KEY_DOCUMENTS:
        case evdev_key_code::KEY_SPELLCHECK:
        case evdev_key_code::KEY_KEYBOARD:
        case evdev_key_code::KEY_SCREENSAVER:
        case evdev_key_code::KEY_IMAGES:
        case evdev_key_code::KEY_AUDIO:
        case evdev_key_code::KEY_VIDEO:
        case evdev_key_code::KEY_MESSENGER:
        case evdev_key_code::KEY_ASSISTANT:
        case evdev_key_code::KEY_NEW:
        case evdev_key_code::KEY_OPEN:
        case evdev_key_code::KEY_CLOSE:
        case evdev_key_code::KEY_SAVE:
        case evdev_key_code::KEY_PRINT:
        case evdev_key_code::KEY_PROPS:
        case evdev_key_code::KEY_UNDO:
        case evdev_key_code::KEY_COPY:
        case evdev_key_code::KEY_CUT:
        case evdev_key_code::KEY_PASTE:
        case evdev_key_code::KEY_FIND:
        case evdev_key_code::KEY_GOTO:
        case evdev_key_code::KEY_ZOOMIN:
        case evdev_key_code::KEY_ZOOMOUT:
        case evdev_key_code::KEY_ZOOMRESET:
        case evdev_key_code::KEY_FULL_SCREEN:
        case evdev_key_code::KEY_SCROLLUP:
        case evdev_key_code::KEY_SCROLLDOWN:
        case evdev_key_code::KEY_EDIT:
        case evdev_key_code::KEY_CANCEL:
        case evdev_key_code::KEY_REDO:
        case evdev_key_code::KEY_REPLY:
        case evdev_key_code::KEY_FORWARDMAIL:
        case evdev_key_code::KEY_SEND:
        case evdev_key_code::KEY_KBD_LAYOUT_NEXT:
        case evdev_key_code::KEY_SCALE:
        case evdev_key_code::KEY_ALL_APPLICATIONS:
        case evdev_key_code::KEY_KBDINPUTASSIST_PREV:
        case evdev_key_code::KEY_KBDINPUTASSIST_NEXT:
        case evdev_key_code::KEY_KBDINPUTASSIST_PREVGROUP:
        case evdev_key_code::KEY_KBDINPUTASSIST_NEXTGROUP:
        case evdev_key_code::KEY_KBDINPUTASSIST_ACCEPT:
        case evdev_key_code::KEY_KBDINPUTASSIST_CANCEL:
        case evdev_key_code::BTN_LEFT:
        case evdev_key_code::BTN_RIGHT:
        case evdev_key_code::BTN_MIDDLE:
        case evdev_key_code::BTN_SIDE:
        case evdev_key_code::BTN_EXTRA:
            return false;

        default:
            return true;
    }
}

void usb_hid_device::post_evdev(evdev_type type, evdev_code code,
                                int32_t value, bool add_syn) {
    input_event ev;

    if (type == evdev_type::EV_KEY && value != 2 && should_key_repeat((evdev_key_code)code)) {
        auto& t = static_cast<timer&>(repeat_timer.get());

        if (value == 0) { // up
            if (repeat_key == (evdev_key_code)code) {
                if (auto status = t.cancel(nullptr); !success(status))
                    print("timer::cancel returned {}\n", status);

                repeat_key = evdev_key_code::KEY_RESERVED;
            }
        } else { // down
            if (repeat_key == evdev_key_code::KEY_RESERVED) {
                if (auto status = t.set(-ONE_MS_TICKS * REPEAT_DELAY_MS, nullptr,
                                        nullptr, false, 0, nullptr); !success(status)) {
                    print("timer::set returned {}\n", status);
                }
            } else if (repeat_key != (evdev_key_code)code) {
                if (auto status = t.cancel(nullptr); !success(status))
                    print("timer::cancel returned {}\n", status);

                if (auto status = t.set(-ONE_MS_TICKS * REPEAT_DELAY_MS, nullptr,
                                        nullptr, false, 0, nullptr); !success(status)) {
                    print("timer::set returned {}\n", status);
                }
            }

            repeat_key = (evdev_key_code)code;
        }
    }

    // FIXME - Linux also has EV_MSC events

    ev.type = type;
    ev.code = code;
    ev.value = value;

    post_event(ev, add_syn);
}

static uint32_t extract_num(span<const uint8_t> sp, unsigned int offset,
                            unsigned int size) {
    uint32_t ret = 0;

    for (unsigned int i = 0; i < size; i++) {
        unsigned int byte_index = (offset + i) / 8;
        unsigned int bit_index = (offset + i) % 8;

        if (byte_index < sp.size() && (sp[byte_index] >> bit_index) & 1)
            ret |= (uint32_t)1 << i;
    }

    return ret;
}

void usb_hid_device::input_usage(uint16_t usage_page, uint32_t usage,
                                 int32_t num, bool is_relative) {
    switch (usage_page) {
        case USAGE_PAGE_GENERIC:
            switch (usage) {
                case GENERIC_USAGE_X:
                    if (is_relative)
                        relative_x = num;
                    else
                        absolute_x = num;
                break;

                case GENERIC_USAGE_Y:
                    if (is_relative)
                        relative_y = num;
                    else
                        absolute_y = num;
                break;

                case GENERIC_USAGE_WHEEL:
                    if (is_relative)
                        relative_wheel = num;
                    else
                        absolute_wheel = num;
                break;

                default:
                    break;
            }
        break;

        case USAGE_PAGE_KEYBOARD:
            if (usage > 255)
                return;

            if (num)
                keys[usage / 64] |= (uint64_t)1 << (usage % 64);
        break;

        case USAGE_PAGE_BUTTONS:
            if (usage == 0 || usage > 5)
                return;

            if (num)
                buttons |= 1 << (usage - 1);
        break;

        case USAGE_PAGE_CONSUMER:
            if (usage == CONSUMER_USAGE_AC_PAN) {
                if (is_relative)
                    relative_hwheel = num;
            } else {
                if (usage >= media_keys.size() * 64)
                    return;

                if (num)
                    media_keys[usage / 64] |= (uint64_t)1 << (usage % 64);
            }
        break;

        default:
            // ignore vendor-defined usages
            if (usage_page >= 0xff00)
                return;

#if 0
            print("input_usage: usage page {:x}, usage {:x}, num {}\n",
                  usage_page, usage, num);
#endif
        break;
    }
}

void usb_hid_device::handle_input_field(usb_hid_field& f,
                                        span<const uint8_t> sp) {
    if (f.is_variable) {
        unsigned int offset = f.bit_offset;

        for (unsigned int i = 0; i < f.count; i++) {
            int32_t signed_num;

            auto num = extract_num(sp, offset, f.size);

            if (f.logical_min < 0 && (num & ((uint32_t)1 << (f.size - 1))))
                signed_num = (int32_t)(num | (~(uint32_t)0 << f.size));
            else
                signed_num = (int32_t)num;

            uint32_t usage;

            if (f.num_usages > 0) {
                if (f.num_usages > i)
                    usage = f.usages[i];
                else
                    usage = f.usages[f.num_usages - 1];
            } else
                usage = f.usage_min + i;

            input_usage(f.usage_page, usage, signed_num, f.is_relative);

            offset += f.size;
        }
    } else {
        unsigned int offset = f.bit_offset;

        for (unsigned int i = 0; i < f.count; i++) {
            auto usage = extract_num(sp, offset, f.size);

            if (usage != 0 && usage >= f.usage_min && usage <= f.usage_max)
                input_usage(f.usage_page, usage, 1, f.is_relative);

            offset += f.size;
        }
    }
}

void usb_hid_device::handle_input(usb_hid_report& r, span<const uint8_t> sp) {
    if (r.id != 0) {
        if (sp.empty() || sp[0] != r.id)
            return;

        sp = sp.subspan(1);
    }

    for (auto le = r.fields.next; le != &r.fields; le = le->next) {
        auto& f = containing_record<usb_hid_field>(le);

        if (f.usage_page == USAGE_PAGE_KEYBOARD) {
            auto clear_bit = [this](uint32_t usage) {
                keys[usage / 64] &= ~((uint64_t)1 << (usage % 64));
            };

            if (f.num_usages > 0) {
                for (unsigned int i = 0; i < f.num_usages; i++) {
                    if (f.usages[i] < 256)
                        clear_bit(f.usages[i]);
                }
            } else {
                for (unsigned int i = f.usage_min; i <= f.usage_max; i++) {
                    if (i < 256)
                        clear_bit(i);
                }
            }
        } else if (f.usage_page == USAGE_PAGE_BUTTONS) {
            auto clear_bit = [this](uint32_t usage) {
                buttons &= (uint8_t)~(1 << usage);
            };

            if (f.num_usages > 0) {
                for (unsigned int i = 0; i < f.num_usages; i++) {
                    if (f.usages[i] != 0 && f.usages[i] < 6)
                        clear_bit(f.usages[i] - 1);
                }
            } else {
                for (unsigned int i = f.usage_min; i <= f.usage_max; i++) {
                    if (i != 0 && i < 6)
                        clear_bit(i - 1);
                }
            }
        } else if (f.usage_page == USAGE_PAGE_CONSUMER) {
            auto clear_bit = [this](uint32_t usage) {
                media_keys[usage / 64] &= ~((uint64_t)1 << (usage % 64));
            };

            if (f.num_usages > 0) {
                for (unsigned int i = 0; i < f.num_usages; i++) {
                    if (f.usages[i] < media_keys.size() * 64)
                        clear_bit(f.usages[i]);
                }
            } else {
                for (unsigned int i = f.usage_min; i <= f.usage_max; i++) {
                    if (i < media_keys.size() * 64)
                        clear_bit(i);
                }
            }
        }
    }

    for (auto le = r.fields.next; le != &r.fields; le = le->next) {
        auto& f = containing_record<usb_hid_field>(le);

        handle_input_field(f, sp);
    }

    if (r.old)
        memcpy(r.old, sp.data(), sp.size());
}

void usb_hid_device::recv_thread() {
    auto buf = (uint8_t*)malloc(packet_size);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    array<uint64_t, 4> old_keys;
    decltype(media_keys) old_media_keys;
    uint8_t old_buttons;

    old_keys[0] = old_keys[1] = old_keys[2] = old_keys[3] = 0;
    old_buttons = 0;

    for (auto& c : old_media_keys) {
        c = 0;
    }

    while (true) {
        auto recv_bytes = queue.normal_transfer(span(buf, packet_size));

        if (recv_bytes == 0)
            break;

        memset(buf + recv_bytes, 0, packet_size - recv_bytes);

        keys = old_keys;
        media_keys = old_media_keys;
        buttons = old_buttons;
        relative_x = relative_y = relative_wheel = relative_hwheel = nullopt;
        absolute_x = absolute_y = absolute_wheel = nullopt;

        for (auto le = input_reports.next; le != &input_reports; le = le->next) {
            auto& r = containing_record<usb_hid_report>(le);

            handle_input(r, span(buf, packet_size));
        }

        if (memcmp(keys.data(), old_keys.data(), keys.size() * sizeof(uint64_t))) {
            for (size_t i = 0; i < keys.size(); i++) {
                auto diff = keys[i] ^ old_keys[i];

                while (diff != 0) {
                    auto bit = (size_t)countr_zero(diff);
                    auto code = (i * 64) + bit;

                    if (keycodes[code] == evdev_key_code::KEY_RESERVED)
                        print("unrecognized keycode {:x}\n", code);
                    else {
                        if (keys[i] & (uint64_t)1 << bit)
                            post_evdev(evdev_type::EV_KEY, (evdev_code)keycodes[code], 1, true);
                        else
                            post_evdev(evdev_type::EV_KEY, (evdev_code)keycodes[code], 0, true);
                    }

                    diff &= ~((uint64_t)1 << bit);
                }
            }

            old_keys = keys;
        }

        if (memcmp(media_keys.data(), old_media_keys.data(), media_keys.size() * sizeof(uint64_t))) {
            for (size_t i = 0; i < media_keys.size(); i++) {
                auto diff = media_keys[i] ^ old_media_keys[i];

                while (diff != 0) {
                    auto bit = (size_t)countr_zero(diff);
                    auto code = (i * 64) + bit;

                    if (code >= sizeof(media_keycodes) / sizeof(*media_keycodes) ||
                        media_keycodes[code] == evdev_key_code::KEY_RESERVED) {
                            print("unrecognized media keycode {:x} ({})\n", code, media_keys[i] & (uint64_t)1 << bit);
                    } else {
                        if (media_keys[i] & (uint64_t)1 << bit)
                            post_evdev(evdev_type::EV_KEY, (evdev_code)media_keycodes[code], 1, true);
                        else
                            post_evdev(evdev_type::EV_KEY, (evdev_code)media_keycodes[code], 0, true);
                    }

                    diff &= ~((uint64_t)1 << bit);
                }
            }

            old_media_keys = media_keys;
        }

        if (buttons != old_buttons) {
            uint8_t diff = buttons ^ old_buttons;

            while (diff != 0) {
                auto bit = (size_t)countr_zero(diff);
                evdev_key_code code;

                switch (bit) {
                    case 0:
                        code = evdev_key_code::BTN_LEFT;
                    break;

                    case 1:
                        code = evdev_key_code::BTN_RIGHT;
                    break;

                    case 2:
                        code = evdev_key_code::BTN_MIDDLE;
                    break;

                    case 3:
                        code = evdev_key_code::BTN_SIDE;
                    break;

                    case 4:
                        code = evdev_key_code::BTN_EXTRA;
                    break;

                    default:
                        code = evdev_key_code::KEY_RESERVED;
                    break;
                }

                if (code != evdev_key_code::KEY_RESERVED) {
                    if (buttons & 1 << bit)
                        post_evdev(evdev_type::EV_KEY, (evdev_code)code, 1, true);
                    else
                        post_evdev(evdev_type::EV_KEY, (evdev_code)code, 0, true);
                }

                diff &= (uint8_t)~(1 << bit);
            }

            old_buttons = buttons;
        }

        if (relative_x.has_value() || relative_y.has_value() ||
            relative_wheel.has_value() || relative_hwheel.has_value() ||
            absolute_x.has_value() || absolute_y.has_value() ||
            absolute_wheel.has_value()) {
            bool done = false;

            if (relative_x.has_value()) {
                if (*relative_x != 0) {
                    post_evdev(evdev_type::EV_REL, (evdev_code)evdev_rel_code::REL_X,
                               *relative_x, false);
                    done = true;
                }
            } else if (absolute_x.has_value()) {
                post_evdev(evdev_type::EV_ABS, (evdev_code)evdev_abs_code::ABS_X,
                           *absolute_x, false);
                done = true;
            }

            if (relative_y.has_value()) {
                if (*relative_y != 0) {
                    post_evdev(evdev_type::EV_REL, (evdev_code)evdev_rel_code::REL_Y,
                               *relative_y, false);
                    done = true;
                }
            } else if (absolute_y.has_value()) {
                post_evdev(evdev_type::EV_ABS, (evdev_code)evdev_abs_code::ABS_Y,
                           *absolute_y, false);
                done = true;
            }

            if (relative_wheel.has_value()) {
                if (*relative_wheel != 0) {
                    post_evdev(evdev_type::EV_REL, (evdev_code)evdev_rel_code::REL_WHEEL,
                               *relative_wheel, false);
                    done = true;
                }
            } else if (absolute_wheel.has_value()) {
                post_evdev(evdev_type::EV_ABS, (evdev_code)evdev_abs_code::ABS_WHEEL,
                           *absolute_wheel, false);
                done = true;
            }

            if (relative_hwheel.has_value() && *relative_hwheel != 0) {
                post_evdev(evdev_type::EV_REL, (evdev_code)evdev_rel_code::REL_HWHEEL,
                            *relative_hwheel, false);
                done = true;
            }

            if (done)
                post_evdev(evdev_type::EV_SYN, (evdev_code)evdev_syn_code::SYN_REPORT, 0, false);
        }
    }

    free(buf);

    if (repeat_timer) {
        auto& t = static_cast<timer&>(repeat_timer.get());

        if (auto status = t.set(0, nullptr, nullptr, false, 0, nullptr); !success(status))
            print("timer::set returned {}\n", status);
    }
}

static void set_protocol(usb_device& d, uint16_t wValue, uint16_t wIndex) {
    d.ep0_queue.control_transfer(span<uint8_t>(),
        USB_REQUEST_TYPE_HOST_TO_DEVICE | USB_REQUEST_TYPE_CLASS | USB_REQUEST_TYPE_RECIPIENT_INTERFACE,
        (enum usb_request)usb_hid_request::SET_PROTOCOL,
        wValue, wIndex, SYSTEM_TIME_RESOLUTION / 2, true);
}

nt_status usb_hid_device::write(span<const span<const uint8_t>> v,
                                uint64_t& written_bytes) {
    if (get_vec_size(v) < sizeof(input_event))
        return nt_status::INVALID_PARAMETER;

    input_event ev;
    span<const uint8_t> top = v.front();

    memcpy_vec((uint8_t*)&ev, v, top, sizeof(input_event));

    if (ev.type != evdev_type::EV_LED)
        return nt_status::NOT_SUPPORTED;

    for (auto le = output_reports.next; le != &output_reports; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

        for (auto le2 = r.fields.next; le2 != &r.fields; le2 = le2->next) {
            auto& f = containing_record<usb_hid_field>(le2);

            if (f.usage_page != USAGE_PAGE_LEDS)
                continue;

            set_led_field(r, f, (evdev_led_code)ev.code, ev.value);
        }
    }

    written_bytes = sizeof(input_event);

    return nt_status::SUCCESS;
}

nt_status usb_hid_device::ioctl(uint32_t code, span<const uint8_t> inbuf,
                                span<uint8_t> outbuf, uintptr_t& written,
                                ACCESS_MASK) {
    switch (code) {
        case IOCTL_MOXNOX_EVDEV_GET_ABSINFO:
            return ioctl_get_absinfo(inbuf, outbuf, written);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

nt_status usb_hid_device::ioctl_get_absinfo(span<const uint8_t> inbuf,
                                            span<uint8_t> outbuf,
                                            uintptr_t& written) {
    if (inbuf.size() < sizeof(evdev_abs_code))
        return nt_status::INVALID_PARAMETER;

    if (outbuf.size() < sizeof(input_absinfo))
        return nt_status::INVALID_PARAMETER;

    auto code = *(evdev_abs_code*)inbuf.data();
    auto& ret = *(input_absinfo*)outbuf.data();

    uint32_t usage;

    switch (code) {
        case evdev_abs_code::ABS_X:
            usage = GENERIC_USAGE_X;
        break;

        case evdev_abs_code::ABS_Y:
            usage = GENERIC_USAGE_Y;
        break;

        case evdev_abs_code::ABS_WHEEL:
            usage = GENERIC_USAGE_WHEEL;
        break;

        default:
            return nt_status::NOT_FOUND;
    }

    // FIXME - should we be populating these?
    ret.value = 0;
    ret.fuzz = 0;
    ret.flat = 0;
    ret.resolution = 0;

    for (auto le = input_reports.next; le != &input_reports; le = le->next) {
        auto& r = containing_record<usb_hid_report>(le);

        for (auto le2 = r.fields.next; le2 != &r.fields; le2 = le2->next) {
            auto& f = containing_record<usb_hid_field>(le2);

            if (f.usage_page != USAGE_PAGE_GENERIC)
                continue;

            if (!f.is_variable)
                continue;

            if (f.num_usages > 0) {
                for (size_t i = 0; i < f.num_usages; i++) {
                    if (f.usages[i] == usage) {
                        ret.minimum = f.logical_min;
                        ret.maximum = f.logical_max;
                        written = sizeof(input_absinfo);
                        return nt_status::SUCCESS;
                    }
                }
            } else {
                if (usage >= f.usage_min && usage < f.usage_min + f.count) {
                    ret.minimum = f.logical_min;
                    ret.maximum = f.logical_max;
                    written = sizeof(input_absinfo);
                    return nt_status::SUCCESS;
                }
            }

        }
    }

    return nt_status::NOT_FOUND;
}

void usb_hid_device::keyboard_repeat_thread() {
    auto& t = static_cast<timer&>(repeat_timer.get());

    t.acquire();

    while (true) {
        auto status = wait_for_object(repeat_timer.get(), false);
        if (!success(status)) {
            print("wait_for_object returned {}\n", status);
            break;
        }

        if (d.dead)
            break;

        auto code = repeat_key;

        if (code == evdev_key_code::KEY_RESERVED)
            continue;

        post_evdev(evdev_type::EV_KEY, (evdev_code)code, 2, true);

        auto& t = static_cast<timer&>(repeat_timer.get());

        if (auto status = t.set(-(SYSTEM_TIME_RESOLUTION / REPEAT_RATE_HZ), nullptr,
                                nullptr, false, 0, nullptr); !success(status)) {
            print("timer::set returned {}\n", status);
            break;
        }
    }

    t.release();
    release();
}

static bool found_usb_hid_device(usb_device& d, const usb_device_config& config,
                                 const usb_interface& interface) {
    if (interface.desc.bAlternateSetting) // FIXME?
        return false;

    usb_endpoint* ep_desc = nullptr;

    // find endpoint number

    for (auto le = interface.endpoints.next; le != &interface.endpoints; le = le->next) {
        auto& e = containing_record<usb_endpoint>(le);

        // look for periodic interrupt IN endpoint

        if (e.desc.direction != USB_ENDPOINT_DIRECTION_IN)
            continue;

        if (e.desc.transfer_type != USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT)
            continue;

        if (e.desc.usage_type != USB_ENDPOINT_USAGE_TYPE_PERIODIC)
            continue;

        if (ep_desc) {
            print("found_usb_hid_device: multiple potential endpoints found\n");
            return false;
        }

        ep_desc = &e;
    }

    if (!ep_desc) {
        print("found_usb_hid_device: no potential endpoint found\n");
        return false;
    }

    if (ep_desc->desc.wMaxPacketSize == 0) {
        print("found_usb_hid_device: wMaxPacketSize was 0\n");
        return false;
    }

    if (interface.desc.bInterfaceSubClass == 1) {
        // change to report protocol
        set_protocol(d, 1, interface.desc.bInterfaceNumber);

        // may fail on some devices, plough straight on
    }

    auto queue = d.configure_endpoint(ep_desc->desc);
    if (!queue) {
        print("usb_device::configure_endpoint failed\n");
        return false;
    }

    auto dev = new usb_hid_device(d, config, *queue,
                                  ep_desc->desc.wMaxPacketSize, interface);
    if (!dev) {
        print("new failed\n");
        return false;
    }

    if (!dev->init) {
        print("usb_hid_device::usb_hid_device failed\n");
        delete dev;
        return false;
    }

    add_kernel_thread([](void* ctx) {
        auto& dev = *(usb_hid_device*)ctx;

        dev.recv_thread();
        dev.release();
    }, dev);

    return true;
}
