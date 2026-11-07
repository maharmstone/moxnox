project(readline VERSION 8.3)

set(RL_GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")
file(MAKE_DIRECTORY ${RL_GEN})
file(CREATE_LINK "${CMAKE_CURRENT_SOURCE_DIR}/repo" "${CMAKE_CURRENT_BINARY_DIR}/readline" SYMBOLIC)

add_library(readline SHARED
    readline.def
    repo/bind.c
    repo/callback.c
    repo/colors.c
    repo/compat.c
    repo/complete.c
    repo/display.c
    repo/funmap.c
    repo/gettimeofday.c
    repo/histexpand.c
    repo/histfile.c
    repo/history.c
    repo/histsearch.c
    repo/input.c
    repo/isearch.c
    repo/keymaps.c
    repo/kill.c
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
    repo/terminal.c
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

set(prefix "")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(LIBVERSION ${PROJECT_VERSION})
set(TERMCAP_PKG_CONFIG_LIB tinfo)
configure_file(repo/readline.pc.in ${RL_GEN}/readline.pc @ONLY)
install(FILES ${RL_GEN}/readline.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
