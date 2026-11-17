// SPDX-License-Identifier: GPL-2.0-only
module;

#include <stdint.h>

export module rangelock;

import list;

export struct rangelock {
    list_entry active = { &active, &active };
    list_entry waiting = { &waiting, &waiting };
};

export enum class rangelock_mode {
    shared,
    exclusive
};

export struct rangelock_req {
    rangelock_req(rangelock& rl, uint64_t start, uint64_t length, rangelock_mode mode, bool kick = false) : rl(rl), start(start), length(length) {
        exclusive = mode == rangelock_mode::exclusive;
        this->lock(kick);
    }

    ~rangelock_req() {
        this->unlock();
    }

    void lock(bool kick);
    void unlock();

    list_entry le;
    rangelock& rl;
    uint64_t start;
    uint64_t length;
    bool exclusive;
    uintptr_t t; // thread*
};
