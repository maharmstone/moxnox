project(nano VERSION 8.7.1)

add_library(gnulib STATIC
    ../gnulib/repo/lib/strcasestr.c
    ../gnulib/repo/lib/mkstemps.c
    ../gnulib/repo/lib/tempname.c
    ../gnulib/getrandom.c
)

target_include_directories(gnulib PRIVATE ../gnulib ../gnulib/repo/lib)
target_compile_definitions(gnulib PRIVATE HAVE_CONFIG_H)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})

configure_file(config.h.in ${GEN}/config.h @ONLY)

add_custom_command(
    OUTPUT ${GEN}/nano.c
    COMMAND sh -c "sed 's/putenv(\"TERM=vt220\")/setenv(\"TERM\", \"vt220\", 0)/g' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/nano.c' > '${GEN}/nano.c'"
    DEPENDS repo/src/nano.c
    VERBATIM
)

add_executable(nano
    repo/src/browser.c
    repo/src/chars.c
    repo/src/color.c
    repo/src/cut.c
    repo/src/files.c
    repo/src/global.c
    repo/src/help.c
    repo/src/history.c
    repo/src/move.c
    ${GEN}/nano.c
    repo/src/prompt.c
    repo/src/rcfile.c
    repo/src/search.c
    repo/src/text.c
    repo/src/utils.c
    repo/src/winio.c
    ../gnulib/repo/lib/regex.c
    ../gnulib/repo/lib/malloc/dynarray_emplace_enlarge.c
    ../gnulib/repo/lib/malloc/dynarray_finalize.c
    ../gnulib/repo/lib/malloc/dynarray_resize.c
)

target_compile_options(nano PRIVATE ${USERSPACE_CFLAGS} -Wno-sign-conversion -Wno-sign-compare -Wno-unused-parameter -Wno-missing-field-initializers -Wno-unused-variable -Wno-conversion -Wno-implicit-fallthrough -Wno-format $<$<CXX_COMPILER_ID:Clang>:-Wno-implicit-int-conversion -Wno-braced-scalar-init -Wno-shorten-64-to-32>)
target_compile_definitions(nano PRIVATE HAVE_CONFIG_H)

target_include_directories(nano PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}
    repo/src
    ../gnulib/repo/lib
    ${CMAKE_BINARY_DIR}/ports/file/gen
)

target_link_libraries(nano PRIVATE ncurses gnulib libmagic moxnox)

install(TARGETS nano DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    ARCHIVE EXCLUDE_FROM_ALL
)
