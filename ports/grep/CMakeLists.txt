project(grep VERSION 3.12)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

# egrep and fgrep are shell script wrappers generated from egrep.sh
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/egrep.sh EGREP_SH)
string(REPLACE "@SHELL@" "/bin/sh" EGREP_SH "${EGREP_SH}")
string(REPLACE "@grep@" "grep" EGREP_SH "${EGREP_SH}")
string(REPLACE "@option@" "-E" EGREP_CONTENT "${EGREP_SH}")
file(CONFIGURE OUTPUT ${GEN}/src/egrep CONTENT "${EGREP_CONTENT}" @ONLY)
string(REPLACE "@option@" "-F" FGREP_CONTENT "${EGREP_SH}")
file(CONFIGURE OUTPUT ${GEN}/src/fgrep CONTENT "${FGREP_CONTENT}" @ONLY)

add_executable(grep
    repo/src/dfasearch.c
    repo/src/grep.c
    repo/src/kwsearch.c
    repo/src/kwset.c
    repo/src/searchutils.c
    repo/lib/colorize-posix.c
    repo/gnulib/lib/argmatch.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/binary-io.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/btoc32.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/c-ctype.c
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
    repo/gnulib/lib/c32rtomb.c
    repo/gnulib/lib/c32tob.c
    repo/gnulib/lib/c32tolower.c
    repo/gnulib/lib/c32toupper.c
    repo/gnulib/lib/chdir-long.c
    repo/gnulib/lib/cloexec.c
    repo/gnulib/lib/close-stream.c
    repo/gnulib/lib/closeout.c
    repo/gnulib/lib/cycle-check.c
    repo/gnulib/lib/dfa.c
    repo/gnulib/lib/dup-safer.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exclude.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fd-safer.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/fnmatch.c
    repo/gnulib/lib/fopen.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/fstatat.c
    repo/gnulib/lib/fts.c
    repo/gnulib/lib/getcwd-lgpl.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/i-ring.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/iswctype.c
    repo/gnulib/lib/localcharset.c
    repo/gnulib/lib/localeinfo.c
    repo/gnulib/lib/lstat.c
    repo/gnulib/lib/mbscasecmp.c
    repo/gnulib/lib/mbsrtoc32s-state.c
    repo/gnulib/lib/mbsrtoc32s.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/mcel.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/memchr2.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/obstack.c
    repo/gnulib/lib/open-safer.c
    repo/gnulib/lib/openat-die.c
    repo/gnulib/lib/openat-proc.c
    repo/gnulib/lib/openat-safer.c
    repo/gnulib/lib/openat.c
    repo/gnulib/lib/opendirat.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/safe-read.c
    repo/gnulib/lib/save-cwd.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stat.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/strstr.c
    repo/gnulib/lib/version-etc-fsf.c
    repo/gnulib/lib/version-etc.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/wctype.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xbinary-io.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xstrtoimax.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/unicase/tolower.c
    repo/gnulib/lib/unicase/toupper.c
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
)

target_compile_definitions(grep PRIVATE HAVE_CONFIG_H)

target_include_directories(grep PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
)

target_compile_options(grep PRIVATE
    ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-type-limits
    -Wno-unused-const-variable
    $<$<C_COMPILER_ID:GNU>:-Wno-calloc-transposed-args>
    $<$<C_COMPILER_ID:Clang>:-Wno-bitwise-conditional-parentheses -Wno-tautological-constant-out-of-range-compare -Wno-bitwise-instead-of-logical>
)

target_link_libraries(grep PRIVATE moxnox)

install(TARGETS grep DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(PROGRAMS ${GEN}/src/egrep ${GEN}/src/fgrep DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
