// SPDX-License-Identifier: MIT
#pragma once

#include <stddef.h>
#include <stdarg.h>
#include <sys/types.h>

#ifndef _FILE_DEFINED // FIXME
#define _FILE_DEFINED
typedef struct __FILE FILE;
#endif

typedef long long fpos_t;

#define BUFSIZ 512
#define EOF (-1)

#define P_tmpdir "/tmp"

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#define _IOFBF 0x0000
#define _IOLBF 0x0040
#define _IONBF 0x0004

#define FILENAME_MAX 260

typedef struct localeinfo_struct* _locale_t;

#define L_ctermid 9

#ifdef __cplusplus
extern "C" {
#endif

FILE* __acrt_iob_func(unsigned int idx);
#define stdin (__acrt_iob_func(0))
#define stdout (__acrt_iob_func(1))
#define stderr (__acrt_iob_func(2))

FILE* fopen(const char* filename, const char* mode);
FILE* freopen(const char* filename, const char* mode, FILE* stream);
int fclose(FILE* stream);
int fflush(FILE* stream);
void setbuf(FILE* stream, char* buf);
int setvbuf(FILE* stream, char* buf, int mode, size_t size);
size_t fread(void* ptr, size_t size, size_t nmemb, FILE* stream);
size_t fwrite(const void* ptr, size_t size, size_t nmemb, FILE* stream);
int fgetc(FILE* stream);
char* fgets(char* s, int n, FILE* stream);
int fputc(int c, FILE* stream);
int fputs(const char* s, FILE* stream);
int getc(FILE* stream);
int getchar(void);
int putc(int c, FILE* stream);
int putchar(int c);
int puts(const char* s);
int ungetc(int c, FILE* stream);
int fseek(FILE* stream, long offset, int whence);
long ftell(FILE* stream);
void rewind(FILE* stream);
int fgetpos(FILE* stream, fpos_t* pos);
int fsetpos(FILE* stream, const fpos_t* pos);
void clearerr(FILE* stream);
int feof(FILE* stream);
int ferror(FILE* stream);
void perror(const char* s);
int remove(const char* filename);
int rename(const char* oldname, const char* newname);
int renameat(int fromfd, const char* from, int tofd, const char* to);
FILE* tmpfile(void);
char* ctermid(char* s);
char* tmpnam(char* s);
int printf(const char* format, ...);
int fprintf(FILE* stream, const char* format, ...);
int sprintf(char* s, const char* format, ...);
int snprintf(char* s, size_t n, const char* format, ...);
int vprintf(const char* format, va_list arg);
int vfprintf(FILE* stream, const char* format, va_list arg);
int vsprintf(char* s, const char* format, va_list arg);
int vsnprintf(char* s, size_t n, const char* format, va_list arg);
int scanf(const char* format, ...);
int fscanf(FILE* stream, const char* format, ...);
int sscanf(const char* s, const char* format, ...);
int vscanf(const char* format, va_list arg);
int vfscanf(FILE* stream, const char* format, va_list arg);
int vsscanf(const char* s, const char* format, va_list arg);
int _fgetc_nolock(FILE* stream);
FILE* _fdopen(int fd, const char* mode);
int _sscanf_l(const char* s, const char* format, _locale_t locale, ...);
ssize_t getline(char** lineptr, size_t* n, FILE* stream);
int fileno(FILE* stream);
FILE* fdopen(int fd, const char* mode);
FILE* popen(const char* command, const char* mode);
int pclose(FILE* stream);
int fseeko(FILE* stream, off_t offset, int whence);
off_t ftello(FILE* stream);
int fseeko64(FILE* stream, off64_t offset, int whence);
off64_t ftello64(FILE* stream);
void flockfile(FILE* f);
void funlockfile(FILE* f);
int getc_unlocked(FILE* f);
int asprintf(char** ptr, const char* format, ...);
int vasprintf(char** ptr, const char* format, va_list ap);
FILE* open_memstream(char** bufp, size_t* sizep);
ssize_t getdelim(char** lineptr, size_t* n, int delimiter, FILE* stream);

int __mingw_vfprintf(FILE* stream, const char* format, va_list argptr);
int __mingw_vfwprintf(FILE* stream, const wchar_t* format, va_list argptr);
int __mingw_vsnprintf(char* buffer, size_t count, const char* format,
                      va_list argptr);
int __mingw_vsprintf(char* buffer, const char* format, va_list argptr);
int __mingw_vswprintf(wchar_t* buffer, size_t count, const wchar_t* format,
                      va_list argptr);

#ifdef __cplusplus
}
#endif
