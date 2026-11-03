project(coreutils VERSION 9.11)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/obstack.in.h OBSTACK_H)
string(REPLACE "defined __GL_GNULIB_HEADER" "1" OBSTACK_H "${OBSTACK_H}")
string(REPLACE "defined __GL_REPLACE_OBSTACK__" "0" OBSTACK_H "${OBSTACK_H}")
string(REPLACE "defined __GL_SMALL_PTRDIFF_T__" "0" OBSTACK_H "${OBSTACK_H}")
file(CONFIGURE OUTPUT ${GEN}/lib/obstack.h CONTENT "${OBSTACK_H}" @ONLY)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/malloc/scratch_buffer.h SCRATCH_H)
string(REPLACE "__always_inline" "inline _GL_ATTRIBUTE_ALWAYS_INLINE" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_likely" "_GL_LIKELY" SCRATCH_H "${SCRATCH_H}")
string(REGEX REPLACE "[^\n]*libc_hidden_proto[^\n]*\n" "" SCRATCH_H "${SCRATCH_H}")
file(CONFIGURE OUTPUT ${GEN}/lib/malloc/scratch_buffer.gl.h CONTENT "${SCRATCH_H}" @ONLY)

file(CONFIGURE OUTPUT ${GEN}/src/version.c CONTENT "#include \"version.h\"\nchar const *Version = \"@PROJECT_VERSION@\";\n" @ONLY)
file(CONFIGURE OUTPUT ${GEN}/src/version.h CONTENT "extern char const *Version;\n" @ONLY)

add_custom_command(
    OUTPUT ${GEN}/lib/freadahead.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/freadahead.c ${GEN}/lib/freadahead.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#elif defined SLOW_BUT_NO_HACKS|#elif defined __moxnox__\n  return 0\;\n#elif defined SLOW_BUT_NO_HACKS|]]
            ${GEN}/lib/freadahead.c
    DEPENDS repo/gnulib/lib/freadahead.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/renameatu.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/renameatu.c ${GEN}/lib/renameatu.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include <stdio.h>\n#include <sys/stat.h>|#include <stdio.h>\n#include <string.h>\n#include <sys/stat.h>|]]
            ${GEN}/lib/renameatu.c
    DEPENDS repo/gnulib/lib/renameatu.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/mountlist.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/mountlist.c ${GEN}/lib/mountlist.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#if MOUNTED_NOT_PORTED\n|#if MOUNTED_NOT_PORTED \&\& !defined(__moxnox__)\n|]]
            ${GEN}/lib/mountlist.c
    DEPENDS repo/gnulib/lib/mountlist.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/expr.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/expr.c ${GEN}/src/expr.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|static_assert (SIZE_MAX <= ULONG_MAX)\;|#if !defined __moxnox__\nstatic_assert (SIZE_MAX <= ULONG_MAX)\;\n#endif|]]
            ${GEN}/src/expr.c
    DEPENDS repo/src/expr.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/stat.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/stat.c ${GEN}/src/stat.c
    COMMAND ${HOST_SED} -z -i
            -e [[s,#include "fs.h",#if defined __linux__ || defined __ANDROID__\n# include "fs.h"\n#endif,]]
            ${GEN}/src/stat.c
    DEPENDS repo/src/stat.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/tools/crc-generate-table${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -O2 -o ${GEN}/tools/crc-generate-table${HOST_EXECUTABLE_SUFFIX} ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/crc-generate-table.c
    DEPENDS repo/gnulib/lib/crc-generate-table.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/crc-sliceby8.h
    COMMAND ${GEN}/tools/crc-generate-table${HOST_EXECUTABLE_SUFFIX} ${GEN}/lib/crc-sliceby8.h
    DEPENDS ${GEN}/tools/crc-generate-table${HOST_EXECUTABLE_SUFFIX}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/dircolors.h
    COMMAND sh -c "perl -w -- '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/dcgen' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/dircolors.hin' > '${GEN}/src/dircolors.h'"
    DEPENDS
        repo/src/dcgen
        repo/src/dircolors.hin
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/tools/make-prime-list${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -O2 -o ${GEN}/tools/make-prime-list${HOST_EXECUTABLE_SUFFIX} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/make-prime-list.c
    DEPENDS repo/src/make-prime-list.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/primes.h
    COMMAND sh -c "'${GEN}/tools/make-prime-list${HOST_EXECUTABLE_SUFFIX}' 5000 > '${GEN}/src/primes.h'"
    DEPENDS ${GEN}/tools/make-prime-list${HOST_EXECUTABLE_SUFFIX}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/tools/crctab${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -O2 -DCRCTAB -o ${GEN}/tools/crctab${HOST_EXECUTABLE_SUFFIX} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/cksum_crc.c
    DEPENDS repo/src/cksum_crc.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/crctab.c
    COMMAND sh -c "'${GEN}/tools/crctab${HOST_EXECUTABLE_SUFFIX}' > '${GEN}/src/crctab.c'"
    DEPENDS ${GEN}/tools/crctab${HOST_EXECUTABLE_SUFFIX}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/parse-datetime.c
    COMMAND ${HOST_BISON} -o ${GEN}/lib/parse-datetime.c ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/parse-datetime.y
    DEPENDS repo/gnulib/lib/parse-datetime.y
    VERBATIM
)

add_library(libcoreutils STATIC
    ${GEN}/lib/crc-sliceby8.h
    ${GEN}/lib/freadahead.c
    ${GEN}/lib/mountlist.c
    ${GEN}/lib/renameatu.c
    repo/gl/lib/buffer-lcm.c
    repo/gl/lib/cl-strtod.c
    repo/gl/lib/cl-strtold.c
    repo/gl/lib/dtimespec-bound.c
    repo/gl/lib/fadvise.c
    repo/gl/lib/fd-reopen.c
    repo/gl/lib/heap.c
    repo/gl/lib/mbbuf.c
    repo/gl/lib/rand-isaac.c
    repo/gl/lib/randint.c
    repo/gl/lib/randperm.c
    repo/gl/lib/randread.c
    repo/gl/lib/root-dev-ino.c
    repo/gl/lib/skipchars.c
    repo/gl/lib/strintcmp.c
    repo/gl/lib/strnumcmp.c
    repo/gl/lib/targetdir.c
    repo/gl/lib/xdectoimax.c
    repo/gl/lib/xdectoumax.c
    repo/gl/lib/xfts.c
    repo/gnulib/lib/acl-errno-valid.c
    repo/gnulib/lib/acl-internal.c
    repo/gnulib/lib/alignalloc.c
    repo/gnulib/lib/allocator.c
    repo/gnulib/lib/areadlink-with-size.c
    repo/gnulib/lib/areadlink.c
    repo/gnulib/lib/areadlinkat-with-size.c
    repo/gnulib/lib/areadlinkat.c
    repo/gnulib/lib/argmatch.c
    repo/gnulib/lib/argv-iter.c
    repo/gnulib/lib/asprintf.c
    repo/gnulib/lib/at-func2.c
    repo/gnulib/lib/backup-find.c
    repo/gnulib/lib/backup-rename.c
    repo/gnulib/lib/backupfile.c
    repo/gnulib/lib/base32.c
    repo/gnulib/lib/base64.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/basename.c
    repo/gnulib/lib/binary-io.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/btoc32.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/byteswap.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/c-file-type.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/c-strncasecmp.c
    repo/gnulib/lib/c-strtod.c
    repo/gnulib/lib/c-strtold.c
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
    repo/gnulib/lib/canonicalize.c
    repo/gnulib/lib/careadlinkat.c
    repo/gnulib/lib/chdir-long.c
    repo/gnulib/lib/chmod.c
    repo/gnulib/lib/chmodat.c
    repo/gnulib/lib/chown.c
    repo/gnulib/lib/chownat.c
    repo/gnulib/lib/cloexec.c
    repo/gnulib/lib/close-stream.c
    repo/gnulib/lib/closein.c
    repo/gnulib/lib/closeout.c
    repo/gnulib/lib/concat-filename.c
    repo/gnulib/lib/copy-acl.c
    repo/gnulib/lib/copy-file-range.c
    repo/gnulib/lib/cpu-supports.c
    repo/gnulib/lib/crc.c
    repo/gnulib/lib/cycle-check.c
    repo/gnulib/lib/di-set.c
    repo/gnulib/lib/dirchownmod.c
    repo/gnulib/lib/dirname-lgpl.c
    repo/gnulib/lib/dirname.c
    repo/gnulib/lib/dtoastr.c
    repo/gnulib/lib/dtotimespec.c
    repo/gnulib/lib/dup-safer-flag.c
    repo/gnulib/lib/dup-safer.c
    repo/gnulib/lib/dup2.c
    repo/gnulib/lib/endian.c
    repo/gnulib/lib/errno-iter.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/euidaccess.c
    repo/gnulib/lib/exclude.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/faccessat.c
    repo/gnulib/lib/fchmodat.c
    repo/gnulib/lib/fchownat.c
    repo/gnulib/lib/fclose.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fd-safer-flag.c
    repo/gnulib/lib/fd-safer.c
    repo/gnulib/lib/fdatasync.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/fdutimensat.c
    repo/gnulib/lib/file-has-acl.c
    repo/gnulib/lib/file-set.c
    repo/gnulib/lib/file-type.c
    repo/gnulib/lib/filemode.c
    repo/gnulib/lib/filenamecat-lgpl.c
    repo/gnulib/lib/filenamecat.c
    repo/gnulib/lib/filevercmp.c
    repo/gnulib/lib/findprog-in.c
    repo/gnulib/lib/fnmatch.c
    repo/gnulib/lib/fopen-safer.c
    repo/gnulib/lib/fopen.c
    repo/gnulib/lib/fprintftime.c
    repo/gnulib/lib/fpurge.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/freopen-safer.c
    repo/gnulib/lib/frexpl.c
    repo/gnulib/lib/fstatat.c
    repo/gnulib/lib/ftoastr.c
    repo/gnulib/lib/fts.c
    repo/gnulib/lib/full-read.c
    repo/gnulib/lib/full-write.c
    repo/gnulib/lib/futimens.c
    repo/gnulib/lib/get-permissions.c
    repo/gnulib/lib/getcwd.c
    repo/gnulib/lib/getdelim.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getgroups.c
    repo/gnulib/lib/gethrxtime.c
    repo/gnulib/lib/getline.c
    repo/gnulib/lib/getlogin.c
    repo/gnulib/lib/getrandom.c
    repo/gnulib/lib/gettime-res.c
    repo/gnulib/lib/gettime.c
    repo/gnulib/lib/getugroups.c
    repo/gnulib/lib/glthread/once.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/hashcode-file-inode.c
    repo/gnulib/lib/hashcode-named-file.c
    repo/gnulib/lib/hashcode-string1.c
    repo/gnulib/lib/hashcode-string2.c
    repo/gnulib/lib/human.c
    repo/gnulib/lib/i-ring.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/idcache.c
    repo/gnulib/lib/imaxtostr.c
    repo/gnulib/lib/ino-map.c
    repo/gnulib/lib/isapipe.c
    repo/gnulib/lib/issymlink.c
    repo/gnulib/lib/issymlinkat.c
    repo/gnulib/lib/iswctype.c
    repo/gnulib/lib/lchmod.c
    repo/gnulib/lib/lchown.c
    repo/gnulib/lib/ldtoastr.c
    repo/gnulib/lib/linebuffer.c
    repo/gnulib/lib/link.c
    repo/gnulib/lib/linkat.c
    repo/gnulib/lib/localcharset.c
    repo/gnulib/lib/localeconv.c
    repo/gnulib/lib/localename-environ.c
    repo/gnulib/lib/localename-unsafe.c
    repo/gnulib/lib/long-options.c
    repo/gnulib/lib/lstat.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/malloc/scratch_buffer_grow.c
    repo/gnulib/lib/malloc/scratch_buffer_grow_preserve.c
    repo/gnulib/lib/malloca.c
    repo/gnulib/lib/mbrlen.c
    repo/gnulib/lib/mbrtoc32.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbs_endswith.c
    repo/gnulib/lib/mbscasecmp.c
    repo/gnulib/lib/mbschr.c
    repo/gnulib/lib/mbslen.c
    repo/gnulib/lib/mbsrtoc32s-state.c
    repo/gnulib/lib/mbsrtoc32s.c
    repo/gnulib/lib/mbsstr.c
    repo/gnulib/lib/mbswidth.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/mcel.c
    repo/gnulib/lib/md5-stream.c
    repo/gnulib/lib/md5.c
    repo/gnulib/lib/memcasecmp.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/memchr2.c
    repo/gnulib/lib/memcmp2.c
    repo/gnulib/lib/memcoll.c
    repo/gnulib/lib/memeq.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/memset_explicit.c
    repo/gnulib/lib/mgetgroups.c
    repo/gnulib/lib/mkancesdirs.c
    repo/gnulib/lib/mkdir-p.c
    repo/gnulib/lib/mkdir.c
    repo/gnulib/lib/mkfifo.c
    repo/gnulib/lib/mkfifoat.c
    repo/gnulib/lib/mknod.c
    repo/gnulib/lib/mknodat.c
    repo/gnulib/lib/mkstemp-safer.c
    repo/gnulib/lib/mkstemp.c
    repo/gnulib/lib/mktime.c
    repo/gnulib/lib/modechange.c
    repo/gnulib/lib/mpsort.c
    repo/gnulib/lib/nanosleep.c
    repo/gnulib/lib/next-prime.c
    repo/gnulib/lib/nl_langinfo.c
    repo/gnulib/lib/nproc.c
    repo/gnulib/lib/nstrftime.c
    repo/gnulib/lib/obstack.c
    repo/gnulib/lib/open-safer.c
    repo/gnulib/lib/open.c
    repo/gnulib/lib/openat-die.c
    repo/gnulib/lib/openat-proc.c
    repo/gnulib/lib/openat-safer.c
    repo/gnulib/lib/openat.c
    repo/gnulib/lib/opendirat.c
    repo/gnulib/lib/physmem.c
    repo/gnulib/lib/pipe-safer.c
    repo/gnulib/lib/pipe2-safer.c
    repo/gnulib/lib/pipe2.c
    repo/gnulib/lib/posix_memalign.c
    repo/gnulib/lib/posixtm.c
    repo/gnulib/lib/posixver.c
    repo/gnulib/lib/printf-args.c
    repo/gnulib/lib/printf-frexp.c
    repo/gnulib/lib/printf-frexpl.c
    repo/gnulib/lib/printf-parse.c
    repo/gnulib/lib/priv-set.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/propername-lite.c
    repo/gnulib/lib/pselect.c
    repo/gnulib/lib/pthread_sigmask.c
    repo/gnulib/lib/putenv.c
    repo/gnulib/lib/qcopy-acl.c
    repo/gnulib/lib/qset-acl.c
    repo/gnulib/lib/qsort.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/read-file.c
    repo/gnulib/lib/readlink.c
    repo/gnulib/lib/readlinkat.c
    repo/gnulib/lib/readtokens.c
    repo/gnulib/lib/readtokens0.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/renameat.c
    repo/gnulib/lib/rmdir.c
    repo/gnulib/lib/safe-read.c
    repo/gnulib/lib/safe-write.c
    repo/gnulib/lib/same-inode.c
    repo/gnulib/lib/same.c
    repo/gnulib/lib/save-cwd.c
    repo/gnulib/lib/savedir.c
    repo/gnulib/lib/savewd.c
    repo/gnulib/lib/se-label.c
    repo/gnulib/lib/se-selinux.c
    repo/gnulib/lib/secure_getenv.c
    repo/gnulib/lib/select.c
    repo/gnulib/lib/set-acl.c
    repo/gnulib/lib/set-permissions.c
    repo/gnulib/lib/setenv.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/settime.c
    repo/gnulib/lib/sha1-stream.c
    repo/gnulib/lib/sha1.c
    repo/gnulib/lib/sha256-stream.c
    repo/gnulib/lib/sha256.c
    repo/gnulib/lib/sha3-stream.c
    repo/gnulib/lib/sha3.c
    repo/gnulib/lib/sha512-stream.c
    repo/gnulib/lib/sha512.c
    repo/gnulib/lib/sig2str.c
    repo/gnulib/lib/sm3-stream.c
    repo/gnulib/lib/sm3.c
    repo/gnulib/lib/snprintf.c
    repo/gnulib/lib/spawn.c
    repo/gnulib/lib/spawn_faction_addclose.c
    repo/gnulib/lib/spawn_faction_adddup2.c
    repo/gnulib/lib/spawn_faction_destroy.c
    repo/gnulib/lib/spawn_faction_init.c
    repo/gnulib/lib/spawnattr_destroy.c
    repo/gnulib/lib/spawnattr_init.c
    repo/gnulib/lib/spawnattr_setdefault.c
    repo/gnulib/lib/spawnattr_setflags.c
    repo/gnulib/lib/spawni.c
    repo/gnulib/lib/spawnp.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stat.c
    repo/gnulib/lib/stdc_bit_width.c
    repo/gnulib/lib/stdc_leading_zeros.c
    repo/gnulib/lib/stdc_memreverse8u.c
    repo/gnulib/lib/stdc_trailing_zeros.c
    repo/gnulib/lib/stpcpy.c
    repo/gnulib/lib/str_endswith.c
    repo/gnulib/lib/strchrnul.c
    repo/gnulib/lib/streq.c
    repo/gnulib/lib/strerror-override.c
    repo/gnulib/lib/strerror.c
    repo/gnulib/lib/strerrorname_np.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/strnul.c
    repo/gnulib/lib/strsignal.c
    repo/gnulib/lib/strstr.c
    repo/gnulib/lib/strtod.c
    repo/gnulib/lib/strtold.c
    repo/gnulib/lib/symlink.c
    repo/gnulib/lib/symlinkat.c
    repo/gnulib/lib/tempname.c
    repo/gnulib/lib/time_rz.c
    repo/gnulib/lib/timegm.c
    repo/gnulib/lib/timespec.c
    repo/gnulib/lib/tmpdir.c
    repo/gnulib/lib/umaxtostr.c
    repo/gnulib/lib/unicase/tolower.c
    repo/gnulib/lib/unicodeio.c
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
    repo/gnulib/lib/unistr/u8-uctomb-aux.c
    repo/gnulib/lib/uniwidth/width.c
    repo/gnulib/lib/unlink.c
    repo/gnulib/lib/unlinkat.c
    repo/gnulib/lib/unlinkdir.c
    repo/gnulib/lib/unsetenv.c
    repo/gnulib/lib/userspec.c
    repo/gnulib/lib/utimecmp.c
    repo/gnulib/lib/utimens.c
    repo/gnulib/lib/utimensat.c
    repo/gnulib/lib/vasnprintf.c
    repo/gnulib/lib/vfzprintf.c
    repo/gnulib/lib/vsnzprintf.c
    repo/gnulib/lib/vzprintf.c
    ${GEN}/lib/parse-datetime.c
    repo/gnulib/lib/vasprintf.c
    repo/gnulib/lib/vaszprintf.c
    repo/gnulib/lib/version-etc-fsf.c
    repo/gnulib/lib/version-etc.c
    repo/gnulib/lib/wctype-h.c
    repo/gnulib/lib/wctype.c
    repo/gnulib/lib/wcwidth.c
    repo/gnulib/lib/write-any-file.c
    repo/gnulib/lib/xalignalloc.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xasprintf.c
    repo/gnulib/lib/xbinary-io.c
    repo/gnulib/lib/xgetcwd.c
    repo/gnulib/lib/xgetgroups.c
    repo/gnulib/lib/xgethostname.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xmemcoll.c
    repo/gnulib/lib/xmemdup0.c
    repo/gnulib/lib/xnanosleep.c
    repo/gnulib/lib/xprintf.c
    repo/gnulib/lib/xsetenv.c
    repo/gnulib/lib/xsize.c
    repo/gnulib/lib/xstrtoimax.c
    repo/gnulib/lib/xstrtol-error.c
    repo/gnulib/lib/xstrtol.c
    repo/gnulib/lib/xstrtold.c
    repo/gnulib/lib/xstrtoul.c
    repo/gnulib/lib/xstrtoumax.c
    repo/gnulib/lib/xtime.c
    repo/gnulib/lib/xvasprintf.c
    repo/gnulib/lib/yesno.c
)

set_target_properties(libcoreutils PROPERTIES OUTPUT_NAME coreutils PREFIX lib)

target_compile_definitions(libcoreutils PRIVATE HAVE_CONFIG_H)
target_link_libraries(libcoreutils PRIVATE libcrypto libiconv)

set(COREUTILS_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gl/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
    ${GEN}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
)

set(COREUTILS_CFLAGS
    ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-type-limits
    -Wno-unused-const-variable
    -Wno-unused-but-set-variable
    -Wno-missing-field-initializers
    -Wno-format-security
    $<$<C_COMPILER_ID:GNU>:-Wno-maybe-uninitialized>
    $<$<C_COMPILER_ID:Clang>:-Wno-tautological-constant-out-of-range-compare -Wno-macro-redefined -Wno-bitwise-instead-of-logical -Wno-ignored-attributes>
)

target_include_directories(libcoreutils PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(libcoreutils PRIVATE ${COREUTILS_CFLAGS})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    target_sources(libcoreutils PRIVATE repo/gnulib/lib/crc-x86_64-pclmul.c)
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    add_library(cu_cksum_simd STATIC
        repo/src/cksum_pclmul.c
        repo/src/cksum_avx2.c
        repo/src/cksum_avx512.c
    )
    set_source_files_properties(repo/src/cksum_pclmul.c PROPERTIES COMPILE_OPTIONS "-mavx;-mpclmul")
    set_source_files_properties(repo/src/cksum_avx2.c PROPERTIES COMPILE_OPTIONS "-mpclmul;-mavx;-mavx2;-mvpclmulqdq")
    set_source_files_properties(repo/src/cksum_avx512.c PROPERTIES COMPILE_OPTIONS "-mpclmul;-mavx;-mavx2;-mavx512bw;-mavx512f;-mvpclmulqdq")

    add_library(cu_wc_simd STATIC
        repo/src/wc_avx2.c
        repo/src/wc_avx512.c
    )
    set_source_files_properties(repo/src/wc_avx2.c PROPERTIES COMPILE_OPTIONS "-mavx2")
    set_source_files_properties(repo/src/wc_avx512.c PROPERTIES COMPILE_OPTIONS "-mavx512bw;-mavx512f")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    add_library(cu_cksum_simd STATIC repo/src/cksum_vmull.c)
    set_source_files_properties(repo/src/cksum_vmull.c PROPERTIES COMPILE_OPTIONS "-march=armv8.1-a+crypto")

    add_library(cu_wc_simd STATIC repo/src/wc_neon.c)
    set_source_files_properties(repo/src/wc_neon.c PROPERTIES COMPILE_OPTIONS "-march=armv8.1-a+simd")
endif()

target_compile_definitions(cu_cksum_simd PRIVATE HAVE_CONFIG_H)
target_include_directories(cu_cksum_simd PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(cu_cksum_simd PRIVATE ${COREUTILS_CFLAGS})

target_compile_definitions(cu_wc_simd PRIVATE HAVE_CONFIG_H)
target_include_directories(cu_wc_simd PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(cu_wc_simd PRIVATE ${COREUTILS_CFLAGS})

###

add_executable(cu_lbracket
    ${GEN}/src/version.c
    repo/src/lbracket.c
)
target_compile_definitions(cu_lbracket PRIVATE HAVE_CONFIG_H)
target_include_directories(cu_lbracket PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(cu_lbracket PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(cu_lbracket PRIVATE libcoreutils moxnox)
set_target_properties(cu_lbracket PROPERTIES OUTPUT_NAME "[")
install(TARGETS cu_lbracket DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(arch
    ${GEN}/src/version.c
    repo/src/uname.c
    repo/src/uname-arch.c
)
target_compile_definitions(arch PRIVATE HAVE_CONFIG_H)
target_include_directories(arch PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(arch PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(arch PRIVATE libcoreutils moxnox)
install(TARGETS arch DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(b2sum
    ${GEN}/src/version.c
    repo/src/cksum.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(b2sum PRIVATE HAVE_CONFIG_H HASH_ALGO_BLAKE2=1)
target_include_directories(b2sum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(b2sum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(b2sum PRIVATE libcoreutils moxnox)
install(TARGETS b2sum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(base32
    ${GEN}/src/version.c
    repo/src/basenc.c
)
target_compile_definitions(base32 PRIVATE HAVE_CONFIG_H BASE_TYPE=32)
target_include_directories(base32 PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(base32 PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(base32 PRIVATE libcoreutils moxnox gmp)
install(TARGETS base32 DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(base64
    ${GEN}/src/version.c
    repo/src/basenc.c
)
target_compile_definitions(base64 PRIVATE HAVE_CONFIG_H BASE_TYPE=64)
target_include_directories(base64 PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(base64 PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(base64 PRIVATE libcoreutils moxnox gmp)
install(TARGETS base64 DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(basename
    ${GEN}/src/version.c
    repo/src/basename.c
)
target_compile_definitions(basename PRIVATE HAVE_CONFIG_H)
target_include_directories(basename PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(basename PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(basename PRIVATE libcoreutils moxnox)
install(TARGETS basename DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(basenc
    ${GEN}/src/version.c
    repo/src/basenc.c
)
target_compile_definitions(basenc PRIVATE HAVE_CONFIG_H BASE_TYPE=42)
target_include_directories(basenc PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(basenc PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(basenc PRIVATE libcoreutils moxnox gmp)
install(TARGETS basenc DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(cat
    ${GEN}/src/version.c
    repo/src/cat.c
)
target_compile_definitions(cat PRIVATE HAVE_CONFIG_H)
target_include_directories(cat PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(cat PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(cat PRIVATE libcoreutils moxnox)
install(TARGETS cat DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(chgrp
    ${GEN}/src/version.c
    repo/src/chown.c
    repo/src/chown-core.c
    repo/src/chown-chgrp.c
)
target_compile_definitions(chgrp PRIVATE HAVE_CONFIG_H)
target_include_directories(chgrp PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(chgrp PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(chgrp PRIVATE libcoreutils moxnox)
install(TARGETS chgrp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(chmod
    ${GEN}/src/version.c
    repo/src/chmod.c
)
target_compile_definitions(chmod PRIVATE HAVE_CONFIG_H)
target_include_directories(chmod PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(chmod PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(chmod PRIVATE libcoreutils moxnox)
install(TARGETS chmod DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(chown
    ${GEN}/src/version.c
    repo/src/chown.c
    repo/src/chown-core.c
    repo/src/chown-chown.c
)
target_compile_definitions(chown PRIVATE HAVE_CONFIG_H)
target_include_directories(chown PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(chown PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(chown PRIVATE libcoreutils moxnox)
install(TARGETS chown DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(cksum
    ${GEN}/src/version.c
    repo/src/cksum.c
    repo/src/sum.c
    repo/src/cksum_crc.c
    ${GEN}/src/crctab.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(cksum PRIVATE HAVE_CONFIG_H HASH_ALGO_CKSUM=1)
target_include_directories(cksum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(cksum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(cksum PRIVATE libcoreutils moxnox libcrypto cu_cksum_simd)
install(TARGETS cksum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(comm
    ${GEN}/src/version.c
    repo/src/comm.c
)
target_compile_definitions(comm PRIVATE HAVE_CONFIG_H)
target_include_directories(comm PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(comm PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(comm PRIVATE libcoreutils moxnox)
install(TARGETS comm DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(cp
    ${GEN}/src/version.c
    repo/src/cp.c
    repo/src/copy.c
    repo/src/copy-file-data.c
    repo/src/cp-hash.c
    repo/src/force-link.c
    repo/src/selinux.c
)
target_compile_definitions(cp PRIVATE HAVE_CONFIG_H)
target_include_directories(cp PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(cp PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(cp PRIVATE libcoreutils moxnox)
install(TARGETS cp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(csplit
    ${GEN}/src/version.c
    repo/src/csplit.c
)
target_compile_definitions(csplit PRIVATE HAVE_CONFIG_H)
target_include_directories(csplit PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(csplit PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(csplit PRIVATE libcoreutils moxnox)
install(TARGETS csplit DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(cut
    ${GEN}/src/version.c
    repo/src/cut.c
    repo/src/set-fields.c
)
target_compile_definitions(cut PRIVATE HAVE_CONFIG_H)
target_include_directories(cut PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(cut PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(cut PRIVATE libcoreutils moxnox)
install(TARGETS cut DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(date
    ${GEN}/src/version.c
    repo/src/date.c
    repo/src/show-date.c
)
target_compile_definitions(date PRIVATE HAVE_CONFIG_H)
target_include_directories(date PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(date PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(date PRIVATE libcoreutils moxnox)
install(TARGETS date DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(dd
    ${GEN}/src/version.c
    repo/src/dd.c
)
target_compile_definitions(dd PRIVATE HAVE_CONFIG_H)
target_include_directories(dd PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(dd PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(dd PRIVATE libcoreutils moxnox)
install(TARGETS dd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(dircolors
    ${GEN}/src/version.c
    repo/src/dircolors.c
    ${GEN}/src/dircolors.h
)
target_compile_definitions(dircolors PRIVATE HAVE_CONFIG_H)
target_include_directories(dircolors PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(dircolors PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(dircolors PRIVATE libcoreutils moxnox)
install(TARGETS dircolors DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(dirname
    ${GEN}/src/version.c
    repo/src/dirname.c
)
target_compile_definitions(dirname PRIVATE HAVE_CONFIG_H)
target_include_directories(dirname PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(dirname PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(dirname PRIVATE libcoreutils moxnox)
install(TARGETS dirname DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(dir
    ${GEN}/src/version.c
    repo/src/ls.c
    repo/src/ls-dir.c
    ${GEN}/src/dircolors.h
)
target_compile_definitions(dir PRIVATE HAVE_CONFIG_H)
target_include_directories(dir PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(dir PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(dir PRIVATE libcoreutils moxnox)
install(TARGETS dir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(du
    ${GEN}/src/version.c
    repo/src/du.c
    repo/src/show-date.c
)
target_compile_definitions(du PRIVATE HAVE_CONFIG_H)
target_include_directories(du PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(du PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(du PRIVATE libcoreutils moxnox)
install(TARGETS du DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(echo
    ${GEN}/src/version.c
    repo/src/echo.c
)
target_compile_definitions(echo PRIVATE HAVE_CONFIG_H)
target_include_directories(echo PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(echo PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(echo PRIVATE libcoreutils moxnox)
install(TARGETS echo DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(env
    ${GEN}/src/version.c
    repo/src/env.c
    repo/src/operand2sig.c
)
target_compile_definitions(env PRIVATE HAVE_CONFIG_H)
target_include_directories(env PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(env PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(env PRIVATE libcoreutils moxnox)
install(TARGETS env DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(expand
    ${GEN}/src/version.c
    repo/src/expand.c
    repo/src/expand-common.c
)
target_compile_definitions(expand PRIVATE HAVE_CONFIG_H)
target_include_directories(expand PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(expand PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(expand PRIVATE libcoreutils moxnox)
install(TARGETS expand DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(expr
    ${GEN}/src/version.c
    ${GEN}/src/expr.c
)
target_compile_definitions(expr PRIVATE HAVE_CONFIG_H)
target_include_directories(expr PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(expr PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(expr PRIVATE libcoreutils moxnox gmp)
install(TARGETS expr DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(hostname
    ${GEN}/src/version.c
    repo/src/hostname.c
)
target_compile_definitions(hostname PRIVATE HAVE_CONFIG_H)
target_include_directories(hostname PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(hostname PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(hostname PRIVATE libcoreutils moxnox)
install(TARGETS hostname DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(kill
    ${GEN}/src/version.c
    repo/src/kill.c
    repo/src/operand2sig.c
)
target_compile_definitions(kill PRIVATE HAVE_CONFIG_H)
target_include_directories(kill PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(kill PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(kill PRIVATE libcoreutils moxnox)
install(TARGETS kill DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(timeout
    ${GEN}/src/version.c
    repo/src/timeout.c
    repo/src/operand2sig.c
)
target_compile_definitions(timeout PRIVATE HAVE_CONFIG_H)
target_include_directories(timeout PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(timeout PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(timeout PRIVATE libcoreutils moxnox)
install(TARGETS timeout DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(factor
    ${GEN}/src/version.c
    repo/src/factor.c
    ${GEN}/src/primes.h
)
target_compile_definitions(factor PRIVATE HAVE_CONFIG_H)
target_include_directories(factor PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(factor PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(factor PRIVATE libcoreutils moxnox gmp)
install(TARGETS factor DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(false
    ${GEN}/src/version.c
    repo/src/false.c
)
target_compile_definitions(false PRIVATE HAVE_CONFIG_H)
target_include_directories(false PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(false PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(false PRIVATE libcoreutils moxnox)
install(TARGETS false DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(fmt
    ${GEN}/src/version.c
    repo/src/fmt.c
)
target_compile_definitions(fmt PRIVATE HAVE_CONFIG_H)
target_include_directories(fmt PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(fmt PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(fmt PRIVATE libcoreutils moxnox)
install(TARGETS fmt DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(fold
    ${GEN}/src/version.c
    repo/src/fold.c
)
target_compile_definitions(fold PRIVATE HAVE_CONFIG_H)
target_include_directories(fold PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(fold PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(fold PRIVATE libcoreutils moxnox)
install(TARGETS fold DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(getlimits
    ${GEN}/src/version.c
    repo/src/getlimits.c
)
target_compile_definitions(getlimits PRIVATE HAVE_CONFIG_H)
target_include_directories(getlimits PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(getlimits PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(getlimits PRIVATE libcoreutils moxnox)
install(TARGETS getlimits DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(ginstall
    ${GEN}/src/version.c
    repo/src/install.c
    repo/src/prog-fprintf.c
    repo/src/copy.c
    repo/src/copy-file-data.c
    repo/src/cp-hash.c
    repo/src/force-link.c
    repo/src/selinux.c
)
target_compile_definitions(ginstall PRIVATE HAVE_CONFIG_H)
target_include_directories(ginstall PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(ginstall PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(ginstall PRIVATE libcoreutils moxnox)
set_target_properties(ginstall PROPERTIES OUTPUT_NAME "install")
install(TARGETS ginstall DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(groups
    ${GEN}/src/version.c
    repo/src/groups.c
    repo/src/group-list.c
)
target_compile_definitions(groups PRIVATE HAVE_CONFIG_H)
target_include_directories(groups PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(groups PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(groups PRIVATE libcoreutils moxnox)
install(TARGETS groups DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(head
    ${GEN}/src/version.c
    repo/src/head.c
)
target_compile_definitions(head PRIVATE HAVE_CONFIG_H)
target_include_directories(head PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(head PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(head PRIVATE libcoreutils moxnox)
install(TARGETS head DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(hostid
    ${GEN}/src/version.c
    repo/src/hostid.c
)
target_compile_definitions(hostid PRIVATE HAVE_CONFIG_H)
target_include_directories(hostid PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(hostid PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(hostid PRIVATE libcoreutils moxnox)
install(TARGETS hostid DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(id
    ${GEN}/src/version.c
    repo/src/id.c
    repo/src/group-list.c
)
target_compile_definitions(id PRIVATE HAVE_CONFIG_H)
target_include_directories(id PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(id PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(id PRIVATE libcoreutils moxnox)
install(TARGETS id DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(join
    ${GEN}/src/version.c
    repo/src/join.c
)
target_compile_definitions(join PRIVATE HAVE_CONFIG_H)
target_include_directories(join PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(join PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(join PRIVATE libcoreutils moxnox)
install(TARGETS join DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(link
    ${GEN}/src/version.c
    repo/src/link.c
)
target_compile_definitions(link PRIVATE HAVE_CONFIG_H)
target_include_directories(link PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(link PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(link PRIVATE libcoreutils moxnox)
install(TARGETS link DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(ln
    ${GEN}/src/version.c
    repo/src/ln.c
    repo/src/force-link.c
    repo/src/relpath.c
)
target_compile_definitions(ln PRIVATE HAVE_CONFIG_H)
target_include_directories(ln PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(ln PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(ln PRIVATE libcoreutils moxnox)
install(TARGETS ln DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(logname
    ${GEN}/src/version.c
    repo/src/logname.c
)
target_compile_definitions(logname PRIVATE HAVE_CONFIG_H)
target_include_directories(logname PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(logname PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(logname PRIVATE libcoreutils moxnox)
install(TARGETS logname DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(ls
    ${GEN}/src/version.c
    repo/src/ls.c
    repo/src/ls-ls.c
    ${GEN}/src/dircolors.h
)
target_compile_definitions(ls PRIVATE HAVE_CONFIG_H)
target_include_directories(ls PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(ls PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(ls PRIVATE libcoreutils moxnox)
install(TARGETS ls DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(md5sum
    ${GEN}/src/version.c
    repo/src/cksum.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(md5sum PRIVATE HAVE_CONFIG_H HASH_ALGO_MD5=1)
target_include_directories(md5sum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(md5sum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(md5sum PRIVATE libcoreutils moxnox libcrypto cu_cksum_simd)
install(TARGETS md5sum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(mkdir
    ${GEN}/src/version.c
    repo/src/mkdir.c
    repo/src/prog-fprintf.c
    repo/src/selinux.c
)
target_compile_definitions(mkdir PRIVATE HAVE_CONFIG_H)
target_include_directories(mkdir PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(mkdir PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(mkdir PRIVATE libcoreutils moxnox)
install(TARGETS mkdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(mkfifo
    ${GEN}/src/version.c
    repo/src/mkfifo.c
    repo/src/selinux.c
)
target_compile_definitions(mkfifo PRIVATE HAVE_CONFIG_H)
target_include_directories(mkfifo PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(mkfifo PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(mkfifo PRIVATE libcoreutils moxnox)
install(TARGETS mkfifo DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(mknod
    ${GEN}/src/version.c
    repo/src/mknod.c
    repo/src/selinux.c
)
target_compile_definitions(mknod PRIVATE HAVE_CONFIG_H)
target_include_directories(mknod PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(mknod PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(mknod PRIVATE libcoreutils moxnox)
install(TARGETS mknod DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(mktemp
    ${GEN}/src/version.c
    repo/src/mktemp.c
)
target_compile_definitions(mktemp PRIVATE HAVE_CONFIG_H)
target_include_directories(mktemp PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(mktemp PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(mktemp PRIVATE libcoreutils moxnox)
install(TARGETS mktemp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(mv
    ${GEN}/src/version.c
    repo/src/mv.c
    repo/src/remove.c
    repo/src/copy.c
    repo/src/copy-file-data.c
    repo/src/cp-hash.c
    repo/src/force-link.c
    repo/src/selinux.c
)
target_compile_definitions(mv PRIVATE HAVE_CONFIG_H)
target_include_directories(mv PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(mv PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(mv PRIVATE libcoreutils moxnox)
install(TARGETS mv DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(nice
    ${GEN}/src/version.c
    repo/src/nice.c
)
target_compile_definitions(nice PRIVATE HAVE_CONFIG_H)
target_include_directories(nice PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(nice PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(nice PRIVATE libcoreutils moxnox)
install(TARGETS nice DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(nl
    ${GEN}/src/version.c
    repo/src/nl.c
)
target_compile_definitions(nl PRIVATE HAVE_CONFIG_H)
target_include_directories(nl PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(nl PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(nl PRIVATE libcoreutils moxnox)
install(TARGETS nl DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(nohup
    ${GEN}/src/version.c
    repo/src/nohup.c
)
target_compile_definitions(nohup PRIVATE HAVE_CONFIG_H)
target_include_directories(nohup PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(nohup PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(nohup PRIVATE libcoreutils moxnox)
install(TARGETS nohup DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(nproc
    ${GEN}/src/version.c
    repo/src/nproc.c
)
target_compile_definitions(nproc PRIVATE HAVE_CONFIG_H)
target_include_directories(nproc PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(nproc PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(nproc PRIVATE libcoreutils moxnox)
install(TARGETS nproc DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(numfmt
    ${GEN}/src/version.c
    repo/src/numfmt.c
    repo/src/set-fields.c
)
target_compile_definitions(numfmt PRIVATE HAVE_CONFIG_H)
target_include_directories(numfmt PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(numfmt PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(numfmt PRIVATE libcoreutils moxnox)
install(TARGETS numfmt DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(od
    ${GEN}/src/version.c
    repo/src/od.c
)
target_compile_definitions(od PRIVATE HAVE_CONFIG_H)
target_include_directories(od PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(od PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(od PRIVATE libcoreutils moxnox)
install(TARGETS od DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(paste
    ${GEN}/src/version.c
    repo/src/paste.c
)
target_compile_definitions(paste PRIVATE HAVE_CONFIG_H)
target_include_directories(paste PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(paste PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(paste PRIVATE libcoreutils moxnox)
install(TARGETS paste DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(pathchk
    ${GEN}/src/version.c
    repo/src/pathchk.c
)
target_compile_definitions(pathchk PRIVATE HAVE_CONFIG_H)
target_include_directories(pathchk PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(pathchk PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(pathchk PRIVATE libcoreutils moxnox)
install(TARGETS pathchk DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(printenv
    ${GEN}/src/version.c
    repo/src/printenv.c
)
target_compile_definitions(printenv PRIVATE HAVE_CONFIG_H)
target_include_directories(printenv PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(printenv PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(printenv PRIVATE libcoreutils moxnox)
install(TARGETS printenv DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(printf
    ${GEN}/src/version.c
    repo/src/printf.c
)
target_compile_definitions(printf PRIVATE HAVE_CONFIG_H)
target_include_directories(printf PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(printf PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(printf PRIVATE libcoreutils moxnox)
install(TARGETS printf DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(pr
    ${GEN}/src/version.c
    repo/src/pr.c
)
target_compile_definitions(pr PRIVATE HAVE_CONFIG_H)
target_include_directories(pr PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(pr PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(pr PRIVATE libcoreutils moxnox)
install(TARGETS pr DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(ptx
    ${GEN}/src/version.c
    repo/src/ptx.c
)
target_compile_definitions(ptx PRIVATE HAVE_CONFIG_H)
target_include_directories(ptx PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(ptx PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(ptx PRIVATE libcoreutils moxnox)
install(TARGETS ptx DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(pwd
    ${GEN}/src/version.c
    repo/src/pwd.c
)
target_compile_definitions(pwd PRIVATE HAVE_CONFIG_H)
target_include_directories(pwd PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(pwd PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(pwd PRIVATE libcoreutils moxnox)
install(TARGETS pwd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(readlink
    ${GEN}/src/version.c
    repo/src/readlink.c
)
target_compile_definitions(readlink PRIVATE HAVE_CONFIG_H)
target_include_directories(readlink PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(readlink PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(readlink PRIVATE libcoreutils moxnox)
install(TARGETS readlink DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(realpath
    ${GEN}/src/version.c
    repo/src/realpath.c
    repo/src/relpath.c
)
target_compile_definitions(realpath PRIVATE HAVE_CONFIG_H)
target_include_directories(realpath PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(realpath PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(realpath PRIVATE libcoreutils moxnox)
install(TARGETS realpath DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(rmdir
    ${GEN}/src/version.c
    repo/src/rmdir.c
    repo/src/prog-fprintf.c
)
target_compile_definitions(rmdir PRIVATE HAVE_CONFIG_H)
target_include_directories(rmdir PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(rmdir PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(rmdir PRIVATE libcoreutils moxnox)
install(TARGETS rmdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(rm
    ${GEN}/src/version.c
    repo/src/rm.c
    repo/src/remove.c
)
target_compile_definitions(rm PRIVATE HAVE_CONFIG_H)
target_include_directories(rm PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(rm PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(rm PRIVATE libcoreutils moxnox)
install(TARGETS rm DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(seq
    ${GEN}/src/version.c
    repo/src/seq.c
)
target_compile_definitions(seq PRIVATE HAVE_CONFIG_H)
target_include_directories(seq PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(seq PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(seq PRIVATE libcoreutils moxnox)
install(TARGETS seq DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sha1sum
    ${GEN}/src/version.c
    repo/src/cksum.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(sha1sum PRIVATE HAVE_CONFIG_H HASH_ALGO_SHA1=1)
target_include_directories(sha1sum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sha1sum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sha1sum PRIVATE libcoreutils moxnox libcrypto cu_cksum_simd)
install(TARGETS sha1sum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sha224sum
    ${GEN}/src/version.c
    repo/src/cksum.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(sha224sum PRIVATE HAVE_CONFIG_H HASH_ALGO_SHA224=1)
target_include_directories(sha224sum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sha224sum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sha224sum PRIVATE libcoreutils moxnox libcrypto cu_cksum_simd)
install(TARGETS sha224sum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sha256sum
    ${GEN}/src/version.c
    repo/src/cksum.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(sha256sum PRIVATE HAVE_CONFIG_H HASH_ALGO_SHA256=1)
target_include_directories(sha256sum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sha256sum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sha256sum PRIVATE libcoreutils moxnox libcrypto cu_cksum_simd)
install(TARGETS sha256sum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sha384sum
    ${GEN}/src/version.c
    repo/src/cksum.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(sha384sum PRIVATE HAVE_CONFIG_H HASH_ALGO_SHA384=1)
target_include_directories(sha384sum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sha384sum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sha384sum PRIVATE libcoreutils moxnox libcrypto cu_cksum_simd)
install(TARGETS sha384sum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sha512sum
    ${GEN}/src/version.c
    repo/src/cksum.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(sha512sum PRIVATE HAVE_CONFIG_H HASH_ALGO_SHA512=1)
target_include_directories(sha512sum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sha512sum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sha512sum PRIVATE libcoreutils moxnox libcrypto cu_cksum_simd)
install(TARGETS sha512sum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(shred
    ${GEN}/src/version.c
    repo/src/shred.c
)
target_compile_definitions(shred PRIVATE HAVE_CONFIG_H)
target_include_directories(shred PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(shred PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(shred PRIVATE libcoreutils moxnox)
install(TARGETS shred DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(shuf
    ${GEN}/src/version.c
    repo/src/shuf.c
)
target_compile_definitions(shuf PRIVATE HAVE_CONFIG_H)
target_include_directories(shuf PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(shuf PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(shuf PRIVATE libcoreutils moxnox)
install(TARGETS shuf DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sleep
    ${GEN}/src/version.c
    repo/src/sleep.c
)
target_compile_definitions(sleep PRIVATE HAVE_CONFIG_H)
target_include_directories(sleep PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sleep PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sleep PRIVATE libcoreutils moxnox)
install(TARGETS sleep DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sort
    ${GEN}/src/version.c
    repo/src/sort.c
)
target_compile_definitions(sort PRIVATE HAVE_CONFIG_H)
target_include_directories(sort PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sort PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sort PRIVATE libcoreutils moxnox libcrypto)
install(TARGETS sort DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(split
    ${GEN}/src/version.c
    repo/src/split.c
    repo/src/temp-stream.c
)
target_compile_definitions(split PRIVATE HAVE_CONFIG_H)
target_include_directories(split PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(split PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(split PRIVATE libcoreutils moxnox)
install(TARGETS split DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(stty
    ${GEN}/src/version.c
    repo/src/stty.c
)
target_compile_definitions(stty PRIVATE HAVE_CONFIG_H)
target_include_directories(stty PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(stty PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(stty PRIVATE libcoreutils moxnox)
install(TARGETS stty DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(stat
    ${GEN}/src/version.c
    ${GEN}/src/stat.c
    repo/src/find-mount-point.c
)
target_compile_definitions(stat PRIVATE HAVE_CONFIG_H)
target_include_directories(stat PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(stat PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(stat PRIVATE libcoreutils moxnox)
install(TARGETS stat DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sum
    ${GEN}/src/version.c
    repo/src/sum.c
    repo/src/cksum.c
    repo/src/blake2/b2sum.c
    repo/src/blake2/blake2b-ref.c
)
target_compile_definitions(sum PRIVATE HAVE_CONFIG_H HASH_ALGO_SUM=1)
target_include_directories(sum PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sum PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sum PRIVATE libcoreutils moxnox)
install(TARGETS sum DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(sync
    ${GEN}/src/version.c
    repo/src/sync.c
)
target_compile_definitions(sync PRIVATE HAVE_CONFIG_H)
target_include_directories(sync PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(sync PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(sync PRIVATE libcoreutils moxnox)
install(TARGETS sync DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(tac
    ${GEN}/src/version.c
    repo/src/tac.c
    repo/src/temp-stream.c
)
target_compile_definitions(tac PRIVATE HAVE_CONFIG_H)
target_include_directories(tac PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(tac PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(tac PRIVATE libcoreutils moxnox)
install(TARGETS tac DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(tail
    ${GEN}/src/version.c
    repo/src/tail.c
    repo/src/iopoll.c
)
target_compile_definitions(tail PRIVATE HAVE_CONFIG_H)
target_include_directories(tail PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(tail PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(tail PRIVATE libcoreutils moxnox)
install(TARGETS tail DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(tee
    ${GEN}/src/version.c
    repo/src/tee.c
    repo/src/iopoll.c
)
target_compile_definitions(tee PRIVATE HAVE_CONFIG_H)
target_include_directories(tee PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(tee PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(tee PRIVATE libcoreutils moxnox)
install(TARGETS tee DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(test
    ${GEN}/src/version.c
    repo/src/test.c
)
target_compile_definitions(test PRIVATE HAVE_CONFIG_H)
target_include_directories(test PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(test PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(test PRIVATE libcoreutils moxnox)
install(TARGETS test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(touch
    ${GEN}/src/version.c
    repo/src/touch.c
)
target_compile_definitions(touch PRIVATE HAVE_CONFIG_H)
target_include_directories(touch PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(touch PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(touch PRIVATE libcoreutils moxnox)
install(TARGETS touch DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(true
    ${GEN}/src/version.c
    repo/src/true.c
)
target_compile_definitions(true PRIVATE HAVE_CONFIG_H)
target_include_directories(true PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(true PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(true PRIVATE libcoreutils moxnox)
install(TARGETS true DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(truncate
    ${GEN}/src/version.c
    repo/src/truncate.c
)
target_compile_definitions(truncate PRIVATE HAVE_CONFIG_H)
target_include_directories(truncate PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(truncate PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(truncate PRIVATE libcoreutils moxnox)
install(TARGETS truncate DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(tr
    ${GEN}/src/version.c
    repo/src/tr.c
)
target_compile_definitions(tr PRIVATE HAVE_CONFIG_H)
target_include_directories(tr PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(tr PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(tr PRIVATE libcoreutils moxnox)
install(TARGETS tr DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(tsort
    ${GEN}/src/version.c
    repo/src/tsort.c
)
target_compile_definitions(tsort PRIVATE HAVE_CONFIG_H)
target_include_directories(tsort PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(tsort PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(tsort PRIVATE libcoreutils moxnox)
install(TARGETS tsort DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(tty
    ${GEN}/src/version.c
    repo/src/tty.c
)
target_compile_definitions(tty PRIVATE HAVE_CONFIG_H)
target_include_directories(tty PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(tty PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(tty PRIVATE libcoreutils moxnox)
install(TARGETS tty DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(uname
    ${GEN}/src/version.c
    repo/src/uname.c
    repo/src/uname-uname.c
)
target_compile_definitions(uname PRIVATE HAVE_CONFIG_H)
target_include_directories(uname PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(uname PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(uname PRIVATE libcoreutils moxnox)
install(TARGETS uname DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(unexpand
    ${GEN}/src/version.c
    repo/src/unexpand.c
    repo/src/expand-common.c
)
target_compile_definitions(unexpand PRIVATE HAVE_CONFIG_H)
target_include_directories(unexpand PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(unexpand PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(unexpand PRIVATE libcoreutils moxnox)
install(TARGETS unexpand DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(uniq
    ${GEN}/src/version.c
    repo/src/uniq.c
)
target_compile_definitions(uniq PRIVATE HAVE_CONFIG_H)
target_include_directories(uniq PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(uniq PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(uniq PRIVATE libcoreutils moxnox)
install(TARGETS uniq DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(unlink
    ${GEN}/src/version.c
    repo/src/unlink.c
)
target_compile_definitions(unlink PRIVATE HAVE_CONFIG_H)
target_include_directories(unlink PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(unlink PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(unlink PRIVATE libcoreutils moxnox)
install(TARGETS unlink DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(vdir
    ${GEN}/src/version.c
    repo/src/ls.c
    repo/src/ls-vdir.c
    ${GEN}/src/dircolors.h
)
target_compile_definitions(vdir PRIVATE HAVE_CONFIG_H)
target_include_directories(vdir PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(vdir PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(vdir PRIVATE libcoreutils moxnox)
install(TARGETS vdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(wc
    ${GEN}/src/version.c
    repo/src/wc.c
)
target_compile_definitions(wc PRIVATE HAVE_CONFIG_H)
target_include_directories(wc PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(wc PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(wc PRIVATE libcoreutils moxnox cu_wc_simd)
install(TARGETS wc DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(whoami
    ${GEN}/src/version.c
    repo/src/whoami.c
)
target_compile_definitions(whoami PRIVATE HAVE_CONFIG_H)
target_include_directories(whoami PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(whoami PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(whoami PRIVATE libcoreutils moxnox)
install(TARGETS whoami DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_executable(yes
    ${GEN}/src/version.c
    repo/src/yes.c
)
target_compile_definitions(yes PRIVATE HAVE_CONFIG_H)
target_include_directories(yes PRIVATE ${COREUTILS_INCLUDES})
target_compile_options(yes PRIVATE ${COREUTILS_CFLAGS})
target_link_libraries(yes PRIVATE libcoreutils moxnox)
install(TARGETS yes DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

add_custom_target(coreutils)
add_dependencies(coreutils
    arch
    b2sum
    base32
    base64
    basename
    basenc
    chgrp
    chmod
    chown
    cksum
    comm
    csplit
    cu_lbracket
    cut
    date
    dd
    dir
    dircolors
    dirname
    du
    env
    expand
    expr
    factor
    false
    fmt
    fold
    cat
    cp
    echo
    getlimits
    ginstall
    kill
    ln
    ls
    mkdir
    mv
    pwd
    rm
    rmdir
    groups
    stat
    touch
    yes
    head
    hostid
    hostname
    id
    join
    link
    logname
    md5sum
    mkfifo
    mknod
    mktemp
    nice
    nl
    nohup
    nproc
    numfmt
    od
    paste
    pathchk
    pr
    printenv
    printf
    ptx
    readlink
    realpath
    seq
    sha1sum
    sha224sum
    sha256sum
    sha384sum
    sha512sum
    shred
    shuf
    sleep
    sort
    split
    stty
    sum
    sync
    tac
    tail
    tee
    test
    timeout
    tr
    true
    truncate
    tsort
    tty
    uname
    unexpand
    uniq
    unlink
    vdir
    wc
    whoami
)
