// SPDX-License-Identifier: LGPL-2.1-or-later
module;

#include <time.h>
#include <stdint.h>
#include <string.h>
#include <span>
#include <string_view>
#include <charconv>
#include <array>
#include <optional>

export module time;

import nt;
import ntdll;
import ucrtbase;
import unicode;
import tz;

using namespace std;

static const unsigned int SYSTEM_TIME_RESOLUTION = 10000000; // 100ns

static uint64_t init_time;

export void init_clock() {
    LARGE_INTEGER now;

    NtQuerySystemTime(&now);

    init_time = (uint64_t)now.QuadPart;
}

export __time64_t win_to_unix_time(int64_t t) {
    return (__time64_t)((t - 116444736000000000) / 10000000);
}

extern "C"
clock_t clock() {
    LARGE_INTEGER now;

    NtQuerySystemTime(&now);

    // CLOCKS_PER_SEC is different on Windows

    return (clock_t)(((uint64_t)now.QuadPart - init_time) / (SYSTEM_TIME_RESOLUTION / CLOCKS_PER_SEC));
}

extern "C"
__time64_t __time64(__time64_t* t) {
    LARGE_INTEGER now;
    __time64_t ret;

    NtQuerySystemTime(&now);

    ret = win_to_unix_time(now.QuadPart);

    if (t)
        *t = ret;

    return ret;
}

extern "C"
__time32_t __time32(__time32_t* t) {
    auto ret = (__time32_t)__time64(nullptr);

    if (t)
        *t = ret;

    return ret;
}

constexpr errno_t constexpr__gmtime64_s(tm* tm, const __time64_t& v) {
    auto secs = v % 86400;
    auto days = v / 86400;

    if (secs < 0) {
        secs += 86400;
        days--;
    }

    tm->tm_sec = (int)(secs % 60);
    tm->tm_min = (int)((secs % 3600) / 60);
    tm->tm_hour = (int)(secs / 3600);

    tm->tm_wday = (int)(((days + 4) % 7) + 7) % 7;

    days -= 11017; // shift epoch to March 1, 2000

    int64_t era = (days >= 0 ? days : (days - 146096)) / 146097;
    auto day_of_era = (unsigned int)(days - (era * 146097));
    unsigned int year_of_era = (day_of_era - (day_of_era / 1460) + (day_of_era / 36524) - (day_of_era / 146096)) / 365;
    unsigned int day_of_year = day_of_era - ((365 * year_of_era) + (year_of_era / 4) - (year_of_era / 100));
    unsigned int month_index = ((5 * day_of_year) + 2) / 153;

    tm->tm_mday = (int)(day_of_year - (((153 * month_index) + 2) / 5) + 1);
    tm->tm_mon = (int)((month_index < 10) ? (month_index + 2) : (month_index - 10));
    tm->tm_year = (int)(year_of_era + (era * 400) + 100);

    if (tm->tm_mon < 2)
        tm->tm_year++;

    bool leap = (year_of_era % 4 == 0 && (year_of_era % 100 != 0 || year_of_era % 400 == 0));

    tm->tm_yday = (month_index < 10) ? (int)(day_of_year + 59 + leap) : (int)(day_of_year - 306);

    return 0;
}

extern "C"
errno_t __gmtime64_s(struct tm* tm, const __time64_t* t) {
    auto ret = constexpr__gmtime64_s(tm, *t);

    tm->tm_isdst = 0;
    tm->tm_gmtoff = 0;
    tm->tm_zone = "UTC";

    return ret;
}

static constexpr bool test_gmtime(int64_t v, int exp_wday, int exp_mday,
                                  int exp_mon, int exp_year, int exp_yday) {
    struct tm tm;

    constexpr__gmtime64_s(&tm, (__time64_t)v);

    return tm.tm_wday == exp_wday && tm.tm_mday == exp_mday &&
           tm.tm_mon == exp_mon && tm.tm_year == exp_year &&
           tm.tm_yday == exp_yday;
}

static_assert(test_gmtime(-604800, 4, 25, 11, 69, 358));
static_assert(test_gmtime(-86400, 3, 31, 11, 69, 364));
static_assert(test_gmtime(-1, 3, 31, 11, 69, 364));
static_assert(test_gmtime(0, 4, 1, 0, 70, 0));
static_assert(test_gmtime(1781027287, 2, 9, 5, 126, 159));

extern "C"
struct tm* __gmtime64(const __time64_t* t) {
    static struct tm tm;

    auto ret = __gmtime64_s(&tm, t);
    if (ret != 0)
        return nullptr;

    return &tm;
}

template<typename T, size_t N>
bool strftime_index(span<T>& sp, int num, const array<basic_string_view<T>, N>& str) {
    if (num < 0 || (size_t)num >= str.size()) {
        if (sp.empty())
            return false;

        sp[0] = '?';
        sp = span(sp.data() + 1, sp.size() - 1);

        return true;
    }

    auto s = str[(size_t)num];

    if (sp.size() < s.size())
        return false;

    memcpy(sp.data(), s.data(), s.size() * sizeof(T));
    sp = span(sp.data() + s.size(), sp.size() - s.size());

    return true;
}

template<typename T>
bool strftime_abbrev_weekday(span<T>& sp, int wday);

template<>
bool strftime_abbrev_weekday<char>(span<char>& sp, int wday) {
    static const array<string_view, 7> str = { "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat" };

    return strftime_index(sp, wday, str);
}

template<>
bool strftime_abbrev_weekday<wchar_t>(span<wchar_t>& sp, int wday) {
    static const array<wstring_view, 7> str = { L"Sun", L"Mon", L"Tue", L"Wed", L"Thu", L"Fri", L"Sat" };

    return strftime_index(sp, wday, str);
}

template<typename T>
bool strftime_weekday(span<T>& sp, int wday);

template<>
bool strftime_weekday(span<char>& sp, int wday) {
    static const array<string_view, 7> str = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

    return strftime_index(sp, wday, str);
}

template<>
bool strftime_weekday(span<wchar_t>& sp, int wday) {
    static const array<wstring_view, 7> str = { L"Sunday", L"Monday", L"Tuesday", L"Wednesday", L"Thursday", L"Friday", L"Saturday" };

    return strftime_index(sp, wday, str);
}

template<typename T>
bool strftime_abbrev_month_name(span<T>& sp, int mon);

template<>
bool strftime_abbrev_month_name<char>(span<char>& sp, int mon) {
    static const array<string_view, 12> str = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };

    return strftime_index(sp, mon, str);
}

template<>
bool strftime_abbrev_month_name<wchar_t>(span<wchar_t>& sp, int mon) {
    static const array<wstring_view, 12> str = { L"Jan", L"Feb", L"Mar", L"Apr", L"May", L"Jun", L"Jul", L"Aug", L"Sep", L"Oct", L"Nov", L"Dec" };

    return strftime_index(sp, mon, str);
}

template<typename T>
bool strftime_month_name(span<T>& sp, int mon);

template<>
bool strftime_month_name<char>(span<char>& sp, int mon) {
    static const array<string_view, 12> str = { "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December" };

    return strftime_index(sp, mon, str);
}

template<>
bool strftime_month_name<wchar_t>(span<wchar_t>& sp, int mon) {
    static const array<wstring_view, 12> str = { L"January", L"February", L"March", L"April", L"May", L"June", L"July", L"August", L"September", L"October", L"November", L"December" };

    return strftime_index(sp, mon, str);
}

template<typename T>
span<T> sp_itoa(span<T> dest, uint64_t num) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0) {
        dest[0] = '0';
        return span(dest.data() + 1, dest.size() - 1);
    } else {
        uint64_t place = 10000000000000000000ull;

        for (unsigned int i = 0; i < 20; i++) {
            if (!first || num >= place) {
                uint64_t digit = num / place;

                dest[0] = (T)(digit + '0');

                if (first && digit != 0)
                    first = false;

                dest = span(dest.data() + 1, dest.size() - 1);

                if (dest.empty())
                    return dest;

                num %= place;
            }

            place /= 10;
        }
    }

    return dest;
}

template<typename T, unsigned int N>
bool strftime_number(span<T>& sp, int num) {
    if (num < 0) { // FIXME - can have negative years?
        if (sp.empty())
            return false;

        sp[0] = '?';
        sp = span(sp.data() + 1, sp.size() - 1);

        return true;
    }

    if (num == 0) {
        if (sp.size() < N)
            return false;

        for (unsigned int i = 0; i < N; i++) {
            sp[i] = '0';
        }

        sp = span(sp.data() + N, sp.size() - N);
        return true;
    }

    if (N > 1) {
        unsigned int limit = 1;

        for (unsigned int i = 1; i < N; i++) {
            limit *= 10;
        }

        while ((unsigned int)num < limit) {
            if (sp.empty())
                return false;

            sp[0] = '0';
            sp = span(sp.data() + 1, sp.size() - 1);

            if (limit == 10)
                break;

            limit /= 10;
        }
    }

    sp = sp_itoa<T>(sp, num);

    return true;
}

template<typename T>
bool strftime_ampm(span<T>& sp, int hour);

template<>
bool strftime_ampm<char>(span<char>& sp, int hour) {
    static const string_view AM = "am";
    static const string_view PM = "pm";

    if (hour < 12) {
        if (sp.size() < AM.size())
            return false;

        memcpy(sp.data(), AM.data(), AM.size());
        sp = span(sp.data() + AM.size(), sp.size() - AM.size());

        return true;
    } else {
        if (sp.size() < PM.size())
            return false;

        memcpy(sp.data(), PM.data(), PM.size());
        sp = span(sp.data() + PM.size(), sp.size() - PM.size());

        return true;
    }
}

template<>
bool strftime_ampm<wchar_t>(span<wchar_t>& sp, int hour) {
    static const wstring_view AM = L"am";
    static const wstring_view PM = L"pm";

    if (hour < 12) {
        if (sp.size() < AM.size())
            return false;

        memcpy(sp.data(), AM.data(), AM.size() * sizeof(wchar_t));
        sp = span(sp.data() + AM.size(), sp.size() - AM.size());

        return true;
    } else {
        if (sp.size() < PM.size())
            return false;

        memcpy(sp.data(), PM.data(), PM.size() * sizeof(wchar_t));
        sp = span(sp.data() + PM.size(), sp.size() - PM.size());

        return true;
    }
}

static constexpr unsigned int days_in_year(int year) {
    if (year % 4)
        return 365;
    else if (!(year % 400))
        return 366;
    else if (!(year % 100))
        return 365;
    else
        return 366;
}

template<typename T>
static size_t do_strftime(T* s, size_t max, const T* format, const struct ::tm* tm) {
    auto fmt = basic_string_view<T>(format);
    auto sp = span(s, max);

    while (true) {
        auto pc = fmt.find('%');

        if (pc == basic_string_view<T>::npos)
            pc = fmt.size();

        if (pc > 0) {
            if (sp.size() < pc)
                return 0;

            memcpy(sp.data(), fmt.data(), pc * sizeof(T));

            sp = span(sp.data() + pc, sp.size() - pc);
            fmt = basic_string_view<T>(fmt.data() + pc, fmt.size() - pc);

            if (fmt.empty())
                break;
        }

        // skip %
        fmt = basic_string_view<T>(fmt.data() + 1, fmt.size() - 1);

        if (fmt.empty())
            break;

        switch (fmt[0]) {
            case 'a': // abbreviated weekday
                if (!strftime_abbrev_weekday<T>(sp, tm->tm_wday))
                    return 0;
            break;

            case 'A': // weekday
                if (!strftime_weekday<T>(sp, tm->tm_wday))
                    return 0;
            break;

            case 'b': // abbreviated month name
            case 'h':
                if (!strftime_abbrev_month_name<T>(sp, tm->tm_mon))
                    return 0;
            break;

            case 'B': // month name
                if (!strftime_month_name<T>(sp, tm->tm_mon))
                    return 0;
            break;

            case 'c': { // "locale's appropriate date and time representation"
                size_t ret;

                if constexpr (is_same_v<T, char>)
                    ret = do_strftime<T>(sp.data(), sp.size(), "%a %b %e %H:%M:%S %Y", tm);
                else
                    ret = do_strftime<T>(sp.data(), sp.size(), L"%a %b %e %H:%M:%S %Y", tm);

                if (ret == 0)
                    return 0;

                sp = span(sp.data() + ret, sp.size() - ret);

                break;
            }

            case 'C': // year / 100
                if (!strftime_number<T, 2>(sp, (tm->tm_year + 1900) / 100))
                    return 0;
            break;

            case 'd': // day of month as decimal number
                if (!strftime_number<T, 2>(sp, tm->tm_mday))
                    return 0;
            break;

            case 'D': // %m/%d/%y
                if (!strftime_number<T, 2>(sp, tm->tm_mon + 1))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = '/';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_mday))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = '/';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_year % 100))
                    return 0;
            break;

            case 'e': // day of month as decimal number, space-padded
                if (tm->tm_mday < 10) {
                    if (sp.empty())
                        return 0;

                    sp[0] = ' ';
                    sp = span(sp.data() + 1, sp.size() - 1);
                }

                if (!strftime_number<T, 1>(sp, tm->tm_mday))
                    return 0;
            break;

            case 'F': // %Y-%m-%d
                if (!strftime_number<T, 4>(sp, tm->tm_year + 1900))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = '-';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_mon + 1))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = '-';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_mday))
                    return 0;
            break;

            case 'g': { // last 2 digits of week-based year
                int wday_mon = (tm->tm_wday + 6) % 7;
                int thu_yday = tm->tm_yday - wday_mon + 3;
                int year;

                if (thu_yday < 0) // Thursday is in previous year - use last ISO week of prev year
                    year = tm->tm_year - 1;
                else if ((unsigned int)thu_yday >= days_in_year(tm->tm_year + 1900)) // Thursday is in next year - this is week 1 of next year
                    year = tm->tm_year + 1;
                else
                    year = tm->tm_year;

                if (!strftime_number<T, 2>(sp, year % 100))
                    return 0;

                break;
            }

            case 'G': { // week-based year
                int wday_mon = (tm->tm_wday + 6) % 7;
                int thu_yday = tm->tm_yday - wday_mon + 3;
                int year;

                if (thu_yday < 0) // Thursday is in previous year - use last ISO week of prev year
                    year = tm->tm_year - 1;
                else if ((unsigned int)thu_yday >= days_in_year(tm->tm_year + 1900)) // Thursday is in next year - this is week 1 of next year
                    year = tm->tm_year + 1;
                else
                    year = tm->tm_year;

                if (!strftime_number<T, 4>(sp, year + 1900))
                    return 0;

                break;
            }

            case 'H': // hour in 24-hour clock
                if (!strftime_number<T, 2>(sp, tm->tm_hour))
                    return 0;
            break;

            case 'I': // hour in 12-hour clock
                if (!strftime_number<T, 2>(sp, ((tm->tm_hour + 11) % 12) + 1))
                    return 0;
            break;

            case 'j': // day of year as decimal number
                if (!strftime_number<T, 3>(sp, tm->tm_yday + 1))
                    return 0;
            break;

            case 'm': // month as decimal number
                if (!strftime_number<T, 2>(sp, tm->tm_mon + 1))
                    return 0;
            break;

            case 'M': // minute as decimal number
                if (!strftime_number<T, 2>(sp, tm->tm_min))
                    return 0;
            break;

            case 'n':
                if (sp.empty())
                    return 0;

                sp[0] = '\n';
                sp = span(sp.data() + 1, sp.size() - 1);
            break;

            case 'p': // am or pm
                if (!strftime_ampm<T>(sp, tm->tm_hour))
                    return 0;
            break;

            case 'r': // %I:%M:%S %p
                if (!strftime_number<T, 2>(sp, ((tm->tm_hour + 11) % 12) + 1))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ':';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_min))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ':';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_sec))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ' ';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_ampm<T>(sp, tm->tm_hour))
                    return 0;
            break;

            case 'R': // %H:%M
                if (!strftime_number<T, 2>(sp, tm->tm_hour))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ':';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_min))
                    return 0;
            break;

            case 'S': // second as decimal number
                if (!strftime_number<T, 2>(sp, tm->tm_sec))
                    return 0;
            break;

            case 't':
                if (sp.empty())
                    return 0;

                sp[0] = '\t';
                sp = span(sp.data() + 1, sp.size() - 1);
            break;

            case 'T': // %H:%M:%S
                if (!strftime_number<T, 2>(sp, tm->tm_hour))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ':';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_min))
                    return 0;

                if (sp.empty())
                    return 0;

                sp[0] = ':';
                sp = span(sp.data() + 1, sp.size() - 1);

                if (!strftime_number<T, 2>(sp, tm->tm_sec))
                    return 0;
            break;

            case 'u': // weekday as decimal number, 1 = Monday, 7 = Sunday
                if (!strftime_number<T, 1>(sp, tm->tm_wday == 0 ? 7 : tm->tm_wday))
                    return 0;
            break;

            case 'U': // week number of year as decimal number, Sunday start
                if (!strftime_number<T, 2>(sp, (tm->tm_yday + 7 - tm->tm_wday) / 7))
                    return 0;
            break;

            case 'V': { // week number of year according to ISO 8601
                int wday_mon = (tm->tm_wday + 6) % 7;
                int thu_yday = tm->tm_yday - wday_mon + 3;
                int week;

                if (thu_yday < 0) // Thursday is in previous year - use last ISO week of prev year
                    week = ((thu_yday + (int)days_in_year(tm->tm_year + 1900 - 1)) / 7) + 1;
                else if ((unsigned int)thu_yday >= days_in_year(tm->tm_year + 1900)) // Thursday is in next year - this is week 1 of next year
                    week = 1;
                else
                    week = (thu_yday / 7) + 1;

                if (!strftime_number<T, 2>(sp, week))
                    return 0;

                break;
            }

            case 'w': // weekday as decimal number, 0 = Sunday, 6 = Saturday
                if (!strftime_number<T, 1>(sp, tm->tm_wday))
                    return 0;
            break;

            case 'W': // week number of year as decimal number, Monday start
                if (!strftime_number<T, 2>(sp, (tm->tm_yday + 7 - (tm->tm_wday + 6) % 7) / 7))
                    return 0;
            break;

            case 'x': { // "locale's appropriate date representation"
                size_t ret;

                if constexpr (is_same_v<T, char>)
                    ret = do_strftime<T>(sp.data(), sp.size(), "%d/%m/%Y", tm);
                else if constexpr (is_same_v<T, wchar_t>)
                    ret = do_strftime<T>(sp.data(), sp.size(), L"%d/%m/%Y", tm);

                if (ret == 0)
                    return 0;

                sp = span(sp.data() + ret, sp.size() - ret);

                break;
            }

            case 'X': { // "locale's appropriate time representation"
                size_t ret;

                if constexpr (is_same_v<T, char>)
                    ret = do_strftime<T>(sp.data(), sp.size(), "%H:%M:%S", tm);
                else if constexpr (is_same_v<T, wchar_t>)
                    ret = do_strftime<T>(sp.data(), sp.size(), L"%H:%M:%S", tm);

                if (ret == 0)
                    return 0;

                sp = span(sp.data() + ret, sp.size() - ret);

                break;
            }

            case 'y': // year % 100
                if (!strftime_number<T, 2>(sp, tm->tm_year % 100))
                    return 0;
            break;

            case 'Y': // year
                if (!strftime_number<T, 4>(sp, tm->tm_year + 1900))
                    return 0;
            break;

            case 'z': { // timezone offset
                if (tm->tm_isdst < 0)
                    break;

                uint32_t num = tm->tm_gmtoff < 0 ? -tm->tm_gmtoff : tm->tm_gmtoff;

                num = ((num / 3600) * 100) + ((num / 60) % 60);

                if (num > 2359)
                    return 0;

                if (sp.size() < 5)
                    return 0;

                sp[0] = tm->tm_gmtoff < 0 ? '-' : '+';
                sp[1] = (T)((num / 1000) + '0');
                sp[2] = (T)(((num / 100) % 10) + '0');
                sp[3] = (T)(((num / 10) % 10) + '0');
                sp[4] = (T)((num % 10) + '0');

                sp = span(sp.data() + 5, sp.size() - 5);

                break;
            }

            case 'Z': { // timezone name
                if (!tm->tm_zone)
                    break;

                string_view tz = tm->tm_zone;

                if constexpr (is_same_v<T, char>) {
                    if (sp.size() < tz.size())
                        return 0;

                    memcpy(sp.data(), tz.data(), tz.size());
                    sp = span(sp.data() + tz.size(), sp.size() - tz.size());
                } else {
                    auto u16len = utf8_to_utf16_len(tz);

                    if (sp.size() < u16len)
                        return 0;

                    utf8_to_utf16(span((char16_t*)sp.data(), sp.size()), tz);
                    sp = span(sp.data() + u16len, sp.size() - u16len);
                }

                break;
            }

            case '%':
                if (sp.empty())
                    return 0;

                sp[0] = '%';
                sp = span(sp.data() + 1, sp.size() - 1);
            break;
        }

        fmt = basic_string_view<T>(fmt.data() + 1, fmt.size() - 1);

        if (fmt.empty())
            break;

        if (sp.empty())
            return 0;
    }

    if (sp.empty())
        return 0;

    sp[0] = 0;

    return (size_t)(sp.data() - s);
}

extern "C"
size_t _strftime(char* s, size_t max, const char* format, const struct tm* tm) {
    return do_strftime<char>(s, max, format, tm);
}

extern "C"
size_t wcsftime(wchar_t* s, size_t max, const wchar_t* format, const struct tm* tm) {
    return do_strftime<wchar_t>(s, max, format, tm);
}

extern "C"
errno_t __localtime64_s(struct tm* tm, const __time64_t* t) {
    auto v = *t;
    decltype(tz_types)::pointer tt = nullptr;

    _tzset();

    RtlAcquireSRWLockShared(&tz_lock);

    if (tz_transitions.size() > 0) {
        optional<size_t> trans_num;

        if (*t < tz_transitions[0].time)
            tt = &tz_types[0];
        else {
            for (size_t i = 1; i < tz_transitions.size(); i++) {
                if (*t < tz_transitions[i].time) {
                    trans_num = i - 1;
                    break;
                }
            }

            if (!trans_num.has_value() && !tz_valid_footer)
                trans_num = tz_transitions.size() - 1;

            if (trans_num.has_value())
                tt = &tz_types[tz_transitions[*trans_num].type];
        }
    } else if (!tz_types.empty())
        tt = &tz_types[0];

    if (tt) {
        v += tt->utoff;

        tm->tm_isdst = tt->dst;
        tm->tm_gmtoff = tt->utoff;
        tm->tm_zone = &tz_names[tt->idx];
    } else {
        constexpr__gmtime64_s(tm, v - tz_std_offset); // get the year first

        tz_adjust_by_rules(v, *tm);
    }

    auto ret = constexpr__gmtime64_s(tm, v);

    RtlReleaseSRWLockShared(&tz_lock);

    return ret;
}

extern "C"
struct tm* __localtime64(const __time64_t* t) {
    static struct tm tm;

    auto ret = __localtime64_s(&tm, t);
    if (ret != 0)
        return nullptr;

    return &tm;
}

static constexpr __time64_t do_mktime(tm* tm) {
    auto& t = *tm;
    struct tm tm2;
    __time64_t ret;

    int year = t.tm_year + 1900;
    auto mon = t.tm_mon;

    // normalize month
    if (mon < 0 || mon >= 12) {
        if (mon < 0) {
            year += (mon / 12) - 1;
            mon = (mon % 12) + 12;
        } else {
            year += (mon / 12);
            mon %= 12;
        }
    }

    // shift so year starts on March 1
    if (mon < 2) {
        mon += 12;
        year--;
    }

    int era = (year >= 0 ? year : (year - 399)) / 400;
    auto year_of_era = (unsigned int)(year - (era * 400));
    auto day_of_year = (unsigned int)((((153 * (mon - 2)) + 2) / 5) + t.tm_mday - 1);
    unsigned int day_of_era = (year_of_era * 365) + (year_of_era / 4) - (year_of_era / 100) + day_of_year;

    int days_since_epoch = (era * 146097) + (int)day_of_era - 719468;

    ret = (days_since_epoch * 86400) + (t.tm_hour * 3600) + (t.tm_min * 60) + t.tm_sec;

    // normalize fields

    constexpr__gmtime64_s(&tm2, ret);

    t.tm_hour = tm2.tm_hour;
    t.tm_min = tm2.tm_min;
    t.tm_sec = tm2.tm_sec;
    t.tm_wday = tm2.tm_wday;
    t.tm_yday = tm2.tm_yday;
    t.tm_year = tm2.tm_year;
    t.tm_mon = tm2.tm_mon;
    t.tm_mday = tm2.tm_mday;

    t.tm_isdst = 0; // FIXME
    t.tm_gmtoff = 0; // FIXME
    t.tm_zone = "UTC"; // FIXME

    return ret;
}

extern "C"
__time64_t __mktime64(struct tm* tm) {
    return do_mktime(tm);
}

static constexpr __time64_t test_mktime(int year, int mon, int day, int hour,
                                        int min, int sec) {
    struct tm tm;

    tm.tm_year = year - 1900;
    tm.tm_mon = mon - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min = min;
    tm.tm_sec = sec;

    return do_mktime(&tm);
}

static_assert(test_mktime(1969, 12, 31, 0, 0, 0) == -86400);
static_assert(test_mktime(1969, 12, 31, 23, 59, 59) == -1);
static_assert(test_mktime(1970, 1, 1, 0, 0, 0) == 0);
static_assert(test_mktime(1970, 1, 1, 12, 34, 56) == 45296);
static_assert(test_mktime(2026, 6, 9, 17, 48, 7) == 1781027287);
static_assert(test_mktime(2024, 6, 9, 17, 48, 7) == 1717955287);
static_assert(test_mktime(2024, 2, 28, 17, 48, 7) == 1709142487);

extern "C"
double __difftime64(__time64_t end, __time64_t start) {
    return (double)(end - start);
}

extern "C"
char* __ctime64(const __time64_t* t) {
    static char buf[26];
    struct tm tm;

    __localtime64_s(&tm, t);

    if (_strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Y\n", &tm) == 0)
        return nullptr;

    return buf;
}

extern "C"
char* asctime(const struct tm* tm) {
    static char buf[26];

    if (_strftime(buf, sizeof(buf), "%a %b %e %H:%M:%S %Y\n", tm) == 0)
        return nullptr;

    return buf;
}

static int timespec_get_utc(timespec& tp) {
    LARGE_INTEGER li;

    auto status = NtQuerySystemTime(&li);
    if (!success(status))
        return 0;

    auto t = (uint64_t)li.QuadPart; // units of 100ns

    t -= 116444736000000000; // change from NT to Unix epoch

    tp.tv_sec = t / 10000000;
    tp.tv_nsec = (long)((t % 10000000) * 100);

    return TIME_UTC;
}

extern "C"
int _timespec64_get(timespec* tp, int base) {
    switch (base) {
        case TIME_UTC:
            return timespec_get_utc(*tp);

        default:
            return 0;
    }
}
