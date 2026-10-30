module;

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <string_view>
#include <span>
#include <memory>
#include <bit>
#include <algorithm>
#include <chrono>

export module tz;

import nt;
import ntdll;
import print;
import unique_handle;
import unicode;

using namespace std;

extern "C" {
    int _daylight;
    long _timezone;
    char* _tzname[] = { (char*)"UTC", (char*)"UTC" };
}

export RTL_SRWLOCK tz_lock;
static char tz_string[256];

static constexpr uint32_t TZIF_MAGIC = 0x66695a54; // "TZif"

struct tzif_header {
    uint32_t magic;
    uint8_t ver;
    uint8_t reserved[15];
    uint32_t isutcnt;
    uint32_t isstdcnt;
    uint32_t leapcnt;
    uint32_t timecnt;
    uint32_t typecnt;
    uint32_t charcnt;
} __attribute__((packed));

struct tzif_time_type {
    int32_t utoff;
    uint8_t dst;
    uint8_t idx;
} __attribute__((packed));

struct tz_transition {
    int64_t time;
    uint8_t type;
};

enum class tz_rule_type {
    none,
    julian_day,
    julian_day_leap,
    month,
};

static unique_ptr<tz_transition[]> tz_transitions_buf;
export span<tz_transition> tz_transitions;
static unique_ptr<tzif_time_type[]> tz_types_buf;
export span<tzif_time_type> tz_types;
export unique_ptr<char[]> tz_names;

export int32_t tz_std_offset;
static int32_t tz_dst_offset;
static tz_rule_type tz_start_rule_type, tz_end_rule_type;
static unsigned int tz_start_param1, tz_start_param2, tz_start_param3;
static unsigned int tz_end_param1, tz_end_param2, tz_end_param3;
static int32_t tz_start_time, tz_end_time;
static unique_ptr<char[]> tz_std_name, tz_dst_name;
export bool tz_valid_footer;

template<bool LARGE_HOURS>
constexpr bool try_parse_rules_offset(string_view& s, int32_t& offset) {
    bool neg = false;
    uint32_t hours, minutes, seconds;

    static constexpr uint32_t HOURS_LIMIT = LARGE_HOURS ? 167 : 24;

    if (!s.empty() && s[0] == '-') {
        neg = true;
        s = string_view(s.data() + 1, s.size() - 1);
    } else if (!s.empty() && s[0] == '+')
        s = string_view(s.data() + 1, s.size() - 1);

    if (s.empty() || s[0] < '0' || s[0] > '9')
        return false;

    // hours

    hours = (uint32_t)(s[0] - '0');
    s = string_view(s.data() + 1, s.size() - 1);

    if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
        hours *= 10;
        hours += (uint32_t)(s[0] - '0');
        s = string_view(s.data() + 1, s.size() - 1);

        if constexpr (LARGE_HOURS) {
            if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
                hours *= 10;
                hours += (uint32_t)(s[0] - '0');
                s = string_view(s.data() + 1, s.size() - 1);
            }
        }
    }

    if (s.empty() || s[0] != ':') {
        if (hours > HOURS_LIMIT)
            return false;

        offset = (int32_t)(hours * 3600);

        if (neg)
            offset = -offset;

        return true;
    }

    // minutes

    s = string_view(s.data() + 1, s.size() - 1);

    if (s.empty() || s[0] < '0' || s[0] > '9')
        return false;

    minutes = (uint32_t)(s[0] - '0');
    s = string_view(s.data() + 1, s.size() - 1);

    if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
        minutes *= 10;
        minutes += (uint32_t)(s[0] - '0');
        s = string_view(s.data() + 1, s.size() - 1);
    }

    if (s.empty() || s[0] != ':') {
        if (hours > HOURS_LIMIT || minutes > 59)
            return false;

        offset = (int32_t)((hours * 3600) + (minutes * 60));

        if (neg)
            offset = -offset;

        return true;
    }

    // seconds

    s = string_view(s.data() + 1, s.size() - 1);

    if (s.empty() || s[0] < '0' || s[0] > '9')
        return false;

    seconds = (uint32_t)(s[0] - '0');
    s = string_view(s.data() + 1, s.size() - 1);

    if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
        seconds *= 10;
        seconds += (uint32_t)(s[0] - '0');
        s = string_view(s.data() + 1, s.size() - 1);
    }

    if (hours > HOURS_LIMIT || minutes > 59 || seconds > 59)
        return false;

    offset = (int32_t)((hours * 3600) + (minutes * 60) + seconds);

    if (neg)
        offset = -offset;

    return true;
}

constexpr bool try_parse_tz_name(string_view& s, string_view& name) {
    if (s.size() < 3)
        return false;

    if (s[0] == '<') {
        if (s.size() < 5)
            return false;

        s = string_view(s.data() + 1, s.size() - 1);

        name = string_view(s.data(), min(s.size(), (size_t)TZNAME_MAX));

        for (size_t i = 0; i < name.size(); i++) {
            if ((s[i] < 'A' || s[i] > 'Z') && (s[i] < 'a' || s[i] > 'z') &&
                (s[i] < '0' || s[i] > '9') && s[i] != '-' && s[i] != '+') {
                name = string_view(name.data(), i);
                break;
            }
        }

        if (name.size() < 3)
            return false;

        s = string_view(s.data() + name.size(), s.size() - name.size());

        if (s.empty() || s[0] != '>')
            return false;

        s = string_view(s.data() + 1, s.size() - 1);
    } else {
        name = string_view(s.data(), min(s.size(), (size_t)TZNAME_MAX));

        for (size_t i = 0; i < name.size(); i++) {
            if ((name[i] < 'A' || name[i] > 'Z') && (name[i] < 'a' || name[i] > 'z')) {
                name = string_view(name.data(), i);
                break;
            }
        }

        if (name.size() < 3)
            return false;

        s = string_view(s.data() + name.size(), s.size() - name.size());
    }

    return true;
}

constexpr bool try_parse_rule(string_view& s, tz_rule_type& rule_type,
                              unsigned int& param1, unsigned int& param2,
                              unsigned int& param3, int32_t& offset) {
    if (s.empty())
        return false;

    if (s[0] == 'J') {
        unsigned int jd;

        rule_type = tz_rule_type::julian_day;

        s = string_view(s.data() + 1, s.size() - 1);

        if (s.empty() || s[0] < '0' || s[0] > '9')
            return false;

        jd = (unsigned int)(s[0] - '0');
        s = string_view(s.data() + 1, s.size() - 1);

        if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
            jd *= 10;
            jd += (unsigned int)(s[0] - '0');
            s = string_view(s.data() + 1, s.size() - 1);

            if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
                jd *= 10;
                jd += (unsigned int)(s[0] - '0');
                s = string_view(s.data() + 1, s.size() - 1);
            }
        }

        if (jd < 1 || jd > 365)
            return false;

        param1 = jd;
    } else if (s[0] >= '0' && s[0] <= '9') {
        unsigned int jd;

        rule_type = tz_rule_type::julian_day_leap;

        jd = (unsigned int)(s[0] - '0');
        s = string_view(s.data() + 1, s.size() - 1);

        if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
            jd *= 10;
            jd += (unsigned int)(s[0] - '0');
            s = string_view(s.data() + 1, s.size() - 1);

            if (!s.empty() && s[0] >= '0' && s[0] <= '9') {
                jd *= 10;
                jd += (unsigned int)(s[0] - '0');
                s = string_view(s.data() + 1, s.size() - 1);
            }
        }

        if (jd > 365)
            return false;

        param1 = jd;
    } else if (s[0] == 'M') {
        unsigned int month, week, day;

        rule_type = tz_rule_type::month;

        s = string_view(s.data() + 1, s.size() - 1);

        if (s.empty() || s[0] < '1' || s[0] > '9')
            return false;

        month = (unsigned int)(s[0] - '0');
        s = string_view(s.data() + 1, s.size() - 1);

        if (s.empty())
            return false;

        if (s[0] >= '0' && s[0] <= '9') {
            month *= 10;
            month += (unsigned int)(s[0] - '0');
            s = string_view(s.data() + 1, s.size() - 1);
        }

        if (month > 12)
            return false;

        if (s.empty() || s[0] != '.')
            return false;

        s = string_view(s.data() + 1, s.size() - 1);

        if (s.empty() || s[0] < '1' || s[0] > '5')
            return false;

        week = (unsigned int)(s[0] - '0');

        s = string_view(s.data() + 1, s.size() - 1);

        if (s.empty() || s[0] != '.')
            return false;

        s = string_view(s.data() + 1, s.size() - 1);

        if (s.empty() || s[0] < '0' || s[0] > '6')
            return false;

        day = (unsigned int)(s[0] - '0');
        s = string_view(s.data() + 1, s.size() - 1);

        param1 = month;
        param2 = week;
        param3 = day;
    } else
        return false;

    if (s.empty() || s[0] != '/')
        return true;

    s = string_view(s.data() + 1, s.size() - 1);

    return try_parse_rules_offset<true>(s, offset);
}

constexpr bool try_parse_rules_string(string_view s, string_view& std,
                                      string_view& dst, int32_t& std_offset,
                                      int32_t& dst_offset, tz_rule_type& start_rule_type,
                                      unsigned int& start_param1, unsigned int& start_param2,
                                      unsigned int& start_param3, int32_t& start_time,
                                      tz_rule_type& end_rule_type, unsigned int& end_param1,
                                      unsigned int& end_param2, unsigned int& end_param3,
                                      int32_t& end_time) {
    start_rule_type = tz_rule_type::none;
    end_rule_type = tz_rule_type::none;
    start_param1 = start_param2 = start_param3 = 0;
    end_param1 = end_param2 = end_param3 = 0;
    start_time = end_time = 7200;

    if (!try_parse_tz_name(s, std))
        return false;

    if (!try_parse_rules_offset<false>(s, std_offset))
        return false;

    if (s.empty()) {
        dst = std;
        dst_offset = std_offset;
        return true;
    }

    if (!try_parse_tz_name(s, dst))
        return false;

    dst_offset = std_offset - 3600;

    if (s.empty())
        return true;

    if (s[0] != ',') {
        if (!try_parse_rules_offset<false>(s, dst_offset))
            return false;
    }

    if (s.empty())
        return true;

    if (s[0] != ',')
        return false;

    s = string_view(s.data() + 1, s.size() - 1);

    if (s.empty())
        return false;

    if (!try_parse_rule(s, start_rule_type, start_param1, start_param2, start_param3, start_time))
        return false;

    if (s.empty() || s[0] != ',')
        return false;

    s = string_view(s.data() + 1, s.size() - 1);

    if (!try_parse_rule(s, end_rule_type, end_param1, end_param2, end_param3, end_time))
        return false;

    if (!s.empty())
        return false;

    return true;
}

constexpr bool test_rules_string(string_view s, string_view exp_std,
                                 string_view exp_dst, int32_t exp_std_offset,
                                 int32_t exp_dst_offset, tz_rule_type exp_start_rule_type,
                                 unsigned int exp_start_param1, unsigned int exp_start_param2,
                                 unsigned int exp_start_param3, int32_t exp_start_time,
                                 tz_rule_type exp_end_rule_type, unsigned int exp_end_param1,
                                 unsigned int exp_end_param2, unsigned int exp_end_param3,
                                 int32_t exp_end_time) {
    string_view std, dst;
    int32_t std_offset, dst_offset;
    tz_rule_type start_rule_type, end_rule_type;
    unsigned int start_param1, start_param2, start_param3;
    unsigned int end_param1, end_param2, end_param3;
    int32_t start_time, end_time;

    if (!try_parse_rules_string(s, std, dst, std_offset, dst_offset,
                                start_rule_type, start_param1, start_param2,
                                start_param3, start_time, end_rule_type, end_param1,
                                end_param2, end_param3, end_time)) {
        return false;
    }

    if (std != exp_std || dst != exp_dst)
        return false;

    if (std_offset != exp_std_offset || dst_offset != exp_dst_offset)
        return false;

    if (start_rule_type != exp_start_rule_type)
        return false;

    if (end_rule_type != exp_end_rule_type)
        return false;

    if (start_param1 != exp_start_param1 || start_param2 != exp_start_param2 || start_param3 != exp_start_param3)
        return false;

    if (end_param1 != exp_end_param1 || end_param2 != exp_end_param2 || end_param3 != exp_end_param3)
        return false;

    if (start_time != exp_start_time || end_time != exp_end_time)
        return false;

    return true;
}

static_assert(test_rules_string("UTC0", "UTC", "UTC", 0, 0, tz_rule_type::none, 0, 0, 0, 7200, tz_rule_type::none, 0, 0, 0, 7200));
static_assert(test_rules_string("EST5", "EST", "EST", 18000, 18000, tz_rule_type::none, 0, 0, 0, 7200, tz_rule_type::none, 0, 0, 0, 7200));
static_assert(test_rules_string("<+0330>-3:30", "+0330", "+0330", -12600, -12600, tz_rule_type::none, 0, 0, 0, 7200, tz_rule_type::none, 0, 0, 0, 7200));
static_assert(test_rules_string("<-03>3", "-03", "-03", 10800, 10800, tz_rule_type::none, 0, 0, 0, 7200, tz_rule_type::none, 0, 0, 0, 7200));
static_assert(test_rules_string("XYZ-5:45:30", "XYZ", "XYZ", -20730, -20730, tz_rule_type::none, 0, 0, 0, 7200, tz_rule_type::none, 0, 0, 0, 7200));
static_assert(test_rules_string("EST5EDT4,M3.2.0,M11.1.0", "EST", "EDT", 18000, 14400, tz_rule_type::month, 3, 2, 0, 7200, tz_rule_type::month, 11, 1, 0, 7200));
static_assert(test_rules_string("<+0930>-9:30<+1030>,M10.1.0,M4.1.0/3", "+0930", "+1030", -34200, -37800, tz_rule_type::month, 10, 1, 0, 7200, tz_rule_type::month, 4, 1, 0, 10800));
static_assert(test_rules_string("EST5EDT,M3.2.0,M11.1.0", "EST", "EDT", 18000, 14400, tz_rule_type::month, 3, 2, 0, 7200, tz_rule_type::month, 11, 1, 0, 7200));
static_assert(test_rules_string("GMT0BST,M3.5.0/1,M10.5.0", "GMT", "BST", 0, -3600, tz_rule_type::month, 3, 5, 0, 3600, tz_rule_type::month, 10, 5, 0, 7200));
static_assert(test_rules_string("CET-1CEST,M3.5.0,M10.5.0/3", "CET", "CEST", -3600, -7200, tz_rule_type::month, 3, 5, 0, 7200, tz_rule_type::month, 10, 5, 0, 10800));
static_assert(test_rules_string("AEST-10AEDT,M10.1.0,M4.1.0/3", "AEST", "AEDT", -36000, -39600, tz_rule_type::month, 10, 1, 0, 7200, tz_rule_type::month, 4, 1, 0, 10800));
static_assert(test_rules_string("NZST-12NZDT,M9.5.0,M4.1.0/3", "NZST", "NZDT", -43200, -46800, tz_rule_type::month, 9, 5, 0, 7200, tz_rule_type::month, 4, 1, 0, 10800));
static_assert(test_rules_string("IST-2IDT,M3.4.4/26,M10.5.0", "IST", "IDT", -7200, -10800, tz_rule_type::month, 3, 4, 4, 93600, tz_rule_type::month, 10, 5, 0, 7200));
static_assert(test_rules_string("WGT3WGST,M3.5.0/-2,M10.5.0/-1", "WGT", "WGST", 10800, 7200, tz_rule_type::month, 3, 5, 0, -7200, tz_rule_type::month, 10, 5, 0, -3600));
static_assert(test_rules_string("<-03>3<-02>,M3.5.0/-2,M10.5.0/-1", "-03", "-02", 10800, 7200, tz_rule_type::month, 3, 5, 0, -7200, tz_rule_type::month, 10, 5, 0, -3600));
static_assert(test_rules_string("EST5EDT,J60,J300", "EST", "EDT", 18000, 14400, tz_rule_type::julian_day, 60, 0, 0, 7200, tz_rule_type::julian_day, 300, 0, 0, 7200));
static_assert(test_rules_string("EST5EDT,60,300", "EST", "EDT", 18000, 14400, tz_rule_type::julian_day_leap, 60, 0, 0, 7200, tz_rule_type::julian_day_leap, 300, 0, 0, 7200));
static_assert(test_rules_string("EST5EDT,J59/23:59:59,J60/0:0:1", "EST", "EDT", 18000, 14400, tz_rule_type::julian_day, 59, 0, 0, 86399, tz_rule_type::julian_day, 60, 0, 0, 1));
static_assert(test_rules_string("IST-1GMT0,M10.5.0,M3.5.0/1", "IST", "GMT", -3600, 0, tz_rule_type::month, 10, 5, 0, 7200, tz_rule_type::month, 3, 5, 0, 3600));
static_assert(test_rules_string("EST5EDT,0/0,J365/25", "EST", "EDT", 18000, 14400, tz_rule_type::julian_day_leap, 0, 0, 0, 0, tz_rule_type::julian_day, 365, 0, 0, 90000));

constexpr bool is_leap_year(unsigned int year) {
    if (year % 4)
        return false;

    if (!(year % 400))
        return true;

    return year % 100;
}

constexpr clock_t find_tz_date(tz_rule_type rule_type, unsigned int param1,
                               unsigned int param2, unsigned int param3,
                               unsigned int year) {
    chrono::sys_days tp;

    switch (rule_type) {
        case tz_rule_type::julian_day: {
            chrono::year_month_day ymd{chrono::year{(int)year} / chrono::month{1} / chrono::day{1}};

            tp = ymd;
            tp += chrono::days{param1 - 1};

            if (is_leap_year(year) && param1 > 59)
                tp += chrono::days{1};

            break;
        }

        case tz_rule_type::julian_day_leap: {
            chrono::year_month_day ymd{chrono::year{(int)year} / chrono::month{1} / chrono::day{1}};

            tp = ymd;
            tp += chrono::days{param1};

            break;
        }

        case tz_rule_type::month: {
            unsigned int month, day;
            chrono::year y{(int)year};
            chrono::month m{param1};
            chrono::weekday dow{param3};

            if (param2 == 5) { // last of month
                chrono::weekday_last wl{dow[chrono::last]};
                chrono::year_month_day ymd{y / m / wl};

                month = param1;
                day = (unsigned int)ymd.day();
            } else {
                chrono::weekday_indexed wi = dow[param2];
                chrono::year_month_weekday ymwd = y / m / wi;
                chrono::year_month_day ymd{ymwd};

                month = param1;
                day = (unsigned int)ymd.day();
            }

            chrono::year_month_day ymd{chrono::year{(int)year} / chrono::month{month} / chrono::day{day}};
            tp = ymd;

            break;
        }

        case tz_rule_type::none:
            break;
    }

    return chrono::duration_cast<chrono::seconds>(tp.time_since_epoch()).count();
}

constexpr bool test_rules_dates(string_view s, unsigned int year,
                                clock_t start, clock_t end) {
    string_view std, dst;
    int32_t std_offset, dst_offset;
    tz_rule_type start_rule_type, end_rule_type;
    unsigned int start_param1, start_param2, start_param3;
    unsigned int end_param1, end_param2, end_param3;
    int32_t start_time, end_time;

    if (!try_parse_rules_string(s, std, dst, std_offset, dst_offset,
                                start_rule_type, start_param1, start_param2,
                                start_param3, start_time, end_rule_type, end_param1,
                                end_param2, end_param3, end_time)) {
        return false;
    }

    auto ts = find_tz_date(start_rule_type, start_param1, start_param2, start_param3,
                           year);

    ts += std_offset + start_time;

    auto ts2 = find_tz_date(end_rule_type, end_param1, end_param2, end_param3,
                            year);

    ts2 += dst_offset + end_time;

    return ts == start && ts2 == end;
}

static_assert(test_rules_dates("EST5EDT,M3.2.0,M11.1.0", 2025, 1741503600, 1762063200));
static_assert(test_rules_dates("GMT0BST,M3.5.0/1,M10.5.0", 2025, 1743296400, 1761440400));
static_assert(test_rules_dates("CET-1CEST,M3.5.0,M10.5.0/3", 2025, 1743296400, 1761440400));
static_assert(test_rules_dates("AEST-10AEDT,M10.1.0,M4.1.0/3", 2025, 1759593600, 1743868800));
static_assert(test_rules_dates("NZST-12NZDT,M9.5.0,M4.1.0/3", 2025, 1758981600, 1743861600));
static_assert(test_rules_dates("IST-2IDT,M3.4.4/26,M10.5.0", 2025, 1743120000, 1761433200));
static_assert(test_rules_dates("WGT3WGST,M3.5.0/-2,M10.5.0/-1", 2025, 1743296400, 1761440400));
static_assert(test_rules_dates("EST5EDT,J60,J300", 2025, 1740812400, 1761544800));
static_assert(test_rules_dates("EST5EDT,60,300", 2024, 1709276400, 1730008800));
static_assert(test_rules_dates("EST5EDT,60,300", 2025, 1740898800, 1761631200));

static bool parse_tzif(span<const uint8_t> data, u16string_view fn) {
    static constexpr size_t TIME_SIZE1 = sizeof(int32_t);
    static constexpr size_t TIME_SIZE = sizeof(int64_t);

    size_t ver1_size;

    {
        auto& h1 = *(tzif_header*)data.data();

        if (h1.magic != TZIF_MAGIC) {
            print_stderr("TZ magic for {} was {:08x}, not {:08x} as expected\n",
                        fn, +h1.magic, TZIF_MAGIC);
            return false;
        }

        if (h1.ver != '2' && h1.ver != '3' && h1.ver != '4') {
            print_stderr("{} had unsupported version value {:02x}\n", fn, +h1.ver);
            return false;
        }

        auto isutcnt1 = byteswap(h1.isutcnt);
        auto isstdcnt1 = byteswap(h1.isstdcnt);
        auto leapcnt1 = byteswap(h1.leapcnt);
        auto timecnt1 = byteswap(h1.timecnt);
        auto typecnt1 = byteswap(h1.typecnt);
        auto charcnt1 = byteswap(h1.charcnt);

        ver1_size = sizeof(tzif_header) + (timecnt1 * TIME_SIZE1) + timecnt1 +
                    (typecnt1 * sizeof(tzif_time_type)) + charcnt1 +
                    (leapcnt1 * (TIME_SIZE1 + 4)) + isstdcnt1 + isutcnt1;

        if (data.size() < ver1_size + sizeof(tzif_header)) {
            print_stderr("{} was {} bytes, not at least {} as expected\n",
                        fn, data.size(), ver1_size + sizeof(tzif_header));
            return false;
        }
    }

    auto& h = *(tzif_header*)(data.data() + ver1_size);

    if (h.magic != TZIF_MAGIC) {
        print_stderr("TZ magic for {} was {:08x}, not {:08x} as expected\n",
                    fn, +h.magic, TZIF_MAGIC);
        return false;
    }

    if (h.ver != '2' && h.ver != '3' && h.ver != '4') {
        print_stderr("{} had unsupported version value {:02x}\n", fn, +h.ver);
        return false;
    }

    auto isutcnt = byteswap(h.isutcnt);
    auto isstdcnt = byteswap(h.isstdcnt);
    auto leapcnt = byteswap(h.leapcnt);
    auto timecnt = byteswap(h.timecnt);
    auto typecnt = byteswap(h.typecnt);
    auto charcnt = byteswap(h.charcnt);

    size_t ver2_size = sizeof(tzif_header) + (timecnt * TIME_SIZE) + timecnt +
                       (typecnt * sizeof(tzif_time_type)) + charcnt +
                       (leapcnt * (TIME_SIZE + 4)) + isstdcnt + isutcnt;

    if (data.size() < ver1_size + ver2_size) {
        print_stderr("{} was {} bytes, not at least {} as expected\n",
                    fn, data.size(), ver1_size + ver2_size);
        return false;
    }

    if (typecnt == 0) {
        print_stderr("{}: typecnt was 0\n", fn);
        return false;
    }

    if (charcnt == 0) {
        print_stderr("{}: charcnt was 0\n", fn);
        return false;
    }

    // read transitions times and type indexes

    unique_ptr<tz_transition[]> tztt;

    if (timecnt > 0) {
        tztt.reset(new tz_transition[timecnt]);
        if (!tztt) {
            print_stderr("new failed\n");
            return false;
        }

        auto times = (int64_t*)((uint8_t*)&h + sizeof(tzif_header));
        auto types = (uint8_t*)times + (sizeof(int64_t) * timecnt);

        for (size_t i = 0; i < timecnt; i++) {
            tztt[i].time = byteswap(times[i]);
            tztt[i].type = types[i];

            if (i != 0 && tztt[i].time <= tztt[i - 1].time) {
                print_stderr("{}: transition times are out of order\n", fn);
                return false;
            }

            if (types[i] >= typecnt) {
                print_stderr("{}: invalid transition type {}\n",
                             fn, types[i]);
                return false;
            }
        }
    }

    // read transition type records

    unique_ptr<tzif_time_type[]> tztt2;

    tztt2.reset(new tzif_time_type[typecnt]);
    if (!tztt2) {
        print_stderr("new failed\n");
        return false;
    }

    memcpy(tztt2.get(), (uint8_t*)&h + sizeof(tzif_header) + ((sizeof(int64_t) + 1) * timecnt),
           sizeof(tzif_time_type) * typecnt);

    for (size_t i = 0; i < typecnt; i++) {
        tztt2[i].utoff = byteswap(tztt2[i].utoff);

        if (tztt2[i].idx >= charcnt) {
            print_stderr("{}: type {} had invalid idx {}\n", fn, i, tztt2[i].idx);
            return false;
        }
    }

    // read time zone designations

    unique_ptr<char[]> names;

    names.reset(new char[charcnt]);
    if (!names) {
        print_stderr("new failed\n");
        return false;
    }

    memcpy(names.get(),
           (uint8_t*)&h + sizeof(tzif_header) + ((sizeof(int64_t) + 1) * timecnt) + (sizeof(tzif_time_type) * typecnt),
           charcnt);

    if (names[charcnt - 1] != 0) {
        print_stderr("{}: designation strings did not end with 0\n", fn);
        return false;
    }

    // read footer

    span footer(data.data() + ver1_size + ver2_size, data.size() - ver1_size - ver2_size);

    if (footer.size() >= 3 && footer[0] == '\n' && footer[footer.size() - 1] == '\n') {
        string_view std, dst;
        int32_t std_offset, dst_offset;
        tz_rule_type start_rule_type, end_rule_type;
        unsigned int start_param1, start_param2, start_param3;
        unsigned int end_param1, end_param2, end_param3;
        int32_t start_time, end_time;
        unique_ptr<char[]> std_name, dst_name;

        if (try_parse_rules_string(string_view((char*)footer.data() + 1, footer.size() - 2),
                                   std, dst, std_offset, dst_offset,
                                   start_rule_type, start_param1, start_param2,
                                   start_param3, start_time, end_rule_type,
                                   end_param1, end_param2, end_param3, end_time)) {
            std_name.reset(new char[std.size() + 1]);
            if (!std_name) {
                print_stderr("new failed\n");
                return false;
            }

            dst_name.reset(new char[dst.size() + 1]);
            if (!dst_name) {
                print_stderr("new failed\n");
                return false;
            }

            memcpy(std_name.get(), std.data(), std.size());
            std_name[std.size()] = 0;

            memcpy(dst_name.get(), dst.data(), dst.size());
            dst_name[dst.size()] = 0;

            tz_std_offset = std_offset;
            tz_dst_offset = dst_offset;
            tz_start_rule_type = start_rule_type;
            tz_end_rule_type = end_rule_type;
            tz_start_param1 = start_param1;
            tz_start_param2 = start_param2;
            tz_start_param3 = start_param3;
            tz_end_param1 = end_param1;
            tz_end_param2 = end_param2;
            tz_end_param3 = end_param3;
            tz_start_time = start_time;
            tz_end_time = end_time;

            swap(tz_std_name, std_name);
            swap(tz_dst_name, dst_name);

            tz_valid_footer = true;
        } else {
            tz_std_name.reset();
            tz_dst_name.reset();

            tz_std_offset = tz_dst_offset = 0;
            tz_start_rule_type = tz_rule_type::none;
            tz_end_rule_type = tz_rule_type::none;
            tz_valid_footer = false;
        }
    } else {
        tz_std_name.reset();
        tz_dst_name.reset();

        tz_std_offset = tz_dst_offset = 0;
        tz_start_rule_type = tz_rule_type::none;
        tz_end_rule_type = tz_rule_type::none;
        tz_valid_footer = false;
    }

    // replace existing values

    swap(tz_transitions_buf, tztt);
    tz_transitions = span(tz_transitions_buf.get(), timecnt);

    swap(tz_types_buf, tztt2);
    tz_types = span(tz_types_buf.get(), typecnt);

    swap(tz_names, names);

    // FIXME - derive _timezone and _tzname from footer
    // FIXME - set _daylight
    // FIXME - copy over _daylight, _timezone, and _tzname to mnposix counterparts

    return true;
}

static bool load_tz_file(string_view fn) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    nt_status status;
    HANDLE h;
    unique_handle file, sect;
    IO_STATUS_BLOCK iosb;
    FILE_STANDARD_INFORMATION fsi;
    unique_ptr<char16_t[]> buf;
    void* addr;
    size_t size;

    static const u16string_view path = u"/share/zoneinfo/";

    if (fn[0] == ':') {
        fn = string_view(fn.data() + 1, fn.size() - 1);
        if (fn.empty())
            return false;
    }

    size_t prefix_size = fn[0] == '/' ? 0 : path.size();
    auto u16len = utf8_to_utf16_len(fn);

    buf.reset(new char16_t[prefix_size + u16len]);
    if (!buf) {
        print_stderr("malloc failed\n");
        return false;
    }

    if (fn[0] != '/')
        memcpy(buf.get(), path.data(), path.size() * sizeof(char16_t));

    utf8_to_utf16(span(buf.get() + prefix_size, u16len), fn);

    auto tzfn = u16string_view(buf.get(), prefix_size + u16len);

    us.Length = us.MaximumLength = (uint16_t)(tzfn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)tzfn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_NON_DIRECTORY_FILE, nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND || status == nt_status::OBJECT_PATH_NOT_FOUND)
        return false;
    else if (!success(status)) {
        print_stderr("NtCreateFile returned {} for {}\n", status, tzfn);
        return false;
    }

    file.reset(h);

    status = NtQueryInformationFile(file.get(), &iosb, &fsi, sizeof(fsi),
                                    FILE_INFORMATION_CLASS::StandardInformation);
    if (!success(status)) {
        print_stderr("NtQueryInformationFile returned {} for {}\n", status, tzfn);
        return false;
    }

    if (fsi.EndOfFile.QuadPart < (int64_t)sizeof(tzif_header)) {
        print_stderr("size of {} was {}, expected at least {}\n",
                     tzfn, fsi.EndOfFile.QuadPart, sizeof(tzif_header));
        return false;
    }

    status = NtCreateSection(&h, SECTION_MAP_READ, nullptr, nullptr,
                             PAGE_READONLY, SEC_COMMIT, file.get());
    if (!success(status)) {
        print_stderr("NtCreateSection returned {} for {}\n", status, tzfn);
        return false;
    }

    sect.reset(h);

    addr = nullptr;
    size = 0;

    status = NtMapViewOfSection(sect.get(), NtCurrentProcess(), &addr, 0, 0,
                                nullptr, &size, SECTION_INHERIT::ViewUnmap, 0,
                                PAGE_READONLY);
    if (!success(status)) {
        print_stderr("NtMapViewOfSection returned {} for {}\n", status, tzfn);
        return false;
    }

    auto ret = parse_tzif(span((uint8_t*)addr, (size_t)fsi.EndOfFile.QuadPart),
                          tzfn);

    status = NtUnmapViewOfSection(NtCurrentProcess(), addr);
    if (!success(status))
        print_stderr("NtUnmapViewOfSection returned {} for {}\n", status, tzfn);

    return ret;
}

export extern "C"
void _tzset() {
    RtlAcquireSRWLockExclusive(&tz_lock);

    auto tz = getenv("TZ");
    auto len = tz ? strlen(tz) : 0;

    if (!tz || !strcmp(tz, "") || len >= sizeof(tz_string)) {
        if (strcmp(tz_string, "")) {
            tz_string[0] = 0;

            _daylight = 0;
            _timezone = 0;
            _tzname[0] = (char*)"UTC";
            _tzname[1] = (char*)"UTC";

            tz_transitions_buf.reset();
            tz_types_buf.reset();
            tz_names.reset();
            tz_transitions = span<tz_transition>();
            tz_types = span<tzif_time_type>();

            tz_std_offset = 0;
            tz_dst_offset = 0;
            tz_start_rule_type = tz_rule_type::none;
            tz_end_rule_type = tz_rule_type::none;
            tz_std_name.reset();
            tz_dst_name.reset();
        }

        RtlReleaseSRWLockExclusive(&tz_lock);

        return;
    }

    if (!strcmp(tz_string, tz)) {
        RtlReleaseSRWLockExclusive(&tz_lock);
        return;
    }

    if (load_tz_file(tz)) {
        strcpy(tz_string, tz);
        RtlReleaseSRWLockExclusive(&tz_lock);
        return;
    }

    {
        string_view std, dst;
        int32_t std_offset, dst_offset;
        tz_rule_type start_rule_type, end_rule_type;
        unsigned int start_param1, start_param2, start_param3;
        unsigned int end_param1, end_param2, end_param3;
        int32_t start_time, end_time;
        unique_ptr<char[]> std_name, dst_name;

        if (try_parse_rules_string(tz, std, dst, std_offset, dst_offset,
                                   start_rule_type, start_param1, start_param2,
                                   start_param3, start_time, end_rule_type, end_param1,
                                   end_param2, end_param3, end_time)) {
            strcpy(tz_string, tz);

            std_name.reset(new char[std.size() + 1]);
            if (!std_name) {
                print_stderr("new failed\n");
                RtlReleaseSRWLockExclusive(&tz_lock);
                return;
            }

            dst_name.reset(new char[dst.size() + 1]);
            if (!dst_name) {
                print_stderr("new failed\n");
                RtlReleaseSRWLockExclusive(&tz_lock);
                return;
            }

            memcpy(std_name.get(), std.data(), std.size());
            std_name[std.size()] = 0;

            memcpy(dst_name.get(), dst.data(), dst.size());
            dst_name[dst.size()] = 0;

            // FIXME - _tzname[0], _tzname[1], _daylight, _timezone

            tz_transitions_buf.reset();
            tz_types_buf.reset();
            tz_names.reset();
            tz_transitions = span<tz_transition>();
            tz_types = span<tzif_time_type>();

            tz_std_offset = std_offset;
            tz_dst_offset = dst_offset;
            tz_start_rule_type = start_rule_type;
            tz_end_rule_type = end_rule_type;
            tz_start_param1 = start_param1;
            tz_start_param2 = start_param2;
            tz_start_param3 = start_param3;
            tz_end_param1 = end_param1;
            tz_end_param2 = end_param2;
            tz_end_param3 = end_param3;
            tz_start_time = start_time;
            tz_end_time = end_time;

            swap(tz_std_name, std_name);
            swap(tz_dst_name, dst_name);

            RtlReleaseSRWLockExclusive(&tz_lock);
            return;
        }
    }

    tz_string[0] = 0;

    _daylight = 0;
    _timezone = 0;
    _tzname[0] = (char*)"UTC";
    _tzname[1] = (char*)"UTC";

    tz_transitions_buf.reset();
    tz_types_buf.reset();
    tz_names.reset();
    tz_transitions = span<tz_transition>();
    tz_types = span<tzif_time_type>();

    tz_std_offset = 0;
    tz_dst_offset = 0;
    tz_start_rule_type = tz_rule_type::none;
    tz_end_rule_type = tz_rule_type::none;
    tz_std_name.reset();
    tz_dst_name.reset();

    RtlReleaseSRWLockExclusive(&tz_lock);
}

extern "C"
long* ____timezone() {
    return &_timezone;
}

export void tz_adjust_by_rules(__time64_t& v, struct ::tm& tm) {
    if (tz_start_rule_type == tz_rule_type::none || tz_end_rule_type == tz_rule_type::none) {
        v -= tz_std_offset;

        tm.tm_isdst = 0;
        tm.tm_zone = tz_std_name.get();
        tm.tm_gmtoff = -tz_std_offset;

        return;
    }

    auto ts = find_tz_date(tz_start_rule_type, tz_start_param1, tz_start_param2,
                           tz_start_param3, (unsigned int)tm.tm_year + 1900);

    ts += tz_std_offset + tz_start_time;

    auto ts2 = find_tz_date(tz_end_rule_type, tz_end_param1, tz_end_param2,
                            tz_end_param3, (unsigned int)tm.tm_year + 1900);

    ts2 += tz_dst_offset + tz_end_time;

    if ((ts < ts2 && v >= ts && v < ts2) || (ts2 < ts && (v >= ts || v < ts2))) {
        v -= tz_dst_offset;
        tm.tm_isdst = 1;
        tm.tm_zone = tz_dst_name.get();
        tm.tm_gmtoff = -tz_dst_offset;
    } else {
        v -= tz_std_offset;
        tm.tm_isdst = 0;
        tm.tm_zone = tz_std_name.get();
        tm.tm_gmtoff = -tz_std_offset;
    }
}
