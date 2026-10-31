cmake_minimum_required(VERSION 3.28)

project(ucrtbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(musl STATIC
    ../ports/musl/repo/src/math/floorf.c
    ../ports/musl/repo/src/math/ceilf.c
    ../ports/musl/repo/src/math/sin.c
    ../ports/musl/repo/src/math/cos.c
    ../ports/musl/repo/src/math/tan.c
    ../ports/musl/repo/src/math/__sin.c
    ../ports/musl/repo/src/math/__cos.c
    ../ports/musl/repo/src/math/__tan.c
    ../ports/musl/repo/src/math/__rem_pio2.c
    ../ports/musl/repo/src/math/__rem_pio2_large.c
    ../ports/musl/repo/src/math/ceil.c
    ../ports/musl/repo/src/math/floor.c
    ../ports/musl/repo/src/math/sinf.c
    ../ports/musl/repo/src/math/cosf.c
    ../ports/musl/repo/src/math/tanf.c
    ../ports/musl/repo/src/math/__sindf.c
    ../ports/musl/repo/src/math/__cosdf.c
    ../ports/musl/repo/src/math/__tandf.c
    ../ports/musl/repo/src/math/__rem_pio2f.c
    ../ports/musl/repo/src/math/scalbn.c
    ../ports/musl/repo/src/math/atan2.c
    ../ports/musl/repo/src/math/atan.c
    ../ports/musl/repo/src/math/atan2f.c
    ../ports/musl/repo/src/math/atanf.c
    ../ports/musl/repo/src/math/acos.c
    ../ports/musl/repo/src/math/acosf.c
    ../ports/musl/repo/src/math/sqrt.c
    ../ports/musl/repo/src/math/sqrtf.c
    ../ports/musl/repo/src/math/sqrt_data.c
    ../ports/musl/repo/src/math/__math_invalid.c
    ../ports/musl/repo/src/math/__math_invalidf.c
    ../ports/musl/repo/src/math/hypot.c
    ../ports/musl/repo/src/math/hypotf.c
    ../ports/musl/repo/src/math/sincos.c
    ../ports/musl/repo/src/math/sincosf.c
    ../ports/musl/repo/src/math/trunc.c
    ../ports/musl/repo/src/math/truncf.c
    ../ports/musl/repo/src/math/exp.c
    ../ports/musl/repo/src/math/expf.c
    ../ports/musl/repo/src/math/exp_data.c
    ../ports/musl/repo/src/math/exp2.c
    ../ports/musl/repo/src/math/exp2f.c
    ../ports/musl/repo/src/math/exp2f_data.c
    ../ports/musl/repo/src/math/__math_oflow.c
    ../ports/musl/repo/src/math/__math_oflowf.c
    ../ports/musl/repo/src/math/__math_uflow.c
    ../ports/musl/repo/src/math/__math_uflowf.c
    ../ports/musl/repo/src/math/__math_xflow.c
    ../ports/musl/repo/src/math/__math_xflowf.c
    ../ports/musl/repo/src/math/pow.c
    ../ports/musl/repo/src/math/powf.c
    ../ports/musl/repo/src/math/pow_data.c
    ../ports/musl/repo/src/math/powf_data.c
    ../ports/musl/repo/src/math/log.c
    ../ports/musl/repo/src/math/logf.c
    ../ports/musl/repo/src/math/log_data.c
    ../ports/musl/repo/src/math/logf_data.c
    ../ports/musl/repo/src/math/__math_divzero.c
    ../ports/musl/repo/src/math/__math_divzerof.c
    ../ports/musl/repo/src/math/log10.c
    ../ports/musl/repo/src/math/log10f.c
    ../ports/musl/repo/src/math/sinh.c
    ../ports/musl/repo/src/math/sinhf.c
    ../ports/musl/repo/src/math/expm1.c
    ../ports/musl/repo/src/math/expm1f.c
    ../ports/musl/repo/src/math/__expo2.c
    ../ports/musl/repo/src/math/__expo2f.c
    ../ports/musl/repo/src/math/cosh.c
    ../ports/musl/repo/src/math/coshf.c
    ../ports/musl/repo/src/math/tanh.c
    ../ports/musl/repo/src/math/tanhf.c
    ../ports/musl/repo/src/math/asin.c
    ../ports/musl/repo/src/math/asinf.c
    ../ports/musl/repo/src/math/fmod.c
    ../ports/musl/repo/src/math/fmodf.c
    ../ports/musl/repo/src/math/frexp.c
    ../ports/musl/repo/src/math/frexpf.c
    ../ports/musl/repo/src/math/modf.c
    ../ports/musl/repo/src/math/modff.c
    ../ports/musl/repo/src/math/copysign.c
    ../ports/musl/repo/src/math/copysignf.c
    ../ports/musl/repo/src/math/round.c
    ../ports/musl/repo/src/math/roundf.c
    ../ports/musl/repo/src/math/ldexp.c
    ../ports/musl/repo/src/math/ldexpf.c
    ../ports/musl/repo/src/math/scalbn.c
    ../ports/musl/repo/src/math/scalbnf.c
    ../ports/musl/repo/src/math/scalbln.c
    ../ports/musl/repo/src/math/scalblnf.c
    ../ports/musl/repo/src/math/log1p.c
    ../ports/musl/repo/src/math/log1pf.c
    ../ports/musl/repo/src/math/asinh.c
    ../ports/musl/repo/src/math/asinhf.c
    ../ports/musl/repo/src/math/acosh.c
    ../ports/musl/repo/src/math/acoshf.c
    ../ports/musl/repo/src/math/atanh.c
    ../ports/musl/repo/src/math/atanhf.c
    ../ports/musl/repo/src/math/log2.c
    ../ports/musl/repo/src/math/log2_data.c
    ../ports/musl/repo/src/math/log2f.c
    ../ports/musl/repo/src/math/log2f_data.c
    ../ports/musl/repo/src/math/fabs.c
    ../ports/musl/repo/src/math/fabsf.c
    ../ports/musl/repo/src/math/nextafter.c
    ../ports/musl/repo/src/math/nextafterf.c
    ../ports/musl/repo/src/math/expm1.c
    ../ports/musl/repo/src/math/expm1f.c
    ../ports/musl/repo/src/math/erf.c
    ../ports/musl/repo/src/math/erff.c
    ../ports/musl/repo/src/math/cbrt.c
    ../ports/musl/repo/src/math/cbrtf.c
    ../ports/musl/repo/src/math/fma.c
    ../ports/musl/repo/src/math/fmaf.c
    ../ports/musl/repo/src/math/lrint.c
    ../ports/musl/repo/src/math/lrintf.c
    ../ports/musl/repo/src/math/rint.c
    ../ports/musl/repo/src/math/rintf.c
    ../ports/musl/repo/src/math/ilogb.c
    ../ports/musl/repo/src/math/ilogbf.c
    ../ports/musl/repo/src/math/logb.c
    ../ports/musl/repo/src/math/logbf.c
    ../ports/musl/repo/src/math/fmin.c
    ../ports/musl/repo/src/math/fminf.c
    ../ports/musl/repo/src/math/fmax.c
    ../ports/musl/repo/src/math/fmaxf.c
    ../ports/musl/repo/src/math/lround.c
    ../ports/musl/repo/src/math/lroundf.c
    ../ports/musl/repo/src/math/llround.c
    ../ports/musl/repo/src/math/llroundf.c
    ../ports/musl/repo/src/math/llrint.c
    ../ports/musl/repo/src/math/llrintf.c
)

target_include_directories(musl PRIVATE musl_compat ../ports/musl/repo/src/include ../ports/musl/repo/src/internal ../ports/musl/repo/arch/generic)
target_compile_options(musl PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/../ports/musl/repo/src/include/features.h)
target_compile_options(musl PRIVATE -Wno-attributes)

add_library(mingwex2 STATIC
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/dmisc.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/gdtoa.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/gethex.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/gmisc.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/hd_init.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/hexnan.c
    gdtoa-misc.cpp
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/smisc.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/strtodg.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/strtodnrp.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/strtof.c
    ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa/sum.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_pformat.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_wpformat.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_vfprintf.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_vfwprintf.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_vfscanf.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_vsnprintf.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_vswprintf.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_vsprintf.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_vsscanf.c
    ../ports/mingw-w64/repo/mingw-w64-crt/stdio/mingw_sformat.c
)

set(DEFAULT_MSVCRT_VERSION 0x600)
set(DEFAULT_WIN32_WINNT 0xa00)
configure_file(../ports/mingw-w64/repo/mingw-w64-headers/crt/_mingw.h.in ${CMAKE_CURRENT_BINARY_DIR}/_mingw.h)
unset(DEFAULT_WIN32_WINNT)
unset(DEFAULT_MSVCRT_VERSION)

target_include_directories(mingwex2 PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ../ports/mingw-w64/repo/mingw-w64-headers/crt ../ports/mingw-w64/repo/mingw-w64-crt/include ../ports/mingw-w64/repo/mingw-w64-headers/include ../ports/mingw-w64/repo/mingw-w64-crt/gdtoa)
target_compile_definitions(mingwex2 PRIVATE -D_CRTIMP= -D_UCRT -D_CRTBLD -D_WIN32 -D_WIN64 -D__MINGW32__)
target_compile_options(mingwex2 PRIVATE ${USERSPACE_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti -fno-exceptions> -mno-stack-arg-probe -fno-stack-protector $<$<CXX_COMPILER_ID:Clang>:-Wno-pragma-pack -Wno-include-next-absolute-path>)
target_compile_options(mingwex2 PRIVATE -Wno-shorten-64-to-32 -Wno-missing-declarations -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-unused-variable -Wno-gnu-pointer-arith -Wno-implicit-float-conversion -Wno-float-conversion -Wno-conversion -Wno-pointer-arith $<$<CXX_COMPILER_ID:GNU>:-Wno-use-after-free -Wno-missing-attributes>)

set(SRC_FILES src/ucrtbase.cpp
    src/stubs.S
    src/ucrtbase.def
    src/errno.cpp
    src/data.cpp
    src/heap.cpp
    src/exit.cpp
    src/locale.cpp
    src/mbcs.cpp
    src/math.cpp
    src/misc.cpp
    src/onexit.cpp
    src/environ.cpp
    src/string.cpp
    src/except.cpp
    src/setjmp.cpp
    src/dir.cpp
    src/ctype.cpp
    src/wcs.cpp
    src/process.cpp
    src/scanf.cpp
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL x86_64)
    list(APPEND SRC_FILES src/x86/setjmp.S)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    list(APPEND SRC_FILES src/aarch64/setjmp.S)
endif()

# jump through some hoops so that ucrtbase doesn't try to link to itself

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    set(UCRTBASE_ALIAS_FLAGS "-Wl,/alternatename:_errno=__errno -Wl,/alternatename:strerror=__strerror -Wl,/alternatename:_lock_file=__lock_file -Wl,/alternatename:_unlock_file=__unlock_file -Wl,/alternatename:fputc=_fputc -Wl,/alternatename:localeconv=_localeconv -Wl,/alternatename:mbrtowc=_mbrtowc -Wl,/alternatename:wcrtomb=_wcrtomb -Wl,/alternatename:getc=_fgetc -Wl,/alternatename:ungetc=_ungetc -Wl,/alternatename:mbrlen=_mbrlen")
else()
    set(UCRTBASE_ALIAS_FLAGS "-Wl,--defsym,_errno=__errno -Wl,--defsym,strerror=__strerror -Wl,--defsym,_lock_file=__lock_file -Wl,--defsym,_unlock_file=__unlock_file -Wl,--defsym,fputc=_fputc -Wl,--defsym,localeconv=_localeconv -Wl,--defsym,mbrtowc=_mbrtowc -Wl,--defsym,wcrtomb=_wcrtomb -Wl,--defsym,getc=_fgetc -Wl,--defsym,ungetc=_ungetc -Wl,--defsym,mbrlen=_mbrlen")
endif()

set(CMAKE_CXX_CREATE_SHARED_LIBRARY "<CMAKE_CXX_COMPILER> <CMAKE_SHARED_LIBRARY_CXX_FLAGS> <LANGUAGE_COMPILE_FLAGS> <LINK_FLAGS> -shared -o <TARGET> -Wl,--out-implib,<TARGET_IMPLIB> -Wl,--major-image-version,<TARGET_VERSION_MAJOR>,--minor-image-version,<TARGET_VERSION_MINOR> <OBJECTS> <LINK_LIBRARIES> ${LIBSTDCXX} -lmingw32 ports/mingw-w64/libucrtbase_static.a ${LIBEH} ${LIBGCC} -lmingw32 ${LIBSTDCXX} -lmingw32 ports/mingw-w64/libucrtbase_static.a ${LIBEH} ${LIBGCC} -lmingw32 ports/mingw-w64/libmingwex.a ports/mingw-w64/libucrtbase_static.a libmoxnox/libkernelbase.a libmoxnox/libntdll.a ${UCRTBASE_ALIAS_FLAGS} ${CMAKE_CURRENT_BINARY_DIR}/libmnposix.a")

add_library(ucrtbase SHARED ${SRC_FILES})
target_sources(ucrtbase PUBLIC FILE_SET CXX_MODULES FILES
    src/file.cpp
    src/time.cpp
    src/except.cpp
    src/tz.cpp
)

target_compile_options(ucrtbase PRIVATE -fno-exceptions -fno-builtin ${USERSPACE_CFLAGS})
target_link_options(ucrtbase PRIVATE -nostartfiles -nostdlib)

target_link_libraries(ucrtbase PRIVATE
    format_nobuiltin
    print_nobuiltin
    unicode_nobuiltin
    list_nobuiltin
    errno_nobuiltin
    unique_handle_nobuiltin
    moxnox_ioctls_nobuiltin
    exec_nobuiltin
)
target_link_libraries(ucrtbase PRIVATE nt_nobuiltin ucrtbase_mod_nobuiltin kernelbase_mod_nobuiltin)
target_link_libraries(ucrtbase PRIVATE -Wl,--whole-archive musl -Wl,--no-whole-archive)
target_link_libraries(ucrtbase PRIVATE -Wl,--whole-archive mingwex2 -Wl,--no-whole-archive)

add_custom_command(TARGET ucrtbase POST_BUILD
    COMMAND sh -c "! \"${CMAKE_OBJDUMP}\" -p \"$1\" | grep -q 'DLL Name: ucrtbase' || { echo 'Error: ucrtbase.dll imports itself' >&2; exit 1; }" -- "$<TARGET_FILE:ucrtbase>"
    VERBATIM
    COMMENT "Checking ucrtbase.dll does not import itself"
)

install(TARGETS ucrtbase
    COMPONENT core
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    set(ARCH arm64)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(ARCH "i386:x86-64")
endif()

add_custom_command(
    OUTPUT libmnposix.a
    COMMAND ${DLLTOOL} -d ${CMAKE_CURRENT_SOURCE_DIR}/../libmoxnox/mnposix.def -l ${CMAKE_CURRENT_BINARY_DIR}/libmnposix.a -m ${ARCH}
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/../libmoxnox/mnposix.def
)

add_custom_target(ucrtbase_mnposix_target ALL DEPENDS libmnposix.a)

add_dependencies(ucrtbase ucrtbase_mnposix_target)

target_link_options(ucrtbase PRIVATE -Wl,--gc-sections -Wl,-u,_tls_used)
