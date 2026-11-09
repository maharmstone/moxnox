// SPDX-License-Identifier: GPL-2.0-only
module;

#include <atomic>
#include <span>

export module fifo;

import nt;
import object_ref;
import spinlock;
import event;
import list;
import objects;

using namespace std;

export class fifo_object : public rw_sync_object {
public:
    fifo_object();
    virtual ~fifo_object();

    void acquire() {
        refcount++;
    }

    void release() {
        if (refcount.fetch_sub(1) == 1)
            delete this;
    }

    static const size_t BUF_PAGES = 1;

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes);
    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes);

    bool init = false;
    spinlock lock;
    unsigned int num_readers = 0;
    unsigned int num_writers = 0;
    event have_readers{false};
    event have_writers{false};
    uint8_t* buf = nullptr;
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;
    event not_full_event{false};
};

static_assert(is_rw_sync_object<fifo_object>);
static_assert(is_readable_object_without_offset<fifo_object>);
static_assert(is_writable_object_without_offset<fifo_object>);
