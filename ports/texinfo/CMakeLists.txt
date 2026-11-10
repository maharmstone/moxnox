project(texinfo VERSION 7.3)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
file(MAKE_DIRECTORY ${GEN}/gnu ${GEN}/bin ${GEN}/Texinfo ${GEN}/texinfo ${GEN}/info-hooks)

find_program(HOST_GAWK gawk REQUIRED)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(HAVE_VISIBILITY 0)
else()
    set(HAVE_VISIBILITY 1)
endif()
configure_file(config.h.in ${GEN}/config.h @ONLY)

set(TEXINFO_CFLAGS ${USERSPACE_CFLAGS} -std=gnu23
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-missing-braces
    -Wno-parentheses
    -Wno-missing-field-initializers
    -Wno-float-conversion
    -Wno-unused-but-set-variable
    -Wno-unused-const-variable
    $<$<C_COMPILER_ID:GNU>:-Wno-discarded-qualifiers>
    $<$<C_COMPILER_ID:GNU>:-Wno-unused-value>
    $<$<C_COMPILER_ID:GNU>:-Wno-type-limits>
    $<$<C_COMPILER_ID:Clang>:-Wno-incompatible-pointer-types-discards-qualifiers>
)

set(GNULIB_LIB_DIR ${REPO}/gnulib/lib)
include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

set(INFO_CMD_SOURCES
    ${REPO}/info/session-cmd.c
    ${REPO}/info/echo-area.c
    ${REPO}/info/infodoc.c
    ${REPO}/info/m-x.c
    ${REPO}/info/indices.c
    ${REPO}/info/nodemenu.c
    ${REPO}/info/footnotes.c
    ${REPO}/info/variables.c
)
file(MAKE_DIRECTORY ${GEN}/host ${GEN}/info)
file(CONFIGURE OUTPUT ${GEN}/host/config.h CONTENT "#define PACKAGE_VERSION \"@PROJECT_VERSION@\"\n" @ONLY)
file(CONFIGURE OUTPUT ${GEN}/host/xalloc.h CONTENT "/* host build of makedoc: xmalloc and xstrdup are malloc and strdup */\n" @ONLY)
add_custom_command(
    OUTPUT ${GEN}/host/makedoc${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -DHAVE_CONFIG_H -Dxmalloc=malloc -Dxstrdup=strdup -I${GEN}/host -I${REPO} -I${REPO}/gnulib/lib ${REPO}/info/makedoc.c -o ${GEN}/host/makedoc${HOST_EXECUTABLE_SUFFIX}
    DEPENDS ${REPO}/info/makedoc.c
    VERBATIM
)
add_custom_command(
    OUTPUT ${GEN}/info/doc.c ${GEN}/info/funs.h
    COMMAND ${GEN}/host/makedoc${HOST_EXECUTABLE_SUFFIX} ${INFO_CMD_SOURCES}
    DEPENDS ${GEN}/host/makedoc${HOST_EXECUTABLE_SUFFIX} ${INFO_CMD_SOURCES}
    WORKING_DIRECTORY ${GEN}/info
    VERBATIM
)

add_library(texinfo_gnulib STATIC
    repo/gnulib/lib/argz.c
    repo/gnulib/lib/btowc.c
    repo/gnulib/lib/c32iscntrl.c
    repo/gnulib/lib/c32isprint.c
    repo/gnulib/lib/c32isspace.c
    repo/gnulib/lib/c32tolower.c
    repo/gnulib/lib/c32width.c
    repo/gnulib/lib/cloexec.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/malloc/dynarray_emplace_enlarge.c
    repo/gnulib/lib/malloc/dynarray_finalize.c
    repo/gnulib/lib/malloc/dynarray_resize.c
    repo/gnulib/lib/hard-locale.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/mbchar.c
    repo/gnulib/lib/mbrtowc.c
    repo/gnulib/lib/mbscasecmp.c
    repo/gnulib/lib/mbslen.c
    repo/gnulib/lib/mbsncasecmp.c
    repo/gnulib/lib/mbszero.c
    repo/gnulib/lib/memchr.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/memrchr.c
    repo/gnulib/lib/open.c
    repo/gnulib/lib/regex.c
    repo/gnulib/lib/setlocale_null.c
    repo/gnulib/lib/setlocale-lock.c
    repo/gnulib/lib/setlocale_null-unlocked.c
    repo/gnulib/lib/strerror.c
    repo/gnulib/lib/string.c
    repo/gnulib/lib/strnlen1.c
    repo/gnulib/lib/unicase/tolower.c
    repo/gnulib/lib/unictype/ctype_cntrl.c
    repo/gnulib/lib/unictype/ctype_print.c
    repo/gnulib/lib/unictype/ctype_space.c
    repo/gnulib/lib/uniwidth/width.c
    repo/gnulib/lib/wcwidth.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/strcasestr.c
    repo/gnulib/lib/strstr.c
)
target_compile_definitions(texinfo_gnulib PRIVATE HAVE_CONFIG_H)
target_include_directories(texinfo_gnulib PUBLIC ${GEN} ${GEN}/gnu ${REPO}/gnulib/lib)
target_compile_options(texinfo_gnulib PRIVATE ${TEXINFO_CFLAGS})
target_link_libraries(texinfo_gnulib PUBLIC libiconv moxnox)

add_executable(texinfo_info
    ${GEN}/info/doc.c
    repo/info/man.c
    repo/info/configfiles.c
    repo/info/dir.c
    repo/info/display.c
    repo/info/dribble.c
    repo/info/echo-area.c
    repo/info/filesys.c
    repo/info/footnotes.c
    repo/info/indices.c
    repo/info/info.c
    repo/info/infodoc.c
    repo/info/infokey.c
    repo/info/infomap.c
    repo/info/infopath.c
    repo/info/m-x.c
    repo/info/nodemenu.c
    repo/info/nodes.c
    repo/info/run-external.c
    repo/info/scan.c
    repo/info/search.c
    repo/info/session.c
    repo/info/session-cmd.c
    repo/info/signals.c
    repo/info/tag.c
    repo/info/terminal.c
    repo/info/tilde.c
    repo/info/util.c
    repo/info/variables.c
    repo/info/window.c
)
set_target_properties(texinfo_info PROPERTIES OUTPUT_NAME info)

target_compile_definitions(texinfo_info PRIVATE
    HAVE_CONFIG_H
    LOCALEDIR="/share/locale"
    INFODIR="/share/info"
    INFODIR2="/share/info"
    SYSCONFDIR="/etc"
    PKGDATADIR="/share/texinfo"
)

target_include_directories(texinfo_info PRIVATE ${GEN}/info ${REPO} ${REPO}/info)
target_compile_options(texinfo_info PRIVATE ${TEXINFO_CFLAGS})
target_link_libraries(texinfo_info PRIVATE texinfo_gnulib tinfo)

add_executable(texinfo_install_info repo/install-info/install-info.c)
set_target_properties(texinfo_install_info PROPERTIES OUTPUT_NAME install-info)

target_compile_definitions(texinfo_install_info PRIVATE
    HAVE_CONFIG_H
    LOCALEDIR="/share/locale"
)

target_include_directories(texinfo_install_info PRIVATE ${REPO})
target_compile_options(texinfo_install_info PRIVATE ${TEXINFO_CFLAGS})
target_link_libraries(texinfo_install_info PRIVATE texinfo_gnulib)

set(TEXI2ANY_SUBST
    -e "s,[@]PACKAGE_VERSION[@],${PROJECT_VERSION},g"
    -e "s,[@]PACKAGE_NAME[@],GNU Texinfo,g"
    -e "s,[@]PACKAGE_URL[@],https://www.gnu.org/software/texinfo/,g"
    -e "s,[@]sysconfdir[@],/etc,g"
    -e "s,[@]datadir[@],/share,g"
    -e "s,[@]libdir[@],/lib,g"
    -e "s,[@]PACKAGE[@],texinfo,g"
    -e "s,[@]CONVERTER[@],texi2any,g"
    -e "s,[@]USE_EXTERNAL_LIBINTL[@],no,g"
    -e "s,[@]USE_EXTERNAL_EASTASIANWIDTH[@],no,g"
    -e "s,[@]USE_EXTERNAL_UNIDECODE[@],no,g"
    -e "s,[@]TEXINFO_DTD_VERSION[@],${PROJECT_VERSION},g"
    -e "s,[@]enable_xs[@],no,g"
    -e "1 s,/usr/bin/env perl,/bin/perl,g"
)

foreach(_s texi2any load_txi_modules)
    add_custom_command(
        OUTPUT ${GEN}/bin/${_s}
        COMMAND ${HOST_SED} ${TEXI2ANY_SUBST} ${REPO}/tta/perl/${_s}.pl > ${GEN}/bin/${_s}
        COMMAND chmod +x ${GEN}/bin/${_s}
        DEPENDS ${REPO}/tta/perl/${_s}.pl
        VERBATIM
    )
endforeach()

add_custom_command(
    OUTPUT ${GEN}/bin/pod2texi
    COMMAND ${HOST_SED} ${TEXI2ANY_SUBST} ${REPO}/Pod-Simple-Texinfo/pod2texi.pl > ${GEN}/bin/pod2texi
    COMMAND chmod +x ${GEN}/bin/pod2texi
    DEPENDS ${REPO}/Pod-Simple-Texinfo/pod2texi.pl
    VERBATIM
)

set(ICONV_CONVERTS_EUC_CN yes)
set(DEFAULT_TEST_LEVEL 1)
set(enable_xs no)
set(use_shared_txi_libraries no)
set(USE_EXTERNAL_LIBINTL no)
set(USE_EXTERNAL_EASTASIANWIDTH no)
set(USE_EXTERNAL_UNIDECODE no)
set(CONVERTER texi2any)
set(configure_input "Generated from ModulePath.pm.in by the Moxnox port.")
configure_file(${REPO}/tta/perl/Texinfo/ModulePath.pm.in ${GEN}/Texinfo/ModulePath.pm @ONLY)

set(TEXINDEX_SUBST
    -e "s,[@]pkgdatadir[@],/share/texinfo,g"
    -e "s,[@]TI_AWK[@],awk,g"
    -e "s,[@]PACKAGE[@],texinfo,g"
    -e "s,[@][@]*VERSION[@][@]*,${PROJECT_VERSION},g"
)
add_custom_command(
    OUTPUT ${GEN}/bin/texindex
    COMMAND ${HOST_SED} ${TEXINDEX_SUBST} ${REPO}/texindex/texindex.in > ${GEN}/bin/texindex
    COMMAND chmod +x ${GEN}/bin/texindex
    DEPENDS ${REPO}/texindex/texindex.in
    VERBATIM
)
add_custom_command(
    OUTPUT ${GEN}/texinfo/texindex.awk
    COMMAND ${HOST_GAWK} -f ${REPO}/texindex/jrtangle ${REPO}/texindex/ti.twjr
    COMMAND ${HOST_SED} -i ${TEXINDEX_SUBST} ${GEN}/texinfo/texindex.awk
    DEPENDS ${REPO}/texindex/jrtangle ${REPO}/texindex/ti.twjr
    WORKING_DIRECTORY ${GEN}/texinfo
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/info-hooks/manual-not-found
    COMMAND ${HOST_SED} -e "s,[@]AWK[@],awk,g" ${REPO}/info/info-hooks-default/manual-not-found.in > ${GEN}/info-hooks/manual-not-found
    COMMAND chmod +x ${GEN}/info-hooks/manual-not-found
    DEPENDS ${REPO}/info/info-hooks-default/manual-not-found.in
    VERBATIM
)

add_custom_target(texinfo ALL DEPENDS
    ${GEN}/bin/texi2any
    ${GEN}/bin/load_txi_modules
    ${GEN}/bin/pod2texi
    ${GEN}/bin/texindex
    ${GEN}/texinfo/texindex.awk
    ${GEN}/info-hooks/manual-not-found
)
add_dependencies(texinfo texinfo_info texinfo_install_info)

install(TARGETS texinfo_info texinfo_install_info DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(PROGRAMS ${GEN}/bin/texi2any ${GEN}/bin/pod2texi ${GEN}/bin/texindex DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(PROGRAMS repo/util/texi2pdf DESTINATION ${CMAKE_INSTALL_PREFIX}/bin RENAME pdftexi2dvi)
install(CODE "file(CREATE_LINK texi2any \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/makeinfo\" SYMBOLIC)")
install(PROGRAMS ${GEN}/bin/load_txi_modules DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any)
install(FILES ${GEN}/Texinfo/ModulePath.pm DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/Texinfo)
install(PROGRAMS ${GEN}/texinfo/texindex.awk DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texinfo)
install(PROGRAMS ${GEN}/info-hooks/manual-not-found DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texinfo/info-hooks)

install(PROGRAMS
    repo/util/texi2pdf
    repo/util/texi2dvi
    repo/util/texi2pdf
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
)
install(FILES
    repo/Pod-Simple-Texinfo/lib/Pod/Simple/Texinfo.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/Pod-Simple-Texinfo/Pod/Simple
)
install(FILES
    repo/tta/perl/Texinfo/Commands.pm
    repo/tta/perl/Texinfo/Commands.pod
    repo/tta/perl/Texinfo/CommandsValues.pm
    repo/tta/perl/Texinfo/CommandsValues.pod
    repo/tta/perl/Texinfo/Common.pm
    repo/tta/perl/Texinfo/Config.pm
    repo/tta/perl/Texinfo/ConfigXS.pm
    repo/tta/perl/Texinfo/DebugTree.pm
    repo/tta/perl/Texinfo/Document.pm
    repo/tta/perl/Texinfo/DocumentXS.pm
    repo/tta/perl/Texinfo/Documentlanguages.pm
    repo/tta/perl/Texinfo/HTMLData.pm
    repo/tta/perl/Texinfo/HTMLDataCSS.pm
    repo/tta/perl/Texinfo/Indices.pm
    repo/tta/perl/Texinfo/IndicesXS.pm
    repo/tta/perl/Texinfo/ManipulateTree.pm
    repo/tta/perl/Texinfo/MiscXS.pm
    repo/tta/perl/Texinfo/Options.pm
    repo/tta/perl/Texinfo/OutputUnits.pm
    repo/tta/perl/Texinfo/Parser.pm
    repo/tta/perl/Texinfo/ParserNonXS.pm
    repo/tta/perl/Texinfo/Reader.pm
    repo/tta/perl/Texinfo/ReaderNonXS.pm
    repo/tta/perl/Texinfo/Report.pm
    repo/tta/perl/Texinfo/StructTransfXS.pm
    repo/tta/perl/Texinfo/Structuring.pm
    repo/tta/perl/Texinfo/Tests.pm
    repo/tta/perl/Texinfo/Transformations.pm
    repo/tta/perl/Texinfo/Translations.pm
    repo/tta/perl/Texinfo/TreeElement.pm
    repo/tta/perl/Texinfo/TreeElementNonXS.pm
    repo/tta/perl/Texinfo/UnicodeData.pm
    repo/tta/perl/Texinfo/XSLoader.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/Texinfo
)
install(FILES
    repo/tta/perl/Texinfo/Convert/ConvertXS.pm
    repo/tta/perl/Texinfo/Convert/Converter.pm
    repo/tta/perl/Texinfo/Convert/DocBook.pm
    repo/tta/perl/Texinfo/Convert/HTML.pm
    repo/tta/perl/Texinfo/Example/IXIN.pm
    repo/tta/perl/Texinfo/Example/IXINSXML.pm
    repo/tta/perl/Texinfo/Convert/Info.pm
    repo/tta/perl/Texinfo/Convert/LaTeX.pm
    repo/tta/perl/Texinfo/Convert/NodeNameNormalization.pm
    repo/tta/perl/Texinfo/Convert/Paragraph.pm
    repo/tta/perl/Texinfo/Convert/ParagraphNonXS.pm
    repo/tta/perl/Texinfo/Convert/PlainTexinfo.pm
    repo/tta/perl/Texinfo/Convert/Plaintext.pm
    repo/tta/perl/Texinfo/Example/ReadDocBook.pm
    repo/tta/perl/Texinfo/Example/TestReader.pm
    repo/tta/perl/Texinfo/Convert/Texinfo.pm
    repo/tta/perl/Texinfo/Convert/TexinfoMarkup.pm
    repo/tta/perl/Texinfo/Convert/TexinfoNonXS.pm
    repo/tta/perl/Texinfo/Convert/TexinfoSXML.pm
    repo/tta/perl/Texinfo/Convert/TexinfoXML.pm
    repo/tta/perl/Texinfo/Convert/TexinfoXS.pm
    repo/tta/perl/Texinfo/Convert/Text.pm
    repo/tta/perl/Texinfo/Convert/TextContent.pm
    repo/tta/perl/Texinfo/Example/TreeElementConverter.pm
    repo/tta/perl/Texinfo/Example/TreeElementConverterXS.pm
    repo/tta/perl/Texinfo/Example/TreeElementReadDocBook.pm
    repo/tta/perl/Texinfo/Convert/Unicode.pm
    repo/tta/perl/Texinfo/Convert/Utils.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/Texinfo/Convert
)
install(FILES
    repo/tta/perl/ext/epub3.pm
    repo/tta/perl/ext/highlight_syntax.pm
    repo/tta/perl/ext/latex2html.pm
    repo/tta/perl/ext/tex4ht.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/ext
)
install(FILES
    repo/tta/perl/init/book.pm
    repo/tta/perl/init/chm.pm
    repo/tta/perl/init/documentation_examples.pm
    repo/tta/perl/init/html32.pm
    repo/tta/perl/init/sc_formatting_with_css.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/init
)
install(FILES
    repo/js/info.css
    repo/js/info.js
    repo/js/modernizr.js
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/js
)
install(FILES
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/lib/Text-Unidecode/lib/Text
)
install(FILES
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x00.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x01.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x02.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x03.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x04.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x05.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x06.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x07.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x08.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x09.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x0a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x0b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x0c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x0d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x0e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x0f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x10.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x11.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x12.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x13.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x14.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x15.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x16.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x17.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x18.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x19.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x1a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x1b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x1c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x1d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x1e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x1f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x20.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x21.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x22.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x23.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x24.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x25.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x26.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x27.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x28.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x29.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x2a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x2b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x2c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x2d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x2e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x2f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x30.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x31.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x32.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x33.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x34.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x35.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x36.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x37.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x38.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x39.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x3a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x3b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x3c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x3d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x3e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x3f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x40.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x41.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x42.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x43.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x44.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x45.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x46.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x47.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x48.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x49.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x4a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x4b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x4c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x4d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x4e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x4f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x50.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x51.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x52.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x53.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x54.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x55.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x56.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x57.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x58.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x59.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x5a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x5b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x5c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x5d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x5e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x5f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x60.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x61.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x62.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x63.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x64.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x65.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x66.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x67.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x68.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x69.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x6a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x6b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x6c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x6d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x6e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x6f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x70.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x71.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x72.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x73.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x74.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x75.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x76.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x77.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x78.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x79.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x7a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x7b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x7c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x7d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x7e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x7f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x80.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x81.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x82.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x83.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x84.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x85.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x86.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x87.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x88.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x89.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x8a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x8b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x8c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x8d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x8e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x8f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x90.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x91.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x92.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x93.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x94.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x95.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x96.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x97.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x98.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x99.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x9a.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x9b.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x9c.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x9d.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x9e.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/x9f.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa0.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa1.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa2.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa3.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa4.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa5.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa6.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa7.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa8.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xa9.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xaa.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xab.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xac.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xad.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xae.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xaf.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb0.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb1.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb2.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb3.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb4.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb5.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb6.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb7.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb8.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xb9.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xba.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xbb.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xbc.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xbd.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xbe.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xbf.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc0.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc1.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc2.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc3.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc4.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc5.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc6.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc7.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc8.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xc9.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xca.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xcb.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xcc.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xcd.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xce.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xcf.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd0.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd1.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd2.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd3.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd4.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd5.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd6.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd7.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd8.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xd9.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xda.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xdb.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xdc.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xdd.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xde.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xdf.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe0.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe1.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe2.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe3.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe4.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe5.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe6.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe7.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe8.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xe9.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xea.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xeb.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xec.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xed.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xee.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xef.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf0.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf1.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf2.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf3.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf4.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf5.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf6.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf7.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf8.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xf9.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xfa.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xfb.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xfc.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xfd.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xfe.pm
    repo/tta/maintain/lib/Text-Unidecode/lib/Text/Unidecode/xff.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/lib/Text-Unidecode/lib/Text/Unidecode
)
install(FILES
    repo/tta/maintain/lib/Unicode-EastAsianWidth/lib/Unicode/EastAsianWidth.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/lib/Unicode-EastAsianWidth/lib/Unicode
)
install(FILES
    repo/tta/maintain/lib/libintl-perl/lib/Locale/Messages.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/Recode.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/TextDomain.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/Util.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/gettext_dumb.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/gettext_pp.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/lib/libintl-perl/lib/Locale
)
install(FILES
    repo/tta/maintain/lib/libintl-perl/lib/Locale/Recode/_Aliases.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/Recode/_Conversions.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/lib/libintl-perl/lib/Locale/Recode
)
install(FILES
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ASMO_449.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ATARI_ST.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ATARI_ST_EURO.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CP10007.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CP1250.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CP1251.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CP1252.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CP1253.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CP1254.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CP1256.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CP1257.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CSN_369103.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/CWI.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/DEC_MCS.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_AT_DE.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_AT_DE_A.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_CA_FR.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_DK_NO.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_DK_NO_A.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_ES.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_ES_A.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_ES_S.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_FI_SE.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_FI_SE_A.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_FR.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_IS_FRISS.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_IT.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_PT.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_UK.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/EBCDIC_US.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ECMA_CYRILLIC.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/GEORGIAN_ACADEMY.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/GEORGIAN_PS.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/GOST_19768_74.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/GREEK7.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/GREEK7_OLD.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/GREEK_CCITT.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/HP_ROMAN8.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM037.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM038.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM1004.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM1026.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM1047.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM256.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM273.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM274.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM275.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM277.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM278.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM280.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM281.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM284.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM285.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM290.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM297.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM420.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM423.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM424.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM437.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM500.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM850.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM851.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM852.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM855.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM857.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM860.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM861.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM862.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM863.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM864.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM865.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM866.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM868.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM869.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM870.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM871.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM874.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM875.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM880.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM891.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM903.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM904.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM905.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IBM918.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/IEC_P27_1.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/INIS.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/INIS_8.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/INIS_CYRILLIC.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_10367_BOX.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_2033_1983.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_5427.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_5427_EXT.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_5428.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_1.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_10.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_11.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_13.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_14.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_15.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_16.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_2.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_3.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_4.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_5.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_6.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_7.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_8.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/ISO_8859_9.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/KOI8_R.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/KOI8_RU.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/KOI8_T.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/KOI8_U.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/KOI_8.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/LATIN_GREEK.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/LATIN_GREEK_1.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACARABIC.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACCROATIAN.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACCYRILLIC.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACGREEK.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACHEBREW.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACICELAND.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACINTOSH.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACROMANIA.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACTHAI.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACTURKISH.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MACUKRAINE.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MAC_IS.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MAC_SAMI.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/MAC_UK.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/NATS_DANO.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/NATS_SEFI.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/NEXTSTEP.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/SAMI_WS2.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/TIS_620.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/US_ASCII.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/UTF_8.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/VISCII.pm
    repo/tta/maintain/lib/libintl-perl/lib/Locale/RecodeData/_Encode.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texi2any/lib/libintl-perl/lib/Locale/RecodeData
)
install(FILES
    repo/util/texinfo.dtd
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texinfo
)
install(FILES
    repo/util/htmlxref.d/Texinfo_GNU.cnf
    repo/util/htmlxref.d/Texinfo_nonGNU.cnf
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texinfo/htmlxref.d
)
install(FILES
    repo/info/info-hooks-default/gnu-manuals-locations.dat
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/texinfo/info-hooks
)
