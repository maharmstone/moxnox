// SPDX-License-Identifier: GPL-2.0-only
module;

#include <optional>
#include <mutex>
#include <string_view>
#include <span>
#include <string.h>
#include "trace.h"

module handle;

import print;
import visit;
import mem;
import kernel_time_page;
import thread;
import unicode;
import trace;
import vfs;

using namespace std;

nt_status name_object(object& obj, string_view name, bool persistent) {
    if (name.empty() || name.size() > MAX_NAME_LEN)
        return nt_status::OBJECT_NAME_INVALID;

    auto ne = (name_entry*)malloc(offsetof(name_entry, name) + name.size());
    if (!ne) {
        print("Out of memory\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto cur_time = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

    optional<lock_guard<spinlock>> lock(name_lock);

    if (obj.name) {
        lock.reset();
        print("Object already has name\n");
        free(ne);
        return nt_status::INTERNAL_ERROR; // FIXME?
    }

    for (auto le = name_list.next; le != &name_list; le = le->next) {
        auto& ne2 = containing_record<name_entry>(le);

        if (ne2.name_len == name.length()) {
            auto ne2sv = string_view(ne2.name, ne2.name_len);

            if (ne2sv == name) {
                lock.reset();
                free(ne);
                return nt_status::OBJECT_NAME_COLLISION;
            }
        }
    }

    obj.name = ne;
    obj.persistent = persistent;
    ne->obj = &obj;

    if (is_list_empty(name_list))
        ne->index = 2;
    else
        ne->index = containing_record<name_entry>(name_list.prev).index + 1;

    ne->creation_time = cur_time;
    ne->name_len = name.length();
    memcpy(ne->name, name.data(), name.length());
    insert_tail_list(name_list, ne->le);

    obj.acquire();

    return nt_status::SUCCESS;
}

nt_status open_object(string_view name, object_ref& obj) {
    if (name.empty()) {
        obj = {};
        return nt_status::OBJECT_NAME_INVALID;
    }

    {
        lock_guard lock(name_lock);

        auto le = name_list.next;
        while (le != &name_list) {
            auto& ne = containing_record<name_entry>(le);

            if (ne.name_len == name.length()) {
                auto nesv = string_view(ne.name, ne.name_len);

                if (nesv == name) {
                    ne.obj->acquire();

                    obj = *ne.obj;
                    return nt_status::SUCCESS;
                }
            }

            le = le->next;
        }
    }

    obj = {};
    return nt_status::OBJECT_NAME_NOT_FOUND;
}

void remove_object_name(object& obj) {
    name_entry* ne;

    {
        lock_guard lock(name_lock);

        if (!obj.name)
            return;

        remove_entry_list(obj.name->le);

        ne = obj.name;
        obj.name = nullptr;
    }

    free(ne);
    object_release(obj);
}

void handle_closed(object& obj, object* proc) {
    auto new_handle_count = --obj.handle_count;

    if (proc)
        object_handle_closed(obj, *proc);

    // remove name if last handle closed

    if (new_handle_count == 0)
        object_last_handle_closed(obj);

    if (obj.name && !obj.persistent && new_handle_count == 0)
        remove_object_name(obj);
}

static nt_status query_object_name_info(object& obj, span<uint8_t> buffer,
                                        uint32_t& written) {
    static const u16string_view prefix = u"/dev/";

    // FIXME - files should be name of block device followed by path

    if (obj.type == object_type::file) {
        auto& f = static_cast<file&>(obj);

        return query_object_name_info(f.ino, buffer, written);
    }

    if (buffer.size() < sizeof(OBJECT_NAME_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& oni = *(OBJECT_NAME_INFORMATION*)buffer.data();

    {
        lock_guard lock(name_lock);

        // FIXME - is it right to return empty string and SUCCESS if no name?
        if (!obj.name) {
            oni.Name.Buffer = nullptr;
            oni.Name.Length = oni.Name.MaximumLength = 0;
            written = sizeof(OBJECT_NAME_INFORMATION);
            return nt_status::SUCCESS;
        }

        auto name = string_view(obj.name->name, obj.name->name_len);

        auto u16len = prefix.size() + utf8_to_utf16_len(name);

        if (buffer.size() < sizeof(OBJECT_NAME_INFORMATION) + (u16len * sizeof(char16_t)))
            return nt_status::BUFFER_TOO_SMALL;

        oni.Name.Length = oni.Name.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
        oni.Name.Buffer = (char16_t*)((uint8_t*)&oni + sizeof(OBJECT_NAME_INFORMATION));

        memcpy(oni.Name.Buffer, prefix.data(), prefix.size() * sizeof(char16_t));

        utf8_to_utf16(span(oni.Name.Buffer + prefix.size(), u16len - prefix.size()), name);

        written = (uint32_t)(sizeof(OBJECT_NAME_INFORMATION) + (u16len * sizeof(char16_t)));
    }

    return nt_status::SUCCESS;
}

static nt_status query_object_basic_info(ACCESS_MASK access_mask,
                                         span<uint8_t> buffer,
                                         uint32_t& return_length) {
    if (buffer.size() < sizeof(OBJECT_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& obi = *(OBJECT_BASIC_INFORMATION*)buffer.data();

    memset(&obi, 0, sizeof(OBJECT_BASIC_INFORMATION));

    obi.GrantedAccess = access_mask;

    // FIXME - Attributes
    // FIXME - HandleCount
    // FIXME - PointerCount
    // FIXME - PagedPoolCharge
    // FIXME - NonPagedPoolCharge
    // FIXME - NameInfoSize
    // FIXME - TypeInfoSize
    // FIXME - SecurityDescriptorSize
    // FIXME - CreationTime

    return_length = sizeof(OBJECT_BASIC_INFORMATION);

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtQueryObject(uintptr_t* args) {
    auto handle = (HANDLE)args[0];
    auto info_class = (OBJECT_INFORMATION_CLASS)args[1];
    auto buffer = (void*)args[2];
    auto buffer_size = (uint32_t)args[3];

    start_deferred_print();

    print("NtQueryObject: handle = ");
    print_handle(handle);
    print(", info_class = {}, buffer = {:x}, buffer_size = {}\n",
          info_class, (uintptr_t)buffer, buffer_size);

    end_deferred_print();
}

extern "C"
nt_status NtQueryObject(HANDLE handle, OBJECT_INFORMATION_CLASS info_class,
                        void* buffer, uint32_t buffer_size,
                        uint32_t* return_length) {
    ACCESS_MASK access_mask;

    TRACE4("NtQueryObject", handle, info_class, buffer, buffer_size);

    auto objrw = deref_handle(handle, access_mask);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    switch (info_class) {
        using enum OBJECT_INFORMATION_CLASS;

        case BasicInformation:
            return query_object_basic_info(access_mask,
                                           span((uint8_t*)buffer, buffer_size),
                                           *return_length);

        case NameInformation:
            return query_object_name_info(obj, span((uint8_t*)buffer, buffer_size),
                                          *return_length);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}
