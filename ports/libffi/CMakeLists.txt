set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

file(STRINGS repo/configure.ac _ac_init REGEX "^AC_INIT\\(\\[libffi\\],\\[")
string(REGEX REPLACE "^AC_INIT\\(\\[libffi\\],\\[([0-9.]+)\\].*" "\\1" PACKAGE_VERSION "${_ac_init}")
set(VERSION ${PACKAGE_VERSION})
file(STRINGS repo/configure.ac _version_string REGEX "^FFI_VERSION_STRING=")
string(REGEX REPLACE "^FFI_VERSION_STRING=\"(.*)\"" "\\1" FFI_VERSION_STRING "${_version_string}")
file(STRINGS repo/configure.ac _version_number REGEX "^FFI_VERSION_NUMBER=")
string(REGEX REPLACE "^FFI_VERSION_NUMBER=(.*)" "\\1" FFI_VERSION_NUMBER "${_version_number}")

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(TARGET X86_WIN64)
    add_custom_command(
        OUTPUT ${GEN}/include/ffitarget.h
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/x86/ffitarget.h ${GEN}/include/ffitarget.h
        COMMAND ${HOST_SED} -i
            -e [[s/^#ifdef __GNUC__$/#if defined(__GNUC__) \&\& !defined(__moxnox__)/]]
            ${GEN}/include/ffitarget.h
        DEPENDS repo/src/x86/ffitarget.h
        VERBATIM
    )
    set(FFI_ARCH_SOURCES
        repo/src/x86/ffiw64.c
        repo/src/x86/win64.S
    )
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(TARGET ARM_WIN64)
    add_custom_command(
        OUTPUT ${GEN}/include/ffitarget.h
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/aarch64/ffitarget.h ${GEN}/include/ffitarget.h
        COMMAND ${HOST_SED} -i
            -e [[s/defined(_WIN32)/(defined(_WIN32) || defined(__moxnox__))/g]]
            -e [[s/^#ifdef _WIN32$/#if defined(_WIN32) || defined(__moxnox__)/]]
            -e [[s/^#ifndef _WIN32$/#if !defined(_WIN32) \&\& !defined(__moxnox__)/]]
            ${GEN}/include/ffitarget.h
        DEPENDS repo/src/aarch64/ffitarget.h
        VERBATIM
    )
    set(FFI_ARCH_SOURCES
        repo/src/aarch64/ffi.c
        repo/src/aarch64/sysv.S
    )
endif()

add_custom_command(
    OUTPUT ${GEN}/src/prep_cif.c
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/prep_cif.c ${GEN}/src/prep_cif.c
    COMMAND ${HOST_SED} -i
        -e [[s/&& defined(_WIN32)$/\&\& (defined(_WIN32) || defined(__moxnox__))/]]
        ${GEN}/src/prep_cif.c
    DEPENDS repo/src/prep_cif.c
    VERBATIM
)

set(HAVE_LONG_DOUBLE 0)
set(FFI_EXEC_TRAMPOLINE_TABLE 0)
configure_file(repo/include/ffi.h.in ${GEN}/include/ffi.h @ONLY)

add_library(ffi SHARED
    ${GEN}/src/prep_cif.c
    repo/src/types.c
    repo/src/raw_api.c
    repo/src/java_raw_api.c
    repo/src/closures.c
    repo/src/tramp.c
    ${FFI_ARCH_SOURCES}
    ${GEN}/include/ffitarget.h
)

target_include_directories(ffi PUBLIC ${GEN}/include)
target_include_directories(ffi PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} repo/include)
target_compile_options(ffi PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default)

target_compile_options(ffi PRIVATE
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-unused-parameter
    -Wno-deprecated-declarations
    $<$<C_COMPILER_ID:Clang>:-Wno-gnu-null-pointer-arithmetic>
)

install(TARGETS ffi DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES ${GEN}/include/ffi.h ${GEN}/include/ffitarget.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES repo/man/ffi.3 repo/man/ffi_call.3 repo/man/ffi_prep_cif.3 repo/man/ffi_prep_cif_var.3
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man3)

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "${CMAKE_INSTALL_PREFIX}")
set(libdir /lib)
set(toolexeclibdir /lib)
set(includedir /include)
set(PACKAGE_NAME libffi)
configure_file(repo/libffi.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libffi.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libffi.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
