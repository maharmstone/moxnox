// SPDX-License-Identifier: LGPL-2.1-or-later
#include <stdint.h>
#include <stdlib.h>
#include <math.h>

using maths_err_func = int (*)(struct _exception*);

static maths_err_func err_func;

extern "C"
void _fpreset() {
    // FIXME(?)
}

extern "C"
void ___setusermatherr(maths_err_func func) {
    err_func = func;
}

extern "C"
div_t div(int numer, int denom) {
    div_t ret;

    ret.quot = numer / denom;
    ret.rem = numer % denom;

    return ret;
}

extern "C"
ldiv_t ldiv(long numer, long denom) {
    ldiv_t ret;

    ret.quot = numer / denom;
    ret.rem = numer % denom;

    return ret;
}

extern "C"
lldiv_t lldiv(long long numer, long long denom) {
    lldiv_t ret;

    ret.quot = numer / denom;
    ret.rem = numer % denom;

    return ret;
}

extern "C"
int abs(int j) {
    return j < 0 ? -j : j;
}

extern "C"
long labs(long j) {
    return j < 0 ? -j : j;
}

extern "C"
long long llabs(long long j) {
    return j < 0 ? -j : j;
}
