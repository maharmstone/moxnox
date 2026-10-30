// SPDX-License-Identifier: MIT
module;

#include <stddef.h>
#include <stdint.h>

export module kernelbase;

import nt;
import ntdll;

export {

inline constexpr uint32_t CTRL_C_EVENT = 0;
inline constexpr uint32_t CTRL_BREAK_EVENT = 1;
inline constexpr uint32_t CTRL_CLOSE_EVENT = 2;
inline constexpr uint32_t CTRL_LOGOFF_EVENT = 5;
inline constexpr uint32_t CTRL_SHUTDOWN_EVENT = 6;

inline constexpr uint32_t DEBUG_PROCESS = 0x00000001;
inline constexpr uint32_t DEBUG_ONLY_THIS_PROCESS = 0x00000002;
inline constexpr uint32_t CREATE_SUSPENDED = 0x00000004;
inline constexpr uint32_t DETACHED_PROCESS = 0x00000008;
inline constexpr uint32_t CREATE_NEW_CONSOLE = 0x00000010;
inline constexpr uint32_t NORMAL_PRIORITY_CLASS = 0x00000020;
inline constexpr uint32_t IDLE_PRIORITY_CLASS = 0x00000040;
inline constexpr uint32_t HIGH_PRIORITY_CLASS = 0x00000080;
inline constexpr uint32_t REALTIME_PRIORITY_CLASS = 0x00000100;
inline constexpr uint32_t CREATE_NEW_PROCESS_GROUP = 0x00000200;
inline constexpr uint32_t CREATE_UNICODE_ENVIRONMENT = 0x00000400;
inline constexpr uint32_t CREATE_SEPARATE_WOW_VDM = 0x00000800;
inline constexpr uint32_t CREATE_SHARED_WOW_VDM = 0x00001000;
inline constexpr uint32_t CREATE_FORCEDOS = 0x00002000;
inline constexpr uint32_t BELOW_NORMAL_PRIORITY_CLASS = 0x00004000;
inline constexpr uint32_t ABOVE_NORMAL_PRIORITY_CLASS = 0x00008000;
inline constexpr uint32_t INHERIT_PARENT_AFFINITY = 0x00010000;
inline constexpr uint32_t INHERIT_CALLER_PRIORITY = 0x00020000;
inline constexpr uint32_t CREATE_PROTECTED_PROCESS = 0x00040000;
inline constexpr uint32_t EXTENDED_STARTUPINFO_PRESENT = 0x00080000;
inline constexpr uint32_t PROCESS_MODE_BACKGROUND_BEGIN = 0x00100000;
inline constexpr uint32_t PROCESS_MODE_BACKGROUND_END = 0x00200000;
inline constexpr uint32_t CREATE_SECURE_PROCESS = 0x00400000;
inline constexpr uint32_t CREATE_BREAKAWAY_FROM_JOB = 0x01000000;
inline constexpr uint32_t CREATE_PRESERVE_CODE_AUTHZ_LEVEL = 0x02000000;
inline constexpr uint32_t CREATE_DEFAULT_ERROR_MODE = 0x04000000;
inline constexpr uint32_t CREATE_NO_WINDOW = 0x08000000;
inline constexpr uint32_t PROFILE_USER = 0x10000000;
inline constexpr uint32_t PROFILE_KERNEL = 0x20000000;
inline constexpr uint32_t PROFILE_SERVER = 0x40000000;
inline constexpr uint32_t CREATE_IGNORE_SYSTEM_DEFAULT = 0x80000000;

struct SECURITY_ATTRIBUTES {
     uint32_t nLength;
     void* lpSecurityDescriptor;
     bool bInheritHandle;
};

struct STARTUPINFOW {
     uint32_t cb;
     char16_t* lpReserved;
     char16_t* lpDesktop;
     char16_t* lpTitle;
     uint32_t dwX;
     uint32_t dwY;
     uint32_t dwXSize;
     uint32_t dwYSize;
     uint32_t dwXCountChars;
     uint32_t dwYCountChars;
     uint32_t dwFillAttribute;
     uint32_t dwFlags;
     uint16_t wShowWindow;
     uint16_t cbReserved2;
     uint8_t* lpReserved2;
     HANDLE hStdInput;
     HANDLE hStdOutput;
     HANDLE hStdError;
};

inline constexpr uint32_t STARTF_USESHOWWINDOW = 0x00000001;
inline constexpr uint32_t STARTF_USESIZE = 0x00000002;
inline constexpr uint32_t STARTF_USEPOSITION = 0x00000004;
inline constexpr uint32_t STARTF_USECOUNTCHARS = 0x00000008;
inline constexpr uint32_t STARTF_USEFILLATTRIBUTE = 0x00000010;
inline constexpr uint32_t STARTF_RUNFULLSCREEN = 0x00000020;
inline constexpr uint32_t STARTF_FORCEONFEEDBACK = 0x00000040;
inline constexpr uint32_t STARTF_FORCEOFFFEEDBACK = 0x00000080;
inline constexpr uint32_t STARTF_USESTDHANDLES = 0x00000100;
inline constexpr uint32_t STARTF_USEHOTKEY = 0x00000200;
inline constexpr uint32_t STARTF_TITLEISLINKNAME = 0x00000800;
inline constexpr uint32_t STARTF_TITLEISAPPID = 0x00001000;
inline constexpr uint32_t STARTF_PREVENTPINNING = 0x00002000;
inline constexpr uint32_t STARTF_UNTRUSTEDSOURCE = 0x00008000;
inline constexpr uint32_t STARTF_HOLOGRAPHIC = 0x00040000;

struct PROCESS_INFORMATION {
     HANDLE hProcess;
     HANDLE hThread;
     uint32_t dwProcessId;
     uint32_t dwThreadId;
};

using HANDLER_ROUTINE = bool (*)(uint32_t ctrl_type);
using TOP_LEVEL_EXCEPTION_FILTER = int32_t (*)(EXCEPTION_POINTERS* exception_info);
using THREAD_START_ROUTINE = uint32_t (*)(void* ctx);

inline constexpr uint32_t TLS_OUT_OF_INDEXES = 0xffffffff;

inline constexpr uint32_t GET_MODULE_HANDLE_EX_FLAG_PIN = 0x00000001;
inline constexpr uint32_t GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT = 0x00000002;
inline constexpr uint32_t GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS = 0x00000004;

extern "C" {

TOP_LEVEL_EXCEPTION_FILTER SetUnhandledExceptionFilter(TOP_LEVEL_EXCEPTION_FILTER filter);
void InitializeCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void DeleteCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void EnterCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void LeaveCriticalSection(RTL_CRITICAL_SECTION* critical_section);
uint32_t TlsAlloc();
void* TlsGetValue(uint32_t tls_index);
bool TlsSetValue(uint32_t tls_index, void* tls_value);
bool TlsFree(uint32_t tls_index);
uint32_t GetLastError();
HANDLE CreateThread(SECURITY_ATTRIBUTES* sa, size_t stack,
                    THREAD_START_ROUTINE start, void* param, uint32_t flags,
                    uint32_t* id);
bool CreateProcessW(const char16_t* application_name, char16_t* command_line,
                    const SECURITY_ATTRIBUTES* process_attributes,
                    const SECURITY_ATTRIBUTES* thread_attributes,
                    bool inherit_handles, uint32_t creation_flags,
                    const void* environment, const char16_t* current_directory,
                    const STARTUPINFOW* startup_info,
                    PROCESS_INFORMATION* process_information);
void Sleep(uint32_t timeout);
uint32_t GetCurrentThreadId();
void RaiseException(uint32_t code, uint32_t flags, uint32_t count,
                    const uintptr_t* args);
size_t VirtualQuery(const void* addr, MEMORY_BASIC_INFORMATION* info,
                    size_t len);
bool VirtualProtect(void* addr, size_t size, uint32_t new_prot,
                    uint32_t* old_prot);

}

}
