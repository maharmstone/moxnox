set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

set(CMAKE_POLICY_VERSION_MINIMUM 3.21)

set(BUILD_SHARED_LIBS ON)
set(BUILD_STATIC_LIBS OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_TESTING OFF)
set(PICKY_COMPILER OFF)

set(ENABLE_DEBUG_LOGGING OFF)

set(CRYPTO_BACKEND OpenSSL)
set(OPENSSL_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/openssl/gen/include)
set(OPENSSL_SSL_LIBRARY ${CMAKE_BINARY_DIR}/ports/openssl/libssl.dll.a)
set(OPENSSL_CRYPTO_LIBRARY ${CMAKE_BINARY_DIR}/ports/openssl/libcrypto.dll.a)
file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})

include_directories(SYSTEM ${CMAKE_CURRENT_SOURCE_DIR}/../openssl/repo/include)

add_library(OpenSSL::Crypto SHARED IMPORTED)
set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ports/openssl/libcrypto.dll
    IMPORTED_IMPLIB ${OPENSSL_CRYPTO_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
)

set(ENABLE_ZLIB_COMPRESSION ON)
set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libz.dll.a)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zlib/repo)

set(CMAKE_PROJECT_libssh2_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR}/project-include.cmake)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
add_subdirectory(repo)

unset(RUNTIME_DEPENDENCIES CACHE)

target_compile_options(libssh2_shared PRIVATE ${USERSPACE_CFLAGS}
    -Wno-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
)

target_compile_options(libssh2_shared PRIVATE "SHELL:-include ${CMAKE_CURRENT_SOURCE_DIR}/u_types.h")

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies libssh2_shared libcrypto zlib)
