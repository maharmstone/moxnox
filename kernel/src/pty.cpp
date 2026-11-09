// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <mutex>
#include <string_view>
#include <array>
#include <atomic>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

export module pty;

import nt;
import print;
import format;
import chardev;
import handle;
import object_ref;
import moxnox_ioctls;
import pipe;
import spinlock;
import scheduler;
import thread;
import objects;
import termios;
import rwsem;
import process;
import list;
import vfs;
import vec;
import visit;

using namespace std;

class ptyctl : public chardev {
public:
    ptyctl() { }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
};

export class pty_client : public chardev {
public:
    pty_client(pipe& recv_pipe, pipe& send_pipe, uint16_t cols, uint16_t rows);
    ~pty_client();
    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;
    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
    sync_object& get_sync_object() override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
    void last_handle_closed() override;
    nt_status userspace_open(object_ref&) override;

    void server_disconnected();
    nt_status canonical_write(span<const uint8_t> s, uint64_t& written_bytes);
    void post_info_event(pty_info_type type);

    nt_status tcgetattr(span<uint8_t> outbuf, uintptr_t& written);
    nt_status tcsetattr(span<const uint8_t> inbuf, bool flush);
    nt_status tcflush(span<const uint8_t> inbuf);
    nt_status set_size(span<const uint8_t> inbuf);
    nt_status get_size(span<uint8_t> outbuf, uintptr_t& written);
    nt_status get_fg_pg(span<uint8_t> outbuf, uintptr_t& written);

    pipe& recv_pipe;
    pipe& send_pipe;
    spinlock size_lock;
    uint16_t cols;
    uint16_t rows;
    bool disconnected = false;
    termios settings;
    bool no_open_handles = true;

    spinlock fg_pg_lock;
    process_group* fg_pg = nullptr;

    rwsem input_buffer_lock;
    array<char, 4096> input_buffer;
    size_t input_buffer_valid = 0;

    spinlock info_streams_lock;
    list_entry info_streams = { &info_streams, &info_streams };
    rwsem info_mutex;

private:
    nt_status set_fg_pg(span<const uint8_t> inbuf);
    nt_status get_info_stream(span<uint8_t> outbuf);
    size_t process_canonical_input(span<const uint8_t> new_data, uint8_t* echo_buf,
                                   bool& found_eof);
};

class pty_server : public chardev {
public:
    pty_server(pty_client& client);
    ~pty_server();
    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;
    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
    sync_object& get_sync_object() override;
    void last_handle_closed() override;

    pty_client& client;

private:
    nt_status write2(span<const span<const uint8_t>> v,
                     uint64_t& written_bytes);
    nt_status get_client(span<uint8_t> outbuf, uintptr_t& written);
};

struct pty_info_event2 {
    list_entry le;
    pty_info_event ev;
};

class pty_info : public chardev {
public:
    pty_info(pty_client& parent) : parent(parent) {
        parent.acquire();

        signalled = false;
    }

    ~pty_info() {
        {
            lock_guard lock(parent.info_streams_lock);
            remove_entry_list(le);
        }

        parent.release();
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    void post_info_event(pty_info_type type);

    list_entry le;
    pty_client& parent;
    bool visited;

    spinlock queue_lock;
    list_entry queue = { &queue, &queue };
    size_t queue_length = 0;
};

export bool create_dev_ptyctl() {
    auto pptr = new ptyctl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "ptyctl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

static nt_status create_pty(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                            uintptr_t& written) {
    auto& t = *get_current_thread();
    nt_status status;

    static atomic<unsigned int> pty_num = 0;

    if (inbuf.size() < sizeof(MOXNOX_IOCTL_PTYCTL_CREATE_PTY_IN))
        return nt_status::INVALID_PARAMETER;

    if (outbuf.size() < sizeof(MOXNOX_IOCTL_PTYCTL_CREATE_PTY_OUT))
        return nt_status::INVALID_PARAMETER;

    auto& in = *(MOXNOX_IOCTL_PTYCTL_CREATE_PTY_IN*)inbuf.data();
    auto& out = *(MOXNOX_IOCTL_PTYCTL_CREATE_PTY_OUT*)outbuf.data();

    auto pptr_recv = new pipe(1);
    if (!pptr_recv) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj = object_ref{*pptr_recv};

    auto pptr_send = new pipe(1);
    if (!pptr_send) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj2 = object_ref{*pptr_send};

    auto pptr_client = new pty_client(*pptr_recv, *pptr_send, in.cols, in.rows);
    if (!pptr_client) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto& settings = pptr_client->settings;

    memset(&settings, 0, sizeof(settings));

    settings.c_iflag = ICRNL | IXON;
    settings.c_oflag = OPOST | ONLCR;
    settings.c_cflag = B38400 | CS8 | CREAD | HUPCL;
    settings.c_lflag = ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ISIG | IEXTEN;

    settings.c_cc[VINTR] = 'C' - 0x40;
    settings.c_cc[VQUIT] = '\\' - 0x40;
    settings.c_cc[VERASE] = 0x7f;
    settings.c_cc[VKILL] = 'U' - 0x40;
    settings.c_cc[VEOF] = 'D' - 0x40;
    settings.c_cc[VSTART] = 'Q' - 0x40;
    settings.c_cc[VSTOP] = 'S' - 0x40;
    settings.c_cc[VSUSP] = 'Z' - 0x40;
    settings.c_cc[VREPRINT] = 'R' - 0x40;
    settings.c_cc[VDISCARD] = 'O' - 0x40;
    settings.c_cc[VWERASE] = 'W' - 0x40;
    settings.c_cc[VLNEXT] = 'V' - 0x40;
    settings.c_cc[VMIN] = 1;

    settings.c_ispeed = 38400;
    settings.c_ospeed = 38400;

    auto obj3 = object_ref{*pptr_client};

    auto pptr_server = new pty_server(*pptr_client);

    if (!pptr_server) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj4 = object_ref{*pptr_server};

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "pty{}", pty_num.fetch_add(1));

        status = name_object(obj3.get(), string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object reutrned {}\n", status);
            return status;
        }
    }

    handle_t server_handle, client_handle;

    auto server_file = new file(*pptr_server, 0, 0);
    if (!server_file) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj5 = object_ref{*server_file};

    status = t.proc->add_handle(*server_file, FILE_READ_DATA | FILE_WRITE_DATA,
                                true, server_handle);
    if (!success(status))
        return status;

    status = object_userspace_open(obj3);
    if (!success(status)) {
        t.proc->close_handle(server_handle);
        return status;
    }

    auto client_file = new file(obj3.get(), 0, 0);
    if (!client_file) {
        print("new failed\n");
        t.proc->close_handle(server_handle);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj6 = object_ref{*client_file};

    status = t.proc->add_handle(*client_file, FILE_READ_DATA | FILE_WRITE_DATA,
                                true, client_handle);
    if (!success(status)) {
        t.proc->close_handle(server_handle);
        return status;
    }

    out.server_handle = server_handle;
    out.client_handle = client_handle;

    written = sizeof(MOXNOX_IOCTL_PTYCTL_CREATE_PTY_OUT);

    return nt_status::SUCCESS;
}

nt_status ptyctl::ioctl(uint32_t code, span<const uint8_t> inbuf,
                        span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_PTYCTL_CREATE_PTY:
            return create_pty(inbuf, outbuf, written);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

pty_client::pty_client(pipe& recv_pipe, pipe& send_pipe, uint16_t cols, uint16_t rows) : recv_pipe(recv_pipe), send_pipe(send_pipe) {
    this->cols = cols;
    this->rows = rows;

    recv_pipe.acquire();
    send_pipe.acquire();
}

pty_client::~pty_client() {
    recv_pipe.release();
    send_pipe.release();

    if (fg_pg)
        fg_pg->release();
}

nt_status pty_client::read(span<const span<uint8_t>> v, uint64_t& read_bytes) {
    auto ret = recv_pipe.read(v, read_bytes);

    if (disconnected && read_bytes == 0) {
        lock_guard l(recv_pipe.lock);

        if (recv_pipe.buf_unread == 0)
            return nt_status::PIPE_DISCONNECTED;
    }

    return ret;
}

static size_t find_newline(span<const uint8_t> s) {
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\n')
            return i;
    }

    return string_view::npos;
}

nt_status pty_client::write(span<const span<const uint8_t>> v,
                            uint64_t& written_bytes) {
    nt_status status;

    if (disconnected)
        return nt_status::PIPE_DISCONNECTED;

    // FIXME - other c_oflags?

    if (settings.c_oflag & OPOST && settings.c_oflag & ONLCR) {
        uint64_t w = 0;

        static const array<uint8_t, 2> crlf = { '\r', '\n' };

        for (auto s : v) {
            if (s.empty())
                continue;

            while (true) {
                auto nl = find_newline(s);

                if (nl == string_view::npos) {
                    uint64_t sent;

                    status = send_pipe.write(span(&s, 1), sent);

                    if (disconnected)
                        return nt_status::PIPE_DISCONNECTED;

                    if (!success(status) || status == nt_status::USER_APC) {
                        if (w == 0)
                            return status;

                        written_bytes = w;
                        return nt_status::SUCCESS;
                    }

                    w += sent;
                    break;
                } else {
                    if (nl != 0) {
                        uint64_t sent;
                        auto sp = span(s.data(), nl);

                        status = send_pipe.write(span(&sp, 1), sent);

                        if (disconnected)
                            return nt_status::PIPE_DISCONNECTED;

                        if (!success(status) || status == nt_status::USER_APC) {
                            if (w == 0)
                                return status;

                            written_bytes = w;
                            return nt_status::SUCCESS;
                        }

                        w += sent;
                        s = span(s.data() + sent, s.size() - sent);

                        if (sent < nl)
                            continue;
                    }

                    // send CRLF

                    {
                        uint64_t sent;
                        auto sp = span(crlf.data(), crlf.size());

                        status = send_pipe.write(span(&sp, 1), sent);

                        if (disconnected)
                            return nt_status::PIPE_DISCONNECTED;

                        if (!success(status) || status == nt_status::USER_APC) {
                            if (w == 0)
                                return status;

                            written_bytes = w;
                            return nt_status::SUCCESS;
                        }

                        if (sent == 1) { // only sent CR
                            sp = span(crlf.data() + 1, 1);
                            status = send_pipe.write(span(&sp, 1), sent);

                            if (disconnected)
                                return nt_status::PIPE_DISCONNECTED;

                            if (!success(status) || status == nt_status::USER_APC) {
                                if (w == 0)
                                    return status;

                                written_bytes = w;
                                return nt_status::SUCCESS;
                            }
                        }
                    }

                    s = span(s.data() + 1, s.size() - 1);
                    w++;
                }
            }
        }

        written_bytes = w;
        return nt_status::SUCCESS;
    } else {
        status = send_pipe.write(v, written_bytes);

        if (disconnected)
            return nt_status::PIPE_DISCONNECTED;

        return status;
    }
}

sync_object& pty_client::get_sync_object() {
    return recv_pipe;
}

void pty_client::server_disconnected() {
    {
        sync_lock sl;
        lock_guard l(sl);

        disconnected = true;

        send_pipe.readonly = true;

        if (!send_pipe.not_full_event.signalled)
            send_pipe.not_full_event.set_holding_sync_lock();

        if (!recv_pipe.signalled) {
            recv_pipe.signalled = true;

            wake_up_waiters(recv_pipe.waiters, WAIT_TYPE_READING);
        }
    }

    if (!(settings.c_cflag & CLOCAL)) {
        process_group* pg = nullptr;

        {
            lock_guard lock(fg_pg_lock);

            if (fg_pg) {
                fg_pg->acquire();
                pg = fg_pg;
            }
        }

        if (pg) {
            auto status = pg->sess.send_signal(SIGHUP);
            if (!success(status))
                print("session::send_signal returned {}\n", status);

            pg->release();
        }
    }
}

pty_server::pty_server(pty_client& client) : client(client) {
    client.acquire();
}

pty_server::~pty_server() {
    client.release();
}

nt_status pty_server::read(span<const span<uint8_t>> v, uint64_t& read_bytes) {
    return client.send_pipe.read(v, read_bytes);
}

size_t pty_client::process_canonical_input(span<const uint8_t> new_data,
                                           uint8_t* echo_buf, bool& found_eof) {
    bool echoe = settings.c_lflag & ECHOE;
    bool echoctl = settings.c_lflag & ECHOCTL;
    bool icrnl = settings.c_iflag & ICRNL;
    bool onlcr = settings.c_oflag & OPOST && settings.c_oflag & ONLCR;
    auto bs = settings.c_cc[VERASE];
    auto eof = settings.c_cc[VEOF];
    auto* ptr = echo_buf;

    for (size_t i = 0; i < new_data.size(); i++) {
        if (new_data[i] == bs) {
            if (input_buffer_valid > 0) {
                input_buffer_valid--;

                while (input_buffer_valid > 0 && ((uint8_t)input_buffer[input_buffer_valid] & 0xc0) == 0x80) {
                    input_buffer_valid--;
                }

                if (ptr) {
                    if (echoe) {
                        *ptr = '\b';
                        ptr++;
                        *ptr = ' ';
                        ptr++;
                        *ptr = '\b';
                        ptr++;
                    } else {
                        *ptr = new_data[i];
                        ptr++;
                    }
                }
            }
        } else if (new_data[i] == eof) {
            found_eof = true;
            break;
        } else if (new_data[i] == '\r' && icrnl) {
            if (input_buffer_valid < input_buffer.size()) {
                input_buffer[input_buffer_valid] = '\n';
                input_buffer_valid++;
            }

            if (ptr && onlcr) {
                *ptr = '\r';
                ptr++;
            }

            if (ptr) {
                *ptr = '\n';
                ptr++;
            }
        } else if (echoctl && (new_data[i] < 0x20 || new_data[i] == 0x7f) && new_data[i] != '\n' && new_data[i] != '\t') { // FIXME - also not START or STOP
            if (input_buffer_valid < input_buffer.size()) {
                input_buffer[input_buffer_valid] = (char)new_data[i];
                input_buffer_valid++;
            }

            if (ptr) {
                *ptr = '^';
                ptr++;
            }

            if (ptr) {
                *ptr = new_data[i] ^ 0x40;
                ptr++;
            }
        } else {
            if (input_buffer_valid < input_buffer.size()) {
                input_buffer[input_buffer_valid] = (char)new_data[i];
                input_buffer_valid++;
            }

            if (ptr) {
                *ptr = new_data[i];
                ptr++;
            }
        }
    }

    return echo_buf ? (size_t)(ptr - echo_buf) : 0;
}

nt_status pty_client::canonical_write(span<const uint8_t> s, uint64_t& written_bytes) {
    nt_status status;
    size_t orig_size = s.size(), bytes;
    bool echo = settings.c_lflag & ECHO;
    bool echonl = settings.c_lflag & ECHONL;
    bool onlcr = settings.c_oflag & OPOST && settings.c_oflag & ONLCR;

    // FIXME - Ctrl+D

    auto do_echo = [this](span<const uint8_t> sp) -> nt_status {
        size_t bytes;

        if (!sp.empty()) {
            auto status = send_pipe.write_nonblock(span(&sp, 1), bytes);
            if (!success(status))
                return status;
        }

        return nt_status::SUCCESS;
    };

    {
        lock_guard lock(input_buffer_lock);

        auto do_line = [this](span<const uint8_t> sp) -> nt_status {
            nt_status status;
            size_t bytes;

            if (sp.empty()) {
                recv_pipe.set_eof_pending();
                return nt_status::SUCCESS;
            }

            while (true) {
                // FIXME - mechanics of canonical writes mean that this should be an uninterruptible wait
                status = recv_pipe.write(span(&sp, 1), bytes);
                if (!success(status) || status == nt_status::USER_APC)
                    return status;

                if (bytes == sp.size())
                    break;

                sp = span(sp.data() + bytes, sp.size() - bytes);
            }

            return nt_status::SUCCESS;
        };

        while (!s.empty()) {
            uint8_t* echobuf = nullptr;
            auto nl = string_view((char*)s.data(), s.size()).find('\r');
            size_t linelen = nl == string_view::npos ? s.size() : (nl + 1);
            bool found_eof = false;

            if (echo) {
                echobuf = (uint8_t*)malloc(3 * linelen);
                if (!echobuf) {
                    print("malloc failed\n");
                    return nt_status::INSUFFICIENT_RESOURCES;
                }
            }

            auto echosize = process_canonical_input(span(s.data(), linelen),
                                                    echobuf, found_eof);

            if (echo) {
                status = do_echo(span(echobuf, echosize));
                if (!success(status)) {
                    free(echobuf);
                    return status;
                }

                free(echobuf);
            }

            if (nl == string_view::npos && !found_eof)
                break;

            status = do_line(span((uint8_t*)input_buffer.data(), input_buffer_valid));
            if (!success(status) || status == nt_status::USER_APC)
                return status;

            input_buffer_valid = 0;

            if (found_eof)
                break;

            if (!echo && echonl) {
                status = send_pipe.write_nonblock("\r", bytes);
                if (!success(status))
                    return status;

                if (onlcr) {
                    status = send_pipe.write_nonblock("\n", bytes);
                    if (!success(status))
                        return status;
                }
            }

            s = span(s.data() + nl + 1, s.size() - nl - 1);
        }
    }

    written_bytes += orig_size; // discard excess data rather than doing a partial write

    return nt_status::SUCCESS;
}

nt_status pty_server::write2(span<const span<const uint8_t>> v,
                             uint64_t& written_bytes) {
    if (client.settings.c_lflag & ICANON) { // FIXME - do we need a spinlock for this?
        // FIXME - should canonical_write itself be doing this?

        written_bytes = 0;

        for (auto s : v) {
            if (s.empty())
                continue;

            auto status = client.canonical_write(s, written_bytes);
            if (!success(status) || status == nt_status::USER_APC)
                return status;
        }

        return nt_status::SUCCESS;
    } else
        return client.recv_pipe.write(v, written_bytes);
}

void pty_client::post_info_event(pty_info_type type) {
    lock_guard lg(info_mutex);

    {
        lock_guard lg2(info_streams_lock);

        for (auto le = info_streams.next; le != &info_streams; le = le->next) {
            auto& info = containing_record<pty_info>(le);

            info.visited = false;
        }

        if (is_list_empty(info_streams))
            return;
    }

    while (true) {
        pty_info* info = nullptr;

        {
            lock_guard lg2(info_streams_lock);

            for (auto le = info_streams.next; le != &info_streams; le = le->next) {
                auto& info2 = containing_record<pty_info>(le);

                if (!info2.visited) {
                    info2.acquire();
                    info = &info2;

                    info2.visited = true;
                    remove_entry_list(info2.le);
                    insert_tail_list(info_streams, info2.le);
                }
            }
        }

        if (!info)
            return;

        info->post_info_event(type);
        info->release();
    }
}

nt_status pty_server::write(span<const span<const uint8_t>> v,
                            uint64_t& written_bytes) {
    nt_status status;
    uint64_t bytes;
    bool noflsh = client.settings.c_lflag & NOFLSH;

    if (!(client.settings.c_lflag & ISIG))
        return write2(v, written_bytes);

    // FIXME - VSUSP

    written_bytes = 0;

    for (size_t j = 0; j < v.size(); j++) {
        auto s = v[j];

        while (!s.empty()) {
            bool again = false;

            for (size_t i = 0; i < s.size(); i++) {
                if (s[i] == client.settings.c_cc[VINTR] ||
                    s[i] == client.settings.c_cc[VSUSP]) {
                    if (i != 0) {
                        auto sp = span(s.data(), i);
                        status = write2(span(&sp, 1), bytes);
                        if (!success(status) || status == nt_status::USER_APC) {
                            if (written_bytes > 0)
                                return nt_status::SUCCESS;
                            else
                                return status;
                        }

                        written_bytes += bytes;
                    }

                    process_group* pg = nullptr;

                    {
                        lock_guard lock(client.fg_pg_lock);

                        if (client.fg_pg) {
                            pg = client.fg_pg;
                            pg->acquire();
                        }
                    }

                    if (pg) {
                        if (s[i] == client.settings.c_cc[VINTR]) {
                            status = send_signal_to_process_group(*pg, SIGINT);
                            if (!success(status))
                                print("send_signal_to_process_group returned {}\n", status);
                        } else if (s[i] == client.settings.c_cc[VSUSP]) {
                            status = send_signal_to_process_group(*pg, SIGTSTP);
                            if (!success(status))
                                print("send_signal_to_process_group returned {}\n", status);

                            client.post_info_event(pty_info_type::suspend); // FIXME - include pid or something?
                        }

                        pg->release();
                    }

                    written_bytes++;
                    s = span(s.data() + i + 1, s.size() - i - 1);

                    if (!noflsh) {
                        client.recv_pipe.clear();
                        client.send_pipe.clear();
                        written_bytes += s.size();

                        for (size_t k = j + 1; k < v.size(); k++) {
                            written_bytes += v[k].size();
                        }

                        return nt_status::SUCCESS;
                    }

                    again = true;
                    break;
                }
            }

            if (again)
                continue;

            status = write2(span(&s, 1), bytes);
            if (!success(status) || status == nt_status::USER_APC) {
                if (written_bytes > 0)
                    return nt_status::SUCCESS;
                else
                    return status;
            }

            written_bytes += bytes;
            break;
        }
    }

    return nt_status::SUCCESS;
}

sync_object& pty_server::get_sync_object() {
    return client.send_pipe;
}

void pty_server::last_handle_closed() {
    client.server_disconnected();

    remove_object_name(client);
}

nt_status pty_client::get_size(span<uint8_t> outbuf, uintptr_t& written) {
    if (outbuf.size() < sizeof(MOXNOX_IOCTL_PTY_GET_SIZE))
        return nt_status::INVALID_PARAMETER;

    auto& out = *(MOXNOX_IOCTL_PTY_GET_SIZE*)outbuf.data();

    {
        lock_guard lock(size_lock);

        out.cols = cols;
        out.rows = rows;
    }

    written = sizeof(MOXNOX_IOCTL_PTY_GET_SIZE);

    return nt_status::SUCCESS;
}

nt_status pty_client::set_size(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(MOXNOX_IOCTL_PTY_GET_SIZE))
        return nt_status::INVALID_PARAMETER;

    auto& in = *(MOXNOX_IOCTL_PTY_GET_SIZE*)inbuf.data();

    bool send_sigwinch = false;

    {
        lock_guard lock(size_lock);

        if (cols != in.cols || rows != in.rows)
            send_sigwinch = true;

        cols = in.cols;
        rows = in.rows;
    }

    if (send_sigwinch) {
        process_group* pg = nullptr;

        {
            lock_guard lock(fg_pg_lock);

            if (fg_pg) {
                pg = fg_pg;
                pg->acquire();
            }
        }

        if (pg) {
            auto status = send_signal_to_process_group(*pg, SIGWINCH);
            if (!success(status)) {
                print("send_signal_to_process_group returned {}\n", status);
                pg->release();
                return status;
            }

            pg->release();
        }
    }

    return nt_status::SUCCESS;
}

nt_status pty_client::get_fg_pg(span<uint8_t> outbuf, uintptr_t& written) {
    if (outbuf.size() < sizeof(uintptr_t))
        return nt_status::INVALID_PARAMETER;

    auto& out = *(uintptr_t*)outbuf.data();

    {
        lock_guard lock(fg_pg_lock);

        if (!fg_pg)
            return nt_status::INVALID_PARAMETER;

        out = fg_pg->tree.key;
    }

    written = sizeof(uintptr_t);

    return nt_status::SUCCESS;
}

nt_status pty_client::ioctl(uint32_t code, span<const uint8_t> inbuf,
                            span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_PTY_ISATTY:
            written = 0;
            return nt_status::SUCCESS;

        case IOCTL_MOXNOX_PTY_TCGETATTR:
            return tcgetattr(outbuf, written);

        case IOCTL_MOXNOX_PTY_TCSETATTR:
            return tcsetattr(inbuf, false);

        case IOCTL_MOXNOX_PTY_TCSETATTR_FLUSH:
            return tcsetattr(inbuf, true);

        case IOCTL_MOXNOX_PTY_TCFLUSH:
            return tcflush(inbuf);

        case IOCTL_MOXNOX_PTY_SET_FOREGROUND_PROCESS_GROUP:
            return set_fg_pg(inbuf);

        case IOCTL_MOXNOX_PTY_GET_INFO_STREAM:
            return get_info_stream(outbuf);

        case IOCTL_MOXNOX_PTY_GET_SIZE:
            return get_size(outbuf, written);

        case IOCTL_MOXNOX_PTY_SET_SIZE:
            return set_size(inbuf);

        case IOCTL_MOXNOX_PTY_GET_FOREGROUND_PROCESS_GROUP:
            return get_fg_pg(outbuf, written);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

nt_status pty_client::tcgetattr(span<uint8_t> outbuf, uintptr_t& written) {
    if (outbuf.size() < sizeof(termios))
        return nt_status::BUFFER_TOO_SMALL;

    memcpy(outbuf.data(), &settings, sizeof(termios));
    written = sizeof(termios);

    return nt_status::SUCCESS;
}

nt_status pty_client::tcsetattr(span<const uint8_t> inbuf, bool flush) {
    if (inbuf.size() < sizeof(termios))
        return nt_status::BUFFER_TOO_SMALL;

    if (flush) {
        {
            lock_guard lock(input_buffer_lock);
            input_buffer_valid = 0;
        }

        recv_pipe.clear();
    }

    memcpy(&settings, inbuf.data(), sizeof(termios));

    return nt_status::SUCCESS;
}

nt_status pty_client::tcflush(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(int))
        return nt_status::BUFFER_TOO_SMALL;

    auto queue_selector = *(int*)inbuf.data();

    switch (queue_selector) {
        case TCIFLUSH:
        case TCOFLUSH:
        case TCIOFLUSH:
            break;

        default:
            return nt_status::INVALID_PARAMETER;
    }

    if (queue_selector == TCIFLUSH || queue_selector == TCIOFLUSH) {
        {
            lock_guard lock(input_buffer_lock);
            input_buffer_valid = 0;
        }

        recv_pipe.clear();
    }

    if (queue_selector == TCOFLUSH || queue_selector == TCIOFLUSH)
        send_pipe.clear();

    return nt_status::SUCCESS;
}

nt_status pty_client::set_fg_pg(span<const uint8_t> inbuf) {
    if (inbuf.size() < sizeof(uintptr_t))
        return nt_status::BUFFER_TOO_SMALL;

    auto pgid = *(uintptr_t*)inbuf.data();

    auto pg = get_process_group(pgid);
    if (!pg)
        return nt_status::INVALID_PARAMETER;

    bool fail = false;

    {
        lock_guard lock(pg->sess.lock);

        if (!pg->sess.terminal || &pg->sess.terminal.get() != this)
            fail = true;
    }

    if (fail) {
        pg->release();
        return nt_status::ACCESS_DENIED;
    }

    {
        lock_guard lock(fg_pg_lock);

        swap(fg_pg, pg);
    }

    if (pg)
        pg->release();

    return nt_status::SUCCESS;
}

nt_status pty_client::get_info_stream(span<uint8_t> outbuf) {
    auto& t = *get_current_thread();
    nt_status status;

    if (outbuf.size() < sizeof(HANDLE))
        return nt_status::BUFFER_TOO_SMALL;

    auto info = new pty_info(*this);
    if (!info) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    {
        lock_guard lock(info_streams_lock);
        insert_tail_list(info_streams, info->le);
    }

    handle_t h;

    status = t.proc->add_handle(*info, FILE_READ_DATA | FILE_WRITE_DATA,
                                true, h);
    if (!success(status)) {
        info->release();
        return status;
    }

    info->release();

    *(HANDLE*)outbuf.data() = h;

    return nt_status::SUCCESS;
}

void pty_info::post_info_event(pty_info_type type) {
    static const size_t MAX_QUEUE_LENGTH = 10;

    auto ev = new pty_info_event2;
    if (!ev) {
        print("new failed\n");
        return;
    }

    ev->ev.type = type;

    bool done = false;

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(queue_lock);

        if (queue_length == 0) {
            signalled = true;
            wake_up_waiters(waiters, WAIT_TYPE_READING);
        }

        if (queue_length < MAX_QUEUE_LENGTH) {
            insert_tail_list(queue, ev->le);
            queue_length++;
            done = true;
        }
    }

    if (!done) {
        delete ev;
        return;
    }
}

nt_status pty_info::read(span<const span<uint8_t>> v, uint64_t& read_bytes) {
    nt_status status;
    size_t vec_size = get_vec_size(v);

    if (vec_size < sizeof(pty_info_event))
        return nt_status::BUFFER_TOO_SMALL;

    span<uint8_t> top = v.front();

    while (true) {
        status = wait_for_object(*this, true);
        if (!success(status) || status == nt_status::USER_APC)
            return status;

        pty_info_event2* ev;

        {
            sync_lock sl;
            lock_guard lock(sl);
            lock_guard lock2(queue_lock);

            if (is_list_empty(queue))
                continue;

            ev = &containing_record<pty_info_event2>(queue.next);

            remove_entry_list(ev->le);
            queue_length--;

            if (queue_length == 0)
                signalled = false;
        }

        memcpy_vec(v, top, (uint8_t*)&ev->ev, sizeof(pty_info_event));
        read_bytes = sizeof(pty_info_event);

        return nt_status::SUCCESS;
    }
}

nt_status pty_server::ioctl(uint32_t code, span<const uint8_t> inbuf,
                            span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_PTY_GET_CLIENT:
            return get_client(outbuf, written);

        case IOCTL_MOXNOX_PTY_SET_SIZE:
            return client.set_size(inbuf);

        case IOCTL_MOXNOX_PTY_ISATTY:
            written = 0;
            return nt_status::SUCCESS;

        case IOCTL_MOXNOX_PTY_TCGETATTR:
            return client.tcgetattr(outbuf, written);

        case IOCTL_MOXNOX_PTY_TCSETATTR:
            return client.tcsetattr(inbuf, false);

        case IOCTL_MOXNOX_PTY_TCSETATTR_FLUSH:
            return client.tcsetattr(inbuf, true);

        case IOCTL_MOXNOX_PTY_TCFLUSH:
            return client.tcflush(inbuf);

        case IOCTL_MOXNOX_PTY_GET_SIZE:
            return client.get_size(outbuf, written);

        case IOCTL_MOXNOX_PTY_GET_FOREGROUND_PROCESS_GROUP:
            return client.get_fg_pg(outbuf, written);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

nt_status pty_server::get_client(span<uint8_t> outbuf, uintptr_t& written) {
    nt_status status;
    auto& t = *get_current_thread();

    if (outbuf.size() < sizeof(HANDLE))
        return nt_status::BUFFER_TOO_SMALL;

    client.acquire();

    auto obj1 = object_ref{client};

    status = object_userspace_open(obj1);
    if (!success(status))
        return status;

    auto client_file = new file(obj1.get(), 0, 0);
    if (!client_file) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj2 = object_ref{*client_file};
    handle_t client_handle;

    status = t.proc->add_handle(obj2.get(), FILE_READ_DATA | FILE_WRITE_DATA,
                                true, client_handle);
    if (!success(status))
        return status;

    *(HANDLE*)outbuf.data() = client_handle;
    written = sizeof(HANDLE);

    return nt_status::SUCCESS;
}

void pty_client::last_handle_closed() {
    sync_lock sl;
    lock_guard lock(sl);

    no_open_handles = true;

    send_pipe.signalled = true;
    wake_up_waiters(send_pipe.waiters, WAIT_TYPE_READING);
}

nt_status pty_client::userspace_open(object_ref&) {
    sync_lock sl;
    lock_guard lock(sl);

    if (!no_open_handles)
        return nt_status::SUCCESS;

    no_open_handles = false;

    {
        lock_guard lock2(send_pipe.lock);
        send_pipe.signalled = send_pipe.buf_unread > 0;
    }

    return nt_status::SUCCESS;
}
