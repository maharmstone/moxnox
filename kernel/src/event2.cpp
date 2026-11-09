// SPDX-License-Identifier: GPL-2.0-only
module;

#include <mutex>
#include <string_view>
#include "trace.h"

module event;

import nt;
import print;
import list;
import thread;
import spinlock;
import handle;
import scheduler;
import trace;

using namespace std;

object_ref create_event(bool autoreset) {
    auto eptr = new event(autoreset);
    if (!eptr) {
        print("new failed\n");
        return {};
    }

    auto& e = *eptr;

    return e;
}

void event::set_holding_sync_lock() {
    // signal waiting threads

    if (autoreset) {
        if (is_list_empty(waiters))
            signalled = true; // if autoreset, only signal if nothing waiting when fired
        else {
            list_entry tmp = { &tmp, &tmp };

            auto le = waiters.next;

            remove_entry_list(*le);
            insert_tail_list(tmp, *le);
            wake_up_waiters(tmp, WAIT_TYPE_READING);
        }
    } else {
        wake_up_waiters(waiters, WAIT_TYPE_READING);

        signalled = true;
    }
}

void set_event(event& e) {
    sync_lock sl;
    lock_guard lock(sl);

    e.set_holding_sync_lock();
}

void reset_event(event& e) {
    sync_lock sl;
    lock_guard lock(sl);

    e.signalled = false;
}

void event::on_wake() {
    if (autoreset)
        signalled = false;
}

static void print_access_mask(ACCESS_MASK access_mask) {
    static constexpr pair<ACCESS_MASK, string_view> values[] = {
        { EVENT_ALL_ACCESS, "EVENT_ALL_ACCESS" },
        { EVENT_QUERY_STATE, "EVENT_QUERY_STATE" },
        { EVENT_MODIFY_STATE, "EVENT_MODIFY_STATE" },
        { DELETE, "DELETE" },
        { READ_CONTROL, "READ_CONTROL" },
        { WRITE_DAC, "WRITE_DAC" },
        { WRITE_OWNER, "WRITE_OWNER" },
        { SYNCHRONIZE, "SYNCHRONIZE" },
        { ACCESS_SYSTEM_SECURITY, "ACCESS_SYSTEM_SECURITY" },
        { MAXIMUM_ALLOWED, "MAXIMUM_ALLOWED" },
        { GENERIC_ALL, "GENERIC_ALL" },
        { GENERIC_EXECUTE, "GENERIC_EXECUTE" },
        { GENERIC_WRITE, "GENERIC_WRITE" },
        { GENERIC_READ, "GENERIC_READ" },
    };

    bool first = true;

    for (auto& v : values) {
        if ((access_mask & v.first) == v.first) {
            access_mask &= ~v.first;

            if (!first)
                print(" | ");

            print("{}", v.second);
            first = false;
        }
    }

    if (access_mask != 0) {
        if (!first)
            print(" | ");

        print("{:x}", access_mask);
    }

    if (first && access_mask == 0)
        print("0");
}

extern "C"
void trace_NtCreateEvent(uintptr_t* args) {
    auto desired_access = (ACCESS_MASK)args[0];
    auto event_type = (EVENT_TYPE)args[1];
    auto initial_state = (bool)args[2];

    start_deferred_print();

    print("NtCreateEvent: desired_access = ");
    print_access_mask(desired_access);
    print(", event_type = {}, initial_state = {}\n", event_type,
          initial_state ? "true" : "false");

    end_deferred_print();
}

extern "C"
NTSTATUS NtCreateEvent(HANDLE* event_handle, ACCESS_MASK desired_access,
                       const OBJECT_ATTRIBUTES* /*object_attributes*/,
                       EVENT_TYPE event_type, bool initial_state) {
    bool autoreset;
    auto& t = *get_current_thread();
    handle_t h;

    TRACE3("NtCreateEvent", desired_access, event_type, initial_state);

    // FIXME - object_attributes

    switch (event_type) {
        case EVENT_TYPE::SynchronizationEvent:
            autoreset = true;
            break;

        case EVENT_TYPE::NotificationEvent:
            autoreset = false;
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

    auto obj = create_event(autoreset);
    if (!obj)
        return nt_status::INTERNAL_ERROR;

    auto& e = static_cast<event&>(obj.get());
    e.signalled = initial_state;

    auto status = t.proc->add_handle(obj.get(), desired_access, true, h);
    if (!success(status))
        return status;

    *event_handle = h;

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtSetEvent(uintptr_t* args) {
    auto event_handle = (HANDLE)args[0];

    start_deferred_print();

    print("NtSetEvent: event_handle = ");
    print_handle(event_handle);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtSetEvent(HANDLE event_handle, int32_t* /*previous_state*/) {
    // FIXME - previous_state

    TRACE1("NtSetEvent", event_handle);

    auto objrw = deref_handle(event_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    // FIXME - check access mask

    auto& obj = objrw.get();

    if (obj.type != object_type::event)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& e = static_cast<event&>(obj);

    set_event(e);

    return nt_status::SUCCESS;
}
