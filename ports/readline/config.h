#include <termios.h>
#include <sys/stat.h>
#include <sys/select.h>
#include <signal.h>

/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in for the GNU readline library. */

/* Copyright (C) 1994-2024 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

/* config.h.in.  Maintained by hand. */

/* Template definitions for autoconf */

/* These are set by AC_USE_SYSTEM_EXTENSIONS */
#define __EXTENSIONS__ 1
#define _ALL_SOURCE 1
#define _GNU_SOURCE 1
/* #undef _POSIX_SOURCE */
/* #undef _POSIX_1_SOURCE */
#define _POSIX_PTHREAD_SEMANTICS 1
#define _TANDEM_SOURCE 1
/* #undef _MINIX */

/* Define NO_MULTIBYTE_SUPPORT to not compile in support for multibyte
   characters, even if the OS supports them. */
/* #undef NO_MULTIBYTE_SUPPORT */

/* #undef _FILE_OFFSET_BITS */

/* Characteristics of the compiler. */
/* #undef inline */

/* #undef sig_atomic_t */

/* #undef size_t */

/* #undef ssize_t */

/* #undef mode_t */

/* #undef const */

/* #undef volatile */

#define PROTOTYPES 1
#define __PROTOTYPES 1

/* #undef __CHAR_UNSIGNED__ */

/* Define if the `S_IS*' macros in <sys/stat.h> do not work properly.  */
/* #undef STAT_MACROS_BROKEN */

/* Define if you have the chown function. */
#define HAVE_CHOWN 1

/* Define if you have the fcntl function. */
#define HAVE_FCNTL 1

/* Define if you have the fnmatch function. */
#define HAVE_FNMATCH 1

/* Define if you have the getpwent function. */
#define HAVE_GETPWENT 1

/* Define if you have the getpwnam function. */
#define HAVE_GETPWNAM 1

/* Define if you have the getpwuid function. */
#define HAVE_GETPWUID 1

/* Define if you have the gettimeofday function. */
#define HAVE_GETTIMEOFDAY 1

/* Define if you have the isascii function. */
/* #undef HAVE_ISASCII */

/* Define if you have the iswctype function.  */
#define HAVE_ISWCTYPE 1

/* Define if you have the iswlower function.  */
#define HAVE_ISWLOWER 1

/* Define if you have the iswupper function.  */
#define HAVE_ISWUPPER 1

/* Define if you have the isxdigit function. */
#define HAVE_ISXDIGIT 1

/* Define if you have the kill function. */
#define HAVE_KILL 1

/* Define if you have the lstat function. */
#define HAVE_LSTAT 1

/* Define if you have the mbrlen function. */
#define HAVE_MBRLEN 1

/* Define if you have the mbrtowc function. */
#define HAVE_MBRTOWC 1

/* Define if you have the mbscasecmp function. */
/* #undef HAVE_MBSCASECMP */

/* Define if you have the mbscmp function. */
/* #undef HAVE_MBSCMP */

/* Define if you have the mbsncmp function. */
/* #undef HAVE_MBSNCMP */

/* Define if you have the mbsrtowcs function. */
#define HAVE_MBSRTOWCS 1

/* Define if you have the memmove function. */
#define HAVE_MEMMOVE 1

/* Define if you have the pselect function.  */
/* #undef HAVE_PSELECT */

/* Define if you have the putenv function.  */
/* #undef HAVE_PUTENV */

/* Define if you have the readlink function.  */
#define HAVE_READLINK 1

/* Define if you have the select function.  */
#define HAVE_SELECT 1

/* Define if you have the setenv function.  */
#define HAVE_SETENV 1

/* Define if you have the setitimer function.  */
/* #undef HAVE_SETITIMER */

/* Define if you have the setlocale function. */
#define HAVE_SETLOCALE 1

/* Define if you have the strcasecmp function.  */
#define HAVE_STRCASECMP 1

/* Define if you have the strcoll function.  */
#define HAVE_STRCOLL 1

/* #undef STRCOLL_BROKEN */

/* Define if you have the strpbrk function.  */
#define HAVE_STRPBRK 1

/* Define if you have the sysconf function.  */
#define HAVE_SYSCONF 1

/* Define if you have the tcgetattr function.  */
#define HAVE_TCGETATTR 1

/* Define if you have the tcgetwinsize function.  */
#define HAVE_TCGETWINSIZE 1

/* Define if you have the tcsetwinsize function.  */
#define HAVE_TCSETWINSIZE 1

/* Define if you have the towlower function.  */
#define HAVE_TOWLOWER 1

/* Define if you have the towupper function.  */
#define HAVE_TOWUPPER 1

/* Define if you have the vsnprintf function.  */
#define HAVE_VSNPRINTF 1

/* Define if you have the wcrtomb function.  */
#define HAVE_WCRTOMB 1

/* Define if you have the wcscoll function.  */
#define HAVE_WCSCOLL 1

/* #undef HAVE_WCSLEN */
/* #undef HAVE_WCSNLEN */

/* Define if you have the wcsnrtombs function.  */
#define HAVE_WCSNRTOMBS 1

/* Define if you have the wctype function.  */
#define HAVE_WCTYPE 1

/* Define if you have the wcwidth function.  */
#define HAVE_WCWIDTH 1

/* and whether it works */
#define WCWIDTH_BROKEN 1

/* Define if you have the <dirent.h> header file.  */
#define HAVE_DIRENT_H 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <langinfo.h> header file.  */
#define HAVE_LANGINFO_H 1

/* Define if you have the <libaudit.h> header file.  */
/* #undef HAVE_LIBAUDIT_H */

/* Define if you have the <limits.h> header file.  */
#define HAVE_LIMITS_H 1

/* Define if you have the <locale.h> header file.  */
#define HAVE_LOCALE_H 1

/* Define if you have the <memory.h> header file.  */
/* #undef HAVE_MEMORY_H */

/* Define if you have the <ndir.h> header file.  */
/* #undef HAVE_NDIR_H */

/* Define if you have the <ncurses/termcap.h> header file.  */
/* #undef HAVE_NCURSES_TERMCAP_H */

/* Define if you have the <pwd.h> header file.  */
#define HAVE_PWD_H 1

/* Define if you have the <stdarg.h> header file.  */
#define HAVE_STDARG_H 1

/* Define if you have the <stdbool.h> header file.  */
#define HAVE_STDBOOL_H 1

/* Define if you have the <stdlib.h> header file.  */
#define HAVE_STDLIB_H 1

/* Define if you have the <string.h> header file.  */
#define HAVE_STRING_H 1

/* Define if you have the <strings.h> header file.  */
#define HAVE_STRINGS_H 1

/* Define if you have the <sys/dir.h> header file.  */
/* #undef HAVE_SYS_DIR_H */

/* Define if you have the <sys/file.h> header file.  */
/* #undef HAVE_SYS_FILE_H */

/* Define if you have the <sys/ioctl.h> header file.  */
/* #undef HAVE_SYS_IOCTL_H */

/* Define if you have the <sys/ndir.h> header file.  */
/* #undef HAVE_SYS_NDIR_H */

/* Define if you have the <sys/pte.h> header file.  */
/* #undef HAVE_SYS_PTE_H */

/* Define if you have the <sys/ptem.h> header file.  */
/* #undef HAVE_SYS_PTEM_H */

/* Define if you have the <sys/select.h> header file.  */
#define HAVE_SYS_SELECT_H 1

/* Define if you have the <sys/stream.h> header file.  */
/* #undef HAVE_SYS_STREAM_H */

/* Define if you have the <sys/time.h> header file.  */
#define HAVE_SYS_TIME_H 1

/* Define if you have the <termcap.h> header file.  */
#define HAVE_TERMCAP_H 1

/* Define if you have the <termio.h> header file.  */
/* #undef HAVE_TERMIO_H */

/* Define if you have the <termios.h> header file.  */
#define HAVE_TERMIOS_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

/* Define if you have the <varargs.h> header file.  */
/* #undef HAVE_VARARGS_H */

/* Define if you have the <wchar.h> header file.  */
#define HAVE_WCHAR_H 1

/* Define if you have the <wctype.h> header file.  */
#define HAVE_WCTYPE_H 1

#define HAVE_MBSTATE_T 1

/* Define if you have wchar_t in <wctype.h>. */
#define HAVE_WCHAR_T 1

/* Define if you have wctype_t in <wctype.h>. */
#define HAVE_WCTYPE_T 1

/* Define if you have wint_t in <wctype.h>. */
#define HAVE_WINT_T 1

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#define HAVE_LANGINFO_CODESET 1

/* Define if you have <linux/audit.h> and it defines AUDIT_USER_TTY */
#define HAVE_DECL_AUDIT_USER_TTY 0

/* Definitions pulled in from aclocal.m4. */
/* #undef GWINSZ_IN_SYS_IOCTL */

/* #undef STRUCT_WINSIZE_IN_SYS_IOCTL */

/* #undef STRUCT_WINSIZE_IN_TERMIOS */

/* #undef TIOCSTAT_IN_SYS_IOCTL */

/* #undef FIONREAD_IN_SYS_IOCTL */

/* #undef SPEED_T_IN_SYS_TYPES */

#define HAVE_GETPW_DECLS 1

#define HAVE_STRUCT_DIRENT_D_INO 1

/* #undef HAVE_STRUCT_DIRENT_D_FILENO */

/* #undef HAVE_STRUCT_DIRENT_D_NAMLEN */

#define HAVE_TIMEVAL 1

/* #undef HAVE_BSD_SIGNALS */

#define HAVE_POSIX_SIGNALS 1

/* #undef HAVE_USG_SIGHOLD */

/* #undef MUST_REINSTALL_SIGHANDLERS */

#define HAVE_POSIX_SIGSETJMP 1

/* #undef CTYPE_NON_ASCII */

/* modify settings or make new ones based on what autoconf tells us. */

/* Ultrix botches type-ahead when switching from canonical to
   non-canonical mode, at least through version 4.3 */
#if !defined (HAVE_TERMIOS_H) || !defined (HAVE_TCGETATTR) || defined (ultrix)
#  define TERMIOS_MISSING
#endif

/* VARARGS defines moved to rlstdc.h */
