set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})

project(ncurses VERSION 6.5.20250802)

# Version
set(NCURSES_MAJOR ${PROJECT_VERSION_MAJOR})
set(NCURSES_MINOR ${PROJECT_VERSION_MINOR})
set(NCURSES_PATCH ${PROJECT_VERSION_PATCH})

# Template variables (shared by curses.h.in, unctrl.h.in, MKterm.h.awk.in)
set(EXP_WIN32_DRIVER 0)
set(HAVE_SGTTY_H 0)
set(HAVE_STDINT_H 1)
set(HAVE_STDNORETURN_H 0)
set(HAVE_TERMIO_H 0)
set(HAVE_TERMIOS_H 1)
set(HAVE_TCGETATTR 1)
set(HAVE_VSSCANF 1)

# curses.h type/feature knobs
set(BROKEN_LINKER 0)
set(cf_cv_1UL "1U")
set(cf_cv_enable_lp64 0) # Windows is LLP64, not LP64
set(cf_cv_enable_reentrant 0)
set(cf_cv_enable_sigwinch 0)
set(cf_cv_type_of_bool "unsigned char")
set(cf_cv_typeof_chtype "uint32_t")
set(cf_cv_typeof_mmask_t "uint32_t")
set(GENERATED_EXT_FUNCS "generated")
set(NCURSES_CCHARW_MAX 5)
set(NCURSES_CH_T "chtype") # narrow build
set(NCURSES_CONST "const")
set(NCURSES_EXT_COLORS 0) # requires wide char
set(NCURSES_EXT_FUNCS 1)
set(NCURSES_INLINE "inline")
set(NCURSES_INTEROP_FUNCS 1)
set(NCURSES_LIBUTF8 0)
set(NCURSES_MOUSE_VERSION 2)
set(NCURSES_OK_WCHAR_T "")
set(NCURSES_OPAQUE 0)
set(NCURSES_OPAQUE_FORM 0)
set(NCURSES_OPAQUE_MENU 0)
set(NCURSES_OPAQUE_PANEL 0)
set(NCURSES_REENTRANT 0)
set(NCURSES_RGB_COLORS 0)
set(NCURSES_SBOOL "signed char")
set(NCURSES_SIGWINCH 0)
set(NCURSES_SIZE_T "short")
set(NCURSES_SP_FUNCS ${NCURSES_PATCH})
set(NCURSES_TPARM_ARG "intptr_t")
set(NCURSES_TPARM_VARARGS 1)
set(NCURSES_USE_DATABASE 1)
set(NCURSES_USE_TERMCAP 0)
set(NCURSES_WATTR_MACROS 1)
set(NCURSES_WCHAR_T 0)
set(NCURSES_WCWIDTH_GRAPHICS 1) # use internal table; no wcwidth in narrow build
set(NCURSES_WINT_T 0)
set(NCURSES_WRAP_PREFIX "_nc_")
set(NCURSES_XNAMES 1)
set(NEED_WCHAR_H 0) # narrow build
set(USE_BUILTIN_BOOL 1)
set(USE_CXX_BOOL 0)
set(USE_STDBOOL_H 1)
set(NCURSES_OSPEED short)

configure_file(ncurses_cfg.h.in ${GEN}/ncurses_cfg.h @ONLY)

add_custom_command(
    OUTPUT ${GEN}/hashsize.h
    COMMAND sh -c "sh '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/MKhashsize.sh' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' > '${GEN}/hashsize.h'"
    DEPENDS
        repo/include/MKhashsize.sh
        repo/include/Caps
        repo/include/Caps-ncurses
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/parametrized.h
    COMMAND sh -c "AWK=awk sh '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/MKparametrized.sh' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' > '${GEN}/parametrized.h'"
    DEPENDS
        repo/include/MKparametrized.sh
        repo/include/Caps
        repo/include/Caps-ncurses
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/termsort.h
    COMMAND sh -c "sh '${CMAKE_CURRENT_SOURCE_DIR}/repo/progs/MKtermsort.sh' awk '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' > '${GEN}/termsort.h'"
    WORKING_DIRECTORY ${GEN}
    DEPENDS
        repo/progs/MKtermsort.sh
        repo/include/Caps
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/ncurses_def.h
    COMMAND sh -c "sh '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/MKncurses_def.sh' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/ncurses_defs' > '${GEN}/ncurses_def.h'"
    DEPENDS
        repo/include/MKncurses_def.sh
        repo/include/ncurses_defs
    VERBATIM
)

configure_file(repo/include/MKterm.h.awk.in ${GEN}/MKterm.h.awk @ONLY)
add_custom_command(
    OUTPUT ${GEN}/term.h
    COMMAND sh -c "awk -f '${GEN}/MKterm.h.awk' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' | sed '/cur_term/s/NCURSES_EXPORT_VAR/TINFO_EXPORT_VAR/' > '${GEN}/term.h'"
    DEPENDS
        ${GEN}/MKterm.h.awk
        repo/include/Caps
        repo/include/Caps-ncurses
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/ncurses_dll.h
    COMMAND sh -c "sed 's/@NCURSES_WRAP_PREFIX@/_nc_/;s/defined(BUILDING_NCURSES)/defined(BUILDING_NCURSES) || defined(BUILDING_TINFO)/' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/ncurses_dll.h.in' | sed '/^#define NCURSES_EXPORT_VAR/a #if defined(BUILDING_TINFO)\\n# define TINFO_EXPORT_VAR(type) NCURSES_EXPORT_GENERAL_EXPORT type\\n#elif defined(_WIN32) || defined(_WIN64)\\n# define TINFO_EXPORT_VAR(type) NCURSES_EXPORT_GENERAL_IMPORT type\\n#else\\n# define TINFO_EXPORT_VAR(type) type\\n#endif' > '${GEN}/ncurses_dll.h'"
    DEPENDS repo/include/ncurses_dll.h.in
    VERBATIM
)

configure_file(repo/include/curses.h.in ${GEN}/curses.h.in.generated @ONLY)

add_custom_command(
    OUTPUT ${GEN}/curses.h
    COMMAND sh -c "{ cat '${GEN}/curses.h.in.generated' && AWK=awk sh '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/MKkey_defs.sh' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' && cat '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/curses.tail'; } | sed '/\\(acs_map\\|curscr\\|newscr\\|stdscr\\|COLS\\|LINES\\|TABSIZE\\)/s/NCURSES_EXPORT_VAR/TINFO_EXPORT_VAR/' > '${GEN}/curses.h'"
    DEPENDS
        ${GEN}/curses.h.in.generated
        repo/include/MKkey_defs.sh
        repo/include/Caps
        repo/include/Caps-ncurses
        repo/include/curses.tail
    VERBATIM
)

configure_file(repo/include/unctrl.h.in ${GEN}/unctrl.h @ONLY)

configure_file(repo/include/termcap.h.in ${GEN}/termcap.h @ONLY)

add_custom_command(
    OUTPUT ${GEN}/init_keytry.h
    COMMAND sh -c "awk -f '${CMAKE_CURRENT_SOURCE_DIR}/MKinit_keytry.awk' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' > '${GEN}/init_keytry.h'"
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/MKinit_keytry.awk
        repo/include/Caps
        repo/include/Caps-ncurses
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/names.c
    COMMAND sh -c "awk -f '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/tinfo/MKnames.awk' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' > '${GEN}/names.c'"
    DEPENDS
        repo/ncurses/tinfo/MKnames.awk
        repo/include/Caps
        repo/include/Caps-ncurses
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/codes.c
    COMMAND sh -c "awk -f '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/tinfo/MKcodes.awk' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' > '${GEN}/codes.c'"
    DEPENDS
        repo/ncurses/tinfo/MKcodes.awk
        repo/include/Caps
        repo/include/Caps-ncurses
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib_unctrl.c
    COMMAND sh -c "echo '' | awk -f '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/base/MKunctrl.awk' > '${GEN}/lib_unctrl.c'"
    DEPENDS
        repo/ncurses/base/MKunctrl.awk
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/keys.list
    COMMAND sh -c "AWK=awk USE_SIGWINCH=0 sh '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/tinfo/MKkeys_list.sh' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' | LC_ALL=C sort > '${GEN}/keys.list'"
    DEPENDS
        repo/ncurses/tinfo/MKkeys_list.sh
        repo/include/Caps
        repo/include/Caps-ncurses
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib_keyname.c
    COMMAND sh -c "awk -f '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/base/MKkeyname.awk' use_sigwinch=0 bigstrings=0 '${GEN}/keys.list' > '${GEN}/lib_keyname.c'"
    DEPENDS
        repo/ncurses/base/MKkeyname.awk
        ${GEN}/keys.list
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib_gen.c
    COMMAND sh -c "FGREP=fgrep sh -e '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/base/MKlib_gen.sh' 'x86_64-w64-mingw32-gcc -E -I${GEN}' awk generated < '${GEN}/curses.h' > '${GEN}/lib_gen.c'"
    DEPENDS
        repo/ncurses/base/MKlib_gen.sh
        ${GEN}/curses.h
        ${GEN}/ncurses_cfg.h
        ${GEN}/ncurses_dll.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/comp_captab.c
    COMMAND sh -c "HASHTABSIZE=$(grep 'define CAPTABSIZE' '${GEN}/hashsize.h' | awk '{print $NF * 2}') && cat '${CMAKE_CURRENT_SOURCE_DIR}/comp_captab_head.c' > '${GEN}/comp_captab.c' && awk -v mode=info -v HASHTABSIZE=$HASHTABSIZE -f '${CMAKE_CURRENT_SOURCE_DIR}/MKhash.awk' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' >> '${GEN}/comp_captab.c' && awk -v mode=cap -v HASHTABSIZE=$HASHTABSIZE -f '${CMAKE_CURRENT_SOURCE_DIR}/MKhash.awk' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' >> '${GEN}/comp_captab.c' && awk -f '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/tinfo/MKcaptab.awk' bigstrings=0 tablename=capalias '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' >> '${GEN}/comp_captab.c' && awk -f '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/tinfo/MKcaptab.awk' bigstrings=0 tablename=infoalias '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' >> '${GEN}/comp_captab.c' && cat '${CMAKE_CURRENT_SOURCE_DIR}/comp_captab_tail.c' >> '${GEN}/comp_captab.c'"
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/comp_captab_head.c
        ${CMAKE_CURRENT_SOURCE_DIR}/comp_captab_tail.c
        ${CMAKE_CURRENT_SOURCE_DIR}/MKhash.awk
        repo/ncurses/tinfo/MKcaptab.awk
        repo/include/Caps
        repo/include/Caps-ncurses
        ${GEN}/hashsize.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/curses.priv.h
    COMMAND sh -c "sed 's/#if (defined(_WIN32) || defined(_WIN64))/#if (defined(_WIN32) || defined(_WIN64)) \\&\\& !defined(MOXNOX)/' '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/curses.priv.h' | sed '/\\(_nc_globals\\|_nc_prescreen\\|_nc_screen_chain\\| SP;\\)/s/NCURSES_EXPORT_VAR/TINFO_EXPORT_VAR/' | sed '/#include <tchar.h>/{n;s|#include <io.h>|#define NO_OLDNAMES\\n#include <io.h>\\n#undef NO_OLDNAMES|}' > '${GEN}/curses.priv.h'"
    DEPENDS repo/ncurses/curses.priv.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/term.priv.h
    COMMAND sh -c "sed '/\\(_nc_globals\\|_nc_prescreen\\)/s/NCURSES_EXPORT_VAR/TINFO_EXPORT_VAR/' '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/term.priv.h' > '${GEN}/term.priv.h'"
    DEPENDS repo/ncurses/term.priv.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/comp_userdefs.c
    COMMAND sh -c "HASHTABSIZE=$(grep 'define CAPTABSIZE' '${GEN}/hashsize.h' | awk '{print $NF * 2}') && cat '${CMAKE_CURRENT_SOURCE_DIR}/comp_userdefs_head.c' > '${GEN}/comp_userdefs.c' && awk -v mode=user -v HASHTABSIZE=$HASHTABSIZE -f '${CMAKE_CURRENT_SOURCE_DIR}/MKhash.awk' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/Caps-ncurses' >> '${GEN}/comp_userdefs.c' && cat '${CMAKE_CURRENT_SOURCE_DIR}/comp_userdefs_tail.c' >> '${GEN}/comp_userdefs.c'"
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/comp_userdefs_head.c
        ${CMAKE_CURRENT_SOURCE_DIR}/comp_userdefs_tail.c
        ${CMAKE_CURRENT_SOURCE_DIR}/MKhash.awk
        repo/include/Caps
        repo/include/Caps-ncurses
        ${GEN}/hashsize.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/write_entry.c
    COMMAND sh -c "sed '/\\t    rc = mkdir(path/{N;N;N;N;s/\\t    rc = mkdir(path\\n#ifndef _NC_WINDOWS_NATIVE\\n\\t\\t       ,0777\\n#endif\\n\\t\\t)/\\t    rc = mkdir(path, 0777)/}' '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/tinfo/write_entry.c' > '${GEN}/write_entry.c'"
    DEPENDS repo/ncurses/tinfo/write_entry.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib_setup.c ${GEN}/reset_cmd.c
    COMMAND sh -c "cp '${CMAKE_CURRENT_SOURCE_DIR}/repo/ncurses/tinfo/lib_setup.c' '${GEN}/lib_setup.c' && cp '${CMAKE_CURRENT_SOURCE_DIR}/repo/progs/reset_cmd.c' '${GEN}/reset_cmd.c' && patch -p0 -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/tcgetwinsize.patch'"
    DEPENDS
        repo/ncurses/tinfo/lib_setup.c
        repo/progs/reset_cmd.c
        tcgetwinsize.patch
    VERBATIM
)

# tinfo shared library

add_library(tinfo SHARED
    repo/ncurses/tinfo/access.c
    repo/ncurses/tinfo/add_tries.c
    repo/ncurses/tinfo/alloc_entry.c
    repo/ncurses/tinfo/alloc_ttype.c
    repo/ncurses/tinfo/captoinfo.c
    repo/ncurses/tinfo/comp_error.c
    repo/ncurses/tinfo/comp_expand.c
    repo/ncurses/tinfo/comp_hash.c
    repo/ncurses/tinfo/comp_parse.c
    repo/ncurses/tinfo/comp_scan.c
    repo/ncurses/tinfo/db_iterator.c
    repo/ncurses/tinfo/doalloc.c
    repo/ncurses/tinfo/entries.c
    repo/ncurses/tinfo/free_ttype.c
    repo/ncurses/tinfo/getenv_num.c
    repo/ncurses/tinfo/hashed_db.c
    repo/ncurses/tinfo/home_terminfo.c
    repo/ncurses/tinfo/init_keytry.c
    repo/ncurses/tinfo/lib_acs.c
    repo/ncurses/tinfo/lib_baudrate.c
    repo/ncurses/tinfo/lib_cur_term.c
    repo/ncurses/tinfo/lib_data.c
    repo/ncurses/tinfo/lib_has_cap.c
    repo/ncurses/tinfo/lib_kernel.c
    repo/ncurses/tinfo/lib_longname.c
    repo/ncurses/tinfo/lib_napms.c
    repo/ncurses/tinfo/lib_options.c
    repo/ncurses/tinfo/lib_raw.c
    ${GEN}/lib_setup.c
    repo/ncurses/tinfo/lib_termcap.c
    repo/ncurses/tinfo/lib_termname.c
    repo/ncurses/tinfo/lib_tgoto.c
    repo/ncurses/tinfo/lib_ti.c
    repo/ncurses/tinfo/lib_tparm.c
    repo/ncurses/tinfo/lib_tputs.c
    repo/ncurses/tinfo/lib_ttyflags.c
    repo/ncurses/tinfo/name_match.c
    repo/ncurses/tinfo/obsolete.c
    repo/ncurses/tinfo/parse_entry.c
    repo/ncurses/tinfo/read_entry.c
    repo/ncurses/tinfo/read_termcap.c
    repo/ncurses/tinfo/strings.c
    repo/ncurses/tinfo/trim_sgr0.c
    ${GEN}/write_entry.c
    repo/ncurses/base/define_key.c
    repo/ncurses/base/key_defined.c
    repo/ncurses/base/keybound.c
    repo/ncurses/base/tries.c
    repo/ncurses/trace/lib_trace.c
    repo/ncurses/trace/lib_traceatr.c
    repo/ncurses/trace/lib_tracechr.c
    repo/ncurses/trace/lib_tracebits.c
    repo/ncurses/trace/trace_buf.c
    repo/ncurses/trace/trace_tries.c
    repo/ncurses/trace/visbuf.c
    repo/ncurses/tty/lib_twait.c
    ${GEN}/names.c
    ${GEN}/codes.c
    ${GEN}/comp_captab.c
    ${GEN}/comp_userdefs.c
    ${GEN}/lib_keyname.c
    ${GEN}/lib_unctrl.c
    fallback.c
)

target_compile_definitions(tinfo PRIVATE BUILDING_TINFO WIN_PTHREADS_TIME_H TERMINFO=\"/share/terminfo\")
target_compile_options(tinfo PRIVATE ${USERSPACE_CFLAGS} -Wno-pointer-sign -Wno-conversion -Wno-unused-parameter -Wno-unused-but-set-variable)
target_include_directories(tinfo PUBLIC ${GEN})
target_include_directories(tinfo PRIVATE repo/ncurses repo/include)

add_custom_command(
    OUTPUT ${GEN}/terminfo.stamp
    COMMAND sh -c "rm -rf '${GEN}/terminfo' && '${HOST_TIC}' -x -o '${GEN}/terminfo' '${CMAKE_CURRENT_SOURCE_DIR}/repo/misc/terminfo.src' && touch '${GEN}/terminfo.stamp'"
    DEPENDS repo/misc/terminfo.src
    VERBATIM
)

add_custom_target(ncurses_terminfo ALL DEPENDS ${GEN}/terminfo.stamp)

install(DIRECTORY ${GEN}/terminfo/ DESTINATION ${CMAKE_INSTALL_PREFIX}/share/terminfo)

# ncurses shared library

add_library(ncurses SHARED
    repo/ncurses/base/keyok.c
    repo/ncurses/base/legacy_coding.c
    repo/ncurses/base/lib_addch.c
    repo/ncurses/base/lib_addstr.c
    repo/ncurses/base/lib_beep.c
    repo/ncurses/base/lib_bkgd.c
    repo/ncurses/base/lib_box.c
    repo/ncurses/base/lib_chgat.c
    repo/ncurses/base/lib_clear.c
    repo/ncurses/base/lib_clearok.c
    repo/ncurses/base/lib_clrbot.c
    repo/ncurses/base/lib_clreol.c
    repo/ncurses/base/lib_color.c
    repo/ncurses/base/lib_colorset.c
    repo/ncurses/base/lib_delch.c
    repo/ncurses/base/lib_delwin.c
    repo/ncurses/base/lib_dft_fgbg.c
    repo/ncurses/base/lib_echo.c
    repo/ncurses/base/lib_endwin.c
    repo/ncurses/base/lib_erase.c
    repo/ncurses/base/lib_flash.c
    repo/ncurses/base/lib_freeall.c
    repo/ncurses/base/lib_getch.c
    repo/ncurses/base/lib_getstr.c
    repo/ncurses/base/lib_hline.c
    repo/ncurses/base/lib_immedok.c
    repo/ncurses/base/lib_inchstr.c
    repo/ncurses/base/lib_initscr.c
    repo/ncurses/base/lib_insch.c
    repo/ncurses/base/lib_insdel.c
    repo/ncurses/base/lib_insnstr.c
    repo/ncurses/base/lib_instr.c
    repo/ncurses/base/lib_isendwin.c
    repo/ncurses/base/lib_leaveok.c
    repo/ncurses/base/lib_mouse.c
    repo/ncurses/base/lib_move.c
    repo/ncurses/base/lib_mvwin.c
    repo/ncurses/base/lib_newterm.c
    repo/ncurses/base/lib_newwin.c
    repo/ncurses/base/lib_nl.c
    repo/ncurses/base/lib_overlay.c
    repo/ncurses/base/lib_pad.c
    repo/ncurses/base/lib_printw.c
    repo/ncurses/base/lib_redrawln.c
    repo/ncurses/base/lib_refresh.c
    repo/ncurses/base/lib_restart.c
    repo/ncurses/base/lib_scanw.c
    repo/ncurses/base/lib_screen.c
    repo/ncurses/base/lib_scroll.c
    repo/ncurses/base/lib_scrollok.c
    repo/ncurses/base/lib_scrreg.c
    repo/ncurses/base/lib_set_term.c
    repo/ncurses/base/lib_slk.c
    repo/ncurses/base/lib_slkatrof.c
    repo/ncurses/base/lib_slkatron.c
    repo/ncurses/base/lib_slkatr_set.c
    repo/ncurses/base/lib_slkatrset.c
    repo/ncurses/base/lib_slkattr.c
    repo/ncurses/base/lib_slkclear.c
    repo/ncurses/base/lib_slkcolor.c
    repo/ncurses/base/lib_slkinit.c
    repo/ncurses/base/lib_slklab.c
    repo/ncurses/base/lib_slkrefr.c
    repo/ncurses/base/lib_slkset.c
    repo/ncurses/base/lib_slktouch.c
    repo/ncurses/base/lib_touch.c
    repo/ncurses/base/lib_ungetch.c
    repo/ncurses/base/lib_vline.c
    repo/ncurses/base/lib_wattroff.c
    repo/ncurses/base/lib_wattron.c
    repo/ncurses/base/lib_winch.c
    repo/ncurses/base/lib_window.c
    repo/ncurses/base/nc_panel.c
    repo/ncurses/base/new_pair.c
    repo/ncurses/base/resizeterm.c
    repo/ncurses/base/safe_sprintf.c
    repo/ncurses/base/use_window.c
    repo/ncurses/base/version.c
    repo/ncurses/base/vsscanf.c
    repo/ncurses/base/wresize.c
    repo/ncurses/tinfo/lib_print.c
    repo/ncurses/tinfo/use_screen.c
    repo/ncurses/trace/lib_tracedmp.c
    repo/ncurses/trace/lib_tracemse.c
    repo/ncurses/trace/trace_xnames.c
    repo/ncurses/trace/varargs.c
    repo/ncurses/tty/hardscroll.c
    repo/ncurses/tty/hashmap.c
    repo/ncurses/tty/lib_mvcur.c
    repo/ncurses/tty/lib_tstp.c
    repo/ncurses/tty/lib_vidattr.c
    repo/ncurses/tty/tty_update.c
    ${GEN}/lib_gen.c
)

target_compile_definitions(ncurses PRIVATE BUILDING_NCURSES WIN_PTHREADS_TIME_H)
target_compile_options(ncurses PRIVATE ${USERSPACE_CFLAGS} -Wno-pointer-sign -Wno-conversion -Wno-unused-parameter -Wno-unused-but-set-variable)
target_include_directories(ncurses PUBLIC ${GEN})
target_include_directories(ncurses PRIVATE repo/ncurses repo/include)
target_link_libraries(ncurses PUBLIC tinfo)

add_custom_command(
    OUTPUT ${GEN}/ncurses.h
    COMMAND ${CMAKE_COMMAND} -E copy ${GEN}/curses.h ${GEN}/ncurses.h
    DEPENDS ${GEN}/curses.h
    VERBATIM
)

add_custom_target(ncurses_generated_headers
    DEPENDS
        ${GEN}/hashsize.h
        ${GEN}/ncurses.h
        ${GEN}/parametrized.h
        ${GEN}/ncurses_def.h
        ${GEN}/init_keytry.h
        ${GEN}/term.h
        ${GEN}/curses.h
        ${GEN}/curses.priv.h
        ${GEN}/term.priv.h
        ${GEN}/ncurses_dll.h
        ${GEN}/termsort.h
)
add_dependencies(ncurses ncurses_generated_headers)
add_dependencies(tinfo ncurses_generated_headers)

# form shared library

add_library(form SHARED
    repo/form/f_trace.c
    repo/form/fld_arg.c
    repo/form/fld_attr.c
    repo/form/fld_current.c
    repo/form/fld_def.c
    repo/form/fld_dup.c
    repo/form/fld_ftchoice.c
    repo/form/fld_ftlink.c
    repo/form/fld_info.c
    repo/form/fld_just.c
    repo/form/fld_link.c
    repo/form/fld_max.c
    repo/form/fld_move.c
    repo/form/fld_newftyp.c
    repo/form/fld_opts.c
    repo/form/fld_pad.c
    repo/form/fld_page.c
    repo/form/fld_stat.c
    repo/form/fld_type.c
    repo/form/fld_user.c
    repo/form/frm_cursor.c
    repo/form/frm_data.c
    repo/form/frm_def.c
    repo/form/frm_driver.c
    repo/form/frm_hook.c
    repo/form/frm_opts.c
    repo/form/frm_page.c
    repo/form/frm_post.c
    repo/form/frm_req_name.c
    repo/form/frm_scale.c
    repo/form/frm_sub.c
    repo/form/frm_user.c
    repo/form/frm_win.c
    repo/form/fty_alnum.c
    repo/form/fty_alpha.c
    repo/form/fty_enum.c
    repo/form/fty_generic.c
    repo/form/fty_int.c
    repo/form/fty_ipv4.c
    repo/form/fty_num.c
    repo/form/fty_regex.c
)

target_compile_definitions(form PRIVATE BUILDING_NCURSES BUILDING_FORM WIN_PTHREADS_TIME_H)
target_compile_options(form PRIVATE ${USERSPACE_CFLAGS} -Wno-unused-parameter -Wno-unused-variable)
target_include_directories(form PRIVATE ${GEN} repo/ncurses repo/include repo/form repo/menu)
target_link_libraries(form PRIVATE ncurses)

# menu shared library

add_library(menu SHARED
    repo/menu/m_attribs.c
    repo/menu/m_cursor.c
    repo/menu/m_driver.c
    repo/menu/m_format.c
    repo/menu/m_global.c
    repo/menu/m_hook.c
    repo/menu/m_item_cur.c
    repo/menu/m_item_nam.c
    repo/menu/m_item_new.c
    repo/menu/m_item_opt.c
    repo/menu/m_item_top.c
    repo/menu/m_item_use.c
    repo/menu/m_item_val.c
    repo/menu/m_item_vis.c
    repo/menu/m_items.c
    repo/menu/m_new.c
    repo/menu/m_opts.c
    repo/menu/m_pad.c
    repo/menu/m_pattern.c
    repo/menu/m_post.c
    repo/menu/m_req_name.c
    repo/menu/m_scale.c
    repo/menu/m_spacing.c
    repo/menu/m_sub.c
    repo/menu/m_trace.c
    repo/menu/m_userptr.c
    repo/menu/m_win.c
)

target_compile_definitions(menu PRIVATE BUILDING_NCURSES BUILDING_MENU WIN_PTHREADS_TIME_H)
target_compile_options(menu PRIVATE ${USERSPACE_CFLAGS})
target_include_directories(menu PRIVATE ${GEN} repo/ncurses repo/include repo/menu)
target_link_libraries(menu PRIVATE ncurses)

# panel shared library

add_library(panel SHARED
    repo/panel/p_above.c
    repo/panel/p_below.c
    repo/panel/p_bottom.c
    repo/panel/p_delete.c
    repo/panel/p_hidden.c
    repo/panel/p_hide.c
    repo/panel/p_move.c
    repo/panel/p_new.c
    repo/panel/p_replace.c
    repo/panel/p_show.c
    repo/panel/p_top.c
    repo/panel/p_update.c
    repo/panel/p_user.c
    repo/panel/p_win.c
    repo/panel/panel.c
)

target_compile_definitions(panel PRIVATE BUILDING_NCURSES BUILDING_PANEL WIN_PTHREADS_TIME_H)
target_compile_options(panel PRIVATE ${USERSPACE_CFLAGS})
target_include_directories(panel PUBLIC repo/panel)
target_include_directories(panel PRIVATE ${GEN} repo/ncurses repo/include)
target_link_libraries(panel PRIVATE ncurses)

install(TARGETS tinfo ncurses form menu panel
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(FILES
    ${GEN}/curses.h
    ${GEN}/ncurses_dll.h
    ${GEN}/term.h
    ${GEN}/termcap.h
    ${GEN}/unctrl.h
    repo/include/term_entry.h
    repo/form/form.h
    repo/menu/eti.h
    repo/menu/menu.h
    repo/panel/panel.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
install(FILES ${GEN}/curses.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include RENAME ncurses.h)

function(ncurses_pc name description requires libs)
    set(NCURSES_PC_NAME ${name})
    set(NCURSES_PC_DESCRIPTION "${description}")
    set(NCURSES_PC_REQUIRES "${requires}")
    set(NCURSES_PC_LIBS "${libs}")
    configure_file(ncurses-lib.pc.in ${GEN}/${name}.pc @ONLY)
    install(FILES ${GEN}/${name}.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
endfunction()

ncurses_pc(ncurses "library" "" "-lncurses -ltinfo")
ncurses_pc(tinfo "terminal interface library" "" "-ltinfo")
ncurses_pc(form "add-on library" "ncurses" "-lform")
ncurses_pc(menu "add-on library" "ncurses" "-lmenu")
ncurses_pc(panel "add-on library" "ncurses" "-lpanel")

# clear

add_executable(clear
    repo/progs/clear.c
    repo/progs/clear_cmd.c
    repo/progs/tty_settings.c
)

target_compile_options(clear PRIVATE ${USERSPACE_CFLAGS})
target_compile_definitions(clear PRIVATE WIN_PTHREADS_TIME_H _UCRT)
target_include_directories(clear PRIVATE ${GEN} repo/include repo/ncurses repo/progs ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(clear PRIVATE moxnox ncurses)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(clear PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS clear DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# tset / reset

add_executable(tset
    repo/progs/tset.c
    ${GEN}/reset_cmd.c
    repo/progs/transform.c
    repo/progs/tty_settings.c
)

target_compile_options(tset PRIVATE ${USERSPACE_CFLAGS})
target_compile_definitions(tset PRIVATE WIN_PTHREADS_TIME_H _UCRT)
target_include_directories(tset PRIVATE ${GEN} repo/include repo/ncurses repo/progs ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tset PRIVATE moxnox ncurses)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(tset PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tset DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(CODE "file(CREATE_LINK tset.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/reset.exe\" SYMBOLIC)")

# tic / captoinfo / infotocap

add_executable(tic
    repo/progs/tic.c
    repo/progs/dump_entry.c
    repo/progs/tparm_type.c
    repo/progs/transform.c
)

target_compile_options(tic PRIVATE ${USERSPACE_CFLAGS})
target_compile_definitions(tic PRIVATE WIN_PTHREADS_TIME_H _UCRT)
target_include_directories(tic PRIVATE ${GEN} repo/include repo/ncurses repo/progs ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tic PRIVATE moxnox ncurses)
add_dependencies(tic ncurses_generated_headers)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(tic PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tic DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(CODE "file(CREATE_LINK tic.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/captoinfo.exe\" SYMBOLIC)")
install(CODE "file(CREATE_LINK tic.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/infotocap.exe\" SYMBOLIC)")

# infocmp

add_executable(infocmp
    repo/progs/infocmp.c
    repo/progs/dump_entry.c
)

target_compile_options(infocmp PRIVATE ${USERSPACE_CFLAGS})
target_compile_definitions(infocmp PRIVATE WIN_PTHREADS_TIME_H _UCRT)
target_include_directories(infocmp PRIVATE ${GEN} repo/include repo/ncurses repo/progs ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(infocmp PRIVATE moxnox ncurses)
add_dependencies(infocmp ncurses_generated_headers)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(infocmp PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS infocmp DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# toe

add_executable(toe
    repo/progs/toe.c
)

target_compile_options(toe PRIVATE ${USERSPACE_CFLAGS})
target_compile_definitions(toe PRIVATE WIN_PTHREADS_TIME_H _UCRT)
target_include_directories(toe PRIVATE ${GEN} repo/include repo/ncurses repo/progs ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(toe PRIVATE moxnox ncurses)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(toe PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS toe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# tput

add_executable(tput
    repo/progs/tput.c
    repo/progs/clear_cmd.c
    ${GEN}/reset_cmd.c
    repo/progs/tparm_type.c
    repo/progs/transform.c
    repo/progs/tty_settings.c
)

target_compile_options(tput PRIVATE ${USERSPACE_CFLAGS})
target_compile_definitions(tput PRIVATE WIN_PTHREADS_TIME_H _UCRT)
target_include_directories(tput PRIVATE ${GEN} repo/include repo/ncurses repo/progs ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tput PRIVATE moxnox ncurses)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(tput PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tput DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

# tabs

add_executable(tabs
    repo/progs/tabs.c
    repo/progs/tty_settings.c
)

target_compile_options(tabs PRIVATE ${USERSPACE_CFLAGS})
target_compile_definitions(tabs PRIVATE WIN_PTHREADS_TIME_H _UCRT)
target_include_directories(tabs PRIVATE ${GEN} repo/include repo/ncurses repo/progs ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(tabs PRIVATE moxnox ncurses)

if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_link_options(tabs PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS tabs DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

####

add_executable(ncurses-test ncurses-test.cpp)

target_compile_options(ncurses-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(ncurses-test moxnox)
target_link_libraries(ncurses-test ncurses)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(ncurses-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS ncurses-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

function(add_ncurses_test name)
    add_executable(${name} ${ARGN})
    target_compile_options(${name} PRIVATE ${USERSPACE_CFLAGS} -Wno-implicit-fallthrough)
    target_compile_definitions(${name} PRIVATE WIN_PTHREADS_TIME_H)
    target_include_directories(${name} PRIVATE repo/include repo/test)
    target_link_libraries(${name} moxnox ncurses)
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_link_options(${name} PUBLIC -mcrtdll=ucrtbase)
    endif()
    install(TARGETS ${name} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
endfunction()

add_ncurses_test(firework repo/test/firework.c)
add_ncurses_test(rain repo/test/rain.c repo/test/popup_msg.c)
add_ncurses_test(worm repo/test/worm.c)
add_ncurses_test(gdc repo/test/gdc.c)
add_ncurses_test(hanoi repo/test/hanoi.c)
add_ncurses_test(xmas repo/test/xmas.c)
add_ncurses_test(newdemo repo/test/newdemo.c)
add_ncurses_test(knight repo/test/knight.c)
add_ncurses_test(lrtest repo/test/lrtest.c)
add_ncurses_test(insdelln repo/test/insdelln.c repo/test/popup_msg.c)
add_ncurses_test(hashtest repo/test/hashtest.c)
add_ncurses_test(firstlast repo/test/firstlast.c)
add_ncurses_test(movewindow repo/test/movewindow.c repo/test/popup_msg.c)
