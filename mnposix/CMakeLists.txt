cmake_minimum_required(VERSION 3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_library(musl_glob STATIC
    ../ports/musl/repo/src/regex/fnmatch.c
    ../ports/musl/repo/src/regex/glob.c
    ../ports/musl/repo/src/string/strchrnul.c
)

target_include_directories(musl_glob PRIVATE glob_compat)
target_compile_options(musl_glob PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -Wno-sign-conversion -Wno-parentheses -Wno-unused-parameter -Wno-conversion -Wno-sign-compare)
target_compile_options(musl_glob PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/glob_compat/glob_compat.h)

add_library(tre STATIC
    ../ports/tre/repo/lib/regcomp.c
    ../ports/tre/repo/lib/regexec.c
    ../ports/tre/repo/lib/regerror.c
    ../ports/tre/repo/lib/tre-ast.c
    ../ports/tre/repo/lib/tre-compile.c
    ../ports/tre/repo/lib/tre-match-backtrack.c
    ../ports/tre/repo/lib/tre-match-parallel.c
    ../ports/tre/repo/lib/tre-mem.c
    ../ports/tre/repo/lib/tre-parse.c
    ../ports/tre/repo/lib/tre-stack.c
)

target_include_directories(tre PRIVATE ../ports/tre)
target_compile_definitions(tre PRIVATE HAVE_CONFIG_H)
target_compile_options(tre PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -Wno-sign-conversion -Wno-conversion)
target_compile_options(tre PRIVATE "SHELL:-include ${CMAKE_SOURCE_DIR}/ports/tre/tre-internal-fix.h")

add_library(musl_crypt STATIC
    ../ports/musl/repo/src/crypt/crypt_r.c
    ../ports/musl/repo/src/crypt/crypt_blowfish.c
    ../ports/musl/repo/src/crypt/crypt_sha256.c
    ../ports/musl/repo/src/crypt/crypt_sha512.c
    ../ports/musl/repo/src/crypt/crypt_des.c
    ../ports/musl/repo/src/crypt/crypt_md5.c
)

target_include_directories(musl_crypt PRIVATE crypt_compat)
target_compile_options(musl_crypt PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -Wno-conversion -Wno-sign-compare -Wno-missing-braces) # -Wno-sign-conversion -Wno-parentheses -Wno-unused-parameter
target_compile_options(musl_crypt PRIVATE "SHELL:-include ${CMAKE_SOURCE_DIR}/mnposix/crypt_compat/crypt.h")

add_library(mnposix SHARED
    src/mnposix.def
    src/stubs.S
    src/termios.cpp
    src/sleep.cpp
    src/signal.cpp
    src/dirname.cpp
    src/time.cpp
    src/security.cpp
    src/poll.cpp
    src/environ.cpp
    src/file.cpp
    src/wcwidth.cpp
    src/dir.cpp
    src/pwd.cpp
    src/rand.cpp
    src/ttyname.cpp
    src/fcntl.cpp
    src/spawn.cpp
    src/pthread.cpp
    src/mmap.cpp
    src/uname.cpp
    src/sysconf.cpp
    src/process.cpp
    src/resource.cpp
    src/error.cpp
    src/sem.cpp
    src/sched.cpp
    src/string.cpp
    src/socket.cpp
    src/syslog.cpp
    src/locale.cpp
    src/hostid.cpp
    src/crypt.cpp
    src/timer.cpp
)

target_compile_options(mnposix PRIVATE -fno-exceptions -fno-builtin ${USERSPACE_CFLAGS})

target_link_libraries(mnposix PRIVATE
    print_nobuiltin
    errno_nobuiltin
    termios_nobuiltin
    unicode_nobuiltin
    unique_handle_nobuiltin
    exec_nobuiltin
    list_nobuiltin
    ucrtbase_mod_nobuiltin
    moxnox_ioctls_nobuiltin
    timepage_nobuiltin
    pe_nobuiltin
    ipv4_nobuiltin
    ipv6_nobuiltin
)

target_link_libraries(mnposix PRIVATE -Wl,--whole-archive musl_glob -Wl,--no-whole-archive)
target_link_libraries(mnposix PRIVATE -Wl,--whole-archive tre -Wl,--no-whole-archive)
target_link_libraries(mnposix PRIVATE -Wl,--whole-archive musl_crypt -Wl,--no-whole-archive)

install(TARGETS mnposix
    COMPONENT core
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)

add_custom_command(TARGET mnposix POST_BUILD
    COMMAND sh -c "! \"${CMAKE_OBJDUMP}\" -p \"$1\" | grep -q 'DLL Name: mnposix' || { echo 'Error: mnposix.dll imports itself' >&2; exit 1; }" -- "$<TARGET_FILE:mnposix>"
    VERBATIM
    COMMENT "Checking mnposix.dll does not import itself"
)
