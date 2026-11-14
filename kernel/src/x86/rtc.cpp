// SPDX-License-Identifier: GPL-2.0-only
module;

#include <optional>
#include <chrono>

export module rtc;

import inout;
import timer;
import acpi;
import list;
import print;
import module_init;
import disable_interrupts;
import efi;
import kernel_time_page;

using namespace std;

static void found_rtc(const acpi_device& d);

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "PNP0B00", found_rtc, nullptr }
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_rtc(init_acpi_hids);

export optional<uint16_t> rtc_port;

template<unsigned N>
static uint8_t get_rtc_register(uint16_t port) {
    outb(port, N);

    return inb(port + 1);
}

template<unsigned N>
static void set_rtc_register(uint16_t port, uint8_t value) {
    outb(port, N);

    outb(port + 1, value);
}

static bool get_update_in_progress(uint16_t port) {
    return get_rtc_register<0xa>(port) & 0x80;
}

static uint8_t to_bcd(uint8_t v) {
    return ((v / 10) << 4) | (v % 10);
}

export void set_rtc(uint16_t port, int year, uint8_t month, uint8_t day,
                    uint8_t hour, uint8_t minute, uint8_t second) {
    if (year < 1970 || year > 2069) {
        print("set_rtc: year {} out of range\n", year);
        return;
    }

    year %= 100;

    // FIXME - weekday register?
    // FIXME - century register?

    disable_interrupts();

    auto register_b = get_rtc_register<0x0b>(port);

    if (!(register_b & 0x02)) { // 12-hour clock
        if (hour == 0)
            hour = 12;
        else if (hour == 12)
            hour = 0x80 | 12;
        else if (hour > 12)
            hour = 0x80 | (hour - 12);
    }

    if (!(register_b & 0x04)) { // BCD
        second = to_bcd(second);
        minute = to_bcd(minute);
        hour = (hour & 0x80) | to_bcd(hour & 0x7f);
        day = to_bcd(day);
        month = to_bcd(month);
        year = to_bcd((uint8_t)year);
    }

    set_rtc_register<0xb>(port, register_b | 0x80); // halt update cycle

    set_rtc_register<0x00>(port, second);
    set_rtc_register<0x02>(port, minute);
    set_rtc_register<0x04>(port, hour);
    set_rtc_register<0x07>(port, day);
    set_rtc_register<0x08>(port, month);
    set_rtc_register<0x09>(port, (uint8_t)year);

    set_rtc_register<0xb>(port, register_b); // resume update cycle

    enable_interrupts();
}

static void init_rtc(uint16_t port) {
    struct ymdhms {
        int year;
        uint8_t month;
        uint8_t day;
        uint8_t hour;
        uint8_t minute;
        uint8_t second;

        bool operator==(const ymdhms& that) const noexcept {
            return year == that.year && month == that.month &&
                   day == that.day && hour == that.hour &&
                   minute == that.minute && second == that.second;
        }
    };

    ymdhms now, last;

    disable_interrupts();

    while (get_update_in_progress(port)) { }

    now.second = get_rtc_register<0x00>(port);
    now.minute = get_rtc_register<0x02>(port);
    now.hour = get_rtc_register<0x04>(port);
    now.day = get_rtc_register<0x07>(port);
    now.month = get_rtc_register<0x08>(port);
    now.year = get_rtc_register<0x09>(port);

    do {
        last = now;

        while (get_update_in_progress(port)) { }

        now.second = get_rtc_register<0x00>(port);
        now.minute = get_rtc_register<0x02>(port);
        now.hour = get_rtc_register<0x04>(port);
        now.day = get_rtc_register<0x07>(port);
        now.month = get_rtc_register<0x08>(port);
        now.year = get_rtc_register<0x09>(port);
    } while (last != now);

    auto register_b = get_rtc_register<0x0b>(port);

    enable_interrupts();

    if (!(register_b & 0x04)) {
        now.second = (now.second & 0x0f) + (uint8_t)((now.second >> 4) * 10);
        now.minute = (now.minute & 0x0f) + (uint8_t)((now.minute >> 4) * 10);
        now.hour = ((now.hour & 0x0f) + (uint8_t)(((now.hour & 0x70) >> 4) * 10)) | (now.hour & 0x80);
        now.day = (now.day & 0x0f) + (uint8_t)((now.day >> 4) * 10);
        now.month = (now.month & 0x0f) + (uint8_t)((now.month >> 4) * 10);
        now.year = (now.year & 0x0f) + (uint8_t)((now.year >> 4) * 10);
    }

    if (!(register_b & 0x02)) {
        bool afternoon = now.hour & 0x80;

        now.hour &= 0x7f;

        if (now.hour == 12)
            now.hour = 0;

        if (afternoon)
            now.hour += 12;
    }

    if (now.year >= 70)
        now.year += 1900;
    else
        now.year += 2000;

    auto ymd = chrono::year_month_day{chrono::year{now.year}, chrono::month{now.month},
                                      chrono::day{now.day}};

    auto day_num = ymd_to_num(ymd);

    kernel_time_page.system_time_offset = (uint64_t)((((day_num * 86400) + (now.hour * 3600) + (now.minute * 60) + now.second) * 10000000) - kernel_time_page.system_time);

    rtc_port = port;
}

static void found_rtc(const acpi_device& d) {
    optional<uint16_t> port;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        if (r.type != acpi_device_resource_type::io)
            continue;

        auto& io = *reinterpret_cast<acpi_device_resource_io*>(&r);

        port = io.io.Minimum;
        break;
    }

    if (!port.has_value()) {
        print("no I/O port found for RTC\n");
        return;
    }

    init_rtc(*port);
}
