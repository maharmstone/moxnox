// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <stdint.h>

export module visit;

import nt;
import objects;
import object_ref;

using namespace std;

export void object_release(object& obj);
export bool object_is_sync_object(object& obj);
export bool object_is_rw_sync_object(object& obj);
export void object_on_wake(object& obj);
export nt_status object_write_without_offset(object& obj,
                                             span<const span<const uint8_t>> v,
                                             uint64_t& written);
export nt_status object_write_with_offset(object& obj, span<const span<const uint8_t>> v,
                                              uint64_t offset, uint64_t& written_bytes);
export nt_status object_ioctl(object& obj, uint32_t control_code, span<const uint8_t> inbuf,
                              span<uint8_t> outbuf, uintptr_t& written);
export nt_status object_read_with_offset(object& obj, span<const span<uint8_t>> v,
                                             uint64_t offset, uint64_t& read_bytes);
export nt_status object_read_without_offset(object& obj, span<const span<uint8_t>> v,
                                                uint64_t& read_bytes);
export nt_status object_read_nonblock(object& obj, span<const span<uint8_t>> v,
                                      uint64_t& read_bytes);
export nt_status object_write_nonblock(object& obj, span<const span<const uint8_t>> v,
                                       uint64_t& written_bytes);
export bool object_is_exclusive(object& obj);
export sync_object& object_get_sync_object(object& obj);
export rw_sync_object& object_get_rw_sync_object(object& obj);
export void object_last_handle_closed(object& obj);
export void object_handle_closed(object& obj, object& proc);
export nt_status object_userspace_open(object_ref& objref);
