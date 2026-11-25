// SPDX-License-Identifier: MIT
module;

#include <span>
#include <string_view>
#include <source_location>
#include <chrono>
#include <optional>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

export module format;

import nt;

using namespace std;

export template<unsigned_integral T>
class byte_size {
public:
    byte_size(T val) : val(val) { }

    static constexpr bool is_byte_size = true;

    T val;
};

span<char> sp_strcpy(span<char> dest, string_view src) {
    auto to_copy = min(src.size(), dest.size());

    memcpy(dest.data(), src.data(), to_copy);

    return span(dest.data() + to_copy, dest.size() - to_copy);
}

span<char> sp_itoa(span<char> dest, uint64_t num, unsigned int padding = 0,
                   bool zero_pad = false, bool left_pad = false) {
    unsigned int trailing_spaces = 0;
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0) {
        if (padding == 0) {
            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        } else {
            for (unsigned int i = 0; i < padding - 1; i++) {
                if (left_pad)
                    trailing_spaces++;
                else {
                    dest[0] = zero_pad ? '0' : ' ';
                    dest = span(dest.data() + 1, dest.size() - 1);
                }

                if (dest.empty())
                    return dest;
            }

            dest[0] = '0';
            dest = span(dest.data() + 1, dest.size() - 1);
        }
    } else {
        uint64_t place = 10000000000000000000ull;

#ifndef __i386 // FIXME
        for (unsigned int i = 0; i < 20; i++) {
            if (!first || padding >= 20 - i || num >= place) {
                uint64_t digit = num / place;

                if (digit == 0 && first && !zero_pad) {
                    if (left_pad)
                        trailing_spaces++;
                    else {
                        dest[0] = ' ';
                        dest = span(dest.data() + 1, dest.size() - 1);
                    }
                } else {
                    dest[0] = (char)(digit + '0');

                    if (first && digit != 0)
                        first = false;

                    dest = span(dest.data() + 1, dest.size() - 1);
                }

                if (dest.empty())
                    return dest;

                num %= place;
            }

            place /= 10;
        }
#endif
    }

    for (unsigned int i = 0; i < trailing_spaces; i++) {
        if (dest.empty())
            break;

        dest[0] = ' ';
        dest = span(dest.data() + 1, dest.size() - 1);
    }

    return dest;
}

span<char> sp_itoa_hex(span<char> dest, uint64_t num, unsigned int padding = 0,
                       bool zero_pad = false, bool left_pad = false) {
    unsigned int trailing_spaces = 0;
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0) {
        if (padding == 0) {
            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        } else {
            for (unsigned int i = 0; i < padding - 1; i++) {
                if (left_pad)
                    trailing_spaces++;
                else {
                    dest[0] = zero_pad ? '0' : ' ';
                    dest = span(dest.data() + 1, dest.size() - 1);
                }

                if (dest.empty())
                    return dest;
            }

            dest[0] = '0';
            dest = span(dest.data() + 1, dest.size() - 1);
        }
    } else {
        for (unsigned int i = 0; i < 16; i++) {
            if (!first || padding >= 16 - i || num & 0xf000000000000000) {
                uint64_t n = num >> 60;

                if (n == 0 && first && !zero_pad) {
                    if (left_pad)
                        trailing_spaces++;
                    else {
                        dest[0] = ' ';
                        dest = span(dest.data() + 1, dest.size() - 1);
                    }
                } else {
                    if (n <= 9)
                        dest[0] = (char)(n + '0');
                    else
                        dest[0] = (char)(n - 0xa + 'a');

                    if (first && n != 0)
                        first = false;

                    dest = span(dest.data() + 1, dest.size() - 1);
                }

                if (dest.empty())
                    return dest;
            }

            num <<= 4;
        }
    }

    for (unsigned int i = 0; i < trailing_spaces; i++) {
        if (dest.empty())
            break;

        dest[0] = ' ';
        dest = span(dest.data() + 1, dest.size() - 1);
    }

    return dest;
}

span<char> sp_itoa_oct(span<char> dest, uint64_t num, unsigned int padding = 0,
                       bool zero_pad = false, bool left_pad = false) {
    unsigned int trailing_spaces = 0;
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0) {
        if (padding == 0) {
            dest[0] = '0';
            return span(dest.data() + 1, dest.size() - 1);
        } else {
            for (unsigned int i = 0; i < padding - 1; i++) {
                if (left_pad)
                    trailing_spaces++;
                else {
                    dest[0] = zero_pad ? '0' : ' ';
                    dest = span(dest.data() + 1, dest.size() - 1);
                }

                if (dest.empty())
                    return dest;
            }

            dest[0] = '0';
            dest = span(dest.data() + 1, dest.size() - 1);
        }
    } else {
        if (num & 0x8000000000000000) {
            dest[0] = '1';
            dest = span(dest.data() + 1, dest.size() - 1);
            first = false;

            if (dest.empty())
                return dest;
        }

        for (unsigned int i = 0; i < 21; i++) {
            if (!first || padding >= 21 - i || num & 0x7000000000000000) {
                uint64_t n = (num & 0x7000000000000000) >> 60;

                if (n == 0 && first && !zero_pad) {
                    if (left_pad)
                        trailing_spaces++;
                    else {
                        dest[0] = ' ';
                        dest = span(dest.data() + 1, dest.size() - 1);
                    }
                } else {
                    dest[0] = (char)(n + '0');

                    if (first && n != 0)
                        first = false;

                    dest = span(dest.data() + 1, dest.size() - 1);
                }

                if (dest.empty())
                    return dest;
            }

            num <<= 3;
        }
    }

    for (unsigned int i = 0; i < trailing_spaces; i++) {
        if (dest.empty())
            break;

        dest[0] = ' ';
        dest = span(dest.data() + 1, dest.size() - 1);
    }

    return dest;
}

export template<typename T>
consteval bool format_spec_valid(string_view) {
    return false;
}

export span<char> do_format_arg(span<char> sp, string_view fmt, integral auto arg, optional<uint64_t> arg2 = nullopt) {
    enum class int_format_base {
        dec,
        hex,
        oct
    };

    auto base = int_format_base::dec;

    if (!fmt.empty() && fmt.back() == 'x') {
        base = int_format_base::hex;
        fmt = string_view(fmt.data(), fmt.size() - 1);
    } else if (!fmt.empty() && fmt.back() == 'o') {
        base = int_format_base::oct;
        fmt = string_view(fmt.data(), fmt.size() - 1);
    }

    unsigned int padding;
    bool zero_pad = false, left_pad = false;

    if (arg2.has_value())
        padding = (unsigned int)*arg2;
    else {
        padding = 0;

        if (fmt.size() >= 2 && fmt.front() == ':') {
            fmt = string_view(fmt.data() + 1, fmt.size() - 1);

            if (fmt[0] == '<') {
                left_pad = true;
                fmt = string_view(fmt.data() + 1, fmt.size() - 1);
            }

            for (size_t i = 0; i < fmt.size(); i++) {
                if (i == 0 && fmt[i] == '0')
                    zero_pad = true;

                padding *= 10;
                padding += (unsigned int)(fmt[i] - '0');
            }
        }
    }

    if (base == int_format_base::hex) {
        if constexpr (is_signed_v<decltype(arg)>) {
            if (arg < 0) {
                sp = sp_strcpy(sp, "-");
                return sp_itoa_hex(sp, (uint64_t)-arg, padding > 0 ? (padding - 1) : 0,
                                   zero_pad, left_pad);
            }
        }

        return sp_itoa_hex(sp, (uint64_t)arg, padding, zero_pad, left_pad);
    } else if (base == int_format_base::oct) {
        if constexpr (is_signed_v<decltype(arg)>) {
            if (arg < 0) {
                sp = sp_strcpy(sp, "-");
                return sp_itoa_oct(sp, (uint64_t)-arg, padding > 0 ? (padding - 1) : 0,
                                   zero_pad, left_pad);
            }
        }

        return sp_itoa_oct(sp, (uint64_t)arg, padding, zero_pad, left_pad);
    } else {
        if constexpr (is_signed_v<decltype(arg)>) {
            if (arg < 0) {
                sp = sp_strcpy(sp, "-");
                return sp_itoa(sp, (uint64_t)-arg, padding > 0 ? (padding - 1) : 0,
                               zero_pad, left_pad);
            }
        }

        return sp_itoa(sp, (uint64_t)arg, padding, zero_pad, left_pad);
    }
}

template<typename T>
requires (integral<T>)
consteval bool format_spec_valid(string_view fmt) {
    if (fmt.empty())
        return true;

    if (fmt.size() >= 2 && fmt.front() == ':') {
        fmt = string_view(fmt.data() + 1, fmt.size() - 1);

        if (fmt.back() == 'x' || fmt.back() == 'o')
            fmt = string_view(fmt.data(), fmt.size() - 1);

        if (fmt.size() > 1 && fmt.front() == '<')
            fmt = string_view(fmt.data() + 1, fmt.size() - 1);

        if (fmt.size() == 2 && fmt[0] == '{' && fmt[1] == '}')
            return true;

        for (size_t i = 0; i < fmt.size(); i++) {
            if (fmt[i] < '0' || fmt[i] > '9')
                return false;
        }

        return true;
    }

    return false;
}

export template<typename T>
requires (unsigned_integral<T>)
span<char> do_format_arg(span<char> sp, string_view, byte_size<T> arg) {
    auto val = (uint64_t)arg.val;

    static const uint16_t decimals[] = {
        0x33, // 0.05 * 1024
        0x99, // 0.15 * 1024
        0x100, // 0.25 * 1024
        0x166, // 0.35 * 1024
        0x1cc, // 0.45 * 1024
        0x233, // 0.55 * 1024
        0x299, // 0.65 * 1024
        0x300, // 0.75 * 1024
        0x366, // 0.85 * 1024
        0x3cc // 0.95 * 1024
    };

    if (val < 1024) {
        sp = sp_itoa(sp, (uint64_t)arg.val);
        sp = sp_strcpy(sp, " bytes");
        return sp;
    }

    unsigned int shift;
    string_view suffix;

    if (val < 1 << 20) {
        shift = 10;
        suffix = " KB";
    } else if (val < 1 << 30) {
        shift = 20;
        suffix = " MB";
    } else if (val < (uint64_t)1 << 40) {
        shift = 30;
        suffix = " GB";
    } else if (val < (uint64_t)1 << 50) {
        shift = 40;
        suffix = " TB";
    } else if (val < (uint64_t)1 << 60) {
        shift = 50;
        suffix = " PB";
    } else {
        shift = 60;
        suffix = " EB";
    }

    auto decimal = (val >> (shift - 10)) & 0x3ff;

    sp = sp_itoa(sp, (val >> shift) + (decimal >= decimals[9] ? 1 : 0));
    sp = sp_strcpy(sp, ".");

    if (decimal >= decimals[9] || decimal < decimals[0])
        sp = sp_strcpy(sp, "0");
    else if (decimal >= decimals[8])
        sp = sp_strcpy(sp, "9");
    else if (decimal >= decimals[7])
        sp = sp_strcpy(sp, "8");
    else if (decimal >= decimals[6])
        sp = sp_strcpy(sp, "7");
    else if (decimal >= decimals[5])
        sp = sp_strcpy(sp, "6");
    else if (decimal >= decimals[4])
        sp = sp_strcpy(sp, "5");
    else if (decimal >= decimals[3])
        sp = sp_strcpy(sp, "4");
    else if (decimal >= decimals[2])
        sp = sp_strcpy(sp, "3");
    else if (decimal >= decimals[1])
        sp = sp_strcpy(sp, "2");
    else
        sp = sp_strcpy(sp, "1");

    sp = sp_strcpy(sp, suffix);

    return sp;
}

template<typename T>
requires (T::is_byte_size)
consteval bool format_spec_valid(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, nt_status arg) {
    switch (arg) {
        case nt_status::SUCCESS:
            return sp_strcpy(sp, "SUCCESS");
        case nt_status::USER_APC:
            return sp_strcpy(sp, "USER_APC");
        case nt_status::ALERTED:
            return sp_strcpy(sp, "ALERTED");
        case nt_status::TIMEOUT:
            return sp_strcpy(sp, "TIMEOUT");
        case nt_status::PENDING:
            return sp_strcpy(sp, "PENDING");
        case nt_status::REPARSE:
            return sp_strcpy(sp, "REPARSE");
        case nt_status::IMAGE_MACHINE_TYPE_MISMATCH_EXE:
            return sp_strcpy(sp, "IMAGE_MACHINE_TYPE_MISMATCH_EXE");
        case nt_status::BREAKPOINT:
            return sp_strcpy(sp, "BREAKPOINT");
        case nt_status::SINGLE_STEP:
            return sp_strcpy(sp, "SINGLE_STEP");
        case nt_status::BUFFER_OVERFLOW:
            return sp_strcpy(sp, "BUFFER_OVERFLOW");
        case nt_status::NO_MORE_FILES:
            return sp_strcpy(sp, "NO_MORE_FILES");
        case nt_status::NO_MORE_ENTRIES:
            return sp_strcpy(sp, "NO_MORE_ENTRIES");
        case nt_status::NOT_IMPLEMENTED:
            return sp_strcpy(sp, "NOT_IMPLEMENTED");
        case nt_status::INVALID_INFO_CLASS:
            return sp_strcpy(sp, "INVALID_INFO_CLASS");
        case nt_status::INFO_LENGTH_MISMATCH:
            return sp_strcpy(sp, "INFO_LENGTH_MISMATCH");
        case nt_status::ACCESS_VIOLATION:
            return sp_strcpy(sp, "ACCESS_VIOLATION");
        case nt_status::INVALID_HANDLE:
            return sp_strcpy(sp, "INVALID_HANDLE");
        case nt_status::INVALID_CID:
            return sp_strcpy(sp, "INVALID_CID");
        case nt_status::INVALID_PARAMETER:
            return sp_strcpy(sp, "INVALID_PARAMETER");
        case nt_status::INVALID_DEVICE_REQUEST:
            return sp_strcpy(sp, "INVALID_DEVICE_REQUEST");
        case nt_status::END_OF_FILE:
            return sp_strcpy(sp, "END_OF_FILE");
        case nt_status::NO_MEMORY:
            return sp_strcpy(sp, "NO_MEMORY");
        case nt_status::ILLEGAL_INSTRUCTION:
            return sp_strcpy(sp, "ILLEGAL_INSTRUCTION");
        case nt_status::ACCESS_DENIED:
            return sp_strcpy(sp, "ACCESS_DENIED");
        case nt_status::BUFFER_TOO_SMALL:
            return sp_strcpy(sp, "BUFFER_TOO_SMALL");
        case nt_status::OBJECT_TYPE_MISMATCH:
            return sp_strcpy(sp, "OBJECT_TYPE_MISMATCH");
        case nt_status::INVALID_DISPOSITION:
            return sp_strcpy(sp, "INVALID_DISPOSITION");
        case nt_status::UNWIND:
            return sp_strcpy(sp, "UNWIND");
        case nt_status::BAD_STACK:
            return sp_strcpy(sp, "BAD_STACK");
        case nt_status::DISK_CORRUPT_ERROR:
            return sp_strcpy(sp, "DISK_CORRUPT_ERROR");
        case nt_status::OBJECT_NAME_INVALID:
            return sp_strcpy(sp, "OBJECT_NAME_INVALID");
        case nt_status::OBJECT_NAME_NOT_FOUND:
            return sp_strcpy(sp, "OBJECT_NAME_NOT_FOUND");
        case nt_status::OBJECT_NAME_COLLISION:
            return sp_strcpy(sp, "OBJECT_NAME_COLLISION");
        case nt_status::OBJECT_PATH_NOT_FOUND:
            return sp_strcpy(sp, "OBJECT_PATH_NOT_FOUND");
        case nt_status::OBJECT_PATH_SYNTAX_BAD:
            return sp_strcpy(sp, "OBJECT_PATH_SYNTAX_BAD");
        case nt_status::CRC_ERROR:
            return sp_strcpy(sp, "CRC_ERROR");
        case nt_status::SHARING_VIOLATION:
            return sp_strcpy(sp, "SHARING_VIOLATION");
        case nt_status::INVALID_PAGE_PROTECTION:
            return sp_strcpy(sp, "INVALID_PAGE_PROTECTION");
        case nt_status::THREAD_IS_TERMINATING:
            return sp_strcpy(sp, "THREAD_IS_TERMINATING");
        case nt_status::PROCEDURE_NOT_FOUND:
            return sp_strcpy(sp, "PROCEDURE_NOT_FOUND");
        case nt_status::INVALID_IMAGE_FORMAT:
            return sp_strcpy(sp, "INVALID_IMAGE_FORMAT");
        case nt_status::DISK_FULL:
            return sp_strcpy(sp, "DISK_FULL");
        case nt_status::INTEGER_DIVIDE_BY_ZERO:
            return sp_strcpy(sp, "INTEGER_DIVIDE_BY_ZERO");
        case nt_status::INSUFFICIENT_RESOURCES:
            return sp_strcpy(sp, "INSUFFICIENT_RESOURCES");
        case nt_status::MEDIA_WRITE_PROTECTED:
            return sp_strcpy(sp, "MEDIA_WRITE_PROTECTED");
        case nt_status::DEVICE_NOT_READY:
            return sp_strcpy(sp, "DEVICE_NOT_READY");
        case nt_status::PIPE_DISCONNECTED:
            return sp_strcpy(sp, "PIPE_DISCONNECTED");
        case nt_status::IO_TIMEOUT:
            return sp_strcpy(sp, "IO_TIMEOUT");
        case nt_status::FILE_IS_A_DIRECTORY:
            return sp_strcpy(sp, "FILE_IS_A_DIRECTORY");
        case nt_status::NOT_SUPPORTED:
            return sp_strcpy(sp, "NOT_SUPPORTED");
        case nt_status::NOT_SAME_DEVICE:
            return sp_strcpy(sp, "NOT_SAME_DEVICE");
        case nt_status::CANT_WAIT:
            return sp_strcpy(sp, "CANT_WAIT");
        case nt_status::INTERNAL_ERROR:
            return sp_strcpy(sp, "INTERNAL_ERROR");
        case nt_status::DIRECTORY_NOT_EMPTY:
            return sp_strcpy(sp, "DIRECTORY_NOT_EMPTY");
        case nt_status::NOT_A_DIRECTORY:
            return sp_strcpy(sp, "NOT_A_DIRECTORY");
        case nt_status::PROCESS_IS_TERMINATING:
            return sp_strcpy(sp, "PROCESS_IS_TERMINATING");
        case nt_status::DLL_NOT_FOUND:
            return sp_strcpy(sp, "DLL_NOT_FOUND");
        case nt_status::ORDINAL_NOT_FOUND:
            return sp_strcpy(sp, "ORDINAL_NOT_FOUND");
        case nt_status::CONTROL_C_EXIT:
            return sp_strcpy(sp, "CONTROL_C_EXIT");
        case nt_status::DLL_INIT_FAILED:
            return sp_strcpy(sp, "DLL_INIT_FAILED");
        case nt_status::UNHANDLED_EXCEPTION:
            return sp_strcpy(sp, "UNHANDLED_EXCEPTION");
        case nt_status::PIPE_BROKEN:
            return sp_strcpy(sp, "PIPE_BROKEN");
        case nt_status::UNRECOGNIZED_VOLUME:
            return sp_strcpy(sp, "UNRECOGNIZED_VOLUME");
        case nt_status::IO_DEVICE_ERROR:
            return sp_strcpy(sp, "IO_DEVICE_ERROR");
        case nt_status::FS_DRIVER_REQUIRED:
            return sp_strcpy(sp, "FS_DRIVER_REQUIRED");
        case nt_status::INVALID_BUFFER_SIZE:
            return sp_strcpy(sp, "INVALID_BUFFER_SIZE");
        case nt_status::INVALID_ADDRESS_COMPONENT:
            return sp_strcpy(sp, "INVALID_ADDRESS_COMPONENT");
        case nt_status::CONNECTION_DISCONNECTED:
            return sp_strcpy(sp, "CONNECTION_DISCONNECTED");
        case nt_status::CONNECTION_RESET:
            return sp_strcpy(sp, "CONNECTION_RESET");
        case nt_status::NOT_FOUND:
            return sp_strcpy(sp, "NOT_FOUND");
        case nt_status::CONNECTION_REFUSED:
            return sp_strcpy(sp, "CONNECTION_REFUSED");
        case nt_status::ADDRESS_ALREADY_ASSOCIATED:
            return sp_strcpy(sp, "ADDRESS_ALREADY_ASSOCIATED");
        case nt_status::CONNECTION_ACTIVE:
            return sp_strcpy(sp, "CONNECTION_ACTIVE");
        case nt_status::NETWORK_UNREACHABLE:
            return sp_strcpy(sp, "NETWORK_UNREACHABLE");
        case nt_status::HOST_UNREACHABLE:
            return sp_strcpy(sp, "HOST_UNREACHABLE");
        case nt_status::PROTOCOL_UNREACHABLE:
            return sp_strcpy(sp, "PROTOCOL_UNREACHABLE");
        case nt_status::CONNECTION_ABORTED:
            return sp_strcpy(sp, "CONNECTION_ABORTED");
        case nt_status::ILLEGAL_DLL_RELOCATION:
            return sp_strcpy(sp, "ILLEGAL_DLL_RELOCATION");
        case nt_status::NOT_A_REPARSE_POINT:
            return sp_strcpy(sp, "NOT_A_REPARSE_POINT");
        case nt_status::REPARSE_POINT_NOT_RESOLVED:
            return sp_strcpy(sp, "REPARSE_POINT_NOT_RESOLVED");
        case nt_status::FLOAT_MULTIPLE_FAULTS:
            return sp_strcpy(sp, "FLOAT_MULTIPLE_FAULTS");
        case nt_status::FLOAT_MULTIPLE_TRAPS:
            return sp_strcpy(sp, "FLOAT_MULTIPLE_TRAPS");
        case nt_status::DEVICE_REMOVED:
            return sp_strcpy(sp, "DEVICE_REMOVED");
        case nt_status::SYSTEM_SHUTDOWN:
            return sp_strcpy(sp, "SYSTEM_SHUTDOWN");
        case nt_status::STACK_BUFFER_OVERRUN:
            return sp_strcpy(sp, "STACK_BUFFER_OVERRUN");
        case nt_status::PROTOCOL_NOT_SUPPORTED:
            return sp_strcpy(sp, "PROTOCOL_NOT_SUPPORTED");
        default:
            return do_format_arg(sp, ":08x", (uint32_t)arg);
    }
}

template<>
consteval bool format_spec_valid<nt_status>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, FILE_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum FILE_INFORMATION_CLASS;

        case DirectoryInformation:
            return sp_strcpy(sp, "DirectoryInformation");
        case FullDirectoryInformation:
            return sp_strcpy(sp, "FullDirectoryInformation");
        case BothDirectoryInformation:
            return sp_strcpy(sp, "BothDirectoryInformation");
        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        case StandardInformation:
            return sp_strcpy(sp, "StandardInformation");
        case InternalInformation:
            return sp_strcpy(sp, "InternalInformation");
        case EaInformation:
            return sp_strcpy(sp, "EaInformation");
        case AccessInformation:
            return sp_strcpy(sp, "AccessInformation");
        case NameInformation:
            return sp_strcpy(sp, "NameInformation");
        case RenameInformation:
            return sp_strcpy(sp, "RenameInformation");
        case LinkInformation:
            return sp_strcpy(sp, "LinkInformation");
        case NamesInformation:
            return sp_strcpy(sp, "NamesInformation");
        case DispositionInformation:
            return sp_strcpy(sp, "DispositionInformation");
        case PositionInformation:
            return sp_strcpy(sp, "PositionInformation");
        case FullEaInformation:
            return sp_strcpy(sp, "FullEaInformation");
        case ModeInformation:
            return sp_strcpy(sp, "ModeInformation");
        case AlignmentInformation:
            return sp_strcpy(sp, "AlignmentInformation");
        case AllInformation:
            return sp_strcpy(sp, "AllInformation");
        case AllocationInformation:
            return sp_strcpy(sp, "AllocationInformation");
        case EndOfFileInformation:
            return sp_strcpy(sp, "EndOfFileInformation");
        case AlternateNameInformation:
            return sp_strcpy(sp, "AlternateNameInformation");
        case StreamInformation:
            return sp_strcpy(sp, "StreamInformation");
        case PipeInformation:
            return sp_strcpy(sp, "PipeInformation");
        case PipeLocalInformation:
            return sp_strcpy(sp, "PipeLocalInformation");
        case PipeRemoteInformation:
            return sp_strcpy(sp, "PipeRemoteInformation");
        case MailslotQueryInformation:
            return sp_strcpy(sp, "MailslotQueryInformation");
        case MailslotSetInformation:
            return sp_strcpy(sp, "MailslotSetInformation");
        case CompressionInformation:
            return sp_strcpy(sp, "CompressionInformation");
        case ObjectIdInformation:
            return sp_strcpy(sp, "ObjectIdInformation");
        case CompletionInformation:
            return sp_strcpy(sp, "CompletionInformation");
        case MoveClusterInformation:
            return sp_strcpy(sp, "MoveClusterInformation");
        case QuotaInformation:
            return sp_strcpy(sp, "QuotaInformation");
        case ReparsePointInformation:
            return sp_strcpy(sp, "ReparsePointInformation");
        case NetworkOpenInformation:
            return sp_strcpy(sp, "NetworkOpenInformation");
        case AttributeTagInformation:
            return sp_strcpy(sp, "AttributeTagInformation");
        case TrackingInformation:
            return sp_strcpy(sp, "TrackingInformation");
        case IdBothDirectoryInformation:
            return sp_strcpy(sp, "IdBothDirectoryInformation");
        case IdFullDirectoryInformation:
            return sp_strcpy(sp, "IdFullDirectoryInformation");
        case ValidDataLengthInformation:
            return sp_strcpy(sp, "ValidDataLengthInformation");
        case ShortNameInformation:
            return sp_strcpy(sp, "ShortNameInformation");
        case IoCompletionNotificationInformation:
            return sp_strcpy(sp, "IoCompletionNotificationInformation");
        case IoStatusBlockRangeInformation:
            return sp_strcpy(sp, "IoStatusBlockRangeInformation");
        case IoPriorityHintInformation:
            return sp_strcpy(sp, "IoPriorityHintInformation");
        case SfioReserveInformation:
            return sp_strcpy(sp, "SfioReserveInformation");
        case SfioVolumeInformation:
            return sp_strcpy(sp, "SfioVolumeInformation");
        case HardLinkInformation:
            return sp_strcpy(sp, "HardLinkInformation");
        case ProcessIdsUsingFileInformation:
            return sp_strcpy(sp, "ProcessIdsUsingFileInformation");
        case NormalizedNameInformation:
            return sp_strcpy(sp, "NormalizedNameInformation");
        case NetworkPhysicalNameInformation:
            return sp_strcpy(sp, "NetworkPhysicalNameInformation");
        case IdGlobalTxDirectoryInformation:
            return sp_strcpy(sp, "IdGlobalTxDirectoryInformation");
        case IsRemoteDeviceInformation:
            return sp_strcpy(sp, "IsRemoteDeviceInformation");
        case UnusedInformation:
            return sp_strcpy(sp, "UnusedInformation");
        case NumaNodeInformation:
            return sp_strcpy(sp, "NumaNodeInformation");
        case StandardLinkInformation:
            return sp_strcpy(sp, "StandardLinkInformation");
        case RemoteProtocolInformation:
            return sp_strcpy(sp, "RemoteProtocolInformation");
        case RenameInformationBypassAccessCheck:
            return sp_strcpy(sp, "RenameInformationBypassAccessCheck");
        case LinkInformationBypassAccessCheck:
            return sp_strcpy(sp, "LinkInformationBypassAccessCheck");
        case VolumeNameInformation:
            return sp_strcpy(sp, "VolumeNameInformation");
        case IdInformation:
            return sp_strcpy(sp, "IdInformation");
        case IdExtdDirectoryInformation:
            return sp_strcpy(sp, "IdExtdDirectoryInformation");
        case ReplaceCompletionInformation:
            return sp_strcpy(sp, "ReplaceCompletionInformation");
        case HardLinkFullIdInformation:
            return sp_strcpy(sp, "HardLinkFullIdInformation");
        case IdExtdBothDirectoryInformation:
            return sp_strcpy(sp, "IdExtdBothDirectoryInformation");
        case DispositionInformationEx:
            return sp_strcpy(sp, "DispositionInformationEx");
        case RenameInformationEx:
            return sp_strcpy(sp, "RenameInformationEx");
        case RenameInformationExBypassAccessCheck:
            return sp_strcpy(sp, "RenameInformationExBypassAccessCheck");
        case DesiredStorageClassInformation:
            return sp_strcpy(sp, "DesiredStorageClassInformation");
        case StatInformation:
            return sp_strcpy(sp, "StatInformation");
        case MemoryPartitionInformation:
            return sp_strcpy(sp, "MemoryPartitionInformation");
        case StatLxInformation:
            return sp_strcpy(sp, "StatLxInformation");
        case CaseSensitiveInformation:
            return sp_strcpy(sp, "CaseSensitiveInformation");
        case LinkInformationEx:
            return sp_strcpy(sp, "LinkInformationEx");
        case LinkInformationExBypassAccessCheck:
            return sp_strcpy(sp, "LinkInformationExBypassAccessCheck");
        case StorageReserveIdInformation:
            return sp_strcpy(sp, "StorageReserveIdInformation");
        case CaseSensitiveInformationForceAccessCheck:
            return sp_strcpy(sp, "CaseSensitiveInformationForceAccessCheck");
        case MoxnoxPosixNonBlock:
            return sp_strcpy(sp, "MoxnoxPosixNonBlock");
        case MoxnoxFchown:
            return sp_strcpy(sp, "MoxnoxFchown");
        case MoxnoxFchmod:
            return sp_strcpy(sp, "MoxnoxFchmod");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<FILE_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view fmt, string_view arg,
                                optional<uint64_t> arg2 = nullopt) {
    size_t padding = 0;
    bool right_pad = false;

    if (fmt.size() >= 2 && fmt.front() == ':') {
        fmt = string_view(fmt.data() + 1, fmt.size() - 1);

        if (fmt.size() > 1 && fmt.front() == '>') {
            right_pad = true;
            fmt = string_view(fmt.data() + 1, fmt.size() - 1);
        }

        if (arg2.has_value())
            padding = (size_t)*arg2;
        else {
            for (size_t i = 0; i < fmt.size(); i++) {
                padding *= 10;
                padding += (size_t)(fmt[i] - '0');
            }
        }
    } else if (arg2.has_value())
        padding = (size_t)*arg2;

    if (padding > arg.size())
        padding -= arg.size();
    else
        padding = 0;

    if (right_pad) {
        while (padding > 0) {
            sp = sp_strcpy(sp, " ");
            padding--;
        }
    }

    sp = sp_strcpy(sp, arg);

    if (!right_pad) {
        while (padding > 0) {
            sp = sp_strcpy(sp, " ");
            padding--;
        }
    }

    return sp;
}

template<>
consteval bool format_spec_valid<string_view>(string_view fmt) {
    if (fmt.size() >= 2 && fmt.front() == ':') {
        fmt = string_view(fmt.data() + 1, fmt.size() - 1);

        if (fmt.size() > 1 && fmt.front() == '>')
            fmt = string_view(fmt.data() + 1, fmt.size() - 1);

        if (fmt.size() == 2 && fmt[0] == '{' && fmt[1] == '}')
            return true;

        for (size_t i = 0; i < fmt.size(); i++) {
            if (fmt[i] < '0' || fmt[i] > '9')
                return false;
        }

        return true;
    }

    return fmt.empty();
}

template<>
consteval bool format_spec_valid<char*>(string_view fmt) {
    return format_spec_valid<string_view>(fmt);
}

template<>
consteval bool format_spec_valid<const char*>(string_view fmt) {
    return format_spec_valid<string_view>(fmt);
}

span<char> do_format_arg(span<char> sp, string_view, const AMD64_CONTEXT& ctx) {
    sp = sp_strcpy(sp, "rax=");
    sp = sp_itoa_hex(sp, ctx.Rax, 16, true);
    sp = sp_strcpy(sp, " rcx=");
    sp = sp_itoa_hex(sp, ctx.Rcx, 16, true);
    sp = sp_strcpy(sp, " rdx=");
    sp = sp_itoa_hex(sp, ctx.Rdx, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "rbx=");
    sp = sp_itoa_hex(sp, ctx.Rbx, 16, true);
    sp = sp_strcpy(sp, " rsp=");
    sp = sp_itoa_hex(sp, ctx.Rsp, 16, true);
    sp = sp_strcpy(sp, " rbp=");
    sp = sp_itoa_hex(sp, ctx.Rbp, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "rsi=");
    sp = sp_itoa_hex(sp, ctx.Rsi, 16, true);
    sp = sp_strcpy(sp, " rdi=");
    sp = sp_itoa_hex(sp, ctx.Rdi, 16, true);
    sp = sp_strcpy(sp, "  r8=");
    sp = sp_itoa_hex(sp, ctx.R8, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " r9=");
    sp = sp_itoa_hex(sp, ctx.R9, 16, true);
    sp = sp_strcpy(sp, " r10=");
    sp = sp_itoa_hex(sp, ctx.R10, 16, true);
    sp = sp_strcpy(sp, " r11=");
    sp = sp_itoa_hex(sp, ctx.R11, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "r12=");
    sp = sp_itoa_hex(sp, ctx.R12, 16, true);
    sp = sp_strcpy(sp, " r13=");
    sp = sp_itoa_hex(sp, ctx.R13, 16, true);
    sp = sp_strcpy(sp, " r14=");
    sp = sp_itoa_hex(sp, ctx.R14, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "r15=");
    sp = sp_itoa_hex(sp, ctx.R15, 16, true);
    sp = sp_strcpy(sp, " rip=");
    sp = sp_itoa_hex(sp, ctx.Rip, 16, true);
    sp = sp_strcpy(sp, " eflags=");
    sp = sp_itoa_hex(sp, ctx.EFlags, 8, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "cs=");
    sp = sp_itoa_hex(sp, ctx.SegCs, 4, true);
    sp = sp_strcpy(sp, " ds=");
    sp = sp_itoa_hex(sp, ctx.SegDs, 4, true);
    sp = sp_strcpy(sp, " es=");
    sp = sp_itoa_hex(sp, ctx.SegEs, 4, true);
    sp = sp_strcpy(sp, " fs=");
    sp = sp_itoa_hex(sp, ctx.SegFs, 4, true);
    sp = sp_strcpy(sp, " gs=");
    sp = sp_itoa_hex(sp, ctx.SegGs, 4, true);
    sp = sp_strcpy(sp, " ss=");
    sp = sp_itoa_hex(sp, ctx.SegSs, 4, true);
    sp = sp_strcpy(sp, "\n");

    return sp;
}

template<>
consteval bool format_spec_valid<AMD64_CONTEXT>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const ARM64_NT_CONTEXT& ctx) {
    sp = sp_strcpy(sp, " x0=");
    sp = sp_itoa_hex(sp, ctx.X0, 16, true);
    sp = sp_strcpy(sp, "  x1=");
    sp = sp_itoa_hex(sp, ctx.X1, 16, true);
    sp = sp_strcpy(sp, "  x2=");
    sp = sp_itoa_hex(sp, ctx.X2, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " x3=");
    sp = sp_itoa_hex(sp, ctx.X3, 16, true);
    sp = sp_strcpy(sp, "  x4=");
    sp = sp_itoa_hex(sp, ctx.X4, 16, true);
    sp = sp_strcpy(sp, "  x5=");
    sp = sp_itoa_hex(sp, ctx.X5, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " x6=");
    sp = sp_itoa_hex(sp, ctx.X6, 16, true);
    sp = sp_strcpy(sp, "  x7=");
    sp = sp_itoa_hex(sp, ctx.X7, 16, true);
    sp = sp_strcpy(sp, "  x8=");
    sp = sp_itoa_hex(sp, ctx.X8, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " x9=");
    sp = sp_itoa_hex(sp, ctx.X9, 16, true);
    sp = sp_strcpy(sp, " x10=");
    sp = sp_itoa_hex(sp, ctx.X10, 16, true);
    sp = sp_strcpy(sp, " x11=");
    sp = sp_itoa_hex(sp, ctx.X11, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x12=");
    sp = sp_itoa_hex(sp, ctx.X12, 16, true);
    sp = sp_strcpy(sp, " x13=");
    sp = sp_itoa_hex(sp, ctx.X13, 16, true);
    sp = sp_strcpy(sp, " x14=");
    sp = sp_itoa_hex(sp, ctx.X14, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x15=");
    sp = sp_itoa_hex(sp, ctx.X15, 16, true);
    sp = sp_strcpy(sp, " x16=");
    sp = sp_itoa_hex(sp, ctx.X16, 16, true);
    sp = sp_strcpy(sp, " x17=");
    sp = sp_itoa_hex(sp, ctx.X17, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x18=");
    sp = sp_itoa_hex(sp, ctx.X18, 16, true);
    sp = sp_strcpy(sp, " x19=");
    sp = sp_itoa_hex(sp, ctx.X19, 16, true);
    sp = sp_strcpy(sp, " x20=");
    sp = sp_itoa_hex(sp, ctx.X20, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x21=");
    sp = sp_itoa_hex(sp, ctx.X21, 16, true);
    sp = sp_strcpy(sp, " x22=");
    sp = sp_itoa_hex(sp, ctx.X22, 16, true);
    sp = sp_strcpy(sp, " x23=");
    sp = sp_itoa_hex(sp, ctx.X23, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x24=");
    sp = sp_itoa_hex(sp, ctx.X24, 16, true);
    sp = sp_strcpy(sp, " x25=");
    sp = sp_itoa_hex(sp, ctx.X25, 16, true);
    sp = sp_strcpy(sp, " x26=");
    sp = sp_itoa_hex(sp, ctx.X26, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "x27=");
    sp = sp_itoa_hex(sp, ctx.X27, 16, true);
    sp = sp_strcpy(sp, " x28=");
    sp = sp_itoa_hex(sp, ctx.X28, 16, true);
    sp = sp_strcpy(sp, "  fp=");
    sp = sp_itoa_hex(sp, ctx.Fp, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, " lr=");
    sp = sp_itoa_hex(sp, ctx.Lr, 16, true);
    sp = sp_strcpy(sp, "  sp=");
    sp = sp_itoa_hex(sp, ctx.Sp, 16, true);
    sp = sp_strcpy(sp, "  pc=");
    sp = sp_itoa_hex(sp, ctx.Pc, 16, true);
    sp = sp_strcpy(sp, "\n");

    sp = sp_strcpy(sp, "cpsr=");
    sp = sp_itoa_hex(sp, ctx.Cpsr, 8, true);
    sp = sp_strcpy(sp, " fpcr=");
    sp = sp_itoa_hex(sp, ctx.Fpcr, 8, true);
    sp = sp_strcpy(sp, " fpsr=");
    sp = sp_itoa_hex(sp, ctx.Fpsr, 8, true);
    sp = sp_strcpy(sp, "\n");

    return sp;
}

template<>
consteval bool format_spec_valid<ARM64_NT_CONTEXT>(string_view fmt) {
    return fmt.empty();
}

span<char> sp_u16string_view(span<char> buf, u16string_view u16sv) {
    if (buf.empty())
        return buf;

    for (unsigned int i = 0; i < u16sv.size(); i++) {
        uint32_t cp;

        if (u16sv[i] < 0xd800 || u16sv[i] > 0xdfff)
            cp = u16sv[i];
        else if (u16sv[i] >= 0xd800 && u16sv[i] < 0xdc00) {
            if (i == u16sv.size() - 1)
                return buf;

            auto next = u16sv[i+1];

            if (next < 0xdc00 || next > 0xdfff)
                return buf;

            cp = (((uint32_t)u16sv[i] & 0x3ff) << 10) | (next & 0x3ff);

            i++;
        } else // invalid
            return buf;

        if (cp < 0x80) {
            buf[0] = (char)cp;
            buf = buf.subspan(1);
        } else if (cp < 0x800) {
            if (buf.size() < 2)
                return buf;

            buf[0] = (char)(0xc0 | ((cp & 0x7c0) >> 6));
            buf[1] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(2);
        } else if (cp < 0x10000) {
            if (buf.size() < 3)
                return buf;

            buf[0] = (char)(0xe0 | ((cp & 0xf000) >> 12));
            buf[1] = (char)(0x80 | ((cp & 0xfc0) >> 6));
            buf[2] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(3);
        } else {
            if (buf.size() < 4)
                return buf;

            buf[0] = (char)(0xf0 | ((cp & 0xe0000) >> 18));
            buf[1] = (char)(0x80 | ((cp & 0x3f000) >> 12));
            buf[2] = (char)(0x80 | ((cp & 0xfc0) >> 6));
            buf[3] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(4);
        }
    }

    return buf;
}

span<char> do_format_arg(span<char> sp, string_view, u16string_view sv) {
    return sp_u16string_view(sp, sv);
}

template<>
consteval bool format_spec_valid<u16string_view>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view sv, const UNICODE_STRING& us) {
    return do_format_arg(sp, sv, u16string_view(us.Buffer, us.Length / sizeof(char16_t)));
}

template<>
consteval bool format_spec_valid<UNICODE_STRING>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const chrono::year_month_day& ymd) {
    sp = sp_itoa(sp, (uint64_t)(int)ymd.year());
    sp = sp_strcpy(sp, "-");

    if ((unsigned int)ymd.month() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (unsigned int)ymd.month());
    sp = sp_strcpy(sp, "-");

    if ((unsigned int)ymd.day() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (unsigned int)ymd.day());

    return sp;
}

template<>
consteval bool format_spec_valid<chrono::year_month_day>(string_view fmt) {
    return fmt.empty();
}

span<char> do_format_arg(span<char> sp, string_view, const chrono::hh_mm_ss<chrono::seconds>& hms) {
    if (hms.hours().count() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (uint64_t)hms.hours().count());
    sp = sp_strcpy(sp, ":");

    if (hms.minutes().count() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (uint64_t)hms.minutes().count());
    sp = sp_strcpy(sp, ":");

    if (hms.seconds().count() < 10)
        sp = sp_strcpy(sp, "0");

    sp = sp_itoa(sp, (uint64_t)hms.seconds().count());

    return sp;
}

template<>
consteval bool format_spec_valid<chrono::hh_mm_ss<chrono::seconds>>(string_view fmt) {
    return fmt.empty();
}

void checker_failure();

template<typename T>
concept supports_arg2 = requires(T t) {
    { do_format_arg(span<char>(), string_view{}, t, optional<uint64_t>(nullopt)) };
};

static_assert(supports_arg2<unsigned int>);
static_assert(supports_arg2<string_view>);

template<size_t N, typename... Args>
auto tuple_drop_n(tuple<Args...>&& t) {
    if constexpr (N >= sizeof...(Args))
        return tuple<>();
    else {
        return []<size_t... Is>(auto&& tuple, index_sequence<Is...>) {
            return forward_as_tuple(get<Is + N>(std::forward<decltype(tuple)>(tuple))...);
        }(std::forward<tuple<Args...>>(t), make_index_sequence<sizeof...(Args) - N>{});
    }
}

template<typename T, typename... Args>
consteval void check_specs(span<string_view, sizeof...(Args) + 1> sp,
                           tuple<T, Args...>*) {
    using type = decay_t<T>;

    if (!format_spec_valid<type>(sp[0]))
        checker_failure();

    if constexpr (sizeof...(Args) > 1 && supports_arg2<type>) {
        if (sp[0].size() >= 2 && sp[0][sp[0].size() - 2] == '{' && sp[0][sp[0].size() - 1] == '}') {
            using left_type = decltype(tuple_drop_n<2>(declval<tuple<T, Args...>>()));

            check_specs(span<string_view, sizeof...(Args) - 1>(sp.data() + 2, sp.size() - 2), (left_type*)nullptr);
            return;
        }
    }

    if constexpr (sizeof...(Args) > 0)
        check_specs(span<string_view, sizeof...(Args)>(sp.data() + 1, sp.size() - 1), (tuple<Args...>*)nullptr);
}

export
template<typename... Args>
struct checker {
    consteval checker(const char* q) : sv(q) {
        unsigned int num_entries = 0;

        // FIXME - {{ and }} escaping

        auto s = sv;

        while (!s.empty()) {
            if (s.front() == '{') {
                num_entries++;

                unsigned int depth = 1;
                size_t end = string_view::npos;

                for (unsigned int i = 1; i < s.size(); i++) {
                    if (s[i] == '{')
                        depth++;
                    else if (s[i] == '}') {
                        depth--;
                        if (depth == 0) {
                            end = i;
                            break;
                        }
                    }
                }

                if (end == string_view::npos)
                    checker_failure();

                s = s.substr(end + 1);
                continue;
            }

            s = s.substr(1);
        }

        if (num_entries > sizeof...(Args))
            checker_failure();

        array<string_view, sizeof...(Args)> spec;

        s = sv;

        unsigned int pos = 0;

        while (!s.empty()) {
            if (s.front() == '{') {
                unsigned int depth = 1;
                size_t end = string_view::npos;

                for (unsigned int i = 1; i < s.size(); i++) {
                    if (s[i] == '{')
                        depth++;
                    else if (s[i] == '}') {
                        depth--;
                        if (depth == 0) {
                            end = i;
                            break;
                        }
                    }
                }

                spec[pos] = s.substr(1, end - 1);

                if (spec[pos].size() >= 2 && spec[pos][spec[pos].size() - 1] == '}' && spec[pos][spec[pos].size() - 2] == '{')
                    pos++;

                s = s.substr(end + 1);
                pos++;
                continue;
            }

            s = s.substr(1);
        }

        if constexpr (sizeof...(Args) > 0)
            check_specs(spec, (tuple<Args...>*)nullptr);
    }

    string_view sv;
};

export
template<typename... Args>
span<char> format_to(span<char> sp, string_view fmt, tuple<Args...>&& args) {
    static constexpr size_t num_args = sizeof...(Args);

    if constexpr (num_args == 0)
        return sp_strcpy(sp, fmt);
    else {
        // FIXME - {{ and }} escaping

        auto brack = fmt.find('{');

        if (brack == string_view::npos)
            return sp_strcpy(sp, fmt);

        if (brack != 0) {
            sp = sp_strcpy(sp, string_view(fmt.data(), brack));
            fmt = string_view(fmt.data() + brack, fmt.size() - brack);
        }

        unsigned int depth = 1;
        size_t close = string_view::npos;
        bool nested = false;

        for (unsigned int i = 1; i < fmt.size(); i++) {
            if (fmt[i] == '{') {
                depth++;
                nested = true;
            } else if (fmt[i] == '}') {
                depth--;
                if (depth == 0) {
                    close = i;
                    break;
                }
            }
        }

        if (close == string_view::npos)
            return sp;

        if constexpr (num_args >= 2) {
            using arg1_type = remove_cvref_t<tuple_element_t<0, remove_cvref_t<decltype(args)>>>;
            using arg2_type = remove_cvref_t<tuple_element_t<1, remove_cvref_t<decltype(args)>>>;

            if constexpr (supports_arg2<arg1_type> && is_integral_v<arg2_type>) {
                if (nested)
                    sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), get<0>(args), get<1>(args));
                else
                    sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), get<0>(args));
            } else {
                sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), get<0>(args));
                nested = false;
            }
        } else {
            sp = do_format_arg(sp, string_view(fmt.data() + 1, close - 1), get<0>(args));
            nested = false;
        }

        fmt = string_view(fmt.data() + close + 1, fmt.size() - close - 1);

        if (fmt.empty())
            return sp;

        if constexpr (num_args >= 2) {
            if (nested)
                return format_to(sp, fmt, tuple_drop_n<2>(std::forward<tuple<Args...>>(args)));
            else
                return format_to(sp, fmt, tuple_drop_n<1>(std::forward<tuple<Args...>>(args)));
        } else
            return sp_strcpy(sp, fmt);
    }
}

export
template<typename... Args>
span<char> format(span<char> sp, type_identity_t<checker<type_identity_t<Args>...>> fmt, Args&&... args) {
    return format_to(sp, fmt.sv, forward_as_tuple(std::forward<Args>(args)...));
}

export span<char> do_format_arg(span<char> sp, string_view, const OBJECT_ATTRIBUTES& oa) {
    sp = sp_strcpy(sp, "(");
    sp = do_format_arg(sp, "", oa.RootDirectory);
    sp = sp_strcpy(sp, ", ");

    if (oa.ObjectName)
        sp = do_format_arg(sp, "", *oa.ObjectName);
    else
        sp = sp_strcpy(sp, "NULL");

    sp = sp_strcpy(sp, ")");

    return sp;
}

template<>
consteval bool format_spec_valid<OBJECT_ATTRIBUTES>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view, const OBJECT_ATTRIBUTES* oa) {
    if (!oa)
        return sp_strcpy(sp, "NULL");
    else
        return do_format_arg(sp, "", *oa);
}

template<>
consteval bool format_spec_valid<OBJECT_ATTRIBUTES*>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                THREAD_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum THREAD_INFORMATION_CLASS;

        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        case Times:
            return sp_strcpy(sp, "Times");
        case Priority:
            return sp_strcpy(sp, "Priority");
        case BasePriority:
            return sp_strcpy(sp, "BasePriority");
        case AffinityMask:
            return sp_strcpy(sp, "AffinityMask");
        case ImpersonationToken:
            return sp_strcpy(sp, "ImpersonationToken");
        case DescriptorTableEntry:
            return sp_strcpy(sp, "DescriptorTableEntry");
        case EnableAlignmentFaultFixup:
            return sp_strcpy(sp, "EnableAlignmentFaultFixup");
        case EventPair_Reusable:
            return sp_strcpy(sp, "EventPair_Reusable");
        case QuerySetWin32StartAddress:
            return sp_strcpy(sp, "QuerySetWin32StartAddress");
        case ZeroTlsCell:
            return sp_strcpy(sp, "ZeroTlsCell");
        case PerformanceCount:
            return sp_strcpy(sp, "PerformanceCount");
        case AmILastThread:
            return sp_strcpy(sp, "AmILastThread");
        case IdealProcessor:
            return sp_strcpy(sp, "IdealProcessor");
        case PriorityBoost:
            return sp_strcpy(sp, "PriorityBoost");
        case SetTlsArrayAddress:
            return sp_strcpy(sp, "SetTlsArrayAddress");
        case IsIoPending:
            return sp_strcpy(sp, "IsIoPending");
        case HideFromDebugger:
            return sp_strcpy(sp, "HideFromDebugger");
        case BreakOnTermination:
            return sp_strcpy(sp, "BreakOnTermination");
        case SwitchLegacyState:
            return sp_strcpy(sp, "SwitchLegacyState");
        case IsTerminated:
            return sp_strcpy(sp, "IsTerminated");
        case LastSystemCall:
            return sp_strcpy(sp, "LastSystemCall");
        case IoPriority:
            return sp_strcpy(sp, "IoPriority");
        case CycleTime:
            return sp_strcpy(sp, "CycleTime");
        case PagePriority:
            return sp_strcpy(sp, "PagePriority");
        case ActualBasePriority:
            return sp_strcpy(sp, "ActualBasePriority");
        case TebInformation:
            return sp_strcpy(sp, "TebInformation");
        case CSwitchMon:
            return sp_strcpy(sp, "CSwitchMon");
        case CSwitchPmu:
            return sp_strcpy(sp, "CSwitchPmu");
        case Wow64Context:
            return sp_strcpy(sp, "Wow64Context");
        case GroupInformation:
            return sp_strcpy(sp, "GroupInformation");
        case UmsInformation:
            return sp_strcpy(sp, "UmsInformation");
        case CounterProfiling:
            return sp_strcpy(sp, "CounterProfiling");
        case IdealProcessorEx:
            return sp_strcpy(sp, "IdealProcessorEx");
        case CpuAccountingInformation:
            return sp_strcpy(sp, "CpuAccountingInformation");
        case SuspendCount:
            return sp_strcpy(sp, "SuspendCount");
        case HeterogeneousCpuPolicy:
            return sp_strcpy(sp, "HeterogeneousCpuPolicy");
        case ContainerId:
            return sp_strcpy(sp, "ContainerId");
        case NameInformation:
            return sp_strcpy(sp, "NameInformation");
        case SelectedCpuSets:
            return sp_strcpy(sp, "SelectedCpuSets");
        case SystemThreadInformation:
            return sp_strcpy(sp, "SystemThreadInformation");
        case ActualGroupAffinity:
            return sp_strcpy(sp, "ActualGroupAffinity");
        case DynamicCodePolicyInfo:
            return sp_strcpy(sp, "DynamicCodePolicyInfo");
        case ExplicitCaseSensitivity:
            return sp_strcpy(sp, "ExplicitCaseSensitivity");
        case WorkOnBehalfTicket:
            return sp_strcpy(sp, "WorkOnBehalfTicket");
        case SubsystemInformation:
            return sp_strcpy(sp, "SubsystemInformation");
        case DbgkWerReportActive:
            return sp_strcpy(sp, "DbgkWerReportActive");
        case AttachContainer:
            return sp_strcpy(sp, "AttachContainer");
        case ManageWritesToExecutableMemory:
            return sp_strcpy(sp, "ManageWritesToExecutableMemory");
        case PowerThrottlingState:
            return sp_strcpy(sp, "PowerThrottlingState");
        case WorkloadClass:
            return sp_strcpy(sp, "WorkloadClass");
        case MoxnoxSetPendingSignal:
            return sp_strcpy(sp, "MoxnoxSetPendingSignal");
        case MoxnoxBlockSignals:
            return sp_strcpy(sp, "MoxnoxBlockSignals");
        case MoxnoxUnblockSignals:
            return sp_strcpy(sp, "MoxnoxUnblockSignals");
        case MoxnoxSignalsMask:
            return sp_strcpy(sp, "MoxnoxSignalsMask");
        case MoxnoxWaitForSignal:
            return sp_strcpy(sp, "MoxnoxWaitForSignal");
        case MoxnoxSignalsPending:
            return sp_strcpy(sp, "MoxnoxSignalsPending");
        case MoxnoxPoll:
            return sp_strcpy(sp, "MoxnoxPoll");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<THREAD_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                PROCESS_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum PROCESS_INFORMATION_CLASS;

        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        case QuotaLimits:
            return sp_strcpy(sp, "QuotaLimits");
        case IoCounters:
            return sp_strcpy(sp, "IoCounters");
        case VmCounters:
            return sp_strcpy(sp, "VmCounters");
        case Times:
            return sp_strcpy(sp, "Times");
        case BasePriority:
            return sp_strcpy(sp, "BasePriority");
        case RaisePriority:
            return sp_strcpy(sp, "RaisePriority");
        case DebugPort:
            return sp_strcpy(sp, "DebugPort");
        case ExceptionPort:
            return sp_strcpy(sp, "ExceptionPort");
        case AccessToken:
            return sp_strcpy(sp, "AccessToken");
        case LdtInformation:
            return sp_strcpy(sp, "LdtInformation");
        case LdtSize:
            return sp_strcpy(sp, "LdtSize");
        case DefaultHardErrorMode:
            return sp_strcpy(sp, "DefaultHardErrorMode");
        case IoPortHandlers:
            return sp_strcpy(sp, "IoPortHandlers");
        case PooledUsageAndLimits:
            return sp_strcpy(sp, "PooledUsageAndLimits");
        case WorkingSetWatch:
            return sp_strcpy(sp, "WorkingSetWatch");
        case UserModeIOPL:
            return sp_strcpy(sp, "UserModeIOPL");
        case EnableAlignmentFaultFixup:
            return sp_strcpy(sp, "EnableAlignmentFaultFixup");
        case PriorityClass:
            return sp_strcpy(sp, "PriorityClass");
        case Wx86Information:
            return sp_strcpy(sp, "Wx86Information");
        case HandleCount:
            return sp_strcpy(sp, "HandleCount");
        case AffinityMask:
            return sp_strcpy(sp, "AffinityMask");
        case PriorityBoost:
            return sp_strcpy(sp, "PriorityBoost");
        case DeviceMap:
            return sp_strcpy(sp, "DeviceMap");
        case SessionInformation:
            return sp_strcpy(sp, "SessionInformation");
        case ForegroundInformation:
            return sp_strcpy(sp, "ForegroundInformation");
        case Wow64Information:
            return sp_strcpy(sp, "Wow64Information");
        case ImageFileName:
            return sp_strcpy(sp, "ImageFileName");
        case LUIDDeviceMapsEnabled:
            return sp_strcpy(sp, "LUIDDeviceMapsEnabled");
        case BreakOnTermination:
            return sp_strcpy(sp, "BreakOnTermination");
        case DebugObjectHandle:
            return sp_strcpy(sp, "DebugObjectHandle");
        case DebugFlags:
            return sp_strcpy(sp, "DebugFlags");
        case HandleTracing:
            return sp_strcpy(sp, "HandleTracing");
        case IoPriority:
            return sp_strcpy(sp, "IoPriority");
        case ExecuteFlags:
            return sp_strcpy(sp, "ExecuteFlags");
        case TlsInformation:
            return sp_strcpy(sp, "TlsInformation");
        case Cookie:
            return sp_strcpy(sp, "Cookie");
        case ImageInformation:
            return sp_strcpy(sp, "ImageInformation");
        case CycleTime:
            return sp_strcpy(sp, "CycleTime");
        case PagePriority:
            return sp_strcpy(sp, "PagePriority");
        case InstrumentationCallback:
            return sp_strcpy(sp, "InstrumentationCallback");
        case ThreadStackAllocation:
            return sp_strcpy(sp, "ThreadStackAllocation");
        case WorkingSetWatchEx:
            return sp_strcpy(sp, "WorkingSetWatchEx");
        case ImageFileNameWin32:
            return sp_strcpy(sp, "ImageFileNameWin32");
        case ImageFileMapping:
            return sp_strcpy(sp, "ImageFileMapping");
        case AffinityUpdateMode:
            return sp_strcpy(sp, "AffinityUpdateMode");
        case MemoryAllocationMode:
            return sp_strcpy(sp, "MemoryAllocationMode");
        case GroupInformation:
            return sp_strcpy(sp, "GroupInformation");
        case TokenVirtualizationEnabled:
            return sp_strcpy(sp, "TokenVirtualizationEnabled");
        case OwnerInformation:
            return sp_strcpy(sp, "OwnerInformation");
        case WindowInformation:
            return sp_strcpy(sp, "WindowInformation");
        case HandleInformation:
            return sp_strcpy(sp, "HandleInformation");
        case MitigationPolicy:
            return sp_strcpy(sp, "MitigationPolicy");
        case DynamicFunctionTableInformation:
            return sp_strcpy(sp, "DynamicFunctionTableInformation");
        case HandleCheckingMode:
            return sp_strcpy(sp, "HandleCheckingMode");
        case KeepAliveCount:
            return sp_strcpy(sp, "KeepAliveCount");
        case RevokeFileHandles:
            return sp_strcpy(sp, "RevokeFileHandles");
        case WorkingSetControl:
            return sp_strcpy(sp, "WorkingSetControl");
        case HandleTable:
            return sp_strcpy(sp, "HandleTable");
        case CheckStackExtentsMode:
            return sp_strcpy(sp, "CheckStackExtentsMode");
        case CommandLineInformation:
            return sp_strcpy(sp, "CommandLineInformation");
        case ProtectionInformation:
            return sp_strcpy(sp, "ProtectionInformation");
        case MemoryExhaustion:
            return sp_strcpy(sp, "MemoryExhaustion");
        case FaultInformation:
            return sp_strcpy(sp, "FaultInformation");
        case TelemetryIdInformation:
            return sp_strcpy(sp, "TelemetryIdInformation");
        case CommitReleaseInformation:
            return sp_strcpy(sp, "CommitReleaseInformation");
        case Reserved1Information:
            return sp_strcpy(sp, "Reserved1Information");
        case Reserved2Information:
            return sp_strcpy(sp, "Reserved2Information");
        case SubsystemProcess:
            return sp_strcpy(sp, "SubsystemProcess");
        case InPrivate:
            return sp_strcpy(sp, "InPrivate");
        case RaiseUMExceptionOnInvalidHandleClose:
            return sp_strcpy(sp, "RaiseUMExceptionOnInvalidHandleClose");
        case SubsystemInformation:
            return sp_strcpy(sp, "SubsystemInformation");
        case Win32kSyscallFilterInformation:
            return sp_strcpy(sp, "Win32kSyscallFilterInformation");
        case EnergyTrackingState:
            return sp_strcpy(sp, "EnergyTrackingState");
        case MoxnoxSetPendingSignal:
            return sp_strcpy(sp, "MoxnoxSetPendingSignal");
        case MoxnoxExec:
            return sp_strcpy(sp, "MoxnoxExec");
        case MoxnoxFindNtdll:
            return sp_strcpy(sp, "MoxnoxFindNtdll");
        case MoxnoxWaitPid:
            return sp_strcpy(sp, "MoxnoxWaitPid");
        case MoxnoxIsOurChild:
            return sp_strcpy(sp, "MoxnoxIsOurChild");
        case MoxnoxChildrenTimes:
            return sp_strcpy(sp, "MoxnoxChildrenTimes");
        case MoxnoxGetSessionId:
            return sp_strcpy(sp, "MoxnoxGetSessionId");
        case MoxnoxNewSession:
            return sp_strcpy(sp, "MoxnoxNewSession");
        case MoxnoxSetControllingTerminal:
            return sp_strcpy(sp, "MoxnoxSetControllingTerminal");
        case MoxnoxGetProcessGroup:
            return sp_strcpy(sp, "MoxnoxGetProcessGroup");
        case MoxnoxSetProcessGroup:
            return sp_strcpy(sp, "MoxnoxSetProcessGroup");
        case MoxnoxSetPendingSignalProcessGroup:
            return sp_strcpy(sp, "MoxnoxSetPendingSignalProcessGroup");
        case MoxnoxMarkAsChild:
            return sp_strcpy(sp, "MoxnoxMarkAsChild");
        case MoxnoxMemoryUnmap:
            return sp_strcpy(sp, "MoxnoxMemoryUnmap");
        case MoxnoxFork:
            return sp_strcpy(sp, "MoxnoxFork");
        case MoxnoxAlarmTimer:
            return sp_strcpy(sp, "MoxnoxAlarmTimer");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<PROCESS_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                MEMORY_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum MEMORY_INFORMATION_CLASS;

        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        case WorkingSetInformation:
            return sp_strcpy(sp, "WorkingSetInformation");
        case MappedFilenameInformation:
            return sp_strcpy(sp, "MappedFilenameInformation");
        case RegionInformation:
            return sp_strcpy(sp, "RegionInformation");
        case WorkingSetExInformation:
            return sp_strcpy(sp, "WorkingSetExInformation");
        case SharedCommitInformation:
            return sp_strcpy(sp, "SharedCommitInformation");
        case ImageInformation:
            return sp_strcpy(sp, "ImageInformation");
        case RegionInformationEx:
            return sp_strcpy(sp, "RegionInformationEx");
        case PrivilegedBasicInformation:
            return sp_strcpy(sp, "PrivilegedBasicInformation");
        case EnclaveImageInformation:
            return sp_strcpy(sp, "EnclaveImageInformation");
        case BasicInformationCapped:
            return sp_strcpy(sp, "BasicInformationCapped");
        case PhysicalContiguityInformation:
            return sp_strcpy(sp, "PhysicalContiguityInformation");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<MEMORY_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                const LARGE_INTEGER& arg) {
    return do_format_arg(sp, "", arg.QuadPart);
}

template<>
consteval bool format_spec_valid<const LARGE_INTEGER>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                const LARGE_INTEGER* arg) {
    if (!arg)
        return sp_strcpy(sp, "NULL");
    else
        return do_format_arg(sp, "", *arg);
}

template<>
consteval bool format_spec_valid<const LARGE_INTEGER*>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                FS_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum FS_INFORMATION_CLASS;

        case VolumeInformation:
            return sp_strcpy(sp, "VolumeInformation");
        case LabelInformation:
            return sp_strcpy(sp, "LabelInformation");
        case SizeInformation:
            return sp_strcpy(sp, "SizeInformation");
        case DeviceInformation:
            return sp_strcpy(sp, "DeviceInformation");
        case AttributeInformation:
            return sp_strcpy(sp, "AttributeInformation");
        case ControlInformation:
            return sp_strcpy(sp, "ControlInformation");
        case FullSizeInformation:
            return sp_strcpy(sp, "FullSizeInformation");
        case ObjectIdInformation:
            return sp_strcpy(sp, "ObjectIdInformation");
        case DriverPathInformation:
            return sp_strcpy(sp, "DriverPathInformation");
        case VolumeFlagsInformation:
            return sp_strcpy(sp, "VolumeFlagsInformation");
        case SectorSizeInformation:
            return sp_strcpy(sp, "SectorSizeInformation");
        case DataCopyInformation:
            return sp_strcpy(sp, "DataCopyInformation");
        case MetadataSizeInformation:
            return sp_strcpy(sp, "MetadataSizeInformation");
        case FullSizeInformationEx:
            return sp_strcpy(sp, "FullSizeInformationEx");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<FS_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                TIMER_TYPE arg) {
    switch (arg) {
        using enum TIMER_TYPE;

        case NotificationTimer:
            return sp_strcpy(sp, "NotificationTimer");
        case SynchronizationTimer:
            return sp_strcpy(sp, "SynchronizationTimer");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<TIMER_TYPE>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                EVENT_TYPE arg) {
    switch (arg) {
        using enum EVENT_TYPE;

        case NotificationEvent:
            return sp_strcpy(sp, "NotificationEvent");
        case SynchronizationEvent:
            return sp_strcpy(sp, "SynchronizationEvent");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<EVENT_TYPE>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                SECTION_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum SECTION_INFORMATION_CLASS;

        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        case ImageInformation:
            return sp_strcpy(sp, "ImageInformation");
        case RelocationInformation:
            return sp_strcpy(sp, "RelocationInformation");
        case OriginalBaseInformation:
            return sp_strcpy(sp, "OriginalBaseInformation");
        case InternalImageInformation:
            return sp_strcpy(sp, "InternalImageInformation");
        case MoxnoxFramebufferInformation:
            return sp_strcpy(sp, "MoxnoxFramebufferInformation");
        case MoxnoxTextConsoleInformation:
            return sp_strcpy(sp, "MoxnoxTextConsoleInformation");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<SECTION_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                OBJECT_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum OBJECT_INFORMATION_CLASS;

        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        case NameInformation:
            return sp_strcpy(sp, "NameInformation");
        case TypeInformation:
            return sp_strcpy(sp, "TypeInformation");
        case TypesInformation:
            return sp_strcpy(sp, "TypesInformation");
        case DataInformation:
            return sp_strcpy(sp, "DataInformation");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<OBJECT_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                SYSTEM_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum SYSTEM_INFORMATION_CLASS;

        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        case ProcessorInformation:
            return sp_strcpy(sp, "ProcessorInformation");
        case PerformanceInformation:
            return sp_strcpy(sp, "PerformanceInformation");
        case TimeOfDayInformation:
            return sp_strcpy(sp, "TimeOfDayInformation");
        case ProcessInformation:
            return sp_strcpy(sp, "ProcessInformation");
        case ProcessorPerformanceInformation:
            return sp_strcpy(sp, "ProcessorPerformanceInformation");
        case HandleInformation:
            return sp_strcpy(sp, "HandleInformation");
        case PagefileInformation:
            return sp_strcpy(sp, "PagefileInformation");
        case InterruptInformation:
            return sp_strcpy(sp, "InterruptInformation");
        case ExceptionInformation:
            return sp_strcpy(sp, "ExceptionInformation");
        case RegistryQuotaInformation:
            return sp_strcpy(sp, "RegistryQuotaInformation");
        case LookasideInformation:
            return sp_strcpy(sp, "LookasideInformation");
        case CodeIntegrityInformation:
            return sp_strcpy(sp, "CodeIntegrityInformation");
        case PolicyInformation:
            return sp_strcpy(sp, "PolicyInformation");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<SYSTEM_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                SHUTDOWN_ACTION arg) {
    switch (arg) {
        using enum SHUTDOWN_ACTION;

        case ShutdownNoReboot:
            return sp_strcpy(sp, "ShutdownNoReboot");
        case ShutdownReboot:
            return sp_strcpy(sp, "ShutdownReboot");
        case ShutdownPowerOff:
            return sp_strcpy(sp, "ShutdownPowerOff");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<SHUTDOWN_ACTION>(string_view fmt) {
    return fmt.empty();
}

export span<char> do_format_arg(span<char> sp, string_view,
                                TIMER_INFORMATION_CLASS arg) {
    switch (arg) {
        using enum TIMER_INFORMATION_CLASS;

        case BasicInformation:
            return sp_strcpy(sp, "BasicInformation");
        default:
            return do_format_arg(sp, "", (unsigned int)arg);
    }
}

template<>
consteval bool format_spec_valid<TIMER_INFORMATION_CLASS>(string_view fmt) {
    return fmt.empty();
}
