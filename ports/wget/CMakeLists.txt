project(wget VERSION 1.25.0)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/malloc/scratch_buffer.h SCRATCH_H)
string(REPLACE "__always_inline" "inline _GL_ATTRIBUTE_ALWAYS_INLINE" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_likely" "_GL_LIKELY" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_unlikely" "_GL_UNLIKELY" SCRATCH_H "${SCRATCH_H}")
string(REGEX REPLACE "[^\n]*libc_hidden_proto[^\n]*\n" "" SCRATCH_H "${SCRATCH_H}")
file(CONFIGURE OUTPUT ${GEN}/lib/malloc/scratch_buffer.gl.h CONTENT "${SCRATCH_H}" @ONLY)

file(CONFIGURE OUTPUT ${GEN}/src/version.c CONTENT "#include \"version.h\"\nconst char *version_string = \"@PROJECT_VERSION@\";\nconst char *compilation_string = \"cmake\";\nconst char *link_string = \"cmake\";\n" @ONLY)

add_custom_command(
    OUTPUT ${GEN}/src/css_.c
    COMMAND sh -c "'${HOST_FLEX}' -o '${GEN}/src/css.c' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/css.l' && { echo '#include \"wget.h\"'; cat '${GEN}/src/css.c'; } > '${GEN}/src/css_.c'"
    DEPENDS repo/src/css.l
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/build_info.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/build_info.c.in ${GEN}/src/build_info.c.in
    COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/repo/build-aux/build_info.pl ${GEN}/src/build_info.c
    DEPENDS
        repo/src/build_info.c.in
        repo/build-aux/build_info.pl
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/host.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/host.c ${GEN}/src/host.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include "host.h"\n|#include "host.h"\n\n#ifndef IN6_ARE_ADDR_EQUAL\n# define IN6_ARE_ADDR_EQUAL(a, b) (memcmp((a), (b), sizeof(struct in6_addr)) == 0)\n#endif\n|]]
            -e [[s|#include "wget.h"\n|#include "wget.h"\n#include <inttypes.h>\n\n#undef PTR_FORMAT\n#define PTR_FORMAT(p) (int) (2 * sizeof (void *)), (uintptr_t) (p)\n|]]
            -e [[s|0x%0\*lx|0x%0*" PRIxPTR "|g]]
            ${GEN}/src/host.c
    DEPENDS repo/src/host.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/openssl.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/openssl.c ${GEN}/src/openssl.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include "wget.h"\n|#include "wget.h"\n#include <inttypes.h>\n\n#undef PTR_FORMAT\n#define PTR_FORMAT(p) (int) (2 * sizeof (void *)), (uintptr_t) (p)\n|]]
            -e [[s|0x%0\*lx|0x%0*" PRIxPTR "|g]]
            ${GEN}/src/openssl.c
    DEPENDS repo/src/openssl.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/unicase/special-casing-table.h
    COMMAND sh -c "'${HOST_GPERF}' -m 10 '${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/unicase/special-casing-table.gperf' | '${HOST_SED}' -e 's|\\([ \"]\\)[^ \"]*/unicase/special-casing-table\\.gperf\\([ \"]\\)|\\1unicase/special-casing-table.gperf\\2|' > '${GEN}/lib/unicase/special-casing-table.h'"
    DEPENDS repo/gnulib/lib/unicase/special-casing-table.gperf
    VERBATIM
)

add_executable(wget
    ${GEN}/src/build_info.c
    ${GEN}/src/css_.c
    ${GEN}/src/host.c
    ${GEN}/src/version.c
    repo/src/connect.c
    repo/src/convert.c
    repo/src/cookies.c
    repo/src/css-url.c
    repo/src/exits.c
    repo/src/ftp-basic.c
    repo/src/ftp.c
    repo/src/ftp-ls.c
    repo/src/ftp-opie.c
    repo/src/hash.c
    repo/src/hsts.c
    repo/src/html-parse.c
    repo/src/html-url.c
    repo/src/http.c
    repo/src/http-ntlm.c
    repo/src/init.c
    repo/src/log.c
    repo/src/main.c
    repo/src/netrc.c
    ${GEN}/src/openssl.c
    repo/src/progress.c
    repo/src/ptimer.c
    repo/src/recur.c
    repo/src/res.c
    repo/src/retr.c
    repo/src/spider.c
    repo/src/url.c
    repo/src/utils.c
    repo/src/warc.c
    ${GEN}/lib/unicase/special-casing-table.h
    repo/gnulib/lib/base32.c
    repo/gnulib/lib/basename.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/btoc32.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/byteswap.c
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
    repo/gnulib/lib/canonicalize-lgpl.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/chdir-long.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/c-strncasecmp.c
    repo/gnulib/lib/dup2.c
    repo/gnulib/lib/dup.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/flock.c
    repo/gnulib/lib/fnmatch.c
    repo/gnulib/lib/fopen.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/fstatat.c
    repo/gnulib/lib/getcwd.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getgroups.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/getpass.c
    repo/gnulib/lib/group-member.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/iswctype.c
    repo/gnulib/lib/localeconv.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/malloc/scratch_buffer_grow.c
    repo/gnulib/lib/malloc/scratch_buffer_grow_preserve.c
    repo/gnulib/lib/mbchar.c
    repo/gnulib/lib/mbrtoc32.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbsrtoc32s.c
    repo/gnulib/lib/mbsrtoc32s-state.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/md5.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/mkdir.c
    repo/gnulib/lib/mktime.c
    repo/gnulib/lib/nl_langinfo.c
    repo/gnulib/lib/openat-die.c
    repo/gnulib/lib/openat-proc.c
    repo/gnulib/lib/printf-args.c
    repo/gnulib/lib/printf-parse.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/readlink.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/rename.c
    repo/gnulib/lib/rmdir.c
    repo/gnulib/lib/save-cwd.c
    repo/gnulib/lib/secure_getenv.c
    repo/gnulib/lib/select.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/sha1.c
    repo/gnulib/lib/sha1-stream.c
    repo/gnulib/lib/sha256.c
    repo/gnulib/lib/snprintf.c
    repo/gnulib/lib/spawn_faction_adddup2.c
    repo/gnulib/lib/spawn_faction_init.c
    repo/gnulib/lib/spawni.c
    repo/gnulib/lib/spawnp.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stdlib.c
    repo/gnulib/lib/strchrnul.c
    repo/gnulib/lib/strerror.c
    repo/gnulib/lib/strerror-override.c
    repo/gnulib/lib/strerror_r.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/strptime.c
    repo/gnulib/lib/strtol.c
    repo/gnulib/lib/strtoll.c
    repo/gnulib/lib/timegm.c
    repo/gnulib/lib/tmpdir.c
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
    repo/gnulib/lib/unlink.c
    repo/gnulib/lib/vasnprintf.c
    repo/gnulib/lib/vsnprintf.c
    repo/gnulib/lib/vsnzprintf.c
    repo/gnulib/lib/wctype.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/wcwidth.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xmemdup0.c
    repo/gnulib/lib/xsize.c
    repo/gnulib/lib/xstrndup.c
)

set_source_files_properties(repo/gnulib/lib/strptime.c PROPERTIES COMPILE_OPTIONS -std=gnu17)

target_compile_definitions(wget PRIVATE
    HAVE_CONFIG_H
    NDEBUG
    SYSTEM_WGETRC="/etc/wgetrc"
    LOCALEDIR="/share/locale"
)

target_include_directories(wget PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
)

target_compile_options(wget PRIVATE
    ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-type-limits
    -Wno-unused-variable
    -Wno-unterminated-string-initialization
    -Wno-pointer-arith
    $<$<C_COMPILER_ID:GNU>:-Wno-calloc-transposed-args>
    $<$<C_COMPILER_ID:Clang>:-Wno-tautological-constant-out-of-range-compare -Wno-deprecated-non-prototype>
)

target_link_libraries(wget PRIVATE libssl libcrypto zlib libiconv moxnox)

install(TARGETS wget DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
