/* config.h.  Generated from config.in by configure.  */
/* config.in.  Generated from configure.ac by autoheader.  */

/* Check that config.h is #included before system headers
    (this works only for glibc, but that should be enough).  */
#if defined(__GLIBC__) && !defined(__FreeBSD_kernel__) && !defined(__CONFIG_H__)
#  error config.h must be #included before system headers
#endif
#define __CONFIG_H__ 1

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Name of host specific core header file to include in elf.c. */
/* #undef CORE_HEADER */

/* Define to 1 if you want to enable -z separate-code in ELF linker by
   default. */
#define DEFAULT_LD_Z_SEPARATE_CODE 0

/* Define if you want run-time sanity checks. */
#define ENABLE_CHECKING 1

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
/* #undef ENABLE_NLS */

/* Define to 1 if you have the Mac OS X function
   CFLocaleCopyPreferredLanguages in the CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYPREFERREDLANGUAGES */

/* Define to 1 if you have the Mac OS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
/* #undef HAVE_DCGETTEXT */

/* Define to 1 if you have the declaration of `asprintf', and to 0 if you
   don't. */
#define HAVE_DECL_ASPRINTF 1

/* Define to 1 if you have the declaration of `basename', and to 0 if you
   don't. */
#define HAVE_DECL_BASENAME 1

/* Define to 1 if you have the declaration of `ffs', and to 0 if you don't. */
#define HAVE_DECL_FFS 0

/* Define to 1 if you have the declaration of `fopen64', and to 0 if you
   don't. */
#define HAVE_DECL_FOPEN64 0

/* Define to 1 if you have the declaration of `fseeko', and to 0 if you don't.
   */
#define HAVE_DECL_FSEEKO 1

/* Define to 1 if you have the declaration of `fseeko64', and to 0 if you
   don't. */
#define HAVE_DECL_FSEEKO64 1

/* Define to 1 if you have the declaration of `ftello', and to 0 if you don't.
   */
#define HAVE_DECL_FTELLO 1

/* Define to 1 if you have the declaration of `ftello64', and to 0 if you
   don't. */
#define HAVE_DECL_FTELLO64 1

/* Define to 1 if you have the declaration of `stpcpy', and to 0 if you don't.
   */
#define HAVE_DECL_STPCPY 0

/* Define to 1 if you have the declaration of `strnlen', and to 0 if you
   don't. */
#define HAVE_DECL_STRNLEN 1

/* Define to 1 if you have the declaration of `strtoull', and to 0 if you
   don't. */
#define HAVE_DECL_STRTOULL 1

/* Define to 1 if you have the declaration of `vasprintf', and to 0 if you
   don't. */
#define HAVE_DECL_VASPRINTF 1

/* Define to 1 if you have the declaration of `___lc_codepage_func', and to 0
   if you don't. */
#define HAVE_DECL____LC_CODEPAGE_FUNC 0

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `fcntl' function. */
#define HAVE_FCNTL 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `fdopen' function. */
#define HAVE_FDOPEN 1

/* Define to 1 if you have the `fileno' function. */
#define HAVE_FILENO 1

/* Define to 1 if you have the `fls' function. */
/* #undef HAVE_FLS */

/* Define to 1 if you have the `fopen64' function. */
/* #undef HAVE_FOPEN64 */

/* Define to 1 if you have the `fseeko' function. */
#define HAVE_FSEEKO 1

/* Define to 1 if you have the `fseeko64' function. */
#define HAVE_FSEEKO64 1

/* Define to 1 if you have the `ftello' function. */
#define HAVE_FTELLO 1

/* Define to 1 if you have the `ftello64' function. */
#define HAVE_FTELLO64 1

/* Define to 1 if you have the `getgid' function. */
#define HAVE_GETGID 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getrlimit' function. */
#define HAVE_GETRLIMIT 1

/* Define if the GNU gettext() function is already present or preinstalled. */
/* #undef HAVE_GETTEXT */

/* Define to 1 if you have the `getuid' function. */
#define HAVE_GETUID 1

/* Define if your compiler supports hidden visibility. */
/* #undef HAVE_HIDDEN */

/* Define if you have the iconv() function and it works. */
#define HAVE_ICONV 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if <sys/procfs.h> has lwpstatus_t. */
/* #undef HAVE_LWPSTATUS_T */

/* Define if <sys/procfs.h> has lwpstatus_t.pr_context. */
/* #undef HAVE_LWPSTATUS_T_PR_CONTEXT */

/* Define if <sys/procfs.h> has lwpstatus_t.pr_fpreg. */
/* #undef HAVE_LWPSTATUS_T_PR_FPREG */

/* Define if <sys/procfs.h> has lwpstatus_t.pr_reg. */
/* #undef HAVE_LWPSTATUS_T_PR_REG */

/* Define if <sys/procfs.h> has lwpxstatus_t. */
/* #undef HAVE_LWPXSTATUS_T */

/* Define to 1 if you have the `madvise' function. */
/* #undef HAVE_MADVISE */

/* Define to 1 if you have the <memory.h> header file. */
/* #undef HAVE_MEMORY_H */

/* Define to 1 if you have a working `mmap' system call. */
#define HAVE_MMAP 1

/* Define to 1 if you have the `mprotect' function. */
#define HAVE_MPROTECT 1

/* Define if <sys/procfs.h> has prpsinfo32_t. */
/* #undef HAVE_PRPSINFO32_T */

/* Define if <sys/procfs.h> has prpsinfo32_t.pr_pid. */
/* #undef HAVE_PRPSINFO32_T_PR_PID */

/* Define if <sys/procfs.h> has prpsinfo_t. */
/* #undef HAVE_PRPSINFO_T */

/* Define if <sys/procfs.h> has prpsinfo_t.pr_pid. */
/* #undef HAVE_PRPSINFO_T_PR_PID */

/* Define if <sys/procfs.h> has prstatus32_t. */
/* #undef HAVE_PRSTATUS32_T */

/* Define if <sys/procfs.h> has prstatus32_t.pr_who. */
/* #undef HAVE_PRSTATUS32_T_PR_WHO */

/* Define if <sys/procfs.h> has prstatus_t. */
/* #undef HAVE_PRSTATUS_T */

/* Define if <sys/procfs.h> has prstatus_t.pr_who. */
/* #undef HAVE_PRSTATUS_T_PR_WHO */

/* Define if <sys/procfs.h> has psinfo32_t. */
/* #undef HAVE_PSINFO32_T */

/* Define if <sys/procfs.h> has psinfo32_t.pr_pid. */
/* #undef HAVE_PSINFO32_T_PR_PID */

/* Define if <sys/procfs.h> has psinfo_t. */
/* #undef HAVE_PSINFO_T */

/* Define if <sys/procfs.h> has psinfo_t.pr_pid. */
/* #undef HAVE_PSINFO_T_PR_PID */

/* Define if <sys/procfs.h> has pstatus32_t. */
/* #undef HAVE_PSTATUS32_T */

/* Define if <sys/procfs.h> has pstatus_t. */
/* #undef HAVE_PSTATUS_T */

/* Define if <sys/procfs.h> has pxstatus_t. */
/* #undef HAVE_PXSTATUS_T */

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define if struct core_dumpx has member c_impl */
/* #undef HAVE_ST_C_IMPL */

/* Define to 1 if you have the `sysconf' function. */
#define HAVE_SYSCONF 1

/* Define to 1 if you have the <sys/file.h> header file. */
/* #undef HAVE_SYS_FILE_H */

/* Define to 1 if you have the <sys/param.h> header file. */
/* #undef HAVE_SYS_PARAM_H */

/* Define to 1 if you have the <sys/procfs.h> header file. */
/* #undef HAVE_SYS_PROCFS_H */

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define if <sys/procfs.h> has win32_pstatus_t. */
/* #undef HAVE_WIN32_PSTATUS_T */

/* Define to 1 if you have the <windows.h> header file. */
/* #undef HAVE_WINDOWS_H */

/* Define to 1 if zstd is enabled. */
#define HAVE_ZSTD 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* The size of `int', as computed by sizeof. */
#define SIZEOF_INT 4

/* The size of `long', as computed by sizeof. */
#define SIZEOF_LONG 4

/* The size of `long long', as computed by sizeof. */
#define SIZEOF_LONG_LONG 8

/* The size of `off_t', as computed by sizeof. */
#define SIZEOF_OFF_T 8

/* The size of `void *', as computed by sizeof. */
#define SIZEOF_VOID_P 8

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* If the compiler supports a TLS storage class, define it to that here */
#define TLS _Thread_local

/* Name of host specific header file to include in trad-core.c. */
/* #undef TRAD_HEADER */

/* Define if 64-bit archives should always be used. */
/* #undef USE_64_BIT_ARCHIVE */

/* Use b modifier when opening binary files? */
/* #undef USE_BINARY_FOPEN */

/* Define if we should use leading underscore on 64 bit mingw targets */
/* #undef USE_MINGW64_LEADING_UNDERSCORES */

/* Use mmap if it's available? */
#define USE_MMAP 1

/* Define if we should default to creating read-only plt entries */
#define USE_SECUREPLT 1

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif

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

/* Enable large inode numbers on Mac OS X 10.5.  */
#ifndef _DARWIN_USE_64_BIT_INODE
# define _DARWIN_USE_64_BIT_INODE 1
#endif

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Use structured /proc on Solaris. */
#define _STRUCTURED_PROC 1

/* Should ar and ranlib use -D behavior by default? */
#define DEFAULT_AR_DETERMINISTIC 0

/* Have objdump use colors in its disassembly by default */
#define DEFAULT_FOR_COLORED_DISASSEMBLY 0

/* Have readelf and objdump follow debug links by default */
#define DEFAULT_FOR_FOLLOW_LINKS 1

/* Have nm use F and f for global and local ifunc symbols */
#define DEFAULT_F_FOR_IFUNC_SYMBOLS 0

/* Should strings use -a behavior by default? */
#define DEFAULT_STRINGS_ALL 1

/* Handle .ctf type-info sections */
/* #undef ENABLE_LIBCTF */

/* Suffix used for executables, if any. */
#define EXECUTABLE_SUFFIX ".exe"

/* Define to 1 if you have the declaration of `environ', and to 0 if you
   don't. */
#define HAVE_DECL_ENVIRON 1

/* Define to 1 if you have the declaration of `getc_unlocked', and to 0 if you
   don't. */
#define HAVE_DECL_GETC_UNLOCKED 1

/* Is the prototype for getopt in <unistd.h> in the expected format? */
#define HAVE_DECL_GETOPT 1

/* Does the platform use an executable suffix? */
#define HAVE_EXECUTABLE_SUFFIX 1

/* Define to 1 if you have the `getc_unlocked' function. */
#define HAVE_GETC_UNLOCKED 1

/* Does <utime.h> define struct utimbuf? */
#define HAVE_GOOD_UTIME_H 1

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define to 1 if debuginfod is enabled. */
/* #undef HAVE_LIBDEBUGINFOD */

/* Define to 1 if debuginfod section downloading is supported. */
/* #undef HAVE_LIBDEBUGINFOD_FIND_SECTION */

/* Define if mbstate_t exists in wchar.h. */
#define HAVE_MBSTATE_T 1

/* Define to 1 if you have the `mkdtemp' function. */
#define HAVE_MKDTEMP 1

/* Define to 1 if you have the `mkstemp' function. */
#define HAVE_MKSTEMP 1

/* Define to 1 if msgpack is available. */
/* #undef HAVE_MSGPACK */

/* Define to 1 if `st_atimensec' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_ATIMENSEC */

/* Define to 1 if `st_atimespec.tv_nsec' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_ATIMESPEC_TV_NSEC */

/* Define to 1 if `st_atim.st__tim.tv_nsec' is a member of `struct stat'. */
/* #undef HAVE_STRUCT_STAT_ST_ATIM_ST__TIM_TV_NSEC */

/* Define to 1 if `st_atim.tv_nsec' is a member of `struct stat'. */
#define HAVE_STRUCT_STAT_ST_ATIM_TV_NSEC 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the `utimensat' function. */
#define HAVE_UTIMENSAT 1

/* Define to 1 if you have the `utimes' function. */
#define HAVE_UTIMES 1

/* Define as const if the declaration of iconv() needs const. */
#define ICONV_CONST

/* Configured target name. */
#define TARGET "x86_64-pc-moxnox"

/* Define to 1 if user symbol names have a leading underscore, 0 if not. */
#define TARGET_PREPENDS_UNDERSCORE 0

/* Define to 1 if the type of the st_atim member of a struct stat is struct
   timespec. */
#define TYPEOF_STRUCT_STAT_ST_ATIM_IS_STRUCT_TIMESPEC 1

/* Define to 1 if `lex' declares `yytext' as a `char *' by default, not a
   `char[]'. */
#define YYTEXT_POINTER 1

/* Define if using AIX 5.2 value for C_WEAKEXT. */
/* #undef AIX_WEAK_SUPPORT */

/* assert broken? */
/* #undef BROKEN_ASSERT */

/* Compiling cross-assembler? */
/* #undef CROSS_COMPILE */

/* Default architecture. */
#define DEFAULT_ARCH "x86_64"

/* Default compression algorithm for --enable-compressed-debug-sections. */
#define DEFAULT_COMPRESSED_DEBUG_ALGORITHM COMPRESS_DEBUG_GABI_ZLIB

/* Default CRIS architecture. */
/* #undef DEFAULT_CRIS_ARCH */

/* Define if you want compressed debug sections by default. */
/* #undef DEFAULT_FLAG_COMPRESS_DEBUG */

/* Define to 1 if you want to generate GNU Build attribute notes by default,
   if none are contained in the input. */
#define DEFAULT_GENERATE_BUILD_NOTES 0

/* Define to 1 if you want to generate ELF common symbols with the STT_COMMON
   type by default. */
#define DEFAULT_GENERATE_ELF_STT_COMMON 0

/* Define to 1 if you want to fix Loongson3 LLSC Errata by default. */
#define DEFAULT_MIPS_FIX_LOONGSON3_LLSC 0

/* Define default value for RISC-V -march. */
/* #undef DEFAULT_RISCV_ARCH_WITH_EXT */

/* Define to 1 if you want to generate RISC-V arch attribute by default. */
#define DEFAULT_RISCV_ATTR 1

/* Define default value for RISC-V -misa-spec. */
/* #undef DEFAULT_RISCV_ISA_SPEC */

/* Define default value for RISC-V -mpriv-spec */
/* #undef DEFAULT_RISCV_PRIV_SPEC */

/* Define to 1 if you want to emit SFrames by default. */
#define DEFAULT_SFRAME 0

/* Define to 1 if you want to check x86 TLS relocation by default. */
#define DEFAULT_X86_TLS_CHECK 1

/* Define to 1 if you want to generate GNU x86 used ISA and feature properties
   by default. */
#define DEFAULT_X86_USED_NOTE 0

/* Supported emulations. */
#define EMULATIONS

/* Define if you want memory to be freed before exit. */
#define ENABLE_LEAK_CHECK BFD_ASAN

/* Define to 1 if you have the declaration of `mempcpy', and to 0 if you
   don't. */
#define HAVE_DECL_MEMPCPY 0

/* Define to 1 if you have the `strsignal' function. */
#define HAVE_STRSIGNAL 1

/* Define if <sys/stat.h> has struct stat.st_mtim.tv_nsec */
#define HAVE_ST_MTIM_TV_NSEC 1

/* Define if <sys/stat.h> has struct stat.st_mtim.tv_sec */
#define HAVE_ST_MTIM_TV_SEC 1

/* Define if <time.h> has struct tm.tm_gmtoff. */
/* #undef HAVE_TM_GMTOFF */

/* Using i386 COFF? */
#define I386COFF 1

/* Default CPU for MIPS targets. */
/* #undef MIPS_CPU_STRING_DEFAULT */

/* Generate 64-bit code by default on MIPS targets. */
/* #undef MIPS_DEFAULT_64BIT */

/* Choose a default ABI for MIPS targets. */
/* #undef MIPS_DEFAULT_ABI */

/* Define value for nds32_arch_name */
/* #undef NDS32_DEFAULT_ARCH_NAME */

/* Define default value for nds32_audio_ext */
/* #undef NDS32_DEFAULT_AUDIO_EXT */

/* Define default value for nds32_dsp_ext */
/* #undef NDS32_DEFAULT_DSP_EXT */

/* Define default value for nds32_dx_regs */
/* #undef NDS32_DEFAULT_DX_REGS */

/* Define default value for nds32_perf_ext */
/* #undef NDS32_DEFAULT_PERF_EXT */

/* Define default value for nds32_perf_ext2 */
/* #undef NDS32_DEFAULT_PERF_EXT2 */

/* Define default value for nds32_string_ext */
/* #undef NDS32_DEFAULT_STRING_EXT */

/* Define default value for nds32_zol_ext */
/* #undef NDS32_DEFAULT_ZOL_EXT */

/* Define default value for nds32_linux_toolchain */
/* #undef NDS32_LINUX_TOOLCHAIN */

/* Define if environ is not declared in system header files. */
/* #undef NEED_DECLARATION_ENVIRON */

/* Define if ffs is not declared in system header files. */
#define NEED_DECLARATION_FFS 1

/* a.out support? */
/* #undef OBJ_MAYBE_AOUT */

/* COFF support? */
/* #undef OBJ_MAYBE_COFF */

/* ECOFF support? */
/* #undef OBJ_MAYBE_ECOFF */

/* ELF support? */
/* #undef OBJ_MAYBE_ELF */

/* generic support? */
/* #undef OBJ_MAYBE_GENERIC */

/* SOM support? */
/* #undef OBJ_MAYBE_SOM */

/* Using strict COFF? */
/* #undef STRICTCOFF */

/* Target alias. */
#define TARGET_ALIAS "x86_64-pc-moxnox"

/* Define as 1 if big endian. */
/* #undef TARGET_BYTES_BIG_ENDIAN */

/* Canonical target. */
#define TARGET_CANONICAL "x86_64-pc-moxnox"

/* Target CPU. */
#define TARGET_CPU "x86_64"

/* Target OS. */
#define TARGET_OS "moxnox"

/* Target vendor. */
#define TARGET_VENDOR "pc"

/* Target specific CPU. */
/* #undef TARGET_WITH_CPU */

/* Allow use of EF_MIPS_ABI_O32 on MIPS targets. */
/* #undef USE_EF_MIPS_ABI_O32 */

/* Use emulation support? */
/* #undef USE_EMULATIONS */

/* Using cgen code? */
/* #undef USING_CGEN */

/* Define to 1 if you want to emit gnu hash in the ELF linker by default. */
#define DEFAULT_EMIT_GNU_HASH 0

/* Define to 1 if you want to emit sysv hash in the ELF linker by default. */
#define DEFAULT_EMIT_SYSV_HASH 1

/* Define to 1 if you want to turn executable stack warnings into errors by
   default. */
#define DEFAULT_LD_ERROR_EXECSTACK 0

/* Define to 1 if you want to turn executable segment warnings into errors by
   default. */
#define DEFAULT_LD_ERROR_RWX_SEGMENTS 0

/* Define to 0 if you want to disable the generation of an executable stack
   when a .note-GNU-stack section is missing. */
#define DEFAULT_LD_EXECSTACK 1

/* Define to 1 if you want to enable --gnu2-tls-tag in ELF i386/x86-64 linker
   by default. */
#define DEFAULT_LD_GNU2_TLS_TAG 2

/* Define to 1 if you want to enable --gnu-tls-tag in ELF i386 linker by
   default. */
#define DEFAULT_LD_GNU_TLS_TAG 2

/* Define to 1 if you want to enable --rosegment in the ELF linker by default.
   */
#define DEFAULT_LD_ROSEGMENT 0

/* The default method for DT_TEXTREL check in ELF linker. */
#define DEFAULT_LD_TEXTREL_CHECK textrel_check_none

/* Define to 1 if DT_TEXTREL check is warning in ELF linker by default. */
#define DEFAULT_LD_TEXTREL_CHECK_WARNING 0

/* Define to 1 if you want to enable --warn-execstack in ELF linker by
   default. */
#define DEFAULT_LD_WARN_EXECSTACK 2

/* Define to 0 if you want to disable --warn-rwx-segments in ELF linker by
   default. */
#define DEFAULT_LD_WARN_RWX_SEGMENTS 1

/* Define to 1 if you want to enable -z mark-plt in ELF x86-64 linker by
   default. */
#define DEFAULT_LD_Z_MARK_PLT 0

/* Define to 1 if you want to enable -z memory_seal in ELF linker by default.
   */
#define DEFAULT_LD_Z_MEMORY_SEAL 0

/* Define to 1 if you want to enable -z relro in ELF linker by default. */
#define DEFAULT_LD_Z_RELRO 0

/* Define to 1 if you want to set DT_RUNPATH instead of DT_RPATH by default.
   */
#define DEFAULT_NEW_DTAGS 0

/* Additional extension a shared object might have. */
/* #undef EXTRA_SHLIB_EXTENSION */

/* Define to choose default GOT handling scheme */
#define GOT_HANDLING_DEFAULT GOT_HANDLING_TARGET_DEFAULT

/* Define to 1 if you have the `close' function. */
#define HAVE_CLOSE 1

/* Define to 1 if you have the <elf-hints.h> header file. */
/* #undef HAVE_ELF_HINTS_H */

// FIXME - we do, but not the non-POSIX ru_maxrss that ld uses
/* Define to 1 if you have the `getrusage' function. */
/* #undef HAVE_GETRUSAGE */

/* Define to 1 if you have the `glob' function. */
#define HAVE_GLOB 1

/* Define .init_array/.fini_array sections are available and working. */
#define HAVE_INITFINI_ARRAY 1

/* The jansson library is to be used */
/* #undef HAVE_JANSSON */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `lseek' function. */
#define HAVE_LSEEK 1

/* Define to 1 if you have the `mallinfo' function. */
/* #undef HAVE_MALLINFO */

/* Define to 1 if you have the `mallinfo2' function. */
/* #undef HAVE_MALLINFO2 */

/* Define to 1 if you have the `open' function. */
#define HAVE_OPEN 1

/* Define to 1 if you have the `realpath' function. */
#define HAVE_REALPATH 1

/* Define to 1 if you have the <sys/mman.h> header file. */
#define HAVE_SYS_MMAN_H 1

/* Define to 1 if you have the `waitpid' function. */
#define HAVE_WAITPID 1

/* Define to 1 if you want to support the --error-handling-script command line
   option. */
#define SUPPORT_ERROR_HANDLING_SCRIPT 1

/* whether to use inline xxhash */
/* #undef WITH_XXHASH */

/* Enable expensive debugging of CTF deduplication type hashing */
/* #undef ENABLE_LIBCTF_HASH_DEBUGGING */

/* Whether libbfd was configured for an ELF target. */
#define HAVE_BFD_ELF 1

/* Define to 1 if you have the <byteswap.h> header file. */
/* #undef HAVE_BYTESWAP_H */

/* Define to 1 if you have the declaration of `bswap_16', and to 0 if you
   don't. */
#define HAVE_DECL_BSWAP_16 0

/* Define to 1 if you have the declaration of `bswap_32', and to 0 if you
   don't. */
#define HAVE_DECL_BSWAP_32 0

/* Define to 1 if you have the declaration of `bswap_64', and to 0 if you
   don't. */
#define HAVE_DECL_BSWAP_64 0

/* Define to 1 if you have the <endian.h> header file. */
#define HAVE_ENDIAN_H 1

/* Whether the platform has a definition of O_CLOEXEC. */
#define HAVE_O_CLOEXEC 1

/* Define to 1 if you have the `pread' function. */
#define HAVE_PREAD 1

/* Define to 1 if you have the `qsort_r' function. */
/* #undef HAVE_QSORT_R */

/* Whether a qsort_r exists with a void *arg as its last arg. */
/* #undef HAVE_QSORT_R_ARG_LAST */

/* Whether a qsort_r exists with the compar function as its last arg. */
/* #undef HAVE_QSORT_R_COMPAR_LAST */

/* Define to 1 if you have the <valgrind/valgrind.h> header file. */
/* #undef HAVE_VALGRIND_VALGRIND_H */

/* Define to one of _getb67, GETB67, getb67 for Cray-2 and Cray-YMP systems.
   This function is required for alloca.c support on those systems. */
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if you have the <alloca.h> header file. */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the `asprintf' function. */
#define HAVE_ASPRINTF 1

/* Define to 1 if you have the `atexit' function. */
#define HAVE_ATEXIT 1

/* Define to 1 if you have the `basename' function. */
#define HAVE_BASENAME 1

/* Define to 1 if you have the `bcmp' function. */
/* #undef HAVE_BCMP */

/* Define to 1 if you have the `bcopy' function. */
/* #undef HAVE_BCOPY */

/* Define to 1 if you have the `bsearch' function. */
#define HAVE_BSEARCH 1

/* Define to 1 if you have the `bzero' function. */
/* #undef HAVE_BZERO */

/* Define to 1 if you have the `calloc' function. */
#define HAVE_CALLOC 1

/* Define to 1 if you have the `canonicalize_file_name' function. */
/* #undef HAVE_CANONICALIZE_FILE_NAME */

/* Define to 1 if you have the `clock' function. */
#define HAVE_CLOCK 1

/* Define to 1 if you have the declaration of `calloc', and to 0 if you don't.
   */
#define HAVE_DECL_CALLOC 1

/* Define to 1 if you have the declaration of `getenv', and to 0 if you don't.
   */
#define HAVE_DECL_GETENV 1

/* Define to 1 if you have the declaration of `malloc', and to 0 if you don't.
   */
#define HAVE_DECL_MALLOC 1

/* Define to 1 if you have the declaration of `realloc', and to 0 if you
   don't. */
#define HAVE_DECL_REALLOC 1

/* Define to 1 if you have the declaration of `sbrk', and to 0 if you don't.
   */
#define HAVE_DECL_SBRK 0

/* Define to 1 if you have the declaration of `snprintf', and to 0 if you
   don't. */
#define HAVE_DECL_SNPRINTF 1

/* Define to 1 if you have the declaration of `strtol', and to 0 if you don't.
   */
#define HAVE_DECL_STRTOL 1

/* Define to 1 if you have the declaration of `strtoll', and to 0 if you
   don't. */
#define HAVE_DECL_STRTOLL 1

/* Define to 1 if you have the declaration of `strtoul', and to 0 if you
   don't. */
#define HAVE_DECL_STRTOUL 1

/* Define to 1 if you have the declaration of `strverscmp', and to 0 if you
   don't. */
#define HAVE_DECL_STRVERSCMP 0

/* Define to 1 if you have the declaration of `vsnprintf', and to 0 if you
   don't. */
#define HAVE_DECL_VSNPRINTF 1

/* Define to 1 if you have the `dup3' function. */
/* #undef HAVE_DUP3 */

/* Define to 1 if you have the `ffs' function. */
/* #undef HAVE_FFS */

/* Define to 1 if you have the `fork' function. */
#define HAVE_FORK 1

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the `getsysinfo' function. */
/* #undef HAVE_GETSYSINFO */

/* Define to 1 if you have the `gettimeofday' function. */
#define HAVE_GETTIMEOFDAY 1

/* Define to 1 if you have the `index' function. */
/* #undef HAVE_INDEX */

/* Define to 1 if you have the `insque' function. */
/* #undef HAVE_INSQUE */

/* Define to 1 if the system has the type `intptr_t'. */
#define HAVE_INTPTR_T 1

/* Define to 1 if you have the <libgen.h> header file. */
#define HAVE_LIBGEN_H 1

/* Define if you have the `long long' type. */
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the <machine/hal_sysinfo.h> header file. */
/* #undef HAVE_MACHINE_HAL_SYSINFO_H */

/* Define to 1 if you have the <malloc.h> header file. */
/* #undef HAVE_MALLOC_H */

/* Define to 1 if you have the `memchr' function. */
#define HAVE_MEMCHR 1

/* Define to 1 if you have the `memcmp' function. */
#define HAVE_MEMCMP 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the `memmem' function. */
#define HAVE_MEMMEM 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the `memrchr' function. */
/* #undef HAVE_MEMRCHR */

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mkstemps' function. */
/* #undef HAVE_MKSTEMPS */

/* Define to 1 if you have the `on_exit' function. */
/* #undef HAVE_ON_EXIT */

/* Define to 1 if you have the `pipe2' function. */
/* #undef HAVE_PIPE2 */

/* Define to 1 if you have the `posix_spawn' function. */
#define HAVE_POSIX_SPAWN 1

/* Define to 1 if you have the `posix_spawnp' function. */
#define HAVE_POSIX_SPAWNP 1

/* Define to 1 if you have the <process.h> header file. */
/* #undef HAVE_PROCESS_H */

/* Define to 1 if you have the `psignal' function. */
/* #undef HAVE_PSIGNAL */

/* Define to 1 if you have the `pstat_getdynamic' function. */
/* #undef HAVE_PSTAT_GETDYNAMIC */

/* Define to 1 if you have the `pstat_getstatic' function. */
/* #undef HAVE_PSTAT_GETSTATIC */

/* Define to 1 if you have the `putenv' function. */
/* #undef HAVE_PUTENV */

/* Define to 1 if you have the `random' function. */
/* #undef HAVE_RANDOM */

/* Define to 1 if you have the `rename' function. */
#define HAVE_RENAME 1

/* Define to 1 if you have the `rindex' function. */
/* #undef HAVE_RINDEX */

/* Define to 1 if you have the `sbrk' function. */
/* #undef HAVE_SBRK */

/* Define to 1 if you have the `setenv' function. */
#define HAVE_SETENV 1

/* Define to 1 if you have the `setproctitle' function. */
/* #undef HAVE_SETPROCTITLE */

/* Define to 1 if you have the `setrlimit' function. */
#define HAVE_SETRLIMIT 1

/* Define to 1 if you have the `sigsetmask' function. */
/* #undef HAVE_SIGSETMASK */

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `spawnve' function. */
/* #undef HAVE_SPAWNVE */

/* Define to 1 if you have the `spawnvpe' function. */
/* #undef HAVE_SPAWNVPE */

/* Define to 1 if you have the <spawn.h> header file. */
#define HAVE_SPAWN_H 1

/* Define to 1 if you have the <stdio_ext.h> header file. */
#define HAVE_STDIO_EXT_H 1

/* Define to 1 if you have the `stpcpy' function. */
/* #undef HAVE_STPCPY */

/* Define to 1 if you have the `stpncpy' function. */
/* #undef HAVE_STPNCPY */

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strerror' function. */
#define HAVE_STRERROR 1

/* Define to 1 if you have the `strncasecmp' function. */
#define HAVE_STRNCASECMP 1

/* Define to 1 if you have the `strndup' function. */
#define HAVE_STRNDUP 1

/* Define to 1 if you have the `strnlen' function. */
#define HAVE_STRNLEN 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtod' function. */
#define HAVE_STRTOD 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoll' function. */
#define HAVE_STRTOLL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the `strtoull' function. */
#define HAVE_STRTOULL 1

/* Define to 1 if you have the `strverscmp' function. */
/* #undef HAVE_STRVERSCMP */

/* Define to 1 if you have the `sysctl' function. */
/* #undef HAVE_SYSCTL */

/* Define to 1 if you have the `sysmp' function. */
/* #undef HAVE_SYSMP */

/* Define if you have the sys_errlist variable. */
/* #undef HAVE_SYS_ERRLIST */

/* Define if you have the sys_nerr variable. */
/* #undef HAVE_SYS_NERR */

/* Define to 1 if you have the <sys/prctl.h> header file. */
/* #undef HAVE_SYS_PRCTL_H */

/* Define to 1 if you have the <sys/pstat.h> header file. */
/* #undef HAVE_SYS_PSTAT_H */

/* Define if you have the sys_siglist variable. */
/* #undef HAVE_SYS_SIGLIST */

/* Define to 1 if you have the <sys/sysctl.h> header file. */
/* #undef HAVE_SYS_SYSCTL_H */

/* Define to 1 if you have the <sys/sysinfo.h> header file. */
/* #undef HAVE_SYS_SYSINFO_H */

/* Define to 1 if you have the <sys/sysmp.h> header file. */
/* #undef HAVE_SYS_SYSMP_H */

/* Define to 1 if you have the <sys/systemcfg.h> header file. */
/* #undef HAVE_SYS_SYSTEMCFG_H */

/* Define to 1 if you have the <sys/table.h> header file. */
/* #undef HAVE_SYS_TABLE_H */

/* Define to 1 if you have the `table' function. */
/* #undef HAVE_TABLE */

/* Define to 1 if you have the `times' function. */
#define HAVE_TIMES 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define to 1 if you have the `tmpnam' function. */
/* #undef HAVE_TMPNAM */

/* Define if you have the \`uintptr_t' type. */
#define HAVE_UINTPTR_T 1

/* Define to 1 if you have the `vasprintf' function. */
#define HAVE_VASPRINTF 1

/* Define to 1 if you have the `vfork' function. */
/* #undef HAVE_VFORK */

/* Define to 1 if you have the <vfork.h> header file. */
/* #undef HAVE_VFORK_H */

/* Define to 1 if you have the `vfprintf' function. */
#define HAVE_VFPRINTF 1

/* Define to 1 if you have the `vprintf' function. */
#define HAVE_VPRINTF 1

/* Define to 1 if you have the `vsprintf' function. */
#define HAVE_VSPRINTF 1

/* Define to 1 if you have the `wait3' function. */
/* #undef HAVE_WAIT3 */

/* Define to 1 if you have the `wait4' function. */
/* #undef HAVE_WAIT4 */

/* Define to 1 if `fork' works. */
#define HAVE_WORKING_FORK 1

/* Define to 1 if `vfork' works. */
/* #undef HAVE_WORKING_VFORK */

/* Define if you have x86 SHA1 HW acceleration support. */
#ifdef __x86_64__
#define HAVE_X86_SHA1_HW_SUPPORT 1
#endif

/* Define to 1 if you have the `_doprnt' function. */
/* #undef HAVE__DOPRNT */

/* Define if you have the _system_configuration variable. */
/* #undef HAVE__SYSTEM_CONFIGURATION */

/* Define to 1 if you have the `__fsetlocking' function. */
/* #undef HAVE___FSETLOCKING */

/* Define if canonicalize_file_name is not declared in system header files. */
#define NEED_DECLARATION_CANONICALIZE_FILE_NAME 1

/* Define if errno must be declared even when <errno.h> is included. */
/* #undef NEED_DECLARATION_ERRNO */

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* The size of `size_t', as computed by sizeof. */
#define SIZEOF_SIZE_T 8

/* Define if you know the direction of stack growth for your system; otherwise
   it will be automatically deduced at run-time. STACK_DIRECTION > 0 => grows
   toward higher addresses STACK_DIRECTION < 0 => grows toward lower addresses
   STACK_DIRECTION = 0 => direction of growth unknown */
#define STACK_DIRECTION -1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to an unsigned 64-bit type available in the compiler. */
#define UNSIGNED_64BIT_TYPE uint64_t

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to the type of a signed integer type wide enough to hold a pointer,
   if such a type exists, and if the system does not define it. */
/* #undef intptr_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */

/* Define to the type of an unsigned integer type wide enough to hold a
   pointer, if such a type exists, and if the system does not define it. */
/* #undef uintptr_t */

/* Define as `fork' if `vfork' does not work. */
#define vfork fork

/* Define if sigsetjmp is available. */
#define HAVE_SIGSETJMP 1

#define ELF_LIST_OPTIONS                  false
#define ELF_SHLIB_LIST_OPTIONS            false
#define ELF_PLT_UNWIND_LIST_OPTIONS       false
#define ELF_SFRAME_LIST_OPTIONS           false
