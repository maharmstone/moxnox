// SPDX-License-Identifier: MIT
#pragma once

#include <signal.h>

#if defined(__aarch64__)

#define _JBLEN 24
typedef unsigned long long _JBTYPE;

typedef _JBTYPE jmp_buf[_JBLEN];

typedef struct _JUMP_BUFFER {
    unsigned long long Frame;
    unsigned long long Reserved;
    unsigned long long X19;
    unsigned long long X20;
    unsigned long long X21;
    unsigned long long X22;
    unsigned long long X23;
    unsigned long long X24;
    unsigned long long X25;
    unsigned long long X26;
    unsigned long long X27;
    unsigned long long X28;
    unsigned long long Fp;
    unsigned long long Lr;
    unsigned long long Sp;
    unsigned long Fpcr;
    unsigned long Fpsr;
    double D[8];
} _JUMP_BUFFER;

#elif defined(__x86_64__)

typedef struct __attribute__((aligned(16))) _SETJMP_FLOAT128 {
    unsigned long long Part[2];
} SETJMP_FLOAT128;

#define _JBLEN 16
typedef SETJMP_FLOAT128 _JBTYPE;

typedef _JBTYPE jmp_buf[_JBLEN];

typedef struct _JUMP_BUFFER {
    unsigned long long Frame;
    unsigned long long Rbx;
    unsigned long long Rsp;
    unsigned long long Rbp;
    unsigned long long Rsi;
    unsigned long long Rdi;
    unsigned long long R12;
    unsigned long long R13;
    unsigned long long R14;
    unsigned long long R15;
    unsigned long long Rip;
    unsigned long MxCsr;
    unsigned short FpCsr;
    unsigned short Spare;
    SETJMP_FLOAT128 Xmm6;
    SETJMP_FLOAT128 Xmm7;
    SETJMP_FLOAT128 Xmm8;
    SETJMP_FLOAT128 Xmm9;
    SETJMP_FLOAT128 Xmm10;
    SETJMP_FLOAT128 Xmm11;
    SETJMP_FLOAT128 Xmm12;
    SETJMP_FLOAT128 Xmm13;
    SETJMP_FLOAT128 Xmm14;
    SETJMP_FLOAT128 Xmm15;
} _JUMP_BUFFER;

#endif

typedef struct {
    jmp_buf jmp;
    sigset_t sig;
} _sigjmp_buf;

typedef _sigjmp_buf sigjmp_buf[1];

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((nothrow, returns_twice)) int _setjmp(jmp_buf env, void* frame);
__attribute__((nothrow, returns_twice)) int _setjmpex(jmp_buf env, void* frame);
__attribute__((noreturn, nothrow)) void longjmp(jmp_buf env, int val);
[[noreturn]] void siglongjmp(sigjmp_buf env, int val);

#ifdef __cplusplus
}
#endif

#ifdef __aarch64__
#define setjmp(env) _setjmpex((env), __builtin_sponentry())
#else
#define setjmp(env) _setjmp((env), NULL)
#endif

#define sigsetjmp(env, savesigs) \
    ((savesigs) ? (sigprocmask(SIG_BLOCK, NULL, &(env)->sig), _setjmp((env)->jmp, NULL)) \
                  : ((env)->sig |= (sigset_t)1 << 63, _setjmp((env)->jmp, NULL)))
