// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <string_view>
#include <stdint.h>

export module chardev;

import nt;
import objects;
import object_ref;

using namespace std;

export class chardev : public rw_sync_object {
public:
    chardev() : rw_sync_object(object_type::chardev, true, true) { }
    virtual ~chardev() { }

    virtual NTSTATUS read(span<const span<uint8_t>> v, uint64_t& read_bytes) = 0;
    virtual NTSTATUS read_nonblock(span<const span<uint8_t>> v,
                                   uint64_t& read_bytes) = 0;
    virtual NTSTATUS write(span<const span<const uint8_t>> v,
                           uint64_t& written_bytes) = 0;
    virtual NTSTATUS write_nonblock(span<const span<const uint8_t>> v,
                                    uint64_t& written_bytes) = 0;

    NTSTATUS write(string_view sv, uint64_t& written_bytes) {
        auto sp = span((const uint8_t*)sv.data(), sv.length());

        return write(span(&sp, 1), written_bytes);
    }

    virtual nt_status ioctl(uint32_t, span<const uint8_t>, span<uint8_t>, uintptr_t&, ACCESS_MASK) {
        return nt_status::NOT_SUPPORTED;
    }

    virtual nt_status flush(bool go_readonly) {
        if (go_readonly)
            readonly = true;

        return nt_status::SUCCESS;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    virtual void clear() {
    }

    virtual sync_object& get_sync_object() {
        return *this;
    }

    virtual void last_handle_closed() {
    }

    virtual nt_status userspace_open(object_ref&) {
        return nt_status::SUCCESS;
    }

    virtual bool is_pipe() {
        return false;
    }

    bool exclusive = true;
    bool readonly = false;
};

static_assert(is_rw_sync_object<chardev>);
static_assert(is_readable_object_without_offset<chardev>);
static_assert(is_nonblocking_readable_object<chardev>);
static_assert(is_nonblocking_writable_object<chardev>);
static_assert(is_writable_object_without_offset<chardev>);
static_assert(has_ioctls<chardev>);
static_assert(has_exclusive_field<chardev>);
static_assert(has_get_sync_object<chardev>);
static_assert(has_last_handle_closed<chardev>);
static_assert(has_userspace_open<chardev>);
