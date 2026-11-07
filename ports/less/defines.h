/* User preferences */
#define SECURE_COMPILE  0
#define SECURE          0
#define SHELL_ESCAPE    1
#define EXAMINE         1
#define TAB_COMPLETE_FILENAME 1
#define CMD_HISTORY     1
#define HILITE_SEARCH   1
#define EDITOR          1
#define EDIT_PGM        "vi"
#define TAGS            1
#define USERFILE        1
#define GLOB            1
#define PIPEC           0
#define LOGFILE         1
#define OSC8_LINK       1
#define GNU_OPTIONS     1
#define ONLY_RETURN     0

/* Key file paths */
#define LESSKEYFILE             ".less"
#define LESSKEYFILE_SYS         "/etc/sysless"
#define DEF_LESSKEYINFILE       ".lesskey"
#define LESSKEYINFILE_SYS       "/etc/syslesskey"
#define LESSHISTFILE            ".lesshst"
#define HAVE_LESSKEYSRC         1

/* Platform - POSIX, not Windows */
#define PATHNAME_SEP    "/"
#define DEF_METACHARS   "; *?\t\n'\"()<>|&"
#define DEF_METAESCAPE  "\\"

/* Compiler */
#define HAVE_ANSI_PROTOS 1
#define HAVE_CONST  1
#define HAVE_VOID   1
#define RETSIGTYPE  void

/* Standard headers */
#define HAVE_SYS_TYPES_H    1
#define HAVE_STDIO_H        1
#define HAVE_FCNTL_H        1
#define HAVE_UNISTD_H       1
#define HAVE_CTYPE_H        1
#define HAVE_LIMITS_H       1
#define HAVE_STDLIB_H       1
#define HAVE_STRING_H       1
#define HAVE_STDINT_H       1
#define HAVE_ERRNO_H        1
#define HAVE_TIME_H         1
#define HAVE_SYS_STAT_H     1
#define HAVE_SYS_IOCTL_H    0
#define HAVE_TERMIOS_H      1
#define HAVE_TERMCAP_H      1
#define HAVE_SGSTAT_H       0
#define HAVE_SYS_STREAM_H   0
#define HAVE_SYS_WAIT_H     0
#define HAVE_STRINGS_H      1
#define HAVE_INTTYPES_H     0
#define HAVE_STDCKDINT_H    0
#define HAVE_VALUES_H       0
#define HAVE_TERMINFO       1
#define HAVE_NCURSES_TERMCAP_H 1

/* Functions */
#define HAVE_PERROR         1
#define HAVE_TIME           1
#define HAVE_TIME_T         1
#define HAVE_SHELL          0
#define HAVE_DUP            1
#define HAVE_MEMCPY         1
#define HAVE_STRCHR         1
#define HAVE_STRSTR         1
#define HAVE_SNPRINTF       1
#define HAVE_STAT           1
#define HAVE_STAT_INO       0
#define HAVE_FILENO         1
#define HAVE_POPEN          1
#define HAVE_REALPATH       1
#define HAVE_NANOSLEEP      1
#define HAVE_STRERROR       1
#define HAVE_SYSTEM         1
#define HAVE_SETTABLE_ERRNO 1
#define HAVE_UPPER_LOWER    1
#define HAVE_TTYNAME        1
#define HAVE_FCHMOD         1
#define HAVE_FSYNC          1
#define HAVE_USLEEP         0
#define HAVE_POLL           1
#define HAVE_LOCALE         0
#define HAVE_STRSIGNAL      1
#define HAVE_SYS_ERRLIST    0

/* Terminal */
#define HAVE_TERMIOS_FUNCS  1
#define HAVE_OSPEED         0
#define HAVE_TCGETWINSIZE   1

/* Signals */
#define HAVE_SIGEMPTYSET    1
#define HAVE_SIGPROCMASK    1
#define HAVE_SIGSET_T       1
#define HAVE_SIGSETJMP      1
#define HAVE_SIGSETMASK     0

/* Regex: use POSIX regcomp */
#define HAVE_POSIX_REGCOMP  1

/* Terminal */
#define TGETENT_OK  1

/* Buffer sizes */
#define CMDBUF_SIZE     2048
#define UNGOT_SIZE      200
#define LINEBUF_SIZE    1024
#define OUTBUF_SIZE     1024
#define PROMPT_SIZE     2048
#define TERMBUF_SIZE    2048
#define TERMSBUF_SIZE   1024
#define TAGLINE_SIZE    1024
#define TABSTOP_MAX     128
#define LINENUM_POOL    1024

/* Misc */
#define HAVE_ERRNO          1
#define HAVE__SETJMP        1
#define MUST_DEFINE_ERRNO   0
#define MUST_DEFINE_OSPEED  0
