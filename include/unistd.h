// SPDX-License-Identifier: MIT
#pragma once

#include <sys/types.h>

#define _POSIX_THREADS 202405L
#define _POSIX_TIMERS 202405L
#define _POSIX_VERSION 202405L
#define _POSIX2_VERSION 202405L

#define STDIN_FILENO 0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#define F_OK 1
#define R_OK 2
#define W_OK 4
#define X_OK 8

#define _SC_CLK_TCK 1
#define _SC_NPROCESSORS_ONLN 2
#define _SC_PAGESIZE 3
#define _SC_PAGE_SIZE _SC_PAGESIZE
#define _SC_GETPW_R_SIZE_MAX 4
#define _SC_HOST_NAME_MAX 5
#define _SC_ARG_MAX 6
#define _SC_PHYS_PAGES 7
#define _SC_NPROCESSORS_CONF 8
#define _SC_OPEN_MAX 9
#define _SC_TTY_NAME_MAX 10

#define _PC_PATH_MAX 0
#define _PC_NAME_MAX 1
#define _PC_PIPE_BUF 2

#define _CS_PATH 0

#if defined(__cplusplus) && __cplusplus >= 201103L
#define __moxnox_noexcept noexcept
#elif defined(__cplusplus)
#define __moxnox_noexcept throw()
#else
#define __moxnox_noexcept
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern int optind;
extern int opterr;
extern int optopt;
extern char* optarg;

char* ttyname(int fd);
int ttyname_r(int fd, char* name, size_t namesize);
uid_t getuid(void);
uid_t geteuid(void);
gid_t getgid(void);
gid_t getegid(void);
int isatty(int fd);
int access(const char* path, int mode);
int close(int fd);
ssize_t read(int fd, void* buf, size_t count);
ssize_t write(int fd, const void* buf, size_t count);
int chdir(const char* path);
char* getcwd(char* buf, size_t size);
int unlink(const char* path);
unsigned int alarm(unsigned int seconds);
int dup(int fd);
int dup2(int oldfd, int newfd);
unsigned int sleep(unsigned int seconds);
int getpid(void);
off_t lseek(int fd, off_t offset, int whence);
int ftruncate(int fd, off_t length);
int rmdir(const char* path);
int execv(const char* path, char* const argv[]);
int execvp(const char* file, char* const argv[]);
int execve(const char* file, char* const argv[], char* const envp[]);
int execlp(const char* file, const char* arg, ...);
int execl(const char* path, const char* arg, ...);
int execle(const char* path, const char* arg, ...);
int getentropy(void *buf, size_t buflen);
int gethostname(char* name, int namelen); // FIXME - POSIX says size_t, but winsock2.h has int
int pause(void);
int pipe(int pipefd[2]);
int fsync(int fd);
int faccessat(int fd, const char* path, int amode, int flag);
int fchdir(int fd);
int symlink(const char* path1, const char* path2);
pid_t getppid(void);
pid_t getpgid(pid_t pid);
int tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgid);
int link(const char* oldpath, const char* newpath);
int readlink(const char* path, char* buf, size_t bufsiz);
int getpagesize(void);
long sysconf(int name);
pid_t getsid(pid_t pid);
pid_t setsid(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgrp(void);
int fchown(int fd, uid_t owner, gid_t group);
int chown(const char* path, uid_t owner, gid_t group);
long gethostid(void);
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
int getresuid(uid_t* ruid, uid_t* euid, uid_t* suid);
int getresgid(gid_t* rgid, gid_t* egid, gid_t* sgid);
int setgid(gid_t gid);
int setuid(uid_t uid);
int lchown(const char* path, uid_t owner, gid_t group);
int getgroups(int gidsetsize, gid_t* grouplist);
int truncate(const char* path, off_t length);
long pathconf(const char* path, int name);
long fpathconf(int fd, int name);
int unlinkat(int fd, const char* path, int flag);
int fchownat(int fd, const char* path, uid_t owner, gid_t group, int flag);
int linkat(int fd1, const char* path1, int fd2, const char* path2, int flag);
int symlinkat(const char* path1, int fd, const char* path2);
ssize_t readlinkat(int fd, const char* path, char* buf, size_t bufsize);
char* getlogin(void);
int getlogin_r(char* name, size_t namesize);
size_t confstr(int name, char* buf, size_t len);
ssize_t pread(int fd, void* buf, size_t nbyte, off_t offset);
ssize_t pwrite(int fd, const void* buf, size_t nbyte, off_t offset);
pid_t fork(void);
__attribute__((noreturn)) void _exit(int status) __moxnox_noexcept;
char* crypt(const char* key, const char* salt);
int seteuid(uid_t uid);
int setegid(gid_t gid);
int getopt(int argc, char* const argv[], const char* optstring);

#ifdef __cplusplus
}
#endif
