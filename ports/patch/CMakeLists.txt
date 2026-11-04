project(patch VERSION 2.8)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

add_custom_command(
    OUTPUT ${GEN}/lib/parse-datetime.c
    COMMAND ${HOST_BISON} -o ${GEN}/lib/parse-datetime.c ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/parse-datetime.y
    DEPENDS repo/gnulib/lib/parse-datetime.y
    VERBATIM
)

add_executable(patch
    repo/src/merge.c
    repo/src/inp.c
    repo/src/patch.c
    repo/src/pch.c
    repo/src/safe.c
    repo/src/util.c
    repo/src/version.c
    ${GEN}/lib/parse-datetime.c
    repo/gnulib/lib/access.c
    repo/gnulib/lib/argmatch.c
    repo/gnulib/lib/backup-find.c
    repo/gnulib/lib/backupfile.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/c32isprint.c
    repo/gnulib/lib/chdir-long.c
    repo/gnulib/lib/chmod.c
    repo/gnulib/lib/chown.c
    repo/gnulib/lib/cloexec.c
    repo/gnulib/lib/close-stream.c
    repo/gnulib/lib/closeout.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/faccessat.c
    repo/gnulib/lib/fchownat.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/fstatat.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getrandom.c
    repo/gnulib/lib/gettime.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/lchown.c
    repo/gnulib/lib/localcharset.c
    repo/gnulib/lib/lstat.c
    repo/gnulib/lib/malloca.c
    repo/gnulib/lib/mbrtoc32.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/mktime.c
    repo/gnulib/lib/open.c
    repo/gnulib/lib/openat-die.c
    repo/gnulib/lib/openat-proc.c
    repo/gnulib/lib/openat.c
    repo/gnulib/lib/opendirat.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/readlink.c
    repo/gnulib/lib/readlinkat.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/renameat.c
    repo/gnulib/lib/renameatu.c
    repo/gnulib/lib/save-cwd.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stat.c
    repo/gnulib/lib/stdopen.c
    repo/gnulib/lib/stpcpy.c
    repo/gnulib/lib/strerror-override.c
    repo/gnulib/lib/strerror.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/symlinkat.c
    repo/gnulib/lib/tempname.c
    repo/gnulib/lib/time_rz.c
    repo/gnulib/lib/timegm.c
    repo/gnulib/lib/timespec.c
    repo/gnulib/lib/unlink.c
    repo/gnulib/lib/unlinkat.c
    repo/gnulib/lib/unsetenv.c
    repo/gnulib/lib/utimens.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xstdopen.c
    repo/gnulib/lib/unictype/ctype_print.c
)

target_compile_definitions(patch PRIVATE HAVE_CONFIG_H ENABLE_MERGE)

target_include_directories(patch PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
)

target_compile_options(patch PRIVATE ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-unused-but-set-variable
    -Wno-unused-const-variable
    -Wno-type-limits
    -Wno-bitwise-instead-of-logical
)

target_link_libraries(patch PRIVATE moxnox)

install(TARGETS patch DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
