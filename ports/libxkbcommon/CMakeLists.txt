project(libxkbcommon VERSION 1.13.2)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/src/xkbcomp)
file(MAKE_DIRECTORY ${GEN}/tools)

add_custom_command(
    OUTPUT ${GEN}/src/xkbcomp/parser.c ${GEN}/src/xkbcomp/parser.h
    COMMAND ${HOST_BISON} --defines=${GEN}/src/xkbcomp/parser.h
            -o ${GEN}/src/xkbcomp/parser.c -p _xkbcommon_
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/xkbcomp/parser.y
    DEPENDS repo/src/xkbcomp/parser.y
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/xkbcomp/keymap.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/xkbcomp/keymap.c ${GEN}/src/xkbcomp/keymap.c
    COMMAND ${HOST_SED} -z -i [[s|#ifdef _WIN32\n    /\* Bit fields are not packed on Windows|#if defined(_WIN32) \|\| defined(__moxnox__)\n    /* Bit fields are not packed on Windows|]] ${GEN}/src/xkbcomp/keymap.c
    DEPENDS repo/src/xkbcomp/keymap.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/tools/tools-common.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/tools/tools-common.c ${GEN}/tools/tools-common.c
    COMMAND ${HOST_SED} -i [[s|"%s/%s-%s", LIBXKBCOMMON_TOOL_PATH, prefix, command|"%s/%s-%s.exe", LIBXKBCOMMON_TOOL_PATH, prefix, command|]] ${GEN}/tools/tools-common.c
    DEPENDS repo/tools/tools-common.c
    VERBATIM
)

add_library(xkbcommon SHARED
    repo/src/compose/parser.c
    repo/src/compose/paths.c
    repo/src/compose/state.c
    repo/src/compose/table.c
    repo/src/xkbcomp/action.c
    repo/src/xkbcomp/ast-build.c
    repo/src/xkbcomp/compat.c
    repo/src/xkbcomp/expr.c
    repo/src/xkbcomp/include.c
    repo/src/xkbcomp/keycodes.c
    ${GEN}/src/xkbcomp/keymap.c
    repo/src/xkbcomp/keymap-dump.c
    repo/src/xkbcomp/keywords.c
    ${GEN}/src/xkbcomp/parser.c
    repo/src/xkbcomp/rules.c
    repo/src/xkbcomp/scanner.c
    repo/src/xkbcomp/symbols.c
    repo/src/xkbcomp/types.c
    repo/src/xkbcomp/vmod.c
    repo/src/xkbcomp/xkbcomp.c
    repo/src/atom.c
    repo/src/context.c
    repo/src/context-priv.c
    repo/src/keysym.c
    repo/src/keysym-case-mappings.c
    repo/src/keysym-utf.c
    repo/src/keymap.c
    repo/src/keymap-compare.c
    repo/src/keymap-priv.c
    repo/src/rmlvo.c
    repo/src/scanner-utils.c
    repo/src/state.c
    repo/src/text.c
    repo/src/utf8.c
    repo/src/utf8-decoding.c
    repo/src/utils.c
    repo/src/utils-paths.c
)

set(XKBCOMMON_CFLAGS
    ${USERSPACE_CFLAGS}
    -std=c11
    -fno-strict-aliasing
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wpointer-arith
    -Wmissing-declarations
    -Wformat=2
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wnested-externs
    -Wbad-function-cast
    -Wshadow
    -Wdate-time
    -Wwrite-strings
    -Wno-conversion
    -Wno-sign-conversion
)

target_include_directories(xkbcommon PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/repo/include)
target_include_directories(xkbcommon PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/xkbcomp
    ${GEN}/src/xkbcomp
)
target_compile_options(xkbcommon PRIVATE ${XKBCOMMON_CFLAGS})
target_link_libraries(xkbcommon PRIVATE moxnox)

install(TARGETS xkbcommon DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES
    repo/include/xkbcommon/xkbcommon.h
    repo/include/xkbcommon/xkbcommon-compat.h
    repo/include/xkbcommon/xkbcommon-compose.h
    repo/include/xkbcommon/xkbcommon-keysyms.h
    repo/include/xkbcommon/xkbcommon-names.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/xkbcommon
)

foreach(define ROOT EXTRA_PATH UNVERSIONED_EXTENSIONS_PATH VERSIONED_EXTENSIONS_PATH)
    file(STRINGS config.h XKB_CONFIG_LINE REGEX "^#define DFLT_XKB_CONFIG_${define} ")
    string(REGEX REPLACE "^#define DFLT_XKB_CONFIG_${define} \"([^\"]*)\".*" "\\1" XKB_CONFIG_${define} "${XKB_CONFIG_LINE}")
endforeach()

configure_file(xkbcommon.pc.in ${GEN}/xkbcommon.pc @ONLY)
install(FILES ${GEN}/xkbcommon.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

###

add_library(xkbcommon_tools STATIC
    ${GEN}/tools/tools-common.c
    repo/src/utf8-decoding.c
    repo/src/keymap-formats.c
)

target_include_directories(xkbcommon_tools PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/tools
)
target_compile_options(xkbcommon_tools PRIVATE ${XKBCOMMON_CFLAGS})
target_link_libraries(xkbcommon_tools PUBLIC xkbcommon moxnox)

add_executable(xkbcli repo/tools/xkbcli.c)
add_executable(xkbcli-info repo/tools/info.c)
add_executable(xkbcli-compile-keymap repo/tools/compile-keymap.c)

add_executable(xkbcli-compile-compose
    repo/tools/compile-compose.c
    repo/src/compose/dump.c
)

add_executable(xkbcli-how-to-type repo/tools/how-to-type.c)

foreach(tool xkbcli xkbcli-info xkbcli-compile-keymap xkbcli-compile-compose xkbcli-how-to-type)
    target_compile_options(${tool} PRIVATE ${XKBCOMMON_CFLAGS})
    target_link_libraries(${tool} PRIVATE xkbcommon_tools)
endforeach()

install(TARGETS xkbcli DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS xkbcli-info xkbcli-compile-keymap xkbcli-compile-compose xkbcli-how-to-type
        DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec/xkbcommon)

###

add_custom_target(libxkbcommon)

add_dependencies(libxkbcommon
    xkbcommon
    xkbcli
    xkbcli-info
    xkbcli-compile-keymap
    xkbcli-compile-compose
    xkbcli-how-to-type
)
