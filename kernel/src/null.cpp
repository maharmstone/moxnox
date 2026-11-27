// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <optional>
#include <functional>
#include <string_view>
#include <stdint.h>

export module null;

import nt;
import print;
import chardev;
import handle;
import object_ref;
import vec;

using namespace std;

class null_chardev : public chardev {
public:
    null_chardev() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;

    nt_status read_nonblock(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        return read(v, read_bytes);
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
};

export bool create_dev_null() {
    auto pptr = new null_chardev;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "null");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

nt_status null_chardev::read(span<const span<uint8_t>>, uint64_t& read_bytes) {
    read_bytes = 0;

    return nt_status::SUCCESS;
}

nt_status null_chardev::write(span<const span<const uint8_t>> v,
                              uint64_t& written_bytes) {
    written_bytes = get_vec_size(v);

    return nt_status::SUCCESS;
}
