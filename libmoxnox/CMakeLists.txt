cmake_minimum_required(VERSION 3.28)

project(moxnox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(DLLS
    ntdll
    ucrtbase
    ws2_32
    iphlpapi
    mnposix
)

add_library(moxnox STATIC IMPORTED GLOBAL)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    set(ARCH arm64)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(ARCH "i386:x86-64")
endif()

foreach(DLL ${DLLS})
    add_custom_command(
        OUTPUT lib${DLL}.a
        COMMAND ${DLLTOOL} -d ${CMAKE_CURRENT_SOURCE_DIR}/${DLL}.def -l ${CMAKE_CURRENT_BINARY_DIR}/lib${DLL}.a -m ${ARCH}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${DLL}.def
    )

    list(APPEND DEPS "lib${DLL}.a")
endforeach()

add_custom_command(
    OUTPUT libmoxnox.a
    COMMAND ${AR} -M < ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.mri
    DEPENDS ${DEPS} ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.mri
)

add_custom_target(moxnox_target ALL DEPENDS libmoxnox.a)

add_dependencies(moxnox moxnox_target)
set_target_properties(moxnox PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/libmoxnox.a)

target_link_libraries(moxnox INTERFACE nt ntdll_mod ucrtbase_mod moxnox_ioctls ws2_32_mod)
