/* lib/config.h.  Generated from config.hin by configure.  */
/* lib/config.hin.  Generated from configure.ac by autoheader.  */

#if defined __GNUC__ && __GNUC__ >= 15 && !defined __clang__
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wtrailing-whitespace"
#endif


/* Witness that <config.h> has been included.  */
#define _GL_CONFIG_H_INCLUDED 1


/* Define if access does not correctly handle trailing slashes. */
#define ACCESS_TRAILING_SLASH_BUG 1

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define to the function xargmatch calls on failures. */
#define ARGMATCH_DIE usage (EXIT_FAILURE)

/* Define to the declaration of the xargmatch failure function. */
#define ARGMATCH_DIE_DECL void usage (int _e)

/* Define if neither thread safety nor multithreading is desired. */
/* #undef AVOID_ANY_THREADS */

/* Brain 16 bit float supported */
#define BF16_SUPPORTED 1

/* Define to the number of bits in type 'ptrdiff_t'. */
/* #undef BITSIZEOF_PTRDIFF_T */

/* Define to the number of bits in type 'sig_atomic_t'. */
/* #undef BITSIZEOF_SIG_ATOMIC_T */

/* Define to the number of bits in type 'size_t'. */
/* #undef BITSIZEOF_SIZE_T */

/* Define to the number of bits in type 'wchar_t'. */
#define BITSIZEOF_WCHAR_T 16

/* Define to the number of bits in type 'wint_t'. */
/* #undef BITSIZEOF_WINT_T */

/* Whether to markup references as bold */
#define BOLD_MAN_REFS 1

/* Define if the c32rtomb function has an incorrect return value. */
/* #undef C32RTOMB_RETVAL_BUG */

/* Define if you wish *printf() functions that have a safe handling of
   non-IEEE-754 'long double' values. */
#define CHECK_PRINTF_SAFE 1

/* Define to 1 if chown fails to change ctime when at least one argument was
   not -1. */
/* #undef CHOWN_CHANGE_TIME_BUG */

/* Define if chown is not POSIX compliant regarding IDs of -1. */
/* #undef CHOWN_FAILS_TO_HONOR_ID_OF_NEGATIVE_ONE */

/* Define if chown modifies symlinks. */
/* #undef CHOWN_MODIFIES_SYMLINK */

/* Define to 1 if chown mishandles trailing slash. */
#define CHOWN_TRAILING_SLASH_BUG 1

/* Define to 1 if using 'alloca.c'. */
/* #undef C_ALLOCA */

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'double'. */
#define DBL_EXPBIT0_BIT 20

/* Define as the word index where to find the exponent of 'double'. */
#define DBL_EXPBIT0_WORD 1

/* Define as the bit index in the word where to find the sign of 'double'. */
/* #undef DBL_SIGNBIT_BIT */

/* Define as the word index where to find the sign of 'double'. */
/* #undef DBL_SIGNBIT_WORD */

/* Define the default level of POSIX conformance. The value is of the form
   YYYYMM, specifying the year and month the standard was adopted. If not
   defined here, it defaults to the value of _POSIX2_VERSION in <unistd.h>.
   Define to 199209 to default to POSIX 1003.2-1992, which makes standard
   programs like 'head', 'tail', and 'sort' accept obsolete options like '+10'
   and '-10'. Define to 200112 to default to POSIX 1003.1-2001, which makes
   these standard programs treat leading-'+' operands as file names and
   require modern usages like '-n 10' instead of '-10'. Whether defined here
   or not, the default can be overridden at run time via the _POSIX2_VERSION
   environment variable. */
/* #undef DEFAULT_POSIX2_VERSION */

/* Define to 1 if /dev/std{in,out,err} and /dev/fd/N, if they exist, might be
   character-special devices whose minor device number is the file descriptor
   number, such as on Solaris. Leave undefined if they are definitely the
   actual files. This determination should be done after any symbolic links
   are followed. */
/* #undef DEV_FD_MIGHT_BE_CHR */

/* Define to 1 for DGUX with <sys/dg_sys_info.h>. */
/* #undef DGUX */

/* the name of the file descriptor member of DIR */
/* #undef DIR_FD_MEMBER_NAME */

#ifdef DIR_FD_MEMBER_NAME
# define DIR_TO_FD(Dir_p) ((Dir_p)->DIR_FD_MEMBER_NAME)
#else
# define DIR_TO_FD(Dir_p) -1
#endif


/* Define to 1 if dlopen exists and libcrypto is linked dynamically. */
/* #undef DLOPEN_LIBCRYPTO */

/* Define to 1 if // is a file system root distinct from /. */
/* #undef DOUBLE_SLASH_IS_DISTINCT_ROOT */

/* Define if struct dirent has a member d_ino that actually works. */
#define D_INO_IN_DIRENT 1

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define to 1 if your platform has fchownat, but it does not reject an empty
   file name. */
/* #undef FCHOWNAT_EMPTY_FILENAME_BUG */

/* Define to 1 if your platform has fchownat, but it cannot perform lchown
   tasks. */
/* #undef FCHOWNAT_NOFOLLOW_BUG */

/* Define this to 1 if F_DUPFD behavior does not match POSIX */
/* #undef FCNTL_DUPFD_BUGGY */

/* Define to nothing if C supports flexible array members, and to 1 if it does
   not. That way, with a declaration like 'struct s { int n; short
   d[FLEXIBLE_ARRAY_MEMBER]; };', the struct hack can be used with pre-C99
   compilers. Use 'FLEXSIZEOF (struct s, d, N * sizeof (short))' to calculate
   the size in bytes of such a struct containing an N-element array. */
#define FLEXIBLE_ARRAY_MEMBER /**/

/* IEEE 16 bit float supported */
#define FLOAT16_SUPPORTED 1

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'float'. */
#define FLT_EXPBIT0_BIT 23

/* Define as the word index where to find the exponent of 'float'. */
#define FLT_EXPBIT0_WORD 0

/* Define as the bit index in the word where to find the sign of 'float'. */
/* #undef FLT_SIGNBIT_BIT */

/* Define as the word index where to find the sign of 'float'. */
/* #undef FLT_SIGNBIT_WORD */

/* Define to 1 if fopen() fails to recognize a trailing slash. */
/* #undef FOPEN_TRAILING_SLASH_BUG */

/* Enable compile-time and run-time bounds-checking, and some warnings,
      without upsetting glibc 2.15+. */
   #if !defined _FORTIFY_SOURCE && defined __OPTIMIZE__ && __OPTIMIZE__
   # define _FORTIFY_SOURCE 2
   #endif


/* Define to 1 if the system's ftello function has the Solaris bug. */
/* #undef FTELLO_BROKEN_AFTER_SWITCHING_FROM_READ_TO_WRITE */

/* Define to 1 if the system's ftello function has the macOS bug. */
/* #undef FTELLO_BROKEN_AFTER_UNGETC */

/* Define to 1 if fflush is known to work on stdin as per POSIX.1-2008 or
   later, 0 if fflush is known to not work, -1 if unknown. */
#define FUNC_FFLUSH_STDIN 1

/* Define to 1 if mkdir mistakenly creates a directory given with a trailing
   dot component. */
/* #undef FUNC_MKDIR_DOT_BUG */

/* Define to 1 if realpath() can malloc memory, always gives an absolute path,
   and handles a trailing slash correctly. */
/* #undef FUNC_REALPATH_NEARLY_WORKS */

/* Define to 1 if realpath() can malloc memory, always gives an absolute path,
   and handles leading slashes and a trailing slash correctly. */
/* #undef FUNC_REALPATH_WORKS */

/* Define to 1 if ungetc is broken when used on arbitrary bytes. */
/* #undef FUNC_UNGETC_BROKEN */

/* Define to 1 if futimesat mishandles a NULL file name. */
/* #undef FUTIMESAT_NULL_BUG */

/* Define to the type of elements in the array argument to 'getgroups'.
   Usually this is either 'int' or 'gid_t'. */
#define GETGROUPS_T gid_t

/* Define this to 1 if getgroups(0,NULL) does not return the number of groups.
   */
/* #undef GETGROUPS_ZERO_BUG */

/* Define this to 'void' or 'struct timezone' to match the system's
   declaration of the second argument to gettimeofday. */
#define GETTIMEOFDAY_TIMEZONE void

/* Define to get faster but larger CRC32 operation. */
#define GL_CRC_SLICE_BY_8 1

/* CRC32 calculation by pclmul hardware instruction enabled */
#ifdef __x86_64__
#define GL_CRC_X86_64_PCLMUL 1
#endif

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module accept shall be considered present. */
#define GNULIB_ACCEPT IN_COREUTILS_GNULIB_TESTS

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module areadlinkat shall be considered present. */
#define GNULIB_AREADLINKAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module areadlinkat-with-size shall be considered
   present. */
#define GNULIB_AREADLINKAT_WITH_SIZE 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module bind shall be considered present. */
#define GNULIB_BIND IN_COREUTILS_GNULIB_TESTS

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module canonicalize shall be considered present. */
#define GNULIB_CANONICALIZE 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module chmodat shall be considered present. */
#define GNULIB_CHMODAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module chownat shall be considered present. */
#define GNULIB_CHOWNAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module close shall be considered present. */
#define GNULIB_CLOSE 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module close-stream shall be considered present. */
#define GNULIB_CLOSE_STREAM 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module connect shall be considered present. */
#define GNULIB_CONNECT IN_COREUTILS_GNULIB_TESTS

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module dirname shall be considered present. */
#define GNULIB_DIRNAME 1

/* Define to 1 if apps call 'exclude' functions from a single thread. */
#define GNULIB_EXCLUDE_SINGLE_THREAD 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module faccessat shall be considered present. */
#define GNULIB_FACCESSAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fcntl-safer shall be considered present. */
#define GNULIB_FCNTL_SAFER 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fdopendir shall be considered present. */
#define GNULIB_FDOPENDIR 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fdutimensat shall be considered present. */
#define GNULIB_FDUTIMENSAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fd-safer-flag shall be considered present. */
#define GNULIB_FD_SAFER_FLAG 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fflush shall be considered present. */
#define GNULIB_FFLUSH 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module filenamecat shall be considered present. */
#define GNULIB_FILENAMECAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fnmatch-gnu shall be considered present. */
#define GNULIB_FNMATCH_GNU 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fopen-gnu shall be considered present. */
#define GNULIB_FOPEN_GNU 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fopen-safer shall be considered present. */
#define GNULIB_FOPEN_SAFER 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module freopen-safer shall be considered present. */
#define GNULIB_FREOPEN_SAFER 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fscanf shall be considered present. */
#define GNULIB_FSCANF 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fstat shall be considered present. */
#define GNULIB_FSTAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module fstatat shall be considered present. */
#define GNULIB_FSTATAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module getcwd shall be considered present. */
#define GNULIB_GETCWD 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module gethostname shall be considered present. */
#define GNULIB_GETHOSTNAME 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module hashcode-string1 shall be considered present. */
#define GNULIB_HASHCODE_STRING1 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module ioctl shall be considered present. */
#define GNULIB_IOCTL IN_COREUTILS_GNULIB_TESTS

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module isblank shall be considered present. */
#define GNULIB_ISBLANK 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module issymlink shall be considered present. */
#define GNULIB_ISSYMLINK 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module issymlinkat shall be considered present. */
#define GNULIB_ISSYMLINKAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module libgmp-mpq shall be considered present. */
#define GNULIB_LIBGMP_MPQ 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module listen shall be considered present. */
#define GNULIB_LISTEN IN_COREUTILS_GNULIB_TESTS

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module localeconv shall be considered present. */
#define GNULIB_LOCALECONV 1

/* Define to the directory where to find the localizations of the translation
   domain 'gnulib', as a C string. */
#define GNULIB_LOCALEDIR "/usr/local/share/locale"

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module lock shall be considered present. */
#define GNULIB_LOCK 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module lstat shall be considered present. */
#define GNULIB_LSTAT 1

/* Do not worry about rare encodings like CP864, EBCDIC, Johab, and Shift JIS
   that glibc does not support. */
#define GNULIB_MBRTOC32_REGULAR 1

/* Define to 1 if apps call mbrtowc, mbrtoc32, and 'regex' functions from a
   single thread. */
#define GNULIB_MBRTOWC_SINGLE_THREAD 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module mcel-prefer shall be considered present. */
#define GNULIB_MCEL_PREFER 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module mkostemp shall be considered present. */
#define GNULIB_MKOSTEMP 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module msvc-nothrow shall be considered present. */
#define GNULIB_MSVC_NOTHROW 1

/* Define to 1 to disable use of VLAs */
#define GNULIB_NO_VLA 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module nstrftime shall be considered present. */
#define GNULIB_NSTRFTIME 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module openat shall be considered present. */
#define GNULIB_OPENAT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module openat-safer shall be considered present. */
#define GNULIB_OPENAT_SAFER 1

/* Define to support parse_datetime2. */
#define GNULIB_PARSE_DATETIME2 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module pipe2-safer shall be considered present. */
#define GNULIB_PIPE2_SAFER 1

/* enable some gnulib portability checks */
/* #undef GNULIB_PORTCHECK */

/* Define to 1 if printf and friends should be labeled with attribute
   "__gnu_printf__" instead of "__printf__" */
/* #undef GNULIB_PRINTF_ATTRIBUTE_FLAVOR_GNU */

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module reallocarray shall be considered present. */
#define GNULIB_REALLOCARRAY 1

/* Define to 1 if apps call 'regex' functions from a single thread. */
#define GNULIB_REGEX_SINGLE_THREAD 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module scanf shall be considered present. */
#define GNULIB_SCANF 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module select shall be considered present. */
#define GNULIB_SELECT 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module setsockopt shall be considered present. */
#define GNULIB_SETSOCKOPT IN_COREUTILS_GNULIB_TESTS

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module snprintf shall be considered present. */
#define GNULIB_SNPRINTF 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module socket shall be considered present. */
#define GNULIB_SOCKET IN_COREUTILS_GNULIB_TESTS

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module stat shall be considered present. */
#define GNULIB_STAT 1

/* Define to 1 if you want the FILE stream functions getc, putc, etc. to use
   unlocked I/O if available, throughout the package. Unlocked I/O can improve
   performance, sometimes dramatically. But unlocked I/O is safe only in
   processes in which two threads never simultaneously access the same FILE
   stream. */
#define GNULIB_STDIO_SINGLE_THREAD 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module strerror shall be considered present. */
#define GNULIB_STRERROR 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module strerror_r-posix shall be considered present. */
#define GNULIB_STRERROR_R_POSIX IN_COREUTILS_GNULIB_TESTS

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module tempname shall be considered present. */
#define GNULIB_TEMPNAME 1

/* Define to 1 when the gnulib module accept should be tested. */
#define GNULIB_TEST_ACCEPT 1

/* Define to 1 when the gnulib module access should be tested. */
#define GNULIB_TEST_ACCESS 1

/* Define to 1 when the gnulib module atoll should be tested. */
#define GNULIB_TEST_ATOLL 1

/* Define to 1 when the gnulib module bind should be tested. */
#define GNULIB_TEST_BIND 1

/* Define to 1 when the gnulib module btoc32 should be tested. */
#define GNULIB_TEST_BTOC32 1

/* Define to 1 when the gnulib module btowc should be tested. */
#define GNULIB_TEST_BTOWC 1

/* Define to 1 when the gnulib module c32isalnum should be tested. */
#define GNULIB_TEST_C32ISALNUM 1

/* Define to 1 when the gnulib module c32isalpha should be tested. */
#define GNULIB_TEST_C32ISALPHA 1

/* Define to 1 when the gnulib module c32isblank should be tested. */
#define GNULIB_TEST_C32ISBLANK 1

/* Define to 1 when the gnulib module c32iscntrl should be tested. */
#define GNULIB_TEST_C32ISCNTRL 1

/* Define to 1 when the gnulib module c32isdigit should be tested. */
#define GNULIB_TEST_C32ISDIGIT 1

/* Define to 1 when the gnulib module c32isgraph should be tested. */
#define GNULIB_TEST_C32ISGRAPH 1

/* Define to 1 when the gnulib module c32islower should be tested. */
#define GNULIB_TEST_C32ISLOWER 1

/* Define to 1 when the gnulib module c32isprint should be tested. */
#define GNULIB_TEST_C32ISPRINT 1

/* Define to 1 when the gnulib module c32ispunct should be tested. */
#define GNULIB_TEST_C32ISPUNCT 1

/* Define to 1 when the gnulib module c32isspace should be tested. */
#define GNULIB_TEST_C32ISSPACE 1

/* Define to 1 when the gnulib module c32isupper should be tested. */
#define GNULIB_TEST_C32ISUPPER 1

/* Define to 1 when the gnulib module c32isxdigit should be tested. */
#define GNULIB_TEST_C32ISXDIGIT 1

/* Define to 1 when the gnulib module c32rtomb should be tested. */
#define GNULIB_TEST_C32RTOMB 1

/* Define to 1 when the gnulib module c32tob should be tested. */
#define GNULIB_TEST_C32TOB 1

/* Define to 1 when the gnulib module c32tolower should be tested. */
#define GNULIB_TEST_C32TOLOWER 1

/* Define to 1 when the gnulib module c32width should be tested. */
#define GNULIB_TEST_C32WIDTH 1

/* Define to 1 when the gnulib module c32_apply_type_test should be tested. */
#define GNULIB_TEST_C32_APPLY_TYPE_TEST 1

/* Define to 1 when the gnulib module c32_get_type_test should be tested. */
#define GNULIB_TEST_C32_GET_TYPE_TEST 1

/* Define to 1 when the gnulib module calloc-gnu should be tested. */
#define GNULIB_TEST_CALLOC_GNU 1

/* Define to 1 when the gnulib module calloc-posix should be tested. */
#define GNULIB_TEST_CALLOC_POSIX 1

/* Define to 1 when the gnulib module canonicalize should be tested. */
#define GNULIB_TEST_CANONICALIZE 1

/* Define to 1 when the gnulib module canonicalize_file_name should be tested.
   */
#define GNULIB_TEST_CANONICALIZE_FILE_NAME 1

/* Define to 1 when the gnulib module chdir should be tested. */
#define GNULIB_TEST_CHDIR 1

/* Define to 1 when the gnulib module chmod should be tested. */
#define GNULIB_TEST_CHMOD 1

/* Define to 1 when the gnulib module chown should be tested. */
#define GNULIB_TEST_CHOWN 1

/* Define to 1 when the gnulib module cloexec should be tested. */
#define GNULIB_TEST_CLOEXEC 1

/* Define to 1 when the gnulib module close should be tested. */
#define GNULIB_TEST_CLOSE 1

/* Define to 1 when the gnulib module closedir should be tested. */
#define GNULIB_TEST_CLOSEDIR 1

/* Define to 1 when the gnulib module connect should be tested. */
#define GNULIB_TEST_CONNECT 1

/* Define to 1 when the gnulib module copy-file-range should be tested. */
#define GNULIB_TEST_COPY_FILE_RANGE 1

/* Define to 1 when the gnulib module creat should be tested. */
#define GNULIB_TEST_CREAT 1

/* Define to 1 when the gnulib module dirfd should be tested. */
#define GNULIB_TEST_DIRFD 1

/* Define to 1 when the gnulib module dup should be tested. */
#define GNULIB_TEST_DUP 1

/* Define to 1 when the gnulib module dup2 should be tested. */
#define GNULIB_TEST_DUP2 1

/* Define to 1 when the gnulib module environ should be tested. */
#define GNULIB_TEST_ENVIRON 1

/* Define to 1 when the gnulib module euidaccess should be tested. */
#define GNULIB_TEST_EUIDACCESS 1

/* Define to 1 when the gnulib module faccessat should be tested. */
#define GNULIB_TEST_FACCESSAT 1

/* Define to 1 when the gnulib module fchdir should be tested. */
#define GNULIB_TEST_FCHDIR 1

/* Define to 1 when the gnulib module fchmodat should be tested. */
#define GNULIB_TEST_FCHMODAT 1

/* Define to 1 when the gnulib module fchownat should be tested. */
#define GNULIB_TEST_FCHOWNAT 1

/* Define to 1 when the gnulib module fclose should be tested. */
#define GNULIB_TEST_FCLOSE 1

/* Define to 1 when the gnulib module fcntl should be tested. */
#define GNULIB_TEST_FCNTL 1

/* Define to 1 when the gnulib module fdatasync should be tested. */
#define GNULIB_TEST_FDATASYNC 1

/* Define to 1 when the gnulib module fdopen should be tested. */
#define GNULIB_TEST_FDOPEN 1

/* Define to 1 when the gnulib module fdopendir should be tested. */
#define GNULIB_TEST_FDOPENDIR 1

/* Define to 1 when the gnulib module fflush should be tested. */
#define GNULIB_TEST_FFLUSH 1

/* Define to 1 when the gnulib module fgetc should be tested. */
#define GNULIB_TEST_FGETC 1

/* Define to 1 when the gnulib module fgets should be tested. */
#define GNULIB_TEST_FGETS 1

/* Define to 1 when the gnulib module fnmatch should be tested. */
#define GNULIB_TEST_FNMATCH 1

/* Define to 1 when the gnulib module fopen should be tested. */
#define GNULIB_TEST_FOPEN 1

/* Define to 1 when the gnulib module fopen-gnu should be tested. */
#define GNULIB_TEST_FOPEN_GNU 1

/* Define to 1 when the gnulib module fprintf should be tested. */
#define GNULIB_TEST_FPRINTF 1

/* Define to 1 when the gnulib module fpurge should be tested. */
#define GNULIB_TEST_FPURGE 1

/* Define to 1 when the gnulib module fputc should be tested. */
#define GNULIB_TEST_FPUTC 1

/* Define to 1 when the gnulib module fputs should be tested. */
#define GNULIB_TEST_FPUTS 1

/* Define to 1 when the gnulib module fread should be tested. */
#define GNULIB_TEST_FREAD 1

/* Define to 1 when the gnulib module free-posix should be tested. */
#define GNULIB_TEST_FREE_POSIX 1

/* Define to 1 when the gnulib module freopen should be tested. */
#define GNULIB_TEST_FREOPEN 1

/* Define to 1 when the gnulib module frexp should be tested. */
#define GNULIB_TEST_FREXP 1

/* Define to 1 when the gnulib module frexpl should be tested. */
#define GNULIB_TEST_FREXPL 1

/* Define to 1 when the gnulib module fscanf should be tested. */
#define GNULIB_TEST_FSCANF 1

/* Define to 1 when the gnulib module fseek should be tested. */
#define GNULIB_TEST_FSEEK 1

/* Define to 1 when the gnulib module fseeko should be tested. */
#define GNULIB_TEST_FSEEKO 1

/* Define to 1 when the gnulib module fstat should be tested. */
#define GNULIB_TEST_FSTAT 1

/* Define to 1 when the gnulib module fstatat should be tested. */
#define GNULIB_TEST_FSTATAT 1

/* Define to 1 when the gnulib module fsync should be tested. */
#define GNULIB_TEST_FSYNC 1

/* Define to 1 when the gnulib module ftell should be tested. */
#define GNULIB_TEST_FTELL 1

/* Define to 1 when the gnulib module ftello should be tested. */
#define GNULIB_TEST_FTELLO 1

/* Define to 1 when the gnulib module ftruncate should be tested. */
#define GNULIB_TEST_FTRUNCATE 1

/* Define to 1 when the gnulib module futimens should be tested. */
#define GNULIB_TEST_FUTIMENS 1

/* Define to 1 when the gnulib module fwrite should be tested. */
#define GNULIB_TEST_FWRITE 1

/* Define to 1 when the gnulib module getaddrinfo should be tested. */
#define GNULIB_TEST_GETADDRINFO 1

/* Define to 1 when the gnulib module getc should be tested. */
#define GNULIB_TEST_GETC 1

/* Define to 1 when the gnulib module getchar should be tested. */
#define GNULIB_TEST_GETCHAR 1

/* Define to 1 when the gnulib module getcwd should be tested. */
#define GNULIB_TEST_GETCWD 1

/* Define to 1 when the gnulib module getdelim should be tested. */
#define GNULIB_TEST_GETDELIM 1

/* Define to 1 when the gnulib module getdtablesize should be tested. */
#define GNULIB_TEST_GETDTABLESIZE 1

/* Define to 1 when the gnulib module getgroups should be tested. */
#define GNULIB_TEST_GETGROUPS 1

/* Define to 1 when the gnulib module gethostname should be tested. */
#define GNULIB_TEST_GETHOSTNAME 1

/* Define to 1 when the gnulib module getline should be tested. */
#define GNULIB_TEST_GETLINE 1

/* Define to 1 when the gnulib module getloadavg should be tested. */
#define GNULIB_TEST_GETLOADAVG 1

/* Define to 1 when the gnulib module getlocalename_l-unsafe should be tested.
   */
#define GNULIB_TEST_GETLOCALENAME_L_UNSAFE 1

/* Define to 1 when the gnulib module getlogin should be tested. */
#define GNULIB_TEST_GETLOGIN 1

/* Define to 1 when the gnulib module getopt-posix should be tested. */
#define GNULIB_TEST_GETOPT_POSIX 1

/* Define to 1 when the gnulib module getpagesize should be tested. */
#define GNULIB_TEST_GETPAGESIZE 1

/* Define to 1 when the gnulib module getprogname should be tested. */
#define GNULIB_TEST_GETPROGNAME 1

/* Define to 1 when the gnulib module getrandom should be tested. */
#define GNULIB_TEST_GETRANDOM 1

/* Define to 1 when the gnulib module getrusage should be tested. */
#define GNULIB_TEST_GETRUSAGE 1

/* Define to 1 when the gnulib module gettimeofday should be tested. */
#define GNULIB_TEST_GETTIMEOFDAY 1

/* Define to 1 when the gnulib module group-member should be tested. */
#define GNULIB_TEST_GROUP_MEMBER 1

/* Define to 1 when the gnulib module ioctl should be tested. */
#define GNULIB_TEST_IOCTL 1

/* Define to 1 when the gnulib module isatty should be tested. */
#define GNULIB_TEST_ISATTY 1

/* Define to 1 when the gnulib module isinf-no-cxx should be tested. */
#define GNULIB_TEST_ISINF_NO_CXX 1

/* Define to 1 when the gnulib module iswblank should be tested. */
#define GNULIB_TEST_ISWBLANK 1

/* Define to 1 when the gnulib module iswctype should be tested. */
#define GNULIB_TEST_ISWCTYPE 1

/* Define to 1 when the gnulib module iswdigit should be tested. */
#define GNULIB_TEST_ISWDIGIT 1

/* Define to 1 when the gnulib module iswpunct should be tested. */
#define GNULIB_TEST_ISWPUNCT 1

/* Define to 1 when the gnulib module iswxdigit should be tested. */
#define GNULIB_TEST_ISWXDIGIT 1

/* Define to 1 when the gnulib module lchmod should be tested. */
#define GNULIB_TEST_LCHMOD 1

/* Define to 1 when the gnulib module lchown should be tested. */
#define GNULIB_TEST_LCHOWN 1

/* Define to 1 when the gnulib module link should be tested. */
#define GNULIB_TEST_LINK 1

/* Define to 1 when the gnulib module linkat should be tested. */
#define GNULIB_TEST_LINKAT 1

/* Define to 1 when the gnulib module listen should be tested. */
#define GNULIB_TEST_LISTEN 1

/* Define to 1 when the gnulib module localeconv should be tested. */
#define GNULIB_TEST_LOCALECONV 1

/* Define to 1 when the gnulib module localename-environ should be tested. */
#define GNULIB_TEST_LOCALENAME_ENVIRON 1

/* Define to 1 when the gnulib module localename-unsafe should be tested. */
#define GNULIB_TEST_LOCALENAME_UNSAFE 1

/* Define to 1 when the gnulib module lseek should be tested. */
#define GNULIB_TEST_LSEEK 1

/* Define to 1 when the gnulib module lstat should be tested. */
#define GNULIB_TEST_LSTAT 1

/* Define to 1 when the gnulib module malloc-gnu should be tested. */
#define GNULIB_TEST_MALLOC_GNU 1

/* Define to 1 when the gnulib module malloc-posix should be tested. */
#define GNULIB_TEST_MALLOC_POSIX 1

/* Define to 1 when the gnulib module mbrlen should be tested. */
#define GNULIB_TEST_MBRLEN 1

/* Define to 1 when the gnulib module mbrtoc32 should be tested. */
#define GNULIB_TEST_MBRTOC32 1

/* Define to 1 when the gnulib module mbrtowc should be tested. */
#define GNULIB_TEST_MBRTOWC 1

/* Define to 1 when the gnulib module mbscasecmp should be tested. */
#define GNULIB_TEST_MBSCASECMP 1

/* Define to 1 when the gnulib module mbschr should be tested. */
#define GNULIB_TEST_MBSCHR 1

/* Define to 1 when the gnulib module mbsinit should be tested. */
#define GNULIB_TEST_MBSINIT 1

/* Define to 1 when the gnulib module mbslen should be tested. */
#define GNULIB_TEST_MBSLEN 1

/* Define to 1 when the gnulib module mbsnlen should be tested. */
#define GNULIB_TEST_MBSNLEN 1

/* Define to 1 when the gnulib module mbsrtoc32s should be tested. */
#define GNULIB_TEST_MBSRTOC32S 1

/* Define to 1 when the gnulib module mbsrtowcs should be tested. */
#define GNULIB_TEST_MBSRTOWCS 1

/* Define to 1 when the gnulib module mbsstr should be tested. */
#define GNULIB_TEST_MBSSTR 1

/* Define to 1 when the gnulib module mbszero should be tested. */
#define GNULIB_TEST_MBSZERO 1

/* Define to 1 when the gnulib module mbs_endswith should be tested. */
#define GNULIB_TEST_MBS_ENDSWITH 1

/* Define to 1 when the gnulib module mbtowc should be tested. */
#define GNULIB_TEST_MBTOWC 1

/* Define to 1 when the gnulib module memchr should be tested. */
#define GNULIB_TEST_MEMCHR 1

/* Define to 1 when the gnulib module memeq should be tested. */
#define GNULIB_TEST_MEMEQ 1

/* Define to 1 when the gnulib module mempcpy should be tested. */
#define GNULIB_TEST_MEMPCPY 1

/* Define to 1 when the gnulib module memrchr should be tested. */
#define GNULIB_TEST_MEMRCHR 1

/* Define to 1 when the gnulib module memset_explicit should be tested. */
#define GNULIB_TEST_MEMSET_EXPLICIT 1

/* Define to 1 when the gnulib module mkdir should be tested. */
#define GNULIB_TEST_MKDIR 1

/* Define to 1 when the gnulib module mkdirat should be tested. */
#define GNULIB_TEST_MKDIRAT 1

/* Define to 1 when the gnulib module mkfifo should be tested. */
#define GNULIB_TEST_MKFIFO 1

/* Define to 1 when the gnulib module mkfifoat should be tested. */
#define GNULIB_TEST_MKFIFOAT 1

/* Define to 1 when the gnulib module mknod should be tested. */
#define GNULIB_TEST_MKNOD 1

/* Define to 1 when the gnulib module mknodat should be tested. */
#define GNULIB_TEST_MKNODAT 1

/* Define to 1 when the gnulib module mkostemp should be tested. */
#define GNULIB_TEST_MKOSTEMP 1

/* Define to 1 when the gnulib module mkstemp should be tested. */
#define GNULIB_TEST_MKSTEMP 1

/* Define to 1 when the gnulib module mktime should be tested. */
#define GNULIB_TEST_MKTIME 1

/* Define to 1 when the gnulib module nanosleep should be tested. */
#define GNULIB_TEST_NANOSLEEP 1

/* Define to 1 when the gnulib module nl_langinfo should be tested. */
#define GNULIB_TEST_NL_LANGINFO 1

/* Define to 1 when the gnulib module open should be tested. */
#define GNULIB_TEST_OPEN 1

/* Define to 1 when the gnulib module openat should be tested. */
#define GNULIB_TEST_OPENAT 1

/* Define to 1 when the gnulib module opendir should be tested. */
#define GNULIB_TEST_OPENDIR 1

/* Define to 1 when the gnulib module perror should be tested. */
#define GNULIB_TEST_PERROR 1

/* Define to 1 when the gnulib module pipe should be tested. */
#define GNULIB_TEST_PIPE 1

/* Define to 1 when the gnulib module pipe2 should be tested. */
#define GNULIB_TEST_PIPE2 1

/* Define to 1 when the gnulib module posix_memalign should be tested. */
#define GNULIB_TEST_POSIX_MEMALIGN 1

/* Define to 1 when the gnulib module posix_spawn should be tested. */
#define GNULIB_TEST_POSIX_SPAWN 1

/* Define to 1 when the gnulib module posix_spawnattr_destroy should be
   tested. */
#define GNULIB_TEST_POSIX_SPAWNATTR_DESTROY 1

/* Define to 1 when the gnulib module posix_spawnattr_init should be tested.
   */
#define GNULIB_TEST_POSIX_SPAWNATTR_INIT 1

/* Define to 1 when the gnulib module posix_spawnattr_setflags should be
   tested. */
#define GNULIB_TEST_POSIX_SPAWNATTR_SETFLAGS 1

/* Define to 1 when the gnulib module posix_spawnattr_setsigdefault should be
   tested. */
#define GNULIB_TEST_POSIX_SPAWNATTR_SETSIGDEFAULT 1

/* Define to 1 when the gnulib module posix_spawnattr_setsigmask should be
   tested. */
#define GNULIB_TEST_POSIX_SPAWNATTR_SETSIGMASK 1

/* Define to 1 when the gnulib module posix_spawnp should be tested. */
#define GNULIB_TEST_POSIX_SPAWNP 1

/* Define to 1 when the gnulib module posix_spawn_file_actions_addclose should
   be tested. */
#define GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_ADDCLOSE 1

/* Define to 1 when the gnulib module posix_spawn_file_actions_adddup2 should
   be tested. */
#define GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_ADDDUP2 1

/* Define to 1 when the gnulib module posix_spawn_file_actions_addopen should
   be tested. */
#define GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_ADDOPEN 1

/* Define to 1 when the gnulib module posix_spawn_file_actions_destroy should
   be tested. */
#define GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_DESTROY 1

/* Define to 1 when the gnulib module posix_spawn_file_actions_init should be
   tested. */
#define GNULIB_TEST_POSIX_SPAWN_FILE_ACTIONS_INIT 1

/* Define to 1 when the gnulib module printf should be tested. */
#define GNULIB_TEST_PRINTF 1

/* Define to 1 when the gnulib module pselect should be tested. */
#define GNULIB_TEST_PSELECT 1

/* Define to 1 when the gnulib module pthread-cond should be tested. */
#define GNULIB_TEST_PTHREAD_COND 1

/* Define to 1 when the gnulib module pthread-mutex should be tested. */
#define GNULIB_TEST_PTHREAD_MUTEX 1

/* Define to 1 when the gnulib module pthread_mutex_timedlock should be
   tested. */
#define GNULIB_TEST_PTHREAD_MUTEX_TIMEDLOCK 1

/* Define to 1 when the gnulib module pthread-once should be tested. */
#define GNULIB_TEST_PTHREAD_ONCE 1

/* Define to 1 when the gnulib module pthread-rwlock should be tested. */
#define GNULIB_TEST_PTHREAD_RWLOCK 1

/* Define to 1 when the gnulib module pthread_sigmask should be tested. */
#define GNULIB_TEST_PTHREAD_SIGMASK 1

/* Define to 1 when the gnulib module pthread-thread should be tested. */
#define GNULIB_TEST_PTHREAD_THREAD 1

/* Define to 1 when the gnulib module putc should be tested. */
#define GNULIB_TEST_PUTC 1

/* Define to 1 when the gnulib module putchar should be tested. */
#define GNULIB_TEST_PUTCHAR 1

/* Define to 1 when the gnulib module putenv should be tested. */
#define GNULIB_TEST_PUTENV 1

/* Define to 1 when the gnulib module puts should be tested. */
#define GNULIB_TEST_PUTS 1

/* Define to 1 when the gnulib module qsort_r should be tested. */
#define GNULIB_TEST_QSORT_R 1

/* Define to 1 when the gnulib module raise should be tested. */
#define GNULIB_TEST_RAISE 1

/* Define to 1 when the gnulib module random should be tested. */
#define GNULIB_TEST_RANDOM 1

/* Define to 1 when the gnulib module random_r should be tested. */
#define GNULIB_TEST_RANDOM_R 1

/* Define to 1 when the gnulib module rawmemchr should be tested. */
#define GNULIB_TEST_RAWMEMCHR 1

/* Define to 1 when the gnulib module read should be tested. */
#define GNULIB_TEST_READ 1

/* Define to 1 when the gnulib module readdir should be tested. */
#define GNULIB_TEST_READDIR 1

/* Define to 1 when the gnulib module readlink should be tested. */
#define GNULIB_TEST_READLINK 1

/* Define to 1 when the gnulib module readlinkat should be tested. */
#define GNULIB_TEST_READLINKAT 1

/* Define to 1 when the gnulib module reallocarray should be tested. */
#define GNULIB_TEST_REALLOCARRAY 1

/* Define to 1 when the gnulib module realloc-posix should be tested. */
#define GNULIB_TEST_REALLOC_POSIX 1

/* Define to 1 when the gnulib module rename should be tested. */
#define GNULIB_TEST_RENAME 1

/* Define to 1 when the gnulib module renameat should be tested. */
#define GNULIB_TEST_RENAMEAT 1

/* Define to 1 when the gnulib module rewinddir should be tested. */
#define GNULIB_TEST_REWINDDIR 1

/* Define to 1 when the gnulib module rmdir should be tested. */
#define GNULIB_TEST_RMDIR 1

/* Define to 1 when the gnulib module rpmatch should be tested. */
#define GNULIB_TEST_RPMATCH 1

/* Define to 1 when the gnulib module scanf should be tested. */
#define GNULIB_TEST_SCANF 1

/* Define to 1 when the gnulib module sched_yield should be tested. */
#define GNULIB_TEST_SCHED_YIELD 1

/* Define to 1 when the gnulib module secure_getenv should be tested. */
#define GNULIB_TEST_SECURE_GETENV 1

/* Define to 1 when the gnulib module select should be tested. */
#define GNULIB_TEST_SELECT 1

/* Define to 1 when the gnulib module setenv should be tested. */
#define GNULIB_TEST_SETENV 1

/* Define to 1 when the gnulib module setlocale should be tested. */
#define GNULIB_TEST_SETLOCALE 1

/* Define to 1 when the gnulib module setlocale_null should be tested. */
#define GNULIB_TEST_SETLOCALE_NULL 1

/* Define to 1 when the gnulib module setsockopt should be tested. */
#define GNULIB_TEST_SETSOCKOPT 1

/* Define to 1 when the gnulib module sig2str should be tested. */
#define GNULIB_TEST_SIG2STR 1

/* Define to 1 when the gnulib module sigaction should be tested. */
#define GNULIB_TEST_SIGACTION 1

/* Define to 1 when the gnulib module signbit should be tested. */
#define GNULIB_TEST_SIGNBIT 1

/* Define to 1 when the gnulib module signbit-no-cxx should be tested. */
#define GNULIB_TEST_SIGNBIT_NO_CXX 1

/* Define to 1 when the gnulib module sigprocmask should be tested. */
#define GNULIB_TEST_SIGPROCMASK 1

/* Define to 1 when the gnulib module sleep should be tested. */
#define GNULIB_TEST_SLEEP 1

/* Define to 1 when the gnulib module snprintf should be tested. */
#define GNULIB_TEST_SNPRINTF 1

/* Define to 1 when the gnulib module socket should be tested. */
#define GNULIB_TEST_SOCKET 1

/* Define to 1 when the gnulib module stat should be tested. */
#define GNULIB_TEST_STAT 1

/* Define to 1 when the gnulib module stdc_bit_width should be tested. */
#define GNULIB_TEST_STDC_BIT_WIDTH 1

/* Define to 1 when the gnulib module stdc_leading_zeros should be tested. */
#define GNULIB_TEST_STDC_LEADING_ZEROS 1

/* Define to 1 when the gnulib module stdc_memreverse8u should be tested. */
#define GNULIB_TEST_STDC_MEMREVERSE8U 1

/* Define to 1 when the gnulib module stdc_trailing_zeros should be tested. */
#define GNULIB_TEST_STDC_TRAILING_ZEROS 1

/* Define to 1 when the gnulib module stpcpy should be tested. */
#define GNULIB_TEST_STPCPY 1

/* Define to 1 when the gnulib module strchrnul should be tested. */
#define GNULIB_TEST_STRCHRNUL 1

/* Define to 1 when the gnulib module strdup should be tested. */
#define GNULIB_TEST_STRDUP 1

/* Define to 1 when the gnulib module streq should be tested. */
#define GNULIB_TEST_STREQ 1

/* Define to 1 when the gnulib module strerror should be tested. */
#define GNULIB_TEST_STRERROR 1

/* Define to 1 when the gnulib module strerrorname_np should be tested. */
#define GNULIB_TEST_STRERRORNAME_NP 1

/* Define to 1 when the gnulib module strerror_r should be tested. */
#define GNULIB_TEST_STRERROR_R 1

/* Define to 1 when the gnulib module strftime should be tested. */
#define GNULIB_TEST_STRFTIME 1

/* Define to 1 when the gnulib module strncpy should be tested. */
#define GNULIB_TEST_STRNCPY 1

/* Define to 1 when the gnulib module strnlen should be tested. */
#define GNULIB_TEST_STRNLEN 1

/* Define to 1 when the gnulib module strnul should be tested. */
#define GNULIB_TEST_STRNUL 1

/* Define to 1 when the gnulib module strsignal should be tested. */
#define GNULIB_TEST_STRSIGNAL 1

/* Define to 1 when the gnulib module strstr should be tested. */
#define GNULIB_TEST_STRSTR 1

/* Define to 1 when the gnulib module strtod should be tested. */
#define GNULIB_TEST_STRTOD 1

/* Define to 1 when the gnulib module strtold should be tested. */
#define GNULIB_TEST_STRTOLD 1

/* Define to 1 when the gnulib module strtoll should be tested. */
#define GNULIB_TEST_STRTOLL 1

/* Define to 1 when the gnulib module strtoull should be tested. */
#define GNULIB_TEST_STRTOULL 1

/* Define to 1 when the gnulib module str_endswith should be tested. */
#define GNULIB_TEST_STR_ENDSWITH 1

/* Define to 1 when the gnulib module str_startswith should be tested. */
#define GNULIB_TEST_STR_STARTSWITH 1

/* Define to 1 when the gnulib module symlink should be tested. */
#define GNULIB_TEST_SYMLINK 1

/* Define to 1 when the gnulib module symlinkat should be tested. */
#define GNULIB_TEST_SYMLINKAT 1

/* Define to 1 when the gnulib module time should be tested. */
#define GNULIB_TEST_TIME 1

/* Define to 1 when the gnulib module timegm should be tested. */
#define GNULIB_TEST_TIMEGM 1

/* Define to 1 when the gnulib module time_r should be tested. */
#define GNULIB_TEST_TIME_R 1

/* Define to 1 when the gnulib module time_rz should be tested. */
#define GNULIB_TEST_TIME_RZ 1

/* Define to 1 when the gnulib module tmpfile should be tested. */
#define GNULIB_TEST_TMPFILE 1

/* Define to 1 when the gnulib module trunc should be tested. */
#define GNULIB_TEST_TRUNC 1

/* Define to 1 when the gnulib module truncl should be tested. */
#define GNULIB_TEST_TRUNCL 1

/* Define to 1 when the gnulib module tzset should be tested. */
#define GNULIB_TEST_TZSET 1

/* Define to 1 when the gnulib module unlink should be tested. */
#define GNULIB_TEST_UNLINK 1

/* Define to 1 when the gnulib module unlinkat should be tested. */
#define GNULIB_TEST_UNLINKAT 1

/* Define to 1 when the gnulib module unsetenv should be tested. */
#define GNULIB_TEST_UNSETENV 1

/* Define to 1 when the gnulib module usleep should be tested. */
#define GNULIB_TEST_USLEEP 1

/* Define to 1 when the gnulib module utime should be tested. */
#define GNULIB_TEST_UTIME 1

/* Define to 1 when the gnulib module utimensat should be tested. */
#define GNULIB_TEST_UTIMENSAT 1

/* Define to 1 when the gnulib module vasprintf should be tested. */
#define GNULIB_TEST_VASPRINTF 1

/* Define to 1 when the gnulib module vaszprintf should be tested. */
#define GNULIB_TEST_VASZPRINTF 1

/* Define to 1 when the gnulib module vfprintf should be tested. */
#define GNULIB_TEST_VFPRINTF 1

/* Define to 1 when the gnulib module vfzprintf should be tested. */
#define GNULIB_TEST_VFZPRINTF 1

/* Define to 1 when the gnulib module vprintf should be tested. */
#define GNULIB_TEST_VPRINTF 1

/* Define to 1 when the gnulib module vsnzprintf should be tested. */
#define GNULIB_TEST_VSNZPRINTF 1

/* Define to 1 when the gnulib module vzprintf should be tested. */
#define GNULIB_TEST_VZPRINTF 1

/* Define to 1 when the gnulib module waitpid should be tested. */
#define GNULIB_TEST_WAITPID 1

/* Define to 1 when the gnulib module wcrtomb should be tested. */
#define GNULIB_TEST_WCRTOMB 1

/* Define to 1 when the gnulib module wctob should be tested. */
#define GNULIB_TEST_WCTOB 1

/* Define to 1 when the gnulib module wctomb should be tested. */
#define GNULIB_TEST_WCTOMB 1

/* Define to 1 when the gnulib module wctype should be tested. */
#define GNULIB_TEST_WCTYPE 1

/* Define to 1 when the gnulib module wcwidth should be tested. */
#define GNULIB_TEST_WCWIDTH 1

/* Define to 1 when the gnulib module wmemchr should be tested. */
#define GNULIB_TEST_WMEMCHR 1

/* Define to 1 when the gnulib module wmempcpy should be tested. */
#define GNULIB_TEST_WMEMPCPY 1

/* Define to 1 when the gnulib module write should be tested. */
#define GNULIB_TEST_WRITE 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module unistd-safer shall be considered present. */
#define GNULIB_UNISTD_SAFER 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module unistr/u8-mbtoucr shall be considered present. */
#define GNULIB_UNISTR_U8_MBTOUCR 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module unistr/u8-uctomb shall be considered present. */
#define GNULIB_UNISTR_U8_UCTOMB 1

/* Define to 1 if apps don't set the locale after calling locale-sensitive
   functions like mbrtowc and wcwidth. */
#define GNULIB_WCHAR_SINGLE_LOCALE 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module xalloc shall be considered present. */
#define GNULIB_XALLOC 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module xalloc-die shall be considered present. */
#define GNULIB_XALLOC_DIE 1

/* Define to a C preprocessor expression that evaluates to 1 or 0, depending
   whether the gnulib module xgetgroups shall be considered present. */
#define GNULIB_XGETGROUPS 1

/* Define if your system defines TIOCGWINSZ in sys/ioctl.h. */
/* #undef GWINSZ_IN_SYS_IOCTL */

/* Define if your system defines TIOCGWINSZ in sys/pty.h. */
/* #undef GWINSZ_IN_SYS_PTY */

/* Define to 1 if you have the 'access' function. */
#define HAVE_ACCESS 1

/* Define to 1 if you have the 'aclsort' function. */
/* #undef HAVE_ACLSORT */

/* Define to 1 if you have the <aclv.h> header file. */
/* #undef HAVE_ACLV_H */

/* Define to 1 if you have the 'aclx_get' function. */
/* #undef HAVE_ACLX_GET */

/* Define to 1 if you have the 'acl_copy_ext_native' function. */
/* #undef HAVE_ACL_COPY_EXT_NATIVE */

/* Define to 1 if you have the 'acl_create_entry_np' function. */
/* #undef HAVE_ACL_CREATE_ENTRY_NP */

/* Define to 1 if you have the 'acl_delete_def_file' function. */
/* #undef HAVE_ACL_DELETE_DEF_FILE */

/* Define to 1 if you have the 'acl_delete_fd_np' function. */
/* #undef HAVE_ACL_DELETE_FD_NP */

/* Define to 1 if you have the 'acl_delete_file_np' function. */
/* #undef HAVE_ACL_DELETE_FILE_NP */

/* Define to 1 if libc or libacl defines the function acl_entries. */
/* #undef HAVE_ACL_ENTRIES */

/* Define to 1 if you have the 'acl_extended_file' function. */
/* #undef HAVE_ACL_EXTENDED_FILE */

/* Define to 1 if the constant ACL_FIRST_ENTRY exists. */
/* #undef HAVE_ACL_FIRST_ENTRY */

/* Define to 1 if you have the 'acl_free' function. */
/* #undef HAVE_ACL_FREE */

/* Define to 1 if you have the 'acl_free_text' function. */
/* #undef HAVE_ACL_FREE_TEXT */

/* Define to 1 if you have the 'acl_from_mode' function. */
/* #undef HAVE_ACL_FROM_MODE */

/* Define to 1 if you have the 'acl_from_text' function. */
/* #undef HAVE_ACL_FROM_TEXT */

/* Define to 1 if you have the 'acl_get_fd' function. */
/* #undef HAVE_ACL_GET_FD */

/* Define to 1 if you have the 'acl_get_file' function. */
/* #undef HAVE_ACL_GET_FILE */

/* Define to 1 if you have the 'acl_get_link_np' function. */
/* #undef HAVE_ACL_GET_LINK_NP */

/* Define to 1 if you have the <acl/libacl.h> header file. */
/* #undef HAVE_ACL_LIBACL_H */

/* Define to 1 if you have the 'acl_set_fd' function. */
/* #undef HAVE_ACL_SET_FD */

/* Define to 1 if you have the 'acl_set_file' function. */
/* #undef HAVE_ACL_SET_FILE */

/* Define to 1 if you have the 'acl_to_short_text' function. */
/* #undef HAVE_ACL_TO_SHORT_TEXT */

/* Define to 1 if you have the 'acl_trivial' function. */
/* #undef HAVE_ACL_TRIVIAL */

/* Define to 1 if the ACL type ACL_TYPE_EXTENDED exists. */
/* #undef HAVE_ACL_TYPE_EXTENDED */

/* Define to 1 if you have the <afunix.h> header file. */
/* #undef HAVE_AFUNIX_H */

/* Define if the __locale_t type contains the name of the LC_MESSAGES
   category. */
/* #undef HAVE_AIX72_LOCALES */

/* Define to 1 if you have 'alloca' after including <alloca.h>, a header that
   may be supplied by this distribution. */
#define HAVE_ALLOCA 1

/* Define to 1 if <alloca.h> works. */
#define HAVE_ALLOCA_H 1

/* Define if you have an arithmetic hrtime_t type. */
/* #undef HAVE_ARITHMETIC_HRTIME_T */

/* Define to 1 if you have the <arpa/inet.h> header file. */
#define HAVE_ARPA_INET_H 1

/* Define to 1 if you have the 'atoll' function. */
#define HAVE_ATOLL 1

/* Define to 1 if you have the <bcrypt.h> header file. */
/* #undef HAVE_BCRYPT_H */

/* Define to 1 if you have the <bp-sym.h> header file. */
/* #undef HAVE_BP_SYM_H */

/* Define to 1 if you have the 'btowc' function. */
#define HAVE_BTOWC 1

/* Define to 1 if nanosleep mishandles large arguments. */
/* #undef HAVE_BUG_BIG_NANOSLEEP */

/* Define to 1 if you have the <byteswap.h> header file. */
/* #undef HAVE_BYTESWAP_H */

/* Define to 1 if you have the 'canonicalize_file_name' function. */
/* #undef HAVE_CANONICALIZE_FILE_NAME */

/* libcap usability */
/* #undef HAVE_CAP */

/* Define to 1 if you have the `catgets' function. */
/* #undef HAVE_CATGETS */

/* Define to 1 if you have the Mac OS X function
   CFLocaleCopyPreferredLanguages in the CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYPREFERREDLANGUAGES */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Define to 1 if you have the 'chown' function. */
#define HAVE_CHOWN 1

/* Define to 1 if you have the 'chroot' function. */
/* #undef HAVE_CHROOT */

/* Define to 1 if you have the 'clock_getres' function. */
#define HAVE_CLOCK_GETRES 1

/* Define to 1 if you have the 'clock_gettime' function. */
#define HAVE_CLOCK_GETTIME 1

/* Define to 1 if you have the 'clock_settime' function. */
/* #undef HAVE_CLOCK_SETTIME */

/* Define to 1 if you have the 'closedir' function. */
#define HAVE_CLOSEDIR 1

/* Define if you have compound literals. */
#define HAVE_COMPOUND_LITERALS 1

/* Define to 1 if you have the 'confstr' function. */
#define HAVE_CONFSTR 1

/* Define if the copysignf function is declared in <math.h> and available in
   libc. */
/* #undef HAVE_COPYSIGNF_IN_LIBC */

/* Define if the copysignl function is declared in <math.h> and available in
   libc. */
/* #undef HAVE_COPYSIGNL_IN_LIBC */

/* Define if the copysign function is declared in <math.h> and available in
   libc. */
/* #undef HAVE_COPYSIGN_IN_LIBC */

/* Define to 1 if the function copy_file_range exists. */
/* #undef HAVE_COPY_FILE_RANGE */

/* Define to 1 if you have the <crtdefs.h> header file. */
/* #undef HAVE_CRTDEFS_H */

/* Define to 1 if the alignas and alignof keywords work. */
#define HAVE_C_ALIGNASOF 1

/* Define to 1 if bool, true and false work as per C2023. */
#define HAVE_C_BOOL 1

/* FIXME */
#define HAVE_C_LINE 1

/* Define to 1 if C nullptr is known to work. */
#define HAVE_C_NULLPTR 1

/* Define to 1 if the static_assert keyword works. */
#define HAVE_C_STATIC_ASSERT 1

/* Define to 1 if C supports variable-length arrays. */
#define HAVE_C_VARARRAYS 1

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* Define to 1 if you have the declaration of 'alarm', and to 0 if you don't.
   */
#define HAVE_DECL_ALARM 1

/* Define to 1 if you have the declaration of 'clearerr_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_CLEARERR_UNLOCKED 0

/* Define to 1 if you have the declaration of 'copysign', and to 0 if you
   don't. */
/* #undef HAVE_DECL_COPYSIGN */

/* Define to 1 if you have the declaration of 'copysignf', and to 0 if you
   don't. */
/* #undef HAVE_DECL_COPYSIGNF */

/* Define to 1 if you have the declaration of 'copysignl', and to 0 if you
   don't. */
/* #undef HAVE_DECL_COPYSIGNL */

/* Define to 1 if you have the declaration of 'dirfd', and to 0 if you don't.
   */
#define HAVE_DECL_DIRFD 1

/* Define to 1 if you have the declaration of 'ecvt', and to 0 if you don't.
   */
#define HAVE_DECL_ECVT 0

/* Define to 1 if you have the declaration of 'endutent', and to 0 if you
   don't. */
/* #undef HAVE_DECL_ENDUTENT */

/* Define to 1 if you have the declaration of 'execvpe', and to 0 if you
   don't. */
#define HAVE_DECL_EXECVPE 0

/* Define to 1 if you have the declaration of 'fchdir', and to 0 if you don't.
   */
#define HAVE_DECL_FCHDIR 1

/* Define to 1 if you have the declaration of 'fcloseall', and to 0 if you
   don't. */
#define HAVE_DECL_FCLOSEALL 0

/* Define to 1 if you have the declaration of 'fcvt', and to 0 if you don't.
   */
#define HAVE_DECL_FCVT 0

/* Define to 1 if you have the declaration of 'fdatasync', and to 0 if you
   don't. */
#define HAVE_DECL_FDATASYNC 0

/* Define to 1 if you have the declaration of 'fdopendir', and to 0 if you
   don't. */
#define HAVE_DECL_FDOPENDIR 1

/* Define to 1 if you have the declaration of 'feof_unlocked', and to 0 if you
   don't. */
#define HAVE_DECL_FEOF_UNLOCKED 0

/* Define to 1 if you have the declaration of 'ferror_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FERROR_UNLOCKED 0

/* Define to 1 if you have the declaration of 'fflush_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FFLUSH_UNLOCKED 0

/* Define to 1 if you have the declaration of 'fgets_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FGETS_UNLOCKED 0

/* Define to 1 if you have the declaration of 'fileno_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FILENO_UNLOCKED 0

/* Define to 1 if you have the declaration of 'fpurge', and to 0 if you don't.
   */
#define HAVE_DECL_FPURGE 0

/* Define to 1 if you have the declaration of 'fputc_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FPUTC_UNLOCKED 0

/* Define to 1 if you have the declaration of 'fputs_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FPUTS_UNLOCKED 0

/* Define to 1 if you have the declaration of 'fread_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FREAD_UNLOCKED 0

/* Define to 1 if you have the declaration of 'freeaddrinfo', and to 0 if you
   don't. */
#define HAVE_DECL_FREEADDRINFO 1

/* Define to 1 if you have the declaration of 'fseeko', and to 0 if you don't.
   */
#define HAVE_DECL_FSEEKO 1

/* Define to 1 if you have the declaration of 'ftello', and to 0 if you don't.
   */
#define HAVE_DECL_FTELLO 1

/* Define to 1 if you have the declaration of 'fwrite_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_FWRITE_UNLOCKED 0

/* Define to 1 if you have the declaration of 'gai_strerror', and to 0 if you
   don't. */
#define HAVE_DECL_GAI_STRERROR 1

/* Define to 1 if you have the declaration of 'gai_strerrorA', and to 0 if you
   don't. */
#define HAVE_DECL_GAI_STRERRORA 0

/* Define to 1 if you have the declaration of 'gcvt', and to 0 if you don't.
   */
#define HAVE_DECL_GCVT 0

/* Define to 1 if you have the declaration of 'getaddrinfo', and to 0 if you
   don't. */
#define HAVE_DECL_GETADDRINFO 1

/* Define to 1 if you have the declaration of 'getchar_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_GETCHAR_UNLOCKED 0

/* Define to 1 if you have the declaration of 'getcwd', and to 0 if you don't.
   */
#define HAVE_DECL_GETCWD 1

/* Define to 1 if you have the declaration of 'getc_unlocked', and to 0 if you
   don't. */
#define HAVE_DECL_GETC_UNLOCKED 1

/* Define to 1 if you have the declaration of 'getdelim', and to 0 if you
   don't. */
#define HAVE_DECL_GETDELIM 0

/* Define to 1 if you have the declaration of 'getdtablesize', and to 0 if you
   don't. */
#define HAVE_DECL_GETDTABLESIZE 0

/* Define to 1 if you have the declaration of 'geteuid', and to 0 if you
   don't. */
#define HAVE_DECL_GETEUID 1

/* Define to 1 if you have the declaration of 'getgrgid', and to 0 if you
   don't. */
#define HAVE_DECL_GETGRGID 1

/* Define to 1 if you have the declaration of 'gethrtime', and to 0 if you
   don't. */
#define HAVE_DECL_GETHRTIME 0

/* Define to 1 if you have the declaration of 'getline', and to 0 if you
   don't. */
#define HAVE_DECL_GETLINE 1

/* Define to 1 if you have the declaration of 'getlogin', and to 0 if you
   don't. */
#define HAVE_DECL_GETLOGIN 1

/* Define to 1 if you have the declaration of 'getnameinfo', and to 0 if you
   don't. */
#define HAVE_DECL_GETNAMEINFO 1

/* Define to 1 if you have the declaration of 'getpwuid', and to 0 if you
   don't. */
#define HAVE_DECL_GETPWUID 1

/* Define to 1 if you have the declaration of 'getuid', and to 0 if you don't.
   */
#define HAVE_DECL_GETUID 1

/* Define to 1 if you have the declaration of 'getw', and to 0 if you don't.
   */
#define HAVE_DECL_GETW 0

/* Define to 1 if you have the declaration of 'htonl', and to 0 if you don't.
   */
#define HAVE_DECL_HTONL 1

/* Define to 1 if you have the declaration of 'htons', and to 0 if you don't.
   */
#define HAVE_DECL_HTONS 1

/* Define to 1 if you have the declaration of 'inet_ntop', and to 0 if you
   don't. */
#define HAVE_DECL_INET_NTOP 1

/* Define to 1 if you have the declaration of 'inet_pton', and to 0 if you
   don't. */
#define HAVE_DECL_INET_PTON 1

/* Define to 1 if you have the declaration of 'initstate', and to 0 if you
   don't. */
#define HAVE_DECL_INITSTATE 0

/* Define to 1 if you have the declaration of 'isblank', and to 0 if you
   don't. */
#define HAVE_DECL_ISBLANK 1

/* Define to 1 if you have the declaration of 'isinf', and to 0 if you don't.
   */
#define HAVE_DECL_ISINF 1

/* Define to 1 if you have the declaration of 'iswblank', and to 0 if you
   don't. */
#define HAVE_DECL_ISWBLANK 1

/* Define to 1 if you have the declaration of 'localtime_r', and to 0 if you
   don't. */
#define HAVE_DECL_LOCALTIME_R 1

/* Define to 1 if you have the declaration of 'mbrlen', and to 0 if you don't.
   */
/* #undef HAVE_DECL_MBRLEN */

/* Define to 1 if you have the declaration of 'mbrtowc', and to 0 if you
   don't. */
/* #undef HAVE_DECL_MBRTOWC */

/* Define to 1 if you have the declaration of 'mbsinit', and to 0 if you
   don't. */
/* #undef HAVE_DECL_MBSINIT */

/* Define to 1 if you have the declaration of 'mbsrtowcs', and to 0 if you
   don't. */
/* #undef HAVE_DECL_MBSRTOWCS */

/* Define to 1 if you have a declaration of mbswidth() in <wchar.h>, and to 0
   otherwise. */
#define HAVE_DECL_MBSWIDTH_IN_WCHAR_H 0

/* Define to 1 if you have the declaration of 'memeq', and to 0 if you don't.
   */
#define HAVE_DECL_MEMEQ 0

/* Define to 1 if you have the declaration of 'memrchr', and to 0 if you
   don't. */
#define HAVE_DECL_MEMRCHR 0

/* Define to 1 if you have the declaration of 'ntohl', and to 0 if you don't.
   */
#define HAVE_DECL_NTOHL 1

/* Define to 1 if you have the declaration of 'ntohs', and to 0 if you don't.
   */
#define HAVE_DECL_NTOHS 1

/* Define to 1 if you have the declaration of 'posix_spawn', and to 0 if you
   don't. */
#define HAVE_DECL_POSIX_SPAWN 1

/* Define to 1 if you have the declaration of 'program_invocation_name', and
   to 0 if you don't. */
#define HAVE_DECL_PROGRAM_INVOCATION_NAME 0

/* Define to 1 if you have the declaration of 'program_invocation_short_name',
   and to 0 if you don't. */
#define HAVE_DECL_PROGRAM_INVOCATION_SHORT_NAME 0

/* Define to 1 if you have the declaration of 'putchar_unlocked', and to 0 if
   you don't. */
#define HAVE_DECL_PUTCHAR_UNLOCKED 0

/* Define to 1 if you have the declaration of 'putc_unlocked', and to 0 if you
   don't. */
#define HAVE_DECL_PUTC_UNLOCKED 0

/* Define to 1 if you have the declaration of 'putw', and to 0 if you don't.
   */
#define HAVE_DECL_PUTW 0

/* Define to 1 if you have the declaration of 'setenv', and to 0 if you don't.
   */
#define HAVE_DECL_SETENV 1

/* Define to 1 if you have the declaration of 'setregid', and to 0 if you
   don't. */
#define HAVE_DECL_SETREGID 0

/* Define to 1 if you have the declaration of 'setstate', and to 0 if you
   don't. */
#define HAVE_DECL_SETSTATE 0

/* Define to 1 if you have the declaration of 'sleep', and to 0 if you don't.
   */
#define HAVE_DECL_SLEEP 1

/* Define to 1 if you have the declaration of 'snprintf', and to 0 if you
   don't. */
#define HAVE_DECL_SNPRINTF 1

/* Define to 1 if you have the declaration of 'stdc_rotate_left_uc', and to 0
   if you don't. */
/* #undef HAVE_DECL_STDC_ROTATE_LEFT_UC */

/* Define to 1 if you have the declaration of 'strdup', and to 0 if you don't.
   */
#define HAVE_DECL_STRDUP 1

/* Define to 1 if you have the declaration of 'streq', and to 0 if you don't.
   */
#define HAVE_DECL_STREQ 0

/* Define to 1 if you have the declaration of 'strerror_r', and to 0 if you
   don't. */
#define HAVE_DECL_STRERROR_R 1

/* Define to 1 if you have the declaration of 'strmode', and to 0 if you
   don't. */
#define HAVE_DECL_STRMODE 0

/* Define to 1 if you have the declaration of 'strnlen', and to 0 if you
   don't. */
#define HAVE_DECL_STRNLEN 1

/* Define to 1 if you have the declaration of 'strsignal', and to 0 if you
   don't. */
#define HAVE_DECL_STRSIGNAL 1

/* Define to 1 if you have the declaration of 'strtoimax', and to 0 if you
   don't. */
#define HAVE_DECL_STRTOIMAX 1

/* Define to 1 if you have the declaration of 'strtoll', and to 0 if you
   don't. */
/* #undef HAVE_DECL_STRTOLL */

/* Define to 1 if you have the declaration of 'strtoull', and to 0 if you
   don't. */
/* #undef HAVE_DECL_STRTOULL */

/* Define to 1 if you have the declaration of 'strtoumax', and to 0 if you
   don't. */
#define HAVE_DECL_STRTOUMAX 1

/* Define to 1 if you have the declaration of 'sysinfo', and to 0 if you
   don't. */
#define HAVE_DECL_SYSINFO 0

/* Define to 1 if you have the declaration of 'sys_siglist', and to 0 if you
   don't. */
/* #undef HAVE_DECL_SYS_SIGLIST */

/* Define to 1 if you have the declaration of 'towlower', and to 0 if you
   don't. */
/* #undef HAVE_DECL_TOWLOWER */

/* Define to 1 if you have the declaration of 'trunc', and to 0 if you don't.
   */
#define HAVE_DECL_TRUNC 1

/* Define to 1 if you have the declaration of 'truncl', and to 0 if you don't.
   */
#define HAVE_DECL_TRUNCL 1

/* Define to 1 if you have the declaration of 'ttyname', and to 0 if you
   don't. */
#define HAVE_DECL_TTYNAME 1

/* Define to 1 if you have the declaration of 'unsetenv', and to 0 if you
   don't. */
#define HAVE_DECL_UNSETENV 1

/* Define to 1 if you have the declaration of 'wcrtomb', and to 0 if you
   don't. */
/* #undef HAVE_DECL_WCRTOMB */

/* Define to 1 if you have the declaration of 'wcsdup', and to 0 if you don't.
   */
#define HAVE_DECL_WCSDUP 1

/* Define to 1 if you have the declaration of 'wcwidth', and to 0 if you
   don't. */
#define HAVE_DECL_WCWIDTH 1

/* Define to 1 if you have the declaration of '_fseeki64', and to 0 if you
   don't. */
/* #undef HAVE_DECL__FSEEKI64 */

/* Define to 1 if you have the declaration of '_putenv', and to 0 if you
   don't. */
#define HAVE_DECL__PUTENV 0

/* Define to 1 if you have the declaration of '_snprintf', and to 0 if you
   don't. */
#define HAVE_DECL__SNPRINTF 0

/* Define to 1 if you have the declaration of '_sys_siglist', and to 0 if you
   don't. */
/* #undef HAVE_DECL__SYS_SIGLIST */

/* Define to 1 if you have the declaration of '__argv', and to 0 if you don't.
   */
/* #undef HAVE_DECL___ARGV */

/* Define to 1 if you have the declaration of '__fpending', and to 0 if you
   don't. */
#define HAVE_DECL___FPENDING 1

/* Define to 1 if you have the 'directio' function. */
/* #undef HAVE_DIRECTIO */

/* Define to 1 if you have the <dirent.h> header file. */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the 'dirfd' function. */
#define HAVE_DIRFD 1

/* Define to 1 if you have the `duplocale' function. */
/* #undef HAVE_DUPLOCALE */

/* Define to 1 if you have the 'eaccess' function. */
/* #undef HAVE_EACCESS */

/* Define to 1 if you have the 'endgrent' function. */
#define HAVE_ENDGRENT 1

/* Define to 1 if you have the <endian.h> header file. */
#define HAVE_ENDIAN_H 1

/* Define to 1 if you have the `endmntent' function. */
/* #undef HAVE_ENDMNTENT */

/* Define to 1 if you have the 'endpwent' function. */
#define HAVE_ENDPWENT 1

/* Define if you have the declaration of environ. */
#define HAVE_ENVIRON_DECL 1

/* Define to 1 if you have the `error' function. */
/* #undef HAVE_ERROR */

/* Define to 1 if you have the <error.h> header file. */
/* #undef HAVE_ERROR_H */

/* Define to 1 if you have the 'euidaccess' function. */
/* #undef HAVE_EUIDACCESS */

/* Define to 1 if you have the 'explicit_memset' function. */
/* #undef HAVE_EXPLICIT_MEMSET */

/* Define to 1 if you have the `faccessat' function. */
#define HAVE_FACCESSAT 1

/* Define to 1 if you have the 'facl' function. */
/* #undef HAVE_FACL */

/* Define if the locale_t type contains insufficient information, as on
   OpenBSD. */
/* #undef HAVE_FAKE_LOCALES */

/* Define to 1 if you have the 'fallocate' function. */
/* #undef HAVE_FALLOCATE */

/* Define to 1 if you have the 'fchdir' function. */
#define HAVE_FCHDIR 1

/* Define to 1 if you have the 'fchmod' function. */
#define HAVE_FCHMOD 1

/* Define to 1 if you have the 'fchmodat' function. */
#define HAVE_FCHMODAT 1

/* Define to 1 if you have the 'fchown' function. */
#define HAVE_FCHOWN 1

/* Define to 1 if you have the 'fchownat' function. */
#define HAVE_FCHOWNAT 1

/* Define to 1 if you have the 'fclonefileat' function. */
/* #undef HAVE_FCLONEFILEAT */

/* Define to 1 if you have the 'fcntl' function. */
#define HAVE_FCNTL 1

/* Define to 1 if you have the `fdatasync' function. */
/* #undef HAVE_FDATASYNC */

/* Define to 1 if you have the 'fdopendir' function. */
#define HAVE_FDOPENDIR 1

/* Define to 1 if you have the <features.h> header file. */
/* #undef HAVE_FEATURES_H */

/* Define to 1 if pipes are FIFOs, 0 if sockets. Leave undefined if not known.
   */
#define HAVE_FIFO_PIPES 1

/* Define to 1 if you have the 'flockfile' function. */
#define HAVE_FLOCKFILE 1

/* Define to 1 if you have the 'fnmatch' function. */
#define HAVE_FNMATCH 1

/* Define to 1 if you have the <fnmatch.h> header file. */
#define HAVE_FNMATCH_H 1

/* Define to 1 if you have the 'fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the 'fpathconf' function. */
#define HAVE_FPATHCONF 1

/* whether fpsetprec is present and required */
/* #undef HAVE_FPSETPREC */

/* Define to 1 if you have the 'fpurge' function. */
/* #undef HAVE_FPURGE */

/* Define to 1 if you have the `freelocale' function. */
/* #undef HAVE_FREELOCALE */

/* Define if the 'free' function is guaranteed to preserve errno. */
/* #undef HAVE_FREE_POSIX */

/* Define if the frexp() function is available and works. */
#define HAVE_FREXP 1

/* Define if the frexpl() function is available. */
#define HAVE_FREXPL 1

/* Define if the frexpl function is available in libc. */
#define HAVE_FREXPL_IN_LIBC 1

/* Define if the frexp function is available in libc. */
#define HAVE_FREXP_IN_LIBC 1

/* Define to 1 if fseeko (and ftello) are declared in stdio.h. */
#define HAVE_FSEEKO 1

/* Define to 1 if you have the 'fstatat' function. */
#define HAVE_FSTATAT 1

/* Define to 1 if you have the 'fstatfs' function. */
/* #undef HAVE_FSTATFS */

/* Define to 1 if you have the 'fsync' function. */
#define HAVE_FSYNC 1

/* Define to 1 if you have the <fs_info.h> header file. */
/* #undef HAVE_FS_INFO_H */

/* Define to 1 if you have the 'fs_stat_dev' function. */
/* #undef HAVE_FS_STAT_DEV */

/* Define to 1 if the system has the ftello function. */
/* #undef HAVE_FTELLO */

/* Define to 1 if you have the `ftruncate' function. */
#define HAVE_FTRUNCATE 1

/* Define to 1 if you have the `fts_open' function. */
/* #undef HAVE_FTS_OPEN */

/* Define to 1 if you have the 'funlockfile' function. */
#define HAVE_FUNLOCKFILE 1

/* Define to 1 if you have the `futimens' function. */
#define HAVE_FUTIMENS 1

/* Define to 1 if you have the `futimes' function. */
/* #undef HAVE_FUTIMES */

/* Define to 1 if you have the `futimesat' function. */
/* #undef HAVE_FUTIMESAT */

/* Define to 1 if you have the 'getacl' function. */
/* #undef HAVE_GETACL */

/* Define to 1 if getaddrinfo exists, or to 0 otherwise. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the 'getattrat' function. */
/* #undef HAVE_GETATTRAT */

/* Define to 1 if getcwd works, but with shorter paths than is generally
   tested with the replacement. */
/* #undef HAVE_GETCWD_SHORTER */

/* Define to 1 if you have the `getdelim' function. */
/* #undef HAVE_GETDELIM */

/* Define to 1 if you have the 'getdtablesize' function. */
/* #undef HAVE_GETDTABLESIZE */

/* Define to 1 if you have the 'getegid' function. */
#define HAVE_GETEGID 1

/* Define to 1 if you have the 'geteuid' function. */
#define HAVE_GETEUID 1

/* Define to 1 if you have the 'getexecname' function. */
/* #undef HAVE_GETEXECNAME */

/* Define to 1 if you have the 'getgid' function. */
#define HAVE_GETGID 1

/* Define to 1 if you have the 'getgrent_nomembers' function. */
/* #undef HAVE_GETGRENT_NOMEMBERS */

/* Define to 1 if you have the 'getgrgid_nomembers' function. */
/* #undef HAVE_GETGRGID_NOMEMBERS */

/* Define to 1 if you have the 'getgrnam_nomembers' function. */
/* #undef HAVE_GETGRNAM_NOMEMBERS */

/* Define to 1 if you have the 'getgrouplist' function. */
/* #undef HAVE_GETGROUPLIST */

/* Define to 1 if your system has a working `getgroups' function. */
#define HAVE_GETGROUPS 1

/* Define to 1 if you have the 'gethostbyname' function. */
/* #undef HAVE_GETHOSTBYNAME */

/* Define to 1 if you have the 'gethostid' function. */
#define HAVE_GETHOSTID 1

/* Define to 1 if you have the 'gethostname' function. */
#define HAVE_GETHOSTNAME 1

/* Define to 1 if you have the `getline' function. */
#define HAVE_GETLINE 1

/* Define to 1 if you have the `getloadavg' function. */
/* #undef HAVE_GETLOADAVG */

/* Define to 1 if you have the 'getlocalename_l' function. */
/* #undef HAVE_GETLOCALENAME_L */

/* Define to 1 if you have the 'getlogin' function. */
#define HAVE_GETLOGIN 1

/* Define to 1 if you have the 'getmntent' function. */
/* #undef HAVE_GETMNTENT */

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the 'getopt_long_only' function. */
#define HAVE_GETOPT_LONG_ONLY 1

/* Define to 1 if you have the 'getppriv' function. */
/* #undef HAVE_GETPPRIV */

/* Define to 1 if you have the `getprogname' function. */
#define HAVE_GETPROGNAME 1

/* Define to 1 if you have the `getrandom' function. */
/* #undef HAVE_GETRANDOM */

/* Define to 1 if you have the 'getrusage' function. */
#define HAVE_GETRUSAGE 1

/* Define to 1 if you have the 'getservbyname' function. */
#define HAVE_GETSERVBYNAME 1

/* Define to 1 if you have the 'getsysinfo' function. */
/* #undef HAVE_GETSYSINFO */

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define to 1 if you have the 'gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the 'getuid' function. */
#define HAVE_GETUID 1

/* Define to 1 if you have the <gmp/gmp.h> header file. */
/* #undef HAVE_GMP_GMP_H */

/* Define to 1 if you have the <gmp.h> header file. */
#define HAVE_GMP_H 1

/* Define if the uselocale function exists, may be safely called, and returns
   sufficient information. */
/* #undef HAVE_GOOD_USELOCALE */

/* Define to 1 if you have the <grp.h> header file. */
#define HAVE_GRP_H 1

/* Define to 1 if you have the 'hasmntopt' function. */
/* #undef HAVE_HASMNTOPT */

/* Define to 1 if you have the <hurd.h> header file. */
/* #undef HAVE_HURD_H */

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <iconv.h> header file. */
#define HAVE_ICONV_H 1

/* Define to 1 if you have the 'inet_ntop' function. */
/* #undef HAVE_INET_NTOP */

/* Define to 1 if you have the 'inet_pton' function. */
/* #undef HAVE_INET_PTON */

/* Define to 1 if you have the 'initgroups' function. */
/* #undef HAVE_INITGROUPS */

/* Define to 1 if you have the `initstate' function. */
/* #undef HAVE_INITSTATE */

/* Define to 1 if the compiler supports one of the keywords 'inline',
   '__inline__', '__inline' and effectively inlines functions marked as such.
   */
#define HAVE_INLINE 1

/* Define to 1 if you have usable inotify support. */
/* #undef HAVE_INOTIFY */

/* Define to 1 if you have the 'inotify_init' function. */
/* #undef HAVE_INOTIFY_INIT */

/* Define if you have the 'intmax_t' type in <stdint.h> or <inttypes.h>. */
#define HAVE_INTMAX_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if <inttypes.h> exists, doesn't clash with <sys/types.h>, and
   declares uintmax_t. */
#define HAVE_INTTYPES_H_WITH_UINTMAX 1

/* Define to 1 if you have the 'ioctl' function. */
/* #undef HAVE_IOCTL */

/* Define to 1 if <sys/socket.h> defines AF_INET. */
#define HAVE_IPV4 1

/* Define to 1 if <sys/socket.h> defines AF_INET6. */
#define HAVE_IPV6 1

/* Define to 1 if you have the 'isapipe' function. */
/* #undef HAVE_ISAPIPE */

/* Define to 1 if you have the 'isblank' function. */
#define HAVE_ISBLANK 1

/* Define if the isnan(double) function is available in libc. */
#define HAVE_ISNAND_IN_LIBC 1

/* Define if the isnan(float) function is available in libc. */
#define HAVE_ISNANF_IN_LIBC 1

/* Define if the isnan(long double) function is available in libc. */
#define HAVE_ISNANL_IN_LIBC 1

/* Define to 1 if you have the `issetugid' function. */
/* #undef HAVE_ISSETUGID */

/* Define to 1 if you have the `iswblank' function. */
#define HAVE_ISWBLANK 1

/* Define to 1 if you have the 'iswcntrl' function. */
#define HAVE_ISWCNTRL 1

/* Define to 1 if you have the 'iswctype' function. */
#define HAVE_ISWCTYPE 1

/* Define if you have <langinfo.h> and nl_langinfo(CODESET). */
#define HAVE_LANGINFO_CODESET 1

/* Define to 1 if you have the <langinfo.h> header file. */
#define HAVE_LANGINFO_H 1

/* Define to 1 if you have the `lchmod' function. */
/* #undef HAVE_LCHMOD */

/* Define to 1 if you have the 'lchown' function. */
#define HAVE_LCHOWN 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define if the ldexpl function is available in libc. */
#define HAVE_LDEXPL_IN_LIBC 1

/* Define if the ldexp function is available in libc. */
#define HAVE_LDEXP_IN_LIBC 1

/* Define to 1 if you have the 'dgc' library (-ldgc). */
/* #undef HAVE_LIBDGC */

/* Define to 1 if you have the <libgen.h> header file. */
#define HAVE_LIBGEN_H 1

/* Define if you have the libgmp library. */
/* #undef HAVE_LIBGMP */

/* Define to 1 if you have the <libintl.h> header file. */
/* #undef HAVE_LIBINTL_H */

/* Define to 1 if you have the 'kstat' library (-lkstat). */
/* #undef HAVE_LIBKSTAT */

/* Define to 1 if you have the 'os' library (-los). */
/* #undef HAVE_LIBOS */

/* Define to 1 if you have the 'perfstat' library (-lperfstat). */
/* #undef HAVE_LIBPERFSTAT */

/* Define to 1 if the bcrypt library is guaranteed to be present. */
/* #undef HAVE_LIB_BCRYPT */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the 'link' function. */
#define HAVE_LINK 1

/* Define to 1 if you have the `linkat' function. */
#define HAVE_LINKAT 1

/* Define to 1 if you have the <linux/falloc.h> header file. */
/* #undef HAVE_LINUX_FALLOC_H */

/* Define to 1 if you have the <linux/fs.h> header file. */
/* #undef HAVE_LINUX_FS_H */

/* Define to 1 if you have 'struct sockaddr_alg' defined. */
/* #undef HAVE_LINUX_IF_ALG_H */

/* Define to 1 if you have the <linux/xattr.h> header file. */
/* #undef HAVE_LINUX_XATTR_H */

/* Define to 1 if you have the 'listmntent' function. */
/* #undef HAVE_LISTMNTENT */

/* Define to 1 if you have the 'listxattr' function. */
/* #undef HAVE_LISTXATTR */

/* Define if localtime-like functions can loop forever on extreme arguments.
   */
/* #undef HAVE_LOCALTIME_INFLOOP_BUG */

/* Define to 1 if you have the 'localtime_r' function. */
#define HAVE_LOCALTIME_R 1

/* Define to 1 if you support file names longer than 14 characters. */
#define HAVE_LONG_FILE_NAMES 1

/* Define to 1 if the system has the type 'long long int'. */
#define HAVE_LONG_LONG_INT 1

/* Define to 1 if you have the 'lstat' function. */
#define HAVE_LSTAT 1

/* Define to 1 if you have the 'lutimens' function. */
/* #undef HAVE_LUTIMENS */

/* Define to 1 if you have the `lutimes' function. */
/* #undef HAVE_LUTIMES */

/* Define to 1 if you have the <machine/hal_sysinfo.h> header file. */
/* #undef HAVE_MACHINE_HAL_SYSINFO_H */

/* Define to 1 if you have the <mach/mach.h> header file. */
/* #undef HAVE_MACH_MACH_H */

/* Define to 1 if malloc (0) returns nonnull. */
#define HAVE_MALLOC_0_NONNULL 1

/* Define to 1 if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define if malloc and calloc set errno on allocation failure. */
#define HAVE_MALLOC_POSIX 1

/* Define to 1 if malloc-like functions do not allocate objects larger than
   PTRDIFF_MAX bytes. */
#define HAVE_MALLOC_PTRDIFF 1

/* Define to 1 if you have the <math.h> header file. */
#define HAVE_MATH_H 1

/* Define to 1 if you have the 'mbrlen' function. */
#define HAVE_MBRLEN 1

/* Define to 1 if you have the 'mbrtowc' function. */
#define HAVE_MBRTOWC 1

/* Define to 1 if you have the 'mbsinit' function. */
#define HAVE_MBSINIT 1

/* Define to 1 if you have the 'mbslen' function. */
/* #undef HAVE_MBSLEN */

/* Define to 1 if you have the 'mbsrtowcs' function. */
#define HAVE_MBSRTOWCS 1

/* Define to 1 if <wchar.h> declares mbstate_t. */
#define HAVE_MBSTATE_T 1

/* Define to 1 if you have the `mbtowc' function. */
#define HAVE_MBTOWC 1

/* Define to 1 if you have the `mempcpy' function. */
/* #undef HAVE_MEMPCPY */

/* Define to 1 if you have the 'memrchr' function. */
/* #undef HAVE_MEMRCHR */

/* Define to 1 if you have the `memset_explicit' function. */
/* #undef HAVE_MEMSET_EXPLICIT */

/* Define to 1 if you have the 'memset_s' function. */
/* #undef HAVE_MEMSET_S */

/* Define to 1 if memset_s support zero-length operations. */
/* #undef HAVE_MEMSET_S_SUPPORTS_ZERO */

/* Define to 1 if you have the 'microuptime' function. */
/* #undef HAVE_MICROUPTIME */

/* Define to 1 if getcwd minimally works, that is, its result can be trusted
   when it succeeds. */
#define HAVE_MINIMALLY_WORKING_GETCWD 1

/* Define to 1 if you have the <minix/config.h> header file. */
/* #undef HAVE_MINIX_CONFIG_H */

/* Define to 1 if <limits.h> defines the MIN and MAX macros. */
/* #undef HAVE_MINMAX_IN_LIMITS_H */

/* Define to 1 if <sys/param.h> defines the MIN and MAX macros. */
/* #undef HAVE_MINMAX_IN_SYS_PARAM_H */

/* Define to 1 if you have the 'mkdirat' function. */
#define HAVE_MKDIRAT 1

/* Define to 1 if you have a 'mkfifo' function. */
#define HAVE_MKFIFO 1

/* Define to 1 if you have the `mkfifoat' function. */
#define HAVE_MKFIFOAT 1

/* Define to 1 if you have the 'mknod' function. */
#define HAVE_MKNOD 1

/* Define to 1 if you have the `mknodat' function. */
#define HAVE_MKNODAT 1

/* Define to 1 if you have the `mkostemp' function. */
#define HAVE_MKOSTEMP 1

/* Define to 1 if you have the 'mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if you have the <mntent.h> header file. */
/* #undef HAVE_MNTENT_H */

/* Define to 1 if you have the 'mode_to_security_class' function. */
/* #undef HAVE_MODE_TO_SECURITY_CLASS */

/* Define to 1 if you have the 'mprotect' function. */
#define HAVE_MPROTECT 1

/* Define to 1 if you have the 'mquery' function. */
/* #undef HAVE_MQUERY */

/* Define to 1 on MSVC platforms that have the "invalid parameter handler"
   concept. */
/* #undef HAVE_MSVC_INVALID_PARAMETER_HANDLER */

/* Define if the locale_t type does not contain the name of each locale
   category. */
/* #undef HAVE_NAMELESS_LOCALES */

/* Define to 1 if you have the 'nanouptime' function. */
/* #undef HAVE_NANOUPTIME */

/* Define to 1 if fchmodat works, except for the trailing slash handling. */
/* #undef HAVE_NEARLY_WORKING_FCHMODAT */

/* Define to 1 if utimensat works, except for the trailing slash handling. */
/* #undef HAVE_NEARLY_WORKING_UTIMENSAT */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the `newlocale' function. */
/* #undef HAVE_NEWLOCALE */

/* Define to 1 if you have the 'next_dev' function. */
/* #undef HAVE_NEXT_DEV */

/* Define to 1 if you have the <nfs/nfs_client.h> header file. */
/* #undef HAVE_NFS_NFS_CLIENT_H */

/* Define to 1 if you have the <nfs/vfs.h> header file. */
/* #undef HAVE_NFS_VFS_H */

/* Define to 1 if you have the 'nice' function. */
/* #undef HAVE_NICE */

/* Define to 1 if you have the <nlist.h> header file. */
/* #undef HAVE_NLIST_H */

/* Define to 1 if you have the 'nl_langinfo' function. */
#define HAVE_NL_LANGINFO 1

/* Define to 1 if you have the <obstack.h> header file. */
/* #undef HAVE_OBSTACK_H */

/* Define to 1 if you have the 'openat' function. */
#define HAVE_OPENAT 1

/* Define to 1 if you have the 'opendir' function. */
#define HAVE_OPENDIR 1

/* Define to 1 if libcrypto is used for MD5. */
#define HAVE_OPENSSL_MD5 1

/* Define to 1 if you have the <openssl/md5.h> header file. */
#define HAVE_OPENSSL_MD5_H 1

/* Define to 1 if libcrypto is used for SHA1. */
#define HAVE_OPENSSL_SHA1 1

/* Define to 1 if libcrypto is used for SHA256. */
#define HAVE_OPENSSL_SHA256 1

/* Define to 1 if libcrypto is used for SHA3. */
#define HAVE_OPENSSL_SHA3 1

/* Define to 1 if libcrypto is used for SHA512. */
#define HAVE_OPENSSL_SHA512 1

/* Define to 1 if you have the <openssl/sha.h> header file. */
#define HAVE_OPENSSL_SHA_H 1

/* Define to 1 if you have the <OS.h> header file. */
/* #undef HAVE_OS_H */

/* Define to 1 if getcwd works, except it sometimes fails when it shouldn't,
   setting errno to ERANGE, ENAMETOOLONG, or ENOENT. */
/* #undef HAVE_PARTLY_WORKING_GETCWD */

/* Define to 1 if you have the 'pathconf' function. */
#define HAVE_PATHCONF 1

/* Define to 1 if you have the <paths.h> header file. */
/* #undef HAVE_PATHS_H */

/* Define to 1 if you have the 'pause' function. */
#define HAVE_PAUSE 1

/* Define to 1 if you have the 'pipe' function. */
#define HAVE_PIPE 1

/* Define to 1 if you have the `pipe2' function. */
/* #undef HAVE_PIPE2 */

/* Define to 1 if you have the 'posix_fadvise' function. */
#define HAVE_POSIX_FADVISE 1

/* Define to 1 if you have the `posix_memalign' function. */
#define HAVE_POSIX_MEMALIGN 1

/* Define to 1 if you have the `posix_spawn' function. */
#define HAVE_POSIX_SPAWN 1

/* Define to 1 if the system has the type 'posix_spawnattr_t'. */
#define HAVE_POSIX_SPAWNATTR_T 1

/* Define to 1 if the system has the type 'posix_spawn_file_actions_t'. */
#define HAVE_POSIX_SPAWN_FILE_ACTIONS_T 1

/* Define to 1 if you have the 'prctl' function. */
/* #undef HAVE_PRCTL */

/* Define to 1 if you have the <priv.h> header file. */
/* #undef HAVE_PRIV_H */

/* Define to 1 if you have the 'pselect' function. */
/* #undef HAVE_PSELECT */

/* Define to 1 if you have the 'pstat_getdynamic' function. */
/* #undef HAVE_PSTAT_GETDYNAMIC */

/* Define to 1 if you have the 'pstat_getprocvm' function. */
/* #undef HAVE_PSTAT_GETPROCVM */

/* Define to 1 if you have the 'pstat_getstatic' function. */
/* #undef HAVE_PSTAT_GETSTATIC */

/* Define if you have the <pthread.h> header and the POSIX threads API. */
#define HAVE_PTHREAD_API 1

/* Define to 1 if you have the `pthread_atfork' function. */
#define HAVE_PTHREAD_ATFORK 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define if the <pthread.h> defines PTHREAD_MUTEX_RECURSIVE. */
#define HAVE_PTHREAD_MUTEX_RECURSIVE 1

/* Define if the POSIX multithreading library has read/write locks. */
#define HAVE_PTHREAD_RWLOCK 1

/* Define if the 'pthread_rwlock_rdlock' function prefers a writer to a
   reader. */
/* #undef HAVE_PTHREAD_RWLOCK_RDLOCK_PREFER_WRITER */

/* Define to 1 if the pthread_sigmask function can be used (despite bugs). */
#define HAVE_PTHREAD_SIGMASK 1

/* Define to 1 if the system has the type 'pthread_spinlock_t'. */
/* #undef HAVE_PTHREAD_SPINLOCK_T */

/* Define to 1 if the system has the type 'pthread_t'. */
#define HAVE_PTHREAD_T 1

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* Define to 1 if you have the `qsort_r' function. */
/* #undef HAVE_QSORT_R */

/* Define to 1 if you have the 'raise' function. */
#define HAVE_RAISE 1

/* Define to 1 if you have the <random.h> header file. */
/* #undef HAVE_RANDOM_H */

/* Define to 1 if you have the 'random_r' function. */
/* #undef HAVE_RANDOM_R */

/* Define to 1 if you have the 'rawmemchr' function. */
/* #undef HAVE_RAWMEMCHR */

/* Define to 1 if you have the 'readdir' function. */
#define HAVE_READDIR 1

/* Define to 1 if you have the 'readlink' function. */
#define HAVE_READLINK 1

/* Define to 1 if you have the `readlinkat' function. */
#define HAVE_READLINKAT 1

/* Define to 1 if you have the `reallocarray' function. */
/* #undef HAVE_REALLOCARRAY */

/* Define to 1 if realloc (..., 0) returns nonnull. */
/* #undef HAVE_REALLOC_0_NONNULL */

/* Define if realloc sets errno on allocation failure. */
#define HAVE_REALLOC_POSIX 1

/* Define to 1 if you have the 'realpath' function. */
#define HAVE_REALPATH 1

/* Define to 1 if you have the 'renameat' function. */
#define HAVE_RENAMEAT 1

/* Define to 1 if you have the `renameat2' function. */
/* #undef HAVE_RENAMEAT2 */

/* Define to 1 if you have the 'rewinddir' function. */
#define HAVE_REWINDDIR 1

/* Define to 1 if you have the 'rpmatch' function. */
/* #undef HAVE_RPMATCH */

/* Define to 1 if 'long double' and 'double' have the same representation. */
#define HAVE_SAME_LONG_DOUBLE_AS_DOUBLE 1

/* Define to 1 if the system has the type 'sa_family_t'. */
#define HAVE_SA_FAMILY_T 1

/* Define to 1 if you have the `sched_getaffinity' function. */
/* #undef HAVE_SCHED_GETAFFINITY */

/* Define to 1 if sched_getaffinity has a glibc compatible declaration. */
/* #undef HAVE_SCHED_GETAFFINITY_LIKE_GLIBC */

/* Define to 1 if you have the 'sched_getaffinity_np' function. */
/* #undef HAVE_SCHED_GETAFFINITY_NP */

/* Define to 1 if you have the <sched.h> header file. */
#define HAVE_SCHED_H 1

/* Define to 1 if you have the 'sched_setparam' function. */
/* #undef HAVE_SCHED_SETPARAM */

/* Define to 1 if you have the 'sched_setscheduler' function. */
/* #undef HAVE_SCHED_SETSCHEDULER */

/* Define to 1 if you have the <sdkddkver.h> header file. */
/* #undef HAVE_SDKDDKVER_H */

/* Define to 1 if you have the <search.h> header file. */
#define HAVE_SEARCH_H 1

/* Define to 1 if you have the 'secure_getenv' function. */
/* #undef HAVE_SECURE_GETENV */

/* Define to 1 if you have the <selinux/context.h> header file. */
/* #undef HAVE_SELINUX_CONTEXT_H */

/* Define to 1 if you have the <selinux/flask.h> header file. */
/* #undef HAVE_SELINUX_FLASK_H */

/* Define to 1 if you have the <selinux/label.h> header file. */
/* #undef HAVE_SELINUX_LABEL_H */

/* Define to 1 if you have the <selinux/selinux.h> header file. */
/* #undef HAVE_SELINUX_SELINUX_H */

/* Define to 1 if you have the <semaphore.h> header file. */
#define HAVE_SEMAPHORE_H 1

/* Define to 1 if you have the 'setdtablesize' function. */
/* #undef HAVE_SETDTABLESIZE */

/* Define to 1 if you have the 'setegid' function. */
#define HAVE_SETEGID 1

/* Define to 1 if you have the 'setenv' function. */
#define HAVE_SETENV 1

/* Define to 1 if you have the 'seteuid' function. */
#define HAVE_SETEUID 1

/* Define to 1 if you have the 'setgroups' function. */
/* #undef HAVE_SETGROUPS */

/* Define to 1 if you have the 'sethostname' function. */
/* #undef HAVE_SETHOSTNAME */

/* Define to 1 if you have the 'setitimer' function. */
/* #undef HAVE_SETITIMER */

/* Define to 1 if you have the `setmntent' function. */
/* #undef HAVE_SETMNTENT */

/* Define to 1 if you have the 'setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define to 1 if you have the `setstate' function. */
/* #undef HAVE_SETSTATE */

/* Define to 1 if you have the 'settimeofday' function. */
/* #undef HAVE_SETTIMEOFDAY */

/* Define to 1 if you have the 'shutdown' function. */
#define HAVE_SHUTDOWN 1

/* Define to 1 if you have the `sig2str' function. */
/* #undef HAVE_SIG2STR */

/* Define to 1 if you have the 'sigaction' function. */
#define HAVE_SIGACTION 1

/* Define to 1 if you have the 'sigaltstack' function. */
/* #undef HAVE_SIGALTSTACK */

/* Define to 1 if the system has the type 'siginfo_t'. */
/* #undef HAVE_SIGINFO_T */

/* Define to 1 if you have the 'siginterrupt' function. */
/* #undef HAVE_SIGINTERRUPT */

/* Define to 1 if 'sig_atomic_t' is a signed integer type. */
/* #undef HAVE_SIGNED_SIG_ATOMIC_T */

/* Define to 1 if 'wchar_t' is a signed integer type. */
/* #undef HAVE_SIGNED_WCHAR_T */

/* Define to 1 if 'wint_t' is a signed integer type. */
/* #undef HAVE_SIGNED_WINT_T */

/* Define to 1 if you have the 'sigprocmask' function. */
#define HAVE_SIGPROCMASK 1

/* Define to 1 if the system has the type 'sigset_t'. */
#define HAVE_SIGSET_T 1

/* Define to 1 if you have the 'sigsuspend' function. */
#define HAVE_SIGSUSPEND 1

/* Define to 1 if you have the 'sleep' function. */
#define HAVE_SLEEP 1

/* libsmack usability */
/* #undef HAVE_SMACK */

/* Define to 1 if you have the 'snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define if the return value of the snprintf function is the number of of
   bytes (excluding the terminating NUL) that would have been produced if the
   buffer had been large enough. */
#define HAVE_SNPRINTF_RETVAL_C99 1

/* Define if the string produced by the snprintf function is always NUL
   terminated. */
#define HAVE_SNPRINTF_TRUNCATION_C99 1

/* Define if the locale_t type is as on Solaris 11.4. */
/* #undef HAVE_SOLARIS114_LOCALES */

/* Define to 1 if you have the <spawn.h> header file. */
#define HAVE_SPAWN_H 1

/* Define to 1 if you have the 'statacl' function. */
/* #undef HAVE_STATACL */

/* Define to 1 if you have the 'statvfs' function. */
/* #undef HAVE_STATVFS */

/* Define to 1 if you have the `statx' function. */
/* #undef HAVE_STATX */

/* Define to 1 if you have the <stdbit.h> header file. */
/* #undef HAVE_STDBIT_H */

/* Define to 1 if you have the <stdbool.h> header file. */
#define HAVE_STDBOOL_H 1

/* Define to 1 if you have the <stdckdint.h> header file. */
#define HAVE_STDCKDINT_H 1

/* Define to 1 if you have the <stdcountof.h> header file. */
#define HAVE_STDCOUNTOF_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define if <stdint.h> exists, doesn't clash with <sys/types.h>, and declares
   uintmax_t. */
#define HAVE_STDINT_H_WITH_UINTMAX 1

/* Define to 1 if you have the <stdio_ext.h> header file. */
#define HAVE_STDIO_EXT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the 'stime' function. */
/* #undef HAVE_STIME */

/* Define to 1 if you have the `stpcpy' function. */
/* #undef HAVE_STPCPY */

/* Define to 1 if you have the `str2sig' function. */
/* #undef HAVE_STR2SIG */

/* Define to 1 if you have the `strchrnul' function. */
/* #undef HAVE_STRCHRNUL */

/* Define to 1 if you have the `strerrorname_np' function. */
/* #undef HAVE_STRERRORNAME_NP */

/* Define to 1 if you have the `strerror_r' function. */
#define HAVE_STRERROR_R 1

/* Define to 1 if you have the 'strftime_z' function. */
/* #undef HAVE_STRFTIME_Z */

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the 'strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the <stropts.h> header file. */
/* #undef HAVE_STROPTS_H */

/* Define to 1 if you have the 'strsignal' function. */
#define HAVE_STRSIGNAL 1

/* Define to 1 if the system has the 'strtod_l' function. */
#define HAVE_STRTOD_L 0

/* Define to 1 if you have the `strtof' function. */
#define HAVE_STRTOF 1

/* Define to 1 if you have the 'strtoimax' function. */
#define HAVE_STRTOIMAX 1

/* Define to 1 if you have the 'strtold' function. */
#define HAVE_STRTOLD 1

/* Define to 1 if you have the `strtold_l' function. */
/* #undef HAVE_STRTOLD_L */

/* Define to 1 if you have the 'strtoll' function. */
#define HAVE_STRTOLL 1

/* Define to 1 if you have the 'strtoull' function. */
#define HAVE_STRTOULL 1

/* Define to 1 if you have the 'strtoumax' function. */
#define HAVE_STRTOUMAX 1

/* Define to 1 if the system has the type 'struct addrinfo'. */
#define HAVE_STRUCT_ADDRINFO 1

/* Define if there is a member named d_type in the struct describing directory
   headers. */
#define HAVE_STRUCT_DIRENT_D_TYPE 1

/* Define to 1 if 'f_fstypename' is a member of 'struct fsstat'. */
/* #undef HAVE_STRUCT_FSSTAT_F_FSTYPENAME */

/* Define to 1 if 'decimal_point' is a member of 'struct lconv'. */
/* #undef HAVE_STRUCT_LCONV_DECIMAL_POINT */

/* Define to 1 if 'int_p_cs_precedes' is a member of 'struct lconv'. */
/* #undef HAVE_STRUCT_LCONV_INT_P_CS_PRECEDES */

/* Define to 1 if 'n_un.n_name' is a member of 'struct nlist'. */
/* #undef HAVE_STRUCT_NLIST_N_UN_N_NAME */

/* Define to 1 if the system has the type 'struct random_data'. */
/* #undef HAVE_STRUCT_RANDOM_DATA */

/* Define to 1 if 'sa_sigaction' is a member of 'struct sigaction'. */
#define HAVE_STRUCT_SIGACTION_SA_SIGACTION 1

/* Define to 1 if 'sa_len' is a member of 'struct sockaddr'. */
/* #undef HAVE_STRUCT_SOCKADDR_SA_LEN */

/* Define to 1 if the system has the type 'struct sockaddr_storage'. */
#define HAVE_STRUCT_SOCKADDR_STORAGE 1

/* Define to 1 if 'ss_family' is a member of 'struct sockaddr_storage'. */
#define HAVE_STRUCT_SOCKADDR_STORAGE_SS_FAMILY 1

/* Define to 1 if 'f_frsize' is a member of 'struct statfs'. */
/* #undef HAVE_STRUCT_STATFS_F_FRSIZE */

/* Define to 1 if 'f_fstypename' is a member of 'struct statfs'. */
/* #undef HAVE_STRUCT_STATFS_F_FSTYPENAME */

/* Define to 1 if 'f_namelen' is a member of 'struct statfs'. */
/* #undef HAVE_STRUCT_STATFS_F_NAMELEN */

/* Define to 1 if 'f_namemax' is a member of 'struct statfs'. */
/* #undef HAVE_STRUCT_STATFS_F_NAMEMAX */

/* Define to 1 if 'f_type' is a member of 'struct statfs'. */
/* #undef HAVE_STRUCT_STATFS_F_TYPE */

/* Define to 1 if 'f_basetype' is a member of 'struct statvfs'. */
#define HAVE_STRUCT_STATVFS_F_BASETYPE 1

/* Define to 1 if 'f_fstypename' is a member of 'struct statvfs'. */
/* #undef HAVE_STRUCT_STATVFS_F_FSTYPENAME */

/* Define to 1 if 'f_namemax' is a member of 'struct statvfs'. */
#define HAVE_STRUCT_STATVFS_F_NAMEMAX 1

/* Define to 1 if 'f_type' is a member of 'struct statvfs'. */
/* #undef HAVE_STRUCT_STATVFS_F_TYPE */

/* Define to 1 if 'st_atimensec' is a member of 'struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_ATIMENSEC */

/* Define to 1 if 'st_atimespec.tv_nsec' is a member of 'struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC */

/* Define to 1 if 'st_atim.st__tim.tv_nsec' is a member of 'struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_ATIM_ST__TIM_TV_NSEC */

/* Define to 1 if 'st_atim.tv_nsec' is a member of 'struct stat'. */
#define HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC 1

/* Define to 1 if 'st_author' is a member of 'struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_AUTHOR */

/* Define to 1 if 'st_birthtimensec' is a member of 'struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_BIRTHTIMENSEC */

/* Define to 1 if 'st_birthtimespec.tv_nsec' is a member of 'struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_BIRTHTIMESPEC_TV_NSEC */

/* Define to 1 if 'st_birthtim.tv_nsec' is a member of 'struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_BIRTHTIM_TV_NSEC */

/* Define to 1 if 'st_blocks' is a member of 'struct stat'. */
#define HAVE_STRUCT_STAT_ST_BLOCKS 1

/* Define to 1 if 'mem_unit' is a member of 'struct sysinfo'. */
/* #undef HAVE_STRUCT_SYSINFO_MEM_UNIT */

/* Define to 1 if 'tm_gmtoff' is a member of 'struct tm'. */
#define HAVE_STRUCT_TM_TM_GMTOFF 1

/* Define to 1 if 'tm_zone' is a member of 'struct tm'. */
#define HAVE_STRUCT_TM_TM_ZONE 1

/* Define to 1 if 'ut_exit' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_EXIT */

/* Define to 1 if 'ut_exit.e_exit' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_EXIT_E_EXIT */

/* Define to 1 if 'ut_exit.e_termination' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_EXIT_E_TERMINATION */

/* Define to 1 if 'ut_host' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_HOST */

/* Define to 1 if 'ut_id' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_ID */

/* Define to 1 if 'ut_name' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_NAME */

/* Define to 1 if 'ut_pid' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_PID */

/* Define to 1 if 'ut_session' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_SESSION */

/* Define to 1 if 'ut_type' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_TYPE */

/* Define to 1 if 'ut_user' is a member of 'struct utmpx'. */
/* #undef HAVE_STRUCT_UTMPX_UT_USER */

/* Define to 1 if 'ut_exit' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_EXIT */

/* Define to 1 if 'ut_exit.e_exit' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_EXIT_E_EXIT */

/* Define to 1 if 'ut_exit.e_termination' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_EXIT_E_TERMINATION */

/* Define to 1 if 'ut_host' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_HOST */

/* Define to 1 if 'ut_id' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_ID */

/* Define to 1 if 'ut_name' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_NAME */

/* Define to 1 if 'ut_pid' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_PID */

/* Define to 1 if 'ut_session' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_SESSION */

/* Define to 1 if 'ut_tv' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_TV */

/* Define to 1 if 'ut_type' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_TYPE */

/* Define to 1 if 'ut_user' is a member of 'struct utmp'. */
/* #undef HAVE_STRUCT_UTMP_UT_USER */

/* Define to 1 if the system has the type 'struct utsname'. */
#define HAVE_STRUCT_UTSNAME 1

/* Define to 1 if your 'struct stat' has 'st_blocks'. Deprecated, use
   'HAVE_STRUCT_STAT_ST_BLOCKS' instead. */
#define HAVE_ST_BLOCKS 1

/* Define to 1 if you have the 'symlink' function. */
#define HAVE_SYMLINK 1

/* Define to 1 if you have the `symlinkat' function. */
#define HAVE_SYMLINKAT 1

/* Define to 1 if you have the 'sync' function. */
/* #undef HAVE_SYNC */

/* Define to 1 if you have the `syncfs' function. */
/* #undef HAVE_SYNCFS */

/* Define to 1 if you have the 'sysctl' function. */
/* #undef HAVE_SYSCTL */

/* Define to 1 if you have the 'sysinfo' function. */
/* #undef HAVE_SYSINFO */

/* FIXME */
#define HAVE_SYSLOG 1

/* Define to 1 if you have the <syslog.h> header file. */
#define HAVE_SYSLOG_H 1

/* Define to 1 if you have the <sys/acl.h> header file. */
/* #undef HAVE_SYS_ACL_H */

/* Define to 1 if you have the <sys/bitypes.h> header file. */
/* #undef HAVE_SYS_BITYPES_H */

/* Define to 1 if you have the <sys/endian.h> header file. */
/* #undef HAVE_SYS_ENDIAN_H */

/* Define to 1 if you have the <sys/fs/s5param.h> header file. */
/* #undef HAVE_SYS_FS_S5PARAM_H */

/* Define to 1 if you have the <sys/fs_types.h> header file. */
/* #undef HAVE_SYS_FS_TYPES_H */

/* Define to 1 if you have the <sys/inttypes.h> header file. */
/* #undef HAVE_SYS_INTTYPES_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
/* #undef HAVE_SYS_IOCTL_H */

/* Define to 1 if you have the <sys/loadavg.h> header file. */
/* #undef HAVE_SYS_LOADAVG_H */

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the <sys/mntent.h> header file. */
/* #undef HAVE_SYS_MNTENT_H */

/* Define to 1 if you have the <sys/mount.h> header file. */
/* #undef HAVE_SYS_MOUNT_H */

/* Define to 1 if you have the <sys/mtio.h> header file. */
/* #undef HAVE_SYS_MTIO_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/process.h> header file. */
/* #undef HAVE_SYS_PROCESS_H */

/* Define to 1 if you have the <sys/pstat.h> header file. */
/* #undef HAVE_SYS_PSTAT_H */

/* Define to 1 if you have the <sys/random.h> header file. */
/* #undef HAVE_SYS_RANDOM_H */

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/single_threaded.h> header file. */
/* #undef HAVE_SYS_SINGLE_THREADED_H */

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/statfs.h> header file. */
/* #undef HAVE_SYS_STATFS_H */

/* Define to 1 if you have the <sys/statvfs.h> header file. */
#define HAVE_SYS_STATVFS_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/sysctl.h> header file. */
/* #undef HAVE_SYS_SYSCTL_H */

/* Define to 1 if you have the <sys/sysinfo.h> header file. */
/* #undef HAVE_SYS_SYSINFO_H */

/* Define to 1 if you have the <sys/systemcfg.h> header file. */
/* #undef HAVE_SYS_SYSTEMCFG_H */

/* Define to 1 if you have the <sys/systeminfo.h> header file. */
/* #undef HAVE_SYS_SYSTEMINFO_H */

/* Define to 1 if you have the <sys/table.h> header file. */
/* #undef HAVE_SYS_TABLE_H */

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <sys/ucred.h> header file. */
/* #undef HAVE_SYS_UCRED_H */

/* Define to 1 if you have the <sys/uio.h> header file. */
#define HAVE_SYS_UIO_H 1

/* Define to 1 if you have the <sys/un.h> header file. */
#define HAVE_SYS_UN_H 1

/* Define to 1 if you have the <sys/utsname.h> header file. */
#define HAVE_SYS_UTSNAME_H 1

/* Define to 1 if you have the <sys/vfs.h> header file. */
/* #undef HAVE_SYS_VFS_H */

/* Define to 1 if you have the <sys/wait.h> header file. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the 'table' function. */
/* #undef HAVE_TABLE */

/* Define to 1 if you have the 'tcgetpgrp' function. */
#define HAVE_TCGETPGRP 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if you have the `thrd_create' function. */
/* #undef HAVE_THRD_CREATE */

/* Define to 1 if you have the <threads.h> header file. */
/* #undef HAVE_THREADS_H */

/* Define to 1 if you have the `timegm' function. */
/* #undef HAVE_TIMEGM */

/* Define to 1 if you have the 'timer_settime' function. */
/* #undef HAVE_TIMER_SETTIME */

/* Define if you have the timespec_get function. */
#define HAVE_TIMESPEC_GET 1

/* Define to 1 if you have the `timespec_getres' function. */
/* #undef HAVE_TIMESPEC_GETRES */

/* Define to 1 if the system has the type 'timezone_t'. */
/* #undef HAVE_TIMEZONE_T */

/* Define if struct tm has the tm_gmtoff member. This macro is obsolete. New
   code should use HAVE_STRUCT_TM_TM_GMTOFF. */
#define HAVE_TM_GMTOFF 1

/* Define to 1 if you have the 'towlower' function. */
/* #undef HAVE_TOWLOWER */

/* Define to 1 if you have the `tsearch' function. */
/* #undef HAVE_TSEARCH */

/* Define to 1 if you have the `tzalloc' function. */
/* #undef HAVE_TZALLOC */

/* Define to 1 if you have the <uchar.h> header file. */
#define HAVE_UCHAR_H 1

/* Define to 1 if you have the 'uname' function. */
#define HAVE_UNAME 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have the <unistring/woe32dll.h> header file. */
/* #undef HAVE_UNISTRING_WOE32DLL_H */

/* Define to 1 if <sys/socket.h> defines AF_UNIX. */
#define HAVE_UNIXSOCKET 1

/* Define to 1 if you have the 'unlinkat' function. */
#define HAVE_UNLINKAT 1

/* Define to 1 if you have the 'unsetenv' function. */
#define HAVE_UNSETENV 1

/* Define to 1 if the system has the type 'unsigned long long int'. */
#define HAVE_UNSIGNED_LONG_LONG_INT 1

/* Define to 1 if you have the `uselocale' function. */
/* #undef HAVE_USELOCALE */

/* Define to 1 if you have the 'usleep' function. */
/* #undef HAVE_USLEEP */

/* Define to 1 if you have the 'utime' function. */
#define HAVE_UTIME 1

/* Define to 1 if you have the 'utimens' function. */
/* #undef HAVE_UTIMENS */

/* Define to 1 if you have the `utimensat' function. */
#define HAVE_UTIMENSAT 1

/* Define to 1 if you have the <utime.h> header file. */
#define HAVE_UTIME_H 1

/* Define to 1 if you have the 'utmpname' function. */
/* #undef HAVE_UTMPNAME */

/* Define to 1 if you have the 'utmpxname' function. */
/* #undef HAVE_UTMPXNAME */

/* Define to 1 if you have the <utmpx.h> header file. */
/* #undef HAVE_UTMPX_H */

/* Define to 1 if you have the <utmp.h> header file. */
/* #undef HAVE_UTMP_H */

/* Define if you have a global __progname variable */
/* #undef HAVE_VAR___PROGNAME */

/* Define to 1 if you have the 'vasnprintf' function. */
/* #undef HAVE_VASNPRINTF */

/* Define to 1 if you have the 'vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to 1 if you have the 'vfork' function. */
/* #undef HAVE_VFORK */

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define to 1 or 0, depending whether the compiler supports simple visibility
   declarations. */
#define HAVE_VISIBILITY 0

/* Define to 1 if you have the 'vmsplice' function. */
/* #undef HAVE_VMSPLICE */

/* Define to 1 if you have the <wchar.h> header file. */
#define HAVE_WCHAR_H 1

/* Define to 1 if you have the 'wcrtomb' function. */
#define HAVE_WCRTOMB 1

/* Define to 1 if you have the 'wcslen' function. */
#define HAVE_WCSLEN 1

/* Define to 1 if you have the 'wctob' function. */
#define HAVE_WCTOB 1

/* Define to 1 if you have the <wctype.h> header file. */
#define HAVE_WCTYPE_H 1

/* Define to 1 if you have the 'wcwidth' function. */
#define HAVE_WCWIDTH 1

/* Define to 1 if the compiler and linker support weak declarations of
   symbols. */
/* #undef HAVE_WEAK_SYMBOLS */

/* Define to 1 if <locale.h> defines the _locale_t type. */
/* #undef HAVE_WINDOWS_LOCALE_T */

/* Define to 1 if you have the <winsock2.h> header file. */
/* #undef HAVE_WINSOCK2_H */

/* Define if you have the 'wint_t' type. */
#define HAVE_WINT_T 1

/* Define to 1 if you have the `wmempcpy' function. */
/* #undef HAVE_WMEMPCPY */

/* Define if the c32rtomb function basically works. */
/* #undef HAVE_WORKING_C32RTOMB */

/* Define to 1 if 'fork' works. */
#define HAVE_WORKING_FORK 1

/* Define to 1 if fstatat (..., 0) works. For example, it does not work in AIX
   7.1. */
#define HAVE_WORKING_FSTATAT_ZERO_FLAG 1

/* Define if the mbrtoc32 function basically works. */
/* #undef HAVE_WORKING_MBRTOC32 */

/* Define to 1 if O_DIRECTORY works, 0 otherwise. */
#define HAVE_WORKING_O_DIRECTORY 1

/* Define to 1 if O_NOATIME works, 0 otherwise. */
#define HAVE_WORKING_O_NOATIME 1

/* Define to 1 if O_NOFOLLOW works, 0 otherwise. */
#define HAVE_WORKING_O_NOFOLLOW 1

/* Define if the renameat2 function exists and it works. */
/* #undef HAVE_WORKING_RENAMEAT2 */

/* Define to 1 if the function strerrorname_np exists and works. */
/* #undef HAVE_WORKING_STRERRORNAME_NP */

/* Define if the uselocale function exists and may safely be called. */
/* #undef HAVE_WORKING_USELOCALE */

/* Define if utimes works properly. */
/* #undef HAVE_WORKING_UTIMES */

/* Define to 1 if 'vfork' works. */
/* #undef HAVE_WORKING_VFORK */

/* Define to 1 if you have the <ws2tcpip.h> header file. */
/* #undef HAVE_WS2TCPIP_H */

/* Define to 1 if you have the <xlocale.h> header file. */
/* #undef HAVE_XLOCALE_H */

/* Define to 1 if you have the '_chsize' function. */
/* #undef HAVE__CHSIZE */

/* Define to 1 if you have the '_fseeki64' function. */
/* #undef HAVE__FSEEKI64 */

/* Define to 1 if you have the '_ftelli64' function. */
/* #undef HAVE__FTELLI64 */

/* Define to 1 if you have the '_set_invalid_parameter_handler' function. */
/* #undef HAVE__SET_INVALID_PARAMETER_HANDLER */

/* Define to 1 if you have the external variable, _system_configuration with a
   member named physmem. */
/* #undef HAVE__SYSTEM_CONFIGURATION */

/* Define to 1 if the compiler supports __builtin_expect,
   and to 2 if <builtins.h> does.  */
#define HAVE___BUILTIN_EXPECT 1
#ifndef HAVE___BUILTIN_EXPECT
# define __builtin_expect(e, c) (e)
#elif HAVE___BUILTIN_EXPECT == 2
# include <builtins.h>
#endif


/* Define to 1 if you have the `__fpurge' function. */
#define HAVE___FPURGE 1

/* Define to 1 if you have the `__freadahead' function. */
/* #undef HAVE___FREADAHEAD */

/* Define to 1 if you have the `__freading' function. */
#define HAVE___FREADING 1

/* Define to 1 if you have the `__fseterr' function. */
#define HAVE___FSETERR 1

/* Define to 1 if the system has the type '__fsword_t'. */
/* #undef HAVE___FSWORD_T */

/* Define to 1 if ctype.h defines __header_inline. */
/* #undef HAVE___HEADER_INLINE */

/* Please see the Gnulib manual for how to use these macros.

   Suppress extern inline with HP-UX cc, as it appears to be broken; see
   <https://lists.gnu.org/r/bug-texinfo/2013-02/msg00030.html>.

   Suppress extern inline with Sun C in standards-conformance mode, as it
   mishandles inline functions that call each other.  E.g., for 'inline void f
   (void) { } inline void g (void) { f (); }', c99 incorrectly complains
   'reference to static identifier "f" in extern inline function'.
   This bug was observed with Oracle Developer Studio 12.6
   (Sun C 5.15 SunOS_sparc 2017/05/30).

   Suppress extern inline (with or without __attribute__ ((__gnu_inline__)))
   on configurations that mistakenly use 'static inline' to implement
   functions or macros in standard C headers like <ctype.h>.  For example,
   if isdigit is mistakenly implemented via a static inline function,
   a program containing an extern inline function that calls isdigit
   may not work since C99 through C23 prohibit extern inline functions
   from calling static functions (ISO C 23 § 6.7.5 ¶ 3)).
   Although a future C standard will likely relax this restriction
   <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3622.txt>,
   respect it for now.  This bug is known to occur on:

     OS X 10.8 and earlier; see:
     https://lists.gnu.org/r/bug-gnulib/2012-12/msg00023.html

     DragonFly; see
     http://muscles.dragonflybsd.org/bulk/clang-master-potential/20141111_102002/logs/ah-tty-0.3.12.log

     FreeBSD; see:
     https://lists.gnu.org/r/bug-gnulib/2014-07/msg00104.html

   OS X 10.9 has a macro __header_inline indicating the bug is fixed for C and
   for clang but remains for g++; see <https://trac.macports.org/ticket/41033>.
   Assume DragonFly and FreeBSD will be similar.

   GCC 4.3 and above with -std=c99 or -std=gnu99 implements ISO C99
   inline semantics, unless -fgnu89-inline is used.  It defines a macro
   __GNUC_STDC_INLINE__ to indicate this situation or a macro
   __GNUC_GNU_INLINE__ to indicate the opposite situation.
   GCC 4.2 with -std=c99 or -std=gnu99 implements the GNU C inline
   semantics but warns, unless -fgnu89-inline is used:
     warning: C99 inline functions are not supported; using GNU89
     warning: to disable this warning use -fgnu89-inline or the gnu_inline function attribute
   It defines a macro __GNUC_GNU_INLINE__ to indicate this situation.
 */
#if (((defined __APPLE__ && defined __MACH__) \
      || defined __DragonFly__ || defined __FreeBSD__) \
     && (defined HAVE___HEADER_INLINE \
         ? (defined __cplusplus && defined __GNUC_STDC_INLINE__ \
            && ! defined __clang__) \
         : ((! defined _DONT_USE_CTYPE_INLINE_ \
             && (defined __GNUC__ || defined __cplusplus)) \
            || (defined _FORTIFY_SOURCE && 0 < _FORTIFY_SOURCE \
                && defined __GNUC__ && ! defined __cplusplus))))
# define _GL_EXTERN_INLINE_STDHEADER_BUG
#endif
#if ((__GNUC__ \
      ? (defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__ \
         && !defined __PCC__) \
      : (199901L <= __STDC_VERSION__ \
         && !defined __HP_cc \
         && !defined __PGI \
         && !(defined __SUNPRO_C && __STDC__))) \
     && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
# define _GL_INLINE inline
# define _GL_EXTERN_INLINE extern inline
# define _GL_EXTERN_INLINE_IN_USE
#elif (2 < __GNUC__ + (7 <= __GNUC_MINOR__) && !defined __STRICT_ANSI__ \
       && !defined __PCC__ \
       && !defined _GL_EXTERN_INLINE_STDHEADER_BUG)
# if defined __GNUC_GNU_INLINE__ && __GNUC_GNU_INLINE__
   /* __gnu_inline__ suppresses a GCC 4.2 diagnostic.  */
#  define _GL_INLINE extern inline __attribute__ ((__gnu_inline__))
# else
#  define _GL_INLINE extern inline
# endif
# define _GL_EXTERN_INLINE extern
# define _GL_EXTERN_INLINE_IN_USE
#else
# define _GL_INLINE _GL_UNUSED static
# define _GL_EXTERN_INLINE _GL_UNUSED static
#endif

/* In GCC 4.6 (inclusive) to 5.1 (exclusive),
   suppress bogus "no previous prototype for 'FOO'"
   and "no previous declaration for 'FOO'" diagnostics,
   when FOO is an inline function in the header; see
   <https://gcc.gnu.org/PR54113> and
   <https://gcc.gnu.org/PR63877>.  */
#if __GNUC__ == 4 && 6 <= __GNUC_MINOR__
# if defined __GNUC_STDC_INLINE__ && __GNUC_STDC_INLINE__
#  define _GL_INLINE_HEADER_CONST_PRAGMA
# else
#  define _GL_INLINE_HEADER_CONST_PRAGMA \
     _Pragma ("GCC diagnostic ignored \"-Wsuggest-attribute=const\"")
# endif
# define _GL_INLINE_HEADER_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wmissing-prototypes\"") \
    _Pragma ("GCC diagnostic ignored \"-Wmissing-declarations\"") \
    _GL_INLINE_HEADER_CONST_PRAGMA
# define _GL_INLINE_HEADER_END \
    _Pragma ("GCC diagnostic pop")
#else
# define _GL_INLINE_HEADER_BEGIN
# define _GL_INLINE_HEADER_END
#endif

/* Define to 1 if the compiler supports the keyword '__inline'. */
#define HAVE___INLINE 1

/* Define to 1 if you have the '__secure_getenv' function. */
/* #undef HAVE___SECURE_GETENV */

/* Define to 1 if you have the '__xpg_strerror_r' function. */
/* #undef HAVE___XPG_STRERROR_R */

/* Define HOST_NAME_MAX when <limits.h> does not define it. */
#define HOST_NAME_MAX 256

/* The host operating system. */
#define HOST_OPERATING_SYSTEM "Moxnox"

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST

/* Define to a symbolic name denoting the flavor of iconv_open()
   implementation. */
/* #undef ICONV_FLAVOR */

/* Define as the bit index in the word where to find bit 0 of the exponent of
   'long double'. */
#define LDBL_EXPBIT0_BIT 20

/* Define as the word index where to find the exponent of 'long double'. */
#define LDBL_EXPBIT0_WORD 1

/* Define as the bit index in the word where to find the sign of 'long
   double'. */
/* #undef LDBL_SIGNBIT_BIT */

/* Define as the word index where to find the sign of 'long double'. */
/* #undef LDBL_SIGNBIT_WORD */

/* versioned libcrypto */
/* #undef LIBCRYPTO_SONAME */

/* Define to 1 if linkat can create hardlinks to symlinks */
#define LINKAT_SYMLINK_NOTSUP 0

/* Define to 1 if linkat fails to recognize a trailing slash. */
#define LINKAT_TRAILING_SLASH_BUG 1

/* Define to 1 if 'link(2)' dereferences symbolic links, 0 if it creates hard
   links to symlinks, -1 if it depends on the variable __xpg4, and -2 if
   unknown. */
#define LINK_FOLLOWS_SYMLINKS 0

/* Define if localename.c overrides newlocale(), duplocale(), freelocale(). */
/* #undef LOCALENAME_ENHANCE_LOCALE_FUNCS */

/* Define to 1 if lseek does not detect pipes. */
/* #undef LSEEK_PIPE_BROKEN */

/* Define to 1 if 'lstat' dereferences a symlink specified with a trailing
   slash. */
/* #undef LSTAT_FOLLOWS_SLASHED_SYMLINK */

/* Define to 1 if 'major', 'minor', and 'makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if 'major', 'minor', and 'makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS */

/* URL for full manual */
#define MANUAL_URL "https://www.gnu.org/software/coreutils/manual/coreutils.html"

/* Define to a substitute value for mmap()'s MAP_ANONYMOUS flag. */
/* #undef MAP_ANONYMOUS */

/* Define if the mbrtoc32 function does not return (size_t) -2 for empty
   input. */
/* #undef MBRTOC32_EMPTY_INPUT_BUG */

/* Define if the mbrtoc32 function may signal encoding errors in the C locale.
   */
/* #undef MBRTOC32_IN_C_LOCALE_MAYBE_EILSEQ */

/* Define if the mbrtoc32 function does not accept the input bytes one-by-one.
   */
/* #undef MBRTOC32_MULTIBYTE_LOCALE_BUG */

/* Define if the mbrtowc function does not return (size_t) -2 for empty input.
   */
/* #undef MBRTOWC_EMPTY_INPUT_BUG */

/* Define if the mbrtowc function may signal encoding errors in the C locale.
   */
#define MBRTOWC_IN_C_LOCALE_MAYBE_EILSEQ 1

/* Define if the mbrtowc function has the NULL pwc argument bug. */
/* #undef MBRTOWC_NULL_ARG1_BUG */

/* Define if the mbrtowc function does not return 0 for a NUL character. */
/* #undef MBRTOWC_NUL_RETVAL_BUG */

/* Define if the mbrtowc function returns a wrong return value. */
/* #undef MBRTOWC_RETVAL_BUG */

/* Define if the mbrtowc function stores a wide character when reporting
   incomplete input. */
/* #undef MBRTOWC_STORES_INCOMPLETE_BUG */

/* Use GNU style printf and scanf.  */
#ifndef __USE_MINGW_ANSI_STDIO
# define __USE_MINGW_ANSI_STDIO 1
#endif


/* Define to 1 if the encoding of NaN 'double's is as in IEEE 754-2008 §
   6.2.1. */
/* #undef MIPS_NAN2008_DOUBLE */

/* Define to 1 if the encoding of NaN 'float's is as in IEEE 754-2008 §
   6.2.1. */
/* #undef MIPS_NAN2008_FLOAT */

/* Define to 1 if the encoding of NaN 'long double's is as in IEEE 754-2008 §
   6.2.1. */
/* #undef MIPS_NAN2008_LONG_DOUBLE */

/* Define to 1 if mkfifo does not reject trailing slash */
#define MKFIFO_TRAILING_SLASH_BUG 1

/* Define to 1 if mknod cannot create a fifo without super-user privileges */
#define MKNOD_FIFO_BUG 1

/* Define if (like SVR2) there is no specific function for reading the list of
   mounted file systems, and your system has these header files: <sys/fstyp.h>
   and <sys/statfs.h>. (SVR3) */
/* #undef MOUNTED_FREAD_FSTYP */

/* Define if there are functions named next_dev and fs_stat_dev for reading
   the list of mounted file systems. (BeOS) */
/* #undef MOUNTED_FS_STAT_DEV */

/* Define if there is a function named getextmntent for reading the list of
   mounted file systems. (Solaris) */
/* #undef MOUNTED_GETEXTMNTENT */

/* Define if there is a function named getfsstat for reading the list of
   mounted file systems. (obsolete Darwin) */
/* #undef MOUNTED_GETFSSTAT */

/* Define if there is a function named getmntent for reading the list of
   mounted file systems, and that function takes a single argument. (4.3BSD,
   SunOS, HP-UX) */
/* #undef MOUNTED_GETMNTENT1 */

/* Define if there is a function named getmntent for reading the list of
   mounted file systems, and that function takes two arguments. (SVR4) */
/* #undef MOUNTED_GETMNTENT2 */

/* Define if there is a function named getmntinfo for reading the list of
   mounted file systems and it returns an array of 'struct statfs'. (4.4BSD,
   Darwin) */
/* #undef MOUNTED_GETMNTINFO */

/* Define if there is a function named getmntinfo for reading the list of
   mounted file systems and it returns an array of 'struct statvfs'. (NetBSD
   3.0) */
/* #undef MOUNTED_GETMNTINFO2 */

/* Define if we are on interix, and ought to use statvfs plus some special
   knowledge on where mounted file systems can be found. (Interix) */
/* #undef MOUNTED_INTERIX_STATVFS */

/* Define if we don't know how to determine the list of mounted file systems.
   */
#define MOUNTED_NOT_PORTED 1

/* Define if there is a function named mntctl that can be used to read the
   list of mounted file systems, and there is a system header file that
   declares 'struct vmount'. (AIX) */
/* #undef MOUNTED_VMOUNT */

/* Define to 1 on musl libc. */
/* #undef MUSL_LIBC */

/* Define to 1 if assertions should be disabled. */
/* #undef NDEBUG */

/* Define to 1 if fchmodat+AT_SYMLINK_NOFOLLOW does not work right on
   non-symlinks. */
/* #undef NEED_FCHMODAT_NONSYMLINK_FIX */

/* Define if the compilation of mktime.c should define 'mktime_internal'. */
#define NEED_MKTIME_INTERNAL 1

/* Define if the compilation of mktime.c should define 'mktime' with the
   native Windows TZ workaround. */
/* #undef NEED_MKTIME_WINDOWS */

/* Define if the compilation of mktime.c should define 'mktime' with the
   algorithmic workarounds. */
#define NEED_MKTIME_WORKING 1

/* Define if the vasnprintf implementation needs special code for the 'a' and
   'A' directives. */
/* #undef NEED_PRINTF_DIRECTIVE_A */

/* Define if the vasnprintf implementation needs special code for the 'b'
   directive. */
/* #undef NEED_PRINTF_DIRECTIVE_B */

/* Define if the vasnprintf implementation needs special code for the 'F'
   directive. */
/* #undef NEED_PRINTF_DIRECTIVE_F */

/* Define if the vasnprintf implementation needs special code for the 'lc'
   directive. */
/* #undef NEED_PRINTF_DIRECTIVE_LC */

/* Define if the vasnprintf implementation needs special code for the 'ls'
   directive. */
/* #undef NEED_PRINTF_DIRECTIVE_LS */

/* Define if the vasnprintf implementation needs special code for 'double'
   arguments. */
/* #undef NEED_PRINTF_DOUBLE */

/* Define if the vasnprintf implementation needs special code for surviving
   out-of-memory conditions. */
/* #undef NEED_PRINTF_ENOMEM */

/* Define if the vasnprintf implementation needs special code for the # flag
   with a zero precision and a zero value in the 'x' and 'X' directives. */
/* #undef NEED_PRINTF_FLAG_ALT_PRECISION_ZERO */

/* Define if the vasnprintf implementation needs special code for the ' flag.
   */
/* #undef NEED_PRINTF_FLAG_GROUPING */

/* Define if the vasnprintf implementation needs special code for the ' flag,
   for integer directives only. */
/* #undef NEED_PRINTF_FLAG_GROUPING_INT */

/* Define if the vasnprintf implementation needs special code for the '-'
   flag. */
/* #undef NEED_PRINTF_FLAG_LEFTADJUST */

/* Define if the vasnprintf implementation needs special code for the 0 flag.
   */
/* #undef NEED_PRINTF_FLAG_ZERO */

/* Define if the vasnprintf implementation needs special code for infinite
   'double' arguments. */
/* #undef NEED_PRINTF_INFINITE_DOUBLE */

/* Define if the vasnprintf implementation needs special code for infinite
   'long double' arguments. */
/* #undef NEED_PRINTF_INFINITE_LONG_DOUBLE */

/* Define if the vasnprintf implementation needs special code for 'long
   double' arguments. */
/* #undef NEED_PRINTF_LONG_DOUBLE */

/* Define if the vasnprintf implementation needs special code for supporting
   large precisions without arbitrary bounds. */
/* #undef NEED_PRINTF_UNBOUNDED_PRECISION */

/* Define to 1 if realloc should abort upon undefined behaviour. */
/* #undef NEED_SANITIZED_REALLOC */

/* Define to 1 to enable general improvements of setlocale. */
#define NEED_SETLOCALE_IMPROVED 0

/* Define to 1 to enable a thread safety fix of setlocale. */
#define NEED_SETLOCALE_MTSAFE 1

/* Define to 1 if localtime_rz, mktime_z exist and can be used with non-NULL
   timezone_t values. */
/* #undef NEED_TIMEZONE_NULL_SUPPORT */

/* Define to 1 if nl_langinfo is thread-safe. */
#define NL_LANGINFO_MTSAFE 1

/* Define to 1 if the nlist n_name member is a pointer */
/* #undef N_NAME_POINTER */

/* Define to 1 if open() fails to recognize a trailing slash. */
#define OPEN_TRAILING_SLASH_BUG 1

/* Name of package */
#define PACKAGE "coreutils"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "bug-coreutils@gnu.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "GNU coreutils"

/* String identifying the packager of this software */
/* #undef PACKAGE_PACKAGER */

/* Packager info for bug reports (URL/e-mail/...) */
/* #undef PACKAGE_PACKAGER_BUG_REPORTS */

/* Packager-specific version information */
/* #undef PACKAGE_PACKAGER_VERSION */

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "GNU coreutils 9.11"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "coreutils"

/* Define to the home page for this package. */
#define PACKAGE_URL "https://www.gnu.org/software/coreutils/"

/* Define to the version of this package. */
#define PACKAGE_VERSION "9.11"

/* Define to the maximum link count that a true pipe can have. */
#define PIPE_LINK_COUNT_MAX (1)

/* Define this if you prefer euidaccess to return the correct result even if
   this would make it nonreentrant. Define this only if your entire
   application is safe even if the uid or gid might temporarily change. If
   your application uses signal handlers or threads it is probably not safe.
   */
#define PREFER_NONREENTRANT_EUIDACCESS 1

/* Define to the type that is the result of default argument promotions of
   type mode_t. */
#define PROMOTED_MODE_T int

/* Define if pthread_create is an inline function. */
/* #undef PTHREAD_CREATE_IS_INLINE */

/* Define if the pthread_in_use() detection is hard. */
/* #undef PTHREAD_IN_USE_DETECTION_HARD */

/* Define if the 'robust' attribute of pthread_mutex* doesn't exist. */
#define PTHREAD_MUTEXATTR_ROBUST_UNIMPLEMENTED 1

/* Define if the pthread_rwlock wait queue handling is not reasonable. */
/* #undef PTHREAD_RWLOCK_BAD_WAITQUEUE */

/* Define if the functions pthread_rwlock_timedrdlock and
   pthread_rwlock_timedwrlock don't exist. */
#define PTHREAD_RWLOCK_LACKS_TIMEOUT 1

/* Define if all pthread_rwlock* functions don't exist. */
/* #undef PTHREAD_RWLOCK_UNIMPLEMENTED */

/* Define to 1 if pthread_sigmask(), when it fails, returns -1 and sets errno.
   */
/* #undef PTHREAD_SIGMASK_FAILS_WITH_ERRNO */

/* Define to 1 if pthread_sigmask may return 0 and have no effect. */
/* #undef PTHREAD_SIGMASK_INEFFECTIVE */

/* Define to 1 if pthread_sigmask requires linking with some library. */
/* #undef PTHREAD_SIGMASK_NOT_IN_LIBC */

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'ptrdiff_t'. */
/* #undef PTRDIFF_T_SUFFIX */

/* Define to 1 if readlink fails to recognize a trailing slash. */
#define READLINK_TRAILING_SLASH_BUG 1

/* Define to 1 if readlink sets errno instead of truncating a too-long link.
   */
/* #undef READLINK_TRUNCATE_BUG */

/* Define if the readutmp module should use the systemd login API. */
/* #undef READUTMP_USE_SYSTEMD */

/* Define if rename does not work when the destination file exists, as on
   Cygwin 1.5 or Windows. */
/* #undef RENAME_DEST_EXISTS_BUG */

/* Define if rename fails to leave hard links alone, as on NetBSD 1.6 or
   Cygwin 1.5. */
/* #undef RENAME_HARD_LINK_BUG */

/* Define if rename does not correctly handle slashes on the destination
   argument, such as on Solaris 11 or NetBSD 1.6. */
#define RENAME_TRAILING_SLASH_DEST_BUG 1

/* Define if rename does not correctly handle slashes on the source argument,
   such as on Solaris 9 or cygwin 1.5. */
#define RENAME_TRAILING_SLASH_SOURCE_BUG 1

/* Define to 1 if gnulib's fchdir() replacement is used. */
/* #undef REPLACE_FCHDIR */

/* Define to 1 if stat needs help when passed a file name with a trailing
   slash */
#define REPLACE_FUNC_STAT_FILE 1

/* Define to 1 if utime needs help when passed a file name with a trailing
   slash */
#define REPLACE_FUNC_UTIME_FILE 1

/* Define if nl_langinfo exists but is overridden by gnulib. */
#define REPLACE_NL_LANGINFO 1

/* Define to 1 if open() should work around the inability to open a directory.
   */
/* #undef REPLACE_OPEN_DIRECTORY */

/* Define if gnulib uses its own posix_spawn and posix_spawnp functions. */
#define REPLACE_POSIX_SPAWN 1

/* Define to 1 if strerror(0) does not return a message implying success. */
/* #undef REPLACE_STRERROR_0 */

/* Define if vasnprintf exists but is overridden by gnulib. */
/* #undef REPLACE_VASNPRINTF */

/* Define to 1 if setlocale (LC_ALL, NULL) is thread-safe. */
#define SETLOCALE_NULL_ALL_MTSAFE 0

/* Define to 1 if setlocale (category, NULL) is thread-safe. */
#define SETLOCALE_NULL_ONE_MTSAFE 0

/* File name of the Bourne shell.  */
#if (defined _WIN32 && !defined __CYGWIN__) || defined __CYGWIN__ || defined __ANDROID__
/* Omit the directory part because
   - For native Windows programs in a Cygwin environment, the Cygwin mounts
     are not visible.
   - For 32-bit Cygwin programs in a 64-bit Cygwin environment, the Cygwin
     mounts are not visible.
   - On Android, /bin/sh does not exist. It's /system/bin/sh instead.  */
# define BOURNE_SHELL "sh"
#else
# define BOURNE_SHELL "/bin/sh"
#endif

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'sig_atomic_t'. */
/* #undef SIG_ATOMIC_T_SUFFIX */

/* Define as the maximum value of type 'size_t', if the system doesn't define
   it. */
#ifndef SIZE_MAX
/* # undef SIZE_MAX */
#endif

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'size_t'. */
/* #undef SIZE_T_SUFFIX */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define if the block counts reported by statfs may be truncated to 2GB and
   the correct values may be stored in the f_spare array. (SunOS 4.1.2, 4.1.3,
   and 4.1.3_U1 are reported to have this problem. SunOS 4.1.1 seems not to be
   affected.) */
/* #undef STATFS_TRUNCATES_BLOCK_COUNTS */

/* Define to 1 if the 'S_IS*' macros in <sys/stat.h> do not work properly. */
/* #undef STAT_MACROS_BROKEN */

/* Define if statfs takes 2 args and struct statfs has a field named f_bsize.
   (4.3BSD, SunOS 4, HP-UX) */
/* #undef STAT_STATFS2_BSIZE */

/* Define if statfs takes 2 args and struct statfs has a field named f_frsize.
   (glibc/Linux > 2.6) */
/* #undef STAT_STATFS2_FRSIZE */

/* Define if statfs takes 2 args and struct statfs has a field named f_fsize.
   (4.4BSD, NetBSD) */
/* #undef STAT_STATFS2_FSIZE */

/* Define if statfs takes 4 args. (SVR3) */
/* #undef STAT_STATFS4 */

/* Define if there is a function named statvfs. (SVR4) */
#define STAT_STATVFS 1

/* Define if statvfs64 should be preferred over statvfs. */
/* #undef STAT_STATVFS64 */

/* Define to 1 if all of the C89 standard headers exist (not just the ones
   required in a freestanding environment). This macro is provided for
   backward compatibility; new code need not use it. */
#define STDC_HEADERS 1

/* Define to 1 if strerror_r returns char *. */
/* #undef STRERROR_R_CHAR_P */

/* Define to 1 if strtod does not set errno upon gradual underflow. */
/* #undef STRTOD_HAS_GRADUAL_UNDERFLOW_PROBLEM */

/* Define to 1 if strtod does not set errno upon flush-to-zero underflow. */
/* #undef STRTOD_HAS_UNDERFLOW_BUG */

/* Define to 1 if strtold does not set errno upon gradual underflow. */
/* #undef STRTOLD_HAS_GRADUAL_UNDERFLOW_PROBLEM */

/* Define to 1 if strtold does not set errno upon flush-to-zero underflow. */
/* #undef STRTOLD_HAS_UNDERFLOW_BUG */

/* Define to 1 if the f_fsid member of struct statfs is an integer. */
/* #undef STRUCT_STATFS_F_FSID_IS_INTEGER */

/* Define to 1 if the f_fsid member of struct statvfs is an integer. */
/* #undef STRUCT_STATVFS_F_FSID_IS_INTEGER */

/* Define to 1 on System V Release 4. */
/* #undef SVR4 */

/* FIXME */
/* #undef TERMIOS_NEEDS_XOPEN_SOURCE */

/* Define to 1 if time_t is signed. */
#define TIME_T_IS_SIGNED 1

/* group used by system for TTYs */
/* #undef TTY_GROUP_NAME */

/* Define to 1 if the type of the st_atim member of a struct stat is struct
   timespec. */
#define TYPEOF_STRUCT_STAT_ST_ATIM_IS_STRUCT_TIMESPEC 1

/* Define to 1 for Encore UMAX. */
/* #undef UMAX */

/* Define to 1 for Encore UMAX 4.3 that has <inq_status/cpustats.h> instead of
   <sys/cpustats.h>. */
/* #undef UMAX4_3 */

/* Define to 1 if unlink (dir) cannot possibly succeed. */
/* #undef UNLINK_CANNOT_UNLINK_DIR */

/* Define to 1 if unlink() on a parent directory may succeed */
/* #undef UNLINK_PARENT_BUG */

/* Define to nonzero if you want access control list support. */
#define USE_ACL 0

/* CRC32 calculation by avx2 hardware instructions enabled */
#ifdef __x86_64__
#define USE_AVX2_CRC32 1
#endif

/* Counting lines with AVX2 enabled */
#ifdef __x86_64__
#define USE_AVX2_WC_LINECOUNT 1
#endif

/* CRC32 calculation by avx512 hardware instructions enabled */
#ifdef __x86_64__
#define USE_AVX512_CRC32 1
#endif

/* Counting lines with AVX512 enabled */
#ifdef __x86_64__
#define USE_AVX512_WC_LINECOUNT 1
#endif

/* Define if the combination of the ISO C and POSIX multithreading APIs can be
   used. */
/* #undef USE_ISOC_AND_POSIX_THREADS */

/* Define if the ISO C multithreading library can be used. */
/* #undef USE_ISOC_THREADS */

/* Define to enable the declarations of ISO C 23 Annex K types and functions.  */
#if !(defined __STDC_WANT_LIB_EXT1__ && __STDC_WANT_LIB_EXT1__)
#undef/**/__STDC_WANT_LIB_EXT1__
#define __STDC_WANT_LIB_EXT1__ 1
#endif


/* Define to 1 if you want to use the Linux kernel cryptographic API. */
#define USE_LINUX_CRYPTO_API 0

/* Counting lines with Neon enabled */
/* #undef USE_NEON_WC_LINECOUNT */

/* CRC32 calculation by pclmul hardware instruction enabled */
#ifdef __x86_64__
#define USE_PCLMUL_CRC32 1
#endif

/* Define if the POSIX multithreading library can be used. */
#define USE_POSIX_THREADS 1

/* Define if references to the POSIX multithreading library are satisfied by
   libc. */
#define USE_POSIX_THREADS_FROM_LIBC 1

/* Define if references to the POSIX multithreading library should be made
   weak. */
/* #undef USE_POSIX_THREADS_WEAK */

/* Define to 1 if <selinux/selinux.h> should be used, to 0 otherwise. */
#define USE_SELINUX_SELINUX_H 0

/* Enable extensions on AIX, Interix, z/OS.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable extensions on Cosmopolitan Libc. */
#ifndef _COSMO_SOURCE
# define _COSMO_SOURCE 1
#endif
/* Enable general extensions on macOS.  */
#ifndef _DARWIN_C_SOURCE
# define _DARWIN_C_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable X/Open compliant socket functions that do not require linking
   with -lxnet on HP-UX 11.11.  */
#ifndef _HPUX_ALT_XOPEN_SOCKET_API
# define _HPUX_ALT_XOPEN_SOCKET_API 1
#endif
/* Identify the host operating system as Minix.
   This macro does not affect the system headers' behavior.
   A future release of Autoconf may stop defining this macro.  */
#ifndef _MINIX
/* # undef _MINIX */
#endif
/* Enable general extensions on NetBSD.
   Enable NetBSD compatibility extensions on Minix.  */
#ifndef _NETBSD_SOURCE
# define _NETBSD_SOURCE 1
#endif
/* Enable OpenBSD compatibility extensions on NetBSD.
   Oddly enough, this does nothing on OpenBSD.  */
#ifndef _OPENBSD_SOURCE
# define _OPENBSD_SOURCE 1
#endif
/* Define to 1 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_SOURCE
/* # undef _POSIX_SOURCE */
#endif
/* Define to 2 if needed for POSIX-compatible behavior.  */
#ifndef _POSIX_1_SOURCE
/* # undef _POSIX_1_SOURCE */
#endif
/* Enable POSIX-compatible threading on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-5:2014.  */
#ifndef __STDC_WANT_IEC_60559_ATTRIBS_EXT__
# define __STDC_WANT_IEC_60559_ATTRIBS_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-1:2014.  */
#ifndef __STDC_WANT_IEC_60559_BFP_EXT__
# define __STDC_WANT_IEC_60559_BFP_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-2:2015.  */
#ifndef __STDC_WANT_IEC_60559_DFP_EXT__
# define __STDC_WANT_IEC_60559_DFP_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex F.  */
#ifndef __STDC_WANT_IEC_60559_EXT__
# define __STDC_WANT_IEC_60559_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TS 18661-4:2015.  */
#ifndef __STDC_WANT_IEC_60559_FUNCS_EXT__
# define __STDC_WANT_IEC_60559_FUNCS_EXT__ 1
#endif
/* Enable extensions specified by C23 Annex H and ISO/IEC TS 18661-3:2015.  */
#ifndef __STDC_WANT_IEC_60559_TYPES_EXT__
# define __STDC_WANT_IEC_60559_TYPES_EXT__ 1
#endif
/* Enable extensions specified by ISO/IEC TR 24731-2:2010.  */
#ifndef __STDC_WANT_LIB_EXT2__
# define __STDC_WANT_LIB_EXT2__ 1
#endif
/* Enable extensions specified by ISO/IEC 24747:2009.  */
#ifndef __STDC_WANT_MATH_SPEC_FUNCS__
# define __STDC_WANT_MATH_SPEC_FUNCS__ 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable X/Open extensions.  Define to 500 only if necessary
   to make mbstate_t available.  */
#ifndef _XOPEN_SOURCE
/* # undef _XOPEN_SOURCE */
#endif


/* An alias of GNULIB_STDIO_SINGLE_THREAD. */
#define USE_UNLOCKED_IO GNULIB_STDIO_SINGLE_THREAD

/* CRC32 calculation by vmull hardware instruction enabled */
/* #undef USE_VMULL_CRC32 */

/* Define if the native Windows multithreading API can be used. */
/* #undef USE_WINDOWS_THREADS */

/* Define to 1 to use the Linux extended attributes library. */
/* #undef USE_XATTR */

/* Version number of package */
#define VERSION "9.11"

/* Define to 1 if unsetenv returns void instead of int. */
/* #undef VOID_UNSETENV */

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'wchar_t'. */
/* #undef WCHAR_T_SUFFIX */

/* Define if the wcrtomb function does not work in the C locale. */
/* #undef WCRTOMB_C_LOCALE_BUG */

/* Define if the wcrtomb function has an incorrect return value. */
/* #undef WCRTOMB_RETVAL_BUG */

/* Define if WSAStartup is needed. */
/* #undef WINDOWS_SOCKETS */

/* Define if sys/ptem.h is required for struct winsize. */
/* #undef WINSIZE_IN_PTEM */

/* Define to l, ll, u, ul, ull, etc., as suitable for constants of type
   'wint_t'. */
/* #undef WINT_T_SUFFIX */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* True if the compiler says it groks GNU C version MAJOR.MINOR.
    Except that
      - clang groks GNU C 4.2, even on Windows, where it does not define
        __GNUC__.
      - The OpenMandriva-modified clang compiler pretends that it groks
        GNU C version 13.1, but it doesn't: It does not support
        __attribute__ ((__malloc__ (f, i))), nor does it support
        __attribute__ ((__warning__ (message))) on a function redeclaration.
      - Users can make clang lie as well, through the -fgnuc-version option.  */
#if defined __GNUC__ && defined __GNUC_MINOR__ && !defined __clang__
# define _GL_GNUC_PREREQ(major, minor) \
    ((major) < __GNUC__ + ((minor) <= __GNUC_MINOR__))
#elif defined __clang__
  /* clang really only groks GNU C 4.2.  */
# define _GL_GNUC_PREREQ(major, minor) \
    ((major) < 4 + ((minor) <= 2))
#else
# define _GL_GNUC_PREREQ(major, minor) 0
#endif


/* Define to enable the declarations of ISO C 11 types and functions. */
/* #undef _ISOC11_SOURCE */

/* Define to 1 if necessary to make fseeko visible. */
/* #undef _LARGEFILE_SOURCE */

/* Define to 1 on platforms where this makes off_t a 64-bit type. */
/* #undef _LARGE_FILES */

/* Define to 1 on Solaris. */
/* #undef _LCONV_C99 */

/* Define so that AIX headers are more compatible with GNU/Linux. */
#define _LINUX_SOURCE_COMPAT 1

/* The _Noreturn keyword of C11.
   Do not use [[noreturn]], because with it the syntax
     extern _Noreturn void func (...);
   would not be valid; such a declaration would be valid only with 'extern'
   and '_Noreturn' swapped, or without the 'extern' keyword.  However, some
   AIX system header files and several gnulib header files use precisely
   this syntax with 'extern'.  So even though C23 deprecates _Noreturn,
   it is currently more portable to prefer it to [[noreturn]].

   Also, do not try to work around LLVM bug 59792 (clang 15 or earlier).
   This rare bug can be worked around by compiling with 'clang -D_Noreturn=',
   though the workaround may generate many false-alarm warnings.  */
#ifndef _Noreturn
# if ((!defined __cplusplus || defined __clang__) \
      && (201112 <= (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0)))
   /* _Noreturn works as-is.  */
# elif _GL_GNUC_PREREQ (2, 8) || defined __clang__ || 0x5110 <= __SUNPRO_C
   /* Prefer __attribute__ ((__noreturn__)) to plain _Noreturn even if the
      latter works, as 'gcc -std=gnu99 -Wpedantic' warns about _Noreturn.  */
#  define _Noreturn __attribute__ ((__noreturn__))
# elif 1200 <= (defined _MSC_VER ? _MSC_VER : 0)
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn
# endif
#endif


/* Define if you want <regex.h> to include <limits.h>, so that it consistently
   overrides <limits.h>'s RE_DUP_MAX. */
#define _REGEX_INCLUDE_LIMITS_H 1

/* Define if you want regoff_t to be at least as wide POSIX requires. */
#define _REGEX_LARGE_OFFSETS 1

/* Number of bits in time_t, on hosts where this is settable. */
/* #undef _TIME_BITS */

/* For standard stat data types on VMS. */
#define _USE_STD_STAT 1

/* Define to rpl_ if the getopt replacement functions and variables should be
   used. */
/* #undef __GETOPT_PREFIX */

/* Define to 1 on platforms where this makes time_t a 64-bit type. */
/* #undef __MINGW_USE_VC2005_COMPAT */

/* Define to 1 if the system <stdint.h> predates C++11. */
/* #undef __STDC_CONSTANT_MACROS */

/* Define to 1 if the system <stdint.h> predates C++11. */
/* #undef __STDC_LIMIT_MACROS */

/* Define to 1 if C does not support variable-length arrays, and if the
   compiler does not already define this. */
/* #undef __STDC_NO_VLA__ */

/* The _GL_ASYNC_SAFE marker should be attached to functions that are
   signal handlers (for signals other than SIGABRT, SIGPIPE) or can be
   invoked from such signal handlers.  Such functions have some restrictions:
     * All functions that it calls should be marked _GL_ASYNC_SAFE as well,
       or should be listed as async-signal-safe in POSIX
       <https://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_04>
       section 2.4.3.  Note that malloc(), sprintf(), and fwrite(), in
       particular, are NOT async-signal-safe.
     * All memory locations (variables and struct fields) that these functions
       access must be marked 'volatile'.  This holds for both read and write
       accesses.  Otherwise the compiler might optimize away stores to and
       reads from such locations that occur in the program, depending on its
       data flow analysis.  For example, when the program contains a loop
       that is intended to inspect a variable set from within a signal handler
           while (!signal_occurred)
             ;
       the compiler is allowed to transform this into an endless loop if the
       variable 'signal_occurred' is not declared 'volatile'.
   Additionally, recall that:
     * A signal handler should not modify errno (except if it is a handler
       for a fatal signal and ends by raising the same signal again, thus
       provoking the termination of the process).  If it invokes a function
       that may clobber errno, it needs to save and restore the value of
       errno.  */
#define _GL_ASYNC_SAFE


/* Attributes.  */
/* Define _GL_HAS_ATTRIBUTE only once, because on FreeBSD, with gcc < 5, if
   <config.h> gets included once again after <sys/cdefs.h>, __has_attribute(x)
   expands to 0 always, and redefining _GL_HAS_ATTRIBUTE would turn off all
   attributes.  */
#ifndef _GL_HAS_ATTRIBUTE
# if (defined __has_attribute \
      && (!defined __clang_minor__ \
          || (defined __apple_build_version__ \
              ? 7000000 <= __apple_build_version__ \
              : 5 <= __clang_major__)))
#  define _GL_HAS_ATTRIBUTE(attr) __has_attribute (__##attr##__)
# else
#  define _GL_HAS_ATTRIBUTE(attr) _GL_ATTR_##attr
/* The following lines list the first GCC version that supports the attribute.
   Although the lines are not used in GCC 5 and later (as GCC 5 introduced
   __has_attribute support), list GCC versions 5+ anyway for completeness.  */
#  define _GL_ATTR_alloc_size _GL_GNUC_PREREQ (4, 3)
#  define _GL_ATTR_always_inline _GL_GNUC_PREREQ (3, 2)
#  define _GL_ATTR_artificial _GL_GNUC_PREREQ (4, 3)
#  define _GL_ATTR_cold _GL_GNUC_PREREQ (4, 3)
#  define _GL_ATTR_const _GL_GNUC_PREREQ (2, 95)
#  define _GL_ATTR_deprecated _GL_GNUC_PREREQ (3, 1)
#  define _GL_ATTR_diagnose_if 0
#  define _GL_ATTR_error _GL_GNUC_PREREQ (4, 3)
#  define _GL_ATTR_externally_visible _GL_GNUC_PREREQ (4, 1)
#  define _GL_ATTR_fallthrough _GL_GNUC_PREREQ (7, 0)
#  define _GL_ATTR_format _GL_GNUC_PREREQ (2, 7)
#  define _GL_ATTR_leaf _GL_GNUC_PREREQ (4, 6)
#  define _GL_ATTR_malloc _GL_GNUC_PREREQ (3, 0)
#  ifdef _ICC
#   define _GL_ATTR_may_alias 0
#  else
#   define _GL_ATTR_may_alias _GL_GNUC_PREREQ (3, 3)
#  endif
#  define _GL_ATTR_noinline _GL_GNUC_PREREQ (3, 1)
#  define _GL_ATTR_nonnull _GL_GNUC_PREREQ (3, 3)
#  define _GL_ATTR_nonnull_if_nonzero _GL_GNUC_PREREQ (15, 1)
#  define _GL_ATTR_nonstring _GL_GNUC_PREREQ (8, 0)
#  define _GL_ATTR_nothrow _GL_GNUC_PREREQ (3, 3)
#  define _GL_ATTR_packed _GL_GNUC_PREREQ (2, 7)
#  define _GL_ATTR_pure _GL_GNUC_PREREQ (2, 96)
#  define _GL_ATTR_reproducible _GL_GNUC_PREREQ (15, 1)
#  define _GL_ATTR_returns_nonnull _GL_GNUC_PREREQ (4, 9)
#  define _GL_ATTR_sentinel _GL_GNUC_PREREQ (4, 0)
#  define _GL_ATTR_unsequenced _GL_GNUC_PREREQ (15, 1)
#  define _GL_ATTR_unused _GL_GNUC_PREREQ (2, 7)
#  define _GL_ATTR_warn_unused_result _GL_GNUC_PREREQ (3, 4)
# endif
#endif

/* Use __has_c_attribute if available.  However, do not use with
   pre-C23 GCC, which can issue false positives if -Wpedantic.  */
#if (defined __has_c_attribute \
     && ! (_GL_GNUC_PREREQ (4, 6) \
           && (defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) <= 201710))
# define _GL_HAVE___HAS_C_ATTRIBUTE 1
#else
# define _GL_HAVE___HAS_C_ATTRIBUTE 0
#endif

/* Attributes in bracket syntax [[...]] vs. attributes in __attribute__((...))
   syntax, in function declarations.  There are two problems here.
   (Last tested with gcc/g++ 14 and clang/clang++ 18.)

   1) We want that the _GL_ATTRIBUTE_* can be cumulated on the same declaration
      in any order.
      =========================== foo.c = foo.cc ===========================
      __attribute__ ((__deprecated__)) [[__nodiscard__]] int bar1 (int);
      [[__nodiscard__]] __attribute__ ((__deprecated__)) int bar2 (int);
      ======================================================================
      This gives a syntax error
        - in C mode with gcc
          <https://gcc.gnu.org/PR108796>, and
        - in C++ mode with clang++ version < 16, and
        - in C++ mode, inside extern "C" {}, still in newer clang++ versions
          <https://github.com/llvm/llvm-project/issues/101990>.
 */
/* Define if, in a function declaration, the attributes in bracket syntax
   [[...]] must come before the attributes in __attribute__((...)) syntax.
   If this is defined, it is best to avoid the bracket syntax, so that the
   various _GL_ATTRIBUTE_* can be cumulated on the same declaration in any
   order.  */
#ifdef __cplusplus
# if defined __clang__
#  define _GL_BRACKET_BEFORE_ATTRIBUTE 1
# endif
#else
# if defined __GNUC__ && !defined __clang__
#  define _GL_BRACKET_BEFORE_ATTRIBUTE 1
# endif
#endif
/*
   2) We want that the _GL_ATTRIBUTE_* can be placed in a declaration
        - without 'extern', in C as well as in C++,
        - with 'extern', in C,
        - with 'extern "C"', in C++
      in the same position.  That is, we don't want to be forced to use a
      macro which arranges for the attribute to come before 'extern' in
      one case and after 'extern' in the other case, because such a macro
      would make the source code of .h files pretty ugly.
      =========================== foo.c = foo.cc ===========================
      #ifdef __cplusplus
      # define CC "C"
      #else
      # define CC
      #endif

      #define ND   [[__nodiscard__]]
      #define WUR  __attribute__((__warn_unused_result__))

      #ifdef __cplusplus
      extern "C" {
      #endif
                                        // gcc   clang  g++   clang++

      ND int foo (int);
      int ND foo (int);                 // warn  error  warn  error
      int foo ND (int);
      int foo (int) ND;                 // warn  error  warn  error

      WUR int foo (int);
      int WUR foo (int);
      int fo1 WUR (int);                // error error  error error
      int foo (int) WUR;

      #ifdef __cplusplus
      }
      #endif

                                        // gcc   clang  g++   clang++

      ND extern CC int foo (int);       //              error error
      extern CC ND int foo (int);       // error error
      extern CC int ND foo (int);       // warn  error  warn  error
      extern CC int foo ND (int);
      extern CC int foo (int) ND;       // warn  error  warn  error

      WUR extern CC int foo (int);      //              warn
      extern CC WUR int foo (int);
      extern CC int WUR foo (int);
      extern CC int foo WUR (int);      // error error  error error
      extern CC int foo (int) WUR;

      ND EXTERN_C_FUNC int foo (int);   //              error error
      EXTERN_C_FUNC ND int foo (int);
      EXTERN_C_FUNC int ND foo (int);   // warn  error  warn  error
      EXTERN_C_FUNC int foo ND (int);
      EXTERN_C_FUNC int foo (int) ND;   // warn  error  warn  error

      WUR EXTERN_C_FUNC int foo (int);  //              warn
      EXTERN_C_FUNC WUR int foo (int);
      EXTERN_C_FUNC int WUR foo (int);
      EXTERN_C_FUNC int fo2 WUR (int);  // error error  error error
      EXTERN_C_FUNC int foo (int) WUR;
      ======================================================================
      So, if we insist on using the 'extern' keyword ('extern CC' idiom):
        * If _GL_ATTRIBUTE_* expands to bracket syntax [[...]]
          in both C and C++, there is one available position:
            - between the function name and the parameter list.
        * If _GL_ATTRIBUTE_* expands to __attribute__((...)) syntax
          in both C and C++, there are several available positions:
            - before the return type,
            - between return type and function name,
            - at the end of the declaration.
        * If _GL_ATTRIBUTE_* expands to bracket syntax [[...]] in C and to
          __attribute__((...)) syntax in C++, there is no available position:
          it would need to come before 'extern' in C but after 'extern "C"'
          in C++.
        * If _GL_ATTRIBUTE_* expands to __attribute__((...)) syntax in C and
          to bracket syntax [[...]] in C++, there is one available position:
            - before the return type.
      Whereas, if we use the 'EXTERN_C_FUNC' idiom, which conditionally
      omits the 'extern' keyword:
        * If _GL_ATTRIBUTE_* expands to bracket syntax [[...]]
          in both C and C++, there are two available positions:
            - before the return type,
            - between the function name and the parameter list.
        * If _GL_ATTRIBUTE_* expands to __attribute__((...)) syntax
          in both C and C++, there are several available positions:
            - before the return type,
            - between return type and function name,
            - at the end of the declaration.
        * If _GL_ATTRIBUTE_* expands to bracket syntax [[...]] in C and to
          __attribute__((...)) syntax in C++, there is one available position:
            - before the return type.
        * If _GL_ATTRIBUTE_* expands to __attribute__((...)) syntax in C and
          to bracket syntax [[...]] in C++, there is one available position:
            - before the return type.
      The best choice is therefore to use the 'EXTERN_C_FUNC' idiom and
      put the attributes before the return type. This works regardless
      to what the _GL_ATTRIBUTE_* macros expand.
 */

/* Attributes in bracket syntax [[...]] vs. attributes in __attribute__((...))
   syntax, in static/inline function definitions.

   There are similar constraints as for function declarations.  However, here,
   we cannot omit the storage-class specifier.  Therefore, the following rule
   applies:
     * The macros
         _GL_ATTRIBUTE_CONST
         _GL_ATTRIBUTE_DEPRECATED
         _GL_ATTRIBUTE_MAYBE_UNUSED
         _GL_ATTRIBUTE_NODISCARD
         _GL_ATTRIBUTE_PURE
         _GL_ATTRIBUTE_REPRODUCIBLE
         _GL_ATTRIBUTE_UNSEQUENCED
       which may expand to bracket syntax [[...]], must come first, before the
       storage-class specifier.
     * Other _GL_ATTRIBUTE_* macros, that expand to __attribute__((...)) syntax,
       are better placed between the storage-class specifier and the return
       type.
 */

/* Attributes in bracket syntax [[...]] vs. attributes in __attribute__((...))
   syntax, in variable declarations.

   At which position can they be placed?
   (Last tested with gcc/g++ 14 and clang/clang++ 18.)

      =========================== foo.c = foo.cc ===========================
      #ifdef __cplusplus
      # define CC "C"
      #else
      # define CC
      #endif

      #define BD   [[__deprecated__]]
      #define AD   __attribute__ ((__deprecated__))

                              // gcc   clang  g++    clang++

      BD extern CC int var;   //              error  error
      extern CC BD int var;   // error error
      extern CC int BD var;   // warn  error  warn   error
      extern CC int var BD;

      AD extern CC int var;   //              warn
      extern CC AD int var;
      extern CC int AD var;
      extern CC int var AD;

      BD extern CC int z[];   //              error  error
      extern CC BD int z[];   // error error
      extern CC int BD z[];   // warn  error  warn   error
      extern CC int z1 BD [];
      extern CC int z[] BD;   // warn  error         error

      AD extern CC int z[];   //              warn
      extern CC AD int z[];
      extern CC int AD z[];
      extern CC int z2 AD []; // error error  error  error
      extern CC int z[] AD;
      ======================================================================

   * For non-array variables, the only good position is after the variable name,
     that is, at the end of the declaration.
   * For array variables, you will need to distinguish C and C++:
       - In C, before the 'extern' keyword.
       - In C++, between the 'extern "C"' and the variable's type.
 */

/* _GL_ATTRIBUTE_ALLOC_SIZE ((N)) declares that the Nth argument of the function
   is the size of the returned memory block.
   _GL_ATTRIBUTE_ALLOC_SIZE ((M, N)) declares that the Mth argument multiplied
   by the Nth argument of the function is the size of the returned memory block.
 */
/* Applies to: functions, pointer to functions, function types.  */
#ifndef _GL_ATTRIBUTE_ALLOC_SIZE
# if _GL_HAS_ATTRIBUTE (alloc_size)
#  define _GL_ATTRIBUTE_ALLOC_SIZE(args) __attribute__ ((__alloc_size__ args))
# else
#  define _GL_ATTRIBUTE_ALLOC_SIZE(args)
# endif
#endif

/* _GL_ATTRIBUTE_ALWAYS_INLINE tells that the compiler should always inline the
   function and report an error if it cannot do so.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_ALWAYS_INLINE
# if _GL_HAS_ATTRIBUTE (always_inline)
#  define _GL_ATTRIBUTE_ALWAYS_INLINE __attribute__ ((__always_inline__))
# else
#  define _GL_ATTRIBUTE_ALWAYS_INLINE
# endif
#endif

/* _GL_ATTRIBUTE_ARTIFICIAL declares that the function is not important to show
    in stack traces when debugging.  The compiler should omit the function from
    stack traces.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_ARTIFICIAL
# if _GL_HAS_ATTRIBUTE (artificial)
#  define _GL_ATTRIBUTE_ARTIFICIAL __attribute__ ((__artificial__))
# else
#  define _GL_ATTRIBUTE_ARTIFICIAL
# endif
#endif

/* _GL_ATTRIBUTE_COLD declares that the function is rarely executed.  */
/* Applies to: functions.  */
/* Avoid __attribute__ ((cold)) on MinGW; see thread starting at
   <https://lists.gnu.org/r/emacs-devel/2019-04/msg01152.html>.
   Also, Oracle Studio 12.6 requires 'cold' not '__cold__'.  */
#ifndef _GL_ATTRIBUTE_COLD
# if _GL_HAS_ATTRIBUTE (cold) && !defined __MINGW32__
#  ifndef __SUNPRO_C
#   define _GL_ATTRIBUTE_COLD __attribute__ ((__cold__))
#  else
#   define _GL_ATTRIBUTE_COLD __attribute__ ((cold))
#  endif
# else
#  define _GL_ATTRIBUTE_COLD
# endif
#endif

/* _GL_ATTRIBUTE_CONST declares:
   It is OK for a compiler to move a call, or omit a duplicate call
   and reuse a cached return value, even if the state changes between calls.
   It is also OK to omit a call if the result is not used.
   This attribute is safe if the function does not change observable state,
   returns a value determined solely by its arguments' values
   without examining state, and always returns exactly once -
   e.g., does not raise an exception, call longjmp, or loop forever.
   (This attribute is stricter than _GL_ATTRIBUTE_PURE because the
   function cannot observe state.  Unlike _GL_ATTRIBUTE_UNSEQUENCED
   the function must return exactly once and cannot access state
   addressed by its pointer arguments or that happens to have the same
   value for all calls to the function, but the function is allowed to
   return a pointer to storage that can be modified later.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_CONST
# if _GL_HAS_ATTRIBUTE (const)
#  define _GL_ATTRIBUTE_CONST __attribute__ ((__const__))
# else
#  define _GL_ATTRIBUTE_CONST
# endif
#endif

/* _GL_ATTRIBUTE_DEALLOC (F, I) declares that the function returns pointers
   that can be freed by passing them as the Ith argument to the
   function F.
   _GL_ATTRIBUTE_DEALLOC_FREE declares that the function returns pointers that
   can be freed via 'free'; it can be used only after declaring 'free'.  */
/* Applies to: functions.  Cannot be used on inline functions.  */
#ifndef _GL_ATTRIBUTE_DEALLOC
# if _GL_GNUC_PREREQ (11, 0)
#  define _GL_ATTRIBUTE_DEALLOC(f, i) __attribute__ ((__malloc__ (f, i)))
# else
#  define _GL_ATTRIBUTE_DEALLOC(f, i)
# endif
#endif
/* If gnulib's <string.h> or <wchar.h> has already defined this macro, continue
   to use this earlier definition, since <stdlib.h> may not have been included
   yet.  */
#ifndef _GL_ATTRIBUTE_DEALLOC_FREE
# if defined __cplusplus && defined __GNUC__ && !defined __clang__
/* Work around GCC bug <https://gcc.gnu.org/PR108231> */
#  define _GL_ATTRIBUTE_DEALLOC_FREE \
     _GL_ATTRIBUTE_DEALLOC ((void (*) (void *)) free, 1)
# else
#  define _GL_ATTRIBUTE_DEALLOC_FREE \
     _GL_ATTRIBUTE_DEALLOC (free, 1)
# endif
#endif

/* _GL_ATTRIBUTE_DEPRECATED: Declares that an entity is deprecated.
   The compiler may warn if the entity is used.  */
/* Applies to:
     - function, variable,
     - struct, union, struct/union member,
     - enumeration, enumeration item,
     - typedef,
   in C++ also: namespace, class, template specialization.  */
#ifndef _GL_ATTRIBUTE_DEPRECATED
# ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#  if _GL_HAVE___HAS_C_ATTRIBUTE
#   if __has_c_attribute (__deprecated__)
#    define _GL_ATTRIBUTE_DEPRECATED [[__deprecated__]]
#   endif
#  endif
# endif
# if !defined _GL_ATTRIBUTE_DEPRECATED && _GL_HAS_ATTRIBUTE (deprecated)
#  define _GL_ATTRIBUTE_DEPRECATED __attribute__ ((__deprecated__))
# endif
# ifndef _GL_ATTRIBUTE_DEPRECATED
#  define _GL_ATTRIBUTE_DEPRECATED
# endif
#endif

/* _GL_ATTRIBUTE_ERROR(msg) requests an error if a function is called and
   the function call is not optimized away.
   _GL_ATTRIBUTE_WARNING(msg) requests a warning if a function is called and
   the function call is not optimized away.  */
/* Applies to: functions.  */
#if !(defined _GL_ATTRIBUTE_ERROR && defined _GL_ATTRIBUTE_WARNING)
# if _GL_HAS_ATTRIBUTE (error)
#  define _GL_ATTRIBUTE_ERROR(msg) __attribute__ ((__error__ (msg)))
#  define _GL_ATTRIBUTE_WARNING(msg) __attribute__ ((__warning__ (msg)))
# elif _GL_HAS_ATTRIBUTE (diagnose_if)
#  define _GL_ATTRIBUTE_ERROR(msg) __attribute__ ((__diagnose_if__ (1, msg, "error")))
#  define _GL_ATTRIBUTE_WARNING(msg) __attribute__ ((__diagnose_if__ (1, msg, "warning")))
# else
#  define _GL_ATTRIBUTE_ERROR(msg)
#  define _GL_ATTRIBUTE_WARNING(msg)
# endif
#endif

/* _GL_ATTRIBUTE_EXTERNALLY_VISIBLE declares that the entity should remain
   visible to debuggers etc., even with '-fwhole-program'.  */
/* Applies to: functions, variables.  */
#ifndef _GL_ATTRIBUTE_EXTERNALLY_VISIBLE
# if _GL_HAS_ATTRIBUTE (externally_visible)
#  define _GL_ATTRIBUTE_EXTERNALLY_VISIBLE __attribute__ ((externally_visible))
# else
#  define _GL_ATTRIBUTE_EXTERNALLY_VISIBLE
# endif
#endif

/* _GL_ATTRIBUTE_FALLTHROUGH declares that it is not a programming mistake if
   the control flow falls through to the immediately following 'case' or
   'default' label.  The compiler should not warn in this case.  */
/* Applies to: Empty statement (;), inside a 'switch' statement.  */
/* Always expands to something.  */
#ifndef _GL_ATTRIBUTE_FALLTHROUGH
# if _GL_HAVE___HAS_C_ATTRIBUTE
#  if __has_c_attribute (__fallthrough__)
#   define _GL_ATTRIBUTE_FALLTHROUGH [[__fallthrough__]]
#  endif
# endif
# if !defined _GL_ATTRIBUTE_FALLTHROUGH && _GL_HAS_ATTRIBUTE (fallthrough)
#  define _GL_ATTRIBUTE_FALLTHROUGH __attribute__ ((__fallthrough__))
# endif
# ifndef _GL_ATTRIBUTE_FALLTHROUGH
#  define _GL_ATTRIBUTE_FALLTHROUGH ((void) 0)
# endif
#endif

/* _GL_ATTRIBUTE_FORMAT ((ARCHETYPE, STRING-INDEX, FIRST-TO-CHECK))
   declares that the STRING-INDEXth function argument is a format string of
   style ARCHETYPE, which is one of:
     printf, gnu_printf
     scanf, gnu_scanf,
     strftime, gnu_strftime,
     strfmon,
   or the same thing prefixed and suffixed with '__'.
   If FIRST-TO-CHECK is not 0, arguments starting at FIRST-TO_CHECK
   are suitable for the format string.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_FORMAT
# if _GL_HAS_ATTRIBUTE (format)
#  define _GL_ATTRIBUTE_FORMAT(spec) __attribute__ ((__format__ spec))
# else
#  define _GL_ATTRIBUTE_FORMAT(spec)
# endif
#endif

/* _GL_ATTRIBUTE_LEAF declares that if the function is called from some other
   compilation unit, it executes code from that unit only by return or by
   exception handling.  This declaration lets the compiler optimize that unit
   more aggressively.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_LEAF
# if _GL_HAS_ATTRIBUTE (leaf)
#  define _GL_ATTRIBUTE_LEAF __attribute__ ((__leaf__))
# else
#  define _GL_ATTRIBUTE_LEAF
# endif
#endif

/* _GL_ATTRIBUTE_MALLOC declares that the function returns a pointer to freshly
   allocated memory.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_MALLOC
# if _GL_HAS_ATTRIBUTE (malloc)
#  define _GL_ATTRIBUTE_MALLOC __attribute__ ((__malloc__))
# else
#  define _GL_ATTRIBUTE_MALLOC
# endif
#endif

/* _GL_ATTRIBUTE_MAY_ALIAS declares that pointers to the type may point to the
   same storage as pointers to other types.  Thus this declaration disables
   strict aliasing optimization.  */
/* Applies to: types.  */
/* Oracle Studio 12.6 mishandles may_alias despite __has_attribute OK.  */
#ifndef _GL_ATTRIBUTE_MAY_ALIAS
# if _GL_HAS_ATTRIBUTE (may_alias) && !defined __SUNPRO_C
#  define _GL_ATTRIBUTE_MAY_ALIAS __attribute__ ((__may_alias__))
# else
#  define _GL_ATTRIBUTE_MAY_ALIAS
# endif
#endif

/* _GL_ATTRIBUTE_MAYBE_UNUSED declares that it is not a programming mistake if
   the entity is not used.  The compiler should not warn if the entity is not
   used.  However, 'int _GL_UNNAMED (i)' is preferable to
   '_GL_ATTRIBUTE_MAYBE_UNUSED int i' when parameter I is unused
   regardless of preprocessor macro settings.  */
/* Applies to:
     - function, variable,
     - struct, union, struct/union member,
     - enumeration, enumeration item,
     - typedef,
   in C++ also: class.  */
/* In C++ and C23, this is spelled [[__maybe_unused__]].
   GCC's syntax is __attribute__ ((__unused__)).
   clang supports both syntaxes.  Except that with clang ≥ 6, < 10, in C++ mode,
   __has_c_attribute (__maybe_unused__) yields true but the use of
   [[__maybe_unused__]] nevertheless produces a warning.  */
#ifndef _GL_ATTRIBUTE_MAYBE_UNUSED
# ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#  if defined __clang__ && defined __cplusplus
#   if !defined __apple_build_version__ && __clang_major__ >= 10
#    define _GL_ATTRIBUTE_MAYBE_UNUSED [[__maybe_unused__]]
#   endif
#  elif _GL_HAVE___HAS_C_ATTRIBUTE
#   if __has_c_attribute (__maybe_unused__)
#    define _GL_ATTRIBUTE_MAYBE_UNUSED [[__maybe_unused__]]
#   endif
#  endif
# endif
# ifndef _GL_ATTRIBUTE_MAYBE_UNUSED
#  define _GL_ATTRIBUTE_MAYBE_UNUSED _GL_ATTRIBUTE_UNUSED
# endif
#endif
/* Alternative spelling of this macro, for convenience and for
   compatibility with glibc/include/libc-symbols.h.  */
#define _GL_UNUSED _GL_ATTRIBUTE_MAYBE_UNUSED
/* Earlier spellings of this macro.  */
#define _UNUSED_PARAMETER_ _GL_ATTRIBUTE_MAYBE_UNUSED

/* _GL_ATTRIBUTE_NODISCARD declares that the caller of the function should not
   discard the return value.  The compiler may warn if the caller does not use
   the return value, unless the caller uses something like ignore_value.  */
/* Applies to: function, enumeration, class.  */
#ifndef _GL_ATTRIBUTE_NODISCARD
# ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#  if defined __clang__ && defined __cplusplus
  /* With clang up to 15.0.6 (at least), in C++ mode, [[__nodiscard__]] produces
     a warning.
     The 1000 below means a yet unknown threshold.  When clang++ version X
     starts supporting [[__nodiscard__]] without warning about it, you can
     replace the 1000 with X.  */
#   if __clang_major__ >= 1000
#    define _GL_ATTRIBUTE_NODISCARD [[__nodiscard__]]
#   endif
#  elif _GL_HAVE___HAS_C_ATTRIBUTE
#   if __has_c_attribute (__nodiscard__)
#    define _GL_ATTRIBUTE_NODISCARD [[__nodiscard__]]
#   endif
#  endif
# endif
# if !defined _GL_ATTRIBUTE_NODISCARD && _GL_HAS_ATTRIBUTE (warn_unused_result)
#  define _GL_ATTRIBUTE_NODISCARD __attribute__ ((__warn_unused_result__))
# endif
# ifndef _GL_ATTRIBUTE_NODISCARD
#  define _GL_ATTRIBUTE_NODISCARD
# endif
#endif

/* _GL_ATTRIBUTE_NOINLINE tells that the compiler should not inline the
   function.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_NOINLINE
# if _GL_HAS_ATTRIBUTE (noinline)
#  define _GL_ATTRIBUTE_NOINLINE __attribute__ ((__noinline__))
# else
#  define _GL_ATTRIBUTE_NOINLINE
# endif
#endif

/* _GL_ATTRIBUTE_NONNULL ((N1, N2,...)) declares that the arguments N1, N2,...
   must not be NULL.
   _GL_ATTRIBUTE_NONNULL () declares that all pointer arguments must not be
   null.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_NONNULL
# if _GL_HAS_ATTRIBUTE (nonnull)
#  define _GL_ATTRIBUTE_NONNULL(args) __attribute__ ((__nonnull__ args))
# else
#  define _GL_ATTRIBUTE_NONNULL(args)
# endif
#endif

/* _GL_ATTRIBUTE_NONNULL_IF_NONZERO (NP, NI) declares that the argument NP
   (a pointer) must not be NULL if the argument NI (an integer) is != 0.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_NONNULL_IF_NONZERO
# if _GL_HAS_ATTRIBUTE (nonnull_if_nonzero)
#  define _GL_ATTRIBUTE_NONNULL_IF_NONZERO(np, ni) __attribute__ ((__nonnull_if_nonzero__ (np, ni)))
# else
#  define _GL_ATTRIBUTE_NONNULL_IF_NONZERO(np, ni)
# endif
#endif

/* _GL_ATTRIBUTE_NONSTRING declares that the contents of a character array is
   not meant to be NUL-terminated.  */
/* Applies to: struct/union members and variables that are arrays of element
   type '[[un]signed] char'.  */
#ifndef _GL_ATTRIBUTE_NONSTRING
# if _GL_HAS_ATTRIBUTE (nonstring)
#  define _GL_ATTRIBUTE_NONSTRING __attribute__ ((__nonstring__))
# else
#  define _GL_ATTRIBUTE_NONSTRING
# endif
#endif

/* There is no _GL_ATTRIBUTE_NORETURN; use _Noreturn instead.  */

/* _GL_ATTRIBUTE_NOTHROW declares that the function does not throw exceptions.
 */
/* Applies to: functions.  */
/* After a function's parameter list, this attribute must come first, before
   other attributes.  */
#ifndef _GL_ATTRIBUTE_NOTHROW
# if defined __cplusplus
#  if _GL_GNUC_PREREQ (2, 8) || __clang_major__ >= 4
#   if __cplusplus >= 201103L
#    define _GL_ATTRIBUTE_NOTHROW noexcept (true)
#   else
#    define _GL_ATTRIBUTE_NOTHROW throw ()
#   endif
#  else
#   define _GL_ATTRIBUTE_NOTHROW
#  endif
# else
#  if _GL_HAS_ATTRIBUTE (nothrow)
#   define _GL_ATTRIBUTE_NOTHROW __attribute__ ((__nothrow__))
#  else
#   define _GL_ATTRIBUTE_NOTHROW
#  endif
# endif
#endif

/* _GL_ATTRIBUTE_PACKED declares:
   For struct members: The member has the smallest possible alignment.
   For struct, union, class: All members have the smallest possible alignment,
   minimizing the memory required.  */
/* Applies to: struct members, struct, union,
   in C++ also: class.  */
#ifndef _GL_ATTRIBUTE_PACKED
/* Oracle Studio 12.6 miscompiles code with __attribute__ ((__packed__)) despite
   __has_attribute OK.  */
# if _GL_HAS_ATTRIBUTE (packed) && !defined __SUNPRO_C
#  define _GL_ATTRIBUTE_PACKED __attribute__ ((__packed__))
# else
#  define _GL_ATTRIBUTE_PACKED
# endif
#endif

/* _GL_ATTRIBUTE_PURE declares:
   It is OK for a compiler to move a call, or omit a duplicate call
   and reuse a cached return value, if observable state is the same.
   It is also OK to omit a call if the return value is not used.
   This attribute is safe if the function does not change observable state,
   returns a value determined solely by its arguments's values
   together with observable state, and always returns exactly once.
   (This attribute is looser than _GL_ATTRIBUTE_CONST because the function
   can depend on observable state.
   Unlike _GL_ATTRIBUTE_REPRODUCIBLE the function must return exactly
   once and cannot change state addressed by its arguments, but the
   function can return a pointer to storage whose contents change later.)  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_PURE
# if _GL_HAS_ATTRIBUTE (pure)
#  define _GL_ATTRIBUTE_PURE __attribute__ ((__pure__))
# else
#  define _GL_ATTRIBUTE_PURE
# endif
#endif

/* _GL_ATTRIBUTE_REPRODUCIBLE declares:
   It is OK for a compiler to move a call, or omit a duplicate call
   and reuse a cached value returned either directly or indirectly via
   a pointer, if other observable state is the same;
   however, pointer arguments cannot alias.
   This attribute is safe for a function that is effectless and idempotent;
   see ISO C 23 § 6.7.13.8 for a definition of these terms.
   (This attribute is looser than _GL_ATTRIBUTE_UNSEQUENCED because
   the function need not be stateless or independent.
   Unlike _GL_ATTRIBUTE_PURE the function need not return exactly once
   and can change state addressed by its pointer arguments, but the
   function cannot return a pointer to storage whose contents change later.)
   See also <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2956.htm> and
   <https://stackoverflow.com/questions/76847905/>.  */
/* Applies to: functions, pointer to functions, function types.  */
#ifndef _GL_ATTRIBUTE_REPRODUCIBLE
/* This may be revisited when gcc and clang support [[reproducible]] or possibly
   __attribute__ ((__reproducible__)).  */
# ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#  if _GL_HAS_ATTRIBUTE (reproducible)
#   define _GL_ATTRIBUTE_REPRODUCIBLE [[reproducible]]
#  endif
# endif
# ifndef _GL_ATTRIBUTE_REPRODUCIBLE
#  define _GL_ATTRIBUTE_REPRODUCIBLE
# endif
#endif

/* _GL_ATTRIBUTE_RETURNS_NONNULL declares that the function's return value is
   a non-NULL pointer.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_RETURNS_NONNULL
# if _GL_HAS_ATTRIBUTE (returns_nonnull)
#  define _GL_ATTRIBUTE_RETURNS_NONNULL __attribute__ ((__returns_nonnull__))
# else
#  define _GL_ATTRIBUTE_RETURNS_NONNULL
# endif
#endif

/* _GL_ATTRIBUTE_SENTINEL(pos) declares that the variadic function expects a
   trailing NULL argument.
   _GL_ATTRIBUTE_SENTINEL () - The last argument is NULL (requires C99).
   _GL_ATTRIBUTE_SENTINEL ((N)) - The (N+1)st argument from the end is NULL.  */
/* Applies to: functions.  */
#ifndef _GL_ATTRIBUTE_SENTINEL
# if _GL_HAS_ATTRIBUTE (sentinel)
#  define _GL_ATTRIBUTE_SENTINEL(pos) __attribute__ ((__sentinel__ pos))
# else
#  define _GL_ATTRIBUTE_SENTINEL(pos)
# endif
#endif

/* _GL_ATTRIBUTE_UNSEQUENCED declares:
   It is OK for a compiler to move a call, or omit a duplicate call
   and reuse a cached value returned either directly or indirectly via
   a pointer, if the state addressed by its pointer arguments is the same;
   however, pointer arguments cannot alias.
   This attribute is safe for a function that is effectless, idempotent,
   stateless, and independent; see ISO C 23 § 6.7.13.8 for a definition of
   these terms.
   (This attribute is stricter than _GL_ATTRIBUTE_REPRODUCIBLE because
   the function must be stateless and independent.  Unlike
   _GL_ATTRIBUTE_CONST the function need not return exactly once, and
   can depend on state accessed via its pointer arguments or that
   happens to have the same value for all calls to the function, but
   the function cannot return a pointer to storage whose contents
   change later.)
   See also <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n2956.htm> and
   <https://stackoverflow.com/questions/76847905/>.  */
/* Applies to: functions, pointer to functions, function types.  */
#ifndef _GL_ATTRIBUTE_UNSEQUENCED
/* This may be revisited when gcc and clang support [[unsequenced]] or possibly
   __attribute__ ((__unsequenced__)).  */
# ifndef _GL_BRACKET_BEFORE_ATTRIBUTE
#  if _GL_HAS_ATTRIBUTE (unsequenced)
#   define _GL_ATTRIBUTE_UNSEQUENCED [[unsequenced]]
#  endif
# endif
# ifndef _GL_ATTRIBUTE_UNSEQUENCED
#  define _GL_ATTRIBUTE_UNSEQUENCED
# endif
#endif

/* A helper macro.  Don't use it directly.  */
#ifndef _GL_ATTRIBUTE_UNUSED
# if _GL_HAS_ATTRIBUTE (unused)
#  define _GL_ATTRIBUTE_UNUSED __attribute__ ((__unused__))
# else
#  define _GL_ATTRIBUTE_UNUSED
# endif
#endif


/* _GL_UNUSED_LABEL; declares that it is not a programming mistake if the
   immediately preceding label is not used.  The compiler should not warn
   if the label is not used.  */
/* Applies to: label (both in C and C++).  */
/* Note that g++ < 4.5 does not support the '__attribute__ ((__unused__)) ;'
   syntax.  But clang does.  */
#ifndef _GL_UNUSED_LABEL
# if !(defined __cplusplus && !_GL_GNUC_PREREQ (4, 5)) || defined __clang__
#  define _GL_UNUSED_LABEL _GL_ATTRIBUTE_UNUSED
# else
#  define _GL_UNUSED_LABEL
# endif
#endif

/* _GL_UNNAMED (ID) is the "name" of an unnamed function parameter.
   Each of the function's unnamed parameters should have a unique "name".
   The "name" cannot be used.  This ports both to C17 and earlier, which
   lack unnamed parameters, and to C++ and later C, which have them.  */
/* Applies to:
     - function parameters.  */
#ifndef _GL_UNNAMED
# if ((defined __STDC_VERSION__ ? __STDC_VERSION__ : 0) < 202311 \
      && !defined __cplusplus)
#  define _GL_UNNAMED(id) unnamed_##id _GL_ATTRIBUTE_UNUSED
# else
#  define _GL_UNNAMED(id)
# endif
#endif

/* The following attributes enable detection of thread safety problems
   and resource leaks at compile-time, by clang ≥ 15, when the warning option
   -Wthread-safety is enabled.  For usage, see
   <https://clang.llvm.org/docs/ThreadSafetyAnalysis.html>.  */
#ifndef _GL_ATTRIBUTE_CAPABILITY_TYPE
# if __clang_major__ >= 15
#  define _GL_ATTRIBUTE_CAPABILITY_TYPE(concept) \
     __attribute__ ((__capability__ (concept)))
# else
#  define _GL_ATTRIBUTE_CAPABILITY_TYPE(concept)
# endif
#endif
#ifndef _GL_ATTRIBUTE_ACQUIRE_CAPABILITY
# if __clang_major__ >= 15
#  define _GL_ATTRIBUTE_ACQUIRE_CAPABILITY(resource) \
     __attribute__ ((__acquire_capability__ (resource)))
# else
#  define _GL_ATTRIBUTE_ACQUIRE_CAPABILITY(resource)
# endif
#endif
#ifndef _GL_ATTRIBUTE_RELEASE_CAPABILITY
# if __clang_major__ >= 15
#  define _GL_ATTRIBUTE_RELEASE_CAPABILITY(resource) \
     __attribute__ ((__release_capability__ (resource)))
# else
#  define _GL_ATTRIBUTE_RELEASE_CAPABILITY(resource)
# endif
#endif


/* In C++, there is the concept of "language linkage", that encompasses
    name mangling and function calling conventions.
    The following macros start and end a block of "C" linkage.  */
#ifdef __cplusplus
# define _GL_BEGIN_C_LINKAGE extern "C" {
# define _GL_END_C_LINKAGE }
#else
# define _GL_BEGIN_C_LINKAGE
# define _GL_END_C_LINKAGE
#endif


/* Define as 'access' if you don't have the eaccess() function. */
#define eaccess access

/* Always use our fgetfilecon wrapper. */
/* #undef fgetfilecon */

/* Always use our fgetfilecon_raw wrapper. */
/* #undef fgetfilecon_raw */

/* Define to the overridden function name */
/* #undef fts_children */

/* Define to the overridden function name */
/* #undef fts_close */

/* Define to the overridden function name */
/* #undef fts_cross_check */

/* Define to the overridden function name */
/* #undef fts_open */

/* Define to the overridden function name */
/* #undef fts_read */

/* Define to the overridden function name */
/* #undef fts_set */

/* Always use our getfilecon wrapper. */
/* #undef getfilecon */

/* Always use our getfilecon_raw wrapper. */
/* #undef getfilecon_raw */

/* Define as 'int' if <sys/types.h> doesn't define. */
/* #undef gid_t */

/* A replacement for va_copy, if needed.  */
#define gl_va_copy(a,b) ((a) = (b))

/* Define to '__inline__' or '__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Type of file serial numbers, also known as inode numbers. */
/* #undef ino_t */

/* Define to long or long long if <stdint.h> and <inttypes.h> don't define. */
/* #undef intmax_t */

/* Work around a bug in Apple GCC 4.0.1 build 5465: In C99 mode, it supports
   the ISO C 99 semantics of 'extern inline' (unlike the GNU C semantics of
   earlier versions), but does not display it by setting __GNUC_STDC_INLINE__.
   __APPLE__ && __MACH__ test for Mac OS X.
   __APPLE_CC__ tests for the Apple compiler and its version.
   __STDC_VERSION__ tests for the C99 mode.  */
#if defined __APPLE__ && defined __MACH__ && __APPLE_CC__ >= 5465 && !defined __cplusplus && __STDC_VERSION__ >= 199901L && !defined __GNUC_STDC_INLINE__
# define __GNUC_STDC_INLINE__ 1
#endif

/* Always use our lgetfilecon wrapper. */
/* #undef lgetfilecon */

/* Always use our lgetfilecon_raw wrapper. */
/* #undef lgetfilecon_raw */

/* Define to 1 if the compiler is checking for lint. */
/* #undef lint */

/* Type of major device numbers. */
#define major_t unsigned int

/* Define to a type if <wchar.h> does not define. */
/* #undef mbstate_t */

/* _GL_CMP (n1, n2) performs a three-valued comparison on n1 vs. n2, where
   n1 and n2 are expressions without side effects, that evaluate to real
   numbers (excluding NaN).
   It returns
     1  if n1 > n2
     0  if n1 == n2
     -1 if n1 < n2
   The naïve code   (n1 > n2 ? 1 : n1 < n2 ? -1 : 0)  produces a conditional
   jump with nearly all GCC versions up to GCC 10.
   This variant     (n1 < n2 ? -1 : n1 > n2)  produces a conditional jump with
   many GCC versions up to GCC 9.
   The better code  (n1 > n2) - (n1 < n2)  from Hacker's Delight § 2-9
   avoids conditional jumps in all GCC versions >= 3.4.  */
#define _GL_CMP(n1, n2) (((n1) > (n2)) - ((n1) < (n2)))


/* Type of minor device numbers. */
#define minor_t unsigned int

/* Define to the real name of the mktime_internal function. */
/* #undef mktime_internal */

/* Define to 'int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to the type of st_nlink in struct stat, or a supertype. */
/* #undef nlink_t */

/* Define to 'long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define as a signed integer type capable of holding a process identifier. */
/* #undef pid_t */

/* Define as the type of the result of subtracting two pointers, if the system
   doesn't define it. */
/* #undef ptrdiff_t */

/* Define to rpl_re_comp if the replacement should be used. */
#define re_comp rpl_re_comp

/* Define to rpl_re_compile_fastmap if the replacement should be used. */
#define re_compile_fastmap rpl_re_compile_fastmap

/* Define to rpl_re_compile_pattern if the replacement should be used. */
#define re_compile_pattern rpl_re_compile_pattern

/* Define to rpl_re_exec if the replacement should be used. */
#define re_exec rpl_re_exec

/* Define to rpl_re_match if the replacement should be used. */
#define re_match rpl_re_match

/* Define to rpl_re_match_2 if the replacement should be used. */
#define re_match_2 rpl_re_match_2

/* Define to rpl_re_search if the replacement should be used. */
#define re_search rpl_re_search

/* Define to rpl_re_search_2 if the replacement should be used. */
#define re_search_2 rpl_re_search_2

/* Define to rpl_re_set_registers if the replacement should be used. */
#define re_set_registers rpl_re_set_registers

/* Define to rpl_re_set_syntax if the replacement should be used. */
#define re_set_syntax rpl_re_set_syntax

/* Define to rpl_re_syntax_options if the replacement should be used. */
#define re_syntax_options rpl_re_syntax_options

/* Define to rpl_regcomp if the replacement should be used. */
#define regcomp rpl_regcomp

/* Define to rpl_regerror if the replacement should be used. */
#define regerror rpl_regerror

/* Define to rpl_regexec if the replacement should be used. */
#define regexec rpl_regexec

/* Define to rpl_regfree if the replacement should be used. */
#define regfree rpl_regfree

/* Define to the equivalent of the C99 'restrict' keyword, or to
   nothing if this is not supported.  Do not define if restrict is
   supported only directly.  */
#define restrict __restrict__
/* Work around a bug in older versions of Sun C++, which did not
   #define __restrict__ or support _Restrict or __restrict__
   even though the corresponding Sun C compiler ended up with
   "#define restrict _Restrict" or "#define restrict __restrict__"
   in the previous line.  This workaround can be removed once
   we assume Oracle Developer Studio 12.5 (2016) or later.  */
#if defined __SUNPRO_CC && !defined __RESTRICT && !defined __restrict__
# define _Restrict
# define __restrict__
#endif

/* Define as 'unsigned int' if <stddef.h> doesn't define. */
/* #undef size_t */

/* type to use in place of socklen_t if not defined */
/* #undef socklen_t */

/* Define as a signed type of the same size as size_t. */
/* #undef ssize_t */

/* Define as 'int' if <sys/types.h> doesn't define. */
/* #undef uid_t */


/* This definition is a duplicate of the one in unitypes.h.
   It is here so that we can cope with an older version of unitypes.h
   that does not contain this definition and that is pre-installed among
   the public header files.  */
# if defined __restrict \
     || 2 < __GNUC__ + (95 <= __GNUC_MINOR__) \
     || __clang_major__ >= 3
#  define _UC_RESTRICT __restrict
# elif 199901L <= __STDC_VERSION__ || defined restrict
#  define _UC_RESTRICT restrict
# else
#  define _UC_RESTRICT
# endif


/* Define to an unsigned 32-bit type if <sys/types.h> lacks this type. */
#define useconds_t unsigned int

/* Define as a macro for copying va_list variables. */
/* #undef va_copy */

/* Define as 'fork' if 'vfork' does not work. */
#define vfork fork

/* Define to empty if the keyword 'volatile' does not work. Warning: valid
   code using 'volatile' can become incorrect without. Disable with care. */
/* #undef volatile */

#if !defined HAVE_C_ALIGNASOF \
    && !(defined __cplusplus && 201103 <= __cplusplus) \
    && !defined alignof
# if defined HAVE_STDALIGN_H
#  include <stdalign.h>
# endif

/* ISO C23 alignas and alignof for platforms that lack it.

   References:
   ISO C23 (latest free draft
   <https://www.open-std.org/jtc1/sc22/wg14/www/docs/n3220.pdf>)
   sections 6.2.8, 6.7.6.
   C++11 (latest free draft
   <https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2011/n3242.pdf>)
   section 18.10. */

/* alignof (TYPE), also known as _Alignof (TYPE), yields the alignment
   requirement of a structure member (i.e., slot or field) that is of
   type TYPE, as an integer constant expression.

   This differs from GCC's and clang's __alignof__ operator, which can
   yield a better-performing alignment for an object of that type.  For
   example, on x86 with GCC and on Linux/x86 with clang,
   __alignof__ (double) and __alignof__ (long long) are 8, whereas
   alignof (double) and alignof (long long) are 4 unless the option
   '-malign-double' is used.

   The result cannot be used as a value for an 'enum' constant, if you
   want to be portable to HP-UX 10.20 cc and AIX 3.2.5 xlc.  */

/* GCC releases before GCC 4.9 had a bug in _Alignof.  See GCC bug 52023
   <https://gcc.gnu.org/PR52023>.
   clang versions < 8.0.0 have the same bug.
   IBM XL C V16.1.0 cc (non-clang) has the same bug.  */
#  if (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112 \
       || (defined __GNUC__ && __GNUC__ < 4 + (__GNUC_MINOR__ < 9) \
           && !defined __clang__) \
       || (defined __clang__ && __clang_major__ < 8) \
       || defined __xlC__)
#   undef/**/_Alignof
#   ifdef __cplusplus
#    if (201103 <= __cplusplus || defined _MSC_VER)
#     define _Alignof(type) alignof (type)
#    else
      template <class __t> struct __alignof_helper { char __a; __t __b; };
#     if (defined __GNUC__ && 4 <= __GNUC__) || defined __clang__
#      define _Alignof(type) __builtin_offsetof (__alignof_helper<type>, __b)
#     else
#      define _Alignof(type) offsetof (__alignof_helper<type>, __b)
#     endif
#     define _GL_STDALIGN_NEEDS_STDDEF 1
#    endif
#   else
#    if (defined __GNUC__ && 4 <= __GNUC__) || defined __clang__
#     define _Alignof(type) __builtin_offsetof (struct { char __a; type __b; }, __b)
#    else
#     define _Alignof(type) offsetof (struct { char __a; type __b; }, __b)
#     define _GL_STDALIGN_NEEDS_STDDEF 1
#    endif
#   endif
#  endif
#  if ! (defined __cplusplus && (201103 <= __cplusplus || defined _MSC_VER))
#   undef/**/alignof
#   define alignof _Alignof
#  endif

/* alignas (A), also known as _Alignas (A), aligns a variable or type
   to the alignment A, where A is an integer constant expression.  For
   example:

      int alignas (8) foo;
      struct s { int a; int alignas (8) bar; };

   aligns the address of FOO and the offset of BAR to be multiples of 8.

   A should be a power of two that is at least the type's alignment
   and at most the implementation's alignment limit.  This limit is
   2**28 on typical GNUish hosts, and 2**13 on MSVC.  To be portable
   to MSVC through at least version 10.0, A should be an integer
   constant, as MSVC does not support expressions such as 1 << 3.
   To be portable to Sun C 5.11, do not align auto variables to
   anything stricter than their default alignment.

   The following C23 requirements are not supported here:

     - If A is zero, alignas has no effect.
     - alignas can be used multiple times; the strictest one wins.
     - alignas (TYPE) is equivalent to alignas (alignof (TYPE)).

   */
# if !defined __STDC_VERSION__ || __STDC_VERSION__ < 201112
#  if defined __cplusplus && (201103 <= __cplusplus || defined _MSC_VER)
#   define _Alignas(a) alignas (a)
#  elif (!defined __attribute__ \
         && ((defined __APPLE__ && defined __MACH__ \
              ? 4 < __GNUC__ + (1 <= __GNUC_MINOR__) \
              : __GNUC__ && !defined __ibmxl__) \
             || (4 <= __clang_major__) \
             || (__ia64 && (61200 <= __HP_cc || 61200 <= __HP_aCC)) \
             || __ICC || 0x590 <= __SUNPRO_C || 0x0600 <= __xlC__))
#   define _Alignas(a) __attribute__ ((__aligned__ (a)))
#  elif 1300 <= _MSC_VER
#   define _Alignas(a) __declspec (align (a))
#  endif
# endif
# if !defined HAVE_STDALIGN_H
#  if ((defined _Alignas \
        && !(defined __cplusplus \
             && (201103 <= __cplusplus || defined _MSC_VER))) \
       || (defined __STDC_VERSION__ && 201112 <= __STDC_VERSION__ \
           && !defined __xlC__))
#   define alignas _Alignas
#  endif
# endif

# if defined _GL_STDALIGN_NEEDS_STDDEF
#  include <stddef.h>
# endif
#endif

#if !(defined __cplusplus \
      ? 1 \
      : (defined __clang__ \
         ? __STDC_VERSION__ >= 202000L && __clang_major__ >= 15 \
         : (defined __GNUC__ \
            ? __STDC_VERSION__ >= 202000L && __GNUC__ >= 13 \
            : defined HAVE_C_BOOL)))
# if !defined __cplusplus && !defined __bool_true_false_are_defined
#  if HAVE_STDBOOL_H
#   include <stdbool.h>
#  else
#   if defined __SUNPRO_C
#    error "<stdbool.h> is not usable with this configuration. To make it usable, add -D_STDC_C99= to $CC."
#   else
#    error "<stdbool.h> does not exist on this platform. Use gnulib module 'stdbool-c99' instead of gnulib module 'stdbool'."
#   endif
#  endif
# endif
# if !true
#  define true (!false)
# endif
#endif

#if defined __cplusplus && HAVE_CXX_NULLPTR < 0
# include <stddef.h>
# undef/**/nullptr
#endif
#ifndef nullptr
# if !defined __cplusplus && !defined HAVE_C_NULLPTR
#  define nullptr ((void *) 0)
# elif defined __cplusplus && HAVE_CXX_NULLPTR <= 0
#  if 3 <= __GNUG__
#   define nullptr __null
#  else
#   define nullptr 0L
#  endif
# endif
#endif

#if (!(defined __clang__ \
       ? (defined __cplusplus \
          ? __cplusplus >= 201703L \
          : __STDC_VERSION__ >= 202000L && __clang_major__ >= 16 \
            && !defined __sun) \
       : (defined __GNUC__ \
          ? (defined __cplusplus \
             ? __cplusplus >= 201103L && __GNUG__ >= 6 \
             : __STDC_VERSION__ >= 202000L && __GNUC__ >= 13 \
               && !defined __sun) \
          : defined HAVE_C_STATIC_ASSERT)) \
     && !defined assert \
     && (!defined __cplusplus \
         || (__cpp_static_assert < 201411 \
             && __GNUG__ < 6 && __clang_major__ < 6)))
 #include <assert.h>
 #undef/**/assert
 /* Solaris 11.4 <assert.h> defines static_assert as a macro with 2 arguments.
    We need it also to be invocable with a single argument.
    Haiku 2022 <assert.h> does not define static_assert at all.  */
 #if (__STDC_VERSION__ - 0 >= 201112L) && !defined __cplusplus
  #undef/**/static_assert
  #define static_assert _Static_assert
 #endif
#endif

#if defined __GNUC__ && __GNUC__ >= 15 && !defined __clang__
# pragma GCC diagnostic pop
#endif



/* The text domainname for Gnulib messages.  Ordinarily this is "gnulib",
   but packages that do their own translations of Gnulib can use something
   different by defining GNULIB_TEXT_DOMAIN in their config.h file.  */
#ifndef GNULIB_TEXT_DOMAIN
# define GNULIB_TEXT_DOMAIN/**/"gnulib"
#endif

