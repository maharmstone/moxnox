// SPDX-License-Identifier: GPL-2.0-only
module;

#include <atomic>
#include <span>

export module fifo;

import nt;
import object_ref;
import spinlock;
import event;

using namespace std;

export class fifo_object {
public:
    fifo_object();

    void acquire() {
        refcount++;
    }

    void release() {
        if (refcount.fetch_sub(1) == 1)
            delete this;
    }

    nt_status read(span<const span<uint8_t>> vec, uint64_t& read_bytes);
    nt_status write(span<const span<const uint8_t>> vec,
                    uint64_t& written_bytes);

    atomic<unsigned int> refcount = 1;
    object_ref p;
    bool init = false;
    spinlock lock;
    unsigned int num_readers = 0;
    unsigned int num_writers = 0;
    event have_readers{false};
    event have_writers{false};
};
