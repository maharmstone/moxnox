// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <span>
#include <string_view>
#include <atomic>
#include <optional>
#include <functional>
#include <utility>
#include <mutex>
#include "timer-asm.h"
#include "trace.h"
#include "assert.h"

import nt;
import format;
import print;
import config;
import visit;
import unicode;
import mem;
import thread;
import disable_interrupts;
import cpu_block;
import process;
import list;
import bsod;
import scheduler;
import trace;
import vfs;
import mapping;
import rangelock;
import moxnox_ioctls;

#ifdef __aarch64__
import aarch64;
#endif

using namespace std;

extern "C"
NTSTATUS NtDisplayString(const UNICODE_STRING* string) {
    auto u16sv = u16string_view(string->Buffer, string->Length / sizeof(char16_t));
    auto buflen = utf16_to_utf8_len(u16sv);

    if (buflen == 0)
        return nt_status::SUCCESS;

    auto buf = (char*)malloc(buflen);
    if (!buf)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(buf, buflen), u16sv);

    print("{}", string_view(buf, buflen));

    free(buf);

    return nt_status::SUCCESS;
}

extern "C" bool copy_from_process(void* ks_addr, const void* us_addr, size_t size) {
    auto t = get_current_thread();

    return t->proc->copy_from(ks_addr, us_addr, size);
}

static bool copy_to_process(void* us_addr, const void* ks_addr, size_t size) {
    auto t = get_current_thread();

    return t->proc->copy_to(us_addr, ks_addr, size);
}

static bool copy_unicode_string_from_process(const UNICODE_STRING* us_us, UNICODE_STRING& ks_us) {
    if (!copy_from_process(&ks_us, us_us, sizeof(UNICODE_STRING)))
        return false;

    if (ks_us.Buffer) {
        auto buf = (char16_t*)malloc(ks_us.Length);
        if (!buf)
            return false;

        if (!copy_from_process(buf, ks_us.Buffer, ks_us.Length)) {
            free(buf);
            return false;
        }

        ks_us.Buffer = buf;
    }

    return true;
}

extern "C"
NTSTATUS user_NtDisplayString(const UNICODE_STRING* string) {
    UNICODE_STRING us;

    // FIXME - needs TCB privilege

    if (!copy_unicode_string_from_process(string, us))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtDisplayString(&us);

    if (us.Buffer)
        free(us.Buffer);

    return ret;
}

extern "C"
NTSTATUS user_NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status) {
    return NtTerminateThread(thread_handle, exit_status);
}

extern "C"
NTSTATUS user_NtTerminateProcess(HANDLE process_handle, NTSTATUS exit_status) {
    return NtTerminateProcess(process_handle, exit_status);
}

extern "C"
NTSTATUS user_NtWriteFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                          void* apc_context, IO_STATUS_BLOCK* us_iosb, void* us_buf,
                          uint32_t length, LARGE_INTEGER* us_byte_offset, uint32_t* us_key) {
    auto& t = *get_current_thread();
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER byte_offset;
    uint32_t key;
    nt_status status;

    if (!us_iosb)
        return nt_status::INVALID_PARAMETER;

    if (us_byte_offset) {
        if (!copy_from_process(&byte_offset, us_byte_offset, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (us_key) {
        if (!copy_from_process(&key, us_key, sizeof(uint32_t)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (length > 0 && (!is_userspace_address((uintptr_t)us_buf) || !is_userspace_address((uintptr_t)us_buf + length - 1)))
        return nt_status::ACCESS_VIOLATION;

    optional<rangelock_req> rlr;
    span<const uint8_t>* spans = nullptr;
    size_t num_spans = 0;

    if (length > 0) {
        status = pin_userspace_buffer_for_reading(*t.proc, span((uint8_t*)us_buf, length),
                                                  rlr, spans, num_spans);
        if (!success(status))
            return status;
    }

    status = vec_NtWriteFile(file_handle, event, apc_routine, apc_context, &iosb,
                             span(spans, num_spans), us_byte_offset ? &byte_offset : nullptr,
                             us_key ? &key : nullptr);

    rlr.reset();

    auto spans_size = sizeof(span<const uint8_t>) * num_spans;
    if (spans_size > PAGE_SIZE)
        vfree(spans, spans_size);
    else if (spans_size > 0)
        free(spans);

    if (success(status)) {
        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            status = nt_status::ACCESS_VIOLATION;
    }

    return status;
}

extern "C"
NTSTATUS user_NtResumeProcess(HANDLE proc_handle) {
    return NtResumeProcess(proc_handle);
}

extern "C"
NTSTATUS user_NtSuspendProcess(HANDLE proc_handle) {
    return NtSuspendProcess(proc_handle);
}

extern "C"
NTSTATUS user_NtAllocateVirtualMemory(HANDLE proc_handle, void** base_address, uintptr_t zero_bits,
                                      size_t* region_size, uint32_t allocation_type,
                                      uint32_t protect) {
    if (!region_size || !base_address)
        return nt_status::INVALID_PARAMETER;

    void* addr;
    size_t size;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&size, region_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtAllocateVirtualMemory(proc_handle, &addr, zero_bits, &size, allocation_type, protect);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(region_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtFreeVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                                  uint32_t free_type) {
    if (!region_size || !base_address)
        return nt_status::INVALID_PARAMETER;

    void* addr;
    size_t size;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&size, region_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtFreeVirtualMemory(proc_handle, &addr, &size, free_type);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(region_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtReadFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                         void* apc_context, IO_STATUS_BLOCK* us_iosb, void* us_buf,
                         uint32_t length, LARGE_INTEGER* us_byte_offset,
                         uint32_t* us_key) {
    auto& t = *get_current_thread();
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER byte_offset;
    uint32_t key;
    nt_status status;

    if (!us_iosb)
        return nt_status::INVALID_PARAMETER;

    if (us_byte_offset) {
        if (!copy_from_process(&byte_offset, us_byte_offset, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (us_key) {
        if (!copy_from_process(&key, us_key, sizeof(uint32_t)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (length > 0 && (!is_userspace_address((uintptr_t)us_buf) || !is_userspace_address((uintptr_t)us_buf + length - 1)))
        return nt_status::ACCESS_VIOLATION;

    optional<rangelock_req> rlr;
    span<uint8_t>* spans = nullptr;
    size_t num_spans = 0;

    if (length > 0) {
        status = pin_userspace_buffer_for_writing(*t.proc, span((uint8_t*)us_buf, length),
                                                  rlr, spans, num_spans);
        if (!success(status))
            return status;
    }

    status = vec_NtReadFile(file_handle, event, apc_routine, apc_context, &iosb,
                            span(spans, num_spans),  us_byte_offset ? &byte_offset : nullptr,
                            us_key ? &key : nullptr);

    rlr.reset();

    auto spans_size = sizeof(span<uint8_t>) * num_spans;
    if (spans_size > PAGE_SIZE)
        vfree(spans, spans_size);
    else if (spans_size > 0)
        free(spans);

    if (success(status)) {
        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            status = nt_status::ACCESS_VIOLATION;
    }

    return status;
}

extern "C"
NTSTATUS user_NtProtectVirtualMemory(HANDLE proc_handle, void** base_address, size_t* region_size,
                                     uint32_t new_prot, uint32_t* us_old_prot) {
    if (!region_size || !base_address)
        return nt_status::INVALID_PARAMETER;

    if (!us_old_prot)
        return nt_status::ACCESS_VIOLATION;

    void* addr;
    size_t size;
    uint32_t old_prot;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&size, region_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtProtectVirtualMemory(proc_handle, &addr, &size, new_prot, &old_prot);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(region_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(us_old_prot, &old_prot, sizeof(uint32_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
void unimplemented_syscall(const char* name) {
    auto& t = *get_current_thread();
    bool written = false;
    char s[255];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Unimplemented syscall {}.\n", name);

    if (t.proc) {
        auto objrw_stderr = t.proc->get_stderr();

        if (objrw_stderr) {
            uint64_t written_bytes;

            // FIXME - make sure process has permissions to write to handle

            auto sp2 = span((const uint8_t*)s, (size_t)(sp.data() - s));

            // FIXME - handle short writes
            auto status = object_write_without_offset(objrw_stderr.get(),
                                                      span(&sp2, 1),
                                                      written_bytes);
            if (success(status))
                written = true;
        }
    }

    if (!written)
        print("{}", string_view(s, (size_t)(sp.data() - s)));

    NtTerminateProcess(NtCurrentProcess(), nt_status::NOT_IMPLEMENTED);
}

static NTSTATUS copy_object_attributes(OBJECT_ATTRIBUTES& oa, UNICODE_STRING& us, const OBJECT_ATTRIBUTES* us_oa) {
    if (!copy_from_process(&oa, us_oa, sizeof(OBJECT_ATTRIBUTES)))
        return nt_status::ACCESS_VIOLATION;

    if (oa.Length < sizeof(OBJECT_ATTRIBUTES))
        return nt_status::INVALID_PARAMETER;

    if (oa.ObjectName) {
        if (!copy_from_process(&us, oa.ObjectName, sizeof(UNICODE_STRING)))
            return nt_status::ACCESS_VIOLATION;

        if (us.Length > PAGE_SIZE)
            return nt_status::INVALID_PARAMETER;

        if (us.Length == 0)
            us.Buffer = nullptr;
        else {
            auto buf = (char16_t*)malloc(us.Length);
            if (!buf)
                return nt_status::INSUFFICIENT_RESOURCES;

            if (!copy_from_process(buf, us.Buffer, us.Length)) {
                free(buf);
                return nt_status::ACCESS_VIOLATION;
            }

            us.Buffer = buf;
        }

        oa.ObjectName = &us;
    }

    // FIXME - SecurityDescriptor
    // FIXME - SecurityQualityOfService

    return nt_status::SUCCESS;
}

static void free_object_attributes(OBJECT_ATTRIBUTES& oa) {
    if (oa.ObjectName && oa.ObjectName->Buffer)
        free(oa.ObjectName->Buffer);

    // FIXME - SecurityDescriptor
    // FIXME - SecurityQualityOfService
}

extern "C"
NTSTATUS user_NtCreateFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                           const OBJECT_ATTRIBUTES* object_attributes,
                           IO_STATUS_BLOCK* io_status_block,
                           const LARGE_INTEGER* allocation_size,
                           uint32_t file_attributes, uint32_t share_access,
                           uint32_t create_disposition, uint32_t create_options,
                           void* ea_buffer, uint32_t ea_length) {
    HANDLE h;
    IO_STATUS_BLOCK iosb;
    LARGE_INTEGER alloc;
    void* eabuf = nullptr;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS ret;

    if (!file_handle || !io_status_block)
        return nt_status::INVALID_PARAMETER;

    if (allocation_size) {
        if (!copy_from_process(&alloc, allocation_size, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (ea_length > 0) {
        eabuf = malloc(ea_length);
        if (!eabuf)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (!copy_from_process(eabuf, ea_buffer, ea_length)) {
            free(eabuf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    ret = copy_object_attributes(oa, us, object_attributes);
    if (!success(ret)) {
        if (eabuf)
            free(eabuf);

        return ret;
    }

    ret = NtCreateFile(&h, desired_access, &oa, &iosb, allocation_size ? &alloc : nullptr,
                       file_attributes, share_access, create_disposition, create_options,
                       eabuf, ea_length);

    if (eabuf)
        free(eabuf);

    free_object_attributes(oa);

    if (success(ret)) {
        if (!copy_to_process(file_handle, &h, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(io_status_block, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtClose(HANDLE handle) {
    return NtClose(handle);
}

extern "C"
NTSTATUS user_NtCreateSection(HANDLE* section_handle, ACCESS_MASK desired_access,
                              const OBJECT_ATTRIBUTES* object_attributes,
                              const LARGE_INTEGER* maximum_size,
                              uint32_t section_page_protection,
                              uint32_t allocation_attributes, HANDLE file_handle) {
    LARGE_INTEGER size;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;

    if (!section_handle)
        return nt_status::INVALID_PARAMETER;

    if (maximum_size) {
        if (!copy_from_process(&size, maximum_size, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (object_attributes) {
        auto ret = copy_object_attributes(oa, us, object_attributes);
        if (!success(ret))
            return ret;
    }

    auto ret = NtCreateSection(&h, desired_access, object_attributes ? &oa : nullptr,
                               maximum_size ? &size : nullptr, section_page_protection,
                               allocation_attributes, file_handle);

    if (object_attributes)
        free_object_attributes(oa);

    if (success(ret)) {
        if (!copy_to_process(section_handle, &h, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtMapViewOfSection(HANDLE section_handle, HANDLE process_handle,
                                 void** base_address, uintptr_t zero_bits,
                                 size_t commit_size, LARGE_INTEGER* section_offset,
                                 size_t* view_size, enum SECTION_INHERIT inherit_disposition,
                                 uint32_t allocation_type, uint32_t win32_protect) {
    void* addr;
    LARGE_INTEGER offset;
    size_t size;

    if (!base_address)
        return nt_status::INVALID_PARAMETER;

    if (!view_size)
        return nt_status::INVALID_PARAMETER;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (section_offset) {
        if (!copy_from_process(&offset, section_offset, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (!copy_from_process(&size, view_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtMapViewOfSection(section_handle, process_handle, &addr, zero_bits,
                                  commit_size, section_offset ? &offset : nullptr,
                                  &size, inherit_disposition,
                                  allocation_type, win32_protect);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (section_offset) {
            if (!copy_to_process(section_offset, &offset, sizeof(LARGE_INTEGER)))
                ret = nt_status::ACCESS_VIOLATION;
        }

        if (!copy_to_process(view_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtUnmapViewOfSection(HANDLE process_handle, void* base_address) {
    return NtUnmapViewOfSection(process_handle, base_address);
}

extern "C"
NTSTATUS user_NtQueryDirectoryFile(HANDLE file_handle, HANDLE event,
                                   PIO_APC_ROUTINE apc_routine, void* apc_context,
                                   IO_STATUS_BLOCK* us_iosb, void* us_file_information,
                                   uint32_t length, FILE_INFORMATION_CLASS fic,
                                   bool return_single_entry, const UNICODE_STRING* us_file_name,
                                   bool restart_scan) {
    IO_STATUS_BLOCK iosb;
    void* info;
    UNICODE_STRING file_name;

    if (!file_handle)
        return nt_status::INVALID_PARAMETER;

    if (!us_iosb || !us_file_information)
        return nt_status::INVALID_PARAMETER;

    if (us_file_name) {
        if (!copy_unicode_string_from_process(us_file_name, file_name))
            return nt_status::ACCESS_VIOLATION;
    }

    if (length > 0) {
        info = malloc(length);
        if (!info) {
            if (us_file_name && file_name.Buffer)
                free(file_name.Buffer);

            return nt_status::INSUFFICIENT_RESOURCES;
        }
    } else
        info = nullptr;

    auto ret = NtQueryDirectoryFile(file_handle, event, apc_routine, apc_context, &iosb, info, length,
                                    fic, return_single_entry, us_file_name ? &file_name : nullptr,
                                    restart_scan);

    if (success(ret)) {
        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;

        if (iosb.Information > 0) {
            if (!copy_to_process(us_file_information, info, iosb.Information))
                ret = nt_status::ACCESS_VIOLATION;
        }
    }

    if (info)
        free(info);

    if (us_file_name && file_name.Buffer)
        free(file_name.Buffer);

    return ret;
}

extern "C"
NTSTATUS user_NtCreateThread(HANDLE* us_thread_handle, ACCESS_MASK desired_access,
                             const OBJECT_ATTRIBUTES* object_attributes,
                             HANDLE process_handle, CLIENT_ID* us_client_id,
                             const CONTEXT* us_thread_context,
                             const INITIAL_TEB* us_initial_teb,
                             bool create_suspended) {
    NTSTATUS status;
    HANDLE h;
    CLIENT_ID client_id;
    CONTEXT thread_context;
    INITIAL_TEB initial_teb;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;

    if (!us_thread_handle || !us_client_id || !us_thread_context || !us_initial_teb)
        return nt_status::INVALID_PARAMETER;


    if (!copy_from_process(&thread_context, us_thread_context, sizeof(CONTEXT)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&initial_teb, us_initial_teb, sizeof(INITIAL_TEB)))
        return nt_status::ACCESS_VIOLATION;

    if (object_attributes) {
        status = copy_object_attributes(oa, us, object_attributes);
        if (!success(status))
            return status;
    }

    status = NtCreateThread(&h, desired_access, object_attributes ? &oa : nullptr, process_handle,
                            &client_id, &thread_context, &initial_teb, create_suspended);

    if (success(status)) {
        if (!copy_to_process(us_thread_handle, &h, sizeof(HANDLE)))
            status = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(us_client_id, &client_id, sizeof(CLIENT_ID)))
            status = nt_status::ACCESS_VIOLATION;
    }

    if (object_attributes)
        free_object_attributes(oa);

    return status;
}

extern "C"
NTSTATUS user_NtResumeThread(HANDLE thread_handle, uint32_t* suspend_count) {
    uint32_t count;

    auto status = NtResumeThread(thread_handle, suspend_count ? &count : 0);

    if (success(status)) {
        if (suspend_count) {
            if (!copy_to_process(suspend_count, &count, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    return status;
}

extern "C"
NTSTATUS user_NtWaitForSingleObject(HANDLE object_handle, bool alertable, const LARGE_INTEGER* us_time_out) {
    LARGE_INTEGER time_out;

    // FIXME - are NtCurrentProcess() or NtThreadProcess() valid values for object_handle?

    if (us_time_out) {
        if (!copy_from_process(&time_out, us_time_out, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    return NtWaitForSingleObject(object_handle, alertable, us_time_out ? &time_out : nullptr);
}

extern "C"
NTSTATUS user_NtWaitForMultipleObjects(uint32_t object_count, HANDLE* objects_array,
                                       bool wait_any, bool alertable,
                                       const LARGE_INTEGER* us_time_out) {
    LARGE_INTEGER time_out;

    if (us_time_out) {
        if (!copy_from_process(&time_out, us_time_out, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    if (object_count == 0 || object_count > MAXIMUM_WAIT_OBJECTS)
        return nt_status::INVALID_PARAMETER;

    auto array = malloc(sizeof(HANDLE) * object_count);
    if (!array)
        return nt_status::INSUFFICIENT_RESOURCES;

    if (!copy_from_process(array, objects_array, sizeof(HANDLE) * object_count)) {
        free(array);
        return nt_status::ACCESS_VIOLATION;
    }

    auto ret = NtWaitForMultipleObjects(object_count, (HANDLE*)array, wait_any,
                                        alertable, us_time_out ? &time_out : nullptr);

    free(array);

    return ret;
}

extern "C"
NTSTATUS user_NtCreateProcess(HANDLE* process_handle, ACCESS_MASK desired_access,
                              const OBJECT_ATTRIBUTES* object_attributes,
                              HANDLE parent_process, bool inherit_object_table,
                              HANDLE section_handle, HANDLE debug_port,
                              HANDLE exception_port) {
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS status;

    if (!process_handle)
        return nt_status::INVALID_PARAMETER;

    if (object_attributes) {
        status = copy_object_attributes(oa, us, object_attributes);
        if (!success(status))
            return status;
    }

    status = NtCreateProcess(&h, desired_access, object_attributes ? &oa : nullptr, parent_process,
                             inherit_object_table, section_handle, debug_port, exception_port);

    if (success(status)) {
        if (!copy_to_process(process_handle, &h, sizeof(HANDLE)))
            status = nt_status::ACCESS_VIOLATION;
    }

    if (object_attributes)
        free_object_attributes(oa);

    return status;
}

extern "C"
NTSTATUS user_NtDuplicateObject(HANDLE source_process_handle, HANDLE source_handle,
                                HANDLE target_process_handle, HANDLE* target_handle,
                                ACCESS_MASK desired_access, uint32_t handle_attributes,
                                uint32_t options) {
    NTSTATUS status;
    HANDLE h;

    status = NtDuplicateObject(source_process_handle, source_handle, target_process_handle, target_handle ? &h : nullptr,
                               desired_access, handle_attributes, options);

    if (success(status)) {
        if (target_handle) {
            if (!copy_to_process(target_handle, &h, sizeof(HANDLE)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    return status;
}

extern "C"
NTSTATUS user_NtReadVirtualMemory(HANDLE process_handle, void* base_address,
                                  void* buffer, uint32_t buffer_size,
                                  uint32_t* number_of_bytes_read) {
    NTSTATUS status;
    uint32_t read;

    if (!buffer)
        return nt_status::INVALID_PARAMETER;

    status = NtReadVirtualMemory(process_handle, base_address, buffer, buffer_size,
                                 number_of_bytes_read ? &read : nullptr);

    if (success(status)) {
        if (number_of_bytes_read) {
            if (!copy_to_process(number_of_bytes_read, &read, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    return status;
}

extern "C"
NTSTATUS user_NtWriteVirtualMemory(HANDLE process_handle, void* base_address,
                                   const void* buffer, uint32_t buffer_size,
                                   uint32_t* number_of_bytes_written) {
    NTSTATUS status;
    uint32_t written;

    if (!buffer)
        return nt_status::INVALID_PARAMETER;

    status = NtWriteVirtualMemory(process_handle, base_address, buffer, buffer_size,
                                  number_of_bytes_written ? &written : nullptr);

    if (success(status)) {
        if (number_of_bytes_written) {
            if (!copy_to_process(number_of_bytes_written, &written, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    return status;
}

extern "C"
NTSTATUS user_NtQueryInformationProcess(HANDLE process_handle, enum PROCESS_INFORMATION_CLASS process_information_class,
                                        void* process_information, uint32_t process_information_length,
                                        uint32_t* return_length) {
    NTSTATUS status;
    void* buf = nullptr;
    uint32_t retlen;

    if (!process_information)
        process_information_length = 0;

    if (process_information_length > 0) {
        buf = malloc(process_information_length);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtQueryInformationProcess(process_handle, process_information_class,
                                       process_information_length > 0 ? buf : nullptr,
                                       process_information_length, &retlen);

    if (success(status)) {
        if (retlen > 0) {
            if (process_information_class == PROCESS_INFORMATION_CLASS::ImageFileName) {
                auto& us = *(UNICODE_STRING*)buf;

                us.Buffer = (char16_t*)((uint8_t*)us.Buffer - (uint8_t*)buf +
                                        (uint8_t*)process_information);
            }

            if (!copy_to_process(process_information, buf, retlen))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    if (return_length) {
        if (!copy_to_process(return_length, &retlen, sizeof(uint32_t)))
            status = nt_status::ACCESS_VIOLATION;
    }

    if (process_information_length > 0)
        free(buf);

    return status;
}

extern "C"
NTSTATUS user_NtQuerySection(HANDLE section_handle, enum SECTION_INFORMATION_CLASS section_information_class,
                             void* section_information, size_t section_information_length,
                             size_t* return_length) {
    NTSTATUS status;
    void* buf = nullptr;
    size_t retlen;

    if (!section_information)
        return nt_status::INVALID_PARAMETER;

    if (section_information_length > 0) {
        buf = malloc(section_information_length);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtQuerySection(section_handle, section_information_class,
                            section_information_length > 0 ? buf : nullptr,
                            section_information_length, &retlen);

    if (success(status)) {
        if (retlen > 0) {
            if (!copy_to_process(section_information, buf, retlen))
                status = nt_status::ACCESS_VIOLATION;
        }

        if (return_length) {
            if (!copy_to_process(return_length, &retlen, sizeof(size_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    if (section_information_length > 0)
        free(buf);

    return status;
}

extern "C"
NTSTATUS user_NtQueryInformationThread(HANDLE thread_handle, enum THREAD_INFORMATION_CLASS thread_information_class,
                                       void* thread_information, uint32_t thread_information_length,
                                       uint32_t* return_length) {
    NTSTATUS status;
    void* buf = nullptr;
    uint32_t retlen;

    if (!thread_information)
        return nt_status::INVALID_PARAMETER;

    if (thread_information_length > 0) {
        buf = malloc(thread_information_length);
        if (!buf)
            return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtQueryInformationThread(thread_handle, thread_information_class,
                                      thread_information_length > 0 ? buf : nullptr,
                                      thread_information_length, &retlen);

    if (success(status)) {
        if (retlen > 0) {
            if (!copy_to_process(thread_information, buf, retlen))
                status = nt_status::ACCESS_VIOLATION;
        }

        if (return_length) {
            if (!copy_to_process(return_length, &retlen, sizeof(uint32_t)))
                status = nt_status::ACCESS_VIOLATION;
        }
    }

    if (thread_information_length > 0)
        free(buf);

    return status;
}

extern "C"
NTSTATUS user_NtCreateTimer(HANDLE* timer_handle, ACCESS_MASK desired_access,
                            const OBJECT_ATTRIBUTES* object_attributes,
                            TIMER_TYPE timer_type) {
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;

    if (object_attributes) {
        auto ret = copy_object_attributes(oa, us, object_attributes);
        if (!success(ret))
            return ret;
    }

    auto ret = NtCreateTimer(&h, desired_access, object_attributes ? &oa : nullptr,
                             timer_type);

    if (object_attributes)
        free_object_attributes(oa);

    if (success(ret)) {
        if (!copy_to_process(timer_handle, &h, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtSetTimer(HANDLE timer_handle, const LARGE_INTEGER* due_time,
                         PTIMER_APC_ROUTINE timer_apc_routine, void* timer_context,
                         bool resume_timer, int32_t period, bool* previous_state) {
    LARGE_INTEGER ks_due_time;
    bool ks_previous_state;

    if (!copy_from_process(&ks_due_time, due_time, sizeof(ks_due_time)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtSetTimer(timer_handle, &ks_due_time, timer_apc_routine,
                          timer_context, resume_timer, period,
                          previous_state ? &ks_previous_state : nullptr);

    if (success(ret)) {
        if (previous_state && !copy_to_process(previous_state, &ks_previous_state, sizeof(ks_previous_state)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtCancelTimer(HANDLE timer_handle, bool* current_state) {
    bool ks_current_state;

    auto ret = NtCancelTimer(timer_handle,
                             current_state ? &ks_current_state : nullptr);

    if (success(ret)) {
        if (current_state && !copy_to_process(current_state, &ks_current_state, sizeof(ks_current_state)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtShutdownSystem(enum SHUTDOWN_ACTION action) {
    // FIXME - check user has SeShutdownPrivilege

    return NtShutdownSystem(action);
}

static void print_ioctl_code(uint32_t code) {
    switch (code) {
        case IOCTL_MOXNOX_CONSOLE_SET_CURSOR:
            print("IOCTL_MOXNOX_CONSOLE_SET_CURSOR");
            break;
        case IOCTL_MOXNOX_VFSCTL_MOUNT:
            print("IOCTL_MOXNOX_VFSCTL_MOUNT");
            break;
        case IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS:
            print("IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS");
            break;
        case IOCTL_MOXNOX_VFSCTL_GET_MOUNT_INFO:
            print("IOCTL_MOXNOX_VFSCTL_GET_MOUNT_INFO");
            break;
        case IOCTL_MOXNOX_VFSCTL_UNMOUNT:
            print("IOCTL_MOXNOX_VFSCTL_UNMOUNT");
            break;
        case IOCTL_MOXNOX_NETCTL_SOCKET:
            print("IOCTL_MOXNOX_NETCTL_SOCKET");
            break;
        case IOCTL_MOXNOX_NETCTL_GET_NETIF_COUNT:
            print("IOCTL_MOXNOX_NETCTL_GET_NETIF_COUNT");
            break;
        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IDS:
            print("IOCTL_MOXNOX_NETCTL_GET_NETIF_IDS");
            break;
        case IOCTL_MOXNOX_NETCTL_GET_NETIF_INFO:
            print("IOCTL_MOXNOX_NETCTL_GET_NETIF_INFO");
            break;
        case IOCTL_MOXNOX_NETCTL_GET_NETIF_BY_NAME:
            print("IOCTL_MOXNOX_NETCTL_GET_NETIF_BY_NAME");
            break;
        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IP_COUNT:
            print("IOCTL_MOXNOX_NETCTL_GET_NETIF_IP_COUNT");
            break;
        case IOCTL_MOXNOX_NETCTL_GET_NETIF_IPS:
            print("IOCTL_MOXNOX_NETCTL_GET_NETIF_IPS");
            break;
        case IOCTL_MOXNOX_SOCKET_BIND:
            print("IOCTL_MOXNOX_SOCKET_BIND");
            break;
        case IOCTL_MOXNOX_SOCKET_CONNECT:
            print("IOCTL_MOXNOX_SOCKET_CONNECT");
            break;
        case IOCTL_MOXNOX_SOCKET_LISTEN:
            print("IOCTL_MOXNOX_SOCKET_LISTEN");
            break;
        case IOCTL_MOXNOX_SOCKET_ACCEPT:
            print("IOCTL_MOXNOX_SOCKET_ACCEPT");
            break;
        case IOCTL_MOXNOX_EVDEV_GET_ABSINFO:
            print("IOCTL_MOXNOX_EVDEV_GET_ABSINFO");
            break;
        case IOCTL_MOXNOX_FB_SET_CURSOR:
            print("IOCTL_MOXNOX_FB_SET_CURSOR");
            break;
        case IOCTL_MOXNOX_FB_MOVE_CURSOR:
            print("IOCTL_MOXNOX_FB_MOVE_CURSOR");
            break;
        case IOCTL_MOXNOX_PTYCTL_CREATE_PTY:
            print("IOCTL_MOXNOX_PTYCTL_CREATE_PTY");
            break;
        case IOCTL_MOXNOX_PTY_ISATTY:
            print("IOCTL_MOXNOX_PTY_ISATTY");
            break;
        case IOCTL_MOXNOX_PTY_TCGETATTR:
            print("IOCTL_MOXNOX_PTY_TCGETATTR");
            break;
        case IOCTL_MOXNOX_PTY_TCSETATTR:
            print("IOCTL_MOXNOX_PTY_TCSETATTR");
            break;
        case IOCTL_MOXNOX_PTY_TCSETATTR_FLUSH:
            print("IOCTL_MOXNOX_PTY_TCSETATTR_FLUSH");
            break;
        case IOCTL_MOXNOX_PTY_TCFLUSH:
            print("IOCTL_MOXNOX_PTY_TCFLUSH");
            break;
        case IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP:
            print("IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP");
            break;
        case IOCTL_MOXNOX_PTY_GET_INFO_STREAM:
            print("IOCTL_MOXNOX_PTY_GET_INFO_STREAM");
            break;
        case IOCTL_MOXNOX_PTY_GET_SIZE:
            print("IOCTL_MOXNOX_PTY_GET_SIZE");
            break;
        case IOCTL_MOXNOX_PTY_SET_SIZE:
            print("IOCTL_MOXNOX_PTY_SET_SIZE");
            break;
        case IOCTL_MOXNOX_SOCKET_GETSOCKNAME:
            print("IOCTL_MOXNOX_SOCKET_GETSOCKNAME");
            break;
        case IOCTL_MOXNOX_SOCKET_GETPEERNAME:
            print("IOCTL_MOXNOX_SOCKET_GETPEERNAME");
            break;
        case IOCTL_MOXNOX_SOCKET_GETSOCKOPT:
            print("IOCTL_MOXNOX_SOCKET_GETSOCKOPT");
            break;
        case IOCTL_MOXNOX_SOCKET_SETSOCKOPT:
            print("IOCTL_MOXNOX_SOCKET_SETSOCKOPT");
            break;
        case IOCTL_MOXNOX_SOCKET_SHUTDOWN:
            print("IOCTL_MOXNOX_SOCKET_SHUTDOWN");
            break;
        case IOCTL_MOXNOX_PTY_GET_FOREGROUND_PROCESS_GROUP:
            print("IOCTL_MOXNOX_PTY_GET_FOREGROUND_PROCESS_GROUP");
            break;
        case IOCTL_MOXNOX_FB_CREATE_FRAMEBUFFER:
            print("IOCTL_MOXNOX_FB_CREATE_FRAMEBUFFER");
            break;
        case IOCTL_MOXNOX_FB_SET_ACTIVE:
            print("IOCTL_MOXNOX_FB_SET_ACTIVE");
            break;
        case IOCTL_MOXNOX_FILE_READV:
            print("IOCTL_MOXNOX_FILE_READV");
            break;
        case IOCTL_MOXNOX_FILE_WRITEV:
            print("IOCTL_MOXNOX_FILE_WRITEV");
            break;
        case IOCTL_MOXNOX_TRACECTL_ENABLE:
            print("IOCTL_MOXNOX_TRACECTL_ENABLE");
            break;
        case IOCTL_MOXNOX_TRACECTL_DISABLE:
            print("IOCTL_MOXNOX_TRACECTL_DISABLE");
            break;
        case IOCTL_MOXNOX_SOCKET_SENDMSG:
            print("IOCTL_MOXNOX_SOCKET_SENDMSG");
            break;
        case IOCTL_MOXNOX_SOCKET_RECVMSG:
            print("IOCTL_MOXNOX_SOCKET_RECVMSG");
            break;
        default:
            print("{:x}", code);
            break;
    }
}

extern "C"
void trace_NtDeviceIoControlFile(uintptr_t* args) {
    auto file_handle = (HANDLE)args[0];
    auto control_code = (uint32_t)args[1];
    auto input_buffer = (const void*)args[2];
    auto input_buffer_len = (uint32_t)args[3];
    auto output_buffer = (void*)args[4];
    auto output_buffer_len = (uint32_t)args[5];

    start_deferred_print();

    print("NtDeviceIoControlFile: file_handle = ");
    print_handle(file_handle);
    print(", control_code = ");
    print_ioctl_code(control_code);
    print(", input_buffer = {:x}, input_buffer_len = {}, output_buffer = {:x}, output_buffer_len = {}\n",
          (uintptr_t)input_buffer, input_buffer_len, (uintptr_t)output_buffer,
          output_buffer_len);

    end_deferred_print();
}

extern "C"
NTSTATUS NtDeviceIoControlFile(HANDLE file_handle, HANDLE /*event*/, PIO_APC_ROUTINE /*apc_routine*/, void* /*apc_context*/,
                               IO_STATUS_BLOCK* iosb, uint32_t control_code, const void* input_buffer,
                               uint32_t input_buffer_len, void* output_buffer, uint32_t output_buffer_len) {
    TRACE6("NtDeviceIoControlFile", file_handle, control_code, input_buffer,
           input_buffer_len, output_buffer, output_buffer_len);

    if (!file_handle)
        return nt_status::INVALID_PARAMETER;

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    return object_ioctl(objrw.get(), control_code, span((uint8_t*)input_buffer, input_buffer_len),
                        span((uint8_t*)output_buffer, output_buffer_len),
                        iosb->Information);
}

extern "C"
NTSTATUS user_NtDeviceIoControlFile(HANDLE file_handle, HANDLE event,
                                    PIO_APC_ROUTINE apc_routine, void* apc_context,
                                    IO_STATUS_BLOCK* user_iosb, uint32_t control_code,
                                    const void* user_input_buffer, uint32_t input_buffer_len,
                                    void* user_output_buffer, uint32_t output_buffer_len) {
    IO_STATUS_BLOCK iosb;
    void* input_buffer = nullptr;
    void* output_buffer = nullptr;

    if (!user_iosb)
        return nt_status::INVALID_PARAMETER;

    if (user_input_buffer && input_buffer_len > 0) {
        input_buffer = malloc(input_buffer_len);
        if (!input_buffer)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (!copy_from_process(input_buffer, user_input_buffer, input_buffer_len))
            return nt_status::ACCESS_VIOLATION;
    }

    if (user_output_buffer && output_buffer_len > 0) {
        output_buffer = malloc(output_buffer_len);
        if (!output_buffer) {
            if (input_buffer)
                free(input_buffer);

            return nt_status::INSUFFICIENT_RESOURCES;
        }
    }

    auto ret = NtDeviceIoControlFile(file_handle, event, apc_routine, apc_context,
                                     &iosb, control_code, input_buffer, input_buffer_len,
                                     output_buffer, output_buffer_len);

    if (input_buffer)
        free(input_buffer);

    if (output_buffer) {
        if (!copy_to_process(user_output_buffer, output_buffer, output_buffer_len)) {
            free(output_buffer);
            return nt_status::ACCESS_VIOLATION;
        }

        free(output_buffer);
    }

    if (!copy_to_process(user_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
        return nt_status::ACCESS_VIOLATION;

    return ret;
}

extern "C"
NTSTATUS user_NtSetInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* us_iosb,
                                   void* us_buffer, uint32_t buffer_size,
                                   FILE_INFORMATION_CLASS file_information_class) {
    IO_STATUS_BLOCK iosb;

    if (!us_iosb || !us_buffer || buffer_size == 0)
        return nt_status::INVALID_PARAMETER;

    buffer_size = min(buffer_size, (uint32_t)PAGE_SIZE);

    auto buffer = malloc(buffer_size);
    if (!buffer)
        return nt_status::INSUFFICIENT_RESOURCES;

    if (!copy_from_process(buffer, us_buffer, buffer_size)) {
        free(buffer);
        return nt_status::ACCESS_VIOLATION;
    }

    auto ret = NtSetInformationFile(file_handle, &iosb, buffer, buffer_size,
                                    file_information_class);

    free(buffer);

    if (success(ret)) {
        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtQueryInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* us_iosb,
                                     void* us_buffer, uint32_t buffer_size,
                                     FILE_INFORMATION_CLASS file_information_class) {
    IO_STATUS_BLOCK iosb;

    if (!us_iosb || !us_buffer || buffer_size == 0)
        return nt_status::INVALID_PARAMETER;

    buffer_size = min(buffer_size, (uint32_t)PAGE_SIZE);

    auto buffer = malloc(buffer_size);
    if (!buffer)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto ret = NtQueryInformationFile(file_handle, &iosb, buffer, buffer_size,
                                      file_information_class);

    if (success(ret) || ret == nt_status::BUFFER_OVERFLOW) {
        if (!copy_to_process(us_buffer, buffer, iosb.Information))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    free(buffer);

    return ret;
}

extern "C"
NTSTATUS user_NtFlushVirtualMemory(HANDLE proc_handle, void** base_address,
                                   size_t* region_size, IO_STATUS_BLOCK* iosb) {
    if (!region_size || !base_address || !iosb)
        return nt_status::INVALID_PARAMETER;

    void* addr;
    size_t size;
    IO_STATUS_BLOCK ks_iosb;

    if (!copy_from_process(&addr, base_address, sizeof(void*)))
        return nt_status::ACCESS_VIOLATION;

    if (!copy_from_process(&size, region_size, sizeof(size_t)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtFlushVirtualMemory(proc_handle, &addr, &size, &ks_iosb);

    if (success(ret)) {
        if (!copy_to_process(base_address, &addr, sizeof(void*)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(region_size, &size, sizeof(size_t)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(iosb, &ks_iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtOpenSection(HANDLE* section_handle, ACCESS_MASK desired_access,
                            const OBJECT_ATTRIBUTES* object_attributes) {
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;

    if (object_attributes) {
        auto ret = copy_object_attributes(oa, us, object_attributes);
        if (!success(ret))
            return ret;
    }

    auto ret = NtOpenSection(&h, desired_access, object_attributes ? &oa : nullptr);

    if (object_attributes)
        free_object_attributes(oa);

    if (success(ret)) {
        if (!copy_to_process(section_handle, &h, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtCreateNamedPipeFile(HANDLE* us_file_handle, ACCESS_MASK desired_access,
                                    const OBJECT_ATTRIBUTES* object_attributes,
                                    IO_STATUS_BLOCK* us_iosb, uint32_t share_access,
                                    uint32_t create_disposition, uint32_t create_options,
                                    uint32_t named_pipe_type, uint32_t read_mode,
                                    uint32_t completion_mode, uint32_t maximum_instances,
                                    uint32_t inbound_quota, uint32_t outbound_quota,
                                    LARGE_INTEGER* us_default_timeout) {
    LARGE_INTEGER default_timeout;
    HANDLE file_handle;
    IO_STATUS_BLOCK iosb;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    NTSTATUS ret;

    if (!us_file_handle || !us_iosb || !object_attributes)
        return nt_status::INVALID_PARAMETER;

    if (us_default_timeout) {
        if (!copy_from_process(&default_timeout, us_default_timeout, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    ret = copy_object_attributes(oa, us, object_attributes);
    if (!success(ret))
        return ret;

    ret = NtCreateNamedPipeFile(&file_handle, desired_access, &oa,
                                &iosb, share_access, create_disposition,
                                create_options, named_pipe_type, read_mode,
                                completion_mode, maximum_instances,
                                inbound_quota, outbound_quota,
                                us_default_timeout ? &default_timeout : nullptr);

    if (success(ret)) {
        if (!copy_to_process(us_file_handle, &file_handle, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    free_object_attributes(oa);

    return ret;
}

extern "C"
NTSTATUS user_NtOpenFile(HANDLE* us_file_handle, ACCESS_MASK desired_access,
                         const OBJECT_ATTRIBUTES* object_attributes,
                         IO_STATUS_BLOCK* us_iosb, uint32_t share_access,
                         uint32_t open_options) {
    HANDLE file_handle;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    nt_status ret;

    if (!us_file_handle || !us_iosb || !object_attributes)
        return nt_status::INVALID_PARAMETER;

    ret = copy_object_attributes(oa, us, object_attributes);
    if (!success(ret))
        return ret;

    ret = NtOpenFile(&file_handle, desired_access, &oa, &iosb, share_access,
                     open_options);

    if (success(ret)) {
        if (!copy_to_process(us_file_handle, &file_handle, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;

        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    free_object_attributes(oa);

    return ret;
}

extern "C"
NTSTATUS user_NtGetNextThread(HANDLE process_handle, HANDLE thread_handle,
                              ACCESS_MASK desired_access,
                              uint32_t handle_attributes, uint32_t flags,
                              HANDLE* us_new_thread_handle) {
    HANDLE new_thread_handle;
    nt_status ret;

    if (!us_new_thread_handle)
        return nt_status::INVALID_PARAMETER;

    ret = NtGetNextThread(process_handle, thread_handle, desired_access,
                          handle_attributes, flags, &new_thread_handle);

    if (success(ret)) {
        if (!copy_to_process(us_new_thread_handle, &new_thread_handle, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtSuspendThread(HANDLE thread_handle,
                              uint32_t* us_previous_suspend_count) {
    nt_status ret;
    uint32_t count;

    ret = NtSuspendThread(thread_handle,
                          us_previous_suspend_count ? &count : nullptr);

    if (success(ret) && us_previous_suspend_count) {
        if (!copy_to_process(us_previous_suspend_count, &count, sizeof(uint32_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtGetContextThread(HANDLE thread_handle, CONTEXT* us_context) {
    nt_status ret;
    CONTEXT context; // FIXME - is this too big for stack?

    if (!us_context)
        return nt_status::INVALID_PARAMETER;

    ret = NtGetContextThread(thread_handle, &context);

    if (success(ret)) {
        if (!copy_to_process(us_context, &context, sizeof(CONTEXT)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtSetContextThread(HANDLE thread_handle, const CONTEXT* us_context) {
    CONTEXT context; // FIXME - is this too big for stack?

    if (!us_context)
        return nt_status::INVALID_PARAMETER;

    if (!copy_from_process(&context, us_context, sizeof(CONTEXT)))
        return nt_status::ACCESS_VIOLATION;

    return NtSetContextThread(thread_handle, &context);
}

extern "C"
NTSTATUS user_NtGetNextProcess(HANDLE process_handle, ACCESS_MASK desired_access,
                               ULONG handle_attributes, ULONG flags,
                               HANDLE* us_new_process_handle) {
    HANDLE new_process_handle;
    nt_status ret;

    if (!us_new_process_handle)
        return nt_status::INVALID_PARAMETER;

    ret = NtGetNextProcess(process_handle, desired_access, handle_attributes,
                           flags, &new_process_handle);

    if (success(ret)) {
        if (!copy_to_process(us_new_process_handle, &new_process_handle,
                             sizeof(HANDLE))) {
            ret = nt_status::ACCESS_VIOLATION;
        }
    }

    return ret;
}

extern "C"
NTSTATUS user_NtOpenProcess(HANDLE* us_process_handle, ACCESS_MASK desired_access,
                            const OBJECT_ATTRIBUTES* object_attributes,
                            const CLIENT_ID* us_client_id) {
    HANDLE process_handle;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    CLIENT_ID client_id;
    nt_status ret;

    if (!us_process_handle || !object_attributes)
        return nt_status::INVALID_PARAMETER;

    if (us_client_id) {
        if (!copy_from_process(&client_id, us_client_id, sizeof(CLIENT_ID)))
            return nt_status::ACCESS_VIOLATION;
    }

    ret = copy_object_attributes(oa, us, object_attributes);
    if (!success(ret))
        return ret;

    ret = NtOpenProcess(&process_handle, desired_access, &oa,
                        us_client_id ? &client_id : nullptr);

    if (success(ret)) {
        if (!copy_to_process(us_process_handle, &process_handle, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    free_object_attributes(oa);

    return ret;
}

extern "C"
NTSTATUS user_NtDelayExecution(bool alertable,
                               const LARGE_INTEGER* us_delay_interval) {
    LARGE_INTEGER delay_interval;

    if (us_delay_interval) {
        if (!copy_from_process(&delay_interval, us_delay_interval, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    return NtDelayExecution(alertable,
                            us_delay_interval ? &delay_interval : nullptr);
}

extern "C"
NTSTATUS user_NtCreateEvent(HANDLE* event_handle, ACCESS_MASK desired_access,
                            const OBJECT_ATTRIBUTES* object_attributes,
                            EVENT_TYPE event_type, bool initial_state) {
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    nt_status ret;

    if (!event_handle)
        return nt_status::INVALID_PARAMETER;

    if (object_attributes) {
        ret = copy_object_attributes(oa, us, object_attributes);
        if (!success(ret))
            return ret;
    }

    ret = NtCreateEvent(&h, desired_access, object_attributes ? &oa : nullptr,
                        event_type, initial_state);

    if (success(ret)) {
        if (!copy_to_process(event_handle, &h, sizeof(HANDLE)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    if (object_attributes)
        free_object_attributes(oa);

    return ret;
}

extern "C"
NTSTATUS user_NtSetEvent(HANDLE event_handle, int32_t* us_previous_state) {
    nt_status ret;
    int32_t previous_state;

    ret = NtSetEvent(event_handle,
                     us_previous_state ? &previous_state : nullptr);

    if (success(ret) && us_previous_state) {
        if (!copy_to_process(us_previous_state, &previous_state, sizeof(int32_t)))
            ret = nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtQueueApcThread(HANDLE thread_handle, PS_APC_ROUTINE apc_routine,
                               void* apc_arg1, void* apc_arg2, void* apc_arg3) {
    if ((uintptr_t)apc_routine & 0xffff000000000000)
        return nt_status::INVALID_PARAMETER;

    return NtQueueApcThread(thread_handle, apc_routine, apc_arg1, apc_arg2,
                            apc_arg3);
}

extern "C"
NTSTATUS user_NtContinue(const CONTEXT* thread_context, bool /*raise_alert*/) {
    auto& t = *get_current_thread();
    CONTEXT ctx;

    if (!copy_from_process(&ctx, thread_context, sizeof(CONTEXT)))
        return nt_status::ACCESS_VIOLATION;

#ifdef __x86_64__
    if (ctx.Rip & 0xffff800000000000)
        return nt_status::INVALID_PARAMETER;

    ctx.EFlags |= 0x200; // force interrupts on
    ctx.EFlags &= ~(uint32_t)0x3000; // force IOPL to 0
#elifdef __aarch64__
    if ((ctx.Cpsr & 0xf) != 0)
        return nt_status::INVALID_PARAMETER;
#endif

    memcpy(&t.syscall_context, &ctx, sizeof(CONTEXT));

    // raise_alert ignored, APCs always get run

    return nt_status::USER_APC; // means rax/x0 will get taken from syscall_context instead
}

extern "C"
NTSTATUS user_NtSetInformationThread(HANDLE thread_handle, THREAD_INFORMATION_CLASS info_class,
                                     const void* us_buffer, uint32_t buffer_size) {
    void* buffer = nullptr;

    buffer_size = min(buffer_size, (uint32_t)PAGE_SIZE);

    if (buffer_size > 0) {
        buffer = malloc(buffer_size);
        if (!buffer)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (!copy_from_process(buffer, us_buffer, buffer_size)) {
            free(buffer);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    auto ret = NtSetInformationThread(thread_handle, info_class, buffer,
                                      buffer_size);

    if (buffer)
        free(buffer);

    return ret;
}

extern "C"
NTSTATUS user_NtSetInformationProcess(HANDLE process_handle, PROCESS_INFORMATION_CLASS info_class,
                                      const void* us_buffer, uint32_t buffer_size) {
    void* buffer = nullptr;

    buffer_size = min(buffer_size, (uint32_t)PAGE_SIZE);

    if (buffer_size > 0) {
        buffer = malloc(buffer_size);
        if (!buffer)
            return nt_status::INSUFFICIENT_RESOURCES;

        if (!copy_from_process(buffer, us_buffer, buffer_size)) {
            free(buffer);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    auto ret = NtSetInformationProcess(process_handle, info_class, buffer,
                                       buffer_size);

    if (buffer)
        free(buffer);

    return ret;
}

extern "C"
nt_status user_NtQueryVirtualMemory(HANDLE process_handle, const void* base_address,
                                    MEMORY_INFORMATION_CLASS info_class,
                                    void* us_buffer, size_t buffer_size,
                                    size_t* us_return_length) {
    size_t return_length;

    if (!us_buffer || !us_return_length || buffer_size == 0 || buffer_size > PAGE_SIZE)
        return nt_status::INVALID_PARAMETER;

    auto buf = malloc(buffer_size);
    if (!buf)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto ret = NtQueryVirtualMemory(process_handle, base_address, info_class,
                                    buf, buffer_size, &return_length);

    if (success(ret)) {
        if (return_length > 0 && !copy_to_process(us_buffer, buf, return_length)) {
            free(buf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    free(buf);

    if (success(ret)) {
        if (!copy_to_process(us_return_length, &return_length, sizeof(size_t)))
            return nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
nt_status user_NtQueryObject(HANDLE handle, OBJECT_INFORMATION_CLASS info_class,
                             void* us_buffer, uint32_t buffer_size,
                             uint32_t* us_return_length) {
    uint32_t return_length;

    if (!us_buffer || !us_return_length || buffer_size == 0 || buffer_size > PAGE_SIZE)
        return nt_status::INVALID_PARAMETER;

    auto buf = malloc(buffer_size);
    if (!buf)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto ret = NtQueryObject(handle, info_class, buf, buffer_size,
                             &return_length);

    if (success(ret)) {
        if (info_class == OBJECT_INFORMATION_CLASS::NameInformation) {
            auto& oni = *(OBJECT_NAME_INFORMATION*)buf;

            if (oni.Name.Buffer)
                oni.Name.Buffer = (char16_t*)((uintptr_t)us_buffer + (uintptr_t)oni.Name.Buffer - (uintptr_t)buf);
        }

        if (return_length > 0 && !copy_to_process(us_buffer, buf, return_length)) {
            free(buf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    free(buf);

    if (success(ret)) {
        if (!copy_to_process(us_return_length, &return_length, sizeof(uint32_t)))
            return nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtFsControlFile(HANDLE file_handle, HANDLE event,
                              PIO_APC_ROUTINE apc_routine, void* apc_context,
                              IO_STATUS_BLOCK* us_iosb, uint32_t code,
                              void* us_input_buffer, uint32_t input_buffer_length,
                              void* us_output_buffer, uint32_t output_buffer_length) {
    IO_STATUS_BLOCK iosb;
    uint8_t* input_buffer;
    uint8_t* output_buffer;

    if (!us_iosb)
        return nt_status::INVALID_PARAMETER;

    if (us_input_buffer && input_buffer_length > 0) {
        if (input_buffer_length > PAGE_SIZE)
            return nt_status::INVALID_PARAMETER;

        input_buffer = (uint8_t*)malloc(input_buffer_length);
        if (!input_buffer) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        if (!copy_from_process(input_buffer, us_input_buffer, input_buffer_length)) {
            free(input_buffer);
            return nt_status::ACCESS_VIOLATION;
        }
    } else {
        input_buffer = nullptr;
        input_buffer_length = 0;
    }

    if (us_output_buffer && output_buffer_length > 0) {
        if (output_buffer_length > PAGE_SIZE) {
            if (input_buffer)
                free(input_buffer);

            return nt_status::INVALID_PARAMETER;
        }

        output_buffer = (uint8_t*)malloc(output_buffer_length);
        if (!output_buffer) {
            print("malloc failed\n");

            if (input_buffer)
                free(input_buffer);

            return nt_status::INSUFFICIENT_RESOURCES;
        }
    } else {
        output_buffer = nullptr;
        output_buffer_length = 0;
    }

    auto ret = NtFsControlFile(file_handle, event, apc_routine, apc_context,
                               &iosb, code, input_buffer, input_buffer_length,
                               output_buffer, output_buffer_length);

    if (input_buffer)
        free(input_buffer);

    if (success(ret)) {
        if (output_buffer) {
            if (!copy_to_process(us_output_buffer, output_buffer, iosb.Information)) {
                free(output_buffer);
                return nt_status::ACCESS_VIOLATION;
            }

            free(output_buffer);
        }

        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            return nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtQueryVolumeInformationFile(HANDLE file_handle,
                                           IO_STATUS_BLOCK* us_iosb,
                                           void* us_fs_information, uint32_t length,
                                           FS_INFORMATION_CLASS fs_information_class) {
    IO_STATUS_BLOCK iosb;

    if (!us_iosb || !us_fs_information || length == 0)
        return nt_status::INVALID_PARAMETER;

    if (length > PAGE_SIZE)
        length = PAGE_SIZE;

    auto buf = malloc(length);
    if (!buf) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto ret = NtQueryVolumeInformationFile(file_handle, &iosb, buf, length,
                                            fs_information_class);
    if (success(ret)) {
        if (!copy_to_process(us_fs_information, buf, iosb.Information)) {
            free(buf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    free(buf);

    if (success(ret)) {
        if (!copy_to_process(us_iosb, &iosb, sizeof(IO_STATUS_BLOCK)))
            return nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS info_class,
                                       void* us_info, uint32_t info_length,
                                       uint32_t* us_return_length) {
    void* buf;
    uint32_t retlen;

    if (!us_info && info_length > 0)
        return nt_status::ACCESS_VIOLATION;

    if (info_length > PAGE_SIZE)
        info_length = PAGE_SIZE;

    if (info_length > 0) {
        buf = malloc(info_length);
        if (!buf) {
            print("malloc failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }
    } else
        buf = nullptr;

    auto ret = NtQuerySystemInformation(info_class, buf, info_length, &retlen);

    if (success(ret) && buf) {
        if (!copy_to_process(us_info, buf, retlen)) {
            free(buf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    if (buf)
        free(buf);

    if (success(ret) && us_return_length) {
        if (!copy_to_process(us_return_length, &retlen, sizeof(retlen)))
            return nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
NTSTATUS user_NtYieldExecution() {
    return NtYieldExecution();
}

extern "C"
nt_status user_NtQueryTimer(HANDLE handle, TIMER_INFORMATION_CLASS info_class,
                            void* us_buffer, uint32_t buffer_length,
                            uint32_t* us_return_length) {
    uint32_t return_length;

    if (!us_buffer || buffer_length == 0 || buffer_length > PAGE_SIZE)
        return nt_status::INVALID_PARAMETER;

    auto buf = malloc(buffer_length);
    if (!buf)
        return nt_status::INSUFFICIENT_RESOURCES;

    auto ret = NtQueryTimer(handle, info_class, buf, buffer_length,
                            &return_length);

    if (success(ret)) {
        if (return_length > 0 && !copy_to_process(us_buffer, buf, return_length)) {
            free(buf);
            return nt_status::ACCESS_VIOLATION;
        }
    }

    free(buf);

    if (success(ret)) {
        if (us_return_length && !copy_to_process(us_return_length, &return_length, sizeof(uint32_t)))
            return nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
nt_status user_NtSetSystemTime(const LARGE_INTEGER* us_system_time,
                               LARGE_INTEGER* us_previous_time) {
    LARGE_INTEGER system_time, previous_time;

    if (!us_system_time)
        return nt_status::INVALID_PARAMETER;

    if (!copy_from_process(&system_time, us_system_time, sizeof(LARGE_INTEGER)))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtSetSystemTime(&system_time, us_previous_time ? &previous_time : nullptr);

    if (success(ret) && us_previous_time) {
        if (!copy_to_process(us_previous_time, &previous_time, sizeof(LARGE_INTEGER)))
            return nt_status::ACCESS_VIOLATION;
    }

    return ret;
}

extern "C"
uint64_t maybe_yield_after_syscall(uint64_t syscall_ret) {
    // interrupts are disabled and sync_lock held

#ifdef DEBUG
    if (!interrupts_disabled())
        bsod("calling maybe_yield_after_syscall with interrupts enabled");
#endif

    auto& cb = *get_cpu_block();
    auto& t = *cb.current_thread;

    if (t.status != thread_status::dying && t.suspend_count == 0) {
        bool has_apcs;

        {
            lock_guard lock(t.apc_lock);
            has_apcs = !is_list_empty(t.pending_apcs);
        }

        if (has_apcs) {
            nt_status ret;

            release_sync_lock();
            enable_interrupts();

            ret = setup_user_apc(syscall_ret);

            disable_interrupts();
            acquire_sync_lock();

            return (uint64_t)ret;
        }

        return syscall_ret;
    }

    memcpy(&cb.thread_context, &t.syscall_context, sizeof(CONTEXT));

#ifdef __x86_64__
    memcpy(&cb.xsave, &t.xsave, XSAVE_SIZE);
    cb.thread_context.Rax = syscall_ret;
#elifdef __aarch64__
    cb.thread_context.X0 = syscall_ret;
#endif

    if (t.status == thread_status::dying) {
        bool terminate_proc = false;

        if (t.proc) {
            lock_guard lock(t.proc->threads_lock);

            if (!t.proc->dying) {
                remove_entry_list(t.process_le);
                terminate_proc = is_list_empty(t.proc->threads);
            }
        }

        release_sync_lock();
        enable_interrupts();

        if (terminate_proc) {
            t.proc->exit_status = t.exit_status;
            t.proc->terminate();
        } else {
            size_t region_size = 0;

            t.proc->free_vm((void**)&t.teb, &region_size, MEM_RELEASE);
            // FIXME - free userspace stack
        }

        disable_interrupts();
        acquire_sync_lock();

        thread_died(t, nullptr, false);
    }

    t.kernel_mode = false;

#ifdef __aarch64__
    __asm __volatile ( // doesn't return, releases sync_lock
        "add x0, x18, #%0\n\t"
        "ldr x0, [x0]\n\t"
        "mov sp, x0\n\t"
        "b scheduler"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#elifdef __x86_64__
    __asm __volatile ( // doesn't return, releases sync_lock
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#endif

    unreachable();
}

extern "C"
void maybe_yield_after_interrupt() {
    // interrupts are disabled and sync_lock held

#ifdef DEBUG
    if (!interrupts_disabled())
        bsod("calling maybe_yield_after_interrupt with interrupts enabled");
#endif

    auto& cb = *get_cpu_block();
    auto& t = *cb.current_thread;

    if (t.status != thread_status::dying && t.suspend_count == 0)
        return;

    if (t.status == thread_status::dying) {
        bool terminate_proc = false;

        if (t.proc) {
            lock_guard lock(t.proc->threads_lock);

            if (!t.proc->dying) {
                remove_entry_list(t.process_le);
                terminate_proc = is_list_empty(t.proc->threads);
            }
        }

        release_sync_lock();
        enable_interrupts();

        if (terminate_proc) {
            t.proc->exit_status = t.exit_status;
            t.proc->terminate();
        } else {
            size_t region_size = 0;

            t.proc->free_vm((void**)&t.teb, &region_size, MEM_RELEASE);
            // FIXME - free userspace stack
        }

        disable_interrupts();
        acquire_sync_lock();

        thread_died(t, nullptr, false);
    }

    t.kernel_mode = false;

#ifdef __aarch64__
    __asm __volatile ( // doesn't return, releases sync_lock
        "add x0, x18, #%0\n\t"
        "ldr x0, [x0]\n\t"
        "mov sp, x0\n\t"
        "b scheduler"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#elifdef __x86_64__
    __asm __volatile ( // doesn't return, releases sync_lock
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
#endif
}
