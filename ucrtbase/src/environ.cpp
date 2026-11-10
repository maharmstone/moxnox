// SPDX-License-Identifier: LGPL-2.1-or-later
#include <string_view>
#include <span>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

import nt;
import ucrtbase;
import print;
import unicode;

using namespace std;

extern "C" int* __errno();
extern "C" char*** ___p__environ();

extern "C"
char* getenv(const char* name) {
    auto namesv = string_view(name);

    auto env = *___p__environ();
    if (!env)
        return nullptr;

    while (*env) {
        auto sv = string_view(*env);

        auto eq = sv.find('=');

        if (eq == string_view::npos) {
            env++;
            continue;
        }

        if (string_view(sv.data(), eq) == namesv)
            return (char*)(sv.data() + eq + 1);

        env++;
    }

    return nullptr;
}

extern "C"
errno_t _getenv_s(size_t* len, char* value, rsize_t valuesz, const char* name) {
    if (!len) {
        *__errno() = EINVAL;
        return EINVAL;
    }

    if (!value && valuesz > 0) {
        *__errno() = EINVAL;
        return EINVAL;
    }

    if (!name) {
        *__errno() = EINVAL;
        return EINVAL;
    }

    auto var = getenv(name);
    if (!var) {
        *len = 0;
        *__errno() = EINVAL;
        return EINVAL;
    }

    *len = strlen(var) + 1;

    if (valuesz < *len) {
        *__errno() = ERANGE;
        return ERANGE;
    }

    strcpy(value, var);

    return 0;
}

static size_t count_env_vars() {
    size_t ret = 0;
    auto env = *___p__environ();

    while (*env) {
        ret++;
        env++;
    }

    return ret;
}

extern "C"
int _putenv(char* string) {
    auto sv = string_view(string);

    auto env = *___p__environ();
    if (!env) {
        *__errno() = EINVAL;
        return -1;
    }

    // FIXME - about copy of environment variables in PEB?

    if (auto eq = sv.find('='); eq != string_view::npos) {
        auto name = string_view(sv.data(), eq);

        while (*env) {
            auto sv2 = string_view(*env);

            auto eq2 = sv2.find('=');

            if (eq2 == string_view::npos) {
                env++;
                continue;
            }

            // replace
            if (string_view(sv2.data(), eq2) == name) {
                *env = string;
                return 0;
            }

            env++;
        }

        // add new

        auto count = count_env_vars();
        auto new_env = (char**)malloc((count + 2) * sizeof(char*));

        if (!new_env) {
            *__errno() = ENOMEM;
            return -1;
        }

        env = *___p__environ();

        memcpy(new_env, env, count * sizeof(char*));
        new_env[count] = string;
        new_env[count + 1] = nullptr;

        *___p__environ() = new_env;
        free(env);

        return 0;
    } else {
        size_t index = 0;
        while (*env) {
            auto sv2 = string_view(*env);

            auto eq2 = sv2.find('=');
            string_view name;

            if (eq2 == string_view::npos)
                name = sv2;
            else
                name = string_view(sv2.data(), eq2);

            // remove
            if (name == sv) {
                auto count = count_env_vars();
                auto new_env = (char**)malloc(count * sizeof(char*));

                if (!new_env) {
                    *__errno() = ENOMEM;
                    return -1;
                }

                env = *___p__environ();
                memcpy(new_env, env, index * sizeof(char*));
                memcpy(&new_env[index], &env[index + 1], (count - index) * sizeof(char*));

                *___p__environ() = new_env;
                free(env);

                return 0;
            }

            env++;
            index++;
        }

        // not found

        *__errno() = EINVAL;
        return -1;
    }
}
