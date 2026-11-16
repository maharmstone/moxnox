// SPDX-License-Identifier: LGPL-2.1-or-later
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <memory>
#include <string_view>
#include <span>

import nt;
import ntdll;
import unique_handle;
import unicode;
import format;
import list;

#undef errno
import errno;

using namespace std;

static thread_local char dl_error_string[255];
static thread_local bool dl_error_pending;

static thread_local char prog_name[100];

struct atfork_func {
    list_entry le;
    void (*func)();
};

static RTL_SRWLOCK atfork_lock;
static list_entry prepare_atfork = { &prepare_atfork, &prepare_atfork };
static list_entry parent_atfork = { &parent_atfork, &parent_atfork };
static list_entry child_atfork = { &child_atfork, &child_atfork };

extern "C"
pid_t getsid(pid_t pid) {
    nt_status status;
    HANDLE proc;
    unique_handle uh;
    uintptr_t sess;
    uint32_t retlen;

    if (pid == 0)
        proc = NtCurrentProcess();
    else {
        OBJECT_ATTRIBUTES oa;
        CLIENT_ID client_id;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);

        client_id.UniqueProcess = (HANDLE)pid;
        client_id.UniqueThread = 0;

        status = NtOpenProcess(&proc, PROCESS_QUERY_INFORMATION, &oa,
                               &client_id);
        if (status == nt_status::INVALID_CID) {
            *_errno() = ESRCH;
            return -1;
        } else if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        uh.reset(proc);
    }

    status = NtQueryInformationProcess(proc, PROCESS_INFORMATION_CLASS::MoxnoxGetSessionId,
                                       &sess, sizeof(sess), &retlen);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return (pid_t)sess;
}

extern "C"
pid_t setsid() {
    auto& teb = *NtCurrentTeb();

    auto status = NtSetInformationProcess(NtCurrentProcess(),
                                          PROCESS_INFORMATION_CLASS::MoxnoxNewSession,
                                          nullptr, 0);
    if (status == nt_status::INVALID_PARAMETER) {
        *_errno() = EPERM;
        return -1;
    } else if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return (pid_t)teb.ClientId.UniqueProcess;
}

extern "C"
int setpgid(pid_t pid, pid_t pgid) {
    auto& teb = *NtCurrentTeb();
    nt_status status;
    HANDLE proc;
    unique_handle uh;
    uintptr_t pgid2;

    if (pgid < 0) {
        *_errno() = EINVAL;
        return -1;
    }

    if (pid == 0)
        proc = NtCurrentProcess();
    else {
        OBJECT_ATTRIBUTES oa;
        CLIENT_ID client_id;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);

        client_id.UniqueProcess = (HANDLE)pid;
        client_id.UniqueThread = 0;

        status = NtOpenProcess(&proc, PROCESS_SET_INFORMATION, &oa,
                               &client_id);
        if (status == nt_status::INVALID_CID) {
            *_errno() = ESRCH;
            return -1;
        } else if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        uh.reset(proc);
    }

    if (pgid == 0) {
        if (pid == 0)
            pgid2 = teb.ClientId.UniqueProcess;
        else
            pgid2 = (uintptr_t)pid;
    } else
        pgid2 = (uintptr_t)pgid;

    status = NtSetInformationProcess(proc, PROCESS_INFORMATION_CLASS::MoxnoxSetProcessGroup,
                                     &pgid2, sizeof(pgid2));
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}

extern "C"
pid_t getpgid(pid_t pid) {
    nt_status status;
    HANDLE proc;
    unique_handle uh;
    uintptr_t ret;
    uint32_t retlen;

    if (pid == 0)
        proc = NtCurrentProcess();
    else {
        OBJECT_ATTRIBUTES oa;
        CLIENT_ID client_id;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);

        client_id.UniqueProcess = (HANDLE)pid;
        client_id.UniqueThread = 0;

        status = NtOpenProcess(&proc, PROCESS_QUERY_INFORMATION, &oa,
                               &client_id);
        if (status == nt_status::INVALID_CID) {
            *_errno() = ESRCH;
            return -1;
        } else if (!success(status)) {
            *_errno() = nt_status_to_errno(status);
            return -1;
        }

        uh.reset(proc);
    }

    status = NtQueryInformationProcess(proc, PROCESS_INFORMATION_CLASS::MoxnoxGetProcessGroup,
                                       &ret, sizeof(ret), &retlen);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return (pid_t)ret;
}

extern "C"
pid_t getpgrp() {
    return getpgid(0);
}

template<typename... Args>
static void set_dl_error_string(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args&&... args) {
    auto sp = span(dl_error_string, sizeof(dl_error_string) - 1);

    sp = format_to(sp, fmt.sv, forward_as_tuple(std::forward<Args>(args)...));

    dl_error_string[sp.data() - dl_error_string] = 0;
    dl_error_pending = true;
}

extern "C"
void* dlopen(const char* file, int flags) {
    nt_status status;
    UNICODE_STRING us;
    void* dll;
    unique_ptr<char16_t[]> u16buf;

    static const int supported_flags = RTLD_LAZY | RTLD_NOW | RTLD_GLOBAL | RTLD_LOCAL;

    // FIXME - if filename contains slash, treat as path (LdrLoadDll should handle this)

    // ignoring RTLD_LAZY and RTLD_NOW

    // FIXME - RTLD_GLOBAL and RTLD_LOCAL?

    if (flags & ~supported_flags) {
        set_dl_error_string("unsupported dlopen flags {:x}",
                            flags & ~supported_flags);
        return nullptr;
    }

    if (!file)
        return NtCurrentTeb()->ProcessEnvironmentBlock->ImageBaseAddress;

    auto sv = string_view(file);

    if (sv.empty()) {
        set_dl_error_string("filename was empty");
        return nullptr;
    }

    auto u16len = utf8_to_utf16_len(sv);

    u16buf.reset(new char16_t[u16len]);
    if (!u16buf) {
        set_dl_error_string("allocation failed");
        return nullptr;
    }

    utf8_to_utf16(span(u16buf.get(), u16len), sv);

    us.Buffer = u16buf.get();
    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));

    status = LdrLoadDll(nullptr, nullptr, &us, &dll);
    if (!success(status)) {
        set_dl_error_string("LdrLoadDll returned {}", status);
        return nullptr;
    }

    return dll;
}

extern "C"
int dlclose(void* handle) {
    auto status = LdrUnloadDll(handle);
    if (!success(status)) {
        set_dl_error_string("LdrUnloadDll returned {}", status);
        return 1;
    }

    return 0;
}

extern "C"
char* dlerror() {
    if (!dl_error_pending)
        return nullptr;

    dl_error_pending = false;

    return dl_error_string;
}

extern "C"
void* dlsym(void* handle, const char* symbol) {
    nt_status status;
    ANSI_STRING as;
    void* addr;

    as.Buffer = (char*)symbol;
    as.Length = as.MaximumLength = (uint16_t)strlen(symbol);

    status = LdrGetProcedureAddress(handle, &as, 0, &addr);
    if (status == nt_status::PROCEDURE_NOT_FOUND) {
        set_dl_error_string("procedure {} not found in image", symbol);
        return nullptr;
    } else if (!success(status)) {
        set_dl_error_string("LdrGetProcedureAddress returned {}", status);
        return nullptr;
    }

    return addr;
}

extern "C"
pid_t fork() {
    nt_status status;
    uintptr_t pid = 0;
    void* ptr = &pid;

    RtlAcquireSRWLockShared(&atfork_lock);

    for (auto le = prepare_atfork.prev; le != &prepare_atfork; le = le->prev) {
        auto& s = containing_record<atfork_func>(le);

        s.func();
    }

    RtlReleaseSRWLockShared(&atfork_lock);

    RtlLockHeap((void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap);

    status = NtSetInformationProcess(NtCurrentProcess(),
                                     PROCESS_INFORMATION_CLASS::MoxnoxFork,
                                     &ptr, sizeof(ptr));

    if (!success(status)) {
        RtlUnlockHeap((void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap);

        RtlAcquireSRWLockShared(&atfork_lock);

        for (auto le = parent_atfork.next; le != &parent_atfork; le = le->next) {
            auto& s = containing_record<atfork_func>(le);

            s.func();
        }

        RtlReleaseSRWLockShared(&atfork_lock);

        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    if (pid == 0) {
        mn_on_fork();

        for (auto le = child_atfork.next; le != &child_atfork; le = le->next) {
            auto& s = containing_record<atfork_func>(le);

            s.func();
        }
    } else {
        RtlUnlockHeap((void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap);

        RtlAcquireSRWLockShared(&atfork_lock);

        for (auto le = parent_atfork.next; le != &parent_atfork; le = le->next) {
            auto& s = containing_record<atfork_func>(le);

            s.func();
        }

        RtlReleaseSRWLockShared(&atfork_lock);
    }

    return (pid_t)pid;
}

extern "C"
const char* getprogname() {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    auto& params = *peb.ProcessParameters;

    if (prog_name[0] != 0)
        return prog_name;

    auto u16sv = u16string_view(params.ImagePathName.Buffer, params.ImagePathName.Length / sizeof(char16_t));

    if (auto slash = u16sv.rfind(u'/'); slash != u16string::npos)
        u16sv = u16string_view(u16sv.data() + slash + 1, u16sv.size() - slash - 1);

    // strip .exe
    if (u16sv.size() >= 4 &&
        u16sv[u16sv.size() - 4] == u'.' &&
        (u16sv[u16sv.size() - 3] == u'e' || u16sv[u16sv.size() - 3] == u'E') &&
        (u16sv[u16sv.size() - 2] == u'x' || u16sv[u16sv.size() - 2] == u'X') &&
        (u16sv[u16sv.size() - 1] == u'e' || u16sv[u16sv.size() - 1] == u'E')) {
        u16sv = u16string_view(u16sv.data(), u16sv.size() - 4);
    }

    auto len = utf16_to_utf8_len(u16sv);

    utf16_to_utf8(span(prog_name, sizeof(prog_name) - 1), u16sv);

    if (len >= sizeof(prog_name))
        prog_name[sizeof(prog_name) - 1] = 0;
    else
        prog_name[len] = 0;

    return prog_name;
}

extern "C"
int pthread_atfork(void (*prepare)(), void (*parent)(),
                   void (*child)()) {
    atfork_func* sprepare;
    atfork_func* sparent;
    atfork_func* schild;

    if (!prepare && !parent && !child)
        return 0;

    if (prepare) {
        sprepare = (atfork_func*)malloc(sizeof(atfork_func));
        if (!sprepare)
            return ENOMEM;

        sprepare->func = prepare;
    }

    if (parent) {
        sparent = (atfork_func*)malloc(sizeof(atfork_func));
        if (!sparent) {
            if (prepare)
                free(sprepare);

            return ENOMEM;
        }

        sparent->func = parent;
    }

    if (child) {
        schild = (atfork_func*)malloc(sizeof(atfork_func));
        if (!schild) {
            if (prepare)
                free(sprepare);

            if (parent)
                free(sparent);

            return ENOMEM;
        }

        schild->func = child;
    }

    RtlAcquireSRWLockExclusive(&atfork_lock);

    if (prepare)
        insert_tail_list(prepare_atfork, sprepare->le);

    if (parent)
        insert_tail_list(parent_atfork, sparent->le);

    if (child)
        insert_tail_list(child_atfork, schild->le);

    RtlReleaseSRWLockExclusive(&atfork_lock);

    return 0;
}
