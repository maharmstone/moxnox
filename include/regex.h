// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>

typedef ssize_t regoff_t;

typedef struct {
    size_t re_nsub;
    void* buffer;
} regex_t;

typedef struct {
    regoff_t rm_so;
    regoff_t rm_eo;
} regmatch_t;

#define REG_EXTENDED  1
#define REG_ICASE     2
#define REG_NEWLINE   4
#define REG_NOSUB     8

#define REG_NOTBOL    1
#define REG_NOTEOL    2

#define REG_STARTEND  4

#define REG_NOMATCH   1
#define REG_BADPAT    2
#define REG_ECOLLATE  3
#define REG_ECTYPE    4
#define REG_EESCAPE   5
#define REG_ESUBREG   6
#define REG_EBRACK    7
#define REG_EPAREN    8
#define REG_EBRACE    9
#define REG_BADBR     10
#define REG_ERANGE    11
#define REG_ESPACE    12
#define REG_BADRPT    13

#ifdef __cplusplus
extern "C" {
#endif

int regcomp(regex_t* preg, const char* regex, int cflags);
int regexec(const regex_t* preg, const char* string, size_t nmatch,
            regmatch_t* pmatch, int eflags);
size_t regerror(int errcode, const regex_t* oreg, char* errbuf,
            size_t errbuf_size);
void regfree(regex_t* preg);

#ifdef __cplusplus
}
#endif

