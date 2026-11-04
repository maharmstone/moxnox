project(libtool VERSION 2.5.4)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
set(BOOTSTRAP_AUX ${REPO}/gl-mod/bootstrap/build-aux)
set(GNULIB_AUX ${REPO}/gnulib/build-aux)
set(AUTOMAKE_LIB ${CMAKE_CURRENT_SOURCE_DIR}/../automake/repo/lib)
file(MAKE_DIRECTORY ${GEN}/m4 ${GEN}/build-aux ${GEN}/libltdl ${GEN}/stage)

find_program(HOST_ACLOCAL aclocal REQUIRED)
find_program(HOST_AUTOCONF autoconf REQUIRED)
find_program(HOST_AUTOHEADER autoheader REQUIRED)
find_program(HOST_AUTOMAKE automake REQUIRED)

set(LT_SERIAL 4441)

set(BOOTSTRAP_EDIT
    -e "s|@MACRO_VERSION@|${PROJECT_VERSION}|g"
    -e "s|@MACRO_REVISION@|${PROJECT_VERSION}|g"
    -e "s|@MACRO_SERIAL@|${LT_SERIAL}|g"
    -e "s|@PACKAGE@|libtool|g"
    -e "s|@PACKAGE_BUGREPORT@|bug-libtool@gnu.org|g"
    -e "s|@PACKAGE_URL@|https://www.gnu.org/software/libtool/|g"
    -e "s|@PACKAGE_NAME@|GNU Libtool|g"
    -e "s|@package_revision@|${PROJECT_VERSION}|g"
    -e "s|@PACKAGE_STRING@|GNU Libtool ${PROJECT_VERSION}|g"
    -e "s|@PACKAGE_TARNAME@|libtool|g"
    -e "s|@PACKAGE_VERSION@|${PROJECT_VERSION}|g"
    -e "s|@VERSION@|${PROJECT_VERSION}|g"
)

set(PKGAUX_SCRIPTS compile config.guess config.sub depcomp install-sh missing)
set(PKGMACRO_FILES libtool.m4 ltargz.m4 ltdl.m4 ltoptions.m4 ltsugar.m4 ltversion.m4 lt~obsolete.m4)
set(PKGLTDL_FILES "COPYING.LIB Makefile.am README configure.ac aclocal.m4 Makefile.in config-h.in configure libltdl/lt__alloc.h libltdl/lt__argz_.h libltdl/lt__dirent.h libltdl/lt__glibc.h libltdl/lt__private.h libltdl/lt__strl.h libltdl/lt_dlloader.h libltdl/lt_error.h libltdl/lt_system.h libltdl/slist.h loaders/dld_link.c loaders/dlopen.c loaders/dyld.c loaders/load_add_on.c loaders/loadlibrary.c loaders/preopen.c loaders/shl_load.c lt__alloc.c lt__argz.c lt__dirent.c lt__strl.c lt_dlloader.c lt_error.c ltdl.c ltdl.h ltdl.mk slist.c")

string(JOIN " " PKGAUX_SCRIPTS_STR ${PKGAUX_SCRIPTS})
string(JOIN " " PKGMACRO_FILES_STR ${PKGMACRO_FILES})
set(CONFIGURE_EDIT ${BOOTSTRAP_EDIT}
    -e "/^\\. /s|@auxscriptsdir@|${BOOTSTRAP_AUX}|g"
    -e "s|@aclocaldir@|/share/aclocal|g"
    -e "s|@aux_dir@|build-aux|g"
    -e "s|@datadir@|/share|g"
    -e "s|@EGREP@|grep -E|g"
    -e "s|@FGREP@|grep -F|g"
    -e "s|@GREP@|grep|g"
    -e "s|@host_triplet@|${CMAKE_SYSTEM_PROCESSOR}-pc-moxnox|g"
    -e "s|@LN_S@|ln -s|g"
    -e "s|@ltdl_ac_aux_dir@|../build-aux|g"
    -e "s|@macro_dir@|m4|g"
    -e "s|@pkgauxdir@|/share/libtool/build-aux|g"
    -e "s|@pkgaux_files@|${PKGAUX_SCRIPTS_STR} ltmain.sh|g"
    -e "s|@pkgaux_install_files@|config.guess config.sub install-sh|g"
    -e "s|@pkgaux_parent_files@|ltmain.sh|g"
    -e "s|@pkgdatadir@|/share/libtool|g"
    -e "s|@pkgltdl_files@|${PKGLTDL_FILES}|g"
    -e "s|@pkgmacro_files@|${PKGMACRO_FILES_STR}|g"
    -e "s|@prefix@|/|g"
    -e "s|@SED@|sed|g"
    -e "s|@srcdir@|.|g"
)

add_custom_command(
    OUTPUT ${GEN}/m4/ltversion.m4
    COMMAND ${HOST_SED} ${BOOTSTRAP_EDIT} ${REPO}/m4/ltversion.in > ${GEN}/m4/ltversion.m4
    DEPENDS ${REPO}/m4/ltversion.in
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/build-aux/ltmain.sh ${GEN}/m4/libtool.m4
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/build-aux/ltmain.in ${BOOTSTRAP_AUX}/funclib.sh ${BOOTSTRAP_AUX}/options-parser ${GEN}/build-aux
    COMMAND sh -c "GREP=grep SED=sed sh '${BOOTSTRAP_AUX}/inline-source' ./build-aux/ltmain.in > '${GEN}/build-aux/ltmain.sh'"
    COMMAND ${HOST_SED} -i ${BOOTSTRAP_EDIT} ${GEN}/build-aux/ltmain.sh
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/m4/libtool.m4 ${GEN}/m4/libtool.m4
    COMMAND ${HOST_PATCH} -p1 -d ${GEN} < ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    DEPENDS ${REPO}/build-aux/ltmain.in ${BOOTSTRAP_AUX}/inline-source ${BOOTSTRAP_AUX}/funclib.sh ${BOOTSTRAP_AUX}/options-parser ${REPO}/m4/libtool.m4 ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/libtoolize
    COMMAND ${HOST_SED} ${CONFIGURE_EDIT} ${REPO}/libtoolize.in > ${GEN}/stage/libtoolize
    COMMAND sh -c "GREP=grep SED=sed sh '${BOOTSTRAP_AUX}/inline-source' libtoolize > '${GEN}/libtoolize'"
    COMMAND ${HOST_SED} -i -e "1s|^#! */usr/bin/env sh|#!/bin/sh|" -e [[/^    test -n "$M4" || {$/i\    : "${M4=m4}"]] ${GEN}/libtoolize
    COMMAND chmod +x ${GEN}/libtoolize
    DEPENDS ${REPO}/libtoolize.in ${BOOTSTRAP_AUX}/inline-source ${BOOTSTRAP_AUX}/funclib.sh ${BOOTSTRAP_AUX}/options-parser ${BOOTSTRAP_AUX}/extract-trace
    WORKING_DIRECTORY ${GEN}/stage
    VERBATIM
)

foreach(_f config.guess config.sub depcomp install-sh)
    configure_file(${GNULIB_AUX}/${_f} ${GEN}/build-aux/${_f} COPYONLY)
endforeach()
foreach(_f compile missing)
    configure_file(${AUTOMAKE_LIB}/${_f} ${GEN}/build-aux/${_f} COPYONLY)
endforeach()
foreach(_f ltargz.m4 ltdl.m4 ltoptions.m4 ltsugar.m4 lt~obsolete.m4 m4.m4)
    configure_file(${REPO}/m4/${_f} ${GEN}/m4/${_f} COPYONLY)
endforeach()

set(LTDL_SOURCES configure.ac ltdl.mk ltdl.c ltdl.h lt__alloc.c lt__argz.c lt__dirent.c lt__strl.c lt_dlloader.c lt_error.c slist.c README
    libltdl/lt__alloc.h libltdl/lt__argz_.h libltdl/lt__dirent.h libltdl/lt__glibc.h libltdl/lt__private.h libltdl/lt__strl.h libltdl/lt_dlloader.h libltdl/lt_error.h libltdl/lt_system.h libltdl/slist.h
    loaders/dld_link.c loaders/dlopen.c loaders/dyld.c loaders/load_add_on.c loaders/loadlibrary.c loaders/preopen.c loaders/shl_load.c)
foreach(_f ${LTDL_SOURCES})
    configure_file(${REPO}/libltdl/${_f} ${GEN}/libltdl/${_f} COPYONLY)
endforeach()
configure_file(${REPO}/gnulib/doc/COPYING.LESSERv2 ${GEN}/libltdl/COPYING.LIB COPYONLY)

file(READ ${REPO}/libltdl/ltdl.mk LTDL_MK)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${REPO}/libltdl/ltdl.mk)
string(FIND "${LTDL_MK}" "## DO NOT REMOVE THIS LINE -- " _marker)
string(SUBSTRING "${LTDL_MK}" 0 ${_marker} LTDL_HEAD)
string(SUBSTRING "${LTDL_MK}" ${_marker} -1 LTDL_BODY)
string(FIND "${LTDL_BODY}" "\n" _nl)
math(EXPR _nl "${_nl} + 1")
string(SUBSTRING "${LTDL_BODY}" ${_nl} -1 LTDL_BODY)

set(_rest "${LTDL_BODY}")
set(LTDL_BODY "")
while(NOT _rest STREQUAL "")
    string(FIND "${_rest}" "\n" _nl)
    if(_nl EQUAL -1)
        set(_line "${_rest}")
        set(_rest "")
    else()
        string(SUBSTRING "${_rest}" 0 ${_nl} _line)
        math(EXPR _nl "${_nl} + 1")
        string(SUBSTRING "${_rest}" ${_nl} -1 _rest)
    endif()
    foreach(_token "libltdl_" "libltdl/")
        string(FIND "${_line}" "${_token}" _p)
        if(_p GREATER -1)
            string(SUBSTRING "${_line}" 0 ${_p} _a)
            math(EXPR _q "${_p} + 8")
            string(SUBSTRING "${_line}" ${_q} -1 _b)
            set(_line "${_a}${_b}")
        endif()
    endforeach()
    string(REPLACE ": libltdl/" ": " _line "${_line}")
    string(REPLACE "$(libltdl_" "$(" _line "${_line}")
    if(NOT _line STREQUAL "\t\t\t  -I$(srcdir)/libltdl -Ilibltdl \\")
        string(APPEND LTDL_BODY "${_line}\n")
    endif()
endwhile()
set(LTDL_VARS "ACLOCAL_AMFLAGS = -I ../m4\nAUTOMAKE_OPTIONS = foreign\nAM_CPPFLAGS =\nAM_LDFLAGS =\nBUILT_SOURCES =\ninclude_HEADERS =\nnoinst_LTLIBRARIES =\nlib_LTLIBRARIES =\nEXTRA_LTLIBRARIES =\nEXTRA_DIST =\nCLEANFILES =\nMOSTLYCLEANFILES =\n")
file(CONFIGURE OUTPUT ${GEN}/libltdl/Makefile.am CONTENT "${LTDL_HEAD}${LTDL_VARS}${LTDL_BODY}" @ONLY)

add_custom_command(
    OUTPUT ${GEN}/libltdl/aclocal.m4 ${GEN}/libltdl/config-h.in ${GEN}/libltdl/configure ${GEN}/libltdl/Makefile.in
    COMMAND ${HOST_ACLOCAL} -I ../m4
    COMMAND ${HOST_AUTOHEADER}
    COMMAND ${HOST_AUTOCONF}
    COMMAND ${HOST_AUTOMAKE} Makefile
    DEPENDS ${GEN}/m4/libtool.m4 ${GEN}/m4/ltversion.m4 ${GEN}/build-aux/ltmain.sh
        ${REPO}/libltdl/configure.ac ${REPO}/m4/ltargz.m4 ${REPO}/m4/ltdl.m4 ${REPO}/m4/ltoptions.m4 ${REPO}/m4/ltsugar.m4 ${REPO}/m4/lt~obsolete.m4 ${REPO}/m4/m4.m4
    WORKING_DIRECTORY ${GEN}/libltdl
    VERBATIM
)

add_custom_target(libtool ALL DEPENDS
    ${GEN}/libtoolize
    ${GEN}/build-aux/ltmain.sh
    ${GEN}/m4/libtool.m4
    ${GEN}/m4/ltversion.m4
    ${GEN}/libltdl/configure
)

install(PROGRAMS ${GEN}/libtoolize DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES
    ${GEN}/m4/libtool.m4
    ${GEN}/m4/ltversion.m4
    repo/m4/ltargz.m4
    repo/m4/ltdl.m4
    repo/m4/ltoptions.m4
    repo/m4/ltsugar.m4
    repo/m4/lt~obsolete.m4
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/aclocal
)

install(PROGRAMS
    ${GEN}/build-aux/compile
    ${GEN}/build-aux/config.guess
    ${GEN}/build-aux/config.sub
    ${GEN}/build-aux/depcomp
    ${GEN}/build-aux/install-sh
    ${GEN}/build-aux/missing
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/libtool/build-aux
)
install(FILES ${GEN}/build-aux/ltmain.sh DESTINATION ${CMAKE_INSTALL_PREFIX}/share/libtool/build-aux)

install(FILES
    ${GEN}/libltdl/COPYING.LIB
    ${GEN}/libltdl/Makefile.am
    ${GEN}/libltdl/aclocal.m4
    ${GEN}/libltdl/Makefile.in
    ${GEN}/libltdl/config-h.in
    repo/libltdl/README
    repo/libltdl/configure.ac
    repo/libltdl/lt__alloc.c
    repo/libltdl/lt__argz.c
    repo/libltdl/lt__dirent.c
    repo/libltdl/lt__strl.c
    repo/libltdl/lt_dlloader.c
    repo/libltdl/lt_error.c
    repo/libltdl/ltdl.c
    repo/libltdl/ltdl.h
    repo/libltdl/ltdl.mk
    repo/libltdl/slist.c
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/libtool
)
install(PROGRAMS ${GEN}/libltdl/configure DESTINATION ${CMAKE_INSTALL_PREFIX}/share/libtool)
install(FILES
    repo/libltdl/libltdl/lt__alloc.h
    repo/libltdl/libltdl/lt__argz_.h
    repo/libltdl/libltdl/lt__dirent.h
    repo/libltdl/libltdl/lt__glibc.h
    repo/libltdl/libltdl/lt__private.h
    repo/libltdl/libltdl/lt__strl.h
    repo/libltdl/libltdl/lt_dlloader.h
    repo/libltdl/libltdl/lt_error.h
    repo/libltdl/libltdl/lt_system.h
    repo/libltdl/libltdl/slist.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/libtool/libltdl
)
install(FILES
    repo/libltdl/loaders/dld_link.c
    repo/libltdl/loaders/dlopen.c
    repo/libltdl/loaders/dyld.c
    repo/libltdl/loaders/load_add_on.c
    repo/libltdl/loaders/loadlibrary.c
    repo/libltdl/loaders/preopen.c
    repo/libltdl/loaders/shl_load.c
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/libtool/loaders
)
