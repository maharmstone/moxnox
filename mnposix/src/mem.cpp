#include <stdlib.h>

#undef errno
import errno;

extern "C"
void* reallocarray(void* ptr, size_t nelem, size_t elsize) {
    size_t v;

    if (__builtin_mul_overflow(nelem, elsize, &v)) {
        *_errno() = ENOMEM;
        return nullptr;
    }

    return realloc(ptr, v);
}
