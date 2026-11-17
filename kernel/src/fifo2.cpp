// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <mutex>
#include <algorithm>
#include <stdint.h>

module fifo;

import nt;
import print;
import vec;
import scheduler;
import config;
import mem;

using namespace std;

fifo_object::fifo_object() : rw_sync_object(object_type::fifo, false, true) {
    not_full_event.signalled = true;

    auto va = vmalloc(BUF_PAGES << PAGE_SHIFT);
    if (!va) {
        print("vmalloc failed\n");
        return;
    }

    buf = (uint8_t*)va;

    init = true;
}

fifo_object::~fifo_object() {
    if (buf)
        vfree(buf, BUF_PAGES << PAGE_SHIFT);
}

template<bool NONBLOCK>
nt_status fifo_object::do_read(span<const span<uint8_t>> v, uint64_t& read_bytes) {
    size_t copied = 0;
    size_t vec_size = get_vec_size(v);

    if (vec_size == 0) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<uint8_t> top = v.front();

    acquire();
    object_ref obj{*this}; // so refcount gets decreased on exit

    while (true) {
        nt_status status;

        if constexpr (NONBLOCK) {
            status = wait_for_object(*this, true, 0);

            if (status == nt_status::TIMEOUT) {
                read_bytes = 0;

                {
                    lock_guard l(lock);

                    if (num_writers == 0)
                        return nt_status::SUCCESS;
                }

                return nt_status::CANT_WAIT;
            }
        } else
            status = wait_for_object(*this, true);

        if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED) {
            read_bytes = 0;
            return status;
        }

        {
            sync_lock sl;
            lock_guard l(sl);
            lock_guard l2(lock);

            if (buf_unread == 0) {
                if (num_writers == 0)
                    return nt_status::PIPE_DISCONNECTED;

                continue;
            }

            size_t buf_read_pos = buf_write_pos + (BUF_PAGES << PAGE_SHIFT) - buf_unread;

            while (buf_read_pos >= BUF_PAGES << PAGE_SHIFT) {
                buf_read_pos -= BUF_PAGES << PAGE_SHIFT;
            }

            size_t to_copy = min(vec_size, buf_unread);

            if (buf_read_pos + to_copy > BUF_PAGES << PAGE_SHIFT) {
                auto first_bit = (BUF_PAGES << PAGE_SHIFT) - buf_read_pos;

                memcpy_vec(v, top, &buf[buf_read_pos], first_bit);

                buf_read_pos = 0;
                buf_unread -= first_bit;
                to_copy -= first_bit;
                copied = first_bit;
            }

            memcpy_vec(v, top, &buf[buf_read_pos], to_copy);
            buf_unread -= to_copy;
            copied += to_copy;

            if (buf_unread == 0 && num_writers > 0)
                signalled = false;

            if (!not_full_event.signalled)
                not_full_event.set_holding_sync_lock();

            if (!write_signalled) {
                write_signalled = true;
                wake_up_waiters(waiters, WAIT_TYPE_WRITING);
            }
        }

        break;
    }

    read_bytes = copied;
    return nt_status::SUCCESS;
}

template nt_status fifo_object::do_read<true>(span<const span<uint8_t>> v,
                                              uint64_t& read_bytes);
template nt_status fifo_object::do_read<false>(span<const span<uint8_t>> v,
                                            uint64_t& read_bytes);

template<bool NONBLOCK>
nt_status fifo_object::do_write(span<const span<const uint8_t>> v,
                                uint64_t& written_bytes) {
    size_t vec_size = get_vec_size(v);

    if (vec_size == 0) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<const uint8_t> top = v.front();

    written_bytes = 0;

    do {
        nt_status status;

        {
            size_t to_copy;

            sync_lock sl;
            lock_guard l(sl);
            lock_guard l2(lock);

            if (num_readers == 0) {
                if (written_bytes > 0)
                    return nt_status::SUCCESS;

                return nt_status::PIPE_BROKEN;
            }

            size_t buf_left = (BUF_PAGES << PAGE_SHIFT) - buf_unread;

            to_copy = min(vec_size, buf_left);

            if (to_copy > 0) {
                if (buf_write_pos + to_copy > BUF_PAGES << PAGE_SHIFT) {
                    auto first_bit = (BUF_PAGES << PAGE_SHIFT) - buf_write_pos;

                    memcpy_vec(&buf[buf_write_pos], v, top, first_bit);

                    buf_write_pos = 0;
                    buf_unread += first_bit;
                    to_copy -= first_bit;
                    vec_size -= first_bit;
                    written_bytes += first_bit;
                }

                memcpy_vec(&buf[buf_write_pos], v, top, to_copy);

                buf_write_pos += to_copy;
                buf_unread += to_copy;
                vec_size -= to_copy;
                written_bytes += to_copy;

                // wake up waiting thread

                if (!signalled) {
                    signalled = true;

                    wake_up_waiters(waiters, WAIT_TYPE_READING);
                }

                if (buf_unread == BUF_PAGES << PAGE_SHIFT) {
                    not_full_event.signalled = false;
                    write_signalled = false;
                }
            }
        }

        if (vec_size == 0)
            break;

        // block while we wait for space to become available

        if constexpr (NONBLOCK) {
            status = wait_for_object(not_full_event, true, 0);

            if (status == nt_status::TIMEOUT) {
                if (written_bytes == 0)
                    return nt_status::CANT_WAIT;
                else
                    break;
            }
        } else
            status = wait_for_object(not_full_event, true);

        if (status == nt_status::USER_APC) {
            if (written_bytes == 0)
                return nt_status::USER_APC;
            else
                break;
        } else if (!success(status))
            return status;
    } while (true);

    return nt_status::SUCCESS;
}

template nt_status fifo_object::do_write<true>(span<const span<const uint8_t>> v,
                                               uint64_t& written_bytes);
template nt_status fifo_object::do_write<false>(span<const span<const uint8_t>> v,
                                                uint64_t& written_bytes);
