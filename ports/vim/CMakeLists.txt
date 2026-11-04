project(vim VERSION 9.1)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/auto)

configure_file(config.h ${GEN}/auto/config.h COPYONLY)
configure_file(osdef.h ${GEN}/auto/osdef.h COPYONLY)

file(MAKE_DIRECTORY ${GEN}/xdiff)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/xdiff/xinclude.h _src)
string(REPLACE "#  include \"../auto/config.h\""
               "#  include \"auto/config.h\"" _src "${_src}")
file(CONFIGURE OUTPUT "${GEN}/xdiff/xinclude.h" CONTENT "${_src}")

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/vim.h _src)
string(REPLACE
    "#ifndef EXTERN\n# define EXTERN extern\n# define INIT(x)\n# define INIT2(a, b)\n# define INIT3(a, b, c)\n# define INIT4(a, b, c, d)\n# define INIT5(a, b, c, d, e)\n# define INIT6(a, b, c, d, e, f)\n#else\n# ifndef INIT\n#  define INIT(x) x\n#  define INIT2(a, b) = {a, b}\n#  define INIT3(a, b, c) = {a, b, c}\n#  define INIT4(a, b, c, d) = {a, b, c, d}\n#  define INIT5(a, b, c, d, e) = {a, b, c, d, e}\n#  define INIT6(a, b, c, d, e, f) = {a, b, c, d, e, f}\n#  define DO_INIT\n# endif\n#endif"
    "#ifdef VIM_MAIN_C\n# define EXTERN\n# define INIT(x) x\n# define INIT2(a, b) = {a, b}\n# define INIT3(a, b, c) = {a, b, c}\n# define INIT4(a, b, c, d) = {a, b, c, d}\n# define INIT5(a, b, c, d, e) = {a, b, c, d, e}\n# define INIT6(a, b, c, d, e, f) = {a, b, c, d, e, f}\n# define DO_INIT\n#elif !defined(EXTERN)\n# define EXTERN extern\n# define INIT(x)\n# define INIT2(a, b)\n# define INIT3(a, b, c)\n# define INIT4(a, b, c, d)\n# define INIT5(a, b, c, d, e)\n# define INIT6(a, b, c, d, e, f)\n#else\n# ifndef INIT\n#  define INIT(x) x\n#  define INIT2(a, b) = {a, b}\n#  define INIT3(a, b, c) = {a, b, c}\n#  define INIT4(a, b, c, d) = {a, b, c, d}\n#  define INIT5(a, b, c, d, e) = {a, b, c, d, e}\n#  define INIT6(a, b, c, d, e, f) = {a, b, c, d, e, f}\n#  define DO_INIT\n# endif\n#endif"
    _src "${_src}")

# use uintptr_t rather than unsigned long
string(REPLACE
    "#ifdef _WIN64\ntypedef unsigned __int64\tlong_u;\ntypedef\t\t __int64\tlong_i;\n# define SCANF_HEX_LONG_U       \"%llx\"\n# define SCANF_DECIMAL_LONG_U   \"%llu\"\n# define PRINTF_HEX_LONG_U      \"0x%llx\"\n#else\ntypedef unsigned long\t\tlong_u;\ntypedef\t\t long\t\tlong_i;\n# define SCANF_HEX_LONG_U       \"%lx\"\n# define SCANF_DECIMAL_LONG_U   \"%lu\"\n# define PRINTF_HEX_LONG_U      \"0x%lx\"\n#endif"
    "#include <stdint.h>\ntypedef uintptr_t\tlong_u;\ntypedef intptr_t\tlong_i;\n# define SCANF_HEX_LONG_U       \"%llx\"\n# define SCANF_DECIMAL_LONG_U   \"%llu\"\n# define PRINTF_HEX_LONG_U      \"0x%llx\""
    _src "${_src}")
file(CONFIGURE OUTPUT "${GEN}/vim.h" CONTENT "${_src}")

add_custom_command(
    OUTPUT ${GEN}/os_unix.c
    COMMAND sh -c "cp '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/os_unix.c' '${GEN}/os_unix.c' && patch -p0 -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/tcgetwinsize.patch' && patch -p0 -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/waitpid.patch'"
    DEPENDS
        repo/src/os_unix.c
        ${CMAKE_CURRENT_SOURCE_DIR}/tcgetwinsize.patch
        ${CMAKE_CURRENT_SOURCE_DIR}/waitpid.patch
    VERBATIM
)

set(_xdiff_srcs
    repo/src/xdiff/xdiffi.c
    repo/src/xdiff/xemit.c
    repo/src/xdiff/xhistogram.c
    repo/src/xdiff/xpatience.c
    repo/src/xdiff/xprepare.c
    repo/src/xdiff/xutils.c
)
add_executable(vim
    repo/src/alloc.c
    repo/src/arabic.c
    repo/src/arglist.c
    repo/src/autocmd.c
    repo/src/beval.c
    repo/src/buffer.c
    repo/src/bufwrite.c
    repo/src/blob.c
    repo/src/blowfish.c
    repo/src/change.c
    repo/src/channel.c
    repo/src/charset.c
    repo/src/cindent.c
    repo/src/clientserver.c
    repo/src/clipboard.c
    repo/src/cmdexpand.c
    repo/src/cmdhist.c
    repo/src/crypt.c
    repo/src/crypt_zip.c
    repo/src/debugger.c
    repo/src/dict.c
    repo/src/diff.c
    repo/src/digraph.c
    repo/src/drawline.c
    repo/src/drawscreen.c
    repo/src/edit.c
    repo/src/eval.c
    repo/src/evalbuffer.c
    repo/src/evalfunc.c
    repo/src/evalvars.c
    repo/src/evalwindow.c
    repo/src/ex_cmds.c
    repo/src/ex_cmds2.c
    repo/src/ex_docmd.c
    repo/src/ex_eval.c
    repo/src/ex_getln.c
    repo/src/fileio.c
    repo/src/filepath.c
    repo/src/findfile.c
    repo/src/float.c
    repo/src/fold.c
    repo/src/fuzzy.c
    repo/src/gc.c
    repo/src/getchar.c
    repo/src/gui_xim.c
    repo/src/hardcopy.c
    repo/src/hashtab.c
    repo/src/help.c
    repo/src/highlight.c
    repo/src/if_cscope.c
    repo/src/if_xcmdsrv.c
    repo/src/indent.c
    repo/src/insexpand.c
    repo/src/json.c
    repo/src/linematch.c
    repo/src/list.c
    repo/src/locale.c
    repo/src/logfile.c
    repo/src/main.c
    repo/src/map.c
    repo/src/mark.c
    repo/src/match.c
    repo/src/mbyte.c
    repo/src/memfile.c
    repo/src/memline.c
    repo/src/menu.c
    repo/src/message.c
    repo/src/misc1.c
    repo/src/misc2.c
    repo/src/mouse.c
    repo/src/move.c
    repo/src/normal.c
    repo/src/ops.c
    repo/src/option.c
    repo/src/optionstr.c
    ${GEN}/os_unix.c
    repo/src/popupmenu.c
    repo/src/popupwin.c
    repo/src/profiler.c
    repo/src/pty.c
    repo/src/quickfix.c
    repo/src/regexp.c
    repo/src/register.c
    repo/src/screen.c
    repo/src/scriptfile.c
    repo/src/search.c
    repo/src/session.c
    repo/src/sha256.c
    repo/src/sign.c
    repo/src/sound.c
    repo/src/spell.c
    repo/src/spellfile.c
    repo/src/spellsuggest.c
    repo/src/strings.c
    repo/src/syntax.c
    repo/src/tabpanel.c
    repo/src/tag.c
    repo/src/term.c
    repo/src/terminal.c
    repo/src/testing.c
    repo/src/textformat.c
    repo/src/textobject.c
    repo/src/textprop.c
    repo/src/time.c
    repo/src/tuple.c
    repo/src/typval.c
    repo/src/ui.c
    repo/src/undo.c
    repo/src/usercmd.c
    repo/src/userfunc.c
    repo/src/version.c
    repo/src/vim9class.c
    repo/src/vim9cmds.c
    repo/src/vim9compile.c
    repo/src/vim9execute.c
    repo/src/vim9expr.c
    repo/src/vim9generics.c
    repo/src/vim9instr.c
    repo/src/vim9script.c
    repo/src/vim9type.c
    repo/src/viminfo.c
    repo/src/window.c
    ${_xdiff_srcs}
    pathdef.c
)

set_source_files_properties(repo/src/main.c PROPERTIES
    COMPILE_DEFINITIONS VIM_MAIN_C
)
set_source_files_properties(repo/src/spell.c PROPERTIES
    COMPILE_DEFINITIONS IN_SPELL_C
)
set_source_files_properties(repo/src/option.c PROPERTIES
    COMPILE_DEFINITIONS IN_OPTION_C
)

target_compile_options(vim PRIVATE
    "SHELL:-include ${GEN}/vim.h"
    ${USERSPACE_CFLAGS}
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-conversion
    -Wno-attributes
    $<$<CXX_COMPILER_ID:Clang>:
        -Wno-macro-redefined
        -Wno-inconsistent-dllimport
    >
)

target_compile_definitions(vim PRIVATE HAVE_CONFIG_H)

target_include_directories(vim PRIVATE
    ${GEN}
    ${GEN}/xdiff
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/proto
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/xdiff
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(vim PRIVATE tinfo libiconv moxnox)

install(TARGETS vim DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    ARCHIVE EXCLUDE_FROM_ALL
)

install(CODE "file(CREATE_LINK vim.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/vi.exe\" SYMBOLIC)")

install(DIRECTORY repo/runtime/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/vim/vim91
)

###

add_executable(xxd repo/src/xxd/xxd.c)

target_compile_definitions(xxd PRIVATE UNIX)

target_compile_options(xxd PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-sign-compare
    -Wno-conversion
)

target_link_libraries(xxd PRIVATE moxnox)

install(TARGETS xxd DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
