// SPDX-License-Identifier: GPL-2.0-only
module;

#include <string.h>
#include <span>
#include <optional>
#include <atomic>
#include <string_view>
#include <mutex>
#include "trace.h"

export module pipe;

import nt;
import print;
import mem;
import config;
import list;
import bsod;
import chardev;
import unicode;
import thread;
import event;
import spinlock;
import handle;
import object_ref;
import scheduler;
import vfs;
import vec;
import trace;

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

class named_duplex_pipe_client : public chardev {
public:
    named_duplex_pipe_client(pipe& recv_pipe, pipe& send_pipe) : recv_pipe(recv_pipe), send_pipe(send_pipe) {
        recv_pipe.acquire();
        send_pipe.acquire();
    }

    ~named_duplex_pipe_client() {
        recv_pipe.release();
        send_pipe.release();
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        auto ret = recv_pipe.read(v, read_bytes);

        if (disconnected && read_bytes == 0) {
            lock_guard l(recv_pipe.lock);

            if (recv_pipe.buf_unread == 0)
                return nt_status::PIPE_DISCONNECTED;
        }

        return ret;
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override {
        if (disconnected)
            return nt_status::PIPE_DISCONNECTED;

        auto ret = send_pipe.write(v, written_bytes);

        if (disconnected)
            return nt_status::PIPE_DISCONNECTED;

        return ret;
    }

    sync_object& get_sync_object() override {
        return recv_pipe;
    }

    void server_disconnected() {
        sync_lock sl;
        lock_guard l(sl);

        disconnected = true;

        send_pipe.readonly = true;

        if (!send_pipe.not_full_event.signalled)
            send_pipe.not_full_event.set_holding_sync_lock();

        recv_pipe.server_closed = true;

        if (!recv_pipe.signalled) {
            recv_pipe.signalled = true;

            wake_up_waiters(recv_pipe.waiters, WAIT_TYPE_READING);
        }
    }

    void last_handle_closed() override;

    bool is_pipe() override {
        return true;
    }

    pipe& recv_pipe;
    pipe& send_pipe;
    bool disconnected = false;
    object_ref server;
};

class named_duplex_pipe_server : public chardev {
public:
    named_duplex_pipe_server(named_duplex_pipe_client& client) : client(client) {
        client.acquire();
    }

    ~named_duplex_pipe_server() {
        client.release();
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override {
        return client.send_pipe.read(v, read_bytes);
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override {
        if (disconnected)
            return nt_status::PIPE_BROKEN;

        auto ret = client.recv_pipe.write(v, written_bytes);

        if (disconnected)
            return nt_status::PIPE_BROKEN;

        return ret;
    }

    sync_object& get_sync_object() override {
        return client.send_pipe;
    }

    void last_handle_closed() override {
        client.server_disconnected();

        remove_object_name(client);
    }

    bool is_pipe() override {
        return true;
    }

    void client_disconnected() {
        sync_lock sl;
        lock_guard l(sl);

        disconnected = true;

        client.recv_pipe.readonly = true;

        if (!client.recv_pipe.not_full_event.signalled)
            client.recv_pipe.not_full_event.set_holding_sync_lock();

        client.send_pipe.server_closed = true;

        if (!client.send_pipe.signalled) {
            client.send_pipe.signalled = true;

            wake_up_waiters(client.send_pipe.waiters, WAIT_TYPE_READING);
        }
    }

    named_duplex_pipe_client& client;
    bool disconnected = false;
};

static spinlock flushable_pipes_lock;
static list_entry flushable_pipes = { &flushable_pipes, &flushable_pipes };

pipe::pipe(unsigned int buffer_pages) : buf_pages(buffer_pages) {
    signalled = false;
    not_full_event.signalled = true;

    if (buffer_pages == 0) {
        buf = nullptr;
        return;
    }

    auto va = vmalloc(buffer_pages << PAGE_SHIFT);
    if (!va) {
        print("vmalloc failed\n");
        return;
    }

    buf = (uint8_t*)va;
}

pipe::~pipe() {
    if (!buf)
        return;

    vfree(buf, buf_pages << PAGE_SHIFT);
}

nt_status pipe::write(span<const span<const uint8_t>> v,
                      uint64_t& written_bytes) {
    size_t vec_size = get_vec_size(v);

    if (readonly) {
        written_bytes = 0;
        return nt_status::MEDIA_WRITE_PROTECTED;
    }

    if (server_closed) {
        written_bytes = 0;
        return nt_status::PIPE_DISCONNECTED;
    }

    if (vec_size == 0) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<const uint8_t> top = v.front();

    written_bytes = 0;

    do {
        {
            size_t to_copy;

            sync_lock sl;
            lock_guard l(sl);
            lock_guard l2(lock);

            size_t buf_left = (buf_pages << PAGE_SHIFT) - buf_unread;

            to_copy = min(vec_size, buf_left);

            if (to_copy > 0) {
                if (buf_write_pos + to_copy > buf_pages << PAGE_SHIFT) {
                    auto first_bit = (buf_pages << PAGE_SHIFT) - buf_write_pos;

                    memcpy_vec(&buf[buf_write_pos], v, top, first_bit);

                    buf_write_pos = 0;
                    buf_unread += first_bit;
                    to_copy -= first_bit;
                    vec_size -= first_bit;
                    written_bytes += first_bit;
                }

                memcpy_vec(&buf[buf_write_pos], v, top, to_copy);

                buf_write_pos += to_copy;
                buf_unread += to_copy;
                vec_size -= to_copy;
                written_bytes += to_copy;

                // wake up waiting thread

                if (!signalled) {
                    signalled = true;

                    wake_up_waiters(waiters, WAIT_TYPE_READING);
                }

                if (buf_unread == buf_pages << PAGE_SHIFT && !server_closed)
                    not_full_event.signalled = false;
            }
        }

        if (vec_size == 0)
            break;

        // block while we wait for space to become available
        auto status = wait_for_object(not_full_event, true);

        if (status == nt_status::USER_APC) {
            if (written_bytes == 0)
                return nt_status::USER_APC;
            else
                break;
        } else if (!success(status))
            return status;

        if (server_closed)
            break;

        if (readonly)
            break;
    } while (true);

    return nt_status::SUCCESS;
}

nt_status pipe::write_nonblock(span<const span<const uint8_t>> v,
                               uint64_t& written_bytes) {
    size_t vec_size = get_vec_size(v);

    if (readonly) {
        written_bytes = 0;
        return nt_status::MEDIA_WRITE_PROTECTED;
    }

    if (server_closed) {
        written_bytes = 0;
        return nt_status::PIPE_DISCONNECTED;
    }

    if (vec_size == 0) {
        written_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<const uint8_t> top = v.front();

    written_bytes = 0;

    {
        size_t to_copy;

        sync_lock sl;
        lock_guard l(sl);
        lock_guard l2(lock);

        size_t buf_left = (buf_pages << PAGE_SHIFT) - buf_unread;

        to_copy = min(vec_size, buf_left);

        if (to_copy > 0) {
            if (buf_write_pos + to_copy > buf_pages << PAGE_SHIFT) {
                auto first_bit = (buf_pages << PAGE_SHIFT) - buf_write_pos;

                memcpy_vec(&buf[buf_write_pos], v, top, first_bit);

                buf_write_pos = 0;
                buf_unread += first_bit;
                to_copy -= first_bit;
                vec_size -= first_bit;
                written_bytes += first_bit;
            }

            memcpy_vec(&buf[buf_write_pos], v, top, to_copy);

            buf_write_pos += to_copy;
            buf_unread += to_copy;
            vec_size -= to_copy;
            written_bytes += to_copy;

            // wake up waiting thread

            if (!signalled) {
                signalled = true;

                wake_up_waiters(waiters, WAIT_TYPE_READING);
            }

            if (buf_unread == buf_pages << PAGE_SHIFT && !server_closed)
                not_full_event.signalled = false;
        }
    }

    return nt_status::SUCCESS;
}

void pipe::set_eof_pending() {
    sync_lock sl;
    lock_guard l(sl);
    lock_guard l2(lock);

    if (eof_pending)
        return;

    eof_pending = true;

    if (!signalled) {
        signalled = true;

        wake_up_waiters(waiters, WAIT_TYPE_READING);
    }
}

NTSTATUS pipe::read(span<const span<uint8_t>> v, uint64_t& read_bytes) {
    size_t copied = 0;
    size_t vec_size = get_vec_size(v);

    if (vec_size == 0) {
        read_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<uint8_t> top = v.front();
    optional<uint64_t> timeout;

    acquire();
    object_ref obj{*this}; // so refcount gets decreased on exit

    if (server_closed)
        timeout = 0;

    auto status = wait_for_object(*this, true, timeout);

    if (status == nt_status::ALERTED) {
        read_bytes = 0;
        return nt_status::SUCCESS; // FIXME - really should be ALERTED (but make sure userspace handles this properly)
    } else if (status == nt_status::TIMEOUT) {
        read_bytes = 0;
        return nt_status::PIPE_DISCONNECTED;
    } else if (!success(status) || status == nt_status::USER_APC) {
        read_bytes = 0;
        return status;
    }

    {
        sync_lock sl;
        lock_guard l(sl);
        lock_guard l2(lock);

        if (buf_unread == 0) {
            read_bytes = 0;

            if (eof_pending) {
                eof_pending = false;
                return nt_status::END_OF_FILE;
            }

            return nt_status::SUCCESS;
        }

        size_t buf_read_pos = buf_write_pos + (buf_pages << PAGE_SHIFT) - buf_unread;

        while (buf_read_pos >= buf_pages << PAGE_SHIFT) {
            buf_read_pos -= buf_pages << PAGE_SHIFT;
        }

        size_t to_copy = min(vec_size, buf_unread);

        if (buf_read_pos + to_copy > buf_pages << PAGE_SHIFT) {
            auto first_bit = (buf_pages << PAGE_SHIFT) - buf_read_pos;

            memcpy_vec(v, top, &buf[buf_read_pos], first_bit);

            buf_read_pos = 0;
            buf_unread -= first_bit;
            to_copy -= first_bit;
            copied = first_bit;
        }

        memcpy_vec(v, top, &buf[buf_read_pos], to_copy);
        buf_unread -= to_copy;
        copied += to_copy;

        if (buf_unread == 0 && !eof_pending && !server_closed)
            signalled = false;

        if (!not_full_event.signalled)
            not_full_event.set_holding_sync_lock();
    }

    read_bytes = copied;
    return nt_status::SUCCESS;
}

void pipe::clear() {
    sync_lock sl;
    lock_guard l(sl);
    lock_guard l2(lock);

    buf_write_pos = 0;
    buf_unread = 0;
    signalled = false;

    if (not_full_event.signalled) {
        not_full_event.signalled = false;
        wake_up_waiters(not_full_event.waiters, WAIT_TYPE_READING);
    }
}

export void add_to_flushable_pipes_list(pipe& p) {
    lock_guard lock(flushable_pipes_lock);

    insert_tail_list(flushable_pipes, p.flushable_le);
}

export void shutdown_pipes() {
    while (true) {
        pipe* p;

        {
            lock_guard lock(flushable_pipes_lock);

            if (is_list_empty(flushable_pipes))
                return;

            p = (pipe*)((uint8_t*)flushable_pipes.next - offsetof(pipe, flushable_le));
            remove_entry_list(p->flushable_le);
        }

        p->flush(true);
    }
}

extern "C"
void trace_NtCreateNamedPipeFile(uintptr_t* args) {
    auto desired_access = (ACCESS_MASK)args[0];
    auto* object_attributes = (OBJECT_ATTRIBUTES*)args[1];
    auto create_disposition = (uint32_t)args[2];

    start_deferred_print();

    print("NtCreateNamedPipeFile: desired_access = ");
    print_file_access_mask(desired_access);
    print(", object_attributes = {}, create_disposition = ", object_attributes);
    print_create_disposition(create_disposition);
    print("\n");

    end_deferred_print();
}

extern "C"
NTSTATUS NtCreateNamedPipeFile(HANDLE* file_handle, ACCESS_MASK desired_access,
                               const OBJECT_ATTRIBUTES* object_attributes,
                               IO_STATUS_BLOCK* iosb, uint32_t /*share_access*/,
                               uint32_t create_disposition, uint32_t /*create_options*/,
                               uint32_t /*named_pipe_type*/, uint32_t /*read_mode*/,
                               uint32_t /*completion_mode*/, uint32_t /*maximum_instances*/,
                               uint32_t /*inbound_quota*/, uint32_t /*outbound_quota*/,
                               const LARGE_INTEGER* /*default_timeout*/) {
    nt_status status;
    auto t = get_current_thread();

    TRACE3("NtCreateNamedPipeFile", desired_access, object_attributes,
           create_disposition);

    // FIXME - share_access
    // FIXME - create_options
    // FIXME - named_pipe_type
    // FIXME - read_mode
    // FIXME - completion_mode
    // FIXME - maximum_instances
    // FIXME - inbound_quota
    // FIXME - outbound_quote
    // FIXME - default_timeout

    switch (create_disposition) {
        case FILE_SUPERSEDE:
            print("FIXME - FILE_SUPERSEDE\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_OPEN:
            print("FIXME - FILE_OPEN\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_OPEN_IF:
            print("FIXME - FILE_OPEN_IF\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_OVERWRITE:
            print("FIXME - FILE_OVERWRITE\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_OVERWRITE_IF:
            print("FIXME - FILE_OVERWRITE_IF\n");
            return nt_status::NOT_IMPLEMENTED;

        case FILE_CREATE:
            break;

        default:
            print("unrecognized create disposition {}\n", create_disposition);
            return nt_status::INVALID_PARAMETER;
    }

    if (!object_attributes->ObjectName || !object_attributes->ObjectName->Buffer)
        return nt_status::INVALID_PARAMETER;

    // FIXME - check desired_access

    auto u16name = u16string_view(object_attributes->ObjectName->Buffer, object_attributes->ObjectName->Length / sizeof(char16_t));

    if (u16name.empty())
        return nt_status::INVALID_PARAMETER;

    auto namelen = utf16_to_utf8_len(u16name);

    if (namelen == 0)
        return nt_status::INVALID_PARAMETER;

    auto name = (char*)malloc(namelen);
    if (!name)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(name, namelen), u16name);

    for (unsigned int i = 0; i < namelen; i++) {
        if (name[i] == '\\')
            name[i] = '/';
    }

    // FIXME - shouldn't be in /dev?
    static const string_view dev_prefix = "/dev/";
    object_ref obj_ref;

    if (namelen < dev_prefix.size() || string_view(name, dev_prefix.size()) != dev_prefix) {
        free(name);
        return nt_status::OBJECT_NAME_NOT_FOUND;
    }

    // FIXME - pipe size
    auto pptr_recv = new pipe(1);
    if (!pptr_recv) {
        print("new failed\n");
        free(name);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj = object_ref{*pptr_recv};

    // FIXME - pipe size
    auto pptr_send = new pipe(1);
    if (!pptr_send) {
        print("new failed\n");
        free(name);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj2 = object_ref{*pptr_send};

    auto pptr_client = new named_duplex_pipe_client(*pptr_recv, *pptr_send);
    if (!pptr_client) {
        print("new failed\n");
        free(name);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj3 = object_ref{*pptr_client};

    auto pptr_server = new named_duplex_pipe_server(*pptr_client);

    if (!pptr_server) {
        print("new failed\n");
        free(name);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj4 = object_ref{*pptr_server};

    status = name_object(*pptr_client, string_view(name + dev_prefix.size(), namelen - dev_prefix.size()),
                         true);
    if (!success(status)) {
        print("name_object returned {}\n", status);
        free(name);
        return status;
    }

    free(name);

    pptr_server->acquire();
    pptr_client->server = *pptr_server;

    handle_t h;

    auto f = new file(*pptr_server, 0);
    if (!f) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto obj5 = object_ref{*f};

    status = t->proc->add_handle(*f, desired_access, true, h);
    if (!success(status))
        return status;

    *file_handle = h;
    iosb->Information = FILE_CREATED;

    return nt_status::SUCCESS;
}

void named_duplex_pipe_client::last_handle_closed() {
    if (!server)
        return;

    auto& s = static_cast<named_duplex_pipe_server&>(server.get());

    s.client_disconnected();
    server.reset();
}
