set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(SLJIT ${CMAKE_CURRENT_SOURCE_DIR}/repo/deps/sljit/sljit_src)

set(CMAKE_POLICY_VERSION_MINIMUM 3.21)

set(BUILD_SHARED_LIBS ON)
set(BUILD_STATIC_LIBS OFF)
set(PCRE2_BUILD_TESTS ON)

set(PCRE2_DEBUG OFF)

set(PCRE2_SUPPORT_JIT ON)

set(PCRE2_SUPPORT_LIBZ ON)
set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libz.dll.a)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zlib/repo)

set(PCRE2_SUPPORT_LIBBZ2 ON)
set(BZIP2_LIBRARIES ${CMAKE_BINARY_DIR}/ports/bzip2/libbz2.dll.a)
set(BZIP2_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../bzip2/repo)

set(PCRE2_SUPPORT_LIBREADLINE ON)
set(READLINE_LIBRARY ${CMAKE_BINARY_DIR}/ports/readline/libreadline.dll.a)
set(READLINE_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/readline)

set(PCRE2_SUPPORT_LIBEDIT OFF)

function(pcre2_sljit_copy name)
    add_custom_command(
        OUTPUT ${GEN}/deps/sljit/sljit_src/${name}
        COMMAND ${CMAKE_COMMAND} -E copy ${SLJIT}/${name} ${GEN}/deps/sljit/sljit_src/${name}
        COMMAND ${HOST_SED} -i
            -e [[s/defined(_WIN64)/(defined(_WIN64) || defined(__moxnox__))/g]]
            -e [[s/^#ifdef _WIN64$/#if defined(_WIN64) || defined(__moxnox__)/]]
            -e [[s/^#ifndef _WIN64$/#if !defined(_WIN64) \&\& !defined(__moxnox__)/]]
            ${ARGN}
            ${GEN}/deps/sljit/sljit_src/${name}
        DEPENDS ${SLJIT}/${name}
        VERBATIM
    )
endfunction()

pcre2_sljit_copy(sljitLir.c)
pcre2_sljit_copy(sljitNativeX86_common.c)
pcre2_sljit_copy(sljitNativeX86_64.c)
pcre2_sljit_copy(sljitConfigInternal.h -e [[s/^#ifdef _WIN32$/#if defined(_WIN32) || defined(__moxnox__)/]])
pcre2_sljit_copy(sljitNativeARM_64.c -e [[s/^#ifdef _WIN32$/#if defined(_WIN32) || defined(__moxnox__)/]])

add_custom_command(
    OUTPUT ${GEN}/deps/sljit/sljit_src/sljitLir.h
    COMMAND ${CMAKE_COMMAND} -E copy ${SLJIT}/sljitLir.h ${GEN}/deps/sljit/sljit_src/sljitLir.h
    DEPENDS ${SLJIT}/sljitLir.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/pcre2_jit_compile.c
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/pcre2_jit_compile.c ${GEN}/src/pcre2_jit_compile.c
    DEPENDS repo/src/pcre2_jit_compile.c
    VERBATIM
)

add_custom_target(pcre2_sljit_patched DEPENDS
    ${GEN}/deps/sljit/sljit_src/sljitLir.c
    ${GEN}/deps/sljit/sljit_src/sljitLir.h
    ${GEN}/deps/sljit/sljit_src/sljitConfigInternal.h
    ${GEN}/deps/sljit/sljit_src/sljitNativeX86_common.c
    ${GEN}/deps/sljit/sljit_src/sljitNativeX86_64.c
    ${GEN}/deps/sljit/sljit_src/sljitNativeARM_64.c
    ${GEN}/src/pcre2_jit_compile.c
)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
add_subdirectory(repo)
unset(BUILD_SHARED_LIBS CACHE)

get_target_property(_pcre2_sources pcre2-8-shared SOURCES)
list(TRANSFORM _pcre2_sources REPLACE "^src/pcre2_jit_compile\\.c$" "${GEN}/src/pcre2_jit_compile.c")
set_target_properties(pcre2-8-shared PROPERTIES SOURCES "${_pcre2_sources}")
set_source_files_properties(${GEN}/src/pcre2_jit_compile.c TARGET_DIRECTORY pcre2-8-shared PROPERTIES GENERATED TRUE)
add_dependencies(pcre2-8-shared pcre2_sljit_patched)
target_include_directories(pcre2-8-shared PRIVATE ${SLJIT})

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies pcre2grep bzip2 zlib)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies pcre2test readline)

foreach(target pcre2-8-shared pcre2-posix-shared pcre2grep pcre2test)
    target_compile_options(${target} PRIVATE ${USERSPACE_CFLAGS}
        -Wno-conversion
        -Wno-sign-conversion
        $<$<C_COMPILER_ID:Clang>:-Wno-implicit-int-conversion>
    )
endforeach()

add_custom_target(pcre2)
add_dependencies(pcre2 pcre2-8-shared pcre2-posix-shared pcre2grep pcre2test)
