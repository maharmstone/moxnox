// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>

import nt;
import ntdll;
import print;
import win32error;

extern "C"
void RtlSetLastWin32Error(uint32_t err_code) {
    NtCurrentTeb()->LastErrorValue = err_code;
}

extern "C"
uint32_t RtlNtStatusToDosErrorNoTeb(nt_status status) {
    switch (status) {
        using enum nt_status;

        case SUCCESS:
            return ERROR_SUCCESS;
        case USER_APC:
            return ERROR_USER_APC;
        case ALERTED:
            return ERROR_ALERTED;
        case TIMEOUT:
            return ERROR_TIMEOUT;
        case PENDING:
            return ERROR_IO_PENDING;
        case REPARSE:
            return ERROR_REPARSE;
        case IMAGE_MACHINE_TYPE_MISMATCH_EXE:
            return ERROR_IMAGE_MACHINE_TYPE_MISMATCH_EXE;
        case BREAKPOINT:
            return (uint32_t)nt_status::BREAKPOINT;
        case SINGLE_STEP:
            return (uint32_t)nt_status::SINGLE_STEP;
        case BUFFER_OVERFLOW:
            return ERROR_MORE_DATA;
        case NO_MORE_FILES:
            return ERROR_NO_MORE_FILES;
        case NO_MORE_ENTRIES:
            return ERROR_NO_MORE_ITEMS;
        case NOT_IMPLEMENTED:
            return ERROR_INVALID_FUNCTION;
        case INVALID_INFO_CLASS:
            return ERROR_INVALID_PARAMETER;
        case INFO_LENGTH_MISMATCH:
            return ERROR_BAD_LENGTH;
        case ACCESS_VIOLATION:
            return ERROR_NOACCESS;
        case INVALID_HANDLE:
            return ERROR_INVALID_HANDLE;
        case INVALID_CID:
            return ERROR_INVALID_PARAMETER;
        case INVALID_PARAMETER:
            return ERROR_INVALID_PARAMETER;
        case INVALID_DEVICE_REQUEST:
            return ERROR_INVALID_FUNCTION;
        case END_OF_FILE:
            return ERROR_HANDLE_EOF;
        case NO_MEMORY:
            return ERROR_NOT_ENOUGH_MEMORY;
        case ILLEGAL_INSTRUCTION:
            return (uint32_t)nt_status::ILLEGAL_INSTRUCTION;
        case ACCESS_DENIED:
            return ERROR_ACCESS_DENIED;
        case BUFFER_TOO_SMALL:
            return ERROR_INSUFFICIENT_BUFFER;
        case OBJECT_TYPE_MISMATCH:
            return ERROR_INVALID_HANDLE;
        case INVALID_DISPOSITION:
            return (uint32_t)nt_status::INVALID_DISPOSITION;
        case UNWIND:
            return ERROR_UNWIND;
        case BAD_STACK:
            return ERROR_BAD_STACK;
        case DISK_CORRUPT_ERROR:
            return ERROR_DISK_CORRUPT;
        case OBJECT_NAME_INVALID:
            return ERROR_INVALID_NAME;
        case OBJECT_NAME_NOT_FOUND:
            return ERROR_FILE_NOT_FOUND;
        case OBJECT_NAME_COLLISION:
            return ERROR_ALREADY_EXISTS;
        case OBJECT_PATH_NOT_FOUND:
            return ERROR_PATH_NOT_FOUND;
        case OBJECT_PATH_SYNTAX_BAD:
            return ERROR_BAD_PATHNAME;
        case CRC_ERROR:
            return ERROR_CRC;
        case SHARING_VIOLATION:
            return ERROR_SHARING_VIOLATION;
        case INVALID_PAGE_PROTECTION:
            return ERROR_INVALID_PARAMETER;
        case THREAD_IS_TERMINATING:
            return ERROR_ACCESS_DENIED;
        case PROCEDURE_NOT_FOUND:
            return ERROR_PROC_NOT_FOUND;
        case INVALID_IMAGE_FORMAT:
            return ERROR_BAD_EXE_FORMAT;
        case DISK_FULL:
            return ERROR_DISK_FULL;
        case INTEGER_DIVIDE_BY_ZERO:
            return (uint32_t)nt_status::INTEGER_DIVIDE_BY_ZERO;
        case INSUFFICIENT_RESOURCES:
            return ERROR_NO_SYSTEM_RESOURCES;
        case MEDIA_WRITE_PROTECTED:
            return ERROR_WRITE_PROTECT;
        case DEVICE_NOT_READY:
            return ERROR_NOT_READY;
        case PIPE_DISCONNECTED:
            return ERROR_PIPE_NOT_CONNECTED;
        case IO_TIMEOUT:
            return ERROR_SEM_TIMEOUT;
        case FILE_IS_A_DIRECTORY:
            return ERROR_ACCESS_DENIED;
        case NOT_SUPPORTED:
            return ERROR_NOT_SUPPORTED;
        case NOT_SAME_DEVICE:
            return ERROR_NOT_SAME_DEVICE;
        case CANT_WAIT:
            return ERROR_CANT_WAIT;
        case INTERNAL_ERROR:
            return ERROR_INTERNAL_ERROR;
        case DIRECTORY_NOT_EMPTY:
            return ERROR_DIR_NOT_EMPTY;
        case NOT_A_DIRECTORY:
            return ERROR_DIRECTORY;
        case PROCESS_IS_TERMINATING:
            return ERROR_ACCESS_DENIED;
        case DLL_NOT_FOUND:
            return ERROR_MOD_NOT_FOUND;
        case ORDINAL_NOT_FOUND:
            return ERROR_INVALID_ORDINAL;
        case CONTROL_C_EXIT:
            return ERROR_CONTROL_C_EXIT;
        case DLL_INIT_FAILED:
            return ERROR_DLL_INIT_FAILED;
        case UNHANDLED_EXCEPTION:
            return ERROR_UNHANDLED_EXCEPTION;
        case PIPE_BROKEN:
            return ERROR_BROKEN_PIPE;
        case UNRECOGNIZED_VOLUME:
            return ERROR_UNRECOGNIZED_VOLUME;
        case IO_DEVICE_ERROR:
            return ERROR_IO_DEVICE;
        case FS_DRIVER_REQUIRED:
            return ERROR_FS_DRIVER_REQUIRED;
        case INVALID_BUFFER_SIZE:
            return ERROR_INVALID_USER_BUFFER;
        case INVALID_ADDRESS_COMPONENT:
            return ERROR_INVALID_NETNAME;
        case CONNECTION_DISCONNECTED:
            return ERROR_NETNAME_DELETED;
        case CONNECTION_RESET:
            return ERROR_NETNAME_DELETED;
        case NOT_FOUND:
            return ERROR_NOT_FOUND;
        case CONNECTION_REFUSED:
            return ERROR_CONNECTION_REFUSED;
        case ADDRESS_ALREADY_ASSOCIATED:
            return ERROR_ADDRESS_ALREADY_ASSOCIATED;
        case CONNECTION_ACTIVE:
            return ERROR_CONNECTION_ACTIVE;
        case NETWORK_UNREACHABLE:
            return ERROR_NETWORK_UNREACHABLE;
        case HOST_UNREACHABLE:
            return ERROR_HOST_UNREACHABLE;
        case PROTOCOL_UNREACHABLE:
            return ERROR_PROTOCOL_UNREACHABLE;
        case CONNECTION_ABORTED:
            return ERROR_CONNECTION_ABORTED;
        case ILLEGAL_DLL_RELOCATION:
            return ERROR_ILLEGAL_DLL_RELOCATION;
        case NOT_A_REPARSE_POINT:
            return ERROR_NOT_A_REPARSE_POINT;
        case REPARSE_POINT_NOT_RESOLVED:
            return ERROR_CANT_RESOLVE_FILENAME;
        case FLOAT_MULTIPLE_FAULTS:
            return ERROR_FLOAT_MULTIPLE_FAULTS;
        case FLOAT_MULTIPLE_TRAPS:
            return ERROR_FLOAT_MULTIPLE_TRAPS;
        case DEVICE_REMOVED:
            return ERROR_DEVICE_REMOVED;
        case SYSTEM_SHUTDOWN:
            return ERROR_SYSTEM_SHUTDOWN;
        case STACK_BUFFER_OVERRUN:
            return ERROR_STACK_BUFFER_OVERRUN;
        case PROTOCOL_NOT_SUPPORTED:
            break;
    }

    return ERROR_MR_MID_NOT_FOUND;
}

extern "C"
uint32_t RtlNtStatusToDosError(nt_status status) {
    auto& teb = *NtCurrentTeb();

    teb.LastStatusValue = status;

    return RtlNtStatusToDosErrorNoTeb(status);
}
