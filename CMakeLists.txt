cmake_minimum_required(VERSION 3.28)

set(CMAKE_USER_MAKE_RULES_OVERRIDE ${CMAKE_CURRENT_SOURCE_DIR}/toolchain.cmake)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(moxnox VERSION 1.0)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_CXX_VISIBILITY_PRESET "hidden")

set(CMAKE_C_STANDARD_LIBRARIES "")
set(CMAKE_CXX_STANDARD_LIBRARIES "")

string(APPEND CMAKE_EXE_LINKER_FLAGS " -static -nodefaultlibs")
string(APPEND CMAKE_SHARED_LINKER_FLAGS " -nodefaultlibs")

list(REMOVE_ITEM CMAKE_C_IMPLICIT_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/include")
list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "${CMAKE_SOURCE_DIR}/include")
include_directories(BEFORE SYSTEM ${CMAKE_SOURCE_DIR}/include)

if(LIBCXX_HEADERS)
    set(CMAKE_NO_SYSTEM_FROM_IMPORTED ON)
    list(REMOVE_ITEM CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES "${LIBCXX_HEADERS}")
    include_directories(BEFORE SYSTEM ${LIBCXX_HEADERS})
endif()

option(USE_COMPILER_RT OFF)

find_program(DLLTOOL NAMES dlltool llvm-dlltool REQUIRED)
find_program(AR NAMES ar llvm-ar REQUIRED)

add_compile_definitions(MOXNOX)
add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)
add_compile_definitions(-D_LIBCPP_VERBOSE_ABORT=)

# set _UCRT everywhere, but avoiding undisablable duplicate definition warnings in ucrtbase_static
add_compile_definitions("$<$<NOT:$<STREQUAL:$<TARGET_PROPERTY:NAME>,ucrtbase_static>>:_UCRT>")
add_compile_definitions(_FILE_OFFSET_BITS=64)

add_compile_options(-U_FORTIFY_SOURCE)

enable_language(ASM)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    string(APPEND CMAKE_ASM_FLAGS " -I${CMAKE_SOURCE_DIR}/modules/aarch64")
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    string(APPEND CMAKE_ASM_FLAGS " -I${CMAKE_SOURCE_DIR}/modules/amd64")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(GENERAL_CFLAGS -gdwarf-5 -march=armv8.1-a -fomit-frame-pointer -fno-strict-aliasing)
    set(USERSPACE_CFLAGS -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -Werror=return-type -fno-rtti -fcheck-new -Wno-invalid-offsetof -mno-stack-arg-probe -fno-threadsafe-statics -fvisibility=hidden ${GENERAL_CFLAGS})
    set(KERNEL_CFLAGS -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -Werror=return-type $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -Wno-invalid-offsetof -fno-threadsafe-statics> -fcheck-new -mno-stack-arg-probe -fvisibility=hidden -fno-stack-protector -fno-builtin ${GENERAL_CFLAGS})
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(GENERAL_CFLAGS -gdwarf-5 -fomit-frame-pointer -fno-strict-aliasing)
    set(USERSPACE_CFLAGS -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -Werror=return-type $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -Wno-invalid-offsetof -fno-threadsafe-statics> -fcheck-new -mno-stack-arg-probe -fvisibility=hidden -march=x86-64-v3 ${GENERAL_CFLAGS})

    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # FIXME - workaround for GCC bug 49001 messing up AVX2 and AVX512
        set(USERSPACE_CFLAGS ${USERSPACE_CFLAGS} -Wa,--muse-unaligned-vector-move)
    endif()

    set(KERNEL_CFLAGS -Wall -Wextra -Wno-address-of-packed-member -Wconversion -Wno-unknown-pragmas -Werror=pointer-arith -Werror=return-type $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti -Wno-invalid-offsetof -fno-threadsafe-statics> -mno-red-zone -fcheck-new -mno-stack-arg-probe -mno-sse -fvisibility=hidden -fno-stack-protector -masm=intel -fno-builtin ${GENERAL_CFLAGS})
endif()

add_library(format_kernel STATIC)
target_sources(format_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
target_compile_options(format_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(format_kernel nt_kernel)

add_library(dwarf_kernel STATIC)
target_sources(dwarf_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/dwarf.cpp)
target_compile_options(dwarf_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(dwarf_kernel format_kernel list_kernel)

add_library(list_kernel STATIC)
target_sources(list_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/list.cpp)
target_compile_options(list_kernel PRIVATE ${KERNEL_CFLAGS})

add_library(unicode_kernel STATIC)
target_sources(unicode_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/unicode.cpp)
target_compile_options(unicode_kernel PRIVATE ${KERNEL_CFLAGS})

add_library(rbtree_kernel STATIC)
target_sources(rbtree_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/rbtree.cpp)
target_compile_options(rbtree_kernel PRIVATE ${KERNEL_CFLAGS})

add_library(vt100_kernel STATIC)
target_sources(vt100_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/vt100.cpp)
target_compile_options(vt100_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(vt100_kernel PRIVATE format_kernel)

add_library(cp437_kernel STATIC)
target_sources(cp437_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/cp437.cpp)
target_compile_options(cp437_kernel PRIVATE ${KERNEL_CFLAGS})

add_library(evdev_kernel STATIC)
target_sources(evdev_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/evdev.cpp)
target_compile_options(evdev_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(evdev_kernel format_kernel)

add_library(nt_kernel STATIC)
target_sources(nt_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
target_compile_options(nt_kernel PRIVATE ${KERNEL_CFLAGS})

add_library(ntdll_kernel STATIC)
target_sources(ntdll_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/ntdll.cpp)
target_compile_options(ntdll_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(ntdll_kernel nt_kernel)
target_compile_definitions(ntdll_kernel PRIVATE KERNEL)

add_library(pe_kernel STATIC)
target_sources(pe_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/pe.cpp)
target_compile_options(pe_kernel PRIVATE ${KERNEL_CFLAGS})

add_library(crt_kernel STATIC)
if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    target_sources(crt_kernel PUBLIC modules/crt.S)
endif()
target_sources(crt_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/crt.cpp)
target_compile_options(crt_kernel PRIVATE ${KERNEL_CFLAGS} -fno-builtin)

add_library(pagesize_kernel STATIC)
target_sources(pagesize_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/pagesize.cpp)
target_compile_options(pagesize_kernel PRIVATE ${KERNEL_CFLAGS})

add_library(timepage_kernel STATIC)
target_sources(timepage_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/timepage.cpp)
target_compile_options(timepage_kernel PRIVATE ${KERNEL_CFLAGS})
target_compile_definitions(timepage_kernel PRIVATE KERNEL)
target_link_libraries(timepage_kernel pagesize_kernel ipv6_kernel)

add_library(moxnox_ioctls_kernel STATIC)
target_sources(moxnox_ioctls_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/moxnox_ioctls.cpp)
target_compile_options(moxnox_ioctls_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(moxnox_ioctls_kernel nt_kernel ipv6_kernel)

add_library(ws2_32_kernel STATIC)
target_sources(ws2_32_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/ws2_32.cpp)
target_compile_options(ws2_32_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(ws2_32_kernel format_kernel)

add_library(ipv6_kernel STATIC)
target_sources(ipv6_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/ipv6.cpp)
target_compile_options(ipv6_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(ipv6_kernel PRIVATE format_kernel ws2_32_kernel)

add_library(ipv4_kernel STATIC)
target_sources(ipv4_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/ipv4.cpp)
target_compile_options(ipv4_kernel PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(ipv4_kernel PRIVATE format_kernel)

add_library(termios_kernel STATIC)
target_sources(termios_kernel PUBLIC FILE_SET CXX_MODULES FILES modules/termios.cpp)
target_compile_options(termios_kernel PRIVATE -fno-exceptions ${KERNEL_CFLAGS})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    add_library(pe_32bit STATIC)
    target_sources(pe_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/pe.cpp)
    target_compile_options(pe_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(nt_32bit STATIC)
    target_sources(nt_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
    target_compile_options(nt_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(format_32bit STATIC)
    target_sources(format_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
    target_compile_options(format_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)
    target_link_libraries(format_32bit nt_32bit)

    add_library(loaderblock_32bit STATIC)
    target_sources(loaderblock_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/loaderblock.cpp)
    target_compile_options(loaderblock_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)
    target_link_libraries(loaderblock_32bit format_32bit)

    add_library(crt_32bit STATIC)
    target_sources(crt_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/crt.cpp)
    target_compile_options(crt_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti -fno-stack-protector -fno-builtin)

    add_library(x86 STATIC)
    target_sources(x86 PUBLIC FILE_SET CXX_MODULES FILES modules/x86.cpp)
    target_compile_options(x86 PRIVATE ${KERNEL_CFLAGS})

    add_library(x86_32bit STATIC)
    target_sources(x86_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/x86.cpp)
    target_compile_options(x86_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(pagesize_32bit STATIC)
    target_sources(pagesize_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/pagesize.cpp)
    target_compile_options(pagesize_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)

    add_library(smbios_32bit STATIC)
    target_sources(smbios_32bit PUBLIC FILE_SET CXX_MODULES FILES modules/smbios.cpp)
    target_compile_options(smbios_32bit PRIVATE -march=i386 -m32 -fno-ident -fno-pic -fno-exceptions -fno-rtti)
endif()

add_library(format STATIC)
target_sources(format PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
target_compile_options(format PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(format PRIVATE nt)

add_library(format-rtti STATIC)
target_sources(format-rtti PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
target_compile_options(format-rtti PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -frtti)
target_link_libraries(format-rtti PRIVATE nt-rtti)

add_library(format-exc STATIC)
target_sources(format-exc PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
target_compile_options(format-exc PRIVATE ${USERSPACE_CFLAGS})
target_link_libraries(format-exc PRIVATE nt-exc)

add_library(format_nobuiltin STATIC)
target_sources(format_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
target_compile_options(format_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(format_nobuiltin PRIVATE nt_nobuiltin)

add_library(dwarf STATIC)
target_sources(dwarf PUBLIC FILE_SET CXX_MODULES FILES modules/dwarf.cpp)
target_compile_options(dwarf PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(dwarf PRIVATE format list)

add_library(dwarf_nobuiltin STATIC)
target_sources(dwarf_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/dwarf.cpp)
target_compile_options(dwarf_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(dwarf_nobuiltin PRIVATE format_nobuiltin list_nobuiltin)

add_library(list STATIC)
target_sources(list PUBLIC FILE_SET CXX_MODULES FILES modules/list.cpp)
target_compile_options(list PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(list_nobuiltin STATIC)
target_sources(list_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/list.cpp)
target_compile_options(list_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})

add_library(unicode STATIC)
target_sources(unicode PUBLIC FILE_SET CXX_MODULES FILES modules/unicode.cpp)
target_compile_options(unicode PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(unicode_nobuiltin STATIC)
target_sources(unicode_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/unicode.cpp)
target_compile_options(unicode_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})

add_library(rbtree STATIC)
target_sources(rbtree PUBLIC FILE_SET CXX_MODULES FILES modules/rbtree.cpp)
target_compile_options(rbtree PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(unique_handle STATIC)
target_sources(unique_handle PUBLIC FILE_SET CXX_MODULES FILES modules/unique_handle.cpp)
target_compile_options(unique_handle PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(unique_handle PRIVATE nt)

add_library(unique_handle_nobuiltin STATIC)
target_sources(unique_handle_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/unique_handle.cpp)
target_compile_options(unique_handle_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(unique_handle_nobuiltin PRIVATE nt_nobuiltin)

add_library(vt100 STATIC)
target_sources(vt100 PUBLIC FILE_SET CXX_MODULES FILES modules/vt100.cpp)
target_compile_options(vt100 PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(vt100 PRIVATE format)

add_library(cp437 STATIC)
target_sources(cp437 PUBLIC FILE_SET CXX_MODULES FILES modules/cp437.cpp)
target_compile_options(cp437 PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(evdev STATIC)
target_sources(evdev PUBLIC FILE_SET CXX_MODULES FILES modules/evdev.cpp)
target_compile_options(evdev PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(evdev format)

add_library(nt STATIC)
target_sources(nt PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
target_compile_options(nt PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(nt-rtti STATIC)
target_sources(nt-rtti PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
target_compile_options(nt-rtti PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -frtti)

add_library(nt-exc STATIC)
target_sources(nt-exc PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
target_compile_options(nt-exc PRIVATE ${USERSPACE_CFLAGS})

add_library(nt_nobuiltin STATIC)
target_sources(nt_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
target_compile_options(nt_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})

add_library(ntdll_mod STATIC)
target_sources(ntdll_mod PUBLIC FILE_SET CXX_MODULES FILES modules/ntdll.cpp)
target_compile_options(ntdll_mod PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ntdll_mod nt)

add_library(ntdll_mod_nobuiltin STATIC)
target_sources(ntdll_mod_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/ntdll.cpp)
target_compile_options(ntdll_mod_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ntdll_mod_nobuiltin nt_nobuiltin)

add_library(pe STATIC)
target_sources(pe PUBLIC FILE_SET CXX_MODULES FILES modules/pe.cpp)
target_compile_options(pe PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(pe_nobuiltin STATIC)
target_sources(pe_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/pe.cpp)
target_compile_options(pe_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})

add_library(loaderblock STATIC)
target_sources(loaderblock PUBLIC FILE_SET CXX_MODULES FILES modules/loaderblock.cpp)
target_compile_options(loaderblock PRIVATE ${KERNEL_CFLAGS})
target_link_libraries(loaderblock gnu-efi format_kernel)

add_library(kernelbase_mod STATIC)
target_sources(kernelbase_mod PUBLIC FILE_SET CXX_MODULES FILES modules/kernelbase.cpp)
target_compile_options(kernelbase_mod PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(kernelbase_mod nt ntdll_mod)

add_library(kernelbase_mod_nobuiltin STATIC)
target_sources(kernelbase_mod_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/kernelbase.cpp)
target_compile_options(kernelbase_mod_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(kernelbase_mod_nobuiltin nt_nobuiltin ntdll_mod_nobuiltin)

add_library(ucrtbase_mod STATIC)
target_link_libraries(ucrtbase_mod PUBLIC ucrtbase_static)
target_sources(ucrtbase_mod PUBLIC FILE_SET CXX_MODULES FILES modules/ucrtbase.cpp)
target_compile_options(ucrtbase_mod PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(ucrtbase_mod_nobuiltin STATIC)
target_link_libraries(ucrtbase_mod_nobuiltin PUBLIC ucrtbase_static)
target_sources(ucrtbase_mod_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/ucrtbase.cpp)
target_compile_options(ucrtbase_mod_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})

add_library(ws2_32_mod STATIC)
target_sources(ws2_32_mod PUBLIC FILE_SET CXX_MODULES FILES modules/ws2_32.cpp)
target_compile_options(ws2_32_mod PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ws2_32_mod format)

add_library(ws2_32_mod_nobuiltin STATIC)
target_sources(ws2_32_mod_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/ws2_32.cpp)
target_compile_options(ws2_32_mod_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ws2_32_mod_nobuiltin format_nobuiltin)

add_library(crt STATIC)
target_sources(crt PUBLIC FILE_SET CXX_MODULES FILES modules/crt.cpp)
target_compile_options(crt PRIVATE -fno-exceptions -fno-builtin ${USERSPACE_CFLAGS})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    target_sources(crt PUBLIC modules/crt.S)
endif()

add_library(pagesize STATIC)
target_sources(pagesize PUBLIC FILE_SET CXX_MODULES FILES modules/pagesize.cpp)
target_compile_options(pagesize PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(pagesize_nobuiltin STATIC)
target_sources(pagesize_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/pagesize.cpp)
target_compile_options(pagesize_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})

add_library(timepage STATIC)
target_sources(timepage PUBLIC FILE_SET CXX_MODULES FILES modules/timepage.cpp)
target_compile_options(timepage PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(timepage pagesize ipv6)

add_library(timepage_nobuiltin STATIC)
target_sources(timepage_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/timepage.cpp)
target_compile_options(timepage_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(timepage_nobuiltin pagesize_nobuiltin ipv6_nobuiltin)

add_library(moxnox_ioctls STATIC)
target_sources(moxnox_ioctls PUBLIC FILE_SET CXX_MODULES FILES modules/moxnox_ioctls.cpp)
target_compile_options(moxnox_ioctls PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(moxnox_ioctls nt ipv6)

add_library(moxnox_ioctls_nobuiltin STATIC)
target_sources(moxnox_ioctls_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/moxnox_ioctls.cpp)
target_compile_options(moxnox_ioctls_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(moxnox_ioctls_nobuiltin nt_nobuiltin ipv6_nobuiltin)

add_library(ipv6 STATIC)
target_sources(ipv6 PUBLIC FILE_SET CXX_MODULES FILES modules/ipv6.cpp)
target_compile_options(ipv6 PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ipv6 format ws2_32_mod)

add_library(ipv6_nobuiltin STATIC)
target_sources(ipv6_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/ipv6.cpp)
target_compile_options(ipv6_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ipv6_nobuiltin format_nobuiltin ws2_32_mod_nobuiltin)

add_library(ipv4 STATIC)
target_sources(ipv4 PUBLIC FILE_SET CXX_MODULES FILES modules/ipv4.cpp)
target_compile_options(ipv4 PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ipv4 format)

add_library(ipv4_nobuiltin STATIC)
target_sources(ipv4_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/ipv4.cpp)
target_compile_options(ipv4_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ipv4_nobuiltin format_nobuiltin)

add_library(print STATIC)
target_sources(print PUBLIC FILE_SET CXX_MODULES FILES modules/print.cpp)
target_compile_options(print PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(print PRIVATE nt format)

add_library(print-rtti STATIC)
target_sources(print-rtti PUBLIC FILE_SET CXX_MODULES FILES modules/print.cpp)
target_compile_options(print-rtti PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -frtti)
target_link_libraries(print-rtti PRIVATE nt-rtti format-rtti)

add_library(print-exc STATIC)
target_sources(print-exc PUBLIC FILE_SET CXX_MODULES FILES modules/print.cpp)
target_compile_options(print-exc PRIVATE ${USERSPACE_CFLAGS})
target_link_libraries(print-exc PRIVATE nt-exc format-exc)

add_library(print_nobuiltin STATIC)
target_sources(print_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/print.cpp)
target_compile_options(print_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(print_nobuiltin PRIVATE nt_nobuiltin format_nobuiltin)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_library(format-ms STATIC)
    target_sources(format-ms PUBLIC FILE_SET CXX_MODULES FILES modules/format.cpp)
    target_compile_options(format-ms PRIVATE -fno-exceptions -fms-extensions ${USERSPACE_CFLAGS})
    target_link_libraries(format-ms PRIVATE nt-ms)

    add_library(nt-ms STATIC)
    target_sources(nt-ms PUBLIC FILE_SET CXX_MODULES FILES modules/nt.cpp)
    target_compile_options(nt-ms PRIVATE -fno-exceptions -fms-extensions ${USERSPACE_CFLAGS})

    add_library(print-ms STATIC)
    target_sources(print-ms PUBLIC FILE_SET CXX_MODULES FILES modules/print.cpp)
    target_compile_options(print-ms PRIVATE -fno-exceptions -fms-extensions ${USERSPACE_CFLAGS})
    target_link_libraries(print-ms PRIVATE nt-ms format-ms)
endif()

add_library(errno STATIC)
target_sources(errno PUBLIC FILE_SET CXX_MODULES FILES modules/errno.cpp)
target_compile_options(errno PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(errno PRIVATE nt)

add_library(errno_nobuiltin STATIC)
target_sources(errno_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/errno.cpp)
target_compile_options(errno_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(errno_nobuiltin PRIVATE nt_nobuiltin)

add_library(smbios STATIC)
target_sources(smbios PUBLIC FILE_SET CXX_MODULES FILES modules/smbios.cpp)
target_compile_options(smbios PRIVATE ${KERNEL_CFLAGS})

add_library(termios STATIC)
target_sources(termios PUBLIC FILE_SET CXX_MODULES FILES modules/termios.cpp)
target_compile_options(termios PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(termios_nobuiltin STATIC)
target_sources(termios_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/termios.cpp)
target_compile_options(termios_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})

add_library(exec STATIC)
target_sources(exec PUBLIC FILE_SET CXX_MODULES FILES modules/exec.cpp)
target_compile_options(exec PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -ffunction-sections)
target_link_libraries(exec PRIVATE nt unique_handle errno unicode pagesize ntdll_mod list)

add_library(exec_nobuiltin STATIC)
target_sources(exec_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/exec.cpp)
target_compile_options(exec_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS} -ffunction-sections)
target_link_libraries(exec_nobuiltin PRIVATE nt_nobuiltin unique_handle_nobuiltin errno_nobuiltin unicode_nobuiltin pagesize_nobuiltin ntdll_mod_nobuiltin list_nobuiltin)

add_library(win32error STATIC)
target_sources(win32error PUBLIC FILE_SET CXX_MODULES FILES modules/win32error.cpp)
target_compile_options(win32error PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

add_library(win32error_nobuiltin STATIC)
target_sources(win32error_nobuiltin PUBLIC FILE_SET CXX_MODULES FILES modules/win32error.cpp)
target_compile_options(win32error_nobuiltin PRIVATE -fno-builtin -fno-exceptions ${USERSPACE_CFLAGS})

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    add_library(aarch64 STATIC)
    target_sources(aarch64 PUBLIC FILE_SET CXX_MODULES FILES modules/aarch64.cpp)
    target_compile_options(aarch64 PRIVATE ${KERNEL_CFLAGS})
    target_link_libraries(aarch64 format_kernel)
endif()

include(check-git.cmake)
check_git_setup()

add_subdirectory(kernel)
add_subdirectory(ntdll)
add_subdirectory(init)
add_subdirectory(libmoxnox)
add_subdirectory(kernelbase)
add_subdirectory(mnposix)
add_subdirectory(ucrtbase)
add_subdirectory(usercon)
add_subdirectory(shell)
add_subdirectory(ws2_32)
add_subdirectory(iphlpapi)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    add_subdirectory(bootloader/aarch64)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    add_subdirectory(bootloader/x86)
endif()

find_program(HOST_SED sed REQUIRED)
find_program(HOST_PATCH patch REQUIRED)
find_program(HOST_MAKE make REQUIRED)
find_program(HOST_FLEX flex REQUIRED)
find_program(HOST_GPERF gperf REQUIRED)
find_program(HOST_BISON bison REQUIRED)
find_program(HOST_M4 m4 REQUIRED)
find_program(HOST_TIC tic REQUIRED)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Moxnox")
    set(HOST_EXECUTABLE_SUFFIX .exe)
else()
    set(HOST_EXECUTABLE_SUFFIX "")
endif()

file(GLOB port_cmake_files ${CMAKE_CURRENT_SOURCE_DIR}/ports/*/CMakeLists.txt)
foreach(cmake_file ${port_cmake_files})
    get_filename_component(port_dir ${cmake_file} DIRECTORY)
    if(EXISTS ${port_dir}/repo/.git)
        add_subdirectory(${port_dir})
    endif()
endforeach()

function(add_moxnox_deps dir)
    get_property(targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)

    foreach(target IN LISTS targets)
        get_target_property(type ${target} TYPE)

        if(type STREQUAL "EXECUTABLE" OR type STREQUAL "SHARED_LIBRARY")
            add_dependencies(${target} moxnox ucrtbase mingwex mingw32)
        endif()
    endforeach()

    get_property(subdirs DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)

    foreach(subdir IN LISTS subdirs)
        add_moxnox_deps(${subdir})
    endforeach()
endfunction()

add_moxnox_deps(${CMAKE_SOURCE_DIR})

set(MOXNOX_CORE_LIBRARIES
    ${CMAKE_BINARY_DIR}/libmoxnox/libiphlpapi.a
    ${CMAKE_BINARY_DIR}/libmoxnox/libkernelbase.a
    $<TARGET_FILE:mingw32>
    $<TARGET_FILE:mingwex>
    ${CMAKE_BINARY_DIR}/libmoxnox/libmnposix.a
    ${CMAKE_BINARY_DIR}/libmoxnox/libmoxnox.a
    ${CMAKE_BINARY_DIR}/libmoxnox/libntdll.a
    ${CMAKE_BINARY_DIR}/libmoxnox/libucrtbase.a
    $<TARGET_FILE:ucrtbase_static>
    ${CMAKE_BINARY_DIR}/libmoxnox/libws2_32.a
    ${CMAKE_BINARY_DIR}/ports/mingw-w64/crt2.o
    ${CMAKE_BINARY_DIR}/ports/mingw-w64/dllcrt2.o
    ${CMAKE_BINARY_DIR}/ports/mingw-w64/gcrt2.o
    ${CMAKE_BINARY_DIR}/ports/mingw-w64/crtbegin.o
    ${CMAKE_BINARY_DIR}/ports/mingw-w64/crtend.o
)

install(FILES ${MOXNOX_CORE_LIBRARIES} DESTINATION lib COMPONENT core)

install(DIRECTORY ${CMAKE_SOURCE_DIR}/include/ DESTINATION include COMPONENT core)

configure_file(${CMAKE_SOURCE_DIR}/config.site.in ${CMAKE_BINARY_DIR}/config.site @ONLY)
install(FILES ${CMAKE_BINARY_DIR}/config.site DESTINATION share COMPONENT core)

install(DIRECTORY DESTINATION root COMPONENT core
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE)
install(DIRECTORY DESTINATION tmp COMPONENT core
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                              GROUP_READ GROUP_WRITE GROUP_EXECUTE
                              WORLD_READ WORLD_WRITE WORLD_EXECUTE)
install(DIRECTORY DESTINATION dev COMPONENT core
        DIRECTORY_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                              GROUP_READ GROUP_EXECUTE
                              WORLD_READ WORLD_EXECUTE)

install(SCRIPT initrd.cmake COMPONENT core)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/gdbinit.in ${CMAKE_CURRENT_BINARY_DIR}/.gdbinit)
