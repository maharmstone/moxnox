// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <stdint.h>

module fifo;

import nt;
import print;
import pipe;

using namespace std;

fifo_object::fifo_object() {
    auto pp = new pipe(1);
    if (!pp) {
        print("new failed\n");
        return;
    }

    p = object_ref{*pp};
    init = true;
}

nt_status fifo_object::read(span<const span<uint8_t>> vec, uint64_t& read_bytes) {
    auto& p2 = static_cast<pipe&>(p.get());

    return p2.read(vec, read_bytes);
}

nt_status fifo_object::write(span<const span<const uint8_t>> vec,
                             uint64_t& written_bytes) {
    auto& p2 = static_cast<pipe&>(p.get());

    return p2.write(vec, written_bytes);
}
