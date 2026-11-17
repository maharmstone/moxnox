// SPDX-License-Identifier: GPL-2.0-only
#include <string.h>
#include <span>
#include <optional>
#include <string_view>
#include <charconv>
#include <atomic>
#include <mutex>

import nt;
import print;
import format;
import inout;
import config;
import interrupts;
import acpi;
import module_init;
import pipe;
import chardev;
import scheduler;
import list;
import spinlock;
import console;
import handle;
import object_ref;
import acpica;
import vec;

using namespace std;

static const unsigned int serial_buffer_pages = 1;
static atomic<unsigned int> serial_port_number = 0;

static bool serial_isr_func(void*);

static void prelim_found_serial_port(string_view uid);
static void found_serial_port(const acpi_device& d);

static acpi_recognizer mod_pnp_hids[] = {
    { {}, "PNP0500", found_serial_port, prelim_found_serial_port },
    { {}, "PNP0501", found_serial_port, prelim_found_serial_port }
};

static void init_acpi_hids() {
    for (unsigned int i = 0; i < sizeof(mod_pnp_hids) / sizeof(*mod_pnp_hids); i++) {
        insert_tail_list(pnp_hids, mod_pnp_hids[i].le);
    }
}

static module_init module_init_serial(init_acpi_hids);

class serial_out_pipe : public pipe {
public:
    serial_out_pipe(unsigned int buffer_pages, uint16_t port) : pipe(buffer_pages), port(port) { }
    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
    nt_status flush(bool go_readonly) override;

    bool ready = true;
    uint16_t port;
};

class serial_duplex_pipe : public chardev {
public:
    serial_duplex_pipe(pipe& recv_pipe, serial_out_pipe& send_pipe, uint16_t port) : recv_pipe(recv_pipe), send_pipe(send_pipe), port(port) {
        recv_pipe.acquire();
        send_pipe.acquire();
    }

    ~serial_duplex_pipe() {
        recv_pipe.release();
        send_pipe.release();
    }

    NTSTATUS read(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        return recv_pipe.read(v, read_bytes);
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override {
        return send_pipe.write(v, written_bytes);
    }

    sync_object& get_sync_object() override {
        return recv_pipe;
    }

    bool isr_func();

    pipe& recv_pipe;
    serial_out_pipe& send_pipe;
    uint16_t port;
};

static object_ref create_serial_in_pipe(unsigned int buffer_pages) {
    auto pptr = new pipe(buffer_pages);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    return p;
}

static object_ref create_serial_out_pipe(unsigned int buffer_pages, uint16_t port) {
    auto pptr = new serial_out_pipe(buffer_pages, port);
    if (!pptr) {
        print("new failed\n");
        return {};
    }

    auto& p = *pptr;

    add_to_flushable_pipes_list(*pptr);

    return p;
}

static bool init_serial_port(uint16_t port) {
    // disable interrupts
    outb(port + 1, 0);

    // enable DLAB
    outb(port + 3, 0x80);

    // set divisor to 3 (38400 baud)

    outb(port, 0x03);
    outb(port + 1, 0);

    // 8 bits, no parity, 1 stop bit
    outb(port + 3, 0x03);

    // enable FIFOs, clear them, and set interrupt trigger level to 14 bytes
    outb(port + 2, 0xc7);

    // put in loopback mode
    outb(port + 4, 0x10);

    // test
    outb(port, 0xa5);

    if (inb(port) != 0xa5)
        return false;

    // serial port okay, enable IRQs and put in normal mode
    outb(port + 4, 0x0f);

    // send IRQ when transmit buffer is empty or data available
    outb(port + 1, 0x03);

    return true;
}

static optional<unsigned int> parse_uid(string_view uid) {
    if (uid.empty())
        return nullopt;

    for (auto c : uid) {
        if (c < '0' || c > '9')
            return nullopt;
    }

    unsigned int val = 0;

    from_chars(uid.data(), uid.data() + uid.size(), val);

    return val;
}

static bool init_serial(uint16_t port, uint32_t irq, string_view uid) {
    if (!init_serial_port(port))
        return false;

    auto send_handle = create_serial_out_pipe(serial_buffer_pages, port);
    if (!send_handle) {
        print("create_serial_out_pipe failed\n");
        return false;
    }

    auto recv_handle = create_serial_in_pipe(serial_buffer_pages);
    if (!recv_handle) {
        print("create_serial_in_pipe failed\n");
        return false;
    }

    auto& send_pipe = static_cast<serial_out_pipe&>(send_handle.get());

    auto& recv_pipe = static_cast<pipe&>(recv_handle.get());

    auto pptr = new serial_duplex_pipe(recv_pipe, send_pipe, port);
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto pipe_handle = object_ref{*pptr};

    // FIXME - numbering assumes that ACPI devices are started in order, i.e. all from one thread

    // use _UID - 1 as number if present, otherwise start from end of numbered range

    auto uidnum = parse_uid(uid);
    unsigned int num;

    if (uidnum.has_value())
        num = *uidnum - 1;
    else
        num = serial_port_number.fetch_add(1);

    {
        char s[10];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "ttyS{}", num);

        auto status = name_object(pipe_handle.get(), string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
    }

    if (!add_irq_handler(irq, false, serial_isr_func, pptr)) {
        print("add_irq_handler failed\n");
        return false;
    }

    if (num == 0 && !com1_pipe_handle)
        com1_pipe_handle.swap(pipe_handle);

    return true;
}

nt_status serial_out_pipe::write(span<const span<const uint8_t>> v,
                                 uint64_t& written_bytes) {
    written_bytes = 0;

    for (auto sv : v) {
        auto s = sv;

        while (!s.empty()) {
            size_t to_copy;

            {
                lock_guard l(lock);

                if (ready) {
                    // send first character
                    outb(port, s[0]);

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

bool serial_duplex_pipe::isr_func() {
    bool done = false;

    while (true) {
        auto iir = inb(port + 2); // interrupt identification register

        if (iir & 1) // not us
            break;

        if (iir & 0x8 || ((iir & 0x6) == 0x4)) { // timeout, or data available
            uint64_t written_bytes;

            auto c = inb(port);
            auto sp = span((const uint8_t*)&c, 1);

            recv_pipe.write(span(&sp, 1), written_bytes);
        }

        if (iir & 0x8 || (iir & 0x6) == 0x2) { // timeout, or transmitter holding register empty
            sync_lock sl;
            lock_guard l(sl);
            lock_guard l2(send_pipe.lock);

            if (send_pipe.buf_unread == 0)
                send_pipe.ready = true;
            else {
                size_t buf_read_pos = send_pipe.buf_write_pos + (send_pipe.buf_pages << PAGE_SHIFT) - send_pipe.buf_unread;

                while (buf_read_pos >= send_pipe.buf_pages << PAGE_SHIFT) {
                    buf_read_pos -= send_pipe.buf_pages << PAGE_SHIFT;
                }

                outb(send_pipe.port, send_pipe.buf[buf_read_pos]);
                send_pipe.buf_unread--;
            }
        }

        done = true;
    }

    return done;
}

static bool serial_isr_func(void* ctx) {
    auto& pipe = *(serial_duplex_pipe*)ctx;

    return pipe.isr_func();
}

nt_status serial_out_pipe::flush(bool go_readonly) {
    if (go_readonly)
        readonly = true;

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

        while (!(inb(port + 5) & 0x20)) { }

        outb(port, buf[buf_read_pos]);

        buf_unread--;
    }

    return nt_status::SUCCESS;
}

static void prelim_found_serial_port(string_view uid) {
    auto val = parse_uid(uid);

    if (!val.has_value())
        return;

    if (*val > serial_port_number)
        serial_port_number = *val;
}

static void found_serial_port(const acpi_device& d) {
    optional<uint16_t> port;
    optional<uint32_t> irq;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        if (r.type == acpi_device_resource_type::io) {
            auto& r2 = static_cast<acpi_device_resource_io&>(r);

            if (r2.io.IoDecode == ACPI_DECODE_16 && !port.has_value())
                port = r2.io.Minimum;
        } else if (r.type == acpi_device_resource_type::irq) {
            auto& r2 = static_cast<acpi_device_resource_irq&>(r);

            irq = r2.irq.Interrupt;
        } else if (r.type == acpi_device_resource_type::extended_irq) {
            auto& r2 = static_cast<acpi_device_resource_extended_irq&>(r);

            irq = r2.extended_irq.Interrupt;
        }
    }

    if (!port.has_value() || !irq.has_value())
        return;

    if (!init_serial(*port, *irq, d.uid))
        print("init_serial failed for port {:x}, irq {:x}\n", *port, *irq);
}
