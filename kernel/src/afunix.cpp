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
import rwsem;
import process;

using namespace std;

static constexpr size_t MAX_DGRAM_SIZE = PAGE_SIZE;
static constexpr size_t MAX_DGRAM_MSGS = 10;

static spinlock bound_sockets_lock;
static list_entry bound_sockets = { &bound_sockets, &bound_sockets };

enum class unix_socket_type {
    stream,
    dgram,
    seqpacket,
};

enum class unix_socket_state {
    closed,
    connected,
    listening,
    awaiting_accept,
};

class base_unix_socket : public socket {
public:
    base_unix_socket(unix_socket_type socket_type) : socket_type(socket_type) { }

    unix_socket_type socket_type;
    spinlock lock;
    bool dead = false, other_end_dead = false;
    bool first;
    event not_full_event{false};
    rwsem state_lock;
    bool bind_done = false;
    object_ref bound_inode;
    list_entry bound_le;
    unique_ptr<char[]> bind_name;
    object_ref accept_queue_not_full_event;
    unsigned int listen_backlog;
    unix_socket_state state = unix_socket_state::closed;
    list_entry accept_le;
    spinlock accept_queue_lock;
    list_entry accept_queue = { &accept_queue, &accept_queue };
    bool accept_done = false;
    bool shutdown_read = false;
    bool shutdown_write = false;
};

template<unix_socket_type TYPE>
struct socket_buffer;

template<>
struct socket_buffer<unix_socket_type::stream> {
    uint8_t buf[PAGE_SIZE];
    size_t buf_unread = 0;
    size_t buf_write_pos = 0;
    list_entry sent_fds = { &sent_fds, &sent_fds };
};

template<unix_socket_type TYPE>
struct sock_msg;

template<>
struct sock_msg<unix_socket_type::dgram> {
    static void* operator new(size_t size, size_t extra) {
        return ::operator new(size + extra);
    }

    static void operator delete(void* p) {
        ::operator delete(p);
    }

    list_entry le;
    size_t size;
    list_entry sent_fds = { &sent_fds, &sent_fds };
    unique_ptr<char[]> name;
    uint8_t buf[];
};

template<>
struct sock_msg<unix_socket_type::seqpacket> {
    static void* operator new(size_t size, size_t extra) {
        return ::operator new(size + extra);
    }

    static void operator delete(void* p) {
        ::operator delete(p);
    }

    list_entry le;
    size_t size;
    list_entry sent_fds = { &sent_fds, &sent_fds };
    uint8_t buf[];
};

template<>
struct socket_buffer<unix_socket_type::dgram> {
    list_entry msgs = { &msgs, &msgs };
    size_t msgs_unread = 0;
};

template<>
struct socket_buffer<unix_socket_type::seqpacket> {
    list_entry msgs = { &msgs, &msgs };
    size_t msgs_unread = 0;
};

template<unix_socket_type TYPE>
class unix_socket : public base_unix_socket {
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

    nt_status connect(const sockaddr* addr, size_t addrlen) override {
        return do_connect<false>(addr, addrlen);
    }

    nt_status connect_nonblock(const sockaddr* addr, size_t addrlen) override {
        return do_connect<true>(addr, addrlen);
    }

    nt_status listen(int backlog) override;

    nt_status accept(HANDLE& sock, void* addr, int& addrlen) override {
        return do_accept<false>(sock, addr, addrlen);
    }

    nt_status accept_nonblock(HANDLE& sock, void* addr, int& addrlen) override {
        return do_accept<true>(sock, addr, addrlen);
    }

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

    unix_socket<TYPE>* other_end = nullptr;
    unix_socket<TYPE>* server = nullptr;
    socket_buffer<TYPE> buf;

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

    template<bool NONBLOCK>
    nt_status do_accept(HANDLE& sock, void* addr, int& addrlen);

    template<bool NONBLOCK>
    nt_status do_connect(const sockaddr* addr, size_t addrlen);
};

struct sent_fd_stream {
    sent_fd_stream(object& obj) : obj(obj) {
        obj.acquire();
    }

    ~sent_fd_stream() {
        object_release(obj);
    }

    list_entry le;
    object& obj;
    ACCESS_MASK access_mask;
    size_t bytes_left;
};

struct sent_fd_dgram {
    sent_fd_dgram(object& obj) : obj(obj) {
        obj.acquire();
    }

    ~sent_fd_dgram() {
        object_release(obj);
    }

    list_entry le;
    object& obj;
    ACCESS_MASK access_mask;
};

nt_status create_new_unix_socket(object_ref& ref, int type) {
    switch (type) {
        case SOCK_STREAM: {
            auto s = new unix_socket<unix_socket_type::stream>;
            if (!s) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            ref = object_ref{*s};
            break;
        }

        case SOCK_DGRAM: {
            auto s = new unix_socket<unix_socket_type::dgram>;
            if (!s) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            ref = object_ref{*s};
            break;
        }

        case SOCK_SEQPACKET: {
            auto s = new unix_socket<unix_socket_type::seqpacket>;
            if (!s) {
                print("new failed\n");
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            ref = object_ref{*s};
            break;
        }

        default:
            return nt_status::INVALID_PARAMETER;
    }

    return nt_status::SUCCESS;
}

template<unix_socket_type TYPE>
template<bool NONBLOCK>
nt_status unix_socket<TYPE>::do_recvfrom(span<const span<uint8_t>> v, int flags,
                                         sockaddr_storage* from, size_t* fromlen,
                                         uint64_t& recv_bytes, span<HANDLE> handles,
                                         size_t& num_handles) {
    nt_status status;
    auto& t = *get_current_thread();
    size_t copied = 0;
    size_t vec_size = get_vec_size(v);

    if constexpr (TYPE == unix_socket_type::stream || TYPE == unix_socket_type::seqpacket) {
        if (!other_end)
            return nt_status::INVALID_PARAMETER;
    }

    if (flags & ~MSG_PEEK)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(state_lock);

        if (state != unix_socket_state::connected)
            return nt_status::CONNECTION_DISCONNECTED;

        if (shutdown_read) {
            recv_bytes = 0;
            return nt_status::SUCCESS;
        }
    }

    // FIXME - from and fromlen?

    if constexpr (TYPE == unix_socket_type::stream) {
        if (vec_size == 0) {
            recv_bytes = 0;
            return nt_status::SUCCESS;
        }
    }

    span<uint8_t> top;

    if (!v.empty())
        top = v.front();

    while (true) {
        if constexpr (NONBLOCK) {
            lock_guard lg(lock);

            if (!other_end || (!other_end_dead && !other_end->shutdown_write)) {
                if constexpr (TYPE == unix_socket_type::stream) {
                    if (buf.buf_unread == 0)
                        return nt_status::CANT_WAIT;
                } else if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
                    if (buf.msgs_unread == 0)
                        return nt_status::CANT_WAIT;
                }
            }
        } else {
            auto status = wait_for_object(*this, true);

            if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED) {
                recv_bytes = 0;
                return status;
            }
        }

        list_entry fds = { &fds, &fds };
        char msgname[sizeof(sockaddr_un::sun_path) + 1];

        {
            sync_lock sl;
            lock_guard l(sl);
            lock_guard l2(lock);

            if constexpr (TYPE == unix_socket_type::stream) {
                if (buf.buf_unread == 0) {
                    if (other_end_dead || other_end->shutdown_write)
                        break;

                    continue;
                }

                size_t buf_read_pos = buf.buf_write_pos + sizeof(buf.buf) - buf.buf_unread;

                while (buf_read_pos >= sizeof(buf.buf)) {
                    buf_read_pos -= sizeof(buf.buf);
                }

                size_t to_copy = min(vec_size, buf.buf_unread);

                if (!(flags & MSG_PEEK)) {
                    if (!is_list_empty(buf.sent_fds)) {
                        auto& f = containing_record<sent_fd_stream>(buf.sent_fds.next);

                        if (f.bytes_left > 0)
                            to_copy = min(to_copy, f.bytes_left);
                    }

                    while (!is_list_empty(buf.sent_fds)) {
                        auto& f = containing_record<sent_fd_stream>(buf.sent_fds.next);

                        if (f.bytes_left < to_copy) {
                            remove_entry_list(f.le);
                            insert_tail_list(fds, f.le);
                        } else
                            break;
                    }

                    for (auto le = buf.sent_fds.next; le != &buf.sent_fds; le = le->next) {
                        auto& f = containing_record<sent_fd_stream>(le);

                        f.bytes_left -= to_copy;
                    }
                }

                if (buf_read_pos + to_copy > sizeof(buf.buf)) {
                    auto first_bit = sizeof(buf.buf) - buf_read_pos;

                    memcpy_vec(v, top, &buf.buf[buf_read_pos], first_bit);

                    buf_read_pos = 0;
                    to_copy -= first_bit;
                    copied = first_bit;

                    if (!(flags & MSG_PEEK))
                        buf.buf_unread -= first_bit;
                }

                memcpy_vec(v, top, &buf.buf[buf_read_pos], to_copy);
                copied += to_copy;

                if (!(flags & MSG_PEEK)) {
                    buf.buf_unread -= to_copy;

                    if (buf.buf_unread == 0)
                        signalled = false;

                    if (!not_full_event.signalled)
                        not_full_event.set_holding_sync_lock();

                    if (!other_end->write_signalled) {
                        other_end->write_signalled = true;
                        wake_up_waiters(other_end->waiters, WAIT_TYPE_WRITING);
                    }
                }
            } else if (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
                if (is_list_empty(buf.msgs)) {
                    if (other_end && (other_end_dead || other_end->shutdown_write))
                        break;

                    continue;
                }

                auto& msg = containing_record<sock_msg<TYPE>>(buf.msgs.next);

                size_t to_copy = min(vec_size, msg.size);

                if (!(flags & MSG_PEEK)) {
                    while (!is_list_empty(msg.sent_fds)) {
                        auto& f = containing_record<sent_fd_dgram>(msg.sent_fds.next);

                        remove_entry_list(f.le);
                        insert_tail_list(fds, f.le);
                    }
                }

                memcpy_vec(v, top, msg.buf, to_copy);
                copied += to_copy;

                if constexpr (TYPE == unix_socket_type::dgram) {
                    if (msg.name && msg.name[0])
                        strcpy(msgname, msg.name.get());
                    else
                        msgname[0] = 0;
                }

                if (!(flags & MSG_PEEK)) {
                    remove_entry_list(msg.le);

                    buf.msgs_unread--;

                    if (buf.msgs_unread == 0)
                        signalled = false;

                    if (!not_full_event.signalled)
                        not_full_event.set_holding_sync_lock();

                    if (other_end && !other_end->write_signalled) {
                        other_end->write_signalled = true;
                        wake_up_waiters(other_end->waiters, WAIT_TYPE_WRITING);
                    }

                    delete &msg;
                }
            }
        }

        num_handles = 0;

        while (!is_list_empty(fds)) {
            if constexpr (TYPE == unix_socket_type::stream) {
                auto& f = containing_record<sent_fd_stream>(fds.next);

                remove_entry_list(f.le);

                if (num_handles < handles.size()) {
                    handle_t h;

                    status = t.proc->add_handle(f.obj, f.access_mask, true, h);
                    if (!success(status)) {
                        delete &f;

                        // FIXME - close handles already done

                        while (!is_list_empty(fds)) {
                            auto& f2 = containing_record<sent_fd_stream>(fds.next);

                            remove_entry_list(f2.le);
                            delete &f2;
                        }

                        return status;
                    }

                    handles[num_handles] = h;
                }

                num_handles++;
                delete &f;
            } else if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
                auto& f = containing_record<sent_fd_dgram>(fds.next);

                remove_entry_list(f.le);

                if (num_handles < handles.size()) {
                    handle_t h;

                    status = t.proc->add_handle(f.obj, f.access_mask, true, h);
                    if (!success(status)) {
                        delete &f;

                        // FIXME - close handles already done

                        while (!is_list_empty(fds)) {
                            auto& f2 = containing_record<sent_fd_dgram>(fds.next);

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
        }

        if (from && fromlen && *fromlen >= sizeof(sa_family_t)) {
            auto& addr = *(sockaddr_un*)from;
            size_t namelen = 0;

            addr.sun_family = AF_UNIX;

            if constexpr (TYPE == unix_socket_type::stream || TYPE == unix_socket_type::seqpacket) {
                lock_guard lock(other_end->state_lock);

                if (other_end->bind_name && other_end->bind_name[0] != 0) {
                    namelen = min(sizeof(sockaddr_un) - offsetof(sockaddr_un, sun_path),
                                    strlen(other_end->bind_name.get()) + 1);
                    memcpy(addr.sun_path, other_end->bind_name.get(), namelen);
                }
            } else {
                if (msgname[0] != 0) {
                    namelen = min(sizeof(sockaddr_un) - offsetof(sockaddr_un, sun_path),
                                  strlen(msgname) + 1);
                    memcpy(addr.sun_path, msgname, namelen);
                }
            }

            *fromlen = offsetof(sockaddr_un, sun_path) + namelen;
        }

        break;
    }

    recv_bytes = copied;
    return nt_status::SUCCESS;
}

static nt_status open_socket(process& proc, string_view path, object_ref& sock) {
    nt_status status;
    optional<reference_wrapper<dentry>> drw;
    uintptr_t us_proc_params;
    uint32_t read_bytes;
    HANDLE curdir;
    object_ref obj;
    unsigned int create_result;

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

    // open socket inode

    status = open_inode(path, *curdirf.dentry, obj, drw, FILE_OPEN,
                        FILE_NON_DIRECTORY_FILE, create_result, false);
    if (!success(status))
        return status;

    if (drw)
        drw->get().release();

    // FIXME - check permissions

    if (obj.get().type != object_type::inode)
        return nt_status::CONNECTION_REFUSED;

    auto& ino = static_cast<inode&>(obj.get());

    if (ino.inode_type != inode_type::socket)
        return nt_status::CONNECTION_REFUSED;

    {
        lock_guard lock(bound_sockets_lock);

        for (auto le = bound_sockets.next; le != &bound_sockets; le = le->next) {
            auto& s = *(base_unix_socket*)((uint8_t*)le - offsetof(base_unix_socket, bound_le));

            if (s.bind_done && s.bound_inode && &s.bound_inode.get() == &ino) {
                s.acquire();
                sock = s;
                break;
            }
        }
    }

    if (!sock)
        return nt_status::CONNECTION_REFUSED;

    return nt_status::SUCCESS;
}

template<unix_socket_type TYPE>
template<bool NONBLOCK>
nt_status unix_socket<TYPE>::do_sendto(span<const span<const uint8_t>> v, int flags,
                                       sockaddr* to, size_t tolen, uint64_t& sent_bytes,
                                       span<const HANDLE> handles) {
    nt_status status;
    object_ref objref;

    if (flags != 0)
        return nt_status::INVALID_PARAMETER;

    {
        lock_guard lock(state_lock);

        if (shutdown_write)
            return nt_status::PIPE_BROKEN;

        if constexpr (TYPE != unix_socket_type::dgram) {
            if (!other_end)
                return nt_status::INVALID_PARAMETER;

            if (state != unix_socket_state::connected)
                return nt_status::CONNECTION_DISCONNECTED;
        }

        if (state == unix_socket_state::connected) {
            objref = *other_end;
            other_end->acquire();
        }
    }

    if constexpr (TYPE == unix_socket_type::dgram) {
        if (to) {
            auto& t = *get_current_thread();

            if (!t.proc)
                return nt_status::INVALID_PARAMETER;

            if (tolen < sizeof(sa_family_t))
                return nt_status::INVALID_PARAMETER;

            auto& un = *(sockaddr_un*)to;

            if (un.sun_family != AF_UNIX)
                return nt_status::INVALID_PARAMETER;

            string_view path(un.sun_path, min(tolen, sizeof(sockaddr_un)) - offsetof(sockaddr_un, sun_path));

            if (auto zero = path.find('\0'); zero != string_view::npos)
                path = string_view(path.data(), zero);

            objref.reset();

            status = open_socket(*t.proc, path, objref);
            if (!success(status))
                return status;

            if (static_cast<base_unix_socket&>(objref.get()).socket_type != TYPE)
                return nt_status::PROTOCOL_NOT_SUPPORTED;
        }

        if (!objref)
            return nt_status::CONNECTION_DISCONNECTED;
    }

    size_t vec_size = get_vec_size(v);

    if constexpr (TYPE == unix_socket_type::stream) {
        if (vec_size == 0) {
            sent_bytes = 0;

            if (!handles.empty())
                return nt_status::INVALID_PARAMETER;

            return nt_status::SUCCESS;
        }
    }

    if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
        if (vec_size > MAX_DGRAM_SIZE)
            return nt_status::INVALID_BUFFER_SIZE;
    }

    list_entry fds = { &fds, &fds };

    for (auto h : handles) {
        ACCESS_MASK access_mask;

        auto obj = deref_handle(h, access_mask);

        if constexpr (TYPE == unix_socket_type::stream) {
            if (!obj) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd_stream>(fds.next);

                    remove_entry_list(f.le);
                    delete &f;
                }

                return nt_status::INVALID_HANDLE;
            }

            auto f = new sent_fd_stream(obj.get());
            if (!f) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd_stream>(fds.next);

                    remove_entry_list(f.le);
                    delete &f;
                }

                return nt_status::INSUFFICIENT_RESOURCES;
            }

            f->access_mask = access_mask;

            insert_tail_list(fds, f->le);
        } else if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
            if (!obj) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd_dgram>(fds.next);

                    remove_entry_list(f.le);
                    delete &f;
                }

                return nt_status::INVALID_HANDLE;
            }

            auto f = new sent_fd_dgram(obj.get());
            if (!f) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd_dgram>(fds.next);

                    remove_entry_list(f.le);
                    delete &f;
                }

                return nt_status::INSUFFICIENT_RESOURCES;
            }

            f->access_mask = access_mask;

            insert_tail_list(fds, f->le);
        }
    }

    span<const uint8_t> top;
    sock_msg<TYPE>* msg = nullptr;

    if (!v.empty())
        top = v.front();

    sent_bytes = 0;

    if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
        msg = new (vec_size) sock_msg<TYPE>;
        if (!msg) {
            print("new failed\n");
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        msg->size = vec_size;
        memcpy_vec(msg->buf, v, top, vec_size);

        if constexpr (TYPE == unix_socket_type::dgram) {
            lock_guard lock(state_lock);

            if (bind_name) {
                auto len = strlen(bind_name.get());

                msg->name.reset(new char[len + 1]);
                if (!msg->name) {
                    print("new failed\n");
                    delete msg;
                    return nt_status::INSUFFICIENT_RESOURCES;
                }

                memcpy(msg->name.get(), bind_name.get(), len);
                msg->name[len] = 0;
            }
        }
    }

    auto& dest = static_cast<unix_socket<TYPE>&>(objref.get());

    do {
        bool done_copy = false;

        {
            lock_guard l(dest.lock);

            if (dest.dead || dest.shutdown_read) {
                if constexpr (TYPE == unix_socket_type::stream) {
                    while (!is_list_empty(fds)) {
                        auto& f = containing_record<sent_fd_stream>(fds.next);

                        remove_entry_list(f.le);
                        delete &f;
                    }
                } else if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
                    while (!is_list_empty(fds)) {
                        auto& f = containing_record<sent_fd_dgram>(fds.next);

                        remove_entry_list(f.le);
                        delete &f;
                    }
                }

                if (sent_bytes == 0) {
                    if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket)
                        delete msg;

                    return nt_status::PIPE_BROKEN;
                } else
                    break;
            }

            if constexpr (TYPE == unix_socket_type::stream) {
                size_t buf_left = sizeof(dest.buf.buf) - dest.buf.buf_unread;

                auto to_copy = min(vec_size, buf_left);

                if (to_copy > 0) {
                    while (!is_list_empty(fds)) {
                        auto& f = containing_record<sent_fd_stream>(fds.next);

                        f.bytes_left = dest.buf.buf_unread;

                        remove_entry_list(f.le);
                        insert_tail_list(dest.buf.sent_fds, f.le);
                    }

                    if (dest.buf.buf_write_pos + to_copy > sizeof(dest.buf.buf)) {
                        auto first_bit = sizeof(dest.buf.buf) - dest.buf.buf_write_pos;

                        memcpy_vec(&dest.buf.buf[dest.buf.buf_write_pos],
                                   v, top, first_bit);

                        dest.buf.buf_write_pos = 0;
                        dest.buf.buf_unread += first_bit;
                        to_copy -= first_bit;
                        vec_size -= first_bit;
                        sent_bytes += first_bit;
                    }

                    memcpy_vec(&dest.buf.buf[dest.buf.buf_write_pos],
                               v, top, to_copy);

                    dest.buf.buf_write_pos += to_copy;
                    dest.buf.buf_unread += to_copy;
                    vec_size -= to_copy;
                    sent_bytes += to_copy;

                    done_copy = true;
                }
            } else if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
                if (dest.buf.msgs_unread < MAX_DGRAM_MSGS) {
                    while (!is_list_empty(fds)) {
                        auto& f = containing_record<sent_fd_dgram>(fds.next);

                        remove_entry_list(f.le);
                        insert_tail_list(msg->sent_fds, f.le);
                    }

                    insert_tail_list(dest.buf.msgs, msg->le);
                    dest.buf.msgs_unread++;
                    done_copy = true;
                    sent_bytes = msg->size;
                }
            }
        }

        if (done_copy) {
            // wake up waiting thread

            sync_lock sl;
            lock_guard l(sl);

            if (!dest.signalled) {
                dest.signalled = true;

                wake_up_waiters(dest.waiters, WAIT_TYPE_READING);
            }

            {
                lock_guard l2(dest.lock);

                if (!dest.dead && !dest.shutdown_read) {
                    if constexpr (TYPE == unix_socket_type::stream) {
                        if (dest.buf.buf_unread == sizeof(dest.buf.buf)) {
                            dest.not_full_event.signalled = false;
                            write_signalled = false;
                        }
                    } else if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
                        if (dest.buf.msgs_unread == MAX_DGRAM_MSGS) {
                            dest.not_full_event.signalled = false;

                            if constexpr (TYPE != unix_socket_type::dgram)
                                write_signalled = false;
                        }
                    }
                }
            }
        }

        if (vec_size == 0 || (done_copy && (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket)))
            break;

        if constexpr (NONBLOCK) {
            if constexpr (TYPE == unix_socket_type::stream) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd_stream>(fds.next);

                    remove_entry_list(f.le);
                    delete &f;
                }
            } else if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
                while (!is_list_empty(fds)) {
                    auto& f = containing_record<sent_fd_dgram>(fds.next);

                    remove_entry_list(f.le);
                    delete &f;
                }
            }

            if (sent_bytes == 0) {
                if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket)
                    delete msg;

                return nt_status::CANT_WAIT;
            } else
                break;
        } else {
            // block while we wait for space to become available
            auto status = wait_for_object(dest.not_full_event, true);

            if (status == nt_status::USER_APC || status == nt_status::ALERTED || !success(status)) {
                if constexpr (TYPE == unix_socket_type::stream) {
                    while (!is_list_empty(fds)) {
                        auto& f = containing_record<sent_fd_stream>(fds.next);

                        remove_entry_list(f.le);
                        delete &f;
                    }
                } else if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket) {
                    while (!is_list_empty(fds)) {
                        auto& f = containing_record<sent_fd_dgram>(fds.next);

                        remove_entry_list(f.le);
                        delete &f;
                    }
                }

                if (status == nt_status::USER_APC) {
                    if (sent_bytes == 0) {
                        if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket)
                            delete msg;

                        return status;
                    } else
                        break;
                } else {
                    if constexpr (TYPE == unix_socket_type::dgram || TYPE == unix_socket_type::seqpacket)
                        delete msg;

                    return status;
                }
            }
        }
    } while (true);

    return nt_status::SUCCESS;
}

template<unix_socket_type TYPE>
unix_socket<TYPE>::unix_socket() : base_unix_socket(TYPE) {
    signalled = false;
    not_full_event.signalled = true;
}

template<unix_socket_type TYPE>
unix_socket<TYPE>::~unix_socket() {
    if constexpr (TYPE == unix_socket_type::stream) {
        while (!is_list_empty(buf.sent_fds)) {
            auto& f = containing_record<sent_fd_stream>(buf.sent_fds.next);

            remove_entry_list(f.le);
            delete &f;
        }
    } else {
        while (!is_list_empty(buf.msgs)) {
            auto& m = containing_record<sock_msg<TYPE>>(buf.msgs.next);

            remove_entry_list(m.le);

            while (!is_list_empty(m.sent_fds)) {
                auto& f = containing_record<sent_fd_dgram>(m.sent_fds.next);

                remove_entry_list(f.le);
                delete &f;
            }

            delete &m;
        }
    }
}

template<unix_socket_type TYPE>
void unix_socket<TYPE>::last_handle_closed() {
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
    } else {
        {
            lock_guard lg(lock);
            dead = true;
        }

        set_event(not_full_event);
    }

    {
        lock_guard lock(state_lock);

        if (bind_done) {
            lock_guard lock2(bound_sockets_lock);
            remove_entry_list(bound_le);
        }

        if (state == unix_socket_state::awaiting_accept) {
            bool ignore;

            {
                lock_guard lock2(server->accept_queue_lock);

                ignore = accept_done;

                if (!ignore)
                    remove_entry_list(accept_le);
            }

            if (!ignore) {
                release();
                server->release();
            }
        }

        state = unix_socket_state::closed;
    }

    while (true) {
        unix_socket* old_server = nullptr;
        unix_socket* s;

        {
            lock_guard lock(accept_queue_lock);

            if (is_list_empty(accept_queue))
                break;

            s = (unix_socket<TYPE>*)((uint8_t*)accept_queue.next - offsetof(::unix_socket<TYPE>, accept_le));

            remove_entry_list(s->accept_le);
            s->accept_done = true;
        }

        {
            lock_guard lock(s->state_lock);

            s->state = unix_socket_state::closed;
            swap(old_server, s->server);
        }

        if (old_server)
            old_server->release();

        s->release();
    }
}

template<unix_socket_type TYPE>
nt_status unix_socket<TYPE>::bind(const sockaddr* addr, size_t addrlen) {
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

    unique_ptr<char[]> name;

    name.reset(new char[path.size() + 1]);
    if (!name) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    memcpy(name.get(), path.data(), path.size());
    name[path.size()] = 0;

    {
        lock_guard lock(state_lock);

        if (bind_done)
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

        bound_inode.swap(obj);
        bind_name.swap(name);
        bind_done = true;

        {
            lock_guard lock2(bound_sockets_lock);
            insert_tail_list(bound_sockets, bound_le);
        }
    }

    return nt_status::SUCCESS;
}

template<unix_socket_type TYPE>
template<bool NONBLOCK>
nt_status unix_socket<TYPE>::do_connect(const sockaddr* addr, size_t addrlen) {
    auto& t = *get_current_thread();
    nt_status status;
    object_ref sock;

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

    status = open_socket(proc, path, sock);
    if (!success(status))
        return status;

    if (static_cast<base_unix_socket&>(sock.get()).socket_type != TYPE)
        return nt_status::PROTOCOL_NOT_SUPPORTED;

    auto& serv = static_cast<unix_socket&>(sock.get());

    {
        lock_guard lock(serv.state_lock);

        if (serv.state != unix_socket_state::listening)
            return nt_status::CONNECTION_REFUSED;
    }

    while (true) {
        if constexpr (NONBLOCK) {
            status = wait_for_object(serv.accept_queue_not_full_event.get(),
                                     true, 0);
            if (status == nt_status::TIMEOUT)
                return nt_status::CANT_WAIT;
        } else
            status = wait_for_object(serv.accept_queue_not_full_event.get(), true);

        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status) || status == nt_status::USER_APC)
            return status;

        {
            lock_guard lock1(state_lock);

            {
                lock_guard lock2(serv.state_lock);

                if (serv.state != unix_socket_state::listening)
                    return nt_status::CONNECTION_REFUSED;

                {
                    lock_guard lock3(serv.accept_queue_lock);

                    if (serv.listen_backlog == 0)
                        continue;

                    acquire();

                    serv.listen_backlog--;
                    insert_tail_list(serv.accept_queue, accept_le);
                }
            }

            state = unix_socket_state::awaiting_accept;
            server = &serv;
            serv.acquire();
        }

        {
            sync_lock sl;
            lock_guard lock(sl);

            if (!serv.signalled && !is_list_empty(serv.accept_queue)) {
                serv.signalled = true;
                wake_up_waiters(serv.waiters, WAIT_TYPE_READING);
            }

            auto& ev = static_cast<event&>(serv.accept_queue_not_full_event.get());

            {
                lock_guard lock2(serv.accept_queue_lock);

                if (serv.listen_backlog == 0 && ev.signalled)
                    ev.signalled = false;
            }
        }

        break;
    }

    return nt_status::SUCCESS;
}

template<unix_socket_type TYPE>
nt_status unix_socket<TYPE>::listen(int backlog) {
    if (backlog < 0)
        return nt_status::INVALID_PARAMETER;

    if (backlog == 0)
        backlog = 1;

    auto ev = create_event(false);
    if (!ev) {
        print("create_event failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    static_cast<event&>(ev.get()).signalled = true;

    {
        lock_guard lock(state_lock);

        if (state != unix_socket_state::closed || !bind_done)
            return nt_status::INVALID_PARAMETER;

        listen_backlog = (unsigned int)backlog;
        state = unix_socket_state::listening;

        swap(accept_queue_not_full_event, ev);
    }

    return nt_status::SUCCESS;
}

template<unix_socket_type TYPE>
template<bool NONBLOCK>
nt_status unix_socket<TYPE>::do_accept(HANDLE& sock, void* us_addr, int& addrlen) {
    nt_status status;

    if (addrlen < 0 || (us_addr && (size_t)addrlen < sizeof(sa_family_t)))
        return nt_status::INVALID_PARAMETER;

    auto& t = *get_current_thread();
    if (!t.proc)
        return nt_status::INTERNAL_ERROR;

    {
        lock_guard lock(state_lock);

        if (state != unix_socket_state::listening)
            return nt_status::INVALID_PARAMETER;
    }

    auto newsock = new unix_socket;
    if (!newsock) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    object_ref ref1{*newsock};

    unix_socket* c;

    while (true) {
        if constexpr (NONBLOCK) {
            status = wait_for_object(*this, true, 0);

            if (status == nt_status::TIMEOUT)
                return nt_status::CANT_WAIT;
        } else
            status = wait_for_object(*this, true);

        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status) || status == nt_status::USER_APC)
            return status;

        {
            sync_lock sl;
            lock_guard lock1(sl);
            lock_guard lock2(accept_queue_lock);

            if (is_list_empty(accept_queue))
                continue;

            auto& s = *(::unix_socket<TYPE>*)((uint8_t*)accept_queue.next - offsetof(::unix_socket<TYPE>, accept_le));

            remove_entry_list(s.accept_le);
            s.accept_done = true;

            c = &s;

            if (is_list_empty(accept_queue))
                this->signalled = false;

            listen_backlog++;

            auto& ev = static_cast<event&>(accept_queue_not_full_event.get());

            if (!ev.signalled) {
                ev.signalled = true;
                wake_up_waiters(ev.waiters, WAIT_TYPE_READING);
            }
        }

        unix_socket* old_server = nullptr;

        auto f = new file(*newsock, 0, 0);
        if (!f) {
            print("new failed\n");

            {
                lock_guard lock(c->state_lock);

                c->state = unix_socket_state::closed;
                swap(old_server, c->server);
            }

            if (old_server)
                old_server->release();

            c->release();
            return nt_status::INSUFFICIENT_RESOURCES;
        }

        object_ref ref2{*f};

        if (bind_name) {
            auto namelen = strlen(bind_name.get());
            unique_ptr<char[]> name;

            name.reset(new char[namelen + 1]);
            if (!name) {
                print("new failed\n");

                {
                    lock_guard lock(c->state_lock);

                    c->state = unix_socket_state::closed;
                    swap(old_server, c->server);
                }

                if (old_server)
                    old_server->release();

                c->release();
                return nt_status::INSUFFICIENT_RESOURCES;
            }

            memcpy(name.get(), bind_name.get(), namelen + 1);
            swap(newsock->bind_name, name);
        }

        newsock->other_end = c;
        newsock->first = true;
        newsock->state = unix_socket_state::connected;

        newsock->acquire();

        bool gone = false;

        {
            lock_guard lock(c->state_lock);

            if (c->state != unix_socket_state::awaiting_accept)
                gone = true;
            else {
                c->other_end = newsock;
                c->first = false;
                c->state = unix_socket_state::connected;
            }

            swap(c->server, old_server);
        }

        if (old_server)
            old_server->release();

        if (gone) {
            newsock->other_end = nullptr;
            newsock->state = unix_socket_state::closed;
            newsock->release();
            c->release();
            continue;
        }

        if (us_addr) {
            sockaddr_un addr;
            size_t namelen = 0;

            addr.sun_family = AF_UNIX;

            {
                lock_guard lock(c->state_lock);

                if (c->bind_name && c->bind_name[0] != 0) {
                    namelen = min(sizeof(sockaddr_un) - offsetof(sockaddr_un, sun_path),
                                  strlen(c->bind_name.get()) + 1);
                    memcpy(addr.sun_path, c->bind_name.get(), namelen);
                }
            }

            status = t.proc->write_vm(us_addr, &addr, (uint32_t)min((size_t)addrlen, offsetof(sockaddr_un, sun_path) + namelen),
                                      nullptr);
            if (!success(status)) {
                newsock->other_end = nullptr;
                newsock->state = unix_socket_state::closed;
                newsock->release();

                c->other_end = nullptr;
                c->state = unix_socket_state::closed;
                c->release();

                return status;
            }

            addrlen = (int)(offsetof(sockaddr_un, sun_path) + namelen);
        }

        status = t.proc->add_handle(*f, FILE_READ_DATA | FILE_WRITE_DATA,
                                    true, sock);
        if (!success(status)) {
            newsock->other_end = nullptr;
            newsock->state = unix_socket_state::closed;
            newsock->release();

            c->other_end = nullptr;
            c->state = unix_socket_state::closed;
            c->release();

            return status;
        }

        return nt_status::SUCCESS;
    }
}

template<unix_socket_type TYPE>
nt_status unix_socket<TYPE>::getsockname(span<uint8_t> buf, size_t& len) {
    if (buf.size() < offsetof(sockaddr_un, sun_path))
        return nt_status::BUFFER_TOO_SMALL;

    auto& addr = *(sockaddr_un*)buf.data();

    addr.sun_family = AF_UNIX;

    {
        lock_guard lock(state_lock);

        if (!bind_name || bind_name[0] == 0) {
            len = offsetof(sockaddr_un, sun_path);
            return nt_status::SUCCESS;
        }

        auto namelen = strlen(bind_name.get());

        if (buf.size() < offsetof(sockaddr_un, sun_path) + namelen + 1)
            return nt_status::BUFFER_TOO_SMALL;

        memcpy(addr.sun_path, bind_name.get(), namelen + 1);
        len = offsetof(sockaddr_un, sun_path) + namelen + 1;
    }

    return nt_status::SUCCESS;
}

template<unix_socket_type TYPE>
nt_status unix_socket<TYPE>::getpeername(span<uint8_t> buf, size_t& len) {
    if (buf.size() < offsetof(sockaddr_un, sun_path))
        return nt_status::BUFFER_TOO_SMALL;

    object_ref objref;

    {
        lock_guard lock(state_lock);

        if (!other_end)
            return nt_status::CONNECTION_DISCONNECTED;

        objref = *other_end;
        other_end->acquire();
    }

    auto& s = static_cast<unix_socket&>(objref.get());

    auto& addr = *(sockaddr_un*)buf.data();

    addr.sun_family = AF_UNIX;

    {
        lock_guard lock(s.state_lock);

        if (!s.bind_name || s.bind_name[0] == 0) {
            len = offsetof(sockaddr_un, sun_path);
            return nt_status::SUCCESS;
        }

        auto namelen = strlen(s.bind_name.get());

        if (buf.size() < offsetof(sockaddr_un, sun_path) + namelen + 1)
            return nt_status::BUFFER_TOO_SMALL;

        memcpy(addr.sun_path, s.bind_name.get(), namelen + 1);
        len = offsetof(sockaddr_un, sun_path) + namelen + 1;
    }

    return nt_status::SUCCESS;
}

template<unix_socket_type TYPE>
nt_status unix_socket<TYPE>::shutdown(int how) {
    if (how != SHUT_RD && how != SHUT_WR && how != SHUT_RDWR)
        return nt_status::INVALID_PARAMETER;

    object_ref objref;

    {
        lock_guard lock(state_lock);

        if (state != unix_socket_state::connected)
            return nt_status::CONNECTION_DISCONNECTED;

        if (how == SHUT_RD || how == SHUT_RDWR)
            shutdown_read = true;

        if (how == SHUT_WR || how == SHUT_RDWR)
            shutdown_write = true;

        objref = *other_end;
        other_end->acquire();
    }

    auto& s = static_cast<unix_socket&>(objref.get());

    if (how == SHUT_WR || how == SHUT_RDWR) {
        sync_lock sl;
        lock_guard lock(sl);

        if (!write_signalled) {
            write_signalled = true;
            wake_up_waiters(waiters, WAIT_TYPE_WRITING);
        }

        if (!s.signalled) {
            s.signalled = true;
            wake_up_waiters(s.waiters, WAIT_TYPE_READING);
        }
    }

    if (how == SHUT_RD || how == SHUT_RDWR) {
        sync_lock sl;
        lock_guard lock(sl);

        if (!signalled) {
            signalled = true;
            wake_up_waiters(waiters, WAIT_TYPE_READING);
        }

        if (!s.write_signalled) {
            s.write_signalled = true;
            wake_up_waiters(s.waiters, WAIT_TYPE_WRITING);
        }

        if (!not_full_event.signalled)
            not_full_event.set_holding_sync_lock();
    }

    return nt_status::SUCCESS;
}

nt_status create_unix_socket_pair(int type, object_ref& ref1, object_ref& ref2) {
    if (type != SOCK_STREAM) // FIXME - support SOCK_DGRAM too
        return nt_status::INVALID_PARAMETER;

    auto s1 = new unix_socket<unix_socket_type::stream>;
    if (!s1) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref1 = object_ref{*s1};

    auto s2 = new unix_socket<unix_socket_type::stream>;
    if (!s2) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    ref2 = object_ref{*s2};

    s1->other_end = s2;
    s1->first = true;
    s1->state = unix_socket_state::connected;

    s2->other_end = s1;
    s2->first = false;
    s2->state = unix_socket_state::connected;

    s1->acquire();
    s2->acquire();

    return nt_status::SUCCESS;
}
