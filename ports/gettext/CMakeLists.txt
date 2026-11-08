project(gettext VERSION 1.0)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
set(TS_LOCAL ${REPO}/libtextstyle/gnulib-local/lib)

foreach(_dir runtime/gnu/malloc tools/gnu/malloc textstyle/gnu textstyle/glib textstyle/textstyle src intl scripts)
    file(MAKE_DIRECTORY ${GEN}/${_dir})
endforeach()

find_program(HOST_XZ xz REQUIRED)

set(GNULIB_LIB_DIR ${REPO}/gnulib/lib)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)

function(gettext_gnulib_header template output)
    if(NOT IS_ABSOLUTE ${template})
        set(template ${CMAKE_CURRENT_SOURCE_DIR}/${template})
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${template})
    file(READ ${template} content)
    get_filename_component(_name ${template} NAME)
    if(_name STREQUAL "error.in.h")
        string(REPLACE "#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n/* Print a message with 'fprintf"
            "#if GNULIB_REPLACE_ERROR\n# undef error_print_progname\n# undef error_message_count\n# undef error_one_per_line\n# define error_print_progname rpl_error_print_progname\n# define error_message_count rpl_error_message_count\n# define error_one_per_line rpl_error_one_per_line\n#endif\n\n#ifdef __cplusplus\nextern \"C\" {\n#endif\n\n/* Print a message with 'fprintf"
            content "${content}")
        string(REPLACE "extern void (*error_print_progname) (void);" "extern DLL_VARIABLE void (*error_print_progname) (void);" content "${content}")
        string(REPLACE "extern unsigned int error_message_count;" "extern DLL_VARIABLE unsigned int error_message_count;" content "${content}")
        string(REPLACE "extern int error_one_per_line;" "extern DLL_VARIABLE int error_one_per_line;" content "${content}")
    endif()
    string(REPLACE "/* The definition of _GL_ARG_NONNULL is copied here.  */"
        "/* The definition of _GL_ARG_NONNULL is copied here.  */\n${GNULIB_SNIPPET_ARG_NONNULL}"
        content "${content}")
    string(REPLACE "/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */"
        "/* The definitions of _GL_FUNCDECL_RPL etc. are copied here.  */\n${GNULIB_SNIPPET_CXXDEFS}"
        content "${content}")
    string(REPLACE "/* The definition of _GL_WARN_ON_USE is copied here.  */"
        "/* The definition of _GL_WARN_ON_USE is copied here.  */\n${GNULIB_SNIPPET_WARN_ON_USE}"
        content "${content}")
    string(REPLACE "/* The definition of _Noreturn is copied here.  */"
        "/* The definition of _Noreturn is copied here.  */\n${GNULIB_SNIPPET_NORETURN}"
        content "${content}")
    file(CONFIGURE OUTPUT ${output} CONTENT "${content}" @ONLY)
endfunction()

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(HAVE_VISIBILITY 0)
else()
    set(HAVE_VISIBILITY 1)
endif()

configure_file(config-runtime.h.in ${GEN}/runtime/config.h @ONLY)
configure_file(config-tools.h.in ${GEN}/tools/config.h @ONLY)
configure_file(config-textstyle.h.in ${GEN}/textstyle/config.h @ONLY)

include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

file(READ ${REPO}/gnulib/lib/malloc/scratch_buffer.h SCRATCH_H)
string(REPLACE "__always_inline" "inline _GL_ATTRIBUTE_ALWAYS_INLINE" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_likely" "_GL_LIKELY" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_unlikely" "_GL_UNLIKELY" SCRATCH_H "${SCRATCH_H}")
string(REGEX REPLACE "[^\n]*libc_hidden_proto[^\n]*\n" "" SCRATCH_H "${SCRATCH_H}")
file(CONFIGURE OUTPUT ${GEN}/runtime/gnu/malloc/scratch_buffer.gl.h CONTENT "${SCRATCH_H}" @ONLY)
file(CONFIGURE OUTPUT ${GEN}/tools/gnu/malloc/scratch_buffer.gl.h CONTENT "${SCRATCH_H}" @ONLY)

file(READ ${REPO}/gnulib/lib/obstack.in.h OBSTACK_H)
string(REPLACE "defined __GL_GNULIB_HEADER" "1" OBSTACK_H "${OBSTACK_H}")
string(REPLACE "defined __GL_REPLACE_OBSTACK__" "0" OBSTACK_H "${OBSTACK_H}")
string(REPLACE "defined __GL_SMALL_PTRDIFF_T__" "0" OBSTACK_H "${OBSTACK_H}")
file(CONFIGURE OUTPUT ${GEN}/tools/gnu/obstack.h CONTENT "${OBSTACK_H}" @ONLY)

file(MAKE_DIRECTORY ${GEN}/tools/gnu/unicase ${GEN}/tools/gnu/uninorm)
add_custom_command(
    OUTPUT ${GEN}/tools/gnu/unicase/special-casing-table.h
    COMMAND ${HOST_GPERF} -m 10 ${REPO}/gnulib/lib/unicase/special-casing-table.gperf > ${GEN}/tools/gnu/unicase/special-casing-table.h
    DEPENDS ${REPO}/gnulib/lib/unicase/special-casing-table.gperf
    VERBATIM
)
add_custom_command(
    OUTPUT ${GEN}/tools/gnu/uninorm/composition-table.h
    COMMAND ${HOST_GPERF} -m 1 ${REPO}/gnulib/lib/uninorm/composition-table.gperf > ${GEN}/tools/gnu/uninorm/composition-table.h-t
    COMMAND ${HOST_SED} -e "s/^const/static const/" ${GEN}/tools/gnu/uninorm/composition-table.h-t > ${GEN}/tools/gnu/uninorm/composition-table.h
    DEPENDS ${REPO}/gnulib/lib/uninorm/composition-table.gperf
    VERBATIM
)

file(READ ${REPO}/gettext-runtime/intl/libgnuintl.in.h LIBGNUINTL_H)
file(READ ${REPO}/gettext-runtime/intl/export.h INTL_EXPORT_H)
string(REGEX REPLACE "extern ([^\"])" "extern LIBINTL_SHLIB_EXPORTED \\1" LIBGNUINTL_H "${LIBGNUINTL_H}")
string(REPLACE "#define _LIBINTL_H 1\n" "#define _LIBINTL_H 1\n${INTL_EXPORT_H}" LIBGNUINTL_H "${LIBGNUINTL_H}")
set(HAVE_POSIX_PRINTF 1)
set(HAVE_ASPRINTF 1)
set(HAVE_SNPRINTF 1)
set(HAVE_WPRINTF 0)
set(HAVE_NEWLOCALE 0)
set(ENHANCE_LOCALE_FUNCS 0)
set(WOE32DLL 0)
file(CONFIGURE OUTPUT ${GEN}/intl/libgnuintl.h CONTENT "${LIBGNUINTL_H}" @ONLY)

foreach(_h glibconfig.h glib.h glib/ghash.h glib/glist.h glib/gmacros.h glib/gprimes.h glib/gprintfint.h glib/gstrfuncs.h glib/gstring.h glib/gtypes.h)
    string(REGEX REPLACE "\\.h$" ".in.h" _in ${_h})
    configure_file(${TS_LOCAL}/${_in} ${GEN}/textstyle/${_h} COPYONLY)
endforeach()
set(HEXVERSION 0x010000)
set(HAVE_ICONV 1)
configure_file(${REPO}/libtextstyle/lib/textstyle/version.in.h ${GEN}/textstyle/textstyle/version.h @ONLY)
set(DLL_VARIABLE "")
configure_file(${REPO}/libtextstyle/lib/textstyle/woe32dll.in.h ${GEN}/textstyle/textstyle/woe32dll.h @ONLY)

add_custom_command(
    OUTPUT ${GEN}/textstyle/ostream.c ${GEN}/textstyle/ostream.h ${GEN}/textstyle/ostream.priv.h ${GEN}/textstyle/ostream.vt.h ${GEN}/textstyle/ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/ostream.oo.c ${TS_LOCAL}/ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/ostream.oo.c ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/ostream.oo.c ${TS_LOCAL}/ostream.oo.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/styled-ostream.c ${GEN}/textstyle/styled-ostream.h ${GEN}/textstyle/styled_ostream.priv.h ${GEN}/textstyle/styled_ostream.vt.h ${GEN}/textstyle/styled-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/styled-ostream.oo.c ${TS_LOCAL}/styled-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/styled-ostream.oo.c ${GEN}/textstyle/styled-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/styled-ostream.oo.c ${TS_LOCAL}/styled-ostream.oo.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/fd-ostream.c ${GEN}/textstyle/fd-ostream.h ${GEN}/textstyle/fd_ostream.priv.h ${GEN}/textstyle/fd_ostream.vt.h ${GEN}/textstyle/fd-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/fd-ostream.oo.c ${TS_LOCAL}/fd-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/fd-ostream.oo.c ${GEN}/textstyle/fd-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/fd-ostream.oo.c ${TS_LOCAL}/fd-ostream.oo.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/file-ostream.c ${GEN}/textstyle/file-ostream.h ${GEN}/textstyle/file_ostream.priv.h ${GEN}/textstyle/file_ostream.vt.h ${GEN}/textstyle/file-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/file-ostream.oo.c ${TS_LOCAL}/file-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/file-ostream.oo.c ${GEN}/textstyle/file-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/file-ostream.oo.c ${TS_LOCAL}/file-ostream.oo.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/html-ostream.c ${GEN}/textstyle/html-ostream.h ${GEN}/textstyle/html_ostream.priv.h ${GEN}/textstyle/html_ostream.vt.h ${GEN}/textstyle/html-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/html-ostream.oo.c ${TS_LOCAL}/html-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/html-ostream.oo.c ${GEN}/textstyle/html-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/html-ostream.oo.c ${TS_LOCAL}/html-ostream.oo.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/iconv-ostream.c ${GEN}/textstyle/iconv-ostream.h ${GEN}/textstyle/iconv_ostream.priv.h ${GEN}/textstyle/iconv_ostream.vt.h ${GEN}/textstyle/iconv-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/iconv-ostream.oo.c ${TS_LOCAL}/iconv-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/iconv-ostream.oo.c ${GEN}/textstyle/iconv-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/iconv-ostream.oo.c ${TS_LOCAL}/iconv-ostream.oo.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/memory-ostream.c ${GEN}/textstyle/memory-ostream.h ${GEN}/textstyle/memory_ostream.priv.h ${GEN}/textstyle/memory_ostream.vt.h ${GEN}/textstyle/memory-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/memory-ostream.oo.c ${TS_LOCAL}/memory-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/memory-ostream.oo.c ${GEN}/textstyle/memory-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/memory-ostream.oo.c ${TS_LOCAL}/memory-ostream.oo.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/term-ostream.c ${GEN}/textstyle/term-ostream.h ${GEN}/textstyle/term_ostream.priv.h ${GEN}/textstyle/term_ostream.vt.h ${GEN}/textstyle/term-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/term-ostream.oo.c ${TS_LOCAL}/term-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/term-ostream.oo.c ${GEN}/textstyle/term-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/term-ostream.oo.c ${TS_LOCAL}/term-ostream.oo.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/html-styled-ostream.c ${GEN}/textstyle/html-styled-ostream.h ${GEN}/textstyle/html_styled_ostream.priv.h ${GEN}/textstyle/html_styled_ostream.vt.h ${GEN}/textstyle/html-styled-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/html-styled-ostream.oo.c ${TS_LOCAL}/html-styled-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/html-styled-ostream.oo.c ${GEN}/textstyle/html-styled-ostream.oo.h ${GEN}/textstyle/styled-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/html-styled-ostream.oo.c ${TS_LOCAL}/html-styled-ostream.oo.h ${GEN}/textstyle/styled-ostream.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/noop-styled-ostream.c ${GEN}/textstyle/noop-styled-ostream.h ${GEN}/textstyle/noop_styled_ostream.priv.h ${GEN}/textstyle/noop_styled_ostream.vt.h ${GEN}/textstyle/noop-styled-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/noop-styled-ostream.oo.c ${TS_LOCAL}/noop-styled-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/noop-styled-ostream.oo.c ${GEN}/textstyle/noop-styled-ostream.oo.h ${GEN}/textstyle/styled-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/noop-styled-ostream.oo.c ${TS_LOCAL}/noop-styled-ostream.oo.h ${GEN}/textstyle/styled-ostream.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/textstyle/term-styled-ostream.c ${GEN}/textstyle/term-styled-ostream.h ${GEN}/textstyle/term_styled_ostream.priv.h ${GEN}/textstyle/term_styled_ostream.vt.h ${GEN}/textstyle/term-styled-ostream.oo.h
    COMMAND ${CMAKE_COMMAND} -E copy ${TS_LOCAL}/term-styled-ostream.oo.c ${TS_LOCAL}/term-styled-ostream.oo.h ${GEN}/textstyle
    COMMAND sh ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${GEN}/textstyle/term-styled-ostream.oo.c ${GEN}/textstyle/term-styled-ostream.oo.h ${GEN}/textstyle/styled-ostream.oo.h ${GEN}/textstyle/ostream.oo.h
    DEPENDS ${REPO}/libtextstyle/gnulib-local/build-aux/moopp ${TS_LOCAL}/term-styled-ostream.oo.c ${TS_LOCAL}/term-styled-ostream.oo.h ${GEN}/textstyle/styled-ostream.h ${GEN}/textstyle/ostream.h
    VERBATIM
)

foreach(_y read-po-gram cldr-plural)
    add_custom_command(
        OUTPUT ${GEN}/src/${_y}.c ${GEN}/src/${_y}.h
        COMMAND ${HOST_BISON} -d -o ${GEN}/src/${_y}.c ${REPO}/gettext-tools/src/${_y}.y
        DEPENDS ${REPO}/gettext-tools/src/${_y}.y
        VERBATIM
    )
endforeach()

set(INTL_GEN ${CMAKE_CURRENT_BINARY_DIR}/gettext-runtime/intl)
file(MAKE_DIRECTORY ${INTL_GEN})
add_custom_command(
    OUTPUT ${INTL_GEN}/plural.c ${INTL_GEN}/plural.h
    COMMAND ${HOST_BISON} --name-prefix=__gettext -d -o ${INTL_GEN}/plural.c ${REPO}/gettext-runtime/intl/plural.y
    DEPENDS ${REPO}/gettext-runtime/intl/plural.y
    VERBATIM
)
set_source_files_properties(repo/gettext-tools/src/plural-exp.c PROPERTIES OBJECT_DEPENDS ${INTL_GEN}/plural.c)

add_custom_command(
    OUTPUT ${GEN}/src/xgettext.c ${GEN}/src/msginit.c
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/gettext-tools/src/xgettext.c ${REPO}/gettext-tools/src/msginit.c ${GEN}/src
    COMMAND ${HOST_PATCH} -p1 -d ${GEN}/src < ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    DEPENDS ${REPO}/gettext-tools/src/xgettext.c ${REPO}/gettext-tools/src/msginit.c ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    VERBATIM
)

set(PACKAGE gettext-tools)
set(VERSION ${PROJECT_VERSION})
set(ARCHIVE_FORMAT dirxz)
set(ARCHIVE_VERSION ${PROJECT_VERSION})
set(prefix /)
set(exec_prefix "\${prefix}")
set(bindir "\${exec_prefix}/bin")
set(datarootdir "\${prefix}/share")
set(datadir "\${datarootdir}")
set(libexecdir "\${exec_prefix}/libexec")
set(localedir "\${datarootdir}/locale")
set(PATH_SEPARATOR ":")
set(RELOCATABLE no)
configure_file(${REPO}/gettext-tools/autotools/autopoint.in ${GEN}/scripts/autopoint @ONLY)
configure_file(${REPO}/gettext-tools/wizard/gettextize.in ${GEN}/scripts/gettextize @ONLY)
configure_file(${REPO}/gettext-tools/misc/po-fetch.in ${GEN}/scripts/po-fetch @ONLY)
configure_file(${REPO}/gettext-tools/src/user-email.sh.in ${GEN}/scripts/user-email @ONLY)
set(PACKAGE gettext-runtime)
configure_file(${REPO}/gettext-runtime/src/gettext.sh.in ${GEN}/scripts/gettext.sh @ONLY)

add_custom_command(
    OUTPUT ${GEN}/archive.dir.tar.xz
    COMMAND ${HOST_XZ} -c -5 ${REPO}/gettext-tools/autotools/archive.dir.tar > ${GEN}/archive.dir.tar.xz
    DEPENDS ${REPO}/gettext-tools/autotools/archive.dir.tar
    VERBATIM
)
add_custom_target(gettext_archive ALL DEPENDS ${GEN}/archive.dir.tar.xz)

set(GETTEXT_CFLAGS ${USERSPACE_CFLAGS} -std=gnu23
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-unused-value
    -Wno-unused-but-set-variable
    -Wno-type-limits
    -Wno-empty-body
    -Wno-attributes
    -Wno-float-conversion
    -Wno-unused-const-variable
    $<$<C_COMPILER_ID:GNU>:-Wno-cast-function-type -Wno-implicit-fallthrough>
    $<$<C_COMPILER_ID:Clang>:-Wno-implicit-int-conversion -Wno-shorten-64-to-32 -Wno-implicit-int-float-conversion -Wno-format-security -Wno-tautological-constant-out-of-range-compare -Wno-string-conversion -Wno-missing-field-initializers -Wno-pointer-bool-conversion -Wno-parentheses-equality -Wno-cast-function-type-mismatch>
)

###

add_library(gettext_grt STATIC
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/binary-io.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/c32isalnum.c
    repo/gnulib/lib/c32iscntrl.c
    repo/gnulib/lib/c32isspace.c
    repo/gnulib/lib/c32width.c
    repo/gnulib-local/lib/closeout.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/full-write.c
    repo/gnulib/lib/fwriteerror.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/malloca.c
    repo/gnulib/lib/mbchar.c
    repo/gnulib/lib/mbslen.c
    repo/gnulib/lib/mbsstr.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/options.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/propername.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/safe-write.c
    repo/gnulib/lib/setlocale.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/striconv.c
    repo/gnulib/lib/string-buffer.c
    repo/gnulib/lib/string-desc.c
    repo/gnulib/lib/string.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/trim.c
    repo/gnulib/lib/unictype/ctype_alnum.c
    repo/gnulib/lib/unictype/ctype_cntrl.c
    repo/gnulib/lib/unictype/ctype_space.c
    repo/gnulib/lib/uniwidth/width.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xstriconv.c
    repo/gnulib/lib/xstring-buffer.c
)
target_compile_definitions(gettext_grt PRIVATE HAVE_CONFIG_H EXEEXT=".exe")
target_include_directories(gettext_grt PUBLIC
    ${GEN}/runtime
    ${GEN}/runtime/gnu
    ${REPO}/gnulib-local/lib
    ${REPO}/gnulib/lib
    ${REPO}/gettext-runtime/intl
)
target_compile_options(gettext_grt PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(gettext_grt PUBLIC libiconv moxnox)

foreach(_p gettext ngettext envsubst)
    add_executable(${_p}_exe repo/gettext-runtime/src/${_p}.c)
    set_target_properties(${_p}_exe PROPERTIES OUTPUT_NAME ${_p})
    target_compile_definitions(${_p}_exe PRIVATE HAVE_CONFIG_H LOCALEDIR="/share/locale" INSTALLDIR="/bin")
    target_compile_options(${_p}_exe PRIVATE ${GETTEXT_CFLAGS})
    target_link_libraries(${_p}_exe PRIVATE gettext_grt)
endforeach()

###

add_library(libtextstyle SHARED
    repo/gnulib/lib/gl_array_list.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/cloexec.c
    repo/gnulib/lib/concat-filename.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/fatal-signal.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/full-write.c
    repo/gnulib/lib/get_ppid_of.c
    repo/gnulib/lib/get_progname_of.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/gl_list.c
    repo/gnulib/lib/localeconv.c
    repo/gnulib/lib/lstat.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/mem-hash-map.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/next-prime.c
    repo/gnulib/lib/obstack.c
    repo/gnulib/lib/glthread/once.c
    repo/gnulib/lib/open.c
    repo/gnulib/lib/printf-frexp.c
    repo/gnulib/lib/printf-frexpl.c
    repo/gnulib/lib/realloc.c
    repo/gnulib/lib/safe-read.c
    repo/gnulib/lib/safe-write.c
    repo/gnulib/lib/same-inode.c
    repo/gnulib/lib/sig-handler.c
    repo/gnulib/lib/signbitl.c
    repo/gnulib/lib/stat.c
    repo/gnulib/lib/stat-time.c
    repo/gnulib/lib/str_endswith.c
    repo/gnulib/lib/str_startswith.c
    repo/gnulib/lib/string.c
    repo/gnulib/lib/term-style-control.c
    repo/gnulib/lib/unistr/u8-mbtouc.c
    repo/gnulib/lib/vaszprintf.c
    repo/gnulib/lib/vsnzprintf.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xconcat-filename.c
    repo/gnulib/lib/xgethostname.c
    repo/gnulib/lib/gl_xlist.c
    repo/gnulib/lib/xsize.c
    repo/gnulib/lib/xvasprintf.c
    repo/gnulib/lib/xasprintf.c
    repo/gnulib/lib/calloc.c
    repo/gnulib/lib/malloc.c
    repo/gnulib/lib/printf-args.c
    repo/gnulib/lib/printf-parse.c
    repo/gnulib/lib/tparm.c
    repo/gnulib/lib/tputs.c
    repo/gnulib/lib/vasnprintf.c
    ${GEN}/textstyle/fd-ostream.c
    ${GEN}/textstyle/file-ostream.c
    ${GEN}/textstyle/html-ostream.c
    ${GEN}/textstyle/html-styled-ostream.c
    ${GEN}/textstyle/iconv-ostream.c
    ${GEN}/textstyle/memory-ostream.c
    ${GEN}/textstyle/noop-styled-ostream.c
    ${GEN}/textstyle/ostream.c
    ${GEN}/textstyle/styled-ostream.c
    ${GEN}/textstyle/term-ostream.c
    ${GEN}/textstyle/term-styled-ostream.c
    repo/libtextstyle/lib/color.c
    repo/libtextstyle/lib/misc.c
    repo/libtextstyle/lib/version.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-additional-sel.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-attr-sel.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-cascade.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-declaration.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-doc-handler.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-enc-handler.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-fonts.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-input.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-num.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-om-parser.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-parser.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-parsing-location.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-prop-list.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-pseudo.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-rgb.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-sel-eng.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-selector.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-simple-sel.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-statement.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-string.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-style.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-stylesheet.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-term.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-tknzr.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-token.c
    repo/libtextstyle/gnulib-local/lib/libcroco/cr-utils.c
    repo/libtextstyle/gnulib-local/lib/glib/ghash.c
    repo/libtextstyle/gnulib-local/lib/glib/glist.c
    repo/libtextstyle/gnulib-local/lib/glib/gmessages.c
    repo/libtextstyle/gnulib-local/lib/glib/gprimes.c
    repo/libtextstyle/gnulib-local/lib/glib/gstrfuncs.c
    repo/libtextstyle/gnulib-local/lib/glib/gstring.c
)
set_target_properties(libtextstyle PROPERTIES OUTPUT_NAME textstyle PREFIX lib)
target_compile_definitions(libtextstyle PRIVATE HAVE_CONFIG_H IN_LIBTEXTSTYLE DEPENDS_ON_LIBICONV=1)
target_include_directories(libtextstyle PRIVATE
    ${GEN}/textstyle
    ${GEN}/textstyle/gnu
    ${GEN}/textstyle/glib
    ${TS_LOCAL}
    ${TS_LOCAL}/glib
    ${TS_LOCAL}/libcroco
    ${REPO}/libtextstyle/lib
    ${REPO}/gnulib/lib
)
target_include_directories(libtextstyle PUBLIC ${REPO}/libtextstyle/lib ${GEN}/textstyle)
target_compile_options(libtextstyle PRIVATE ${GETTEXT_CFLAGS} -fvisibility=default)
target_link_libraries(libtextstyle PRIVATE LibXml2 libiconv moxnox)

###

set(GETTEXT_TOOLS_DEFS
    HAVE_CONFIG_H
    EXEEXT=".exe"
    LOCALEDIR="/share/locale"
    USEJAVA=0
    GETTEXTJAR="/share/gettext/gettext.jar"
    BINDIR="/bin"
    LIBDIR="/lib"
    LIBEXECDIR="/libexec"
    GETTEXTDATADIR="/share/gettext"
    PROJECTSDIR="/share/gettext/projects"
)
set(GETTEXT_TOOLS_INCLUDES
    ${GEN}/tools
    ${GEN}/tools/gnu
    ${GEN}/src
    ${GEN}/intl
    ${REPO}/gettext-tools/src
    ${REPO}/gettext-tools/libgrep
    ${REPO}/gnulib-local/lib
    ${REPO}/gnulib/lib
    ${REPO}/gettext-runtime/intl
)

add_library(gettext_lib STATIC
    repo/gnulib/lib/access.c
    repo/gnulib/lib/argmatch.c
    repo/gnulib/lib/asyncsafe-spin.c
    repo/gnulib-local/lib/backupfile.c
    repo/gnulib-local/lib/addext.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/bcp47.c
    repo/gnulib/lib/binary-io.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/btoc32.c
    repo/gnulib/lib/byteswap.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/c-strncasecmp.c
    repo/gnulib/lib/c-strstr.c
    repo/gnulib/lib/c32_apply_type_test.c
    repo/gnulib/lib/c32_get_type_test.c
    repo/gnulib/lib/c32isalnum.c
    repo/gnulib/lib/c32isalpha.c
    repo/gnulib/lib/c32isblank.c
    repo/gnulib/lib/c32iscntrl.c
    repo/gnulib/lib/c32isdigit.c
    repo/gnulib/lib/c32isgraph.c
    repo/gnulib/lib/c32islower.c
    repo/gnulib/lib/c32isprint.c
    repo/gnulib/lib/c32ispunct.c
    repo/gnulib/lib/c32isspace.c
    repo/gnulib/lib/c32isupper.c
    repo/gnulib/lib/c32isxdigit.c
    repo/gnulib/lib/c32tolower.c
    repo/gnulib/lib/c32width.c
    repo/gnulib/lib/canonicalize.c
    repo/gnulib/lib/canonicalize-lgpl.c
    repo/gnulib/lib/classpath.c
    repo/gnulib/lib/clean-temp.c
    repo/gnulib/lib/clean-temp-simple.c
    repo/gnulib-local/lib/closeout.c
    repo/gnulib/lib/copy-file.c
    repo/gnulib/lib/copy-file-range.c
    repo/gnulib/lib/csharpcomp.c
    repo/gnulib/lib/csharpexec.c
    repo/gnulib/lib/cygpath.c
    repo/gnulib/lib/dirname.c
    repo/gnulib/lib/dirname-lgpl.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib-local/lib/error-progname.c
    repo/gnulib/lib/execute.c
    repo/gnulib/lib/fd-safer-flag.c
    repo/gnulib/lib/dup-safer-flag.c
    repo/gnulib/lib/fdopendir.c
    repo/gnulib/lib/file-set.c
    repo/gnulib/lib/findprog.c
    repo/gnulib/lib/findprog-in.c
    repo/gnulib/lib/fstrcmp.c
    repo/gnulib/lib/fwriteerror.c
    repo/gnulib/lib/gcd.c
    repo/gnulib/lib/getcwd.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/getrandom.c
    repo/gnulib/lib/gettime.c
    repo/gnulib/lib/malloc/scratch_buffer_grow.c
    repo/gnulib/lib/malloc/scratch_buffer_grow_preserve.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/gl_hash_map.c
    repo/gnulib/lib/gl_hash_set.c
    repo/gnulib/lib/hashcode-named-file.c
    repo/gnulib/lib/hashcode-string2.c
    repo/gnulib/lib/hashkey-string.c
    repo/gnulib/lib/issymlink.c
    repo/gnulib/lib/iswctype.c
    repo/gnulib/lib/javacomp.c
    repo/gnulib/lib/javaexec.c
    repo/gnulib/lib/javaversion.c
    repo/gnulib/lib/gl_linked_list.c
    repo/gnulib/lib/gl_linkedhash_list.c
    repo/gnulib/lib/localename.c
    repo/gnulib/lib/localename-environ.c
    repo/gnulib/lib/localename-unsafe.c
    repo/gnulib/lib/malloca.c
    repo/gnulib/lib/gl_map.c
    repo/gnulib/lib/mbchar.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbslen.c
    repo/gnulib/lib/mbsrtoc32s.c
    repo/gnulib/lib/mbsstr.c
    repo/gnulib/lib/mbswidth.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/memset_explicit.c
    repo/gnulib/lib/options.c
    repo/gnulib/lib/pipe-filter-ii.c
    repo/gnulib/lib/pipe-filter-aux.c
    repo/gnulib/lib/pipe2-safer.c
    repo/gnulib/lib/spawn.c
    repo/gnulib/lib/spawni.c
    repo/gnulib/lib/spawn_faction_addchdir.c
    repo/gnulib/lib/spawn_faction_addclose.c
    repo/gnulib/lib/spawn_faction_adddup2.c
    repo/gnulib/lib/spawn_faction_addopen.c
    repo/gnulib/lib/spawn_faction_destroy.c
    repo/gnulib/lib/spawn_faction_init.c
    repo/gnulib/lib/spawnattr_destroy.c
    repo/gnulib/lib/spawnattr_init.c
    repo/gnulib/lib/spawnattr_setflags.c
    repo/gnulib/lib/spawnattr_setsigmask.c
    repo/gnulib/lib/spawnp.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/propername.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/read-file.c
    repo/gnulib/lib/readlink.c
    repo/gnulib/lib/rmdir.c
    repo/gnulib/lib/gl_set.c
    repo/gnulib/lib/setenv.c
    repo/gnulib/lib/setlocale.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/sf-istream.c
    repo/gnulib/lib/sh-quote.c
    repo/gnulib/lib/spawn-pipe.c
    repo/gnulib/lib/glthread/spin.c
    repo/gnulib/lib/strchrnul.c
    repo/gnulib/lib/strerror.c
    repo/gnulib/lib/strerror-override.c
    repo/gnulib/lib/striconv.c
    repo/gnulib/lib/striconveh.c
    repo/gnulib/lib/striconveha.c
    repo/gnulib/lib/string-buffer.c
    repo/gnulib/lib/string-buffer-printf.c
    repo/gnulib/lib/string-desc.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/strtol.c
    repo/gnulib/lib/strtoul.c
    repo/gnulib/lib/supersede.c
    repo/gnulib/lib/tempname.c
    repo/gnulib/lib/tmpdir.c
    repo/gnulib/lib/trim.c
    repo/gnulib/lib/unicase/cased.c
    repo/gnulib/lib/unicase/empty-prefix-context.c
    repo/gnulib/lib/unicase/empty-suffix-context.c
    repo/gnulib/lib/unicase/ignorable.c
    repo/gnulib/lib/unicase/special-casing.c
    repo/gnulib/lib/unicase/tocasefold.c
    repo/gnulib/lib/unicase/tolower.c
    repo/gnulib/lib/unicase/u8-casefold.c
    repo/gnulib/lib/unicase/u8-casemap.c
    repo/gnulib/lib/unicase/u8-ct-casefold.c
    repo/gnulib/lib/uniconv/u8-conv-from-enc.c
    repo/gnulib/lib/unictype/combiningclass.c
    repo/gnulib/lib/unictype/ctype_alnum.c
    repo/gnulib/lib/unictype/ctype_alpha.c
    repo/gnulib/lib/unictype/ctype_blank.c
    repo/gnulib/lib/unictype/ctype_cntrl.c
    repo/gnulib/lib/unictype/ctype_digit.c
    repo/gnulib/lib/unictype/ctype_graph.c
    repo/gnulib/lib/unictype/ctype_lower.c
    repo/gnulib/lib/unictype/ctype_print.c
    repo/gnulib/lib/unictype/ctype_punct.c
    repo/gnulib/lib/unictype/ctype_space.c
    repo/gnulib/lib/unictype/ctype_upper.c
    repo/gnulib/lib/unictype/ctype_xdigit.c
    repo/gnulib/lib/unictype/pr_soft_dotted.c
    repo/gnulib/lib/unictype/pr_white_space.c
    repo/gnulib/lib/unictype/pr_xid_continue.c
    repo/gnulib/lib/unictype/pr_xid_start.c
    repo/gnulib/lib/unictype/sy_java_whitespace.c
    repo/gnulib/lib/unilbrk/lbrktables.c
    repo/gnulib/lib/unilbrk/u8-possible-linebreaks.c
    repo/gnulib/lib/unilbrk/u8-width-linebreaks.c
    repo/gnulib/lib/unilbrk/ulc-common.c
    repo/gnulib/lib/unilbrk/ulc-width-linebreaks.c
    repo/gnulib/lib/uniname/uniname.c
    repo/gnulib/lib/uninorm/canonical-decomposition.c
    repo/gnulib/lib/uninorm/composition.c
    repo/gnulib/lib/uninorm/decompose-internal.c
    repo/gnulib/lib/uninorm/decomposing-form.c
    repo/gnulib/lib/uninorm/decomposition-table.c
    repo/gnulib/lib/uninorm/nfc.c
    repo/gnulib/lib/uninorm/nfd.c
    repo/gnulib/lib/uninorm/u8-normalize.c
    repo/gnulib/lib/unistr/u16-mbtouc-aux.c
    repo/gnulib/lib/unistr/u32-chr.c
    repo/gnulib/lib/unistr/u32-cpy.c
    repo/gnulib/lib/unistr/u32-pcpy.c
    repo/gnulib/lib/unistr/u32-strcat.c
    repo/gnulib/lib/unistr/u32-strlen.c
    repo/gnulib/lib/unistr/u8-check.c
    repo/gnulib/lib/unistr/u8-cpy.c
    repo/gnulib/lib/unistr/u8-mblen.c
    repo/gnulib/lib/unistr/u8-mbtouc-aux.c
    repo/gnulib/lib/unistr/u8-mbtouc-unsafe-aux.c
    repo/gnulib/lib/unistr/u8-mbtoucr.c
    repo/gnulib/lib/unistr/u8-prev.c
    repo/gnulib/lib/unistr/u8-uctomb-aux.c
    repo/gnulib/lib/uniwidth/width.c
    repo/gnulib/lib/unlink.c
    repo/gnulib/lib/unsetenv.c
    repo/gnulib/lib/utimens.c
    repo/gnulib/lib/vc-mtime.c
    repo/gnulib/lib/wait-process.c
    repo/gnulib/lib/windows-cygpath.c
    repo/gnulib-local/lib/xerror.c
    repo/gnulib/lib/xgetcwd.c
    repo/gnulib/lib/xmalloca.c
    repo/gnulib/lib/gl_xmap.c
    repo/gnulib/lib/xmemdup0.c
    repo/gnulib/lib/gl_xset.c
    repo/gnulib/lib/xsetenv.c
    repo/gnulib/lib/xstrerror.c
    repo/gnulib/lib/xstriconv.c
    repo/gnulib/lib/xstriconveh.c
    repo/gnulib/lib/xstring-buffer.c
    repo/gnulib/lib/xstring-buffer-printf.c
    repo/gnulib/lib/xstring-desc.c
    repo/gnulib/lib/xstrtoul.c
    repo/gnulib/lib/fnmatch.c
    repo/gnulib/lib/mbsrtoc32s-state.c
)
set_source_files_properties(repo/gnulib/lib/unicase/special-casing.c PROPERTIES OBJECT_DEPENDS ${GEN}/tools/gnu/unicase/special-casing-table.h)
set_source_files_properties(repo/gnulib/lib/uninorm/composition.c PROPERTIES OBJECT_DEPENDS ${GEN}/tools/gnu/uninorm/composition-table.h)
target_compile_definitions(gettext_lib PRIVATE HAVE_CONFIG_H EXEEXT=".exe" IN_LIBGETTEXTLIB DEPENDS_ON_LIBICONV=1 DEPENDS_ON_LIBINTL=1)
target_include_directories(gettext_lib PRIVATE ${GEN}/tools/gnu/uninorm)
target_include_directories(gettext_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/tools
    ${GEN}/tools/gnu
    ${REPO}/gnulib-local/lib
    ${REPO}/gnulib/lib
    ${REPO}/gettext-runtime/intl
)
target_compile_options(gettext_lib PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(gettext_lib PUBLIC LibXml2 libiconv moxnox)

add_library(gettext_grep STATIC
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/kwset.c
    repo/gnulib/lib/memchr2.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/wctype-h.c
    repo/gettext-tools/libgrep/m-fgrep.c
    repo/gettext-tools/libgrep/m-regex.c
)
target_compile_definitions(gettext_grep PRIVATE HAVE_CONFIG_H IN_GETTEXT_TOOLS_LIBGREP)
target_include_directories(gettext_grep PRIVATE
    ${GEN}/tools
    ${GEN}/tools/gnu
    ${REPO}/gettext-tools/libgrep
    ${REPO}/gnulib/lib
    ${REPO}/gettext-runtime/intl
)
target_compile_options(gettext_grep PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(gettext_grep PUBLIC gettext_lib moxnox)

add_library(gettext_src STATIC
    repo/gettext-tools/src/message.c
    repo/gettext-tools/src/pos.c
    repo/gettext-tools/src/po-error.c
    repo/gettext-tools/src/xerror-handler.c
    repo/gettext-tools/src/read-catalog-abstract.c
    repo/gettext-tools/src/read-po-lex.c
    repo/gettext-tools/src/po-charset.c
    repo/gettext-tools/src/read-po.c
    repo/gettext-tools/src/read-properties.c
    repo/gettext-tools/src/read-stringtable.c
    repo/gettext-tools/src/open-catalog.c
    repo/gettext-tools/src/dir-list.c
    repo/gettext-tools/src/str-list.c
    repo/gettext-tools/src/read-catalog-special.c
    repo/gettext-tools/src/read-catalog.c
    repo/gettext-tools/src/read-catalog-file.c
    repo/gettext-tools/src/write-catalog.c
    repo/gettext-tools/src/write-properties.c
    repo/gettext-tools/src/write-stringtable.c
    repo/gettext-tools/src/write-po.c
    repo/gettext-tools/src/msgl-ascii.c
    repo/gettext-tools/src/msgl-ofn.c
    repo/gettext-tools/src/msgl-iconv.c
    repo/gettext-tools/src/msgl-equal.c
    repo/gettext-tools/src/msgl-cat.c
    repo/gettext-tools/src/msgl-header.c
    repo/gettext-tools/src/msgl-english.c
    repo/gettext-tools/src/msgl-check.c
    repo/gettext-tools/src/file-list.c
    repo/gettext-tools/src/msgl-charset.c
    repo/gettext-tools/src/msgl-fsearch.c
    repo/gettext-tools/src/lang-table.c
    repo/gettext-tools/src/po-time.c
    repo/gettext-tools/src/plural-exp.c
    repo/gettext-tools/src/plural-eval.c
    repo/gettext-tools/src/plural-table.c
    repo/gettext-tools/src/plural-count.c
    repo/gettext-tools/src/sentence.c
    repo/gettext-tools/src/format.c
    repo/gettext-tools/src/format-c.c
    repo/gettext-tools/src/format-c++-brace.c
    repo/gettext-tools/src/format-python.c
    repo/gettext-tools/src/format-python-brace.c
    repo/gettext-tools/src/format-java.c
    repo/gettext-tools/src/format-java-printf.c
    repo/gettext-tools/src/format-csharp.c
    repo/gettext-tools/src/format-javascript.c
    repo/gettext-tools/src/format-scheme.c
    repo/gettext-tools/src/format-lisp.c
    repo/gettext-tools/src/format-elisp.c
    repo/gettext-tools/src/format-librep.c
    repo/gettext-tools/src/format-rust.c
    repo/gettext-tools/src/format-go.c
    repo/gettext-tools/src/format-ruby.c
    repo/gettext-tools/src/format-sh.c
    repo/gettext-tools/src/format-sh-printf.c
    repo/gettext-tools/src/format-awk.c
    repo/gettext-tools/src/format-lua.c
    repo/gettext-tools/src/format-pascal.c
    repo/gettext-tools/src/format-modula2.c
    repo/gettext-tools/src/format-d.c
    repo/gettext-tools/src/format-ocaml.c
    repo/gettext-tools/src/format-smalltalk.c
    repo/gettext-tools/src/format-qt.c
    repo/gettext-tools/src/format-qt-plural.c
    repo/gettext-tools/src/format-kde.c
    repo/gettext-tools/src/format-kde-kuit.c
    repo/gettext-tools/src/format-boost.c
    repo/gettext-tools/src/format-tcl.c
    repo/gettext-tools/src/format-perl.c
    repo/gettext-tools/src/format-perl-brace.c
    repo/gettext-tools/src/format-php.c
    repo/gettext-tools/src/format-gcc-internal.c
    repo/gettext-tools/src/format-gfc-internal.c
    repo/gettext-tools/src/read-desktop.c
    repo/gettext-tools/src/locating-rules.c
    repo/gettext-tools/src/its.c
    repo/gettext-tools/src/search-path.c
    ${GEN}/src/read-po-gram.c
)
target_compile_definitions(gettext_src PRIVATE ${GETTEXT_TOOLS_DEFS} IN_LIBGETTEXTSRC)
target_include_directories(gettext_src PUBLIC ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(gettext_src PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(gettext_src PUBLIC gettext_lib gettext_grep libtextstyle LibXml2 libiconv moxnox)

add_executable(msgcmp
    repo/gettext-tools/src/msgcmp.c
    repo/gettext-tools/src/msgl-fsearch.c
)
set_target_properties(msgcmp PROPERTIES OUTPUT_NAME msgcmp)
target_compile_definitions(msgcmp PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgcmp PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgcmp PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgcmp PRIVATE gettext_src)

add_executable(msgfmt
    repo/gettext-tools/src/msgfmt.c
    repo/gettext-tools/src/write-mo.c
    repo/gettext-tools/src/write-java.c
    repo/gettext-tools/src/write-csharp.c
    repo/gettext-tools/src/write-resources.c
    repo/gettext-tools/src/write-tcl.c
    repo/gettext-tools/src/write-qt.c
    repo/gettext-tools/src/write-desktop.c
    repo/gettext-tools/src/write-xml.c
    repo/gettext-runtime/intl/hash-string.c
)
set_target_properties(msgfmt PROPERTIES OUTPUT_NAME msgfmt)
target_compile_definitions(msgfmt PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgfmt PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgfmt PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgfmt PRIVATE gettext_src)

add_executable(msgmerge
    repo/gettext-tools/src/msgmerge.c
    repo/gettext-tools/src/msgl-merge.c
)
set_target_properties(msgmerge PROPERTIES OUTPUT_NAME msgmerge)
target_compile_definitions(msgmerge PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgmerge PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgmerge PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgmerge PRIVATE gettext_src)

add_executable(msgunfmt
    repo/gettext-tools/src/msgunfmt.c
    repo/gettext-tools/src/read-mo.c
    repo/gettext-tools/src/read-java.c
    repo/gettext-tools/src/read-csharp.c
    repo/gettext-tools/src/read-resources.c
    repo/gettext-tools/src/read-tcl.c
    repo/gettext-runtime/intl/hash-string.c
)
set_target_properties(msgunfmt PROPERTIES OUTPUT_NAME msgunfmt)
target_compile_definitions(msgunfmt PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgunfmt PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgunfmt PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgunfmt PRIVATE gettext_src)

add_executable(xgettext
    ${GEN}/src/xgettext.c
    repo/gettext-tools/src/if-error.c
    repo/gettext-tools/src/xg-pos.c
    repo/gettext-tools/src/xg-encoding.c
    repo/gettext-tools/src/xg-mixed-string.c
    repo/gettext-tools/src/xg-formatstring.c
    repo/gettext-tools/src/xg-arglist-context.c
    repo/gettext-tools/src/xg-arglist-callshape.c
    repo/gettext-tools/src/xg-arglist-parser.c
    repo/gettext-tools/src/xg-message.c
    repo/gettext-tools/src/xg-check.c
    repo/gettext-tools/src/x-po.c
    repo/gettext-tools/src/x-c.c
    repo/gettext-tools/src/x-python.c
    repo/gettext-tools/src/x-java.c
    repo/gettext-tools/src/x-csharp.c
    repo/gettext-tools/src/x-javascript.c
    repo/gettext-tools/src/x-scheme.c
    repo/gettext-tools/src/x-lisp.c
    repo/gettext-tools/src/x-elisp.c
    repo/gettext-tools/src/x-librep.c
    repo/gettext-tools/src/x-ruby.c
    repo/gettext-tools/src/x-sh.c
    repo/gettext-tools/src/x-awk.c
    repo/gettext-tools/src/x-lua.c
    repo/gettext-tools/src/x-modula2.c
    repo/gettext-tools/src/x-smalltalk.c
    repo/gettext-tools/src/x-vala.c
    repo/gettext-tools/src/x-tcl.c
    repo/gettext-tools/src/x-perl.c
    repo/gettext-tools/src/x-php.c
    repo/gettext-tools/src/x-ycp.c
    repo/gettext-tools/src/x-rst.c
    repo/gettext-tools/src/x-desktop.c
)
set_target_properties(xgettext PROPERTIES OUTPUT_NAME xgettext)
target_compile_definitions(xgettext PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(xgettext PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(xgettext PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(xgettext PRIVATE gettext_src)

add_executable(msgattrib
    repo/gettext-tools/src/msgattrib.c
)
set_target_properties(msgattrib PROPERTIES OUTPUT_NAME msgattrib)
target_compile_definitions(msgattrib PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgattrib PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgattrib PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgattrib PRIVATE gettext_src)

add_executable(msgcat
    repo/gettext-tools/src/msgcat.c
)
set_target_properties(msgcat PROPERTIES OUTPUT_NAME msgcat)
target_compile_definitions(msgcat PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgcat PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgcat PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgcat PRIVATE gettext_src)

add_executable(msgcomm
    repo/gettext-tools/src/msgcomm.c
)
set_target_properties(msgcomm PROPERTIES OUTPUT_NAME msgcomm)
target_compile_definitions(msgcomm PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgcomm PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgcomm PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgcomm PRIVATE gettext_src)

add_executable(msgconv
    repo/gettext-tools/src/msgconv.c
)
set_target_properties(msgconv PROPERTIES OUTPUT_NAME msgconv)
target_compile_definitions(msgconv PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgconv PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgconv PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgconv PRIVATE gettext_src)

add_executable(msgen
    repo/gettext-tools/src/msgen.c
)
set_target_properties(msgen PROPERTIES OUTPUT_NAME msgen)
target_compile_definitions(msgen PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgen PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgen PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgen PRIVATE gettext_src)

add_executable(msgexec
    repo/gettext-tools/src/msgexec.c
)
set_target_properties(msgexec PROPERTIES OUTPUT_NAME msgexec)
target_compile_definitions(msgexec PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgexec PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgexec PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgexec PRIVATE gettext_src)

add_executable(msgfilter
    repo/gettext-tools/src/msgfilter.c
    repo/gettext-tools/src/filter-sr-latin.c
    repo/gettext-tools/src/filter-quote.c
)
set_target_properties(msgfilter PROPERTIES OUTPUT_NAME msgfilter)
target_compile_definitions(msgfilter PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msgfilter PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msgfilter PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msgfilter PRIVATE gettext_src)

add_executable(msggrep
    repo/gettext-tools/src/msggrep.c
)
set_target_properties(msggrep PROPERTIES OUTPUT_NAME msggrep)
target_compile_definitions(msggrep PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msggrep PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msggrep PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msggrep PRIVATE gettext_src)

add_executable(msginit
    ${GEN}/src/msginit.c
    repo/gettext-tools/src/msgl-merge.c
    repo/gettext-runtime/intl/localealias.c
)
set_target_properties(msginit PROPERTIES OUTPUT_NAME msginit)
target_compile_definitions(msginit PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msginit PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msginit PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msginit PRIVATE gettext_src)

add_executable(msguniq
    repo/gettext-tools/src/msguniq.c
)
set_target_properties(msguniq PROPERTIES OUTPUT_NAME msguniq)
target_compile_definitions(msguniq PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(msguniq PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(msguniq PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(msguniq PRIVATE gettext_src)

add_executable(recode_sr_latin
    repo/gettext-tools/src/recode-sr-latin.c
    repo/gettext-tools/src/filter-sr-latin.c
)
set_target_properties(recode_sr_latin PROPERTIES OUTPUT_NAME recode-sr-latin)
target_compile_definitions(recode_sr_latin PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/bin")
target_include_directories(recode_sr_latin PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(recode_sr_latin PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(recode_sr_latin PRIVATE gettext_src)

add_executable(gettext_hostname
    repo/gettext-tools/src/hostname.c
)
set_target_properties(gettext_hostname PROPERTIES OUTPUT_NAME hostname)
target_compile_definitions(gettext_hostname PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/libexec/gettext")
target_include_directories(gettext_hostname PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(gettext_hostname PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(gettext_hostname PRIVATE gettext_src)

add_executable(gettext_urlget
    repo/gettext-tools/src/urlget.c
)
set_target_properties(gettext_urlget PROPERTIES OUTPUT_NAME urlget)
target_compile_definitions(gettext_urlget PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/libexec/gettext")
target_include_directories(gettext_urlget PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(gettext_urlget PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(gettext_urlget PRIVATE gettext_src)

add_executable(gettext_cldr_plurals
    ${GEN}/src/cldr-plural.c
    repo/gettext-tools/src/cldr-plural-exp.c
    repo/gettext-tools/src/cldr-plurals.c
)
set_target_properties(gettext_cldr_plurals PROPERTIES OUTPUT_NAME cldr-plurals)
target_compile_definitions(gettext_cldr_plurals PRIVATE ${GETTEXT_TOOLS_DEFS} INSTALLDIR="/libexec/gettext")
target_include_directories(gettext_cldr_plurals PRIVATE ${GETTEXT_TOOLS_INCLUDES})
target_compile_options(gettext_cldr_plurals PRIVATE ${GETTEXT_CFLAGS})
target_link_libraries(gettext_cldr_plurals PRIVATE gettext_src)

add_custom_target(gettext)
add_dependencies(gettext gettext_exe ngettext_exe envsubst_exe libtextstyle gettext_archive
    msgcmp msgfmt msgmerge msgunfmt xgettext msgattrib msgcat msgcomm msgconv msgen msgexec msgfilter msggrep msginit msguniq recode_sr_latin gettext_hostname gettext_urlget gettext_cldr_plurals)

###

install(TARGETS gettext_exe ngettext_exe envsubst_exe msgcmp msgfmt msgmerge msgunfmt xgettext msgattrib msgcat msgcomm msgconv msgen msgexec msgfilter msggrep msginit msguniq recode_sr_latin
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS gettext_hostname gettext_urlget gettext_cldr_plurals DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec/gettext)
install(TARGETS libtextstyle DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(PROGRAMS ${GEN}/scripts/gettext.sh ${GEN}/scripts/autopoint ${GEN}/scripts/gettextize ${GEN}/scripts/po-fetch
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(PROGRAMS ${GEN}/scripts/user-email repo/gettext-tools/src/project-id
    DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec/gettext)

install(FILES repo/libtextstyle/lib/textstyle.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES ${GEN}/textstyle/textstyle/version.h ${GEN}/textstyle/textstyle/woe32dll.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/textstyle)

install(FILES repo/gettext-runtime/m4/nls.m4 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/aclocal)

install(FILES
    ${GEN}/archive.dir.tar.xz
    repo/gettext-runtime/ABOUT-NLS
    repo/gnulib/build-aux/config.rpath
    repo/gettext-tools/misc/disclaim-translations.txt
    repo/gnulib-local/lib/gettext.h
    repo/gnulib/lib/javaversion.class
    repo/gettext-tools/src/msgunfmt.tcl
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/gettext
)
install(FILES
    repo/gnulib/m4/build-to-host.m4
    repo/gettext-runtime/m4/gettext.m4
    repo/gnulib/m4/host-cpu-c-abi.m4
    repo/gnulib/m4/iconv.m4
    repo/gnulib/m4/intlmacosx.m4
    repo/gnulib/m4/lib-ld.m4
    repo/gnulib/m4/lib-link.m4
    repo/gnulib/m4/lib-prefix.m4
    repo/gettext-runtime/m4/nls.m4
    repo/gettext-runtime/m4/po.m4
    repo/gettext-runtime/m4/progtest.m4
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/gettext/m4
)
install(FILES
    repo/gettext-tools/wizard/po-templates/traditional/Makefile.in.in
    repo/gettext-tools/wizard/po-templates/traditional/Makevars.template
    repo/gettext-tools/wizard/po-templates/traditional/Rules-quot
    repo/gettext-tools/wizard/po-templates/traditional/boldquot.sed
    repo/gettext-tools/wizard/po-templates/traditional/en@boldquot.header
    repo/gettext-tools/wizard/po-templates/traditional/en@quot.header
    repo/gettext-tools/wizard/po-templates/traditional/fetch-po
    repo/gettext-tools/wizard/po-templates/traditional/insert-header.sed
    repo/gettext-tools/wizard/po-templates/traditional/quot.sed
    repo/gettext-tools/wizard/po-templates/traditional/remove-potcdate.sed
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/gettext/po
)
install(FILES
    repo/gettext-tools/src/its-extensions.xsd
    repo/gettext-tools/src/its.xsd10
    repo/gettext-tools/src/its.xsd11
    repo/gettext-tools/src/locating-rules.xsd10
    repo/gettext-tools/src/locating-rules.xsd11
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/gettext/schema
)
install(FILES repo/gettext-tools/styles/po-default.css repo/gettext-tools/styles/po-vim.css
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/gettext/styles)
install(FILES repo/gettext-tools/projects/index repo/gettext-tools/projects/team-address
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/gettext/projects)
foreach(_proj GNOME KDE TP)
    install(FILES
        repo/gettext-tools/projects/${_proj}/team-address
        repo/gettext-tools/projects/${_proj}/teams.html
        repo/gettext-tools/projects/${_proj}/teams.url
        repo/gettext-tools/projects/${_proj}/trigger
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/gettext/projects/${_proj}
    )
endforeach()
install(FILES
    repo/gettext-tools/its/docbook.loc
    repo/gettext-tools/its/docbook4.its
    repo/gettext-tools/its/docbook5.its
    repo/gettext-tools/its/glade.loc
    repo/gettext-tools/its/glade1.its
    repo/gettext-tools/its/glade2.its
    repo/gettext-tools/its/gsettings.its
    repo/gettext-tools/its/gsettings.loc
    repo/gettext-tools/its/gtkbuilder.its
    repo/gettext-tools/its/metainfo.its
    repo/gettext-tools/its/metainfo.loc
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/gettext-${PROJECT_VERSION}/its
)
