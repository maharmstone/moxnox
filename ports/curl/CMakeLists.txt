set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/include/curl)

function(curl_read var path)
    file(READ ${path} _out)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${path})
    set(${var} "${_out}" PARENT_SCOPE)
endfunction()

function(curl_patch var old new)
    string(FIND "${${var}}" "${old}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "curl patch anchor not found: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" _out "${${var}}")
    set(${var} "${_out}" PARENT_SCOPE)
endfunction()

curl_read(_curl_h repo/include/curl/curl.h)
curl_patch(_curl_h [=[    defined(__sun__) || defined(__serenity__) || defined(__vxworks__)
#include <sys/select.h>]=] [=[    defined(__sun__) || defined(__serenity__) || defined(__vxworks__) || \
    defined(__moxnox__)
#include <sys/select.h>]=])
curl_patch(_curl_h [=[#elif defined(_WIN32) || \
     (CURL_HAS_DECLSPEC_ATTRIBUTE(dllexport) && \]=] [=[#elif defined(_WIN32) || defined(__moxnox__) || \
     (CURL_HAS_DECLSPEC_ATTRIBUTE(dllexport) && \]=])
file(CONFIGURE OUTPUT ${GEN}/include/curl/curl.h CONTENT "${_curl_h}" @ONLY)

foreach(_h curlver.h easy.h header.h mprintf.h multi.h options.h stdcheaders.h
        system.h typecheck-gcc.h urlapi.h websockets.h)
    configure_file(repo/include/curl/${_h} ${GEN}/include/curl/${_h} COPYONLY)
endforeach()

include_directories(BEFORE ${GEN}/include)
list(APPEND CMAKE_REQUIRED_INCLUDES ${GEN}/include)

set(BUILD_SHARED_LIBS ON)
set(BUILD_STATIC_LIBS OFF)
set(BUILD_CURL_EXE ON)
set(BUILD_TESTING OFF)
set(BUILD_EXAMPLES OFF)
set(BUILD_LIBCURL_DOCS OFF)
set(BUILD_MISC_DOCS OFF)
set(ENABLE_CURL_MANUAL OFF)
set(CURL_USE_PKGCONFIG OFF)
set(PICKY_COMPILER OFF)

# so that debug builds do not become libcurl-d.dll
set(CMAKE_DEBUG_POSTFIX "")

set(CURL_DISABLE_LDAP ON)
set(CURL_DISABLE_LDAPS ON)
set(CURL_DISABLE_HSTS ON)
set(ENABLE_UNIX_SOCKETS OFF)

set(CURL_CA_BUNDLE "/etc/ssl/cert.pem" CACHE STRING "" FORCE)
set(CURL_CA_PATH "none" CACHE STRING "" FORCE)

set(CURL_USE_OPENSSL ON)
set(OPENSSL_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/openssl/gen/include)
set(OPENSSL_SSL_LIBRARY ${CMAKE_BINARY_DIR}/ports/openssl/libssl.dll.a)
set(OPENSSL_CRYPTO_LIBRARY ${CMAKE_BINARY_DIR}/ports/openssl/libcrypto.dll.a)

file(MAKE_DIRECTORY ${OPENSSL_INCLUDE_DIR})
add_library(OpenSSL::SSL SHARED IMPORTED)
set_target_properties(OpenSSL::SSL PROPERTIES
    IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ports/openssl/libssl.dll
    IMPORTED_IMPLIB ${OPENSSL_SSL_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
)
add_library(OpenSSL::Crypto SHARED IMPORTED)
set_target_properties(OpenSSL::Crypto PROPERTIES
    IMPORTED_LOCATION ${CMAKE_BINARY_DIR}/ports/openssl/libcrypto.dll
    IMPORTED_IMPLIB ${OPENSSL_CRYPTO_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${OPENSSL_INCLUDE_DIR}
)

set(CURL_ZLIB ON)
set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libzlib.dll.a)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zlib/repo)

set(CURL_ZSTD ON)
set(ZSTD_LIBRARY ${CMAKE_BINARY_DIR}/ports/zstd/libzstd.dll.a)
set(ZSTD_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zstd/repo/lib)

set(CURL_BROTLI OFF)
set(USE_NGHTTP2 OFF)
set(USE_LIBIDN2 OFF)
set(CURL_USE_LIBPSL OFF)
set(CURL_USE_LIBSSH2 OFF)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
add_subdirectory(repo)

target_include_directories(libcurl_shared SYSTEM PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../openssl/repo/include)

target_include_directories(libcurl_shared BEFORE INTERFACE $<BUILD_INTERFACE:${GEN}/include>)
install(DIRECTORY ${GEN}/include/curl
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    FILES_MATCHING PATTERN "*.h")

target_compile_options(libcurl_shared PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-type-limits
    -Wno-unused-parameter
)

target_compile_options(curl PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-conversion
    -Wno-shorten-64-to-32
)

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies libcurl_shared libssl libcrypto zlib zstd)
