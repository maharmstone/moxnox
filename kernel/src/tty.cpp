// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <mutex>
#include <stdint.h>

export module tty;

import nt;
import print;
import chardev;
import handle;
import object_ref;
import thread;
import process;

using namespace std;

class tty_chardev : public chardev {
public:
    tty_chardev() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status read_nonblock(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status write_nonblock(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::INVALID_PARAMETER;
    }

    nt_status userspace_open(object_ref& objref) override;
};

export bool create_dev_tty() {
    auto pptr = new tty_chardev;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "tty");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

nt_status tty_chardev::userspace_open(object_ref& objref) {
    auto& t = *get_current_thread();
    object* terminal;

    {
        lock_guard lock(t.proc->pg_lock);
        lock_guard lock2(t.proc->pg->sess.lock);

        if (!t.proc->pg->sess.terminal)
            return nt_status::INVALID_PARAMETER;

        terminal = &t.proc->pg->sess.terminal.get();
        terminal->acquire();
    }

    object_ref r{*terminal};

    objref.swap(r);

    return nt_status::SUCCESS;
}
