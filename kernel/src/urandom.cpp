// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <algorithm>
#include <mutex>
#include <stdint.h>
#include <string.h>

export module urandom;

import nt;
import print;
import chardev;
import handle;
import object_ref;
import vec;
import kernel_time_page;
import config;
import chacha20;
import spinlock;
import event;
import scheduler;

using namespace std;

static spinlock urandom_lock;
static uint32_t urandom_key[8]; // FIXME - initialize from rdseed/rdrand if present (and not broken)?
static event urandom_event{false};
static size_t urandom_bits_left = 256;

class random_chardev : public chardev {
public:
    random_chardev() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;
    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
};

export bool create_dev_urandom() {
    auto pptr = new random_chardev;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "urandom");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

nt_status random_chardev::write(span<const span<const uint8_t>> v,
                                uint64_t& written_bytes) {
    written_bytes = get_vec_size(v);

    return nt_status::SUCCESS;
}

nt_status random_chardev::read(span<const span<uint8_t>> v,
                               uint64_t& read_bytes) {
    uint32_t in[16], out[16];
    nt_status status;
    auto len = get_vec_size(v);

    read_bytes = len;

    if (read_bytes == 0)
        return nt_status::SUCCESS;

    status = wait_for_object(urandom_event, true);
    if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED)
        return status;

    // see RFC 8439

    in[0] = 0x61707865;
    in[1] = 0x3320646e;
    in[2] = 0x79622d32;
    in[3] = 0x6b206574;

    span<uint8_t> top = v.front();

    while (len > 0) {
        lock_guard lock(urandom_lock);

        in[4] = urandom_key[0];
        in[5] = urandom_key[1];
        in[6] = urandom_key[2];
        in[7] = urandom_key[3];
        in[8] = urandom_key[4];
        in[9] = urandom_key[5];
        in[10] = urandom_key[6];
        in[11] = urandom_key[7];

        in[12] = 0; // block counter

        in[13] = 0; // nonce
        in[14] = 0;
        in[15] = 0;

        while (true) {
            chacha_block(out, in);

            memcpy_vec(v, top, (uint8_t*)out, min(len, sizeof(out)));

            in[12]++;

            if (len > sizeof(out))
                len -= sizeof(out);
            else {
                len = 0;
                break;
            }

            // don't hold the spinlock for more than 1MB at a time
            if (in[12] == 16384)
                break;
        }

        chacha_block(out, in);
        memcpy(urandom_key, out, sizeof(urandom_key));
    }

    // don't leave on stack
    for (size_t i = 0; i < 16; i++) {
        *(volatile uint32_t*)&in[i] = 0;
        *(volatile uint32_t*)&out[i] = 0;
    }

    return nt_status::SUCCESS;
}

export void add_randomness(span<const uint8_t> data) {
    uint32_t in[16], out[16];

    in[0] = 0x61707865;
    in[1] = 0x3320646e;
    in[2] = 0x79622d32;
    in[3] = 0x6b206574;

    in[12] = 0;
    in[13] = 0;
    in[14] = 0;
    in[15] = 0;

    bool do_set_event = false;

    {
        lock_guard lock(urandom_lock);

        while (!data.empty()) {
            auto n = min(data.size(), sizeof(urandom_key));

            in[4] = urandom_key[0];
            in[5] = urandom_key[1];
            in[6] = urandom_key[2];
            in[7] = urandom_key[3];
            in[8] = urandom_key[4];
            in[9] = urandom_key[5];
            in[10] = urandom_key[6];
            in[11] = urandom_key[7];

            // xor key with data
            for (size_t i = 0; i < n; i++) {
                *((uint8_t*)in + 16 + i) ^= data[i];
            }

            chacha_block(out, in);
            memcpy(urandom_key, out, sizeof(urandom_key));

            data = span(data.data() + n, data.size() - n);
        }

        if (urandom_bits_left > 0) {
            urandom_bits_left--;

            if (urandom_bits_left == 0)
                do_set_event = true;
        }
    }

    if (do_set_event)
        set_event(urandom_event);

    // don't leave on stack
    for (size_t i = 0; i < 16; i++) {
        *(volatile uint32_t*)&in[i] = 0;
        *(volatile uint32_t*)&out[i] = 0;
    }
}
