// SPDX-License-Identifier: GPL-2.0-only
module;

#include <string.h>
#include <span>
#include <atomic>
#include <mutex>
#include <algorithm>

#ifdef __x86_64__
#include <x86intrin.h>
#endif

export module evdev_device;

import nt;
import print;
import evdev;
import list;
import thread;
import spinlock;
import handle;
import object_ref;
import moxnox_ioctls;
import chardev;
import event;
import scheduler;
import rwsem;
import kernel_time_page;
import config;
import disable_interrupts;
import vec;

#ifdef __aarch64__
import aarch64;
#endif

using namespace std;

static object_ref keyboard_pipe_obj, mice_pipe_obj;

class evdev_multiplexer : public chardev {
public:
    evdev_multiplexer() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override;
    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override;
    nt_status userspace_open(object_ref& objref) override;
    void post_event(const input_event& ev, uint32_t id);

    rwsem post_event_mutex;
    spinlock client_lock;
    list_entry clients = { &clients, &clients };

private:
    nt_status ioctl_get_absinfo(span<const uint8_t> inbuf, span<uint8_t> outbuf,
                                uintptr_t& written);
};

struct input_event_with_id_item {
    list_entry le;
    input_event_with_id ev;
};

struct input_event_item {
    list_entry le;
    input_event ev;
};

class evdev_multiplexer_client : public chardev {
public:
    evdev_multiplexer_client(evdev_multiplexer& p) : p(p) {
        p.acquire();

        {
            lock_guard lock(p.client_lock);
            insert_tail_list(p.clients, le);
        }

        signalled = false;
    }

    ~evdev_multiplexer_client() {
        {
            lock_guard lock(p.client_lock);
            remove_entry_list(le);
        }

        while (true) {
            input_event_with_id_item* it = nullptr;

            {
                lock_guard lock(items_lock);

                if (is_list_empty(items))
                    break;

                it = &containing_record<input_event_with_id_item>(items.next);
                remove_entry_list(it->le);
            }

            delete it;
        }

        p.release();
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override {
        return p.write(v, written_bytes);
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override {
        return p.ioctl(code, inbuf, outbuf, written);
    }

    void post_event(const input_event& ev, uint32_t id);

    list_entry le;
    bool visited = false;
    spinlock items_lock;
    list_entry items = { &items, &items };
    size_t items_left = 256;

private:
    evdev_multiplexer& p;
};

static atomic<uint32_t> next_keyboard_id = 0;
static spinlock evdev_contexts_spinlock;
static list_entry evdev_contexts = { &evdev_contexts, &evdev_contexts };

export class evdev_device : public chardev {
public:
    // FIXME - empty ctor to work round GCC bug
    evdev_device() : chardev() { }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status userspace_open(object_ref& objref) override;

    void post_event(input_event ev, bool add_syn);

    spinlock client_lock;
    list_entry clients = { &clients, &clients };

private:
    rwsem post_event_mutex;
};

export class evdev_device_client : public chardev {
public:
    evdev_device_client(evdev_device& p) : p(p) {
        p.acquire();

        {
            lock_guard lock(p.client_lock);
            insert_tail_list(p.clients, le);
        }

        signalled = false;
    }

    ~evdev_device_client() {
        {
            lock_guard lock(p.client_lock);
            remove_entry_list(le);
        }

        while (true) {
            input_event_item* it = nullptr;

            {
                lock_guard lock(items_lock);

                if (is_list_empty(items))
                    break;

                it = &containing_record<input_event_item>(items.next);
                remove_entry_list(it->le);
            }

            delete it;
        }

        p.release();
    }

    nt_status read(span<const span<uint8_t>> v, uint64_t& read_bytes) override;

    nt_status write(span<const span<const uint8_t>> v,
                    uint64_t& written_bytes) override {
        return p.write(v, written_bytes);
    }

    nt_status ioctl(uint32_t code, span<const uint8_t> inbuf,
                    span<uint8_t> outbuf, uintptr_t& written) override {
        return p.ioctl(code, inbuf, outbuf, written);
    }

    void post_event(const input_event& ev);

    list_entry le;
    bool visited = false;
    spinlock items_lock;
    list_entry items = { &items, &items };
    size_t items_left = 256;

private:
    evdev_device& p;
};

export class evdev_context {
public:
    evdev_context(chardev& p) : p(p) {
        p.acquire();

        id = ++next_keyboard_id;

        {
            lock_guard lock(evdev_contexts_spinlock);
            insert_tail_list(evdev_contexts, this->le);
        }
    }

    ~evdev_context() {
        {
            lock_guard lock(evdev_contexts_spinlock);
            remove_entry_list(this->le);
        }

        p.release();
    }

    void acquire() {
        refcount++;
    }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    nt_status write(const input_event& ev) {
        uint64_t written_bytes;
        auto sp = span((const uint8_t*)&ev, sizeof(input_event));

        return p.write(span(&sp, 1), written_bytes);
    }

    void listen_thread();
    void unregister();

    bool dead = false;
    uint32_t id;
    list_entry le;
    chardev& p;

private:
    atomic<unsigned int> refcount = 1;
};

export bool init_evdev_multiplexers() {
    auto pptr = new evdev_multiplexer;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    keyboard_pipe_obj = object_ref{*pptr};

    auto status = name_object(keyboard_pipe_obj.get(), "keyboard");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    pptr = new evdev_multiplexer;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    mice_pipe_obj = object_ref{*pptr};

    status = name_object(mice_pipe_obj.get(), "mice");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}

void evdev_context::listen_thread() {
    bool sent_key = false, sent_mouse = false;
    auto& k = static_cast<evdev_multiplexer&>(keyboard_pipe_obj.get());
    auto& m = static_cast<evdev_multiplexer&>(mice_pipe_obj.get());

    // FIXME - make sure this thread dies if device goes away

    while (true) {
        input_event ev;
        nt_status status;

        auto sp = span((uint8_t*)&ev, sizeof(ev));

        while (true) {
            uint64_t read_bytes;

            status = p.read(span(&sp, 1), read_bytes);
            if (dead || !success(status))
                break;

            if (read_bytes == sp.size())
                break;

            sp = span(sp.data() + read_bytes, sp.size() - read_bytes);
        }

        if (dead)
            break;

        if (!success(status)) {
            print("evdev_device::read returned {}\n", status);
            break;
        }

        bool is_syn = ev.type == evdev_type::EV_SYN && (evdev_syn_code)ev.code == evdev_syn_code::SYN_REPORT;

        if (ev.type == evdev_type::EV_KEY) {
            if ((evdev_key_code)ev.code >= evdev_key_code::BTN_LEFT && (evdev_key_code)ev.code <= evdev_key_code::BTN_EXTRA) {
                m.post_event(ev, this->id);
                sent_mouse = true;
            } else {
                k.post_event(ev, this->id);
                sent_key = true;
            }
        } else if (ev.type == evdev_type::EV_REL || ev.type == evdev_type::EV_ABS) {
            m.post_event(ev, this->id);
            sent_mouse = true;
        } else if (is_syn) {
            if (sent_mouse) {
                m.post_event(ev, this->id);
                sent_mouse = false;
            }

            if (sent_key) {
                k.post_event(ev, this->id);
                sent_key = false;
            }
        }
    }
}

void evdev_context::unregister() {
    dead = true;
}

export evdev_context* register_evdev_device(evdev_device& p) {
    object_ref objref{p};

    p.acquire();

    auto status = p.userspace_open(objref);
    if (!success(status)) {
        print("evdev_device::userspace_open returned {}\n", status);
        return nullptr;
    }

    auto kc = new evdev_context(static_cast<chardev&>(objref.get()));
    if (!kc) {
        print("new failed\n");
        return nullptr;
    }

    add_kernel_thread([](void* ctx) {
        auto& kc = *(evdev_context*)ctx;

        kc.listen_thread();

        delete &kc;
    }, kc);

    return kc;
}

nt_status evdev_multiplexer::userspace_open(object_ref& objref) {
    auto c = new evdev_multiplexer_client(*this);
    if (!c) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    object_ref r{*c};

    objref.swap(r);

    return nt_status::SUCCESS;
}

nt_status evdev_multiplexer_client::read(span<const span<uint8_t>> v,
                                         uint64_t& read_bytes) {
    size_t vec_size = get_vec_size(v);

    if (vec_size < sizeof(input_event_with_id))
        return nt_status::BUFFER_TOO_SMALL;

    span<uint8_t> top = v.front();

    read_bytes = 0;

    while (true) {
        auto status = wait_for_object(*this, true);
        if (!success(status) || status == nt_status::USER_APC || status == nt_status::ALERTED)
            return status;

        while (true) {
            input_event_with_id_item* it = nullptr;

            {
                lock_guard lock(items_lock);

                if (!is_list_empty(items) && vec_size >= sizeof(input_event_with_id)) {
                    auto& it2 = containing_record<input_event_with_id_item>(items.next);

                    remove_entry_list(it2.le);
                    it = &it2;
                    items_left++;
                }
            }

            if (!it) {
                if (read_bytes > 0)
                    return nt_status::SUCCESS;

                break;
            }

            {
                sync_lock sl;
                lock_guard lock(sl);
                lock_guard lock2(items_lock);

                if (is_list_empty(items))
                    signalled = false;
            }

            memcpy_vec(v, top, (uint8_t*)&it->ev, sizeof(input_event_with_id));
            vec_size -= sizeof(input_event_with_id);

            delete it;

            read_bytes += sizeof(input_event_with_id);
        }
    }
}

void evdev_multiplexer::post_event(const input_event& ev, uint32_t id) {
    lock_guard lg(post_event_mutex);

    {
        lock_guard lock(client_lock);

        if (is_list_empty(clients))
            return;
    }

    while (true) {
        evdev_multiplexer_client* c = nullptr;
        bool done = false;

        {
            lock_guard lock(client_lock);

            for (auto le = clients.next; le != &clients; le = le->next) {
                auto& c2 = containing_record<evdev_multiplexer_client>(le);

                if (c2.visited) {
                    done = true;
                    break;
                }

                c = &c2;
                c2.acquire();
                c2.visited = true;

                remove_entry_list(c2.le);
                insert_tail_list(clients, c2.le);
            }
        }

        if (done)
            break;

        c->post_event(ev, id);
        c->release();
    }

    {
        lock_guard lock(client_lock);

        for (auto le = clients.next; le != &clients; le = le->next) {
            auto& c = containing_record<evdev_multiplexer_client>(le);

            c.visited = false;
        }
    }
}

void evdev_multiplexer_client::post_event(const input_event& ev, uint32_t id) {
    // FIXME - drop oldest event if over limit

    auto it = new input_event_with_id_item;
    if (!it) {
        print("new failed\n");
        return;
    }

    it->ev.ev = ev;
    it->ev.id = id;

    bool failed = false;

    {
        lock_guard lock(items_lock);

        if (items_left == 0)
            failed = true;
        else {
            insert_tail_list(items, it->le);
            items_left--;
        }
    }

    if (failed) {
        // FIXME - Linux posts SYN_DROPPED event
        delete it;
        return;
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(items_lock);

        if (!is_list_empty(items) && !signalled) {
            signalled = true;
            wake_up_waiters(waiters, WAIT_TYPE_READING);
        }
    }
}

nt_status evdev_multiplexer::write(span<const span<const uint8_t>> v,
                                   uint64_t& written_bytes) {
    if (get_vec_size(v) < sizeof(input_event_with_id))
        return nt_status::INVALID_PARAMETER;

    input_event_with_id ev;
    span<const uint8_t> top = v.front();

    memcpy_vec((uint8_t*)&ev, v, top, sizeof(input_event_with_id));

    evdev_context* ctx = nullptr;

    {
        lock_guard lock(evdev_contexts_spinlock);

        for (auto le = evdev_contexts.next; le != &evdev_contexts; le = le->next) {
            auto& c = containing_record<evdev_context>(le);

            if (c.id == ev.id) {
                c.acquire();
                ctx = &c;
                break;
            }
        }
    }

    if (!ctx)
        return nt_status::NOT_FOUND;

    auto status = ctx->write(ev.ev);

    if (success(status))
        written_bytes = sizeof(input_event_with_id);

    ctx->release();

    return status;
}

nt_status evdev_device_client::read(span<const span<uint8_t>> v,
                                    uint64_t& read_bytes) {
    size_t vec_size = get_vec_size(v);

    if (vec_size < sizeof(input_event))
        return nt_status::BUFFER_TOO_SMALL;

    span<uint8_t> top = v.front();

    read_bytes = 0;

    while (true) {
        auto status = wait_for_object(*this, true);
        if (!success(status) || status == nt_status::USER_APC)
            return status;

        while (true) {
            input_event_item* it = nullptr;

            {
                lock_guard lock(items_lock);

                if (!is_list_empty(items) && vec_size >= sizeof(input_event)) {
                    auto& it2 = containing_record<input_event_item>(items.next);

                    remove_entry_list(it2.le);
                    it = &it2;
                    items_left++;
                }
            }

            if (!it) {
                if (read_bytes > 0)
                    return nt_status::SUCCESS;

                break;
            }

            {
                sync_lock sl;
                lock_guard lock(sl);
                lock_guard lock2(items_lock);

                if (is_list_empty(items))
                    signalled = false;
            }

            memcpy_vec(v, top, (uint8_t*)&it->ev, sizeof(input_event));
            vec_size -= sizeof(input_event);

            delete it;

            read_bytes += sizeof(input_event);
        }
    }
}

void evdev_device_client::post_event(const input_event& ev) {
    // FIXME - drop oldest event if over limit

    auto it = new input_event_item;
    if (!it) {
        print("new failed\n");
        return;
    }

    it->ev = ev;

    bool failed = false;

    {
        lock_guard lock(items_lock);

        if (items_left == 0)
            failed = true;
        else {
            insert_tail_list(items, it->le);
            items_left--;
        }
    }

    if (failed) {
        // FIXME - Linux posts SYN_DROPPED event
        delete it;
        return;
    }

    {
        sync_lock sl;
        lock_guard lock(sl);
        lock_guard lock2(items_lock);

        if (!is_list_empty(items) && !signalled) {
            signalled = true;
            wake_up_waiters(waiters, WAIT_TYPE_READING);
        }
    }
}

nt_status evdev_device::userspace_open(object_ref& objref) {
    auto c = new evdev_device_client(*this);
    if (!c) {
        print("new failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    object_ref r{*c};

    objref.swap(r);

    return nt_status::SUCCESS;
}

static void populate_event_time(input_event& ev) {
    disable_preempt();

    do {
        auto t1 = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;
        uint64_t r, delta;

#ifdef __x86_64__
        r = __rdtsc();
#elifdef __aarch64__
        r = cntvct_el0_load();
#endif

        r -= kernel_time_page.system_time_tsc;

        if (kernel_time_page.tsc_freq != 0) { // might not be set up yet
            delta = r * SYSTEM_TIME_RESOLUTION / kernel_time_page.tsc_freq;
            delta = min(kernel_time_page.tick_rate - 1, delta);
        } else
            delta = 0;

        auto t2 = (uint64_t)kernel_time_page.system_time + kernel_time_page.system_time_offset;

        if (t1 == t2) {
            t1 += delta;

            ev.time.tv_usec = (uint32_t)(((t1 % SYSTEM_TIME_RESOLUTION) * 1000000) / SYSTEM_TIME_RESOLUTION);
            t1 /= SYSTEM_TIME_RESOLUTION;

            ev.time.tv_sec = t1 - 11644473600;

            break;
        }
    } while (true);

    enable_preempt();
}

void evdev_device::post_event(input_event ev, bool add_syn) {
    input_event syn;

    syn.type = evdev_type::EV_SYN;
    syn.code = (evdev_code)evdev_syn_code::SYN_REPORT;
    syn.value = 0;

    lock_guard lg(post_event_mutex);

    {
        lock_guard lock(client_lock);

        if (is_list_empty(clients))
            return;
    }

    populate_event_time(ev);

    if (add_syn)
        syn.time = ev.time;

    while (true) {
        evdev_device_client* c = nullptr;
        bool done = false;

        {
            lock_guard lock(client_lock);

            for (auto le = clients.next; le != &clients; le = le->next) {
                auto& c2 = containing_record<evdev_device_client>(le);

                if (c2.visited) {
                    done = true;
                    break;
                }

                c = &c2;
                c2.acquire();
                c2.visited = true;

                remove_entry_list(c2.le);
                insert_tail_list(clients, c2.le);
            }
        }

        if (done)
            break;

        c->post_event(ev);

        if (add_syn)
            c->post_event(syn);

        c->release();
    }

    {
        lock_guard lock(client_lock);

        for (auto le = clients.next; le != &clients; le = le->next) {
            auto& c = containing_record<evdev_device_client>(le);

            c.visited = false;
        }
    }
}

nt_status evdev_multiplexer::ioctl(uint32_t code, span<const uint8_t> inbuf,
                                   span<uint8_t> outbuf, uintptr_t& written) {
    switch (code) {
        case IOCTL_MOXNOX_EVDEV_GET_ABSINFO:
            return ioctl_get_absinfo(inbuf, outbuf, written);

        default:
            return nt_status::NOT_SUPPORTED;
    }
}

nt_status evdev_multiplexer::ioctl_get_absinfo(span<const uint8_t> inbuf,
                                               span<uint8_t> outbuf,
                                               uintptr_t& written) {
    if (inbuf.size() < sizeof(MOXNOX_IOCTL_EVDEV_MULTIPLEXER_GET_ABSINFO))
        return nt_status::INVALID_PARAMETER;

    auto& input = *(MOXNOX_IOCTL_EVDEV_MULTIPLEXER_GET_ABSINFO*)inbuf.data();

    evdev_context* ctx = nullptr;

    {
        lock_guard lock(evdev_contexts_spinlock);

        for (auto le = evdev_contexts.next; le != &evdev_contexts; le = le->next) {
            auto& c = containing_record<evdev_context>(le);

            if (c.id == input.id) {
                c.acquire();
                ctx = &c;
                break;
            }
        }
    }

    if (!ctx)
        return nt_status::NOT_FOUND;

    auto ret = ctx->p.ioctl(IOCTL_MOXNOX_EVDEV_GET_ABSINFO,
                            span((uint8_t*)&input.code, sizeof(input.code)),
                            outbuf, written);

    ctx->release();

    return ret;
}
