// SPDX-License-Identifier: GPL-2.0-only
module;

#include <atomic>

export module event;

import object_ref;
import objects;

using namespace std;

export class event : public sync_object {
public:
    event(bool autoreset) : sync_object(object_type::event, false), autoreset(autoreset) { }

    void release() {
        if (--refcount == 0)
            delete this;
    }

    void on_wake();
    void set_holding_sync_lock();

    bool autoreset;
};

static_assert(is_sync_object<event>);
static_assert(object_has_on_wake<event>);

export object_ref create_event(bool autoreset);
export void set_event(event& e);
export void reset_event(event& e);
