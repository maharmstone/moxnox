project(m4 VERSION 1.4.21)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

configure_file(config.h.in ${GEN}/config.h @ONLY)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/malloc/scratch_buffer.h SCRATCH_H)
string(REPLACE "__always_inline" "inline _GL_ATTRIBUTE_ALWAYS_INLINE" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_likely" "_GL_LIKELY" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_unlikely" "_GL_UNLIKELY" SCRATCH_H "${SCRATCH_H}")
string(REGEX REPLACE "[^\n]*libc_hidden_proto[^\n]*\n" "" SCRATCH_H "${SCRATCH_H}")
file(CONFIGURE OUTPUT ${GEN}/gnu/malloc/scratch_buffer.gl.h CONTENT "${SCRATCH_H}" @ONLY)

add_custom_command(
    OUTPUT ${GEN}/gnu/freadahead.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/freadahead.c ${GEN}/gnu/freadahead.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#elif defined SLOW_BUT_NO_HACKS|#elif defined __moxnox__\n  return 0\;\n#elif defined SLOW_BUT_NO_HACKS|]]
            ${GEN}/gnu/freadahead.c
    DEPENDS repo/gnulib/lib/freadahead.c
    VERBATIM
)

add_executable(m4
    repo/src/builtin.c
    repo/src/debug.c
    repo/src/eval.c
    repo/src/format.c
    repo/src/freeze.c
    repo/src/input.c
    repo/src/m4.c
    repo/src/macro.c
    repo/src/output.c
    repo/src/path.c
    repo/src/symtab.c
    ${GEN}/gnu/freadahead.c
    repo/gnulib/lib/asyncsafe-spin.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/c-stack.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/c32isalnum.c
    repo/gnulib/lib/c32iscntrl.c
    repo/gnulib/lib/c32isprint.c
    repo/gnulib/lib/c32isspace.c
    repo/gnulib/lib/c32width.c
    repo/gnulib/lib/canonicalize-lgpl.c
    repo/gnulib/lib/canonicalize.c
    repo/gnulib/lib/chdir-long.c
    repo/gnulib/lib/clean-temp-simple.c
    repo/gnulib/lib/clean-temp.c
    repo/gnulib/lib/cloexec.c
    repo/gnulib/lib/close-stream.c
    repo/gnulib/lib/closein.c
    repo/gnulib/lib/closeout.c
    repo/gnulib/lib/concat-filename.c
    repo/gnulib/lib/dup-safer-flag.c
    repo/gnulib/lib/dup-safer.c
    repo/gnulib/lib/dup2.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/execute.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/fatal-signal.c
    repo/gnulib/lib/fclose.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fd-safer-flag.c
    repo/gnulib/lib/fd-safer.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/file-set.c
    repo/gnulib/lib/filenamecat-lgpl.c
    repo/gnulib/lib/filenamecat.c
    repo/gnulib/lib/findprog-in.c
    repo/gnulib/lib/fopen-safer.c
    repo/gnulib/lib/fopen.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/frexp.c
    repo/gnulib/lib/frexpl.c
    repo/gnulib/lib/fstatat.c
    repo/gnulib/lib/getcwd.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getrandom.c
    repo/gnulib/lib/gl_avltree_oset.c
    repo/gnulib/lib/gl_linked_list.c
    repo/gnulib/lib/gl_linkedhash_list.c
    repo/gnulib/lib/gl_list.c
    repo/gnulib/lib/gl_oset.c
    repo/gnulib/lib/gl_xlist.c
    repo/gnulib/lib/gl_xoset.c
    repo/gnulib/lib/glthread/spin.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/hashcode-named-file.c
    repo/gnulib/lib/hashcode-string2.c
    repo/gnulib/lib/hashkey-string.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/issymlink.c
    repo/gnulib/lib/iswctype.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/malloc/scratch_buffer_grow.c
    repo/gnulib/lib/malloc/scratch_buffer_grow_preserve.c
    repo/gnulib/lib/malloca.c
    repo/gnulib/lib/mbchar.c
    repo/gnulib/lib/mbrtoc32.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbslen.c
    repo/gnulib/lib/mbsstr.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/memchr2.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/mkdir.c
    repo/gnulib/lib/mkstemp-safer.c
    repo/gnulib/lib/mkstemp.c
    repo/gnulib/lib/next-prime.c
    repo/gnulib/lib/nl_langinfo.c
    repo/gnulib/lib/obstack.c
    repo/gnulib/lib/open.c
    repo/gnulib/lib/openat-die.c
    repo/gnulib/lib/openat-proc.c
    repo/gnulib/lib/openat.c
    repo/gnulib/lib/pipe2-safer.c
    repo/gnulib/lib/pipe2.c
    repo/gnulib/lib/printf-args.c
    repo/gnulib/lib/printf-frexp.c
    repo/gnulib/lib/printf-frexpl.c
    repo/gnulib/lib/printf-parse.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/propername.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/readlink.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/rename.c
    repo/gnulib/lib/rmdir.c
    repo/gnulib/lib/save-cwd.c
    repo/gnulib/lib/secure_getenv.c
    repo/gnulib/lib/setlocale.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/sig-handler.c
    repo/gnulib/lib/spawn-pipe.c
    repo/gnulib/lib/spawn.c
    repo/gnulib/lib/spawn_faction_addchdir.c
    repo/gnulib/lib/spawn_faction_addclose.c
    repo/gnulib/lib/spawn_faction_adddup2.c
    repo/gnulib/lib/spawn_faction_addopen.c
    repo/gnulib/lib/spawn_faction_destroy.c
    repo/gnulib/lib/spawn_faction_init.c
    repo/gnulib/lib/spawnattr_destroy.c
    repo/gnulib/lib/spawnattr_init.c
    repo/gnulib/lib/spawnattr_setflags.c
    repo/gnulib/lib/spawnattr_setsigmask.c
    repo/gnulib/lib/spawni.c
    repo/gnulib/lib/spawnp.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stpcpy.c
    repo/gnulib/lib/strchrnul.c
    repo/gnulib/lib/striconv.c
    repo/gnulib/lib/string.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/strstr.c
    repo/gnulib/lib/strtod.c
    repo/gnulib/lib/tempname.c
    repo/gnulib/lib/tmpdir.c
    repo/gnulib/lib/trim.c
    repo/gnulib/lib/unictype/ctype_alnum.c
    repo/gnulib/lib/unictype/ctype_cntrl.c
    repo/gnulib/lib/unictype/ctype_print.c
    repo/gnulib/lib/unictype/ctype_space.c
    repo/gnulib/lib/uniwidth/width.c
    repo/gnulib/lib/vasnprintf.c
    repo/gnulib/lib/vaszprintf.c
    repo/gnulib/lib/version-etc-fsf.c
    repo/gnulib/lib/version-etc.c
    repo/gnulib/lib/vfzprintf.c
    repo/gnulib/lib/vzprintf.c
    repo/gnulib/lib/wait-process.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/wctype.c
    repo/gnulib/lib/wcwidth.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xasprintf.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xmemdup0.c
    repo/gnulib/lib/xprintf.c
    repo/gnulib/lib/xsize.c
    repo/gnulib/lib/xstriconv.c
    repo/gnulib/lib/xvasprintf.c
)

target_compile_definitions(m4 PRIVATE HAVE_CONFIG_H)

target_include_directories(m4 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}
    ${GEN}/gnu
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
)

target_compile_options(m4 PRIVATE ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-cast-function-type
    -Wno-type-limits
    $<$<C_COMPILER_ID:Clang>:-Wno-format-security -Wno-tautological-constant-out-of-range-compare>
)

target_link_libraries(m4 PRIVATE libiconv moxnox)

install(TARGETS m4 DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
