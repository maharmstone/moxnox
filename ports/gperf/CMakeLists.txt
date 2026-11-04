project(gperf VERSION 3.3)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

configure_file(config.h.in ${GEN}/config.h @ONLY)

set(GNULIB_LIB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/repo/lib)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

add_library(gperf_gnulib STATIC
    ../gnulib/repo/lib/cloexec.c
    ../gnulib/repo/lib/error.c
    ../gnulib/repo/lib/exitfail.c
    ../gnulib/repo/lib/fcntl.c
    ../gnulib/repo/lib/fopen.c
    ../gnulib/repo/lib/gl_hash_map.c
    ../gnulib/repo/lib/gl_map.c
    ../gnulib/repo/lib/gl_xmap.c
    ../gnulib/repo/lib/memset_explicit.c
    ../gnulib/repo/lib/open.c
    ../gnulib/repo/lib/read-file.c
    ../gnulib/repo/lib/xalloc-die.c
    ../gnulib/repo/lib/xsize.c
)

target_compile_definitions(gperf_gnulib PRIVATE HAVE_CONFIG_H)

target_include_directories(gperf_gnulib PRIVATE
    ${GEN}
    ${GEN}/gnu
    ${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/repo/lib
)

target_compile_options(gperf_gnulib PRIVATE ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-sign-compare
    -Wno-sign-conversion
    -Wno-unused-parameter
    $<$<C_COMPILER_ID:GNU>:-Wno-conversion -Wno-type-limits>
    $<$<C_COMPILER_ID:Clang>:-Wno-implicit-int-conversion -Wno-shorten-64-to-32>
)

add_executable(gperf
    repo/lib/hash.cc
    repo/src/arraylist.cc
    repo/src/bool-array.cc
    repo/src/hash-table.cc
    repo/src/input.cc
    repo/src/keyword.cc
    repo/src/keyword-list.cc
    repo/src/main.cc
    repo/src/options.cc
    repo/src/output.cc
    repo/src/positions.cc
    repo/src/search.cc
    repo/src/version.cc
)

target_compile_definitions(gperf PRIVATE HAVE_CONFIG_H)

target_include_directories(gperf PRIVATE
    ${GEN}
    ${GEN}/gnu
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/repo/lib
)

target_compile_options(gperf PRIVATE ${USERSPACE_CFLAGS}
    $<$<CXX_COMPILER_ID:GNU>:-Wno-conversion>
    $<$<CXX_COMPILER_ID:Clang>:-Wno-sign-conversion -Wno-shorten-64-to-32 -Wno-implicit-float-conversion -Wno-bitwise-instead-of-logical -Wno-vla-cxx-extension -Wno-bitwise-conditional-parentheses -Wno-format-zero-length>
)

target_link_libraries(gperf PRIVATE gperf_gnulib moxnox)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(gperf PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS gperf DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
