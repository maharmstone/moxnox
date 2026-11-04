project(tar VERSION 1.35)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

file(CONFIGURE OUTPUT ${GEN}/lib/rmt-command.h CONTENT "#ifndef DEFAULT_RMT_COMMAND\n# define DEFAULT_RMT_COMMAND \"/libexec/rmt.exe\"\n#endif\n" @ONLY)

add_custom_command(
    OUTPUT ${GEN}/gnu/parse-datetime.c
    COMMAND ${HOST_BISON} -o ${GEN}/gnu/parse-datetime.c ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/parse-datetime.y
    DEPENDS repo/gnulib/lib/parse-datetime.y
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/checkpoint.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/checkpoint.c ${GEN}/src/checkpoint.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include <sys/ioctl.h>\n||]]
            -e [[s|#ifdef TIOCGWINSZ\n  struct winsize ws;\n  if (ioctl (fileno (fp), TIOCGWINSZ, \&ws) == 0 \&\& 0 < ws.ws_col)\n    return ws.ws_col;\n#endif\n|  struct winsize ws;\n  if (tcgetwinsize (fileno (fp), \&ws) == 0 \&\& 0 < ws.ws_col)\n    return ws.ws_col;\n|]]
            ${GEN}/src/checkpoint.c
    DEPENDS repo/src/checkpoint.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/rtapelib.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/paxutils/lib/rtapelib.c ${GEN}/lib/rtapelib.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|  if (initgroups (pw->pw_name, gid) != 0 \&\& errno != EPERM)\n    return "initgroups"\;\n|#if HAVE_INITGROUPS\n  if (initgroups (pw->pw_name, gid) != 0 \&\& errno != EPERM)\n    return "initgroups"\;\n#endif\n|]]
            -e [[s|  if (gethostbyname (remote_host) == NULL)\n    error (EXIT_ON_EXEC_ERROR, 0, _("Cannot connect to %s: resolve failed"),\n\t   remote_host)\;\n|  {\n    struct addrinfo *rmt_ai\;\n    if (getaddrinfo (remote_host, NULL, NULL, \&rmt_ai) != 0)\n      error (EXIT_ON_EXEC_ERROR, 0, _("Cannot connect to %s: resolve failed"),\n\t     remote_host)\;\n    freeaddrinfo (rmt_ai)\;\n  }\n|]]
            ${GEN}/lib/rtapelib.c
    DEPENDS repo/paxutils/lib/rtapelib.c
    VERBATIM
)

add_executable(tar
    ${GEN}/src/checkpoint.c
    repo/src/buffer.c
    repo/src/compare.c
    repo/src/create.c
    repo/src/delete.c
    repo/src/exclist.c
    repo/src/exit.c
    repo/src/extract.c
    repo/src/incremen.c
    repo/src/list.c
    repo/src/map.c
    repo/src/misc.c
    repo/src/names.c
    repo/src/sparse.c
    repo/src/suffix.c
    repo/src/system.c
    repo/src/tar.c
    repo/src/transform.c
    repo/src/unlink.c
    repo/src/update.c
    repo/src/utf8.c
    repo/src/warning.c
    repo/src/xattrs.c
    repo/src/xheader.c
    ${GEN}/lib/rtapelib.c
    repo/paxutils/paxlib/error.c
    repo/paxutils/paxlib/exit-status.c
    repo/paxutils/paxlib/names.c
    repo/lib/wordsplit.c
    ${GEN}/gnu/parse-datetime.c
    repo/gnulib/lib/at-func2.c
    repo/gnulib/lib/fnmatch.c
    repo/gnulib/lib/fopen.c
    repo/gnulib/lib/allocator.c
    repo/gnulib/lib/areadlinkat.c
    repo/gnulib/lib/areadlinkat-with-size.c
    repo/gnulib/lib/areadlink.c
    repo/gnulib/lib/argmatch.c
    repo/gnulib/lib/argp-ba.c
    repo/gnulib/lib/argp-eexst.c
    repo/gnulib/lib/argp-fmtstream.c
    repo/gnulib/lib/argp-fs-xinl.c
    repo/gnulib/lib/argp-help.c
    repo/gnulib/lib/argp-parse.c
    repo/gnulib/lib/argp-pin.c
    repo/gnulib/lib/argp-pvh.c
    repo/gnulib/lib/argp-pv.c
    repo/gnulib/lib/argp-version-etc.c
    repo/gnulib/lib/argp-xinl.c
    repo/gnulib/lib/backupfile.c
    repo/gnulib/lib/backup-find.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/basename.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/c32iscntrl.c
    repo/gnulib/lib/c32tolower.c
    repo/gnulib/lib/c32width.c
    repo/gnulib/lib/careadlinkat.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/chdir-long.c
    repo/gnulib/lib/chmod.c
    repo/gnulib/lib/chown.c
    repo/gnulib/lib/closeout.c
    repo/gnulib/lib/close-stream.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/dirname-lgpl.c
    repo/gnulib/lib/dirname.c
    repo/gnulib/lib/dup2.c
    repo/gnulib/lib/dup-safer.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exclude.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/fchmodat.c
    repo/gnulib/lib/fchownat.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/fd-safer.c
    repo/gnulib/lib/fdutimensat.c
    repo/gnulib/lib/filenamecat-lgpl.c
    repo/gnulib/lib/fprintftime.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/fstatat.c
    repo/gnulib/lib/full-read.c
    repo/gnulib/lib/full-write.c
    repo/gnulib/lib/getcwd.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/gettime.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/human.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/imaxtostr.c
    repo/gnulib/lib/lchown.c
    repo/gnulib/lib/linkat.c
    repo/gnulib/lib/localeconv.c
    repo/gnulib/lib/mbchar.c
    repo/gnulib/lib/mbrtoc32.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbscasecmp.c
    repo/gnulib/lib/mbuiter.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/modechange.c
    repo/gnulib/lib/nl_langinfo.c
    repo/gnulib/lib/nstrftime.c
    repo/gnulib/lib/obstack.c
    repo/gnulib/lib/offtostr.c
    repo/gnulib/lib/openat-die.c
    repo/gnulib/lib/openat-proc.c
    repo/gnulib/lib/openat-safer.c
    repo/gnulib/lib/opendirat.c
    repo/gnulib/lib/opendir-safer.c
    repo/gnulib/lib/open-safer.c
    repo/gnulib/lib/priv-set.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/readlinkat.c
    repo/gnulib/lib/readlink.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/renameat.c
    repo/gnulib/lib/renameatu.c
    repo/gnulib/lib/rpmatch.c
    repo/gnulib/lib/safe-read.c
    repo/gnulib/lib/safe-write.c
    repo/gnulib/lib/save-cwd.c
    repo/gnulib/lib/savedir.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stdopen.c
    repo/gnulib/lib/stpcpy.c
    repo/gnulib/lib/strchrnul.c
    repo/gnulib/lib/strerror.c
    repo/gnulib/lib/strerror-override.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/strtol.c
    repo/gnulib/lib/strtoul.c
    repo/gnulib/lib/symlinkat.c
    repo/gnulib/lib/timegm.c
    repo/gnulib/lib/time_rz.c
    repo/gnulib/lib/timespec.c
    repo/gnulib/lib/umaxtostr.c
    repo/gnulib/lib/unlinkat.c
    repo/gnulib/lib/unlinkdir.c
    repo/gnulib/lib/unsetenv.c
    repo/gnulib/lib/version-etc-fsf.c
    repo/gnulib/lib/version-etc.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/wcwidth.c
    repo/gnulib/lib/wmempcpy.c
    repo/gnulib/lib/xgetcwd.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xsize.c
    repo/gnulib/lib/xstrtoumax.c
    repo/gnulib/lib/xvasprintf.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/mktime.c
    repo/gnulib/lib/printf-args.c
    repo/gnulib/lib/printf-parse.c
    repo/gnulib/lib/snprintf.c
    repo/gnulib/lib/unicase/tolower.c
    repo/gnulib/lib/unictype/ctype_cntrl.c
    repo/gnulib/lib/uniwidth/width.c
    repo/gnulib/lib/vasnprintf.c
    repo/gnulib/lib/vsnprintf.c
)

target_compile_definitions(tar PRIVATE HAVE_CONFIG_H)

target_include_directories(tar PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/gnu
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/paxutils/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/paxutils/paxlib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
)

target_compile_options(tar PRIVATE ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-type-limits
    -Wno-unused-const-variable
    -Wno-unused-value
    -Wno-unused-but-set-variable
    -Wno-missing-field-initializers
    $<$<C_COMPILER_ID:GNU>:-Wno-calloc-transposed-args>
    $<$<C_COMPILER_ID:Clang>:-Wno-tautological-constant-out-of-range-compare -Wno-bitwise-instead-of-logical>
)

target_link_libraries(tar PRIVATE libiconv moxnox)

install(TARGETS tar DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
