project(sed VERSION 4.10)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

# obstack.h uses its own substitution scheme
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/obstack.in.h OBSTACK_H)
string(REPLACE "defined __GL_GNULIB_HEADER" "1" OBSTACK_H "${OBSTACK_H}")
string(REPLACE "defined __GL_REPLACE_OBSTACK__" "0" OBSTACK_H "${OBSTACK_H}")
string(REPLACE "defined __GL_SMALL_PTRDIFF_T__" "0" OBSTACK_H "${OBSTACK_H}")
file(CONFIGURE OUTPUT ${GEN}/lib/obstack.h CONTENT "${OBSTACK_H}" @ONLY)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/malloc/scratch_buffer.h SCRATCH_H)
file(CONFIGURE OUTPUT ${GEN}/lib/malloc/scratch_buffer.gl.h CONTENT "${SCRATCH_H}" @ONLY)

file(CONFIGURE OUTPUT ${GEN}/sed/version.c CONTENT "#include <config.h>\nchar const *Version = \"4.10\";\n" @ONLY)
file(CONFIGURE OUTPUT ${GEN}/sed/version.h CONTENT "extern char const *Version;\n" @ONLY)

add_executable(sed
    ${GEN}/sed/version.c
    repo/sed/compile.c
    repo/sed/debug.c
    repo/sed/execute.c
    repo/sed/mbcs.c
    repo/sed/regexp.c
    repo/sed/sed.c
    repo/sed/utils.c
    repo/gnulib/lib/acl-internal.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/c32isprint.c
    repo/gnulib/lib/c32isspace.c
    repo/gnulib/lib/c32rtomb.c
    repo/gnulib/lib/c32tob.c
    repo/gnulib/lib/c32tolower.c
    repo/gnulib/lib/c32toupper.c
    repo/gnulib/lib/canonicalize-lgpl.c
    repo/gnulib/lib/copy-acl.c
    repo/gnulib/lib/dfa.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/get-permissions.c
    repo/gnulib/lib/getdelim.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/issymlink.c
    repo/gnulib/lib/iswctype.c
    repo/gnulib/lib/localcharset.c
    repo/gnulib/lib/localeinfo.c
    repo/gnulib/lib/lstat.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/malloc/scratch_buffer_grow.c
    repo/gnulib/lib/malloc/scratch_buffer_grow_preserve.c
    repo/gnulib/lib/mbrtoc32.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/memeq.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/obstack.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/qcopy-acl.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/readlink.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/rename.c
    repo/gnulib/lib/rmdir.c
    repo/gnulib/lib/se-selinux.c
    repo/gnulib/lib/set-permissions.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stat.c
    repo/gnulib/lib/streq.c
    repo/gnulib/lib/strerror-override.c
    repo/gnulib/lib/strerror.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strverscmp.c
    repo/gnulib/lib/unicase/tolower.c
    repo/gnulib/lib/unicase/toupper.c
    repo/gnulib/lib/unictype/ctype_print.c
    repo/gnulib/lib/unictype/ctype_space.c
    repo/gnulib/lib/version-etc-fsf.c
    repo/gnulib/lib/version-etc.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/wctype.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xmalloc.c
)

target_compile_definitions(sed PRIVATE HAVE_CONFIG_H)

target_include_directories(sed PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
    ${GEN}/sed
)

target_compile_options(sed PRIVATE ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-unused-value
    -Wno-type-limits
    -Wno-tautological-constant-out-of-range-compare
    -Wno-bitwise-instead-of-logical
)

target_link_libraries(sed PRIVATE moxnox)

install(TARGETS sed DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
