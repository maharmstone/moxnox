cmake_minimum_required(VERSION 3.28)

project(shell)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

###

# set(SRC_FILES src/shell.cpp)
#
# add_executable(shell ${SRC_FILES})
# target_sources(shell PUBLIC FILE_SET CXX_MODULES FILES
#     src/cd.cpp
# )
#
# target_compile_options(shell PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
#
# if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
#     target_compile_options(shell PRIVATE -masm=intel)
# endif()
#
# target_link_libraries(shell moxnox print unicode unique_handle list pagesize termios errno win32error)
# target_link_libraries(shell format)
# target_link_libraries(shell readline)
#
# install(TARGETS shell DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
#     COMPONENT core
#     CXX_MODULES_BMI EXCLUDE_FROM_ALL
# )

###

add_executable(mnecho src/echo.cpp)

target_compile_options(mnecho PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnecho print unicode format)
target_link_libraries(mnecho moxnox)

install(TARGETS mnecho DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnls src/ls.cpp)

target_compile_options(mnls PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnls print)
target_link_libraries(mnls format list unicode errno)
target_link_libraries(mnls moxnox)

install(TARGETS mnls DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mncat src/cat.cpp)

target_compile_options(mncat PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mncat print)
target_link_libraries(mncat format unicode)
target_link_libraries(mncat moxnox)

install(TARGETS mncat DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnpwd src/pwd.cpp)

target_compile_options(mnpwd PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnpwd print unicode format)
target_link_libraries(mnpwd moxnox)

install(TARGETS mnpwd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnyes src/yes.cpp)

target_compile_options(mnyes PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnyes print format)
target_link_libraries(mnyes moxnox)

install(TARGETS mnyes DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnmkdir src/mkdir.cpp)

target_compile_options(mnmkdir PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnmkdir print)
target_link_libraries(mnmkdir format unicode)
target_link_libraries(mnmkdir moxnox)

install(TARGETS mnmkdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mntouch src/touch.cpp)

target_compile_options(mntouch PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mntouch print)
target_link_libraries(mntouch format unicode)
target_link_libraries(mntouch moxnox)

install(TARGETS mntouch DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnrm src/rm.cpp)

target_compile_options(mnrm PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnrm print)
target_link_libraries(mnrm format unicode)
target_link_libraries(mnrm moxnox)

install(TARGETS mnrm DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnrmdir src/rmdir.cpp)

target_compile_options(mnrmdir PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnrmdir print)
target_link_libraries(mnrmdir format unicode)
target_link_libraries(mnrmdir moxnox)

install(TARGETS mnrmdir DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(screenshot src/screenshot.cpp)

target_compile_options(screenshot PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(screenshot print format unicode)
target_link_libraries(screenshot moxnox)
target_link_libraries(screenshot png_shared zlib)

install(TARGETS screenshot DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(seh-test src/seh-test.cpp)

target_compile_options(seh-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(seh-test print)
target_link_libraries(seh-test format)
target_link_libraries(seh-test moxnox)

install(TARGETS seh-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

# if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
#     add_executable(seh2-test src/seh2-test.cpp)
#
#     target_compile_options(seh2-test PRIVATE -fno-exceptions -fms-extensions ${USERSPACE_CFLAGS})
#
#     target_link_libraries(seh2-test print-ms)
#
#     install(TARGETS seh2-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)
# endif()

###

add_executable(file-test src/file-test.cpp)

target_compile_options(file-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(file-test PUBLIC -mcrtdll=ucrtbase)
endif()

target_link_libraries(file-test print)
target_link_libraries(file-test format)

install(TARGETS file-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(crt-test src/crt-test.cpp)

target_compile_options(crt-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(crt-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS crt-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(regex-test src/regex-test.cpp)

target_compile_options(regex-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(regex-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS regex-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(glob-test src/glob-test.cpp)

target_compile_options(glob-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(glob-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS glob-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ps src/ps.cpp)

target_compile_options(ps PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(ps print unique_handle format list)
target_link_libraries(ps moxnox)

install(TARGETS ps DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnkill src/kill.cpp)

target_compile_options(mnkill PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnkill moxnox print errno)

install(TARGETS mnkill DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(killall src/killall.cpp)

target_compile_options(killall PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(killall print format unique_handle unicode errno)
target_link_libraries(killall moxnox)

install(TARGETS killall DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(reboot src/reboot.cpp)

target_compile_options(reboot PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(reboot print)
target_link_libraries(reboot format)
target_link_libraries(reboot moxnox)

install(TARGETS reboot DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(halt src/halt.cpp)

target_compile_options(halt PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(halt print)
target_link_libraries(halt format)
target_link_libraries(halt moxnox)

install(TARGETS halt DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnmv src/mv.cpp)
target_sources(mnmv PUBLIC FILE_SET CXX_MODULES FILES src/copy.cpp)

target_compile_options(mnmv PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnmv print)
target_link_libraries(mnmv format unique_handle list unicode)
target_link_libraries(mnmv moxnox)

install(TARGETS mnmv DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(mncp src/cp.cpp)
target_sources(mncp PUBLIC FILE_SET CXX_MODULES FILES src/copy.cpp)

target_compile_options(mncp PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mncp print)
target_link_libraries(mncp format unique_handle list unicode)
target_link_libraries(mncp moxnox)

install(TARGETS mncp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(mount src/mount.cpp)

target_compile_options(mount PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mount print)
target_link_libraries(mount format unique_handle unicode)
target_link_libraries(mount moxnox)

install(TARGETS mount DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(umount src/umount.cpp)

target_compile_options(umount PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(umount print)
target_link_libraries(umount format unique_handle unicode)
target_link_libraries(umount moxnox)

install(TARGETS umount DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(sock-test src/sock-test.cpp)

target_compile_options(sock-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(sock-test moxnox print ipv4 ipv6 errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(sock-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS sock-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(udp-server-test src/udp-server-test.cpp)

target_compile_options(udp-server-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(udp-server-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(udp-server-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS udp-server-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(udp-client-test src/udp-client-test.cpp)

target_compile_options(udp-client-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(udp-client-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(udp-client-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS udp-client-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(getaddrinfo-test src/getaddrinfo-test.cpp)

target_compile_options(getaddrinfo-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(getaddrinfo-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(getaddrinfo-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS getaddrinfo-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(tcp-client-test src/tcp-client-test.cpp)

target_compile_options(tcp-client-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(tcp-client-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tcp-client-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tcp-client-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(tcp-server-test src/tcp-server-test.cpp)

target_compile_options(tcp-server-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(tcp-server-test moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tcp-server-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tcp-server-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(httpd-test src/httpd-test.cpp)

target_compile_options(httpd-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(httpd-test moxnox print errno pe)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(httpd-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS httpd-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(exc-test src/exc-test.cpp)

target_compile_options(exc-test PRIVATE ${USERSPACE_CFLAGS})
target_link_libraries(exc-test moxnox print-exc)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(exc-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS exc-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ifconfig src/ifconfig.cpp)

target_compile_options(ifconfig PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ifconfig moxnox print unique_handle ipv4 ipv6 timepage)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(ifconfig PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS ifconfig DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ping src/ping.cpp)

target_compile_options(ping PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ping print unique_handle ipv4 ipv6 errno)
target_link_libraries(ping moxnox)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(ping PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS ping DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(telnetd src/telnetd.cpp)

target_compile_options(telnetd PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(telnetd moxnox print errno unique_handle unicode pe)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(telnetd PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS telnetd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(rtti-test src/rtti-test.cpp)

target_compile_options(rtti-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS} -frtti)
target_link_libraries(rtti-test moxnox print-rtti)
target_link_options(rtti-test PRIVATE -Wl,--allow-multiple-definition) # FIXME

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(rtti-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS rtti-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(tls-test src/tls-test.cpp)

target_compile_options(tls-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(tls-test moxnox print unique_handle pe)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tls-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tls-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(tls2-test src/tls2-test.cpp)

target_compile_options(tls2-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(tls2-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(tls2-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tls2-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(condvar-test src/condvar-test.cpp)

target_compile_options(condvar-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(condvar-test moxnox print unique_handle pe)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(condvar-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS condvar-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(apc-test src/apc-test.cpp)

target_compile_options(apc-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(apc-test moxnox print unique_handle pe)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(apc-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS apc-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

add_executable(signal-test src/signal-test.cpp)

target_compile_options(signal-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(signal-test moxnox print unique_handle)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(signal-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS signal-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(keyboard-test src/keyboard-test.cpp)

target_compile_options(keyboard-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(keyboard-test moxnox print evdev unique_handle)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(keyboard-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS keyboard-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mouse-test src/mouse-test.cpp)

target_compile_options(mouse-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(mouse-test moxnox print evdev unique_handle)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(mouse-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS mouse-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnstat src/stat.cpp)

target_compile_options(mnstat PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(mnstat moxnox print errno)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(mnstat PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS mnstat DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

# add_executable(readline-test src/readline-test.cpp)
#
# target_compile_options(readline-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
# target_link_libraries(readline-test moxnox readline)
#
# if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
#     target_link_options(readline-test PUBLIC -mcrtdll=ucrtbase)
# endif()
#
# install(TARGETS readline-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(setjmp-test src/setjmp-test.cpp)

target_compile_options(setjmp-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(setjmp-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS setjmp-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(popen-test src/popen-test.cpp)

target_compile_options(popen-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(popen-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS popen-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(spawn-test src/spawn-test.cpp)

target_compile_options(spawn-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(spawn-test PUBLIC -mcrtdll=ucrtbase)
endif()

target_link_libraries(spawn-test moxnox print unicode)

install(TARGETS spawn-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(pthread-test src/pthread-test.cpp)

target_compile_options(pthread-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(pthread-test PUBLIC -mcrtdll=ucrtbase)
endif()

target_link_libraries(pthread-test moxnox print unicode)

install(TARGETS pthread-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(fifo-test src/fifo-test.cpp)

target_compile_options(fifo-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(fifo-test PUBLIC -mcrtdll=ucrtbase)
endif()

target_link_libraries(fifo-test moxnox print unicode)

install(TARGETS fifo-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(ntqo-test src/ntqo-test.cpp)

target_compile_options(ntqo-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ntqo-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(ntqo-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS ntqo-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(mnln src/ln.cpp)

target_compile_options(mnln PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(mnln print unicode unique_handle)
target_link_libraries(mnln moxnox)

install(TARGETS mnln DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

###

add_executable(dl-test src/dl-test.cpp)

target_compile_options(dl-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(dl-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(dl-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS dl-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(df src/df.cpp)

target_compile_options(df PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(df print)
target_link_libraries(df format unique_handle unicode)
target_link_libraries(df moxnox)

install(TARGETS df DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(trace src/trace.cpp)

target_compile_options(trace PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(trace moxnox print unique_handle)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(trace PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS trace DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)

###

add_executable(fork-test src/fork-test.cpp)

target_compile_options(fork-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(fork-test moxnox print)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(fork-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS fork-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin COMPONENT core)
