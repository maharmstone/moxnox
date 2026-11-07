set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
file(MAKE_DIRECTORY ${GEN}/src/unix)

add_custom_command(
    OUTPUT ${GEN}/src/unix/core.c
        ${GEN}/src/unix/fs.c
        ${GEN}/src/unix/process.c
        ${GEN}/src/unix/tcp.c
        ${GEN}/src/unix/thread.c
        ${GEN}/src/unix/tty.c
        ${GEN}/src/unix/udp.c
    COMMAND sh -c "cp '${REPO}/src/unix/core.c' '${REPO}/src/unix/fs.c' '${REPO}/src/unix/process.c' '${REPO}/src/unix/tcp.c' '${REPO}/src/unix/thread.c' '${REPO}/src/unix/tty.c' '${REPO}/src/unix/udp.c' '${GEN}/src/unix/' && patch -p1 -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch'"
    DEPENDS
        ${REPO}/src/unix/core.c
        ${REPO}/src/unix/fs.c
        ${REPO}/src/unix/process.c
        ${REPO}/src/unix/tcp.c
        ${REPO}/src/unix/thread.c
        ${REPO}/src/unix/tty.c
        ${REPO}/src/unix/udp.c
        ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    VERBATIM
)

add_custom_target(libuv_patched DEPENDS
    ${GEN}/src/unix/core.c
    ${GEN}/src/unix/fs.c
    ${GEN}/src/unix/process.c
    ${GEN}/src/unix/tcp.c
    ${GEN}/src/unix/thread.c
    ${GEN}/src/unix/tty.c
    ${GEN}/src/unix/udp.c
)

set(LIBUV_BUILD_TESTS OFF)
set(LIBUV_BUILD_BENCH OFF)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE) # libuv is chatty
add_subdirectory(repo)

add_custom_target(libuv)
add_dependencies(libuv uv)

function(libuv_swap_source target name_regex patched)
    get_target_property(_sources ${target} SOURCES)
    list(TRANSFORM _sources REPLACE "^(.*/)?${name_regex}$" "${patched}")
    set_target_properties(${target} PROPERTIES SOURCES "${_sources}")
    set_source_files_properties(${patched} TARGET_DIRECTORY ${target}
        PROPERTIES GENERATED TRUE)
    add_dependencies(${target} libuv_patched)
endfunction()

foreach(target uv uv_a)
    libuv_swap_source(${target} "src/unix/core\\.c" ${GEN}/src/unix/core.c)
    libuv_swap_source(${target} "src/unix/fs\\.c" ${GEN}/src/unix/fs.c)
    libuv_swap_source(${target} "src/unix/process\\.c" ${GEN}/src/unix/process.c)
    libuv_swap_source(${target} "src/unix/tcp\\.c" ${GEN}/src/unix/tcp.c)
    libuv_swap_source(${target} "src/unix/thread\\.c" ${GEN}/src/unix/thread.c)
    libuv_swap_source(${target} "src/unix/tty\\.c" ${GEN}/src/unix/tty.c)
    libuv_swap_source(${target} "src/unix/udp\\.c" ${GEN}/src/unix/udp.c)

    # upstream's CMakeLists.txt knows no Moxnox platform: add the sources it would
    # have added, and drop the pthread library it adds for every other unix
    target_sources(${target} PRIVATE
        ${REPO}/src/unix/posix-hrtime.c
        ${REPO}/src/unix/posix-poll.c
        ${REPO}/src/unix/no-proctitle.c
        ${REPO}/src/unix/no-fsevents.c
        ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.c
    )

    foreach(prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
        get_target_property(_libs ${target} ${prop})
        if(_libs)
            list(REMOVE_ITEM _libs pthread)
            set_target_properties(${target} PROPERTIES ${prop} "${_libs}")
        endif()
    endforeach()

    # the patched copies in gen/ lose own-directory access to their neighbours
    target_include_directories(${target} PRIVATE ${REPO}/src/unix)

    # include/uv/unix.h includes sys/param.h, which Moxnox lacks (see sys/param.h
    # here), and selects uv/posix.h only for platforms it knows: force-including
    # it primes the include guard and defines UV_PLATFORM_LOOP_FIELDS
    target_include_directories(${target} PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
    target_compile_options(${target} PUBLIC "$<BUILD_INTERFACE:SHELL:-include ${REPO}/include/uv/posix.h>")
endforeach()

target_compile_options(uv PRIVATE ${USERSPACE_CFLAGS})

target_compile_options(uv PRIVATE
    -Wno-conversion
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-sign-compare
    -Wno-strict-prototypes
    -Wno-discarded-qualifiers
)
