// SPDX-License-Identifier: LGPL-2.1-or-later
#include <string_view>
#include <span>
#include <utility>
#include <stdlib.h>

import ucrtbase;
import nt;
import print;
import unicode;

using namespace std;

static int fmode = _O_TEXT;
static unsigned int commode = 0;
static int argc = 0;
static char** argv = nullptr;
static char** __environ = nullptr;
static int app_type;

extern "C"
void _set_app_type(_crt_app_type type) {
    app_type = type;
}

extern "C"
int* ___p___argc() {
    return &argc;
}

extern "C"
char*** ___p___argv() {
    return &argv;
}

extern "C"
unsigned int* __p__commode() {
    return &commode;
}

extern "C"
char*** ___p__environ() {
    return &__environ;
}

extern "C"
int* ___p__fmode() {
    return &fmode;
}

extern "C"
void _initterm(_PVFV* first, _PVFV* last) {
    for (auto* f = first; f < last; f++) {
        if (!*f)
            continue;

        (**f)();
    }
}

extern "C"
int _initterm_e(_PIFV* first, _PIFV* last) {
    for (auto* f = first; f < last; f++) {
        if (!*f)
            continue;

        auto ret = (**f)();
        if (ret != 0)
            return ret;
    }

    return 0;
}

extern "C"
int _initialize_narrow_environment() {
    return 0;
}

static unsigned int count_cmdline_args(u16string_view cmdline) {
    unsigned int argc = 0;
    size_t pos = 0;
    size_t len = cmdline.size();

    auto skip_whitespace = [&]() {
        while (pos < len && (cmdline[pos] == u' ' || cmdline[pos] == u'\t')) {
            pos++;
        }
    };

    skip_whitespace();

    // argv[0]: quotes group, but backslashes are literal
    if (pos < len) {
        if (cmdline[pos] == u'"') {
            pos++;

            while (pos < len && cmdline[pos] != u'"') {
                pos++;
            }

            if (pos < len)
                pos++; // closing quote
        } else {
            while (pos < len && cmdline[pos] != u' ' && cmdline[pos] != u'\t') {
                pos++;
            }
        }

        argc++;
    }

    // remaining arguments use the general quoting rules
    while (true) {
        skip_whitespace();

        if (pos >= len)
            break;

        bool in_quotes = false;

        while (pos < len) {
            auto c = cmdline[pos];

            if ((c == u' ' || c == u'\t') && !in_quotes)
                break;

            if (c == u'\\') {
                size_t nbs = 0;

                while (pos < len && cmdline[pos] == u'\\') {
                    nbs++;
                    pos++;
                }

                if (pos < len && cmdline[pos] == u'"' && nbs & 1)
                    pos++; // odd run escapes the quote, consuming it here

                continue;
            }

            if (c == u'"') {
                if (in_quotes && pos + 1 < len && cmdline[pos + 1] == u'"')
                    pos += 2; // doubled quote inside quotes is a literal quote
                else {
                    in_quotes = !in_quotes;
                    pos++;
                }

                continue;
            }

            pos++;
        }

        argc++;
    }

    return argc;
}

static void parse_cmdline(u16string_view cmdline, span<u16string_view> argv,
                          char16_t* scratch) {
    auto out = scratch;
    size_t argc = 0;
    size_t pos = 0;
    size_t len = cmdline.size();

    auto skip_whitespace = [&]() {
        while (pos < len && (cmdline[pos] == u' ' || cmdline[pos] == u'\t')) {
            pos++;
        }
    };

    skip_whitespace();

    // argv[0]: quotes group, but backslashes are literal
    if (pos < len) {
        char16_t* start = out;

        if (cmdline[pos] == u'"') {
            pos++;

            while (pos < len && cmdline[pos] != u'"') {
                *out = cmdline[pos];
                out++;
                pos++;
            }

            if (pos < len)
                pos++; // closing quote
        } else {
            while (pos < len && cmdline[pos] != u' ' && cmdline[pos] != u'\t') {
                *out = cmdline[pos];
                out++;
                pos++;
            }
        }

        argv[argc] = u16string_view(start, (size_t)(out - start));
        argc++;
    }

    // remaining arguments use the general quoting rules
    while (true) {
        skip_whitespace();

        if (pos >= len)
            break;

        if (argc >= argv.size())
            break;

        char16_t* start = out;
        bool in_quotes = false;

        while (pos < len) {
            auto c = cmdline[pos];

            if ((c == u' ' || c == u'\t') && !in_quotes)
                break;

            if (c == u'\\') {
                size_t nbs = 0;

                while (pos < len && cmdline[pos] == u'\\') {
                    nbs++;
                    pos++;
                }

                if (pos < len && cmdline[pos] == u'"') {
                    // backslashes immediately before a quote: each pair becomes one literal backslash
                    for (size_t i = 0; i < nbs / 2; i++) {
                        *out = u'\\';
                        out++;
                    }

                    if (nbs & 1) {
                        // odd: the last backslash escapes the quote into a literal
                        *out = u'"';
                        out++;
                        pos++;
                    }

                    // even: leave the quote for the next iteration to treat as a delimiter
                } else {
                    // not before a quote: backslashes are literal
                    for (size_t i = 0; i < nbs; i++) {
                        *out = u'\\';
                        out++;
                    }
                }

                continue;
            }

            if (c == u'"') {
                if (in_quotes && pos + 1 < len && cmdline[pos + 1] == u'"') {
                    // doubled quote inside quotes is a literal quote
                    *out = u'"';
                    out++;
                    pos += 2;
                } else {
                    in_quotes = !in_quotes;
                    pos++;
                }

                continue;
            }

            *out = c;
            out++;
            pos++;
        }

        argv[argc] = u16string_view(start, (size_t)(out - start));
        argc++;
    }
}

static void do_cmdline() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    u16string_view cmdline;

    if (peb->ProcessParameters->CommandLine.Length == 0xffff) {
        // oversize, need to find Length manually
        cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer);
    } else {
        cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                 peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));
    }

    argc = (int)count_cmdline_args(cmdline);

    span<u16string_view> u16argv;
    u16string_view* buf = nullptr;

    if (argc == 0)
        return;

    buf = (decltype(buf))malloc((unsigned int)argc * sizeof(*buf));
    if (!buf) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    argv = (decltype(argv))malloc((unsigned int)argc * sizeof(*argv));
    if (!argv) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    auto scratch = (char16_t*)malloc(cmdline.size() * sizeof(char16_t));
    if (!scratch) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    u16argv = span(buf, (unsigned int)argc);
    parse_cmdline(cmdline, u16argv, scratch);

    for (unsigned int i = 0; i < (unsigned int)argc; i++) {
        auto arg = u16argv[i];
        auto u8len = utf16_to_utf8_len(arg);

        argv[i] = (char*)malloc(u8len + 1);
        if (!argv[i]) {
            print_stderr("malloc failed\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        utf16_to_utf8(span(argv[i], u8len), arg);
        argv[i][u8len] = 0;
    }

    free(buf);
    free(scratch);
}

static void do_env() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto env = u16string_view((char16_t*)peb->ProcessParameters->Environment,
                              peb->ProcessParameters->EnvironmentSize / sizeof(char16_t));

    auto env2 = env;
    unsigned int env_size = 0;

    while (true) {
        if (env2.empty() || env2[0] == 0)
            break;

        env_size++;

        auto zero = env2.find(u'\0');

        if (zero != u16string_view::npos)
            env2 = u16string_view(env2.data() + zero + 1, env2.size() - zero - 1);
        else
            break;
    }

    __environ = (char**)malloc((env_size + 1) * sizeof(char*));
    if (!__environ) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    auto p = __environ;

    while (true) {
        if (env.empty() || env[0] == 0)
            break;

        auto zero = env.find(u'\0');

        u16string_view bit;

        if (zero == u16string_view::npos)
            bit = env;
        else
            bit = u16string_view(env.data(), zero);

        auto u8len = utf16_to_utf8_len(bit);

        *p = (char*)malloc(u8len + 1);
        if (!*p) {
            print_stderr("malloc failed\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        utf16_to_utf8(span(*p, u8len), bit);
        (*p)[u8len] = 0;

        p++;

        if (zero != u16string_view::npos)
            env = u16string_view(env.data() + zero + 1, env.size() - zero - 1);
        else
            break;
    }

    *p = nullptr;
}

extern "C"
int _configure_narrow_argv(int /*mode*/) {
    // FIXME - what is the parameter supposed to do?

    do_cmdline();
    do_env();

    return 0;
}
