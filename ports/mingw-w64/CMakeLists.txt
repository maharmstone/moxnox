set(DEFAULT_MSVCRT_VERSION 0x600)
set(DEFAULT_WIN32_WINNT 0xa00)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/repo/mingw-w64-headers/crt/_mingw.h.in ${CMAKE_CURRENT_BINARY_DIR}/_mingw.h)
unset(DEFAULT_WIN32_WINNT)
unset(DEFAULT_MSVCRT_VERSION)

add_library(ucrtbase_static STATIC
    repo/mingw-w64-crt/misc/ucrt_amsg_exit.c
    repo/mingw-w64-crt/misc/ucrt__getmainargs.c
    repo/mingw-w64-crt/misc/__set_app_type.c
    repo/mingw-w64-crt/misc/__p___initenv.c
    repo/mingw-w64-crt/misc/__initenv.c
    repo/mingw-w64-crt/misc/_onexit.c
    repo/mingw-w64-crt/stdio/ucrt_fprintf.c
    repo/mingw-w64-crt/stdio/ucrt_ms_fwprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vfprintf.c
    repo/mingw-w64-crt/stdio/ucrt_snprintf.c
    repo/mingw-w64-crt/stdio/ucrt_sprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vsprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vprintf.c
    repo/mingw-w64-crt/stdio/ucrt_sscanf.c
    repo/mingw-w64-crt/stdio/ucrt_fscanf.c
    repo/mingw-w64-crt/stdio/ucrt_scanf.c
    repo/mingw-w64-crt/stdio/ucrt_vfscanf.c
    repo/mingw-w64-crt/stdio/ucrt_vscanf.c
    repo/mingw-w64-crt/stdio/ucrt_vsscanf.c
    repo/mingw-w64-crt/stdio/ucrt_swprintf.c
    repo/mingw-w64-crt/stdio/ucrt__vscprintf.c
    repo/mingw-w64-crt/stdio/ucrt_vsnprintf.c
    repo/mingw-w64-crt/stdio/ucrt__vsnprintf.c
    repo/mingw-w64-crt/stdio/ucrt___local_stdio_printf_options.c
    repo/mingw-w64-crt/stdio/ucrt___local_stdio_scanf_options.c
    repo/mingw-w64-crt/stdio/ucrt_printf.c
    in6addr.cpp
    redirects.S
)
target_include_directories(ucrtbase_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR} repo/mingw-w64-headers/crt repo/mingw-w64-crt/include repo/mingw-w64-headers/include)

target_compile_definitions(ucrtbase_static PRIVATE -D_CRTIMP= -D_WIN32 -D_WIN64)
target_compile_options(ucrtbase_static PRIVATE -ffreestanding -fno-stack-protector)
target_compile_options(ucrtbase_static PRIVATE -Wno-include-next-absolute-path)

###

add_library(mingwex STATIC
    repo/mingw-w64-crt/misc/dllmain.c
    repo/mingw-w64-crt/math/fpclassify.c
    repo/mingw-w64-crt/math/fpclassifyl.c
    repo/mingw-w64-crt/math/isnan.c
    repo/mingw-w64-crt/math/isnanl.c
    repo/mingw-w64-crt/misc/feclearexcept.c
    repo/mingw-w64-crt/misc/fegetenv.c
    repo/mingw-w64-crt/misc/fegetround.c
    repo/mingw-w64-crt/misc/feraiseexcept.c
    repo/mingw-w64-crt/misc/fesetenv.c
    repo/mingw-w64-crt/misc/fesetround.c
    repo/mingw-w64-crt/misc/fetestexcept.c
    repo/mingw-w64-crt/misc/mingw_controlfp.c
    repo/mingw-w64-crt/misc/mingw_setfp.c
    repo/mingw-w64-crt/misc/getopt.c
    repo/mingw-w64-crt/misc/mingw_matherr.c
    repo/mingw-w64-crt/misc/wmemchr.c
    repo/mingw-w64-crt/misc/wmemcmp.c
    repo/mingw-w64-crt/misc/wmemcpy.c
    repo/mingw-w64-crt/misc/wmemmove.c
    repo/mingw-w64-crt/misc/wmemset.c
    stack_chk_fail.cpp
    stack_chk_guard.cpp
    repo/mingw-w64-crt/stdio/strtok_r.c
    repo/mingw-w64-crt/stdio/_Exit.c
    repo/mingw-w64-crt/stdio/asprintf.c
    repo/mingw-w64-crt/stdio/vasprintf.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    target_sources(mingwex PRIVATE repo/mingw-w64-crt/cfguard/guard_dispatch.S)
endif()

if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -print-resource-dir
        OUTPUT_VARIABLE CLANG_RESOURCE_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/float.h CONTENT
"#pragma once
#include \"${CLANG_RESOURCE_DIR}/include/float.h\"
#include \"${CMAKE_CURRENT_SOURCE_DIR}/repo/mingw-w64-headers/crt/float.h\"
")
endif()

target_include_directories(mingwex PRIVATE ${CMAKE_CURRENT_BINARY_DIR} repo/mingw-w64-headers/crt repo/mingw-w64-crt/include repo/mingw-w64-headers/include)

target_compile_definitions(mingwex PRIVATE -D_CRTIMP= -D_UCRT -D_CRTBLD -D_WIN32 -D_WIN64 -D__MINGW32__)
target_compile_options(mingwex PRIVATE ${USERSPACE_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions> -mno-stack-arg-probe -fno-stack-protector $<$<CXX_COMPILER_ID:Clang>:-Wno-pragma-pack -Wno-include-next-absolute-path>)
target_compile_options(mingwex PRIVATE -Wno-shorten-64-to-32 -Wno-missing-declarations -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-unused-variable -Wno-gnu-pointer-arith -Wno-implicit-float-conversion -Wno-float-conversion -Wno-conversion -Wno-pointer-arith $<$<CXX_COMPILER_ID:GNU>:-Wno-use-after-free -Wno-missing-attributes>)
target_link_libraries(mingwex PRIVATE ntdll_mod)

###

add_library(mingw32 STATIC
    repo/mingw-w64-crt/crt/gccmain.c
    repo/mingw-w64-crt/crt/natstart.c
    repo/mingw-w64-crt/crt/wildcard.c
    repo/mingw-w64-crt/crt/dllargv.c
    repo/mingw-w64-crt/crt/_newmode.c
    repo/mingw-w64-crt/crt/tlssup.c
    repo/mingw-w64-crt/crt/xncommod.c
    repo/mingw-w64-crt/crt/cinitexe.c
    repo/mingw-w64-crt/crt/merr.c
    repo/mingw-w64-crt/crt/pesect.c
    repo/mingw-w64-crt/crt/xthdloc.c
    repo/mingw-w64-crt/crt/mingw_helpers.c
    repo/mingw-w64-crt/crt/pseudo-reloc.c
    repo/mingw-w64-crt/crt/usermatherr.c
    repo/mingw-w64-crt/crt/xtxtmode.c
    repo/mingw-w64-crt/crt/crt_handler.c
    tlsthrd.cpp
    repo/mingw-w64-crt/crt/tlsmcrt.c
    repo/mingw-w64-crt/crt/cxa_thread_atexit.c
    tls_atexit.cpp
)

target_compile_definitions(mingw32 PRIVATE -D_CRTIMP= -D_UCRT -D_CRTBLD -D_WIN32 -D_WIN64 -D__MINGW32__)
target_include_directories(mingw32 PRIVATE ${CMAKE_CURRENT_BINARY_DIR} repo/mingw-w64-headers/crt repo/mingw-w64-crt/include repo/mingw-w64-headers/include)
target_compile_options(mingw32 PRIVATE ${USERSPACE_CFLAGS} -fno-stack-protector $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions>)
target_link_libraries(mingw32 PRIVATE nt_mod ntdll_mod kernelbase_mod)

###

set(crt2_src repo/mingw-w64-crt/crt/crtexe.c)
set(dllcrt2_src repo/mingw-w64-crt/crt/crtdll.c)
set(gcrt2_src repo/mingw-w64-crt/profile/gcrt0.c)
set(crtbegin_src repo/mingw-w64-crt/crt/crtbegin.c)
set(crtend_src repo/mingw-w64-crt/crt/crtend.c)

foreach(crtobj crt2 gcrt2 crtbegin crtend)
    add_library(${crtobj}_obj OBJECT ${${crtobj}_src})

    target_compile_definitions(${crtobj}_obj PRIVATE -D_CRTIMP= -D_UCRT -D_CRTBLD -D_WIN32 -D_WIN64 -D__MINGW32__)
    target_include_directories(${crtobj}_obj PRIVATE ${CMAKE_CURRENT_BINARY_DIR} repo/mingw-w64-headers/crt repo/mingw-w64-crt/include repo/mingw-w64-headers/include)
    target_compile_options(${crtobj}_obj PRIVATE -fno-stack-protector)

    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${crtobj}.o
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_OBJECTS:${crtobj}_obj> ${CMAKE_CURRENT_BINARY_DIR}/${crtobj}.o
        DEPENDS ${crtobj}_obj $<TARGET_OBJECTS:${crtobj}_obj>
        VERBATIM
    )

    add_custom_target(${crtobj} ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${crtobj}.o)
endforeach()

###

add_library(dllcrt2_obj OBJECT ${dllcrt2_src})

target_compile_definitions(dllcrt2_obj PRIVATE -D_CRTIMP= -D_UCRT -D_CRTBLD -D_WIN32 -D_WIN64 -D__MINGW32__)
target_include_directories(dllcrt2_obj PRIVATE ${CMAKE_CURRENT_BINARY_DIR} repo/mingw-w64-headers/crt repo/mingw-w64-crt/include repo/mingw-w64-headers/include)
target_compile_options(dllcrt2_obj PRIVATE -fno-stack-protector)

if(NOT CMAKE_NM)
    message(FATAL_ERROR "CMAKE_NM is not set; needed to generate the exclude-symbols list")
endif()

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -print-file-name=libgcc.a
        OUTPUT_VARIABLE EXCLUDE_SYMBOLS_LIBGCC
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND ${CMAKE_C_COMPILER} -print-file-name=libgcc_eh.a
        OUTPUT_VARIABLE EXCLUDE_SYMBOLS_LIBGCC_EH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    set(EXCLUDE_SYMBOLS_COMPILER_LIBS ${EXCLUDE_SYMBOLS_LIBGCC} ${EXCLUDE_SYMBOLS_LIBGCC_EH})
else()
    set(EXCLUDE_SYMBOLS_COMPILER_LIBS "")
endif()
list(JOIN EXCLUDE_SYMBOLS_COMPILER_LIBS " " EXCLUDE_SYMBOLS_COMPILER_LIBS_STR)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/exclude-symbols.s
    COMMAND sh -c "{ printf '.section .drectve\\n' && ${CMAKE_NM} -g --defined-only $<TARGET_FILE:mingw32> $<TARGET_FILE:mingwex> $<TARGET_FILE:ucrtbase_static> ${EXCLUDE_SYMBOLS_COMPILER_LIBS_STR} | awk 'NF==3 { print $3 }' | sort -u | awk '{ printf \" .ascii \\\" -exclude-symbols:%s\\\"\\n\", $1 }' ; } > ${CMAKE_CURRENT_BINARY_DIR}/exclude-symbols.s"
    DEPENDS mingw32 mingwex ucrtbase_static $<TARGET_FILE:mingw32> $<TARGET_FILE:mingwex> $<TARGET_FILE:ucrtbase_static> ${EXCLUDE_SYMBOLS_COMPILER_LIBS}
    VERBATIM
)

add_library(exclude_symbols_obj OBJECT ${CMAKE_CURRENT_BINARY_DIR}/exclude-symbols.s)

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2.o
    COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_OBJECTS:dllcrt2_obj> ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2.o
    COMMAND sh -c ": > ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2-drectve.bin ; ${CMAKE_OBJCOPY} --dump-section .drectve=${CMAKE_CURRENT_BINARY_DIR}/dllcrt2-drectve.bin $<TARGET_OBJECTS:dllcrt2_obj> ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2-scratch.o 2>/dev/null || true"
    COMMAND ${CMAKE_OBJCOPY} --dump-section .drectve=${CMAKE_CURRENT_BINARY_DIR}/exclude-symbols.bin $<TARGET_OBJECTS:exclude_symbols_obj> ${CMAKE_CURRENT_BINARY_DIR}/exclude-symbols-scratch.o
    COMMAND sh -c "cat ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2-drectve.bin ${CMAKE_CURRENT_BINARY_DIR}/exclude-symbols.bin > ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2-drectve-combined.bin"
    COMMAND ${CMAKE_OBJCOPY} --remove-section .drectve ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2.o
    COMMAND ${CMAKE_OBJCOPY} --add-section .drectve=${CMAKE_CURRENT_BINARY_DIR}/dllcrt2-drectve-combined.bin ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2.o
    DEPENDS dllcrt2_obj $<TARGET_OBJECTS:dllcrt2_obj> exclude_symbols_obj $<TARGET_OBJECTS:exclude_symbols_obj>
    VERBATIM
)

add_custom_target(dllcrt2 ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/dllcrt2.o)

###

set(MINGW_COMPAT_CRT
    _bsd_types.h
    corecrt.h
    crtdefs.h
    excpt.h
    malloc.h
    _mingw_mac.h
    _mingw_secapi.h
    _mingw_unicode.h
    sec_api/stralign_s.h
    _timeval.h
    vadefs.h
)

set(MINGW_COMPAT_INCLUDE
    apisetcconv.h
    apiset.h
    basetsd.h
    bcrypt.h
    bemapiset.h
    cderr.h
    cguid.h
    combaseapi.h
    commdlg.h
    concurrencysal.h
    consoleapi2.h
    consoleapi3.h
    consoleapi.h
    datetimeapi.h
    dde.h
    ddeml.h
    debugapi.h
    dlgs.h
    dpapi.h
    driverspecs.h
    errhandlingapi.h
    fibersapi.h
    fileapi.h
    fltwinerror.h
    guiddef.h
    handleapi.h
    heapapi.h
    imm.h
    inaddr.h
    interlockedapi.h
    ioapiset.h
    jobapi.h
    joystickapi.h
    ktmtypes.h
    libloaderapi.h
    lzexpand.h
    mciapi.h
    mcx.h
    memoryapi.h
    minwinbase.h
    minwindef.h
    mmeapi.h
    mmiscapi2.h
    mmiscapi.h
    mmsyscom.h
    mmsystem.h
    msxml.h
    namedpipeapi.h
    namespaceapi.h
    nb30.h
    ncrypt.h
    ntstatus.h
    ntverp.h
    oaidl.h
    objbase.h
    objidlbase.h
    objidl.h
    ole2.h
    oleauto.h
    oleidl.h
    playsoundapi.h
    poppack.h
    processenv.h
    processthreadsapi.h
    processtopologyapi.h
    profileapi.h
    propidl.h
    prsht.h
    psdk_inc/_fd_types.h
    psdk_inc/intrin-impl.h
    psdk_inc/_ip_mreq1.h
    psdk_inc/_ip_types.h
    psdk_inc/_socket_types.h
    psdk_inc/_wsadata.h
    psdk_inc/_wsa_errnos.h
    psdk_inc/_xmitfile.h
    pshpack1.h
    pshpack2.h
    pshpack4.h
    pshpack8.h
    realtimeapiset.h
    reason.h
    rpcasync.h
    rpcdce.h
    rpcdcep.h
    rpc.h
    rpcndr.h
    rpcnsi.h
    rpcnsip.h
    rpcnterr.h
    rpcsal.h
    sal.h
    sdkddkver.h
    securityappcontainer.h
    securitybaseapi.h
    servprov.h
    shellapi.h
    specstrings.h
    stralign.h
    stringapiset.h
    synchapi.h
    sysinfoapi.h
    systemtopologyapi.h
    threadpoolapiset.h
    threadpoollegacyapiset.h
    timeapi.h
    timezoneapi.h
    tvout.h
    unknwnbase.h
    unknwn.h
    urlmon.h
    utilapiset.h
    virtdisk.h
    winapifamily.h
    winbase.h
    wincon.h
    wincontypes.h
    wincrypt.h
    windef.h
    windows.h
    winefs.h
    winerror.h
    wingdi.h
    winioctl.h
    winnetwk.h
    winnls.h
    winnt.h
    winperf.h
    winreg.h
    winscard.h
    winsmcrd.h
    winsock.h
    winspool.h
    winsvc.h
    winuser.h
    winver.h
    wnnc.h
    wow64apiset.h
    wtypesbase.h
    wtypes.h
)

foreach(f ${MINGW_COMPAT_CRT})
    cmake_path(GET f PARENT_PATH subdir)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mingw-compat/include/${subdir})
    file(COPY_FILE repo/mingw-w64-headers/crt/${f} ${CMAKE_CURRENT_BINARY_DIR}/mingw-compat/include/${f} ONLY_IF_DIFFERENT)
endforeach()

foreach(f ${MINGW_COMPAT_INCLUDE})
    cmake_path(GET f PARENT_PATH subdir)
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/mingw-compat/include/${subdir})
    file(COPY_FILE repo/mingw-w64-headers/include/${f} ${CMAKE_CURRENT_BINARY_DIR}/mingw-compat/include/${f} ONLY_IF_DIFFERENT)
endforeach()

file(COPY_FILE ${CMAKE_CURRENT_BINARY_DIR}/_mingw.h ${CMAKE_CURRENT_BINARY_DIR}/mingw-compat/include/_mingw.h ONLY_IF_DIFFERENT)
