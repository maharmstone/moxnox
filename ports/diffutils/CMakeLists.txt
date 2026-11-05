project(diffutils VERSION 3.12)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

file(CONFIGURE OUTPUT ${GEN}/paths.h CONTENT "#define DEFAULT_DIFF_PROGRAM \"diff\"\n#define LOCALEDIR \"/share/locale\"\n" @ONLY)
file(CONFIGURE OUTPUT ${GEN}/version.h CONTENT "extern char const *Version;\n" @ONLY)
file(CONFIGURE OUTPUT ${GEN}/version.c CONTENT "#include <config.h>\nchar const *Version = \"3.12\";\n" @ONLY)

add_library(diffutils_gnulib STATIC
    repo/gnulib/lib/allocator.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/binary-io.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/btoc32.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/c-file-type.c
    repo/gnulib/lib/c-stack.c
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
    repo/gnulib/lib/dup2.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exclude.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/filenamecat-lgpl.c
    repo/gnulib/lib/filenamecat.c
    repo/gnulib/lib/fnmatch.c
    repo/gnulib/lib/fopen.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/fstatat.c
    repo/gnulib/lib/getcwd-lgpl.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/getrandom.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/iswctype.c
    repo/gnulib/lib/localcharset.c
    repo/gnulib/lib/lstat.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/malloca.c
    repo/gnulib/lib/mbrtoc32.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbscasecmp.c
    repo/gnulib/lib/mbsrtoc32s-state.c
    repo/gnulib/lib/mbsrtoc32s.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/mcel.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/mkdir.c
    repo/gnulib/lib/mkstemp.c
    repo/gnulib/lib/mktime.c
    repo/gnulib/lib/nl_langinfo.c
    repo/gnulib/lib/nstrftime.c
    repo/gnulib/lib/openat-die.c
    repo/gnulib/lib/openat-proc.c
    repo/gnulib/lib/openat.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/propername-lite.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/readlinkat.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/save-cwd.c
    repo/gnulib/lib/setenv.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/sh-quote.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stat.c
    repo/gnulib/lib/stdbit.c
    repo/gnulib/lib/stdc_bit_width.c
    repo/gnulib/lib/stdc_leading_zeros.c
    repo/gnulib/lib/stdopen.c
    repo/gnulib/lib/stpcpy.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/system-quote.c
    repo/gnulib/lib/tempname.c
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
    repo/gnulib/lib/unsetenv.c
    repo/gnulib/lib/version-etc-fsf.c
    repo/gnulib/lib/version-etc.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/wctype.c
    repo/gnulib/lib/wcwidth.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xfreopen.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xmalloca.c
    repo/gnulib/lib/xstdopen.c
    repo/gnulib/lib/xstrtoimax.c
    repo/lib/cmpbuf.c
    repo/lib/diagnose.c
)

set(DIFFUTILS_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
)

set(DIFFUTILS_CFLAGS
    ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-unused-const-variable
    -Wno-type-limits
    -Wno-tautological-constant-out-of-range-compare
    -Wno-bool-operation
    $<$<C_COMPILER_ID:GNU>:-Wno-calloc-transposed-args>
)

target_compile_definitions(diffutils_gnulib PRIVATE HAVE_CONFIG_H)
target_include_directories(diffutils_gnulib PRIVATE ${DIFFUTILS_INCLUDES})
target_compile_options(diffutils_gnulib PRIVATE ${DIFFUTILS_CFLAGS})
target_link_libraries(diffutils_gnulib PRIVATE moxnox)

add_executable(diff
    repo/src/analyze.c
    repo/src/context.c
    repo/src/diff.c
    repo/src/dir.c
    repo/src/ed.c
    repo/src/ifdef.c
    repo/src/io.c
    repo/src/normal.c
    repo/src/side.c
    repo/src/system.c
    repo/src/util.c
    ${GEN}/version.c
)

add_executable(diff3
    repo/src/diff3.c
    repo/src/system.c
    ${GEN}/version.c
)

add_executable(sdiff
    repo/src/sdiff.c
    repo/src/system.c
    ${GEN}/version.c
)

add_executable(cmp
    repo/src/cmp.c
    repo/src/system.c
    ${GEN}/version.c
)

target_compile_definitions(diff PRIVATE HAVE_CONFIG_H)
target_include_directories(diff PRIVATE ${DIFFUTILS_INCLUDES})
target_compile_options(diff PRIVATE ${DIFFUTILS_CFLAGS})
target_link_libraries(diff PRIVATE diffutils_gnulib moxnox)
install(TARGETS diff DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

target_compile_definitions(diff3 PRIVATE HAVE_CONFIG_H)
target_include_directories(diff3 PRIVATE ${DIFFUTILS_INCLUDES})
target_compile_options(diff3 PRIVATE ${DIFFUTILS_CFLAGS})
target_link_libraries(diff3 PRIVATE diffutils_gnulib moxnox)
install(TARGETS diff3 DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

target_compile_definitions(sdiff PRIVATE HAVE_CONFIG_H)
target_include_directories(sdiff PRIVATE ${DIFFUTILS_INCLUDES})
target_compile_options(sdiff PRIVATE ${DIFFUTILS_CFLAGS})
target_link_libraries(sdiff PRIVATE diffutils_gnulib moxnox)
install(TARGETS sdiff DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

target_compile_definitions(cmp PRIVATE HAVE_CONFIG_H)
target_include_directories(cmp PRIVATE ${DIFFUTILS_INCLUDES})
target_compile_options(cmp PRIVATE ${DIFFUTILS_CFLAGS})
target_link_libraries(cmp PRIVATE diffutils_gnulib moxnox)
install(TARGETS cmp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_custom_target(diffutils)
add_dependencies(diffutils
    diff
    diff3
    sdiff
    cmp
)
