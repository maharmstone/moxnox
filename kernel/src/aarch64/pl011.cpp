// SPDX-License-Identifier: GPL-2.0-only
#include <stddef.h>
#include <stdint.h>
#include <utility>
#include <optional>
#include <span>
#include <string_view>
#include <atomic>
#include <charconv>
#include <mutex>

import nt;
import module_init;
import acpi;
import print;
import pipe;
import object_ref;
import format;
import mem;
import interrupts;
import config;
import chardev;
import scheduler;
import spinlock;
import console;
import handle;

using namespace std;

static void found_pl011(const acpi_device& d);

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "ARMH0011", found_pl011, nullptr },
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_keyboard(init_acpi_hids);

static const unsigned int serial_buffer_pages = 1;
static atomic<unsigned int> pl011_number = 0;

static const uint32_t PL011_UARTCR_UARTEN = 1 << 0;
static const uint32_t PL011_UARTCR_TXE = 1 << 8;
static const uint32_t PL011_UARTCR_RXE = 1 << 9;

static const uint32_t PL011_UARTFR_BUSY = 1 << 3;

static const uint32_t PL011_UARTLCR_H_FEN = 1 << 4;
static const unsigned int PL011_UARTLCR_H_WLEN_OFFSET = 5;
static const uint32_t PL011_UARTLCR_H_WLEN_MASK = 3 << PL011_UARTLCR_H_WLEN_OFFSET;

static const uint32_t PL011_UARTMIS_TXMIS = 1 << 5;
static const uint32_t PL011_UARTIMSC_TXIM = 1 << 5;
static const uint32_t PL011_UARTICR_TXIC = 1 << 5;

static const uint32_t PL011_UARTMIS_RXMIS = 1 << 4;
static const uint32_t PL011_UARTIMSC_RXIM = 1 << 4;
static const uint32_t PL011_UARTICR_RXIC = 1 << 4;

class pl011_out_pipe : public pipe {
public:
    pl011_out_pipe(unsigned int buffer_pages) : pipe(buffer_pages) { }
    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
    nt_status flush(bool go_readonly) override;

    bool ready = true;
    uint64_t base_address;
};

class pl011_pipe : public chardev {
public:
    pl011_pipe(pipe& recv_pipe, pl011_out_pipe& send_pipe,
               uint64_t base_address, uint64_t base_clock,
               unsigned int baud_rate);

    ~pl011_pipe() {
        recv_pipe.release();
        send_pipe.release();
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        return recv_pipe.read(v, read_bytes);
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override {
        return send_pipe.write(v, written_bytes);
    }

    sync_object& get_sync_object() override {
        return recv_pipe;
    }

    void irq();

    pipe& recv_pipe;
    pl011_out_pipe& send_pipe;
    bool init = false;
    uint64_t base_address;
};

pl011_pipe::pl011_pipe(pipe& recv_pipe, pl011_out_pipe& send_pipe, uint64_t pa,
                       uint64_t base_clock, unsigned int baud_rate) : recv_pipe(recv_pipe), send_pipe(send_pipe) {
    recv_pipe.acquire();
    send_pipe.acquire();

    auto va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        return;
    }

    if (!map_page(pa, va, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(va, 1);
        return;
    }

    base_address = va;

    auto& UARTFR = *(volatile uint32_t*)(base_address + 0x18);
    auto& UARTIBRD = *(volatile uint32_t*)(base_address + 0x24);
    auto& UARTFBRD = *(volatile uint32_t*)(base_address + 0x28);
    auto& UARTLCR_H = *(volatile uint32_t*)(base_address + 0x2c);
    auto& UARTCR = *(volatile uint32_t*)(base_address + 0x30);
    auto& UARTIMSC = *(volatile uint32_t*)(base_address + 0x38);
    auto& UARTICR = *(volatile uint32_t*)(base_address + 0x44);
    auto& UARTDMACR = *(volatile uint32_t*)(base_address + 0x48);

    // disable UART
    UARTCR &= ~PL011_UARTCR_UARTEN;

    while (UARTFR & PL011_UARTFR_BUSY) { }

    // disable FIFOs
    UARTLCR_H &= ~PL011_UARTLCR_H_FEN;

    auto div = 4 * base_clock / baud_rate;

    UARTIBRD = (div >> 6) & 0xffff;
    UARTFBRD = div & 0x3f;

    // set word length to 8 bits
    UARTLCR_H &= ~(PL011_UARTLCR_H_WLEN_MASK);
    UARTLCR_H |= 3 << PL011_UARTLCR_H_WLEN_OFFSET;

    // mask interrupts
    UARTIMSC = PL011_UARTIMSC_RXIM | PL011_UARTIMSC_TXIM;

    // clear pending interrupts
    UARTICR = 0x3ff;

    // disable DMA
    UARTDMACR = 0;

    // enable receiving, transmission, and UART
    UARTCR |= PL011_UARTCR_RXE | PL011_UARTCR_TXE | PL011_UARTCR_UARTEN;

    init = true;
}

nt_status pl011_out_pipe::write(span<const span<const uint8_t>> v,
                                uint64_t& written_bytes) {
    written_bytes = 0;

    auto& UARTDR = *(volatile uint32_t*)base_address;

    for (auto sv : v) {
        auto s = sv;

        while (!s.empty()) {
            size_t to_copy;

            {
                lock_guard l(lock);

                if (ready) {
                    // send first character

                    UARTDR = (uint32_t)s[0];

                    ready = false;
                    s = s.subspan(1);
                    written_bytes++;

                    if (s.empty())
                        break;
                }

                size_t buf_left = (buf_pages << PAGE_SHIFT) - buf_unread;

                to_copy = min(s.size(), buf_left);

                if (to_copy > 0) {
                    if (buf_write_pos + to_copy > buf_pages << PAGE_SHIFT) {
                        auto first_bit = (buf_pages << PAGE_SHIFT) - buf_write_pos;

                        memcpy(&buf[buf_write_pos], s.data(), first_bit);

                        buf_write_pos = 0;
                        buf_unread += first_bit;
                        to_copy -= first_bit;
                        s = s.subspan(first_bit);
                        written_bytes += first_bit;
                    }

                    memcpy(&buf[buf_write_pos], s.data(), to_copy);
                    buf_write_pos += to_copy;
                    buf_unread += to_copy;
                    s = s.subspan(to_copy);
                    written_bytes += to_copy;
                }
            }

            if (s.empty())
                break;

            // block while we wait for space to become available

            auto status = wait_for_object(not_full_event, true);

            if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED)
                return status;
        }
    }

    return nt_status::SUCCESS;
}

nt_status pl011_out_pipe::flush(bool go_readonly) {
    if (go_readonly)
        readonly = true;

    auto& UARTDR = *(volatile uint32_t*)base_address;
    auto& UARTFR = *(volatile uint32_t*)(base_address + 0x18);

    lock_guard l(lock);

    while (true) {
        if (buf_unread == 0) {
            ready = true;
            return nt_status::SUCCESS;
        }

        size_t buf_read_pos = buf_write_pos + (buf_pages << PAGE_SHIFT) - buf_unread;

        while (buf_read_pos >= buf_pages << PAGE_SHIFT) {
            buf_read_pos -= buf_pages << PAGE_SHIFT;
        }

        while (UARTFR & PL011_UARTFR_BUSY) { }

        UARTDR = (uint32_t)buf[buf_read_pos];

        buf_unread--;
    }

    return nt_status::SUCCESS;
}

void pl011_pipe::irq() {
    auto uartmis_value = *(volatile uint32_t*)(send_pipe.base_address + 0x40);

    if (uartmis_value & PL011_UARTMIS_TXMIS) {
        auto& UARTICR = *(volatile uint32_t*)(send_pipe.base_address + 0x44);

        UARTICR |= PL011_UARTICR_TXIC;

        {
            sync_lock sl;
            lock_guard l(sl);
            lock_guard l2(send_pipe.lock);

            if (send_pipe.buf_unread == 0)
                send_pipe.ready = true;
            else {
                auto& UARTDR = *(volatile uint32_t*)send_pipe.base_address;

                size_t buf_read_pos = send_pipe.buf_write_pos + (send_pipe.buf_pages << PAGE_SHIFT) - send_pipe.buf_unread;

                while (buf_read_pos >= send_pipe.buf_pages << PAGE_SHIFT) {
                    buf_read_pos -= send_pipe.buf_pages << PAGE_SHIFT;
                }

                UARTDR = (uint32_t)(send_pipe.buf[buf_read_pos]);
                send_pipe.buf_unread--;
            }
        }
    }

    if (uartmis_value & PL011_UARTMIS_RXMIS) {
        uint64_t written_bytes;
        auto& UARTDR = *(volatile uint32_t*)send_pipe.base_address;
        auto& UARTICR = *(volatile uint32_t*)(send_pipe.base_address + 0x44);

        UARTICR |= PL011_UARTICR_RXIC;

        auto c = (uint8_t)UARTDR;

        auto sp = span((const uint8_t*)&c, 1);

        // FIXME - don't sleep here!
        recv_pipe.write(span(&sp, 1), written_bytes);
    }
}

static bool pl011_irq_handler(void* ctx) {
    auto& p = *(pl011_pipe*)ctx;

    p.irq();

    return true;
}

static object_ref create_pl011_out_pipe(unsigned int buffer_pages) {
    auto pptr = new pl011_out_pipe(buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    add_to_flushable_pipes_list(*pptr);

    return p;
}

static object_ref create_pl011_in_pipe(unsigned int buffer_pages) {
    auto pptr = new pipe(buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    return p;
}

static optional<unsigned int> parse_uid(string_view uid) {
    if (uid.empty())
        return nullopt;

    for (auto c : uid) {
        if (c < '0' || c > '9')
            return nullopt;
    }

    unsigned int val;

    from_chars(uid.data(), uid.data() + uid.size(), val);

    return val;
}

static bool init_pl011(uint64_t mem_start, uint64_t mem_length,
                       uint32_t irq, string_view uid,
                       uint64_t base_clock) {
    if (mem_length == 0) {
        print("init_pl011: invalid mem_length {}\n", mem_length);
        return false;
    }

    // FIXME - where does clock frequency come from?
    auto send_handle = create_pl011_out_pipe(serial_buffer_pages);
    if (!send_handle) {
        print("create_pl011_out_pipe failed\n");
        return false;
    }

    auto recv_handle = create_pl011_in_pipe(serial_buffer_pages);
    if (!recv_handle) {
        print("create_pl011_in_pipe failed\n");
        return false;
    }

    auto& send_pipe = static_cast<pl011_out_pipe&>(send_handle.get());

    auto& recv_pipe = static_cast<pipe&>(recv_handle.get());

    auto pptr = new pl011_pipe(recv_pipe, send_pipe, mem_start, base_clock,
                               115200);
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    if (!pptr->init) {
        print("pl011_pipe::pl011_pipe failed\n");
        delete pptr;
        return false;
    }

    auto pipe_handle = object_ref{*pptr};

    send_pipe.base_address = pptr->base_address;

    if (!add_irq_handler(irq, true, pl011_irq_handler, pptr)) {
        print("add_irq_handler failed\n");
        return false;
    }

    // FIXME - numbering assumes that ACPI devices are started in order, i.e. all from one thread

    // use _UID as number if present, otherwise start from end of numbered range

    auto uidnum = parse_uid(uid);
    unsigned int num;

    if (uidnum.has_value())
        num = *uidnum;
    else
        num = pl011_number.fetch_add(1);

    {
        char s[15];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "ttyAMA{}", num);

        auto status = name_object(pipe_handle.get(), string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    if (num == 0 && !com1_pipe_handle)
        com1_pipe_handle.swap(pipe_handle);

    return true;
}

static void found_pl011(const acpi_device& d) {
    optional<pair<uint64_t, uint64_t>> mem;
    optional<uint32_t> irq;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        switch (r.type) {
            case acpi_device_resource_type::fixed_memory32: {
                auto& r2 = static_cast<acpi_device_resource_fixed_memory32&>(r);

                if (!mem.has_value()) {
                    mem = make_pair(r2.fixed_memory32.Address,
                                    r2.fixed_memory32.AddressLength);
                }

                break;
            }

            case acpi_device_resource_type::address: {
                auto& r2 = static_cast<acpi_device_resource_address&>(r);

                if (!mem.has_value()) {
                    mem = make_pair(r2.address.Address.Minimum,
                                    r2.address.Address.AddressLength);
                }

                break;
            }

            case acpi_device_resource_type::irq: {
                auto& r2 = static_cast<acpi_device_resource_irq&>(r);

                if (!irq.has_value())
                    irq = r2.irq.Interrupt;

                break;
            }

            case acpi_device_resource_type::extended_irq: {
                auto& r2 = static_cast<acpi_device_resource_extended_irq&>(r);

                if (!irq.has_value())
                    irq = r2.extended_irq.Interrupt;

                break;
            }

            default:
                break;
        }
    }

    if (!mem.has_value() || !irq.has_value())
        return;

    uint64_t base_clock = 24000000;

    for (auto le = d.properties.next; le != &d.properties; le = le->next) {
        auto& p = containing_record<acpi_property>(le);

        if (p.name == "clock-frequency") {
            base_clock = p.val;
            break;
        }
    }

    if (!init_pl011(mem->first, mem->second, *irq, d.uid, base_clock))
        print("init_pl011 failed for {:x},{:x}\n", mem->first, mem->second);
}
