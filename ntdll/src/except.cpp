// SPDX-License-Identifier: LGPL-2.1-or-later
#include "list.h"
#include <stdlib.h>
#include <stdint.h>
#include <optional>
#include <string_view>
#include <span>
#include <utility>
#include <mutex>
#include "../../modules/exception_record_offsets.h"

import format;
import dwarf;
import print;
import unicode;
import pe;
import ntdll;
import ntdll_private;
import crt;
import termios;
import moxnox_ioctls;

using namespace std;

static_assert(offsetof(EXCEPTION_RECORD, ExceptionCode) == EXCEPTION_RECORD_EXCEPTION_CODE);
static_assert(offsetof(EXCEPTION_RECORD, ExceptionFlags) == EXCEPTION_RECORD_EXCEPTION_FLAGS);
static_assert(offsetof(EXCEPTION_RECORD, ExceptionRecord) == EXCEPTION_RECORD_EXCEPTION_RECORD);
static_assert(offsetof(EXCEPTION_RECORD, ExceptionAddress) == EXCEPTION_RECORD_EXCEPTION_ADDRESS);
static_assert(offsetof(EXCEPTION_RECORD, NumberParameters) == EXCEPTION_RECORD_NUMBER_PARAMETERS);
static_assert(offsetof(EXCEPTION_RECORD, ExceptionInformation) == EXCEPTION_RECORD_EXCEPTION_INFORMATION);

static uint64_t get_preferred_base(ldr_entry& ent) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)ent.ldr.DllBase;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)ent.ldr.DllBase + dos_header.e_lfanew);

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return nt_header.OptionalHeader64.ImageBase;
    else
        return nt_header.OptionalHeader32.ImageBase;
}

static void print_rip(uint64_t rip) {
    LDR_DATA_TABLE_ENTRY* mod;
    nt_status status;
    char s[1024];
    auto sp = span(s, sizeof(s));

    {
        mn_loader_lock lock;
        lock_guard lg(lock);

        status = LdrFindEntryForAddress((void*)rip, &mod);
    }

    if (success(status)) {
        auto& ent = *CONTAINING_RECORD(mod, ldr_entry, ldr);
        char mod_name[255];
        uint64_t rva = rip - (uintptr_t)ent.ldr.DllBase;

        // FIXME - also get filename and line no. from DWARF

        auto u16sv_mod_name = u16string_view(ent.ldr.BaseDllName.Buffer, ent.ldr.BaseDllName.Length / sizeof(char16_t));

        utf16_to_utf8(mod_name, u16sv_mod_name);

        auto mod_name_sv = string_view(mod_name, utf16_to_utf8_len(u16sv_mod_name));

        auto debug_info = span((uint8_t*)ent.ldr.DllBase + ent.offset_debug_info, ent.size_debug_info);
        auto debug_abbrev = span((uint8_t*)ent.ldr.DllBase + ent.offset_debug_abbrev, ent.size_debug_abbrev);
        auto debug_rnglists = span((uint8_t*)ent.ldr.DllBase + ent.offset_debug_rnglists, ent.size_debug_rnglists);
        auto debug_addr = span((uint8_t*)ent.ldr.DllBase + ent.offset_debug_addr, ent.size_debug_addr);
        auto debug_str = span((uint8_t*)ent.ldr.DllBase + ent.offset_debug_str, ent.size_debug_str);
        auto debug_str_offsets = span((uint8_t*)ent.ldr.DllBase + ent.offset_debug_str_offsets, ent.size_debug_str_offsets);
        auto debug_line = span((uint8_t*)ent.ldr.DllBase + ent.offset_debug_line,
                               ent.size_debug_line);
        auto debug_line_str = span((uint8_t*)ent.ldr.DllBase + ent.offset_debug_line_str,
                                   ent.size_debug_line_str);

        auto base = get_preferred_base(ent);

        dwarf_print_function(debug_info, debug_abbrev, debug_rnglists,
                             debug_addr, debug_str, debug_str_offsets,
                             rva, base, (uintptr_t)ent.ldr.DllBase, sp,
                             mod_name_sv);

        sp = format(sp, " ");

        dwarf_print_line_no(debug_line, debug_line_str, debug_str,
                            debug_str_offsets, rva + base, sp);

        sp = format(sp, "\n");
    } else
        sp = format(sp, "{:016x}\n", rip);

    print_stderr("{}", string_view(s, (size_t)(sp.data() - s)));
}

optional<uint64_t> read_uint64_t(uint64_t addr) {
    NTSTATUS status;
    uint64_t ret;
    uint32_t bytes_read;

    status = NtReadVirtualMemory(NtCurrentProcess(), (void*)addr, &ret, sizeof(uint64_t),
                                 &bytes_read);
    if (!success(status) || bytes_read != sizeof(uint64_t))
        return nullopt;

    return ret;
}

static nt_status handle_exception(EXCEPTION_RECORD* exception,
                                  CONTEXT& orig_context) {
    CONTEXT context = orig_context;
    DISPATCHER_CONTEXT dispatcher_context;
    bool first_frame = true;

    while (true) {
        uint64_t image_base, frame;
        void* handler_data = nullptr;
#ifdef __x86_64__
        auto control_pc = context.Rip;
#elifdef __aarch64__
        auto control_pc = context.Pc;
#endif

        auto lookup_pc = control_pc;

        if (!first_frame) {
#ifdef __x86_64__
            lookup_pc--;
#elifdef __aarch64__
            lookup_pc -= sizeof(uint32_t);
#endif
        }

        first_frame = false;

        auto function_entry = RtlLookupFunctionEntry(lookup_pc, &image_base,
                                                     nullptr);
        if (!function_entry) { // leaf function
#ifdef __x86_64__
            context.Rip = *(uint64_t*)context.Rsp;
            context.Rsp += 8;

            if (context.Rip == 0)
                break;
#elifdef __aarch64__
            context.Pc = context.Lr;

            if (context.Pc == 0 || context.Pc == control_pc)
                break;
#endif

            continue;
        }

        CONTEXT context_before = context;

        auto handler = RtlVirtualUnwind(UNW_FLAG_EHANDLER, image_base,
                                        control_pc, function_entry,
                                        &context, &handler_data, &frame,
                                        nullptr);
        if (!handler)
            continue;

        dispatcher_context.ControlPc = control_pc;
        dispatcher_context.ImageBase = image_base;
        dispatcher_context.FunctionEntry = function_entry;
        dispatcher_context.EstablisherFrame = frame;
#ifdef __x86_64__
        dispatcher_context.TargetIp = 0;
#elifdef __aarch64__
        dispatcher_context.TargetPc = 0;
#endif
        dispatcher_context.ContextRecord = &context_before;
        dispatcher_context.LanguageHandler = handler;
        dispatcher_context.HandlerData = handler_data;
#ifdef __aarch64__
        dispatcher_context.ControlPcIsUnwound = lookup_pc != control_pc;
        dispatcher_context.NonVolatileRegisters = nullptr; // FIXME?
#endif

        auto disp = handler(exception, (void*)frame, &orig_context,
                            &dispatcher_context);

        switch (disp) {
            case EXCEPTION_DISPOSITION::ExceptionContinueExecution:
                // FIXME - check for EH_NONCONTINUABLE
                return nt_status::SUCCESS;

            case EXCEPTION_DISPOSITION::ExceptionContinueSearch:
                break;

            case EXCEPTION_DISPOSITION::ExceptionNestedException:
                print("FIXME - ExceptionNestedException\n");
                break;

            case EXCEPTION_DISPOSITION::ExceptionCollidedUnwind:
                print("FIXME - ExceptionCollidedUnwind\n");
                break;

            default:
                return nt_status::INVALID_DISPOSITION;
        }
    }

    return nt_status::UNHANDLED_EXCEPTION;
}

static bool stdout_is_tty() {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    auto status = NtDeviceIoControlFile(peb.ProcessParameters->StandardOutput,
                                        0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_PTY_ISATTY, nullptr, 0,
                                        nullptr, 0);
    return success(status);
}

extern "C"
void mn_signal(EXCEPTION_RECORD* exception, CONTEXT* context) {
    auto& peb = *NtCurrentTeb()->ProcessEnvironmentBlock;
    termios tc_settings;
    IO_STATUS_BLOCK iosb;
    char s[1024];
    auto sp = span(s, sizeof(s));

    auto status = handle_exception(exception, *context);
    if (success(status)) {
        NtContinue(context, false); // doesn't return
        unreachable();
    }

    // reset terminal settings

    memset(&tc_settings, 0, sizeof(tc_settings));

    tc_settings.c_iflag = ICRNL | IXON;
    tc_settings.c_oflag = OPOST | ONLCR;
    tc_settings.c_cflag = B38400 | CS8 | CREAD | HUPCL;
    tc_settings.c_lflag = ICANON | ECHO | ISIG | IEXTEN;

    tc_settings.c_cc[VINTR] = 'C' - 0x40;
    tc_settings.c_cc[VQUIT] = '\\' - 0x40;
    tc_settings.c_cc[VERASE] = 0x7f;
    tc_settings.c_cc[VKILL] = 'U' - 0x40;
    tc_settings.c_cc[VEOF] = 'D' - 0x40;
    tc_settings.c_cc[VSTART] = 'Q' - 0x40;
    tc_settings.c_cc[VSTOP] = 'S' - 0x40;
    tc_settings.c_cc[VSUSP] = 'Z' - 0x40;
    tc_settings.c_cc[VREPRINT] = 'R' - 0x40;
    tc_settings.c_cc[VDISCARD] = 'O' - 0x40;
    tc_settings.c_cc[VWERASE] = 'W' - 0x40;
    tc_settings.c_cc[VLNEXT] = 'V' - 0x40;
    tc_settings.c_cc[VMIN] = 1;

    tc_settings.c_ispeed = 38400;
    tc_settings.c_ospeed = 38400;

    NtDeviceIoControlFile(peb.ProcessParameters->StandardOutput, 0,
                          nullptr, nullptr, &iosb,
                          IOCTL_MOXNOX_PTY_TCSETATTR, &tc_settings,
                          sizeof(termios), nullptr, 0);

    if (stdout_is_tty()) {
        print("\e[?1049l" // disable alternative buffer
              "\e[?25h" // re-enable cursor
              "\e[0m" // reset colours and attributes
              "\e[?1000l" // disable mouse tracking
              "\e[?2004l" // disable bracketed paste mode
              "\e>\e[?1l" // reset application keypad and cursor key modes
              "\e7\e[r\e8" // reset scrolling region
        );
    }

    if (status != nt_status::UNHANDLED_EXCEPTION) {
        sp = format(sp, "Error {} received while trying to handle exception\n", status);
        print_stderr("{}", string_view(s, (size_t)(sp.data() - s)));
    }

    // FIXME - print EXE name

    if ((NTSTATUS)exception->ExceptionCode == nt_status::CONTROL_C_EXIT) {
        print_stderr("\nTerminated\n");
        NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
        unreachable();
    }

    if (exception->ExceptionCode == MOXNOX_STUB_FUNCTION) {
        auto func = string_view((char*)exception->ExceptionInformation[0]);

        print_stderr("Call to unimplemented function {}\n", func);
    } else {
        switch ((NTSTATUS)exception->ExceptionCode) {
            case nt_status::ACCESS_VIOLATION:
                // FIXME - translate error code to descriptive message
                sp = format(sp, "Page fault (address = {:x}, error code = {:x})\n",
                            exception->ExceptionInformation[1], exception->ExceptionInformation[0]);
            break;

            case nt_status::INTEGER_DIVIDE_BY_ZERO:
                sp = format(sp, "Divide by zero\n");
            break;

            case nt_status::ILLEGAL_INSTRUCTION:
                sp = format(sp, "Invalid opcode\n");
            break;

            case nt_status::BREAKPOINT:
                sp = format(sp, "Unhandled breakpoint\n");
            break;

            case nt_status::SINGLE_STEP:
                sp = format(sp, "Unhandled single-step breakpoint\n");
            break;

            case nt_status::BAD_STACK:
                sp = format(sp, "Stack alignment error\n");
            break;

            case nt_status::FLOAT_MULTIPLE_FAULTS:
            case nt_status::FLOAT_MULTIPLE_TRAPS:
                sp = format(sp, "SIMD exception\n");
            break;

#ifdef __aarch64__
            case nt_status::UNHANDLED_EXCEPTION: {
                union ARM64_ESR {
                    struct {
                        uint64_t ISS : 25;
                        uint64_t IL : 1;
                        uint64_t EC : 6;
                        uint64_t ISS2 : 24;
                    };
                    uint64_t value;
                } __attribute((packed)) esr;

                esr.value = exception->ExceptionInformation[0];

                sp = format(sp, "Unhandled exception (ESR_EL1 = {:08x} [ISS = {:x}, IL = {:x}, EC = {:x}, ISS2 = {:x}])\n",
                            esr.value, +esr.ISS, +esr.IL, +esr.EC, +esr.ISS2);
                break;
            }
#endif

            default:
                sp = format(sp, "Unhandled exception {:08x}\n", exception->ExceptionCode);
            break;
        }
    }

    sp = format(sp, "{}", *context);
    print_stderr("{}", string_view(s, (size_t)(sp.data() - s)));

    print_stderr("\n");

    UNWIND_HISTORY_TABLE history_table;
    bool first_frame = true;

    do {
        uint64_t image_base;

#ifdef __aarch64__
        uint64_t rip = context->Pc;
#elifdef __x86_64__
        uint64_t rip = context->Rip;
#endif
        print_rip(rip);

        if (rip == 0)
            break;

        auto lookup_pc = rip;

        if (!first_frame) {
#ifdef __x86_64__
            lookup_pc--;
#elifdef __aarch64__
            lookup_pc -= sizeof(uint32_t);
#endif
        }

        first_frame = false;

        auto func = RtlLookupFunctionEntry(lookup_pc, &image_base, &history_table);

        if (!func) {
#ifdef __aarch64__
            context->Pc = context->Lr;
            context->Lr = 0;

            if (context->Pc == rip)
                break;
#elifdef __x86_64__
            context->Rip = *(uint64_t*)context->Rsp;
            context->Rsp += sizeof(uint64_t);
#endif
        } else {
            void* data;
            uint64_t frame;

            RtlVirtualUnwind(0, image_base, rip, func, context, &data, &frame,
                             nullptr);
#ifdef __aarch64__
            if (context->Pc == rip || context->Pc == 0)
                break;
#endif
        }
    } while (true);

    NtTerminateProcess(NtCurrentProcess(), (NTSTATUS)exception->ExceptionCode);
    unreachable();
}

extern "C"
EXCEPTION_DISPOSITION __C_specific_handler(EXCEPTION_RECORD* rec, void* frame,
                                           CONTEXT* context_record,
                                           DISPATCHER_CONTEXT* dispatcher_context) {
    auto pc = dispatcher_context->ControlPc;
    auto image_base = dispatcher_context->ImageBase;
    auto& table = *(SCOPE_TABLE*)dispatcher_context->HandlerData;

    for (uint32_t i = 0; i < table.Count; i++) {
        auto& r = table.ScopeRecord[i];

        if (pc < image_base + r.BeginAddress || pc >= image_base + r.EndAddress)
            continue;

        if (rec->ExceptionFlags & (EH_UNWINDING | EH_EXIT_UNWIND)) {
            if (r.JumpTarget)
                continue;

#ifdef __x86_64__
            if (rec->ExceptionFlags & EH_TARGET_UNWIND &&
                dispatcher_context->TargetIp >= image_base + r.BeginAddress &&
                dispatcher_context->TargetIp < image_base + r.EndAddress) {
                break;
            }
#elifdef __aarch64__
            if (rec->ExceptionFlags & EH_TARGET_UNWIND &&
                dispatcher_context->TargetPc >= image_base + r.BeginAddress &&
                dispatcher_context->TargetPc < image_base + r.EndAddress) {
                break;
            }
#endif

            auto handler = (TERMINATION_HANDLER)(image_base + r.HandlerAddress);

            dispatcher_context->ScopeIndex = i + 1;

#ifdef __x86_64__
            handler(true, frame);
#elifdef __aarch64__
            handler(true, (uint64_t)frame);
#endif
        } else {
            if (!r.JumpTarget)
                continue;

            if (r.HandlerAddress != EXCEPTION_EXECUTE_HANDLER) {
                EXCEPTION_POINTERS exception_pointers;

                auto filter = (EXCEPTION_FILTER)(image_base + r.HandlerAddress);

                exception_pointers.ExceptionRecord = rec;
                exception_pointers.ContextRecord = context_record;

#ifdef __x86_64__
                auto ret = filter(&exception_pointers, frame);
#elifdef __aarch64__
                auto ret = filter(&exception_pointers, (uint64_t)frame);
#endif

                if (ret == EXCEPTION_CONTINUE_EXECUTION)
                    return EXCEPTION_DISPOSITION::ExceptionContinueExecution;
                else if (ret == EXCEPTION_CONTINUE_SEARCH)
                    continue;
                else if (ret != EXCEPTION_EXECUTE_HANDLER)
                    print_stderr("__C_specific_handler: filter returned unexpected value {}\n", ret);
            }

            RtlUnwindEx(frame, (uint8_t*)image_base + r.JumpTarget, rec, 0,
                        dispatcher_context->ContextRecord,
                        dispatcher_context->HistoryTable);
        }
    }

    return EXCEPTION_DISPOSITION::ExceptionContinueSearch;
}

#ifdef __x86_64__

enum class unwind_opcode {
    UWOP_PUSH_NONVOL = 0,
    UWOP_ALLOC_LARGE = 1,
    UWOP_ALLOC_SMALL = 2,
    UWOP_SET_FPREG = 3,
    UWOP_SAVE_NONVOL = 4,
    UWOP_SAVE_NONVOL_FAR = 5,
    UWOP_SAVE_XMM128 = 8,
    UWOP_SAVE_XMM128_FAR = 9,
    UWOP_PUSH_MACHFRAME = 10,
};

enum class unwind_register {
    rax = 0,
    rcx = 1,
    rdx = 2,
    rbx = 3,
    rsp = 4,
    rbp = 5,
    rsi = 6,
    rdi = 7,
    r8 = 8,
    r9 = 9,
    r10 = 10,
    r11 = 11,
    r12 = 12,
    r13 = 13,
    r14 = 14,
    r15 = 15
};

static uint64_t get_register(const CONTEXT& context, enum unwind_register reg) {
    switch (reg) {
        case unwind_register::rax:
            return context.Rax;
        case unwind_register::rcx:
            return context.Rcx;
        case unwind_register::rdx:
            return context.Rdx;
        case unwind_register::rbx:
            return context.Rbx;
        case unwind_register::rsp:
            return context.Rsp;
        case unwind_register::rbp:
            return context.Rbp;
        case unwind_register::rsi:
            return context.Rsi;
        case unwind_register::rdi:
            return context.Rdi;
        case unwind_register::r8:
            return context.R8;
        case unwind_register::r9:
            return context.R9;
        case unwind_register::r10:
            return context.R10;
        case unwind_register::r11:
            return context.R11;
        case unwind_register::r12:
            return context.R12;
        case unwind_register::r13:
            return context.R13;
        case unwind_register::r14:
            return context.R14;
        case unwind_register::r15:
            return context.R15;
    }

    return 0;
}

static void set_register(CONTEXT& context, enum unwind_register reg, uint64_t value) {
    switch (reg) {
        case unwind_register::rax:
            context.Rax = value;
            break;
        case unwind_register::rcx:
            context.Rcx = value;
            break;
        case unwind_register::rdx:
            context.Rdx = value;
            break;
        case unwind_register::rbx:
            context.Rbx = value;
            break;
        case unwind_register::rsp:
            context.Rsp = value;
            break;
        case unwind_register::rbp:
            context.Rbp = value;
            break;
        case unwind_register::rsi:
            context.Rsi = value;
            break;
        case unwind_register::rdi:
            context.Rdi = value;
            break;
        case unwind_register::r8:
            context.R8 = value;
            break;
        case unwind_register::r9:
            context.R9 = value;
            break;
        case unwind_register::r10:
            context.R10 = value;
            break;
        case unwind_register::r11:
            context.R11 = value;
            break;
        case unwind_register::r12:
            context.R12 = value;
            break;
        case unwind_register::r13:
            context.R13 = value;
            break;
        case unwind_register::r14:
            context.R14 = value;
            break;
        case unwind_register::r15:
            context.R15 = value;
            break;
    }
}

#endif

extern "C"
void RtlUnwindEx(void* target_frame, void* target_ip,
                 EXCEPTION_RECORD* exception_record, void* return_value,
                 CONTEXT* context, UNWIND_HISTORY_TABLE* history_table) {
    CONTEXT new_context;
    EXCEPTION_RECORD er;
    uint64_t frame;

    RtlCaptureContext(context);
    new_context = *context;

    if (!exception_record) {
        memset(&er, 0, sizeof(er));
        er.ExceptionCode = (uint32_t)nt_status::UNWIND;
        er.ExceptionFlags = EH_UNWINDING;
#ifdef __x86_64__
        er.ExceptionAddress = (void*)context->Rip;
#elifdef __aarch64__
        er.ExceptionAddress = (void*)context->Pc;
#endif
        exception_record = &er;
    } else
        exception_record->ExceptionFlags |= EH_UNWINDING;

    if (!target_frame)
        exception_record->ExceptionFlags |= EH_EXIT_UNWIND;

#ifdef __x86_64__
    frame = context->Rsp;
#elifdef __aarch64__
    frame = context->Sp;
#endif

    bool first_frame = true;

    while (true) {
        uint64_t image_base;
        void* data = nullptr;

#ifdef __x86_64__
        auto lookup_pc = context->Rip;

        if (!first_frame)
            lookup_pc--;
#elifdef __aarch64__
        auto lookup_pc = context->Pc;

        if (!first_frame)
            lookup_pc -= sizeof(uint32_t);
#endif

        first_frame = false;

        auto function_entry = RtlLookupFunctionEntry(lookup_pc, &image_base,
                                                     history_table);
        if (!function_entry) {
#ifdef __x86_64__
            frame = new_context.Rsp;
            new_context.Rip = *(uint64_t *)new_context.Rsp;
            new_context.Rsp += sizeof(uint64_t);

            if (!new_context.Rip)
                break;
#elifdef __aarch64__
            frame = new_context.Sp;
            new_context.Pc = new_context.Lr;
            new_context.Lr = 0;

            if (!new_context.Pc)
                break;
#endif

            if (!frame)
                break;

            if (frame == (uint64_t)target_frame)
                break;

            *context = new_context;

            continue;
        }

#ifdef __x86_64__
        auto control_pc = new_context.Rip;
#elifdef __aarch64__
        auto control_pc = new_context.Pc;
#endif

        auto handler = RtlVirtualUnwind(UNW_FLAG_UHANDLER, image_base,
                                        control_pc, function_entry,
                                        &new_context, &data, &frame, nullptr);
        if (!frame)
            break;

        if (target_frame && frame == (uint64_t)target_frame)
            exception_record->ExceptionFlags |= EH_TARGET_UNWIND;

        if (handler) {
            DISPATCHER_CONTEXT dispatcher_context;
            dispatcher_context.ControlPc = control_pc;
            dispatcher_context.ImageBase = image_base;
            dispatcher_context.FunctionEntry = function_entry;
            dispatcher_context.EstablisherFrame = frame;
#ifdef __x86_64__
            dispatcher_context.TargetIp = (uint64_t)target_ip;
#elifdef __aarch64__
            dispatcher_context.TargetPc = (uint64_t)target_ip;
#endif
            dispatcher_context.ContextRecord = context;
            dispatcher_context.LanguageHandler = handler;
            dispatcher_context.HandlerData = data;
            dispatcher_context.HistoryTable = history_table;
            dispatcher_context.ScopeIndex = 0;
#ifdef __aarch64__
            dispatcher_context.ControlPcIsUnwound = lookup_pc != control_pc;
            dispatcher_context.NonVolatileRegisters = nullptr; // FIXME?
#endif

            handler(exception_record, (void*)frame, context,
                    &dispatcher_context);
        }

        if (target_frame && frame == (uint64_t)target_frame)
            break;

        *context = new_context;
    }

#ifdef __x86_64__
    context->Rax = (uint64_t)return_value;
#elifdef __aarch64__
    context->X0 = (uint64_t)return_value;
#endif

    if (target_ip) {
#ifdef __x86_64__
        context->Rip = (uint64_t)target_ip;
#elifdef __aarch64__
        context->Pc = (uint64_t)target_ip;
#endif
    }

    NtContinue(context, false);
}

static span<const uint8_t> find_pdata(void* img_addr) {
    const auto& dos_header = *(IMAGE_DOS_HEADER*)img_addr;

    if (dos_header.e_magic != IMAGE_DOS_SIGNATURE)
        return span<const uint8_t>();

    const auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)img_addr + dos_header.e_lfanew);

    if (nt_header.Signature != IMAGE_NT_SIGNATURE)
        return span<const uint8_t>();

    span<const IMAGE_SECTION_HEADER> sections;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader64 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);
    else if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
        sections = span((IMAGE_SECTION_HEADER*)((uint8_t*)&nt_header.OptionalHeader32 + nt_header.FileHeader.SizeOfOptionalHeader), nt_header.FileHeader.NumberOfSections);

    // FIXME - probably should be looking in data directory rather than going off name

    for (const auto& sect : sections) {
        if (string_view(sect.Name) == ".pdata")
            return span((uint8_t*)img_addr + sect.VirtualAddress, sect.VirtualSize);
    }

    return span<const uint8_t>();
}

extern "C"
RUNTIME_FUNCTION* RtlLookupFunctionEntry(uintptr_t pc, uintptr_t* base,
                                         UNWIND_HISTORY_TABLE* /*history_table*/) {
    void* img_base;

    // FIXME - history_table?

    if (!RtlPcToFileHeader((void*)pc, &img_base))
        return nullptr;

    *base = (uintptr_t)img_base;

    pc -= (uintptr_t)*base;

    // FIXME - how do JITted programs provide their pdata?

    auto pdata = find_pdata(img_base);
    if (pdata.empty())
        return nullptr;

    span<RUNTIME_FUNCTION> rt_funcs((RUNTIME_FUNCTION*)pdata.data(),
                                    pdata.size() / sizeof(RUNTIME_FUNCTION));

    for (auto& f : rt_funcs) {
#ifdef __x86_64__
        if (pc >= f.BeginAddress && pc < f.EndAddress)
            return &f;

        if (f.BeginAddress > pc) // stored in order
            break;
#elifdef __aarch64__
        uint32_t length;

        if (f.BeginAddress > pc) // stored in order
            break;

        if (f.Flag) // packed unwind data
            length = f.FunctionLength * 4;
        else {
            const auto& xdata = *(IMAGE_ARM64_RUNTIME_FUNCTION_ENTRY_XDATA*)((uintptr_t)img_base + f.UnwindData);

            length = xdata.FunctionLength * 4;
        }

        if (pc >= f.BeginAddress && pc < f.BeginAddress + length)
            return &f;
#endif
    }

    return nullptr;
}

#ifdef __x86_64__

extern "C"
EXCEPTION_ROUTINE RtlVirtualUnwind(uint32_t type, uint64_t image_base,
                                   uint64_t pc, RUNTIME_FUNCTION* func,
                                   CONTEXT* context, void** handler_data,
                                   uint64_t* establisher_frame,
                                   KNONVOLATILE_CONTEXT_POINTERS* /*ctx_ptr*/) {
    bool machine_frame = false;

    auto& ui = *(UNWIND_INFO*)((uint8_t*)image_base + func->UnwindInfoAddress);

    if (ui.version != 1)
        return nullptr;

    uint64_t frame;

    if (ui.frame_register == 0)
        frame = context->Rsp;
    else {
        frame = get_register(*context, (enum unwind_register)ui.frame_register);
        frame -= ui.frame_offset * 16;
    }

    *establisher_frame = frame;

    auto func_offset = (uint32_t)(pc - (uintptr_t)image_base - func->BeginAddress);

    span codes((UNWIND_CODE*)((uint8_t*)image_base + func->UnwindInfoAddress + sizeof(UNWIND_INFO)),
               ui.unwind_codes_count);

    for (size_t i = 0; i < codes.size(); i++) {
        const auto& c = codes[i];

        // FIXME - skip some codes if we're in prologue

        if (c.offset > func_offset)
            continue;

        switch ((enum unwind_opcode)c.code) {
            case unwind_opcode::UWOP_PUSH_NONVOL: { // push reg (.seh_pushreg)
                auto val = read_uint64_t(context->Rsp);

                if (!val.has_value())
                    return nullptr;

                set_register(*context, (enum unwind_register)c.info, *val);
                context->Rsp += sizeof(uint64_t);
                break;
            }

            case unwind_opcode::UWOP_ALLOC_LARGE: // sub rsp, n (.seh_stackalloc)
                if (c.info == 0) {
                    if (i == codes.size() - 1)
                        return nullptr;

                    context->Rsp += *(uint16_t*)&codes[i + 1] * 8;
                    i++;
                } else {
                    if (i >= codes.size() - 2)
                        return nullptr;

                    context->Rsp += *(uint32_t*)&codes[i + 1];
                    i += 2;
                }
                break;

            case unwind_opcode::UWOP_ALLOC_SMALL: // sub rsp, n (.seh_stackalloc)
                context->Rsp += (c.info * 8) + 8;
                break;

            case unwind_opcode::UWOP_SET_FPREG: // lea framereg, [rsp + n] (.seh_setframe)
                context->Rsp = frame;
                break;

            case unwind_opcode::UWOP_SAVE_NONVOL: { // mov [rsp+n], reg (.seh_savereg)
                if (i == codes.size() - 1)
                    return nullptr;

                auto val = read_uint64_t(frame + (*(uint16_t*)&codes[i + 1] * 8));

                if (!val.has_value())
                    return nullptr;

                set_register(*context, (enum unwind_register)c.info, *val);
                i++;
                break;
            }

            case unwind_opcode::UWOP_SAVE_NONVOL_FAR: { // mov [rsp+n], reg (.seh_savereg)
                if (i >= codes.size() - 2)
                    return nullptr;

                auto val = read_uint64_t(frame + *(uint32_t*)&codes[i + 1]);

                if (!val.has_value())
                    return nullptr;

                set_register(*context, (enum unwind_register)c.info, *val);
                i += 2;
                break;
            }

            case unwind_opcode::UWOP_SAVE_XMM128: // movaps [rsp+n], xmmreg (.seh_savexmm)
                if (i == codes.size() - 1)
                    return nullptr;

                // FIXME - ctx_ptr?

                i++;
                break;

            case unwind_opcode::UWOP_SAVE_XMM128_FAR: // movaps [rsp+n], xmmreg (.seh_savexmm)
                if (i >= codes.size() - 2)
                    return nullptr;

                // FIXME - ctx_ptr?

                i += 2;
                break;

            case unwind_opcode::UWOP_PUSH_MACHFRAME: { // interrupt (.seh_pushframe)
                if (c.info != 0)
                    context->Rsp += sizeof(uint64_t);

                auto val = read_uint64_t(context->Rsp);

                if (!val.has_value())
                    return nullptr;

                context->Rip = *val;

                val = read_uint64_t(context->Rsp + (3 * sizeof(uint64_t)));

                if (!val.has_value())
                    return nullptr;

                context->Rsp = *val;
                machine_frame = true;
                break;
            }

            default:
                return nullptr;
        }
    }

    if (!machine_frame) {
        context->Rip = *(uint64_t*)context->Rsp;
        context->Rsp += 8;
    }

    auto ptr = (uint8_t*)&ui + sizeof(UNWIND_INFO) + (ui.unwind_codes_count * sizeof(UNWIND_CODE));

    if (ui.unwind_codes_count % 2)
        ptr += sizeof(UNWIND_CODE);

    *handler_data = (uint32_t*)(ptr + sizeof(uint32_t));

    if (!(ui.flags & type))
        return nullptr;

    // FIXME - also return nullptr if inside prologue

    return (EXCEPTION_ROUTINE)(image_base + *(uint32_t*)ptr);
}

#elifdef __aarch64__

static void unwind_context_packed(CONTEXT& context, const RUNTIME_FUNCTION& rf) {
    // FIXME - unwinding in prologue or epilogue

    size_t intsz = rf.RegI * sizeof(uint64_t);

    if (rf.CR == 1)
        intsz += sizeof(uint64_t); // lr

    size_t fpsz = rf.RegF ? ((rf.RegF + 1) * sizeof(uint64_t)) : 0;
    size_t savsz = intsz + fpsz;

    if (rf.H)
        savsz += 8 * sizeof(uint64_t);

    savsz = (savsz + 0xf) & ~(size_t)0xf;

    size_t locsz = (rf.FrameSize * 16) - savsz;

    if (rf.CR == 2 || rf.CR == 3) { // step 6a, 6b, or 6c
        // mov sp, x29
        context.Sp = context.X[29];

        // ldr x29, [sp]
        if (auto ret = read_uint64_t(context.Sp); ret.has_value())
            context.X[29] = *ret;
        else
            return;

        // ldr lr, [sp, #8]
        if (auto ret = read_uint64_t(context.Sp + 8); ret.has_value())
            context.Lr = *ret;
        else
            return;
    }

    // step 6
    context.Sp += locsz;

    // can ignore case when H == 1 (step 5)

    if (rf.RegF > 0) { // step 4
        for (unsigned int i = 0; i < rf.RegF + 1; i++) {
            // ldr d(8+i), [sp, #intsz + (i * 8)]
            if (auto ret = read_uint64_t(context.Sp + intsz + (i * 8)); ret.has_value())
                *(uint64_t*)&context.V[8 + i].D[0] = *ret;
            else
                return;
        }
    }

    if (rf.CR == 1) {
        // ldr lr, [sp,#(intsz-8)]

        if (auto ret = read_uint64_t(context.Sp + intsz - 8); ret.has_value())
            context.Lr = *ret;
        else
            return;
    }

    if (rf.RegI > 0) { // step 2
        for (unsigned int i = 0; i < rf.RegI; i++) {
            // ldr x(19 + i), [sp, #savsz + (i * 8)]
            if (auto ret = read_uint64_t(context.Sp + (i * 8)); ret.has_value())
                context.X[19 + i] = *ret;
            else
                return;
        }
    }

    context.Sp += savsz;

    if (rf.CR == 2) { // step 1
        // FIXME - pacibsp
        return;
    }
}

extern "C"
EXCEPTION_ROUTINE RtlVirtualUnwind(uint32_t type, uint64_t image_base,
                                   uint64_t /*pc*/, RUNTIME_FUNCTION* func,
                                   CONTEXT* context, void** handler_data,
                                   uint64_t* establisher_frame,
                                   KNONVOLATILE_CONTEXT_POINTERS* /*ctx_ptr*/) {
    // FIXME - handle case when address in prologue or epilogue (don't ignore pc)

    struct aarch64_seh_extended_counts {
        uint16_t epilogue_count;
        uint8_t code_words;
        uint8_t reserved;
    };

    if (func->Flag) {
        unwind_context_packed(*context, *func);

        context->Pc = context->Lr;
        *establisher_frame = context->Sp;
        *handler_data = nullptr;

        return nullptr;
    }

    const auto& xdata = *(IMAGE_ARM64_RUNTIME_FUNCTION_ENTRY_XDATA*)(image_base + func->UnwindData);

    if (xdata.Version != 0)
        return nullptr;

    uint8_t code_words;
    uint16_t epilogue_count;
    size_t off = 0;

    if (xdata.EpilogInHeader) {
        code_words = xdata.CodeWords;
        epilogue_count = 0;
    } else if (xdata.EpilogCount != 0 || xdata.CodeWords != 0) {
        code_words = xdata.CodeWords;
        epilogue_count = xdata.EpilogCount;
    } else {
        auto& ec = *(aarch64_seh_extended_counts*)((uint8_t*)&xdata + sizeof(uint32_t));

        off = sizeof(aarch64_seh_extended_counts);

        epilogue_count = ec.epilogue_count;
        code_words = ec.code_words;
    }

    span codes((uint8_t*)&xdata + sizeof(uint32_t) + off + (epilogue_count * sizeof(uint32_t)),
               code_words * sizeof(uint32_t));

    for (size_t i = 0; i < codes.size(); i++) {
        switch (codes[i]) {
            case 0x00 ... 0x1f:
                // alloc_s
                context->Sp += codes[i] * 16;
                break;

            case 0x20 ... 0x3f: {
                // save_r19r20_x

                uint8_t z = codes[i] & 0x1f;

                if (auto ret = read_uint64_t(context->Sp); ret.has_value())
                    context->X[19] = *ret;
                else
                    return nullptr;

                if (auto ret = read_uint64_t(context->Sp + 8); ret.has_value())
                    context->X[20] = *ret;
                else
                    return nullptr;

                context->Sp += z * 8;

                break;
            }

            case 0x40 ... 0x7f: {
                uint32_t z = codes[i] & 0x3f;

                // save_fplr

                if (auto ret = read_uint64_t(context->Sp + (z * 8)); ret.has_value())
                    context->Fp = *ret;
                else
                    return nullptr;

                if (auto ret = read_uint64_t(context->Sp + ((z + 1) * 8)); ret.has_value())
                    context->Lr = *ret;
                else
                    return nullptr;

                break;
            }

            case 0x80 ... 0xbf: {
                uint32_t z = codes[i] & 0x3f;

                // save_fplr_x

                if (auto ret = read_uint64_t(context->Sp); ret.has_value())
                    context->Fp = *ret;
                else
                    return nullptr;

                if (auto ret = read_uint64_t(context->Sp + 8); ret.has_value())
                    context->Lr = *ret;
                else
                    return nullptr;

                context->Sp += (z + 1) * 8;

                break;
            }

            case 0xc0 ... 0xc7: {
                if (i == codes.size() - 1)
                    return nullptr;

                // alloc_m

                auto x = (uint16_t)(((codes[i] & 7) << 8) | codes[i + 1]);

                context->Sp += x * 16;

                i++;
                break;
            }

            case 0xc8 ... 0xcb: {
                if (i == codes.size() - 1)
                    return nullptr;

                // save_regp

                auto x = (uint8_t)(((codes[i] & 3) << 2) | (codes[i + 1] >> 6));
                uint8_t z = codes[i + 1] & 0x3f;

                if (auto ret = read_uint64_t(context->Sp + (z * 8)); ret.has_value())
                    context->X[19 + x] = *ret;
                else
                    return nullptr;

                if (auto ret = read_uint64_t(context->Sp + ((z + 1) * 8)); ret.has_value())
                    context->X[20 + x] = *ret;
                else
                    return nullptr;

                i++;
                break;
            }

            case 0xcc ... 0xcf: {
                if (i == codes.size() - 1)
                    return nullptr;

                // save_regp_x

                auto x = (uint8_t)(((codes[i] & 3) << 2) | (codes[i + 1] >> 6));
                uint8_t z = codes[i + 1] & 0x3f;

                if (auto ret = read_uint64_t(context->Sp); ret.has_value())
                    context->X[19 + x] = *ret;
                else
                    return nullptr;

                if (auto ret = read_uint64_t(context->Sp + 8); ret.has_value())
                    context->X[20 + x] = *ret;
                else
                    return nullptr;

                context->Sp += (z + 1) * 8;

                i++;
                break;
            }

            case 0xd0 ... 0xd3: {
                if (i == codes.size() - 1)
                    return nullptr;

                // save_reg

                auto x = (uint8_t)(((codes[i] & 3) << 2) | (codes[i + 1] >> 6));
                uint8_t z = codes[i + 1] & 0x3f;

                if (auto ret = read_uint64_t(context->Sp + (z * 8)); ret.has_value())
                    context->X[19 + x] = *ret;
                else
                    return nullptr;

                i++;
                break;
            }

            case 0xd4 ... 0xd5: {
                if (i == codes.size() - 1)
                    return nullptr;

                auto x = (uint8_t)(((codes[i] & 1) << 3) | (codes[i + 1] >> 5));
                uint8_t z = codes[i + 1] & 0x1f;

                // save_reg_x

                if (auto ret = read_uint64_t(context->Sp); ret.has_value())
                    context->X[19 + x] = *ret;
                else
                    return nullptr;

                context->Sp += (z + 1) * 8;

                i++;
                break;
            }

            case 0xd6 ... 0xd7: {
                if (i == codes.size() - 1)
                    return nullptr;

                // save_lrpair

                auto x = (uint8_t)(((codes[i] & 1) << 2) | (codes[i + 1] >> 6));
                uint8_t z = codes[i + 1] & 0x3f;

                if (auto ret = read_uint64_t(context->Sp + (z * 8)); ret.has_value())
                    context->X[19 + (2 * x)] = *ret;
                else
                    return nullptr;

                if (auto ret = read_uint64_t(context->Sp + ((z + 1) * 8)); ret.has_value())
                    context->Lr = *ret;
                else
                    return nullptr;

                i++;
                break;
            }

            case 0xd8 ... 0xd9: {
                if (i == codes.size() - 1)
                    return nullptr;

                // save_fregp

                auto x = (uint8_t)(((codes[i] & 1) << 2) | (codes[i + 1] >> 6));
                uint8_t z = codes[i + 1] & 0x3f;

                if (auto ret = read_uint64_t(context->Sp + (z * 8)); ret.has_value())
                    *(uint64_t*)&context->V[8 + x].D[0] = *ret;
                else
                    return nullptr;

                if (auto ret = read_uint64_t(context->Sp + ((z + 1) * 8)); ret.has_value())
                    *(uint64_t*)&context->V[9 + x].D[0] = *ret;
                else
                    return nullptr;

                i++;
                break;
            }

            case 0xda ... 0xdb: {
                if (i == codes.size() - 1)
                    return nullptr;

                // save_fregp_x

                auto x = (uint8_t)(((codes[i] & 1) << 2) | (codes[i + 1] >> 6));
                uint8_t z = codes[i + 1] & 0x3f;

                if (auto ret = read_uint64_t(context->Sp); ret.has_value())
                    *(uint64_t*)&context->V[8 + x].D[0] = *ret;
                else
                    return nullptr;

                if (auto ret = read_uint64_t(context->Sp + 8); ret.has_value())
                    *(uint64_t*)&context->V[9 + x].D[0] = *ret;
                else
                    return nullptr;

                context->Sp += (z + 1) * 8;

                i++;
                break;
            }

            case 0xdc ... 0xdd: {
                if (i == codes.size() - 1)
                    return nullptr;

                // save_freg

                auto x = (uint8_t)(((codes[i] & 1) << 2) | (codes[i + 1] >> 6));
                uint8_t z = codes[i + 1] & 0x3f;

                if (auto ret = read_uint64_t(context->Sp + (z * 8)); ret.has_value())
                    *(uint64_t*)&context->V[8 + x].D[0] = *ret;
                else
                    return nullptr;

                i++;
                break;
            }

            case 0xde: {
                if (i == codes.size() - 1)
                    return nullptr;

                // save_freg_x

                uint8_t x = codes[i + 1] >> 5;
                uint8_t z = codes[i + 1] & 0x1f;

                if (auto ret = read_uint64_t(context->Sp); ret.has_value())
                    *(uint64_t*)&context->V[8 + x].D[0] = *ret;
                else
                    return nullptr;

                context->Sp += (z + 1) * 8;

                i++;
                break;
            }

            case 0xdf:
                if (i == codes.size() - 1)
                    return nullptr;

                // FIXME - alloc_z

                i++;
                return nullptr;

            case 0xe0: {
                if (i > codes.size() - 4)
                    return nullptr;

                // alloc_l

                auto size = (uint64_t)((codes[i + 1] << 16) | (codes[i + 2] << 8) | codes[i + 3]);

                context->Sp += size * 16;

                i += 3;
                break;
            }

            case 0xe1:
                // set_fp
                context->Sp = context->Fp;
                break;

            case 0xe2:
                if (i == codes.size() - 1)
                    return nullptr;

                // add_fp
                context->Sp = context->Fp - (codes[i + 1] * 8);

                i++;
                break;

            case 0xe3:
                // nop
                break;

            case 0xe4: // end
            case 0xe5: // end_c
                i = codes.size() - 1;
                break;

            case 0xe6: {
                unsigned int next_chain = 1;

                // save_next

                while (i + next_chain < codes.size()) {
                    if (codes[i + next_chain] == 0xe6)
                        next_chain++;
                    else
                        break;
                }

                if (i + next_chain == codes.size())
                    return nullptr;

                switch (codes[i + next_chain]) {
                    case 0x20 ... 0x3f: { // save_r19r20_x
                        // save_next handles only the extra pair at [Sp + next_chain*16].
                        // The base opcode (save_r19r20_x) is processed by the main loop.

                        if (next_chain >= 6)
                            return nullptr;

                        uint64_t base = context->Sp + (next_chain * 16);

                        if (auto ret = read_uint64_t(base); ret.has_value())
                            context->X[19 + (next_chain * 2)] = *ret;
                        else
                            return nullptr;

                        if (auto ret = read_uint64_t(base + 8); ret.has_value())
                            context->X[20 + (next_chain * 2)] = *ret;
                        else
                            return nullptr;

                        // No Sp change; leave i so the main loop processes the base opcode next.
                        break;
                    }

                    case 0xc8 ... 0xcb: { // save_regp
                        if (i + next_chain == codes.size() - 1)
                            return nullptr;

                        auto x = (uint8_t)(((codes[i + next_chain] & 3) << 2) | (codes[i + next_chain + 1] >> 6));
                        auto z = (uint8_t)((codes[i + next_chain + 1] & 0x3f) + (next_chain * 2));

                        if ((next_chain * 2) + x >= 11)
                            return nullptr;

                        if (auto ret = read_uint64_t(context->Sp + (z * 8)); ret.has_value())
                            context->X[19 + (next_chain * 2) + x] = *ret;
                        else
                            return nullptr;

                        if (auto ret = read_uint64_t(context->Sp + ((z + 1) * 8)); ret.has_value())
                            context->X[20 + (next_chain * 2) + x] = *ret;
                        else
                            return nullptr;

                        // No Sp change; leave i so the main loop processes the base opcode next.
                        break;
                    }

                    case 0xcc ... 0xcf: { // save_regp_x
                        if (i + next_chain == codes.size() - 1)
                            return nullptr;

                        auto x = (uint8_t)(((codes[i + next_chain] & 3) << 2) | (codes[i + next_chain + 1] >> 6));

                        if ((next_chain * 2) + x >= 11)
                            return nullptr;

                        uint64_t base = context->Sp + (next_chain * 16);

                        if (auto ret = read_uint64_t(base); ret.has_value())
                            context->X[19 + (next_chain * 2) + x] = *ret;
                        else
                            return nullptr;

                        if (auto ret = read_uint64_t(base + 8); ret.has_value())
                            context->X[20 + (next_chain * 2) + x] = *ret;
                        else
                            return nullptr;

                        // No Sp change; leave i so the main loop processes the base opcode next.
                        break;
                    }

                    case 0xd8 ... 0xd9: { // save_fregp
                        if (i + next_chain == codes.size() - 1)
                            return nullptr;

                        auto x = (uint8_t)(((codes[i + next_chain] & 1) << 2) | (codes[i + next_chain + 1] >> 6));
                        auto z = (uint8_t)((codes[i + next_chain + 1] & 0x3f) + (next_chain * 2));

                        if ((next_chain * 2) + x >= 22)
                            return nullptr;

                        if (auto ret = read_uint64_t(context->Sp + (z * 8)); ret.has_value())
                            *(uint64_t*)&context->V[8 + (next_chain * 2) + x].D[0] = *ret;
                        else
                            return nullptr;

                        if (auto ret = read_uint64_t(context->Sp + ((z + 1) * 8)); ret.has_value())
                            *(uint64_t*)&context->V[9 + (next_chain * 2) + x].D[0] = *ret;
                        else
                            return nullptr;

                        // No Sp change; leave i so the main loop processes the base opcode next.
                        break;
                    }

                    case 0xda ... 0xdb: { // save_fregp_x
                        if (i + next_chain == codes.size() - 1)
                            return nullptr;

                        auto x = (uint8_t)(((codes[i + next_chain] & 1) << 2) | (codes[i + next_chain + 1] >> 6));

                        if ((next_chain * 2) + x >= 22)
                            return nullptr;

                        uint64_t base = context->Sp + (next_chain * 16);

                        if (auto ret = read_uint64_t(base); ret.has_value())
                            *(uint64_t*)&context->V[8 + (next_chain * 2) + x].D[0] = *ret;
                        else
                            return nullptr;

                        if (auto ret = read_uint64_t(base + 8); ret.has_value())
                            *(uint64_t*)&context->V[9 + (next_chain * 2) + x].D[0] = *ret;
                        else
                            return nullptr;

                        // No Sp change; leave i so the main loop processes the base opcode next.
                        break;
                    }

                    // FIXME - save_any_xreg, save_any_dreg, save_any_qreg

                    default:
                        return nullptr;
                }

                break;
            }

            case 0xe7:
                if (i > codes.size() - 3)
                    return nullptr;

                // FIXME - save_any_xreg, save_any_dreg, save_any_qreg, save_zreg, save_preg

                i += 2;
                return nullptr;

            case 0xfc:
                // FIXME - pac_sign_lr
                return nullptr;

            default:
                return nullptr;
        }
    }

    context->Pc = context->Lr;

    *establisher_frame = context->Sp;

    if (type != 0 && xdata.ExceptionDataPresent) {
        auto handler = (uint32_t*)((uint8_t*)&xdata + sizeof(uint32_t) + off + (epilogue_count * sizeof(uint32_t)) + (code_words * sizeof(uint32_t)));

        *handler_data = &handler[1];

        return (EXCEPTION_ROUTINE)(image_base + *handler);
    }

    return nullptr;
}

#endif
