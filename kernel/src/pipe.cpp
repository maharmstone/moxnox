// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <string_view>
#include <stdint.h>

export module pipe;

import nt;
import list;
import chardev;
import event;
import spinlock;

using namespace std;

export class pipe : public chardev {
public:
    pipe(unsigned int buffer_pages);
    virtual ~pipe();

    NTSTATUS read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;
    NTSTATUS write(span<const span<const uint8_t>> v,
                   uint64_t& written_bytes) override;
    NTSTATUS write_nonblock(span<const span<const uint8_t>> v,
                            uint64_t& written_bytes);

    NTSTATUS write_nonblock(string_view sv, uint64_t& written_bytes) {
        span sp = span((const uint8_t*)sv.data(), sv.length());

        return write_nonblock(span(&sp, 1), written_bytes);
    }

    void clear() override;

    bool is_pipe() override {
        return true;
    }

    void set_eof_pending();

    uint8_t* buf;
    unsigned int buf_pages;
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;
    bool server_closed = false;
    spinlock lock;
    event not_full_event{false};
    list_entry flushable_le;
    bool eof_pending = false;
};

export void add_to_flushable_pipes_list(pipe& p);
export void shutdown_pipes();
