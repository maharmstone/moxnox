// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdlib.h>
#include <string_view>
#include <span>
#include <mutex>

import print;
import ucrtbase;
import unicode;
import nt;
import ntdll;

#undef errno
import errno;

#include <string.h>

using namespace std;

static bool allocated_peb_env = false;

static int update_ucrt_env(string_view name, string_view value) {
    // FIXME - lock?

    auto ent = (char*)malloc(name.size() + 1 + value.size() + 1);
    if (!ent) {
        *_errno() = ENOMEM;
        return -1;
    }

    memcpy(ent, name.data(), name.size());
    ent[name.size()] = '=';
    memcpy(ent + name.size() + 1, value.data(), value.size());
    ent[name.size() + 1 + value.size()] = 0;

    auto env = *__p__environ();
    size_t num_entries = 0;

    while (env[num_entries]) {
        auto sv = string_view(env[num_entries]);

        if (sv.size() < name.size()) {
            num_entries++;
            continue;
        }

        if (string_view(sv.data(), name.size()) != name) {
            num_entries++;
            continue;
        }

        if (sv.size() > name.size() && sv[name.size()] != '=') {
            num_entries++;
            continue;
        }

        swap(env[num_entries], ent);

        // not freeing ent because of putenv

        return 0;
    }

    auto newenv = (char**)malloc((num_entries + 2) * sizeof(char*));
    if (!newenv) {
        free(ent);
        *_errno() = ENOMEM;
        return -1;
    }

    memcpy(newenv, env, num_entries * sizeof(char*));
    newenv[num_entries] = ent;
    newenv[num_entries + 1] = nullptr;

    *__p__environ() = newenv;
    free(env);

    return 0;
}

extern "C"
int setenv(const char* name, const char* value, int overwrite) {
    char16_t* u16value;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!name) {
        *_errno() = EINVAL;
        return -1;
    }

    auto namesv = string_view(name);
    if (namesv.empty() || namesv.find('=') != string_view::npos) {
        *_errno() = EINVAL;
        return -1;
    }

    auto valuesv = string_view(value);

    auto u16namelen = utf8_to_utf16_len(namesv);

    auto u16name = (char16_t*)malloc(u16namelen * sizeof(char16_t));
    if (!u16name) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16name, u16namelen), namesv);

    auto u16namesv = u16string_view(u16name, u16namelen);

    auto u16valuelen = utf8_to_utf16_len(valuesv);

    if (!valuesv.empty()) {
        u16value = (char16_t*)malloc(u16valuelen * sizeof(char16_t));
        if (!u16value) {
            free(u16name);
            *_errno() = ENOMEM;
            return -1;
        }

        utf8_to_utf16(span(u16value, u16valuelen), valuesv);
    } else
        u16value = nullptr;

    auto u16valuesv = u16string_view(u16value, u16valuelen);

    {
        mn_peb_lock peb_lock;
        lock_guard lock(peb_lock);
        size_t found = u16string_view::npos, foundsize = 0;

        auto env = u16string_view((char16_t*)peb->ProcessParameters->Environment,
                                  peb->ProcessParameters->EnvironmentSize / sizeof(char16_t));

        while (true) {
            if (env.empty() || env[0] == 0)
                break;

            auto zero = env.find(u'\0');

            u16string_view bit;

            if (zero == u16string_view::npos)
                bit = env;
            else
                bit = u16string_view(env.data(), zero);

            auto eq = bit.find(u'=');

            u16string_view bitname;

            if (eq == u16string_view::npos)
                bitname = bit;
            else
                bitname = u16string_view(bit.data(), eq);

            if (bitname == u16namesv) {
                if (!overwrite) {
                    free(u16name);

                    if (u16value)
                        free(u16value);

                    return 0;
                }

                found = (size_t)(bitname.data() - (char16_t*)peb->ProcessParameters->Environment);
                foundsize = bit.size() + 1;
                break;
            }

            if (zero != u16string_view::npos)
                env = u16string_view(env.data() + zero + 1, env.size() - zero - 1);
            else
                break;
        }

        size_t newlen = peb->ProcessParameters->EnvironmentSize / sizeof(char16_t);

        newlen -= foundsize;
        newlen += u16namelen + 1 + u16valuelen + 1;

        auto newenv = (char16_t*)malloc(newlen * sizeof(char16_t));
        if (!newenv) {
            free(u16name);

            if (u16value)
                free(u16value);

            *_errno() = ENOMEM;
            return -1;
        }

        if (found == u16string_view::npos) {
            memcpy(newenv, peb->ProcessParameters->Environment,
                   peb->ProcessParameters->EnvironmentSize);
        } else {
            memcpy(newenv, peb->ProcessParameters->Environment,
                   found * sizeof(char16_t));
            memcpy(newenv + found, (char16_t*)peb->ProcessParameters->Environment + found + foundsize,
                   peb->ProcessParameters->EnvironmentSize - ((found + foundsize) * sizeof(char16_t)));
        }

        auto ptr = newenv + newlen - u16namelen - 1 - u16valuelen - 1 - 1;

        memcpy(ptr, u16namesv.data(), u16namesv.size() * sizeof(char16_t));
        ptr += u16namesv.size();

        *ptr = '=';
        ptr++;

        memcpy(ptr, u16valuesv.data(), u16valuesv.size() * sizeof(char16_t));
        ptr += u16valuesv.size();

        *ptr = 0;
        ptr++;

        *ptr = 0;
        ptr++;

        peb->ProcessParameters->EnvironmentSize = newlen * sizeof(char16_t);

        auto oldenv = peb->ProcessParameters->Environment;
        peb->ProcessParameters->Environment = newenv;

        if (allocated_peb_env)
            free(oldenv);

        allocated_peb_env = true;
    }

    free(u16name);

    if (u16value)
        free(u16value);

    return update_ucrt_env(namesv, valuesv);
}

static int update_ucrt_env_remove(string_view name) {
    // FIXME - lock?

    auto env = *__p__environ();
    size_t num_entries = 0;

    while (env[num_entries]) {
        num_entries++;
    }

    for (size_t i = 0; i < num_entries; i++) {
        auto sv = string_view(env[i]);

        if (sv.size() < name.size())
            continue;

        if (string_view(sv.data(), name.size()) != name)
            continue;

        if (sv.size() > name.size() && sv[name.size()] != '=')
            continue;

        if (i != num_entries - 1)
            swap(env[num_entries - 1], env[i]);

        // not freeing because of putenv

        env[num_entries - 1] = nullptr;
        num_entries--;
    }

    return 0;
}

extern "C"
int unsetenv(const char* name) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    if (!name) {
        *_errno() = EINVAL;
        return -1;
    }

    auto namesv = string_view(name);
    if (namesv.empty() || namesv.find('=') != string_view::npos) {
        *_errno() = EINVAL;
        return -1;
    }

    auto u16namelen = utf8_to_utf16_len(namesv);

    auto u16name = (char16_t*)malloc(u16namelen * sizeof(char16_t));
    if (!u16name) {
        *_errno() = ENOMEM;
        return -1;
    }

    utf8_to_utf16(span(u16name, u16namelen), namesv);

    auto u16namesv = u16string_view(u16name, u16namelen);

    {
        mn_peb_lock peb_lock;
        lock_guard lock(peb_lock);
        size_t found = u16string_view::npos, foundsize = 0;

        auto env = u16string_view((char16_t*)peb->ProcessParameters->Environment,
                                  peb->ProcessParameters->EnvironmentSize / sizeof(char16_t));

        while (true) {
            if (env.empty() || env[0] == 0)
                break;

            auto zero = env.find(u'\0');

            u16string_view bit;

            if (zero == u16string_view::npos)
                bit = env;
            else
                bit = u16string_view(env.data(), zero);

            auto eq = bit.find(u'=');

            u16string_view bitname;

            if (eq == u16string_view::npos)
                bitname = bit;
            else
                bitname = u16string_view(bit.data(), eq);

            if (bitname == u16namesv) {
                found = (size_t)(bitname.data() - (char16_t*)peb->ProcessParameters->Environment);
                foundsize = bit.size() + 1;
                break;
            }

            if (zero != u16string_view::npos)
                env = u16string_view(env.data() + zero + 1, env.size() - zero - 1);
            else
                break;
        }

        if (found == u16string_view::npos) {
            free(u16name);
            return 0;
        }

        size_t newlen = peb->ProcessParameters->EnvironmentSize / sizeof(char16_t);

        newlen -= foundsize;

        auto newenv = (char16_t*)malloc(newlen * sizeof(char16_t));
        if (!newenv) {
            free(u16name);
            *_errno() = ENOMEM;
            return -1;
        }

        memcpy(newenv, peb->ProcessParameters->Environment,
               found * sizeof(char16_t));
        memcpy(newenv + found, (char16_t*)peb->ProcessParameters->Environment + found + foundsize,
               peb->ProcessParameters->EnvironmentSize - ((found + foundsize) * sizeof(char16_t)));

        auto ptr = newenv + newlen - 1;

        *ptr = 0;
        ptr++;

        peb->ProcessParameters->EnvironmentSize = newlen * sizeof(char16_t);

        auto oldenv = peb->ProcessParameters->Environment;
        peb->ProcessParameters->Environment = newenv;

        if (allocated_peb_env)
            free(oldenv);

        allocated_peb_env = true;
    }

    free(u16name);

    return update_ucrt_env_remove(namesv);
}

extern "C"
char* secure_getenv(const char* name) {
    // FIXME - if uid and euid or gid and egid differ, should be returning NULL

    return getenv(name);
}
