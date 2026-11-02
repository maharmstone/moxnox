include(CheckCSourceCompiles)
include(CMakePushCheckState)

add_library(pixman SHARED
    repo/pixman/pixman.c
    repo/pixman/pixman-access.c
    repo/pixman/pixman-access-accessors.c
    repo/pixman/pixman-arm.c
    repo/pixman/pixman-bits-image.c
    repo/pixman/pixman-combine32.c
    repo/pixman/pixman-combine-float.c
    repo/pixman/pixman-conical-gradient.c
    repo/pixman/pixman-edge.c
    repo/pixman/pixman-edge-accessors.c
    repo/pixman/pixman-fast-path.c
    repo/pixman/pixman-filter.c
    repo/pixman/pixman-glyph.c
    repo/pixman/pixman-general.c
    repo/pixman/pixman-gradient-walker.c
    repo/pixman/pixman-image.c
    repo/pixman/pixman-implementation.c
    repo/pixman/pixman-linear-gradient.c
    repo/pixman/pixman-matrix.c
    repo/pixman/pixman-mips.c
    repo/pixman/pixman-noop.c
    repo/pixman/pixman-ppc.c
    repo/pixman/pixman-radial-gradient.c
    repo/pixman/pixman-region16.c
    repo/pixman/pixman-region32.c
    repo/pixman/pixman-region64f.c
    repo/pixman/pixman-riscv.c
    repo/pixman/pixman-solid-fill.c
    repo/pixman/pixman-timer.c
    repo/pixman/pixman-trap.c
    repo/pixman/pixman-utils.c
    repo/pixman/pixman-x86.c
)

set_target_properties(pixman PROPERTIES
    VERSION 0.46.4
)

# FIXME - shouldn't have to specify twice
set(PIXMAN_VERSION_MAJOR 0)
set(PIXMAN_VERSION_MINOR 46)
set(PIXMAN_VERSION_MICRO 4)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/repo/pixman/pixman-version.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pixman-version.h"
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    cmake_push_check_state()
    set(CMAKE_REQUIRED_FLAGS "${USERSPACE_CFLAGS}")
    set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

    set(PIXMAN_USE_SSE2 ON) # supported on every amd64 CPU

    check_c_source_compiles("
        #include <mmintrin.h>
        #include <xmmintrin.h>
        #include <emmintrin.h>
        int param;
        int main () {
            __m128i a = _mm_set1_epi32 (param), b = _mm_set1_epi32 (param + 1), c;
            c = _mm_xor_si128 (a, b);
            return _mm_cvtsi128_si32(c);
        }
    " PIXMAN_USE_SSSE3)

    cmake_pop_check_state()

    set(PIXMAN_USE_X86_MMX ON) # supported on every amd64 CPU

    target_sources(pixman PRIVATE
        repo/pixman/pixman-mmx.c
        repo/pixman/pixman-sse2.c
        repo/pixman/pixman-ssse3.c
    )
else()
    set(PIXMAN_USE_SSE2 OFF)
    set(PIXMAN_USE_SSSE3 OFF)
    set(PIXMAN_USE_X86_MMX OFF)

    # FIXME - NEON support not compiled in
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/pixman-config.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/pixman-config.h"
)

add_library(pixman_mod STATIC)
target_sources(pixman_mod PUBLIC FILE_SET CXX_MODULES FILES pixman.cpp)
target_include_directories(pixman_mod PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/repo/pixman)
target_include_directories(pixman_mod PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_compile_options(pixman_mod PRIVATE ${USERSPACE_CFLAGS} -fno-exceptions)

target_include_directories(pixman PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/repo/pixman)
target_include_directories(pixman PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_compile_definitions(pixman PRIVATE -DHAVE_CONFIG_H)
target_compile_definitions(pixman PRIVATE "-DPIXMAN_API=__declspec(dllexport)")
target_compile_options(pixman PRIVATE ${USERSPACE_CFLAGS} -Wno-conversion -Wno-unused-parameter -Wno-type-limits -Wno-sign-compare -Wno-missing-field-initializers -Wno-unused-local-typedefs -Wno-unknown-attributes)
target_compile_options(pixman PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti> -mno-stack-arg-probe)

target_link_libraries(pixman PUBLIC pixman_mod)

install(TARGETS pixman DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

install(FILES repo/pixman/pixman.h ${CMAKE_CURRENT_BINARY_DIR}/pixman-version.h
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/pixman-1)

###

add_executable(pixman-test pixman-test.cpp)

target_compile_options(pixman-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(pixman-test moxnox print errno)
target_link_libraries(pixman-test pixman)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(pixman-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS pixman-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
