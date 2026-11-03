project(automake VERSION 1.18.1)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/bin)
file(MAKE_DIRECTORY ${GEN}/lib/Automake)
file(MAKE_DIRECTORY ${GEN}/m4)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
set(APIVERSION 1.18)
set(RELEASE_YEAR 2025)
set(DATADIR /share)
set(PKGVDATADIR ${DATADIR}/automake-${APIVERSION})
set(AUTOMAKE_ACDIR ${DATADIR}/aclocal-${APIVERSION})
set(SYSTEM_ACDIR ${DATADIR}/aclocal)

set(AUTOMAKE_EDIT
    -e "s|@PERL[@]|/bin/perl|g"
    -e "s|@SHELL[@]|/bin/sh|g"
    -e "s|@PACKAGE[@]|automake|g"
    -e "s|@VERSION[@]|${PROJECT_VERSION}|g"
    -e "s|@APIVERSION[@]|${APIVERSION}|g"
    -e "s|@PACKAGE_BUGREPORT[@]|bug-automake@gnu.org|g"
    -e "s|@PACKAGE_URL[@]|https://www.gnu.org/software/automake/|g"
    -e "s|@RELEASE_YEAR[@]|${RELEASE_YEAR}|g"
    -e "s|@datadir[@]|${DATADIR}|g"
    -e "s|@bindir[@]|/bin|g"
    -e "s|@docdir[@]|${DATADIR}/doc/automake|g"
    -e "s|@pkgvdatadir[@]|${PKGVDATADIR}|g"
    -e "s|@scriptdir[@]|${PKGVDATADIR}|g"
    -e "s|@amdir[@]|${PKGVDATADIR}/am|g"
    -e "s|@automake_acdir[@]|${AUTOMAKE_ACDIR}|g"
    -e "s|@system_acdir[@]|${SYSTEM_ACDIR}|g"
    -e "s|@am_AUTOCONF[@]|autoconf|g"
    -e "s|@am_AUTOM4TE[@]|autom4te|g"
    -e "s|@am_AUTORECONF[@]|autoreconf|g"
    -e "s|@am_AUTOHEADER[@]|autoheader|g"
    -e "s|@am_AUTOUPDATE[@]|autoupdate|g"
)

foreach(script automake aclocal)
    add_custom_command(
        OUTPUT ${GEN}/bin/${script}
        COMMAND ${HOST_SED} ${AUTOMAKE_EDIT}
                -e "s|@configure_input[@]|Generated from bin/${script}.in; do not edit by hand.|g"
                ${REPO}/bin/${script}.in > ${GEN}/bin/${script}
        COMMAND chmod +x ${GEN}/bin/${script}
        DEPENDS repo/bin/${script}.in
        VERBATIM
    )
endforeach()

add_custom_command(
    OUTPUT ${GEN}/lib/Automake/Config.pm
    COMMAND ${HOST_SED} ${AUTOMAKE_EDIT}
            -e "s|@configure_input[@]|Generated from lib/Automake/Config.in; do not edit by hand.|g"
            ${REPO}/lib/Automake/Config.in > ${GEN}/lib/Automake/Config.pm
    DEPENDS repo/lib/Automake/Config.in
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/m4/amversion.m4
    COMMAND ${HOST_SED} ${AUTOMAKE_EDIT}
            -e "s|@configure_input[@]|Generated from m4/amversion.in; do not edit by hand.|g"
            ${REPO}/m4/amversion.in > ${GEN}/m4/amversion.m4
    DEPENDS repo/m4/amversion.in
    VERBATIM
)

add_custom_target(automake ALL DEPENDS
    ${GEN}/bin/automake
    ${GEN}/bin/aclocal
    ${GEN}/lib/Automake/Config.pm
    ${GEN}/m4/amversion.m4
)

install(PROGRAMS ${GEN}/bin/automake ${GEN}/bin/aclocal DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(CODE "file(CREATE_LINK automake \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/automake-${APIVERSION}\" SYMBOLIC)")
install(CODE "file(CREATE_LINK aclocal \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/aclocal-${APIVERSION}\" SYMBOLIC)")

install(FILES
    repo/lib/COPYING
    repo/lib/INSTALL
    repo/lib/texinfo.tex
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGVDATADIR}
)

install(PROGRAMS
    repo/lib/config.guess
    repo/lib/config.sub
    repo/lib/install-sh
    repo/lib/mdate-sh
    repo/lib/missing
    repo/lib/mkinstalldirs
    repo/lib/ylwrap
    repo/lib/depcomp
    repo/lib/compile
    repo/lib/py-compile
    repo/lib/ar-lib
    repo/lib/test-driver
    repo/lib/tap-driver.sh
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGVDATADIR}
)

install(FILES
    repo/lib/Automake/ChannelDefs.pm
    repo/lib/Automake/Channels.pm
    repo/lib/Automake/Condition.pm
    repo/lib/Automake/Configure_ac.pm
    repo/lib/Automake/DisjConditions.pm
    repo/lib/Automake/FileUtils.pm
    repo/lib/Automake/General.pm
    repo/lib/Automake/Getopt.pm
    repo/lib/Automake/Item.pm
    repo/lib/Automake/ItemDef.pm
    repo/lib/Automake/Language.pm
    repo/lib/Automake/Location.pm
    repo/lib/Automake/Options.pm
    repo/lib/Automake/Rule.pm
    repo/lib/Automake/RuleDef.pm
    repo/lib/Automake/Variable.pm
    repo/lib/Automake/VarDef.pm
    repo/lib/Automake/Version.pm
    repo/lib/Automake/XFile.pm
    repo/lib/Automake/Wrap.pm
    ${GEN}/lib/Automake/Config.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGVDATADIR}/Automake
)

install(FILES
    repo/lib/am/check.am
    repo/lib/am/check2.am
    repo/lib/am/clean-hdr.am
    repo/lib/am/clean.am
    repo/lib/am/compile.am
    repo/lib/am/configure.am
    repo/lib/am/data.am
    repo/lib/am/dejagnu.am
    repo/lib/am/depend.am
    repo/lib/am/depend2.am
    repo/lib/am/distdir.am
    repo/lib/am/footer.am
    repo/lib/am/header-vars.am
    repo/lib/am/header.am
    repo/lib/am/install.am
    repo/lib/am/inst-vars.am
    repo/lib/am/java.am
    repo/lib/am/lang-compile.am
    repo/lib/am/lex.am
    repo/lib/am/library.am
    repo/lib/am/libs.am
    repo/lib/am/libtool.am
    repo/lib/am/lisp.am
    repo/lib/am/ltlib.am
    repo/lib/am/ltlibrary.am
    repo/lib/am/mans-vars.am
    repo/lib/am/mans.am
    repo/lib/am/program.am
    repo/lib/am/progs.am
    repo/lib/am/python.am
    repo/lib/am/remake-hdr.am
    repo/lib/am/scripts.am
    repo/lib/am/subdirs.am
    repo/lib/am/tags.am
    repo/lib/am/texi-vers.am
    repo/lib/am/texibuild.am
    repo/lib/am/texinfos.am
    repo/lib/am/vala.am
    repo/lib/am/yacc.am
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGVDATADIR}/am
)

install(FILES
    ${GEN}/m4/amversion.m4
    repo/m4/ar-lib.m4
    repo/m4/as.m4
    repo/m4/auxdir.m4
    repo/m4/cond.m4
    repo/m4/cond-if.m4
    repo/m4/depend.m4
    repo/m4/depout.m4
    repo/m4/dmalloc.m4
    repo/m4/extra-recurs.m4
    repo/m4/gcj.m4
    repo/m4/init.m4
    repo/m4/install-sh.m4
    repo/m4/lead-dot.m4
    repo/m4/lex.m4
    repo/m4/lispdir.m4
    repo/m4/maintainer.m4
    repo/m4/make.m4
    repo/m4/missing.m4
    repo/m4/mkdirp.m4
    repo/m4/obsolete.m4
    repo/m4/options.m4
    repo/m4/python.m4
    repo/m4/prog-cc-c-o.m4
    repo/m4/rmf.m4
    repo/m4/runlog.m4
    repo/m4/sanity.m4
    repo/m4/silent.m4
    repo/m4/strip.m4
    repo/m4/substnot.m4
    repo/m4/tar.m4
    repo/m4/upc.m4
    repo/m4/vala.m4
    repo/m4/xargsn.m4
    DESTINATION ${CMAKE_INSTALL_PREFIX}${AUTOMAKE_ACDIR}
)

install(FILES repo/m4/internal/ac-config-macro-dirs.m4
    DESTINATION ${CMAKE_INSTALL_PREFIX}${AUTOMAKE_ACDIR}/internal
)

install(FILES repo/m4/acdir/README DESTINATION ${CMAKE_INSTALL_PREFIX}${SYSTEM_ACDIR})
