// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <optional>
#include <functional>
#include <string_view>
#include <variant>
#include <utility>
#include <stdint.h>

module visit;

import nt;
import event;
import pipe;
import volume;
import semaphore;
import chardev;
import timer;
import thread;
import process;
import section;
import objects;
import vfs;
import net;
import fifo;

using namespace std;

// FIXME - concept to make sure func is invocable(?)
template<typename T>
static void visit_obj(object& obj, const T& func) {
    switch (obj.type) {
        case object_type::thread:
            func(static_cast<thread&>(obj));
        break;

        case object_type::timer:
            func(static_cast<timer&>(obj));
        break;

        case object_type::event:
            func(static_cast<event&>(obj));
        break;

        case object_type::chardev:
            func(static_cast<chardev&>(obj));
        break;

        case object_type::semaphore:
            func(static_cast<semaphore&>(obj));
        break;

        case object_type::volume:
            func(static_cast<volume&>(obj));
        break;

        case object_type::inode:
            func(static_cast<inode&>(obj));
        break;

        case object_type::process:
            func(static_cast<process&>(obj));
        break;

        case object_type::section:
            func(static_cast<section&>(obj));
        break;

        case object_type::file:
            func(static_cast<file&>(obj));
        break;

        case object_type::socket:
            func(static_cast<socket&>(obj));
        break;

        case object_type::fifo:
            func(static_cast<fifo_object&>(obj));
        break;

        default:
            unreachable();
    }
}

void object_release(object& obj) {
    visit_obj(obj, [](auto&& o) {
        o.release();
    });
}

bool object_is_sync_object(object& obj) {
    bool ret = false;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_sync_object<remove_reference_t<decltype(o)>>)
            ret = true;
        else if constexpr (has_get_sync_object<remove_reference_t<decltype(o)>>)
            ret = true;
        else if constexpr (has_get_rw_sync_object<remove_reference_t<decltype(o)>>)
            ret = true;
        else if constexpr (is_same_v<remove_reference_t<decltype(o)>, file>)
            ret = object_is_sync_object(o.ino);
    });

    return ret;
}

bool object_is_rw_sync_object(object& obj) {
    bool ret = false;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_rw_sync_object<remove_reference_t<decltype(o)>>)
            ret = true;
        else if constexpr (is_same_v<remove_reference_t<decltype(o)>, file>)
            ret = object_is_rw_sync_object(o.ino);
    });

    return ret;
}

void object_on_wake(object& obj) {
    visit_obj(obj, [&](auto&& o) {
        if constexpr (object_has_on_wake<remove_reference_t<decltype(o)>>)
            o.on_wake();
    });
}

nt_status object_write_without_offset(object& obj, span<const span<const uint8_t>> v,
                                      uint64_t& written) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>)
            status = o.write(v, written);
        else {
            status = nt_status::NOT_SUPPORTED;
            written = 0;
        }
    });

    return status;
}

nt_status object_write_with_offset(object& obj, span<const span<const uint8_t>> v,
                                   uint64_t offset, uint64_t& written_bytes) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_writable_object_with_offset<remove_reference_t<decltype(o)>>)
            status = o.write(v, offset, written_bytes);
        else
            status = nt_status::NOT_SUPPORTED;
    });

    return status;
}

nt_status object_ioctl(object& obj, uint32_t control_code, span<const uint8_t> inbuf,
                       span<uint8_t> outbuf, uintptr_t& written) {
    nt_status ret;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_ioctls<decltype(o)>)
            ret = o.ioctl(control_code, inbuf, outbuf, written);
        else
            ret = nt_status::NOT_SUPPORTED;
    });

    return ret;
}

nt_status object_read_with_offset(object& obj, span<const span<uint8_t>> v,
                                  uint64_t offset, uint64_t& read_bytes) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
            status = o.read(v, offset, read_bytes);
        else
            status = nt_status::NOT_SUPPORTED;
    });

    return status;
}

nt_status object_read_without_offset(object& obj, span<const span<uint8_t>> v,
                                     uint64_t& read_bytes) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>)
            status = o.read(v, read_bytes);
        else
            status = nt_status::NOT_SUPPORTED;
    });

    return status;
}

nt_status object_read_nonblock(object& obj, span<const span<uint8_t>> v,
                               uint64_t& read_bytes) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_nonblocking_readable_vec_object<remove_reference_t<decltype(o)>>)
            status = o.read_nonblock(v, read_bytes);
        else
            status = nt_status::NOT_SUPPORTED;
    });

    return status;
}

nt_status object_write_nonblock(object& obj, span<const span<const uint8_t>> v,
                                uint64_t& written_bytes) {
    nt_status status;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (is_nonblocking_writable_vec_object<remove_reference_t<decltype(o)>>)
            status = o.write_nonblock(v, written_bytes);
        else
            status = nt_status::NOT_SUPPORTED;
    });

    return status;
}

bool object_is_exclusive(object& obj) {
    bool ret;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_exclusive_field<remove_reference_t<decltype(o)>>)
            ret = o.exclusive;
        else
            ret = false;
    });

    return ret;
}

sync_object& object_get_sync_object(object& obj) {
    sync_object* so = nullptr;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_get_sync_object<remove_reference_t<decltype(o)>>)
            so = &o.get_sync_object();
        else if constexpr (has_get_rw_sync_object<remove_reference_t<decltype(o)>>)
            so = &o.get_rw_sync_object();
        else if constexpr (is_sync_object<remove_reference_t<decltype(o)>>)
            so = &static_cast<sync_object&>(o);
        else if constexpr (is_same_v<remove_reference_t<decltype(o)>, file>)
            so = &object_get_sync_object(o.ino);
    });

    return *so;
}

rw_sync_object& object_get_rw_sync_object(object& obj) {
    rw_sync_object* so = nullptr;

    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_get_rw_sync_object<remove_reference_t<decltype(o)>>)
            so = &o.get_rw_sync_object();
        else if constexpr (is_rw_sync_object<remove_reference_t<decltype(o)>>)
            so = &static_cast<rw_sync_object&>(o);
        else if constexpr (is_same_v<remove_reference_t<decltype(o)>, file>)
            so = &object_get_rw_sync_object(o.ino);
    });

    return *so;
}

void object_last_handle_closed(object& obj) {
    visit_obj(obj, [&](auto&& o) {
        if constexpr (has_last_handle_closed<remove_reference_t<decltype(o)>>)
            o.last_handle_closed();
    });
}

nt_status object_userspace_open(object_ref& objref) {
    nt_status ret = nt_status::SUCCESS;

    visit_obj(objref.get(), [&](auto&& o) {
        if constexpr (has_userspace_open<remove_reference_t<decltype(o)>>)
            ret = o.userspace_open(objref);
    });

    return ret;
}
