// SPDX-License-Identifier: GPL-2.0-only
module;

#include <span>
#include <mutex>
#include <algorithm>
#include <optional>
#include <string_view>
#include <stdint.h>
#include <sys/un.h>

module net;

import print;
import config;
import vec;
import scheduler;
import event;
import visit;
import thread;
import handle;
import vfs;

using namespace std;

class unix_socket : public socket {
public:
    unix_socket();
    ~unix_socket();

    void last_handle_closed() override;

    nt_status recvfrom(span<const span<uint8_t>> v, int flags,
                       sockaddr_storage* from, size_t* fromlen,
                       uint64_t& recv_bytes, span<HANDLE> handles,
                       size_t& num_handles) override {
        return do_recvfrom<false>(v, flags, from, fromlen, recv_bytes,
                                  handles, num_handles);
    }

    nt_status sendto(span<const span<const uint8_t>> v, int flags, sockaddr* to,
                     size_t tolen, uint64_t& sent_bytes,
                     span<const HANDLE> handles) override {
        return do_sendto<false>(v, flags, to, tolen, sent_bytes, handles);
    }

    nt_status bind(const sockaddr* addr, size_t addrlen) override;
    nt_status connect(const sockaddr* addr, size_t addrlen) override;
    nt_status listen(int backlog) override;
    nt_status accept(HANDLE& sock, void* addr, int& addrlen) override;
    nt_status getsockname(span<uint8_t>, size_t&) override;
    nt_status getpeername(span<uint8_t> buf, size_t& len) override;

    nt_status recvfrom_nonblock(span<const span<uint8_t>> v, int flags,
                                sockaddr_storage* from, size_t* fromlen,
                                uint64_t& recv_bytes, span<HANDLE> handles,
                                size_t& num_handles) override {
        return do_recvfrom<true>(v, flags, from, fromlen, recv_bytes,
                                 handles, num_handles);
    }

    nt_status sendto_nonblock(span<const span<const uint8_t>> v, int flags,
                              sockaddr* to, size_t tolen,
                              uint64_t& sent_bytes,
                              span<const HANDLE> handles) override {
        return do_sendto<true>(v, flags, to, tolen, sent_bytes, handles);
    }

    nt_status shutdown(int how) override;

    spinlock lock;
    unix_socket* other_end = nullptr;
    bool dead = false, other_end_dead = false;
    bool first;
    event not_full_event{false};
    list_entry sent_fds = { &sent_fds, &sent_fds };

    uint8_t buf[PAGE_SIZE];
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;

private:
    template<bool NONBLOCK>
    nt_status do_recvfrom(span<const span<uint8_t>> v, int flags,
                          sockaddr_storage* from, size_t* fromlen,
                          uint64_t& recv_bytes, span<HANDLE> handles,
                          size_t& num_handles);

    template<bool NONBLOCK>
    nt_status do_sendto(span<const span<const uint8_t>> v, int flags,
                        sockaddr* to, size_t tolen, uint64_t& sent_bytes,
                        span<const HANDLE> handles);
};

struct sent_fd {
    sent_fd(object& obj) : obj(obj) {
        obj.acquire();
    }

    ~sent_fd() {
        object_release(obj);
    }

    list_entry le;
    object& obj;
    ACCESS_MASK access_mask;
    size_t bytes_left;
};

nt_status create_new_unix_socket(object_ref& ref) {
    auto s = new unix_socket;
    if (!s) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref = object_ref{*s};

    return nt_status::SUCCESS;
}

template<bool NONBLOCK>
nt_status unix_socket::do_recvfrom(span<const span<uint8_t>> v, int flags,
                                   sockaddr_storage*, size_t*,
                                   uint64_t& recv_bytes, span<HANDLE> handles,
                                   size_t& num_handles) {
    nt_status status;
    auto& t = *get_current_thread();
    size_t copied = 0;
    size_t vec_size = get_vec_size(v);

    if (!other_end)
        return nt_status::INVALID_PARAMETER;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    // FIXME - from and fromlen?

    if (vec_size == 0) {
        recv_bytes = 0;
        return nt_status::SUCCESS;
    }

    span<uint8_t> top = v.front();

    while (true) {
        if constexpr (NONBLOCK) {
            lock_guard lg(lock);

            if (buf_unread == 0 && !other_end_dead)
                return nt_status::CANT_WAIT;
        } else {
            auto status = wait_for_object(*this, true);

            if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED) {
                recv_bytes = 0;
                return status;
            }
        }

        list_entry fds = { &fds, &fds };

        {
            sync_lock sl;
            lock_guard l(sl);
            lock_guard l2(lock);

            if (buf_unread == 0) {
                if (other_end_dead)
                    break;

                continue;
            }

            size_t buf_read_pos = buf_write_pos + sizeof(buf) - buf_unread;

            while (buf_read_pos >= sizeof(buf)) {
                buf_read_pos -= sizeof(buf);
            }

            size_t to_copy = min(vec_size, buf_unread);

            if (!is_list_empty(sent_fds)) {
                auto& f = containing_record<sent_fd>(sent_fds.next);

                if (f.bytes_left > 0)
                    to_copy = min(to_copy, f.bytes_left);
            }

            while (!is_list_empty(sent_fds)) {
                auto& f = containing_record<sent_fd>(sent_fds.next);

                if (f.bytes_left < to_copy) {
                    remove_entry_list(f.le);
                    insert_tail_list(fds, f.le);
                } else
                    break;
            }

            for (auto le = sent_fds.next; le != &sent_fds; le = le->next) {
                auto& f = containing_record<sent_fd>(le);

                f.bytes_left -= to_copy;
            }

            if (buf_read_pos + to_copy > sizeof(buf)) {
                auto first_bit = sizeof(buf) - buf_read_pos;

                memcpy_vec(v, top, &buf[buf_read_pos], first_bit);

                buf_read_pos = 0;
                buf_unread -= first_bit;
                to_copy -= first_bit;
                copied = first_bit;
            }

            memcpy_vec(v, top, &buf[buf_read_pos], to_copy);
            buf_unread -= to_copy;
            copied += to_copy;

            if (buf_unread == 0)
                signalled = false;

            if (!not_full_event.signalled)
                not_full_event.set_holding_sync_lock();

            if (!other_end->write_signalled) {
                other_end->write_signalled = true;
                wake_up_waiters(other_end->waiters, WAIT_TYPE_WRITING);
            }
        }

        num_handles = 0;

        while (!is_list_empty(fds)) {
            auto& f = containing_record<sent_fd>(fds.next);

            remove_entry_list(f.le);

            if (num_handles < handles.size()) {
                handle_t h;

                status = t.proc->add_handle(f.obj, f.access_mask, true, h);
                if (!success(status)) {
                    delete &f;

                    // FIXME - close handles already done

                    while (!is_list_empty(fds)) {
                        auto& f2 = containing_record<sent_fd>(fds.next);

                        remove_entry_list(f2.le);
                        delete &f2;
                    }

                    return status;
                }

                handles[num_handles] = h;
            }

            num_handles++;
            delete &f;
        }

        break;
    }

    recv_bytes = copied;
    return nt_status::SUCCESS;
}

template<bool NONBLOCK>
nt_status unix_socket::do_sendto(span<const span<const uint8_t>> v, int flags,
                                 sockaddr*, size_t, uint64_t& sent_bytes,
                                 span<const HANDLE> handles) {
    if (!other_end)
        return nt_status::INVALID_PARAMETER;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    size_t vec_size = get_vec_size(v);

    if (vec_size == 0) {
        sent_bytes = 0;

        if (!handles.empty())
            return nt_status::INVALID_PARAMETER;

        return nt_status::SUCCESS;
    }

    list_entry fds = { &fds, &fds };

    for (auto h : handles) {
        ACCESS_MASK access_mask;

        auto obj = deref_handle(h, access_mask);

        if (!obj) {
            while (!is_list_empty(fds)) {
                auto& f = containing_record<sent_fd>(fds.next);

                remove_entry_list(f.le);
                delete &f;
            }

            return nt_status::INVALID_HANDLE;
        }

        auto f = new sent_fd(obj.get());
        if (!f) {
            while (!is_list_empty(fds)) {
                auto& f = containing_record<sent_fd>(fds.next);

                remove_entry_list(f.le);
                delete &f;
            }

            return nt_status::INSUFFICIENT_RESOURCES;
        }

        f->access_mask = access_mask;

        insert_tail_list(fds, f->le);
    }

    span<const uint8_t> top = v.front();

    sent_bytes = 0;

    do {
        size_t to_copy;

        {
            lock_guard l(other_end->lock);

            size_t buf_left = sizeof(other_end->buf) - other_end->buf_unread;

            if (other_end->dead) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd>(fds.next);

                    remove_entry_list(f.le);
                    delete &f;
                }

                if (sent_bytes == 0)
                    return nt_status::PIPE_DISCONNECTED;
                else
                    break;
            }

            to_copy = min(vec_size, buf_left);

            if (to_copy > 0) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd>(fds.next);

                    f.bytes_left = other_end->buf_unread;

                    remove_entry_list(f.le);
                    insert_tail_list(other_end->sent_fds, f.le);
                }

                if (other_end->buf_write_pos + to_copy > sizeof(other_end->buf)) {
                    auto first_bit = sizeof(other_end->buf) - other_end->buf_write_pos;

                    memcpy_vec(&other_end->buf[other_end->buf_write_pos], v, top,
                               first_bit);

                    other_end->buf_write_pos = 0;
                    other_end->buf_unread += first_bit;
                    to_copy -= first_bit;
                    vec_size -= first_bit;
                    sent_bytes += first_bit;
                }

                memcpy_vec(&other_end->buf[other_end->buf_write_pos], v, top,
                           to_copy);

                other_end->buf_write_pos += to_copy;
                other_end->buf_unread += to_copy;
                vec_size -= to_copy;
                sent_bytes += to_copy;
            }
        }

        if (to_copy > 0) {
            // wake up waiting thread

            sync_lock sl;
            lock_guard l(sl);

            if (!other_end->signalled) {
                other_end->signalled = true;

                wake_up_waiters(other_end->waiters, WAIT_TYPE_READING);
            }

            {
                lock_guard l2(other_end->lock);

                if (other_end->buf_unread == sizeof(other_end->buf) && !other_end->dead) {
                    other_end->not_full_event.signalled = false;
                    write_signalled = false;
                }
            }
        }

        if (vec_size == 0)
            break;

        if constexpr (NONBLOCK) {
            while (!is_list_empty(fds)) {
                auto& f = containing_record<sent_fd>(fds.next);

                remove_entry_list(f.le);
                delete &f;
            }

            if (sent_bytes == 0)
                return nt_status::CANT_WAIT;
            else
                break;
        } else {
            // block while we wait for space to become available
            auto status = wait_for_object(other_end->not_full_event, true);

            if (status == nt_status::USER_APC || status == nt_status::ALERTED || !success(status)) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd>(fds.next);

                    remove_entry_list(f.le);
                    delete &f;
                }

                if (status == nt_status::USER_APC) {
                    if (sent_bytes == 0)
                        return status;
                    else
                        break;
                } else if (!success(status) || status == nt_status::ALERTED)
                    return status;
            }
        }
    } while (true);

    return nt_status::SUCCESS;
}

unix_socket::unix_socket() {
    signalled = false;
    not_full_event.signalled = true;
}

unix_socket::~unix_socket() {
    while (!is_list_empty(sent_fds)) {
        auto& f = containing_record<sent_fd>(sent_fds.next);

        remove_entry_list(f.le);
        delete &f;
    }
}

void unix_socket::last_handle_closed() {
    if (other_end) {
        bool both_dead;

        {
            auto& first_lock = first ? lock : other_end->lock;
            auto& second_lock = first ? other_end->lock : lock;

            sync_lock sl;
            lock_guard lg1(sl);
            lock_guard lg2(first_lock);
            lock_guard lg3(second_lock);

            dead = true;

            if (!other_end->signalled) {
                other_end->signalled = true;
                wake_up_waiters(other_end->waiters, WAIT_TYPE_READING);
            }

            if (!other_end->write_signalled) {
                other_end->write_signalled = true;
                wake_up_waiters(other_end->waiters, WAIT_TYPE_WRITING);
            }

            not_full_event.set_holding_sync_lock();

            other_end->other_end_dead = true;
            both_dead = other_end->dead;
        }

        if (both_dead) {
            other_end->release();
            release();
        }
    }
}

nt_status unix_socket::bind(const sockaddr* addr, size_t addrlen) {
    auto& t = *get_current_thread();
    nt_status status;
    object_ref obj;
    unsigned int create_result;
    optional<reference_wrapper<dentry>> drw;
    uintptr_t us_proc_params;
    uint32_t read_bytes;
    HANDLE curdir;

    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    auto& proc = *t.proc;

    // parse addr

    if (addr->sa_family != AF_UNIX)
        return nt_status::INVALID_PARAMETER;

    if (addrlen < offsetof(sockaddr_un, sun_path))
        return nt_status::INVALID_PARAMETER;

    auto& un = *(sockaddr_un*)addr;

    string_view path(un.sun_path, min(addrlen, sizeof(sockaddr_un)) - offsetof(sockaddr_un, sun_path));

    if (auto zero = path.find('\0'); zero != string_view::npos)
        path = string_view(path.data(), zero);

    // get current directory

    status = proc.read_vm((uint8_t*)proc.peb + offsetof(PEB, ProcessParameters),
                          &us_proc_params, sizeof(us_proc_params), &read_bytes);
    if (!success(status))
        return status;

    if (read_bytes != sizeof(us_proc_params))
        return nt_status::NO_MEMORY;

    status = proc.read_vm((uint8_t*)us_proc_params + offsetof(RTL_USER_PROCESS_PARAMETERS, CurrentDirectory.Handle),
                          &curdir, sizeof(HANDLE), &read_bytes);

    if (!success(status))
        return status;

    if (read_bytes != sizeof(HANDLE))
        return nt_status::NO_MEMORY;

    auto curdirobj = deref_handle(curdir);

    if (!curdirobj)
        return nt_status::INVALID_HANDLE;

    if (curdirobj.get().type != object_type::file)
        return nt_status::OBJECT_TYPE_MISMATCH;

    auto& curdirf = static_cast<file&>(curdirobj.get());

    if (!curdirf.dentry)
        return nt_status::INVALID_PARAMETER;

    // create inode

    status = open_inode(path, *curdirf.dentry, obj, drw, FILE_CREATE,
                        FILE_NON_DIRECTORY_FILE, create_result, false, true);
    if (status == nt_status::OBJECT_NAME_COLLISION)
        return nt_status::ADDRESS_ALREADY_ASSOCIATED;
    else if (!success(status))
        return status;

    if (drw)
        drw->get().release();

    // FIXME - record as bound

    return nt_status::SUCCESS;
}

nt_status unix_socket::connect(const sockaddr* /*addr*/, size_t /*addrlen*/) {
    print("FIXME - unix_socket::connect\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::listen(int /*backlog*/) {
    print("FIXME - unix_socket::listen\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::accept(HANDLE& /*sock*/, void* /*addr*/, int& /*addrlen*/) {
    print("FIXME - unix_socket::accept\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::getsockname(span<uint8_t>, size_t&) {
    print("FIXME - unix_socket::getsockname\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::getpeername(span<uint8_t> /*buf*/, size_t& /*len*/) {
    print("FIXME - unix_socket::getpeername\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status unix_socket::shutdown(int /*how*/) {
    print("FIXME - unix_socket::shutdown\n");

    return nt_status::NOT_IMPLEMENTED;
}

nt_status create_unix_socket_pair(int type, object_ref& ref1, object_ref& ref2) {
    if (type != SOCK_STREAM) // FIXME - support SOCK_DGRAM too
        return nt_status::INVALID_PARAMETER;

    auto s1 = new unix_socket;
    if (!s1) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref1 = object_ref{*s1};

    auto s2 = new unix_socket;
    if (!s2) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref2 = object_ref{*s2};

    s1->other_end = s2;
    s1->first = true;

    s2->other_end = s1;
    s2->first = false;

    s1->acquire();
    s2->acquire();

    return nt_status::SUCCESS;
}
