// SPDX-License-Identifier: GPL-2.0-only
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <span>
#include <string_view>
#include <utility>

using namespace std;

import nt;
import print;
import unique_handle;
import moxnox_ioctls;
import kernelbase;
import evdev;
import unicode;
import win32error;

static span<char16_t> environment;

static uint32_t do_serial(void*) {
    NTSTATUS status;
    UNICODE_STRING us;
    OBJECT_ATTRIBUTES oa;
    IO_STATUS_BLOCK iosb;
    HANDLE h;

    // FIXME - should be soliciting username and password

#ifdef __aarch64__
    static const char16_t dev[] = u"\\dev\\ttyAMA0";
#else
    static const char16_t dev[] = u"\\dev\\ttyS0";
#endif

    static const char16_t dev2[] = u"\\dev\\hvc0";

    us.Length = us.MaximumLength = (uint16_t)(sizeof(dev) - sizeof(char16_t));
    us.Buffer = (char16_t*)dev;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, 0, nullptr, 0);

    if (status == nt_status::OBJECT_NAME_NOT_FOUND) {
        us.Length = us.MaximumLength = (uint16_t)(sizeof(dev2) - sizeof(char16_t));
        us.Buffer = (char16_t*)dev2;

        status = NtCreateFile(&h, FILE_READ_DATA | FILE_WRITE_DATA, &oa, &iosb,
                              nullptr, 0, 0, FILE_OPEN, 0, nullptr, 0);
    }

    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::SHARING_VIOLATION) {
        return 1;
    }

    if (!success(status)) {
        print("NtCreateFile returned {}\n", status);
        return 1;
    }

    while (true) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));

        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = h;
        si.hStdOutput = h;
        si.hStdError = h;

        // CreateProcess forces us to take a copy :-(

        static const char16_t serial_cmdline[] = u"usercon.exe serial";

        auto cmdline = (char16_t*)malloc(sizeof(serial_cmdline));
        if (!cmdline) {
            print_stderr("malloc failed\n");
            continue;
        }

        memcpy(cmdline, serial_cmdline, sizeof(serial_cmdline));

        if (!CreateProcessW(u"/bin/usercon.exe", cmdline,
                            nullptr, nullptr, false, CREATE_UNICODE_ENVIRONMENT,
                            environment.data(), nullptr, &si, &pi)) {
            if (GetLastError() == ERROR_SYSTEM_SHUTDOWN) {
                while (true) {
                    NtDelayExecution(false, nullptr);
                }
            }

            print_stderr("CreateProcessW failed (last error {})\n",
                         GetLastError());
            free(cmdline);
            continue;
        }

        free(cmdline);

        NtClose(pi.hThread);

        unique_handle proc{pi.hProcess};

        do {
            auto status = NtWaitForSingleObject(proc.get(), true, nullptr);

            if (status == nt_status::ALERTED)
                continue;

            if (!success(status)) {
                print_stderr("NtWaitForSingleObject returned {}\n", status);
                break;
            }

            break;
        } while (true);
    }

    NtClose(h);

    return 0;
}

static void prepare_environment() {
    struct passwd pwd, *pwdres;
    char pwdbuf[1024];

    // FIXME - better way of doing this
    static const u16string_view vars[] = {
        u"PATH=/bin",
        u"LD_LIBRARY_PATH=/lib:.",
        u"CONFIG_SITE=/share/config.site",
        u"TZ=GB" // FIXME - should be set by user
    };

    // FIXME - USER environment variable

    static const u16string_view home_prefix = u"HOME=";
    static const u16string_view shell_prefix = u"SHELL=";

    if (getpwuid_r(0, &pwd, pwdbuf, sizeof(pwdbuf), &pwdres) != 0 || !pwdres) {
        print_stderr("getpwuid_r failed for root\n");
        NtTerminateProcess(NtCurrentProcess(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    auto u16homelen = utf8_to_utf16_len(pwdres->pw_dir);
    auto u16shelllen = utf8_to_utf16_len(pwdres->pw_shell);

    size_t len = 1;

    for (auto v : vars) {
        len += v.size() + 1;
    }

    len += home_prefix.size() + u16homelen + 1;
    len += shell_prefix.size() + u16shelllen + 1;

    auto env = (char16_t*)malloc(len * sizeof(char16_t));
    if (!env) {
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    auto ptr = env;

    for (auto v : vars) {
        memcpy(ptr, v.data(), v.size() * sizeof(char16_t));
        ptr += v.size();
        *ptr = 0;
        ptr++;
    }

    memcpy(ptr, home_prefix.data(), home_prefix.size() * sizeof(char16_t));
    ptr += home_prefix.size();
    utf8_to_utf16(span(ptr, u16homelen), pwdres->pw_dir);
    ptr += u16homelen;
    *ptr = 0;
    ptr++;

    memcpy(ptr, shell_prefix.data(), shell_prefix.size() * sizeof(char16_t));
    ptr += shell_prefix.size();
    utf8_to_utf16(span(ptr, u16shelllen), pwdres->pw_shell);
    ptr += u16shelllen;
    *ptr = 0;
    ptr++;

    *ptr = 0;

    environment = span(env, len);
}

static void mount_dev() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_handle vfsctl, dir;
    nt_status status;

    static const u16string_view directory = u"/dev";
    static const string_view mount_type = "devfs";

    auto e = getenv("VFSCTL");

    if (!e || !e[0]) {
        print("Could not read VFSCTL environment variable.\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INTERNAL_ERROR);
        unreachable();
    }

    uintptr_t h = 0;

    auto e2 = string_view(e);
    while (!e2.empty()) {
        if (e2.front() < '0' || e2.front() > '9') {
            print("Could not parse VFSCTL value \"{}\" as integer.\n", e);
            NtTerminateThread(NtCurrentThread(), nt_status::INTERNAL_ERROR);
            unreachable();
        }

        h *= 10;
        h += (uintptr_t)(e2.front() - '0');
        e2 = string_view(e2.data() + 1, e2.size() - 1);
    }

    vfsctl.reset(h);

    // open or create directory

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(directory.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)directory.data();

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN_IF, FILE_DIRECTORY_FILE, nullptr, 0);
    if (success(status))
        dir.reset(h);
    else {
        print("NtCreateFile returned {} for {}\n", status, directory);
        NtTerminateThread(NtCurrentThread(), status);
        unreachable();
    }

    size_t buflen = sizeof(MOXNOX_VFSCTL_MOUNT) + mount_type.size();
    auto buf = (MOXNOX_VFSCTL_MOUNT*)malloc(buflen);
    if (!buf) {
        print("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    buf->mount_type = sizeof(MOXNOX_VFSCTL_MOUNT);
    buf->source = 0;
    buf->directory = dir.get();
    buf->options = 0;
    memcpy((char*)buf + buf->mount_type, mount_type.data(), mount_type.size());

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_MOUNT, buf,
                                   (uint32_t)buflen, nullptr, 0);
    if (!success(status)) {
        print("IOCTL_MOXNOX_VFSCTL_MOUNT returned {}\n", status);
        NtTerminateThread(NtCurrentThread(), status);
        unreachable();
    }

    free(buf);
}

static unsigned int power_button_thread(void*) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE h;

    static const u16string_view fn = u"/dev/keyboard";

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return 1;
    }

    unique_handle power_button{h};

    while (true) {
        input_event_with_id ev;

        status = NtReadFile(power_button.get(), 0, nullptr, nullptr,
                            &iosb, &ev, sizeof(ev), nullptr, nullptr);

        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status)) {
            print_stderr("NtReadFile returned {}\n", status);
            return 1;
        }

        if (ev.ev.type != evdev_type::EV_KEY)
            continue;

        auto code = (evdev_key_code)ev.ev.code;

        if (code == evdev_key_code::KEY_POWER && ev.ev.value == 1) {
            status = NtShutdownSystem(SHUTDOWN_ACTION::ShutdownPowerOff);

            // shouldn't return

            print_stderr("NtShutdownSystem returned {}\n", status);
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    bool serial_mode = false;

    // FIXME - start services etc.

    for (int i = 1; i < argc; i++) {
        static const char search[] = "console=";

        string_view sv = argv[i];

        if (sv.size() >= sizeof(search) - 1 && !memcmp(sv.data(), search, sizeof(search) - 1)) {
            serial_mode = true;
            break;
        }
    }

    mount_dev();

    prepare_environment();

    if (!serial_mode) {
        auto h = CreateThread(nullptr, 0, do_serial, nullptr, 0, nullptr);
        if (h == 0)
            print_stderr("CreateThread failed (last error {})\n", GetLastError());
        else
            NtClose(h);
    }

    {
        auto h = CreateThread(nullptr, 0, power_button_thread, nullptr,
                              0, nullptr);
        if (h == 0)
            print_stderr("CreateThread failed (last error {})\n", GetLastError());
        else
            NtClose(h);
    }

    while (true) {
        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        memset(&pi, 0, sizeof(pi));

        char16_t* cmdline = nullptr;

        if (serial_mode) {
            // CreateProcess forces us to take a copy :-(

            static const char16_t serial_cmdline[] = u"usercon.exe serial";

            cmdline = (char16_t*)malloc(sizeof(serial_cmdline));
            if (!cmdline) {
                print_stderr("malloc failed\n");
                continue;
            }

            memcpy(cmdline, serial_cmdline, sizeof(serial_cmdline));
        }

        if (!CreateProcessW(u"/bin/usercon.exe", cmdline,
                            nullptr, nullptr, false, CREATE_UNICODE_ENVIRONMENT,
                            environment.data(), nullptr, &si, &pi)) {
            if (GetLastError() == ERROR_SYSTEM_SHUTDOWN) {
                while (true) {
                    NtDelayExecution(false, nullptr);
                }
            }

            print_stderr("CreateProcessW failed (last error {})\n",
                            GetLastError());

            if (cmdline)
                free(cmdline);

            continue;
        }

        if (cmdline)
            free(cmdline);

        NtClose(pi.hThread);

        unique_handle proc{pi.hProcess};

        do {
            auto status = NtWaitForSingleObject(proc.get(), true, nullptr);

            if (status == nt_status::ALERTED)
                continue;

            if (!success(status)) {
                print_stderr("NtWaitForSingleObject returned {}\n", status);
                break;
            }

            break;
        } while (true);
    }
}
