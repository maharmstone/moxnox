// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int strcasecmp(const char *s1, const char *s2);
int strncasecmp(const char *s1, const char *s2, size_t n);
int ffs(int i);
int ffsl(long i);

#ifdef __cplusplus
}
#endif
