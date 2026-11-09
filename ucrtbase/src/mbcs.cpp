// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <string.h>
#include <locale.h>

import errno;
import ucrtbase;
import print;

extern "C" int* __errno();

static const size_t MB_CUR_MAX = 4;

struct mbstate {
    uint16_t codepoint;
    uint8_t left;
    uint8_t total;
};

static_assert(sizeof(mbstate) <= sizeof(int));

extern "C"
size_t _mbrtowc(wchar_t* pwc, const char* s, size_t n, mbstate_t* ps) {
    static mbstate_t static_state;

    auto& st = *reinterpret_cast<mbstate*>(ps ? ps : &static_state);

    if (!s) {
        if (st.left > 0) {
            st.left = 0;
            st.codepoint = 0;
            st.total = 0;
            *__errno() = EILSEQ;
            return (size_t)-1;
        }

        return 0;
    }

    if (n == 0)
        return (size_t)-2;

    size_t read = 0;
    uint16_t cp;
    uint8_t left, total;

    if (st.left > 0) {
        cp = st.codepoint;
        left = st.left;
        total = st.total;
    } else {
        auto c = (uint8_t)s[read];
        read++;

        if (c < 0x80) {
            if (pwc)
                *pwc = c;

            return c == 0 ? 0 : 1;
        }

        if (c < 0xc2) {
            *__errno() = EILSEQ;
            return (size_t)-1;
        } else if (c < 0xe0) {
            cp = c & 0x1f;
            left = 1;
            total = 1;
        } else if (c < 0xf0) {
            cp = c & 0x0f;
            left = 2;
            total = 2;
        } else {
            *__errno() = EILSEQ;
            return (size_t)-1;
        }
    }

    while (left > 0) {
        if (read >= n) {
            st.codepoint = cp;
            st.left = left;
            st.total = total;

            return (size_t)-2;
        }

        auto c = (uint8_t)s[read];
        read++;

        if (c < 0x80 || c > 0xbf) {
            st.left = 0;
            st.codepoint = 0;
            st.total = 0;
            *__errno() = EILSEQ;
            return (size_t)-1;
        }

        cp = (uint16_t)((cp << 6) | (c & 0x3f));
        left--;
    }

    if ((total == 2 && cp < 0x800) || (cp >= 0xd800 && cp <= 0xdfff)) {
        st.left = 0;
        st.codepoint = 0;
        st.total = 0;
        *__errno() = EILSEQ;
        return (size_t)-1;
    }

    st.left = 0;
    st.codepoint = 0;
    st.total = 0;

    if (pwc)
        *pwc = (wchar_t)cp;

    return read;
}

extern "C"
int _mbtowc(wchar_t* pwc, const char* s, size_t n) {
    static mbstate_t state;

    if (!s) {
        memset(&state, 0, sizeof(state));
        return 0;
    }

    auto ret = _mbrtowc(pwc, s, n, &state);

    if ((ssize_t)ret < 0)
        return -1;
    else
        return (int)ret;
}

extern "C"
size_t _wcrtomb(char* s, wchar_t wc, mbstate_t* /*ps*/) {
    if (!s)
        return 1;

    if (wc < 0x80) {
        *s = (char)wc;
        return 1;
    }

    if (wc < 0x800) {
        s[0] = (char)(0xc0 | (wc >> 6));
        s[1] = (char)(0x80 | (wc & 0x3f));

        return 2;
    }

    // FIXME?
    if (wc >= 0xd800 && wc <= 0xdfff) {
        *__errno() = EILSEQ;
        return (size_t)-1;
    }

    s[0] = (char)(0xe0 | (wc >> 12));
    s[1] = (char)(0x80 | ((wc >> 6) & 0x3f));
    s[2] = (char)(0x80 | (wc & 0x3f));

    return 3;
}

extern "C"
size_t _mbrlen(const char* s, size_t n, mbstate_t* ps) {
    static mbstate_t state;

    return _mbrtowc(nullptr, s, n, ps ? ps : &state);
}

extern "C"
int ____mb_cur_max_func() {
    // maximum bytes per code-point for UTF-8

    return 4;
}

extern "C"
size_t _wcsrtombs(char* dest, const wchar_t** src, size_t dsize, mbstate_t* ps) {
    size_t total = 0;
    auto p = *src;

    while (true) {
        char buf[4];

        size_t n = _wcrtomb(buf, *p, ps);

        if (n == (size_t)-1) {
            if (dest)
                *src = p;

            return (size_t)-1;
        }

        if (dest && n > dsize) {
            *src = p;
            return total;
        }

        if (*p == 0) {
            if (dest) {
                *dest = 0;
                *src = nullptr;
            }

            return total;
        }

        if (dest) {
            memcpy(dest, buf, n);
            dest += n;
            dsize -= n;
        }

        total += n;
        p++;
    }
}

extern "C"
size_t _wcstombs(char* dest, const wchar_t* src, size_t dsize) {
    static mbstate_t state;
    const wchar_t* src_ptr = src;

    state = mbstate_t{};

    return _wcsrtombs(dest, &src_ptr, dsize, &state);
}

extern "C"
size_t _mbstowcs(wchar_t* pwcs, const char* s, size_t n) {
    mbstate_t state;
    size_t count = 0;

    memset(&state, 0, sizeof(state));

    while (!pwcs || count < n) {
        wchar_t wc;

        auto res = _mbrtowc(pwcs ? &wc : nullptr, s, MB_CUR_MAX, &state);

        if (res == 0) {
            if (pwcs)
                pwcs[count] = L'\0';

            return count;
        }

        if (res == (size_t)-1 || res == (size_t)-2)
            return (size_t)-1;

        if (pwcs)
            pwcs[count] = wc;

        s += res;

        count++;
    }

    return count;
}

extern "C"
size_t mbsrtowcs(wchar_t* pwcs, const char** s, size_t n, mbstate_t* state) {
    static mbstate_t static_state;
    size_t count = 0;
    auto st = state ? state : &static_state;
    auto p = *s;

    while (!pwcs || count < n) {
        wchar_t wc;

        auto res = _mbrtowc(pwcs ? &wc : nullptr, p, MB_CUR_MAX, st);

        if (res == 0) {
            if (pwcs) {
                pwcs[count] = L'\0';
                *s = nullptr;
            }

            return count;
        }

        if (res == (size_t)-1) {
            if (pwcs)
                *s = p;

            return (size_t)-1;
        }

        if (pwcs)
            pwcs[count] = wc;

        p += res;

        count++;
    }

    *s = p;

    return count;
}

extern "C"
int mblen(const char* s, size_t n) {
    mbstate_t state{};

    auto ret = _mbrtowc(nullptr, s, n, &state);

    if ((ssize_t)ret < 0)
        return -1;

    return (int)ret;
}

extern "C"
size_t mbrtoc32(char32_t* pc32, const char* s, size_t n, mbstate_t* ps) {
    static mbstate_t static_state;

    auto& st = *reinterpret_cast<mbstate*>(ps ? ps : &static_state);

    if (!s) {
        if (st.left > 0) {
            st.left = 0;
            st.codepoint = 0;
            st.total = 0;
            *__errno() = EILSEQ;
            return (size_t)-1;
        }

        return 0;
    }

    if (n == 0)
        return (size_t)-2;

    size_t read = 0;
    char32_t cp;
    uint8_t left, total;

    if (st.left > 0) {
        cp = st.codepoint;
        left = st.left;
        total = st.total;
    } else {
        auto c = (uint8_t)s[read];
        read++;

        if (c < 0x80) {
            if (pc32)
                *pc32 = c;

            return c == 0 ? 0 : 1;
        }

        if (c < 0xc2) {
            *__errno() = EILSEQ;
            return (size_t)-1;
        } else if (c < 0xe0) {
            cp = c & 0x1f;
            left = 1;
            total = 1;
        } else if (c < 0xf0) {
            cp = c & 0x0f;
            left = 2;
            total = 2;
        } else if (c < 0xf4) {
            cp = c & 0x07;
            left = 3;
            total = 3;
        } else {
            *__errno() = EILSEQ;
            return (size_t)-1;
        }
    }

    while (left > 0) {
        if (read >= n) {
            st.codepoint = (uint16_t)cp;
            st.left = left;
            st.total = total;

            return (size_t)-2;
        }

        auto c = (uint8_t)s[read];
        read++;

        if (c < 0x80 || c > 0xbf) {
            st.left = 0;
            st.codepoint = 0;
            st.total = 0;
            *__errno() = EILSEQ;
            return (size_t)-1;
        }

        cp = (char32_t)((cp << 6) | (c & 0x3f));
        left--;
    }

    if ((total == 2 && cp < 0x800) || (total == 3 && cp < 0x10000) || (cp >= 0xd800 && cp <= 0xdfff) || cp > 0x10ffff) {
        st.left = 0;
        st.codepoint = 0;
        st.total = 0;
        *__errno() = EILSEQ;
        return (size_t)-1;
    }

    st.left = 0;
    st.codepoint = 0;
    st.total = 0;

    if (pc32)
        *pc32 = cp;

    return read;
}

extern "C"
size_t c32rtomb(char* s, char32_t c32, mbstate_t* /*ps*/) {
    if (!s)
        return 1;

    if (c32 < 0x80) {
        *s = (char)c32;
        return 1;
    } else if (c32 < 0x800) {
        s[0] = (char)(0xc0 | (c32 >> 6));
        s[1] = (char)(0x80 | (c32 & 0x3f));

        return 2;
    } else if (c32 < 0x10000) {
        if (c32 >= 0xd800 && c32 <= 0xdfff) {
            *__errno() = EILSEQ;
            return (size_t)-1;
        }

        s[0] = (char)(0xe0 | (c32 >> 12));
        s[1] = (char)(0x80 | ((c32 >> 6) & 0x3f));
        s[2] = (char)(0x80 | (c32 & 0x3f));

        return 3;
    } else if (c32 < 0x110000) {
        s[0] = (char)(0xf0 | (c32 >> 18));
        s[1] = (char)(0x80 | ((c32 >> 12) & 0x3f));
        s[2] = (char)(0x80 | ((c32 >> 6) & 0x3f));
        s[3] = (char)(0x80 | (c32 & 0x3f));

        return 4;
    } else {
        *__errno() = EILSEQ;
        return (size_t)-1;
    }
}
