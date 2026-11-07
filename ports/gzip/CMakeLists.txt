project(gzip VERSION 1.14)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})
file(MAKE_DIRECTORY ${GEN}/scripts)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

# the z* convenience wrappers are sh scripts generated from .in templates
foreach(script gunzip zcat zcmp zdiff zegrep zfgrep zforce zgrep zless zmore znew gzexe)
    file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/${script}.in SCRIPT_CONTENT)
    string(REPLACE "@VERSION@" "1.14" SCRIPT_CONTENT "${SCRIPT_CONTENT}")
    string(REPLACE "'gzip'" "gzip" SCRIPT_CONTENT "${SCRIPT_CONTENT}")
    string(REPLACE "'grep'" "grep" SCRIPT_CONTENT "${SCRIPT_CONTENT}")
    string(REPLACE "@GREP@" "grep" SCRIPT_CONTENT "${SCRIPT_CONTENT}")
    file(CONFIGURE OUTPUT ${GEN}/scripts/${script} CONTENT "${SCRIPT_CONTENT}" @ONLY)
endforeach()

file(CONFIGURE OUTPUT ${GEN}/version.c CONTENT "#include <config.h>\n#include \"version.h\"\nchar const *Version = \"1.14\";\n" @ONLY)
file(CONFIGURE OUTPUT ${GEN}/version.h CONTENT "extern char const *Version;\n" @ONLY)

add_executable(gzip
    repo/bits.c
    repo/deflate.c
    repo/gzip.c
    repo/inflate.c
    repo/trees.c
    repo/unlzh.c
    repo/unlzw.c
    repo/unpack.c
    repo/unzip.c
    repo/util.c
    repo/zip.c
    ${GEN}/version.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/byteswap.c
    repo/gnulib/lib/cloexec.c
    repo/gnulib/lib/crc.c
    repo/gnulib/lib/dup-safer.c
    repo/gnulib/lib/endian.c
    repo/gnulib/lib/fclose.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fd-safer.c
    repo/gnulib/lib/fdatasync.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/fprintf.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/frexpl.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/gettime.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/lstat.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/open-safer.c
    repo/gnulib/lib/openat-safer.c
    repo/gnulib/lib/openat.c
    repo/gnulib/lib/printf-args.c
    repo/gnulib/lib/printf-frexp.c
    repo/gnulib/lib/printf-frexpl.c
    repo/gnulib/lib/printf-parse.c
    repo/gnulib/lib/printf.c
    repo/gnulib/lib/qsort.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/savedir.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/stat.c
    repo/gnulib/lib/strerror-override.c
    repo/gnulib/lib/strerror.c
    repo/gnulib/lib/unlink.c
    repo/gnulib/lib/unlinkat.c
    repo/gnulib/lib/utimens.c
    repo/gnulib/lib/vasnprintf.c
    repo/gnulib/lib/vfprintf.c
    repo/gnulib/lib/vfzprintf.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xsize.c
    repo/gnulib/lib/yesno.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    target_sources(gzip PRIVATE repo/gnulib/lib/crc-x86_64-pclmul.c)
endif()

target_compile_definitions(gzip PRIVATE HAVE_CONFIG_H GNU_STANDARD=1)

target_include_directories(gzip PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}
    ${GEN}/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
)

target_compile_options(gzip PRIVATE ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-type-limits
    $<$<C_COMPILER_ID:Clang>:-Wno-tautological-constant-out-of-range-compare>
)

target_link_libraries(gzip PRIVATE moxnox)

install(TARGETS gzip DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(PROGRAMS
    ${GEN}/scripts/gunzip
    ${GEN}/scripts/zcat
    ${GEN}/scripts/zcmp
    ${GEN}/scripts/zdiff
    ${GEN}/scripts/zegrep
    ${GEN}/scripts/zfgrep
    ${GEN}/scripts/zforce
    ${GEN}/scripts/zgrep
    ${GEN}/scripts/zless
    ${GEN}/scripts/zmore
    ${GEN}/scripts/znew
    ${GEN}/scripts/gzexe
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)
