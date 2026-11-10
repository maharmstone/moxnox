// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <concepts>
#include <algorithm>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <wchar.h>

using namespace std;

import unicode;

extern "C" int* __errno();

extern "C"
size_t wcsnlen(const wchar_t* s, size_t maxlen) {
    size_t i = 0;

    while (i < maxlen && s[i] != 0) {
        i++;
    }

    return i;
}

extern "C"
wint_t _towlower(wint_t c) {
    // FIXME - other scripts etc.

    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    else
        return c;
}

extern "C"
wint_t _towupper(wint_t c) {
    // FIXME - other scripts etc.

    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
}

extern "C"
wchar_t* wcsncpy(wchar_t* dest, const wchar_t* src, size_t n) {
    auto orig_dest = dest;

    while (n > 0) {
        if (*src == 0)
            break;

        *dest = *src;
        dest++;
        src++;
        n--;
    }

    while (n > 0) {
        *dest = 0;
        dest++;
        n--;
    }

    return orig_dest;
}

extern "C"
int __stdio_common_vswprintf(uint64_t /*options*/, wchar_t* buffer,
                             [[maybe_unused]] size_t buffer_count,
                             const wchar_t* format,
                             _locale_t /*locale*/, va_list valist) {
    // FIXME - options
    // FIXME - locale

    return __mingw_vswprintf(buffer, buffer_count, format, valist);
}

extern "C"
int _iswspace(wint_t ch) {
    switch (ch) {
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
        case '\v':
            return 1;

        default:
            return 0;
    }
}

template<integral T>
static T wcsto(const wchar_t* nptr, wchar_t** endptr, int base) {
    bool negative = false;
    bool empty = true;
    bool overflow = false;
    T ret = 0;

    if(endptr)
        *endptr = (wchar_t*)nptr;

    while (_iswspace(*nptr)) {
        nptr++;
    }

    if (*nptr == '-') {
        negative = true;
        nptr++;
    } else if(*nptr == '+')
        nptr++;

    if ((base == 0 || base == 16) && nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
        base = 16;
        nptr += 2;
    }

    if(base == 0) {
        if (*nptr == '0')
            base = 8;
        else
            base = 10;
    }

    while (*nptr) {
        int digit;

        if (*nptr >= '0' && *nptr <= min((char)('0' + base - 1), '9'))
            digit = *nptr - '0';
        else if (base > 10 && *nptr >= 'A' && *nptr <= min((char)('A' + base - 11), 'Z'))
            digit = *nptr - 'A' + 10;
        else if (base > 10 && *nptr >= 'a' && *nptr <= min((char)('a' + base - 11), 'z'))
            digit = *nptr - 'a' + 10;
        else
            break;

        if constexpr (is_signed_v<T>) {
            if (negative)
                digit = -digit;
        }

        nptr++;
        empty = false;

        if constexpr (is_signed_v<T>) {
            if (!negative && (ret > numeric_limits<T>::max() / (T)base || ret * (T)base > numeric_limits<T>::max() - (T)digit)) {
                ret = numeric_limits<T>::max();
                overflow = true;
                break;
            } else if (negative && (ret < numeric_limits<T>::min() / (T)base || ret * (T)base < numeric_limits<T>::min() - (T)digit)) {
                ret = numeric_limits<T>::min();
                overflow = true;
                break;
            }
        } else {
            if (ret > numeric_limits<T>::max() / (T)base || ret * (T)base > numeric_limits<T>::max() - (T)digit) {
                ret = numeric_limits<T>::max();
                overflow = true;
                break;
            }
        }

        ret = (ret * (T)base) + (T)digit;
    }

    if(endptr && !empty)
        *endptr = (wchar_t*)nptr;

    if (overflow)
        *__errno() = ERANGE;

    if constexpr (is_signed_v<T>)
        return ret;
    else {
        if (overflow)
            return ret;
        else
            return negative ? -ret : ret;
    }
}

extern "C"
long wcstol(const wchar_t* str, wchar_t** str_end, int base) {
    return wcsto<long>(str, str_end, base);
}

extern "C"
long long wcstoll(const wchar_t* str, wchar_t** str_end, int base) {
    return wcsto<long long>(str, str_end, base);
}

extern "C"
unsigned long wcstoul(const wchar_t* str, wchar_t** str_end, int base) {
    return wcsto<unsigned long>(str, str_end, base);
}

extern "C"
unsigned long long wcstoull(const wchar_t* str, wchar_t** str_end, int base) {
    return wcsto<unsigned long long>(str, str_end, base);
}

extern "C"
wchar_t* wcschr(const wchar_t* s, wchar_t c) {
    do {
        if (*s == c)
            return (wchar_t*)s;

        if (*s == 0)
            break;

        s++;
    } while (true);

    return nullptr;
}

extern "C"
wchar_t* wcsrchr(const wchar_t* s, wchar_t c) {
    wchar_t* last = nullptr;

    while (true) {
        if (*s == c)
            last = (wchar_t*)s;

        if (*s == L'\0')
            break;

        s++;
    }

    return last;
}

extern "C"
int __wcsnicmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        auto c1 = *s1;
        auto c2 = *s2;

        if (c1 >= 'A' && c1 <= 'Z')
            c1 = c1 - 'A' + 'a';

        if (c2 >= 'A' && c2 <= 'Z')
            c2 = c2 - 'A' + 'a';

        if (c1 < c2)
            return -1;
        else if (c2 < c1)
            return 1;

        if (c1 == 0)
            break;

        s1++;
        s2++;
    }

    return 0;
}

extern "C"
wchar_t* wcscpy(wchar_t* dest, const wchar_t* src) {
    auto orig_dest = dest;

    while (*src != 0) {
        *dest = *src;
        src++;
        dest++;
    }

    *dest = 0;

    return orig_dest;
}

extern "C"
wchar_t* __wcstok(wchar_t* str, const wchar_t* delim, wchar_t** ptr) {
    wchar_t* s = str ? str : *ptr;

    auto delimlen = wcslen(delim);

    // skip leading delimiters

    while (true) {
        if (s[0] == 0) {
            *ptr = s;
            return nullptr;
        }

        bool found = false;
        for (size_t i = 0; i < delimlen; i++) {
            if (s[0] == delim[i]) {
                found = true;
                break;
            }
        }

        if (!found)
            break;

        s++;
    }

    auto ret = s;

    while (true) {
        if (s[0] == 0) {
            *ptr = s;
            return ret;
        }

        for (size_t i = 0; i < delimlen; i++) {
            if (s[0] == delim[i]) {
                s[0] = 0;
                s++;

                *ptr = s;

                return ret;
            }
        }

        s++;
    }
}

extern "C"
int wcsncmp(const wchar_t* s1, const wchar_t* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        wchar_t c1 = s1[i];
        wchar_t c2 = s2[i];

        if (c1 == 0 && c2 == 0)
            return 0;
        else if (c1 == 0)
            return -1;
        else if (c2 == 0)
            return 1;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;
    }

    return 0;
}

extern "C"
size_t wcsxfrm(wchar_t* dest, const wchar_t* src, size_t size) {
    auto len = wcslen(src);

    if (size > 0)
        memcpy(dest, src, min(len + 1, size) * sizeof(wchar_t));

    return len;
}

extern "C"
float wcstof(const wchar_t* str, wchar_t** str_end) {
    auto u16sv = u16string_view((char16_t*)str);
    auto len = utf16_to_utf8_len(u16sv);
    char* end;

    // FIXME - clamp to first non-ASCII unit (utf16_to_utf8_len returns 0 if any lone surrogate found)

    if (len == 0) {
        if (str_end)
            *str_end = (wchar_t*)str;

        *__errno() = EINVAL;
        return 0.0f;
    }

    unique_ptr<char[]> buf;

    buf.reset(new char[len + 1]);
    if (!buf) {
        *__errno() = ENOMEM;
        return 0.0f;
    }

    auto sp = span(buf.get(), len);

    utf16_to_utf8(sp, u16sv);
    buf[len] = 0;

    auto ret = strtof(buf.get(), &end);

    if (str_end)
        *str_end = (wchar_t*)str + utf8_to_utf16_len(string_view(buf.get(), (size_t)(end - buf.get())));

    return ret;
}

extern "C"
double wcstod(const wchar_t* str, wchar_t** str_end) {
    auto u16sv = u16string_view((char16_t*)str);
    auto len = utf16_to_utf8_len(u16sv);
    char* end;

    // FIXME - clamp to first non-ASCII unit (utf16_to_utf8_len returns 0 if any lone surrogate found)

    if (len == 0) {
        if (str_end)
            *str_end = (wchar_t*)str;

        *__errno() = EINVAL;
        return 0.0;
    }

    unique_ptr<char[]> buf;

    buf.reset(new char[len + 1]);
    if (!buf) {
        *__errno() = ENOMEM;
        return 0.0;
    }

    auto sp = span(buf.get(), len);

    utf16_to_utf8(sp, u16sv);
    buf[len] = 0;

    auto ret = strtod(buf.get(), &end);

    if (str_end)
        *str_end = (wchar_t*)str + utf8_to_utf16_len(string_view(buf.get(), (size_t)(end - buf.get())));

    return ret;
}

extern "C"
size_t wcslen(const wchar_t* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

extern "C"
wchar_t* wcscat(wchar_t* dest, const wchar_t* src) {
    auto orig_dest = dest;

    while (dest[0] != 0) {
        dest++;
    }

    memcpy(dest, src, (wcslen(src) + 1) * sizeof(wchar_t));

    return orig_dest;
}

extern "C"
wchar_t* _wcsdup(const wchar_t* string) {
    auto sv = wstring_view(string);

    auto ret = (wchar_t*)malloc((sv.size() + 1) * sizeof(wchar_t));
    if (!ret)
        return nullptr;

    memcpy(ret, string, (sv.size() + 1) * sizeof(wchar_t));

    return ret;
}
