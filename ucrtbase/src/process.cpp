// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <string_view>
#include <utility>
#include <span>
#include <memory>
#include <optional>

import nt;
import ntdll;
import unique_handle;
import unicode;
import print;
import exec;
import file;

#undef errno
import errno;

using namespace std;

extern "C" int* __errno();
extern "C" char*** ___p__environ();
extern "C" int __access(const char* path, int mode);

extern "C"
int _getpid() {
    PROCESS_BASIC_INFORMATION pbi;

    auto status = NtQueryInformationProcess(NtCurrentProcess(),
                                            PROCESS_INFORMATION_CLASS::BasicInformation,
                                            &pbi, sizeof(pbi), nullptr);
    if (!success(status))
        return 0;

    return (int)pbi.UniqueProcessId;
}

extern "C"
int system(const char* command) {
    PROCESS_BASIC_INFORMATION pbi;
    nt_status status;
    unique_handle proc, thread;
    unique_ptr<char16_t[]> cmdline;

    static const u16string_view DASH_C = u" -c ";

    static const char16_t u16shell[] = u"/bin/sh";
    static const size_t u16shelllen = (sizeof(u16shell) / sizeof(char16_t)) - 1;

    if (!command) {
        // FIXME - should check whether shell actually exists?

        return 1;
    }

    if (!command[0]) {
        *__errno() = EINVAL;
        return -1;
    }

    string_view cmd(command);
    auto u16cmdlen = utf8_to_utf16_len(cmd);

    auto cmdlinelen = u16shelllen + DASH_C.size() + u16cmdlen;

    cmdline.reset(new char16_t[cmdlinelen]);
    if (!cmdline) {
        *__errno() = ENOMEM;
        return -1;
    }

    memcpy(cmdline.get(), u16shell, u16shelllen * sizeof(char16_t));
    memcpy(cmdline.get() + u16shelllen, DASH_C.data(), DASH_C.size() * sizeof(char16_t));
    utf8_to_utf16(span(cmdline.get() + u16shelllen + DASH_C.size(), u16cmdlen), cmd);

    status = create_process(u16shell, u16string_view(cmdline.get(), cmdlinelen),
                            nullptr, nullopt, nullopt, nullopt, proc, thread);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    while (true) {
        status = NtWaitForSingleObject(proc.get(), true, nullptr);
        if (!success(status)) {
            *__errno() = nt_status_to_errno(status);
            return -1;
        }

        if (status == nt_status::SUCCESS)
            break;
    }

    status = NtQueryInformationProcess(proc.get(), PROCESS_INFORMATION_CLASS::BasicInformation,
                                       &pbi, sizeof(pbi), nullptr);
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    return (int)pbi.ExitStatus;
}

extern "C"
int _execve(const char* path, char* const* argv, char* const* envp) {
    unique_handle proc, thread;
    nt_status status;
    CLIENT_ID client_id;
    HANDLE h;

    // Linux allows argv and envp to be NULL here, against the spec

    if (!path || path[0] == 0 || !argv || !envp) {
        *__errno() = EINVAL;
        return -1;
    }

    auto ret = do_exec(path, argv, envp, proc, thread, client_id, nullptr,
                       mn_get_alarm_left());
    if (ret) {
        *__errno() = ret;
        return -1;
    }

    h = thread.get();

    status = NtSetInformationProcess(NtCurrentProcess(),
                                     PROCESS_INFORMATION_CLASS::MoxnoxExec,
                                     &h, sizeof(h));
    if (!success(status)) {
        *__errno() = nt_status_to_errno(status);
        return -1;
    }

    unreachable();
}

extern "C"
int _execv(const char* path, char* const argv[]) {
    return _execve(path, argv, *___p__environ());
}

extern "C"
int _execvp(const char* file, char* const argv[]) {
    auto check_access = [](const char* exe) -> int {
        if (!__access(exe, X_OK))
            return 0;
        else
            return *__errno();
    };

    auto ret = search_path(file, [&](const char* exe) -> int {
        // FIXME - if found but not a valid executable, should be running shell with file

        if (_execv(exe, argv) == -1)
            return *__errno();
        else
            return 0;
    }, check_access);

    if (ret) {
        *__errno() = ret;
        return -1;
    }

    return 0;
}

extern "C"
int _execlp(const char* file, const char* arg, ...) {
    va_list ap;
    size_t num_args;

    if (arg) {
        num_args = 2;

        va_start(ap, arg);

        while (true) {
            if (!va_arg(ap, char*))
                break;

            num_args++;
        }

        va_end(ap);
    } else
        num_args = 1;

    auto argv = (char**)malloc(sizeof(char*) * num_args);
    if (!argv) {
        *__errno() = ENOMEM;
        return -1;
    }

    argv[0] = (char*)arg;

    if (arg) {
        auto* a = &argv[1];

        va_start(ap, arg);

        while (true) {
            auto s = va_arg(ap, char*);
            if (!s)
                break;

            *a = s;
            a++;
        }

        va_end(ap);

        *a = nullptr;
    }

    auto ret = _execvp(file, argv);

    free(argv);

    return ret;
}

extern "C"
int _execl(const char* path, const char* arg, ...) {
    va_list ap;
    size_t num_args;

    if (arg) {
        num_args = 2;

        va_start(ap, arg);

        while (true) {
            if (!va_arg(ap, char*))
                break;

            num_args++;
        }

        va_end(ap);
    } else
        num_args = 1;

    auto argv = (char**)malloc(sizeof(char*) * num_args);
    if (!argv) {
        *__errno() = ENOMEM;
        return -1;
    }

    argv[0] = (char*)arg;

    if (arg) {
        auto* a = &argv[1];

        va_start(ap, arg);

        while (true) {
            auto s = va_arg(ap, char*);
            if (!s)
                break;

            *a = s;
            a++;
        }

        va_end(ap);

        *a = nullptr;
    }

    auto ret = _execve(path, argv, *___p__environ());

    free(argv);

    return ret;
}

extern "C"
int _execle(const char* path, const char* arg, ...) {
    va_list ap;
    size_t num_args;
    char** envp;

    if (arg) {
        num_args = 2;

        va_start(ap, arg);

        while (true) {
            if (!va_arg(ap, char*))
                break;

            num_args++;
        }

        envp = va_arg(ap, char**);

        va_end(ap);
    } else {
        num_args = 1;

        va_start(ap, arg);
        envp = va_arg(ap, char**);
        va_end(ap);
    }

    auto argv = (char**)malloc(sizeof(char*) * num_args);
    if (!argv) {
        *__errno() = ENOMEM;
        return -1;
    }

    argv[0] = (char*)arg;

    if (arg) {
        auto* a = &argv[1];

        va_start(ap, arg);

        while (true) {
            auto s = va_arg(ap, char*);
            if (!s)
                break;

            *a = s;
            a++;
        }

        va_end(ap);

        *a = nullptr;
    }

    auto ret = _execve(path, argv, envp);

    free(argv);

    return ret;
}
