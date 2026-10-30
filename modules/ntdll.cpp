// SPDX-License-Identifier: MIT
module;

#include <stddef.h>
#include <stdint.h>

export module ntdll;

import nt;

export {

using PRTL_HEAP_COMMIT_ROUTINE = NTSTATUS (*)(const void* base, void** commit_address,
                                              size_t* commit_size);

struct RTL_HEAP_PARAMETERS {
    uint32_t Length;
    size_t SegmentReserve;
    size_t SegmentCommit;
    size_t DeCommitFreeBlockThreshold;
    size_t DeCommitTotalFreeThreshold;
    size_t MaximumAllocationSize;
    size_t VirtualMemoryThreshold;
    size_t InitialCommit;
    size_t InitialReserve;
    PRTL_HEAP_COMMIT_ROUTINE CommitRoutine;
    size_t Reserved[2];
};

inline constexpr uint32_t HEAP_NO_SERIALIZE = 0x00000001;
inline constexpr uint32_t HEAP_GROWABLE  = 0x00000002;
inline constexpr uint32_t HEAP_GENERATE_EXCEPTIONS = 0x00000004;
inline constexpr uint32_t HEAP_ZERO_MEMORY = 0x00000008;
inline constexpr uint32_t HEAP_REALLOC_IN_PLACE_ONLY = 0x00000010;
inline constexpr uint32_t HEAP_TAIL_CHECKING_ENABLED = 0x00000020;
inline constexpr uint32_t HEAP_FREE_CHECKING_ENABLED = 0x00000040;
inline constexpr uint32_t HEAP_DISABLE_COALESCE_ON_FREE = 0x00000080;
inline constexpr uint32_t HEAP_CREATE_ALIGN_16 = 0x00010000;
inline constexpr uint32_t HEAP_CREATE_ENABLE_TRACING = 0x00020000;
inline constexpr uint32_t HEAP_CREATE_ENABLE_EXECUTE = 0x00040000;

struct LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY InLoadOrderLinks;
    LIST_ENTRY InMemoryOrderLinks;
    LIST_ENTRY InInitializationOrderLinks;
    void* DllBase;
    void* EntryPoint;
    uint32_t SizeOfImage;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
    union {
        uint32_t Flags;
        struct {
            uint32_t PackagedBinary : 1;
            uint32_t MarkedForRemoval : 1;
            uint32_t ImageDll : 1;
            uint32_t LoadNotificationsSent : 1;
            uint32_t TelemetryEntryProcessed : 1;
            uint32_t ProcessStaticImport : 1;
            uint32_t InLegacyLists : 1;
            uint32_t InIndexes : 1;
            uint32_t ShimDll : 1;
            uint32_t InExceptionTable : 1;
            uint32_t ReservedFlags1 : 2;
            uint32_t LoadInProgress : 1;
            uint32_t LoadConfigProcessed : 1;
            uint32_t EntryProcessed : 1;
            uint32_t ProtectDelayLoad : 1;
            uint32_t ReservedFlags3 : 2;
            uint32_t DontCallForThreads : 1;
            uint32_t ProcessAttachCalled : 1;
            uint32_t ProcessAttachFailed : 1;
            uint32_t CorDeferredValidate : 1;
            uint32_t CorImage : 1;
            uint32_t DontRelocate : 1;
            uint32_t CorILOnly : 1;
            uint32_t ChpeImage : 1;
            uint32_t ReservedFlags5 : 2;
            uint32_t Redirected : 1;
            uint32_t ReservedFlags6 : 2;
            uint32_t CompatDatabaseProcessed : 1;
        };
    };
    uint16_t LoadCount;
    uint16_t TlsIndex;
    LIST_ENTRY HashLinks;
    uint32_t TimeDateStamp;
};

inline constexpr uint32_t DLL_PROCESS_DETACH = 0;
inline constexpr uint32_t DLL_PROCESS_ATTACH = 1;
inline constexpr uint32_t DLL_THREAD_ATTACH = 2;
inline constexpr uint32_t DLL_THREAD_DETACH = 3;

struct mn_image_info {
    void* base;
    uint64_t offset_debug_info;
    uint64_t size_debug_info;
    uint64_t offset_debug_abbrev;
    uint64_t size_debug_abbrev;
    uint64_t offset_debug_rnglists;
    uint64_t size_debug_rnglists;
    uint64_t offset_debug_str;
    uint64_t size_debug_str;
    uint64_t offset_debug_addr;
    uint64_t size_debug_addr;
    uint64_t offset_debug_str_offsets;
    uint64_t size_debug_str_offsets;
    uint64_t offset_debug_line;
    uint64_t size_debug_line;
    uint64_t offset_debug_line_str;
    uint64_t size_debug_line_str;
};

enum class EXCEPTION_DISPOSITION : int {
    ExceptionContinueExecution = 0,
    ExceptionContinueSearch = 1,
    ExceptionNestedException = 2,
    ExceptionCollidedUnwind = 3,
    ExceptionExecuteHandler = 4
};

inline constexpr int32_t EXCEPTION_EXECUTE_HANDLER = 1;
inline constexpr int32_t EXCEPTION_CONTINUE_SEARCH = 0;
inline constexpr int32_t EXCEPTION_CONTINUE_EXECUTION = -1;

inline constexpr uint32_t EH_NONCONTINUABLE = 0x00000001;
inline constexpr uint32_t EH_UNWINDING = 0x00000002;
inline constexpr uint32_t EH_EXIT_UNWIND = 0x00000004;
inline constexpr uint32_t EH_STACK_INVALID = 0x00000008;
inline constexpr uint32_t EH_NESTED_CALL = 0x00000010;
inline constexpr uint32_t EH_TARGET_UNWIND = 0x00000020;
inline constexpr uint32_t EH_COLLIDED_UNWIND = 0x00000040;

using EXCEPTION_ROUTINE = EXCEPTION_DISPOSITION(*)(EXCEPTION_RECORD*, void*,
                                                   CONTEXT*, void*);

struct EXCEPTION_POINTERS {
    EXCEPTION_RECORD* ExceptionRecord;
    CONTEXT* ContextRecord;
};

struct SCOPE_TABLE {
    uint32_t Count;
    struct {
        uint32_t BeginAddress;
        uint32_t EndAddress;
        uint32_t HandlerAddress;
        uint32_t JumpTarget;
    } ScopeRecord[1];
};

#ifdef __x86_64__
using EXCEPTION_FILTER = int32_t (*)(EXCEPTION_POINTERS* exception_pointers,
                                     void* establisher_frame);
using TERMINATION_HANDLER = void (*)(bool abnormal_termination,
                                     void* establisher_frame);
#elifdef __aarch64__
using EXCEPTION_FILTER = int32_t (*)(EXCEPTION_POINTERS* exception_pointers,
                                     uint64_t establisher_frame);
using TERMINATION_HANDLER = void (*)(bool abnormal_termination,
                                     uint64_t establisher_frame);
#endif

struct ANSI_STRING {
    uint16_t Length;
    uint16_t MaximumLength;
    char* Buffer;
};

struct RTL_CONDITION_VARIABLE {
    void* Ptr;
};

struct RTL_SRWLOCK {
    void* Ptr;
};

inline constexpr uint32_t PF_FLOATING_POINT_PRECISION_ERRATA = 0;
inline constexpr uint32_t PF_FLOATING_POINT_EMULATED = 1;
inline constexpr uint32_t PF_COMPARE_EXCHANGE_DOUBLE = 2;
inline constexpr uint32_t PF_MMX_INSTRUCTIONS_AVAILABLE = 3;
inline constexpr uint32_t PF_PPC_MOVEMEM_64BIT_OK = 4;
inline constexpr uint32_t PF_ALPHA_BYTE_INSTRUCTIONS = 5;
inline constexpr uint32_t PF_XMMI_INSTRUCTIONS_AVAILABLE = 6;
inline constexpr uint32_t PF_3DNOW_INSTRUCTIONS_AVAILABLE = 7;
inline constexpr uint32_t PF_RDTSC_INSTRUCTION_AVAILABLE = 8;
inline constexpr uint32_t PF_PAE_ENABLED = 9;
inline constexpr uint32_t PF_XMMI64_INSTRUCTIONS_AVAILABLE = 10;
inline constexpr uint32_t PF_SSE_DAZ_MODE_AVAILABLE = 11;
inline constexpr uint32_t PF_NX_ENABLED = 12;
inline constexpr uint32_t PF_SSE3_INSTRUCTIONS_AVAILABLE = 13;
inline constexpr uint32_t PF_COMPARE_EXCHANGE128 = 14;
inline constexpr uint32_t PF_COMPARE64_EXCHANGE128 = 15;
inline constexpr uint32_t PF_CHANNELS_ENABLED = 16;
inline constexpr uint32_t PF_XSAVE_ENABLED = 17;
inline constexpr uint32_t PF_ARM_VFP_32_REGISTERS_AVAILABLE = 18;
inline constexpr uint32_t PF_ARM_NEON_INSTRUCTIONS_AVAILABLE = 19;
inline constexpr uint32_t PF_SECOND_LEVEL_ADDRESS_TRANSLATION = 20;
inline constexpr uint32_t PF_VIRT_FIRMWARE_ENABLED = 21;
inline constexpr uint32_t PF_RDWRFSGSBASE_AVAILABLE = 22;
inline constexpr uint32_t PF_FASTFAIL_AVAILABLE = 23;
inline constexpr uint32_t PF_ARM_DIVIDE_INSTRUCTION_AVAILABLE = 24;
inline constexpr uint32_t PF_ARM_64BIT_LOADSTORE_ATOMIC = 25;
inline constexpr uint32_t PF_ARM_EXTERNAL_CACHE_AVAILABLE = 26;
inline constexpr uint32_t PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE = 27;
inline constexpr uint32_t PF_RDRAND_INSTRUCTION_AVAILABLE = 28;
inline constexpr uint32_t PF_ARM_V8_INSTRUCTIONS_AVAILABLE = 29;
inline constexpr uint32_t PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE = 30;
inline constexpr uint32_t PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE = 31;
inline constexpr uint32_t PF_RDTSCP_INSTRUCTION_AVAILABLE = 32;
inline constexpr uint32_t PF_RDPID_INSTRUCTION_AVAILABLE = 33;
inline constexpr uint32_t PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE = 34;
inline constexpr uint32_t PF_MONITORX_INSTRUCTION_AVAILABLE = 35;
inline constexpr uint32_t PF_SSSE3_INSTRUCTIONS_AVAILABLE = 36;
inline constexpr uint32_t PF_SSE4_1_INSTRUCTIONS_AVAILABLE = 37;
inline constexpr uint32_t PF_SSE4_2_INSTRUCTIONS_AVAILABLE = 38;
inline constexpr uint32_t PF_AVX_INSTRUCTIONS_AVAILABLE = 39;
inline constexpr uint32_t PF_AVX2_INSTRUCTIONS_AVAILABLE = 40;
inline constexpr uint32_t PF_AVX512F_INSTRUCTIONS_AVAILABLE = 41;
inline constexpr uint32_t PF_ERMS_AVAILABLE = 42;
inline constexpr uint32_t PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE = 43;
inline constexpr uint32_t PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE = 44;
inline constexpr uint32_t PF_ARM_V83_LRCPC_INSTRUCTIONS_AVAILABLE = 45;
inline constexpr uint32_t PF_ARM_SVE_INSTRUCTIONS_AVAILABLE = 46;
inline constexpr uint32_t PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE = 47;
inline constexpr uint32_t PF_ARM_SVE2_1_INSTRUCTIONS_AVAILABLE = 48;
inline constexpr uint32_t PF_ARM_SVE_AES_INSTRUCTIONS_AVAILABLE = 49;
inline constexpr uint32_t PF_ARM_SVE_PMULL128_INSTRUCTIONS_AVAILABLE = 50;
inline constexpr uint32_t PF_ARM_SVE_BITPERM_INSTRUCTIONS_AVAILABLE = 51;
inline constexpr uint32_t PF_ARM_SVE_BF16_INSTRUCTIONS_AVAILABLE = 52;
inline constexpr uint32_t PF_ARM_SVE_EBF16_INSTRUCTIONS_AVAILABLE = 53;
inline constexpr uint32_t PF_ARM_SVE_B16B16_INSTRUCTIONS_AVAILABLE = 54;
inline constexpr uint32_t PF_ARM_SVE_SHA3_INSTRUCTIONS_AVAILABLE = 55;
inline constexpr uint32_t PF_ARM_SVE_SM4_INSTRUCTIONS_AVAILABLE = 56;
inline constexpr uint32_t PF_ARM_SVE_I8MM_INSTRUCTIONS_AVAILABLE = 57;
inline constexpr uint32_t PF_ARM_SVE_F32MM_INSTRUCTIONS_AVAILABLE = 58;
inline constexpr uint32_t PF_ARM_SVE_F64MM_INSTRUCTIONS_AVAILABLE = 59;
inline constexpr uint32_t PF_BMI2_INSTRUCTIONS_AVAILABLE = 60;
inline constexpr uint32_t PF_MOVDIR64B_INSTRUCTION_AVAILABLE = 61;
inline constexpr uint32_t PF_ARM_LSE2_AVAILABLE = 62;
inline constexpr uint32_t PF_RESERVED_FEATURE = 63;
inline constexpr uint32_t PF_ARM_SHA3_INSTRUCTIONS_AVAILABLE = 64;
inline constexpr uint32_t PF_ARM_SHA512_INSTRUCTIONS_AVAILABLE = 65;
inline constexpr uint32_t PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE = 66;
inline constexpr uint32_t PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE = 67;
inline constexpr uint32_t PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE = 68;
inline constexpr uint32_t PF_ARM_V86_EBF16_INSTRUCTIONS_AVAILABLE = 69;
inline constexpr uint32_t PF_ARM_SME_INSTRUCTIONS_AVAILABLE = 70;
inline constexpr uint32_t PF_ARM_SME2_INSTRUCTIONS_AVAILABLE = 71;
inline constexpr uint32_t PF_ARM_SME2_1_INSTRUCTIONS_AVAILABLE = 72;
inline constexpr uint32_t PF_ARM_SME2_2_INSTRUCTIONS_AVAILABLE = 73;
inline constexpr uint32_t PF_ARM_SME_AES_INSTRUCTIONS_AVAILABLE = 74;
inline constexpr uint32_t PF_ARM_SME_SBITPERM_INSTRUCTIONS_AVAILABLE = 75;
inline constexpr uint32_t PF_ARM_SME_SF8MM4_INSTRUCTIONS_AVAILABLE = 76;
inline constexpr uint32_t PF_ARM_SME_SF8MM8_INSTRUCTIONS_AVAILABLE = 77;
inline constexpr uint32_t PF_ARM_SME_SF8DP2_INSTRUCTIONS_AVAILABLE = 78;
inline constexpr uint32_t PF_ARM_SME_SF8DP4_INSTRUCTIONS_AVAILABLE = 79;
inline constexpr uint32_t PF_ARM_SME_SF8FMA_INSTRUCTIONS_AVAILABLE = 80;
inline constexpr uint32_t PF_ARM_SME_F8F32_INSTRUCTIONS_AVAILABLE = 81;
inline constexpr uint32_t PF_ARM_SME_F8F16_INSTRUCTIONS_AVAILABLE = 82;
inline constexpr uint32_t PF_ARM_SME_F16F16_INSTRUCTIONS_AVAILABLE = 83;
inline constexpr uint32_t PF_ARM_SME_B16B16_INSTRUCTIONS_AVAILABLE = 84;
inline constexpr uint32_t PF_ARM_SME_F64F64_INSTRUCTIONS_AVAILABLE = 85;
inline constexpr uint32_t PF_ARM_SME_I16I64_INSTRUCTIONS_AVAILABLE = 86;
inline constexpr uint32_t PF_ARM_SME_LUTv2_INSTRUCTIONS_AVAILABLE = 87;
inline constexpr uint32_t PF_ARM_SME_FA64_INSTRUCTIONS_AVAILABLE = 88;

#ifdef __x86_64__

inline constexpr size_t UNWIND_HISTORY_TABLE_SIZE = 12;

struct UNWIND_HISTORY_TABLE_ENTRY {
    uint64_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
};

struct UNWIND_HISTORY_TABLE {
    uint32_t Count;
    uint8_t Search;
    uint64_t LowAddress;
    uint64_t HighAddress;
    UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
};

struct KNONVOLATILE_CONTEXT_POINTERS {
    union {
        M128A* FloatingContext[16];
        struct {
            M128A* Xmm0;
            M128A* Xmm1;
            M128A* Xmm2;
            M128A* Xmm3;
            M128A* Xmm4;
            M128A* Xmm5;
            M128A* Xmm6;
            M128A* Xmm7;
            M128A* Xmm8;
            M128A* Xmm9;
            M128A* Xmm10;
            M128A* Xmm11;
            M128A* Xmm12;
            M128A* Xmm13;
            M128A* Xmm14;
            M128A* Xmm15;
        };
    };
    union {
        uint64_t* IntegerContext[16];
        struct {
            uint64_t* Rax;
            uint64_t* Rcx;
            uint64_t* Rdx;
            uint64_t* Rbx;
            uint64_t* Rsp;
            uint64_t* Rbp;
            uint64_t* Rsi;
            uint64_t* Rdi;
            uint64_t* R8;
            uint64_t* R9;
            uint64_t* R10;
            uint64_t* R11;
            uint64_t* R12;
            uint64_t* R13;
            uint64_t* R14;
            uint64_t* R15;
        };
    };
};

struct DISPATCHER_CONTEXT_AMD64 {
    uint64_t ControlPc;
    uint64_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
    uint64_t EstablisherFrame;
    uint64_t TargetIp;
    AMD64_CONTEXT* ContextRecord;
    EXCEPTION_ROUTINE LanguageHandler;
    void* HandlerData;
    UNWIND_HISTORY_TABLE* HistoryTable;
    uint32_t ScopeIndex;
    uint32_t Fill0;
};

using DISPATCHER_CONTEXT = DISPATCHER_CONTEXT_AMD64;

#elifdef __aarch64__

inline constexpr size_t UNWIND_HISTORY_TABLE_SIZE = 12;

struct UNWIND_HISTORY_TABLE_ENTRY {
    uint64_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
};

struct UNWIND_HISTORY_TABLE {
    uint32_t Count;
    uint8_t LocalHint;
    uint8_t GlobalHint;
    uint8_t Search;
    uint8_t Once;
    uint64_t LowAddress;
    uint64_t HighAddress;
    UNWIND_HISTORY_TABLE_ENTRY Entry[UNWIND_HISTORY_TABLE_SIZE];
};

struct KNONVOLATILE_CONTEXT_POINTERS {
    uint64_t* X19;
    uint64_t* X20;
    uint64_t* X21;
    uint64_t* X22;
    uint64_t* X23;
    uint64_t* X24;
    uint64_t* X25;
    uint64_t* X26;
    uint64_t* X27;
    uint64_t* X28;
    uint64_t* Fp;
    uint64_t* Lr;
    uint64_t* D8;
    uint64_t* D9;
    uint64_t* D10;
    uint64_t* D11;
    uint64_t* D12;
    uint64_t* D13;
    uint64_t* D14;
    uint64_t* D15;
};

struct DISPATCHER_CONTEXT_ARM64 {
    uintptr_t ControlPc;
    uintptr_t ImageBase;
    RUNTIME_FUNCTION* FunctionEntry;
    uintptr_t EstablisherFrame;
    uintptr_t TargetPc;
    ARM64_NT_CONTEXT* ContextRecord;
    EXCEPTION_ROUTINE LanguageHandler;
    void* HandlerData;
    UNWIND_HISTORY_TABLE* HistoryTable;
    uint32_t ScopeIndex;
    bool ControlPcIsUnwound; // FIXME?
    uint8_t* NonVolatileRegisters;
};

using DISPATCHER_CONTEXT = DISPATCHER_CONTEXT_ARM64;

#endif

extern "C" {

void* RtlCreateHeap(uint32_t flags, void* heap_base, size_t reserve_size, size_t commit_size,
                    void* lock, RTL_HEAP_PARAMETERS* parameters);
void* RtlAllocateHeap(void* heap_handle, uint32_t flags, size_t size);
void* RtlReAllocateHeap(void* heap_handle, uint32_t flags, void* ptr,
                        size_t size);
bool RtlFreeHeap(void* heap_handle, uint32_t flags, void* base_address);
void* RtlDestroyHeap(void* heap_handle);
void RtlRaiseException(EXCEPTION_RECORD* exception_record);
void RtlCaptureContext(CONTEXT* context);
NTSTATUS LdrGetProcedureAddress(void* dll_handle, const ANSI_STRING* procedure_name,
                                uint32_t procedure_number, void** procedure_address);
EXCEPTION_DISPOSITION __C_specific_handler(EXCEPTION_RECORD* exception_record,
                                           void* establisher_frame,
                                           CONTEXT* context_record,
                                           DISPATCHER_CONTEXT* dispatcher_context);
void RtlExitUserProcess(uint32_t status);
nt_status RtlInitializeCriticalSection(RTL_CRITICAL_SECTION* critical_section);
nt_status RtlDeleteCriticalSection(RTL_CRITICAL_SECTION* critical_section);
nt_status RtlEnterCriticalSection(RTL_CRITICAL_SECTION* critical_section);
nt_status RtlLeaveCriticalSection(RTL_CRITICAL_SECTION* critical_section);
void RtlSetLastWin32Error(uint32_t err_code);
uint32_t RtlNtStatusToDosErrorNoTeb(nt_status status);
uint32_t RtlNtStatusToDosError(nt_status status);
void RtlInitializeConditionVariable(RTL_CONDITION_VARIABLE* var);
void RtlWakeAllConditionVariable(RTL_CONDITION_VARIABLE* var);
void RtlExitUserThread(nt_status exit_status) __attribute__((noreturn));
void RtlAcquirePebLock();
void RtlReleasePebLock();
void RtlAcquireLoaderLock();
void RtlReleaseLoaderLock();
nt_status RtlSleepConditionVariableCS(RTL_CONDITION_VARIABLE* variable,
                                      RTL_CRITICAL_SECTION* crit,
                                      const LARGE_INTEGER* timeout);
nt_status RtlSleepConditionVariableSRW(RTL_CONDITION_VARIABLE* var,
                                       RTL_SRWLOCK* lock,
                                       const LARGE_INTEGER* timeout,
                                       uint32_t flags);
void RtlWakeConditionVariable(RTL_CONDITION_VARIABLE* variable);
nt_status RtlWaitOnAddress(const void* addr, const void* cmp, size_t size,
                           const LARGE_INTEGER* timeout);
void RtlWakeConditionVariable(RTL_CONDITION_VARIABLE* variable);
void RtlWakeAddressSingle(const void* addr);
void RtlWakeAddressAll(const void* addr);
void* RtlPcToFileHeader(const void* pc, void** address);
nt_status LdrAddRefDll(uint32_t flags, HMODULE mod);
nt_status LdrUnloadDll(HMODULE mod);
void RtlAcquireSRWLockExclusive(RTL_SRWLOCK* lock);
void RtlReleaseSRWLockExclusive(RTL_SRWLOCK* lock);
bool RtlTryAcquireSRWLockExclusive(RTL_SRWLOCK* lock);
void RtlUnwindEx(void* target_frame, void* target_ip,
                 EXCEPTION_RECORD* exception_record, void* return_value,
                 CONTEXT* original_context,
                 UNWIND_HISTORY_TABLE* history_table);
RUNTIME_FUNCTION* RtlLookupFunctionEntry(uintptr_t pc, uintptr_t* base,
                                         UNWIND_HISTORY_TABLE* table);
EXCEPTION_ROUTINE RtlVirtualUnwind(uint32_t type, uint64_t image_base,
                                   uint64_t pc, RUNTIME_FUNCTION *func,
                                   CONTEXT* context, void** handler_data,
                                   uint64_t* establisher_frame,
                                   KNONVOLATILE_CONTEXT_POINTERS* ctx_ptr);
nt_status LdrFindEntryForAddress(void* address, LDR_DATA_TABLE_ENTRY** entry);
nt_status LdrGetDllHandle(const char16_t* dll_path, uint32_t* dll_characteristics,
                          const UNICODE_STRING* dll_name, void** dll_handle);
bool RtlIsProcessorFeaturePresent(uint32_t processor_feature);
bool RtlQueryPerformanceFrequency(LARGE_INTEGER* freq);
bool RtlQueryPerformanceCounter(LARGE_INTEGER* count);
nt_status LdrLoadDll(const char16_t* dll_path, uint32_t* dll_characteristics,
                     const UNICODE_STRING* dll_name, void** dll_handle);
void* RtlSecureZeroMemory(void* ptr, size_t count);
uint32_t RtlRandom(uint32_t* seed);
void* mn_allocate_aligned(void* heap_handle, size_t alignment, size_t size);
void RtlAcquireSRWLockShared(RTL_SRWLOCK* lock);
void RtlReleaseSRWLockShared(RTL_SRWLOCK* lock);
bool RtlTryAcquireSRWLockShared(RTL_SRWLOCK* lock);
nt_status mn_alarm(uint64_t time, uint64_t* left);
void mn_clear_alarm_on_fork();
uint64_t mn_get_alarm_left();

}

__inline void* GetProcessHeap() {
    return (void*)NtCurrentTeb()->ProcessEnvironmentBlock->ProcessHeap;
}

#ifndef KERNEL
struct mn_peb_lock {
    void lock() {
        RtlAcquirePebLock();
    }

    void unlock() {
        RtlReleasePebLock();
    }
};

struct mn_loader_lock {
    void lock() {
        RtlAcquireLoaderLock();
    }

    void unlock() {
        RtlReleaseLoaderLock();
    }
};
#endif

};
