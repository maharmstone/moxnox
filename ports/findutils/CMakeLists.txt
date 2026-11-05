project(findutils VERSION 4.10.0)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})
file(MAKE_DIRECTORY ${GEN}/lib)
file(MAKE_DIRECTORY ${GEN}/lib/malloc)
file(MAKE_DIRECTORY ${GEN}/lib/selinux)
file(MAKE_DIRECTORY ${GEN}/gl/lib)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

function(findutils_patch srcpath destpath)
    add_custom_command(
        OUTPUT ${GEN}/${destpath}
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/repo/${srcpath} ${GEN}/${destpath}
        COMMAND ${HOST_SED} -z -i ${ARGN} ${GEN}/${destpath}
        DEPENDS repo/${srcpath}
        VERBATIM
    )
endfunction()

findutils_patch(lib/buildcmd.c lib/buildcmd.c
    -e [[s|#ifndef _POSIX_SOURCE|#if !defined _POSIX_SOURCE \&\& defined HAVE_SYS_PARAM_H|]]
)

findutils_patch(gnulib/lib/fts.c gl/lib/fts.c
    -e [[s|(long int) \&dummy == (long int) (void \*) \&dummy|(intptr_t) \&dummy == (intptr_t) (void *) \&dummy|]]
)

findutils_patch(gnulib/lib/freadahead.c gl/lib/freadahead.c
    -e [[s|#elif defined SLOW_BUT_NO_HACKS|#elif defined __moxnox__ /* Moxnox: unbuffered stdio */\n  return 0\;\n#elif defined SLOW_BUT_NO_HACKS|]]
)

add_custom_command(
    OUTPUT ${GEN}/gl/lib/parse-datetime.c
    COMMAND ${HOST_BISON} -o ${GEN}/gl/lib/parse-datetime.c ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/parse-datetime.y
    DEPENDS repo/gnulib/lib/parse-datetime.y
    VERBATIM
)

add_library(findutils_gnulib STATIC
    ${GEN}/gl/lib/freadahead.c
    repo/gnulib/lib/mountlist.c
    ${GEN}/gl/lib/parse-datetime.c
    repo/gnulib/lib/allocator.c
    repo/gnulib/lib/areadlinkat.c
    repo/gnulib/lib/argmatch.c
    repo/gnulib/lib/argv-iter.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/basename.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/btoc32.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/c32_apply_type_test.c
    repo/gnulib/lib/c32_get_type_test.c
    repo/gnulib/lib/c32isalnum.c
    repo/gnulib/lib/c32isalpha.c
    repo/gnulib/lib/c32isblank.c
    repo/gnulib/lib/c32iscntrl.c
    repo/gnulib/lib/c32isdigit.c
    repo/gnulib/lib/c32isgraph.c
    repo/gnulib/lib/c32islower.c
    repo/gnulib/lib/c32isprint.c
    repo/gnulib/lib/c32ispunct.c
    repo/gnulib/lib/c32isspace.c
    repo/gnulib/lib/c32isupper.c
    repo/gnulib/lib/c32isxdigit.c
    repo/gnulib/lib/c32tolower.c
    repo/gnulib/lib/c32width.c
    repo/gnulib/lib/careadlinkat.c
    repo/gnulib/lib/chdir-long.c
    repo/gnulib/lib/cloexec.c
    repo/gnulib/lib/close-stream.c
    repo/gnulib/lib/closein.c
    repo/gnulib/lib/closeout.c
    repo/gnulib/lib/cycle-check.c
    repo/gnulib/lib/dirname-lgpl.c
    repo/gnulib/lib/dup-safer-flag.c
    repo/gnulib/lib/dup-safer.c
    repo/gnulib/lib/dup2.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/faccessat.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fd-safer-flag.c
    repo/gnulib/lib/fd-safer.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/filemode.c
    repo/gnulib/lib/fnmatch.c
    repo/gnulib/lib/fopen-safer.c
    repo/gnulib/lib/fopen.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/fstatat.c
    ${GEN}/gl/lib/fts.c
    repo/gnulib/lib/getcwd.c
    repo/gnulib/lib/getdelim.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/gettime.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/human.c
    repo/gnulib/lib/i-ring.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/idcache.c
    repo/gnulib/lib/iswctype.c
    repo/gnulib/lib/localcharset.c
    repo/gnulib/lib/localeconv.c
    repo/gnulib/lib/lstat.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/malloca.c
    repo/gnulib/lib/mbchar.c
    repo/gnulib/lib/mbrtoc32.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbslen.c
    repo/gnulib/lib/mbsrtoc32s-state.c
    repo/gnulib/lib/mbsrtoc32s.c
    repo/gnulib/lib/mbsstr.c
    repo/gnulib/lib/mbswidth.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/mktime.c
    repo/gnulib/lib/modechange.c
    repo/gnulib/lib/nl_langinfo.c
    repo/gnulib/lib/nstrftime.c
    repo/gnulib/lib/open-safer.c
    repo/gnulib/lib/openat-die.c
    repo/gnulib/lib/openat-proc.c
    repo/gnulib/lib/openat-safer.c
    repo/gnulib/lib/openat.c
    repo/gnulib/lib/opendir-safer.c
    repo/gnulib/lib/opendirat.c
    repo/gnulib/lib/pipe-safer.c
    repo/gnulib/lib/printf-args.c
    repo/gnulib/lib/printf-parse.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/readlinkat.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/safe-read.c
    repo/gnulib/lib/save-cwd.c
    repo/gnulib/lib/se-selinux.c
    repo/gnulib/lib/selinux-at.c
    repo/gnulib/lib/setenv.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/snprintf.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stat.c
    repo/gnulib/lib/stpcpy.c
    repo/gnulib/lib/strerror-override.c
    repo/gnulib/lib/strerror_r.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strndup.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/time_rz.c
    repo/gnulib/lib/timegm.c
    repo/gnulib/lib/unicase/tolower.c
    repo/gnulib/lib/unictype/ctype_alnum.c
    repo/gnulib/lib/unictype/ctype_alpha.c
    repo/gnulib/lib/unictype/ctype_blank.c
    repo/gnulib/lib/unictype/ctype_cntrl.c
    repo/gnulib/lib/unictype/ctype_digit.c
    repo/gnulib/lib/unictype/ctype_graph.c
    repo/gnulib/lib/unictype/ctype_lower.c
    repo/gnulib/lib/unictype/ctype_print.c
    repo/gnulib/lib/unictype/ctype_punct.c
    repo/gnulib/lib/unictype/ctype_space.c
    repo/gnulib/lib/unictype/ctype_upper.c
    repo/gnulib/lib/unictype/ctype_xdigit.c
    repo/gnulib/lib/unistr/u32-chr.c
    repo/gnulib/lib/unistr/u32-cpy.c
    repo/gnulib/lib/unistr/u32-pcpy.c
    repo/gnulib/lib/unistr/u32-strcat.c
    repo/gnulib/lib/unistr/u32-strlen.c
    repo/gnulib/lib/uniwidth/width.c
    repo/gnulib/lib/unlinkat.c
    repo/gnulib/lib/unsetenv.c
    repo/gnulib/lib/vasnprintf.c
    repo/gnulib/lib/version-etc-fsf.c
    repo/gnulib/lib/version-etc.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/wctype.c
    repo/gnulib/lib/wcwidth.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xsize.c
    repo/gnulib/lib/xstrtod.c
    repo/gnulib/lib/xstrtoul.c
    repo/gnulib/lib/xstrtoumax.c
    repo/gnulib/lib/yesno.c
)

set(FINDUTILS_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib-local/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib
)

set(FINDUTILS_CFLAGS ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-error=return-type
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-return-type
    -Wno-implicit-fallthrough
    -Wno-sign-compare
    -Wno-type-limits
    -Wno-unused-const-variable
    -Wno-unused-but-set-variable
    -Wno-bitwise-instead-of-logical
    $<$<C_COMPILER_ID:GNU>:-Wno-calloc-transposed-args>
)

target_compile_definitions(findutils_gnulib PRIVATE HAVE_CONFIG_H)
target_include_directories(findutils_gnulib PRIVATE ${FINDUTILS_INCLUDES})
target_compile_options(findutils_gnulib PRIVATE ${FINDUTILS_CFLAGS})
target_link_libraries(findutils_gnulib PRIVATE moxnox)

add_executable(find
    repo/find/ftsfind.c
    repo/find/finddata.c
    repo/find/fstype.c
    repo/find/parser.c
    repo/find/pred.c
    repo/find/exec.c
    repo/find/tree.c
    repo/find/util.c
    repo/find/sharefile.c
    repo/find/print.c
    ${GEN}/lib/buildcmd.c
    repo/lib/dircallback.c
    repo/lib/extendbuf.c
    repo/lib/fdleak.c
    repo/lib/findutils-version.c
    repo/lib/listfile.c
    repo/lib/printquoted.c
    repo/lib/qmark.c
    repo/lib/regextype.c
    repo/lib/safe-atoi.c
    repo/lib/splitstring.c
    repo/lib/bugreports.c
)

target_compile_definitions(find PRIVATE HAVE_CONFIG_H)
target_include_directories(find PRIVATE ${FINDUTILS_INCLUDES})
target_compile_options(find PRIVATE ${FINDUTILS_CFLAGS})
target_link_libraries(find PRIVATE findutils_gnulib moxnox)
install(TARGETS find DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_executable(xargs
    repo/xargs/xargs.c
    ${GEN}/lib/buildcmd.c
    repo/lib/extendbuf.c
    repo/lib/fdleak.c
    repo/lib/findutils-version.c
    repo/lib/safe-atoi.c
    repo/lib/bugreports.c
)

target_compile_definitions(xargs PRIVATE HAVE_CONFIG_H)
target_include_directories(xargs PRIVATE ${FINDUTILS_INCLUDES})
target_compile_options(xargs PRIVATE ${FINDUTILS_CFLAGS})
target_link_libraries(xargs PRIVATE findutils_gnulib moxnox)
install(TARGETS xargs DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_custom_target(findutils)
add_dependencies(findutils
    find
    xargs
)
