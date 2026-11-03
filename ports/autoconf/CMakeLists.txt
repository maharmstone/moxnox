project(autoconf VERSION 2.73)

find_program(HOST_PERL perl REQUIRED)
find_program(HOST_MAKEINFO makeinfo REQUIRED)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/bin)
file(MAKE_DIRECTORY ${GEN}/lib/autoconf)
file(MAKE_DIRECTORY ${GEN}/lib/autoscan)
file(MAKE_DIRECTORY ${GEN}/lib/autotest)
file(MAKE_DIRECTORY ${GEN}/lib/m4sugar)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
set(PKGDATADIR /share/autoconf)
set(RELEASE_YEAR 2026)

set(AUTOCONF_EDIT
    -e "s|@SHELL[@]|/bin/sh|g"
    -e "s|@PERL[@]|/bin/perl|g"
    -e "s|@PERL_FLOCK[@]|no|g"
    -e "s|@bindir[@]|/bin|g"
    -e "s|@pkgdatadir[@]|${PKGDATADIR}|g"
    -e "s|@prefix[@]||g"
    -e "s|@autoconf-name[@]|autoconf|g"
    -e "s|@autoheader-name[@]|autoheader|g"
    -e "s|@autom4te-name[@]|autom4te|g"
    -e "s|@M4[@]|m4|g"
    -e "s|@M4_DEBUGFILE[@]|--debugfile|g"
    -e "s|@M4_GNU[@]|--gnu|g"
    -e "s|@AWK[@]|gawk|g"
    -e "s|@RELEASE_YEAR[@]|${RELEASE_YEAR}|g"
    -e "s|@VERSION[@]|${PROJECT_VERSION}|g"
    -e "s|@PACKAGE_NAME[@]|GNU Autoconf|g"
)

set(AUTOCONF_SCRIPTS autoconf autoheader autom4te autoreconf autoscan autoupdate ifnames)

foreach(script ${AUTOCONF_SCRIPTS})
    add_custom_command(
        OUTPUT ${GEN}/bin/${script}
        COMMAND ${HOST_SED} ${AUTOCONF_EDIT}
                -e "s|@configure_input[@]|Generated from bin/${script}.in; do not edit by hand.|g"
                ${REPO}/bin/${script}.in > ${GEN}/bin/${script}
        COMMAND chmod +x ${GEN}/bin/${script}
        DEPENDS repo/bin/${script}.in
        VERBATIM
    )
endforeach()

add_custom_command(
    OUTPUT ${GEN}/lib/autom4te.cfg
    COMMAND ${HOST_SED} ${AUTOCONF_EDIT}
            -e "s|@configure_input[@]|Generated from lib/autom4te.in; do not edit by hand.|g"
            ${REPO}/lib/autom4te.in > ${GEN}/lib/autom4te.cfg
    DEPENDS repo/lib/autom4te.in
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/autoconf_version.m4
    COMMAND ${HOST_SED}
            -e "s|@PACKAGE_NAME[@]|GNU Autoconf|g"
            -e "s|@PACKAGE_TARNAME[@]|autoconf|g"
            -e "s|@PACKAGE_VERSION[@]|${PROJECT_VERSION}|g"
            -e "s|@PACKAGE_STRING[@]|GNU Autoconf ${PROJECT_VERSION}|g"
            -e "s|@PACKAGE_BUGREPORT[@]|bug-autoconf@gnu.org|g"
            -e "s|@PACKAGE_URL[@]|https://www.gnu.org/software/autoconf/|g"
            -e "s|@RELEASE_YEAR[@]|${RELEASE_YEAR}|g"
            ${REPO}/lib/autoconf_version.in > ${GEN}/lib/autoconf_version.m4
    DEPENDS repo/lib/autoconf_version.in
    VERBATIM
)

set(FREEZE_AUTOM4TE
    ${CMAKE_COMMAND} -E env
        autom4te_perllibdir=${REPO}/lib
        AUTOM4TE_CFG=${GEN}/lib/autom4te.cfg
        M4=${HOST_M4}
    ${HOST_PERL} ${GEN}/bin/autom4te -B ${GEN}/lib -B ${REPO}/lib
)

set(M4SUGAR_M4F_DEPENDENCIES
    ${GEN}/bin/autom4te
    ${GEN}/lib/autom4te.cfg
    ${GEN}/lib/autoconf_version.m4
    repo/lib/m4sugar/m4sugar.m4
)

set(M4SH_M4F_DEPENDENCIES
    ${M4SUGAR_M4F_DEPENDENCIES}
    ${GEN}/lib/m4sugar/m4sugar.m4f
    repo/lib/m4sugar/m4sh.m4
)

add_custom_command(
    OUTPUT ${GEN}/lib/m4sugar/m4sugar.m4f
    COMMAND ${FREEZE_AUTOM4TE} --language=m4sugar --freeze --output=${GEN}/lib/m4sugar/m4sugar.m4f
    DEPENDS ${M4SUGAR_M4F_DEPENDENCIES}
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/m4sugar/m4sh.m4f
    COMMAND ${FREEZE_AUTOM4TE} --language=m4sh --freeze --output=${GEN}/lib/m4sugar/m4sh.m4f
    DEPENDS ${M4SH_M4F_DEPENDENCIES}
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/autotest/autotest.m4f
    COMMAND ${FREEZE_AUTOM4TE} --language=autotest --freeze --output=${GEN}/lib/autotest/autotest.m4f
    DEPENDS ${M4SH_M4F_DEPENDENCIES}
            ${GEN}/lib/m4sugar/m4sh.m4f
            repo/lib/autotest/autotest.m4
            repo/lib/autotest/general.m4
            repo/lib/autotest/specific.m4
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

set(AUTOCONF_M4
    repo/lib/autoconf/autoconf.m4
    repo/lib/autoconf/general.m4
    repo/lib/autoconf/status.m4
    repo/lib/autoconf/oldnames.m4
    repo/lib/autoconf/specific.m4
    repo/lib/autoconf/autoheader.m4
    repo/lib/autoconf/autoupdate.m4
    repo/lib/autoconf/autotest.m4
    repo/lib/autoconf/autoscan.m4
    repo/lib/autoconf/lang.m4
    repo/lib/autoconf/c.m4
    repo/lib/autoconf/erlang.m4
    repo/lib/autoconf/fortran.m4
    repo/lib/autoconf/functions.m4
    repo/lib/autoconf/go.m4
    repo/lib/autoconf/a68.m4
    repo/lib/autoconf/headers.m4
    repo/lib/autoconf/types.m4
    repo/lib/autoconf/libs.m4
    repo/lib/autoconf/programs.m4
    repo/lib/autoconf/trailer.m4
)

add_custom_command(
    OUTPUT ${GEN}/lib/autoconf/autoconf.m4f
    COMMAND ${FREEZE_AUTOM4TE} --language=autoconf-without-aclocal-m4 --freeze --output=${GEN}/lib/autoconf/autoconf.m4f
    DEPENDS ${M4SH_M4F_DEPENDENCIES}
            ${GEN}/lib/m4sugar/m4sh.m4f
            ${AUTOCONF_M4}
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/autoscan/autoscan.list
    COMMAND sh -c "echo '# Automatically Generated: do not edit this file' > '${GEN}/lib/autoscan/autoscan.list' && sed '/^[#]/!q' '${REPO}/lib/autoscan/autoscan.pre' >> '${GEN}/lib/autoscan/autoscan.list' && ( sed -n '/^[^#]/p' '${REPO}/lib/autoscan/autoscan.pre'; autom4te_perllibdir='${REPO}/lib' AUTOM4TE_CFG='${GEN}/lib/autom4te.cfg' M4='${HOST_M4}' '${HOST_PERL}' '${GEN}/bin/autom4te' -B '${GEN}/lib' -B '${REPO}/lib' --cache '' -M -l autoconf-without-aclocal-m4 -t\"$(printf 'AN_OUTPUT:$1: $2\\t$3')\" ) | LC_ALL=C sort >> '${GEN}/lib/autoscan/autoscan.list'"
    DEPENDS repo/lib/autoscan/autoscan.pre ${GEN}/lib/autoconf/autoconf.m4f
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/INSTALL
    COMMAND sh -c "( echo @firstparagraphindent insert; cat '${REPO}/doc/install.texi' ) > '${GEN}/install.texi' && '${HOST_MAKEINFO}' --no-headers --no-validate --no-split --plaintext '${GEN}/install.texi' | sed -e '/^$/H; /^$/d; x; s/\\n//p; s/\\n*//; x' > '${GEN}/INSTALL'"
    DEPENDS repo/doc/install.texi
    VERBATIM
)

set(AUTOCONF_SCRIPT_OUTPUTS)
foreach(script ${AUTOCONF_SCRIPTS})
    list(APPEND AUTOCONF_SCRIPT_OUTPUTS ${GEN}/bin/${script})
endforeach()

add_custom_target(autoconf ALL DEPENDS
    ${AUTOCONF_SCRIPT_OUTPUTS}
    ${GEN}/lib/autom4te.cfg
    ${GEN}/lib/autoconf_version.m4
    ${GEN}/lib/m4sugar/m4sugar.m4f
    ${GEN}/lib/m4sugar/m4sh.m4f
    ${GEN}/lib/autotest/autotest.m4f
    ${GEN}/lib/autoconf/autoconf.m4f
    ${GEN}/lib/autoscan/autoscan.list
    ${GEN}/INSTALL
)

install(PROGRAMS ${AUTOCONF_SCRIPT_OUTPUTS} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES
    ${GEN}/INSTALL
    ${GEN}/lib/autom4te.cfg
    ${GEN}/lib/autoconf_version.m4
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGDATADIR}
)

install(FILES
    repo/lib/Autom4te/C4che.pm
    repo/lib/Autom4te/ChannelDefs.pm
    repo/lib/Autom4te/Channels.pm
    repo/lib/Autom4te/Config.pm
    repo/lib/Autom4te/Configure_ac.pm
    repo/lib/Autom4te/FileUtils.pm
    repo/lib/Autom4te/General.pm
    repo/lib/Autom4te/Getopt.pm
    repo/lib/Autom4te/Request.pm
    repo/lib/Autom4te/XFile.pm
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGDATADIR}/Autom4te
)

install(FILES
    ${AUTOCONF_M4}
    ${GEN}/lib/autoconf/autoconf.m4f
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGDATADIR}/autoconf
)

install(FILES ${GEN}/lib/autoscan/autoscan.list
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGDATADIR}/autoscan
)

install(FILES
    repo/lib/m4sugar/m4sugar.m4
    repo/lib/m4sugar/foreach.m4
    repo/lib/m4sugar/m4sh.m4
    ${GEN}/lib/m4sugar/m4sugar.m4f
    ${GEN}/lib/m4sugar/m4sh.m4f
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGDATADIR}/m4sugar
)

install(FILES
    repo/lib/autotest/autotest.m4
    repo/lib/autotest/general.m4
    repo/lib/autotest/specific.m4
    ${GEN}/lib/autotest/autotest.m4f
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGDATADIR}/autotest
)

install(PROGRAMS
    repo/build-aux/config.guess
    repo/build-aux/config.sub
    repo/build-aux/install-sh
    DESTINATION ${CMAKE_INSTALL_PREFIX}${PKGDATADIR}/build-aux
)
