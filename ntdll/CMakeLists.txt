cmake_minimum_required(VERSION 3.28)

project(ntdll)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/ntdll.cpp
    src/ntdll.def
    src/heap.cpp
    src/except.cpp
    src/time.cpp
    src/stubs.S
    src/critsect.cpp
    src/error.cpp
    src/sync.cpp
    src/apc.cpp
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    list(APPEND SRC_FILES
        src/syscalls-aarch64.S
        src/continue-aarch64.S)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    list(APPEND SRC_FILES
        src/syscalls-x86.S
        src/continue-x86.S)
endif()

add_library(ntdll SHARED ${SRC_FILES})

target_sources(ntdll PUBLIC FILE_SET CXX_MODULES FILES
    src/ntdll_private.cpp
    src/loader.cpp
)

target_compile_options(ntdll PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(ntdll PRIVATE -nostartfiles -nostdlib -Wl,--gc-sections)
target_link_libraries(ntdll PRIVATE
    format_nobuiltin
    dwarf_nobuiltin
    list_nobuiltin
    unicode_nobuiltin
    nt_nobuiltin
    ntdll_mod_nobuiltin
    pe_nobuiltin
    crt
    # pagesize
    timepage_nobuiltin
    unique_handle_nobuiltin
    print_nobuiltin
    termios_nobuiltin
    exec_nobuiltin
    moxnox_ioctls_nobuiltin
    win32error_nobuiltin
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    target_link_libraries(ntdll PRIVATE aarch64)
endif()

install(TARGETS ntdll
    COMPONENT core
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)

add_custom_command(TARGET ntdll POST_BUILD
    COMMAND sh -c "! \"${CMAKE_OBJDUMP}\" -p \"$1\" | grep -q 'DLL Name' || { echo 'Error: ntdll has imports' >&2; exit 1; }" -- "$<TARGET_FILE:ntdll>"
    VERBATIM
    COMMENT "Checking ntdll has no imports"
)
