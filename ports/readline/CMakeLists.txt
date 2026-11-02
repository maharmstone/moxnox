project(readline VERSION 8.3)

set(RL_GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY ${RL_GEN})
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/repo" "${CMAKE_CURRENT_BINARY_DIR}/readline" SYMBOLIC)

foreach(_file terminal.c histfile.c kill.c funmap.c)
    add_custom_command(
        OUTPUT ${RL_GEN}/${_file}
        COMMAND sh -c "sed -e 's/defined (_WIN32)/(defined (_WIN32) \\&\\& !defined (MOXNOX))/g' -e 's/defined(_WIN32)/(defined(_WIN32) \\&\\& !defined(MOXNOX))/g' '${CMAKE_CURRENT_SOURCE_DIR}/repo/${_file}' > '${RL_GEN}/${_file}'"
        DEPENDS repo/${_file}
        VERBATIM
    )
endforeach()

add_custom_command(
    OUTPUT ${RL_GEN}/input.c
    COMMAND sh -c "sed -e 's/defined (_WIN32)/(defined (_WIN32) \\&\\& !defined (MOXNOX))/g' -e 's/defined (__MINGW32__)/(defined (__MINGW32__) \\&\\& !defined (MOXNOX))/g' -e 's/defined (__MINGW32_MAJOR_VERSION)/(defined (__MINGW32_MAJOR_VERSION) \\&\\& !defined (MOXNOX))/g' '${CMAKE_CURRENT_SOURCE_DIR}/repo/input.c' > '${RL_GEN}/input.c'"
    DEPENDS repo/input.c
    VERBATIM
)

add_library(readline SHARED
    readline.def
    repo/bind.c
    repo/callback.c
    repo/colors.c
    repo/compat.c
    repo/complete.c
    repo/display.c
    ${RL_GEN}/funmap.c
    repo/gettimeofday.c
    repo/histexpand.c
    ${RL_GEN}/histfile.c
    repo/history.c
    repo/histsearch.c
    ${RL_GEN}/input.c
    repo/isearch.c
    repo/keymaps.c
    ${RL_GEN}/kill.c
    repo/macro.c
    repo/mbutil.c
    repo/misc.c
    repo/nls.c
    repo/parens.c
    repo/parse-colors.c
    repo/readline.c
    repo/rltty.c
    repo/savestring.c
    repo/search.c
    repo/shell.c
    repo/signals.c
    ${RL_GEN}/terminal.c
    repo/text.c
    repo/tilde.c
    repo/undo.c
    repo/util.c
    repo/vi_mode.c
    repo/xfree.c
    repo/xmalloc.c
)

target_compile_definitions(readline PRIVATE HAVE_CONFIG_H RL_TIMEOUT_USE_SELECT READLINE_LIBRARY=)
target_compile_options(readline PRIVATE
    ${USERSPACE_CFLAGS}
    -fvisibility=default
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-conversion
    -Wno-implicit-fallthrough
    -Wno-unused-variable
    -Wno-unused-function
    -Wno-parentheses
    -Wno-type-limits
    -Wno-unused-but-set-variable
    -Wno-unused-label
    $<$<CXX_COMPILER_ID:Clang>:-Wno-incompatible-pointer-types-discards-qualifiers>
    $<$<CXX_COMPILER_ID:GNU>:-Wno-discarded-qualifiers>
)

target_include_directories(readline PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(readline PRIVATE repo ${CMAKE_CURRENT_SOURCE_DIR} ${GEN})

target_link_libraries(readline PRIVATE tinfo moxnox)
# target_link_options(readline PRIVATE -Wl,--exclude-libs,ALL)

install(TARGETS readline
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(FILES
    repo/chardefs.h
    repo/history.h
    repo/keymaps.h
    repo/readline.h
    repo/rlconf.h
    repo/rlstdc.h
    repo/rltypedefs.h
    repo/tilde.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/readline
)
