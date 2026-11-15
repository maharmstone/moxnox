set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)
set(CMAKE_POLICY_VERSION_MINIMUM 3.21)

set(CMAKE_C_EXTENSIONS OFF)

set(BUILD_SHARED_LIBS ON)
set(BUILD_TESTS OFF)
set(BUILD_EXAMPLES OFF)

set(USE_HTTP_PARSER llhttp)
set(LLHTTP_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../llhttp/repo/include)
set(LLHTTP_LIBRARY ${CMAKE_BINARY_DIR}/ports/llhttp/libllhttp.dll.a)

set(USE_BUNDLED_ZLIB OFF)
set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libz.dll.a)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zlib/repo)

set(REGEX_BACKEND pcre2)
set(PCRE2_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/pcre2/repo/interface)
set(PCRE2_LIBRARY ${CMAKE_BINARY_DIR}/ports/pcre2/repo/libpcre2-8.dll.a)

set(USE_NTLMCLIENT OFF)

set(USE_SSH ON)
set(PKG_CONFIG_EXECUTABLE "")
set(LIBSSH2_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../libssh2/repo/include)
set(LIBSSH2_LIBRARY ${CMAKE_BINARY_DIR}/ports/libssh2/repo/src/libssh2.dll.a)
set(HAVE_LIBSSH2_MEMORY_CREDENTIALS 1)

set(USE_HTTPS OpenSSL)
set(OPENSSL_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/openssl/gen/include)
set(OPENSSL_SSL_LIBRARY ${CMAKE_BINARY_DIR}/ports/openssl/libssl.dll.a)
set(OPENSSL_CRYPTO_LIBRARY ${CMAKE_BINARY_DIR}/ports/openssl/libcrypto.dll.a)
file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})
include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../openssl/repo/include)

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
add_subdirectory(repo)
unset(CMAKE_C_STANDARD CACHE)

foreach(target util libgit2 xdiff git2_cli)
    target_compile_options(${target} PRIVATE ${USERSPACE_CFLAGS}
        -Wno-conversion
        -Wno-sign-conversion
        -Wno-pointer-arith
        -Wno-strict-prototypes
        $<$<C_COMPILER_ID:Clang>:-Wno-missing-field-initializers>
    )
endforeach()

target_compile_options(git2_cli PRIVATE "SHELL:-include alloca.h")

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies util openssl_generate openssl_gen_headers)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies libgit2 openssl_generate openssl_gen_headers)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies libgit2package llhttp libssl libcrypto libssh2_shared pcre2-8-shared zlib)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies git2_cli llhttp libssl libcrypto libssh2_shared pcre2-8-shared zlib)
