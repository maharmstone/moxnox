project(pkgconf VERSION 3.0.7)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)

configure_file(config.h ${GEN}/libpkgconf/config.h COPYONLY)

set(PKGCONF_DEFINES
    HAVE_CONFIG_H
    _ATFILE_SOURCE _BSD_SOURCE _DARWIN_C_SOURCE _DEFAULT_SOURCE
    _POSIX_C_SOURCE=200809L _XOPEN_SOURCE=700
)

set(PKGCONF_PATH_DEFINES
    PERSONALITY_PATH="/share/pkgconfig/personality.d:/etc/pkgconfig/personality.d"
    PKG_DEFAULT_PATH="/lib/pkgconfig:/share/pkgconfig"
    SYSTEM_INCLUDEDIR="/include"
    SYSTEM_LIBDIR="/lib"
)

set(PKGCONF_CFLAGS ${USERSPACE_CFLAGS} -std=c99 -Wno-sign-conversion)

add_library(libpkgconf SHARED
    repo/libpkgconf/argvsplit.c
    repo/libpkgconf/audit.c
    repo/libpkgconf/bsdstubs.c
    repo/libpkgconf/buffer.c
    repo/libpkgconf/bufferset.c
    repo/libpkgconf/bytecode.c
    repo/libpkgconf/cache.c
    repo/libpkgconf/client.c
    repo/libpkgconf/dependency.c
    repo/libpkgconf/fileio.c
    repo/libpkgconf/fragment.c
    repo/libpkgconf/license.c
    repo/libpkgconf/output.c
    repo/libpkgconf/parser.c
    repo/libpkgconf/path.c
    repo/libpkgconf/personality.c
    repo/libpkgconf/pkg.c
    repo/libpkgconf/queue.c
    repo/libpkgconf/tuple.c
    repo/libpkgconf/variable.c
    repo/libpkgconf/version.c
)
set_target_properties(libpkgconf PROPERTIES OUTPUT_NAME pkgconf PREFIX lib)
target_include_directories(libpkgconf PUBLIC ${REPO} PRIVATE ${GEN})
target_compile_definitions(libpkgconf PRIVATE ${PKGCONF_DEFINES} ${PKGCONF_PATH_DEFINES})
target_compile_options(libpkgconf PRIVATE ${PKGCONF_CFLAGS} -fvisibility=default)
target_link_libraries(libpkgconf PRIVATE moxnox)

add_executable(pkgconf_exe
    repo/cli/main.c
    repo/cli/core.c
    repo/cli/getopt_long.c
    repo/cli/renderer-msvc.c
)
set_target_properties(pkgconf_exe PROPERTIES OUTPUT_NAME pkgconf)
target_include_directories(pkgconf_exe PRIVATE ${GEN} ${REPO}/cli)
target_compile_definitions(pkgconf_exe PRIVATE ${PKGCONF_DEFINES} ${PKGCONF_PATH_DEFINES})
target_compile_options(pkgconf_exe PRIVATE ${PKGCONF_CFLAGS})
target_link_libraries(pkgconf_exe PRIVATE libpkgconf moxnox)

add_executable(bomtool
    repo/cli/bomtool/main.c
    repo/cli/getopt_long.c
)
target_include_directories(bomtool PRIVATE ${GEN} ${REPO}/cli ${REPO}/cli/bomtool)
target_compile_definitions(bomtool PRIVATE ${PKGCONF_DEFINES} ${PKGCONF_PATH_DEFINES})
target_compile_options(bomtool PRIVATE ${PKGCONF_CFLAGS})
target_link_libraries(bomtool PRIVATE libpkgconf moxnox)

add_executable(spdxtool
    repo/cli/spdxtool/main.c
    repo/cli/spdxtool/core.c
    repo/cli/spdxtool/software.c
    repo/cli/spdxtool/serialize.c
    repo/cli/spdxtool/simplelicensing.c
    repo/cli/spdxtool/util.c
    repo/cli/spdxtool/generate.c
    repo/cli/getopt_long.c
)
target_include_directories(spdxtool PRIVATE ${GEN} ${REPO}/cli ${REPO}/cli/spdxtool)
target_compile_definitions(spdxtool PRIVATE ${PKGCONF_DEFINES} ${PKGCONF_PATH_DEFINES})
target_compile_options(spdxtool PRIVATE ${PKGCONF_CFLAGS})
target_link_libraries(spdxtool PRIVATE libpkgconf moxnox)

add_executable(pccritic
    repo/cli/pccritic/main.c
    repo/cli/pccritic/critic.c
    repo/cli/getopt_long.c
)
target_include_directories(pccritic PRIVATE ${GEN} ${REPO}/cli ${REPO}/cli/pccritic)
target_compile_definitions(pccritic PRIVATE ${PKGCONF_DEFINES} ${PKGCONF_PATH_DEFINES})
target_compile_options(pccritic PRIVATE ${PKGCONF_CFLAGS})
target_link_libraries(pccritic PRIVATE libpkgconf moxnox)

set(prefix /)
set(includedir /include)
set(libdir /lib)
set(datarootdir /share)
set(datadir /share)
set(PACKAGE_VERSION ${PROJECT_VERSION})
configure_file(repo/libpkgconf.pc.in ${GEN}/libpkgconf.pc @ONLY)

install(TARGETS libpkgconf DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS pkgconf_exe bomtool spdxtool pccritic DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(CODE "file(CREATE_LINK pkgconf.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/pkg-config.exe\" SYMBOLIC)")
install(FILES
    repo/libpkgconf/bsdstubs.h
    repo/libpkgconf/iter.h
    repo/libpkgconf/libpkgconf.h
    repo/libpkgconf/stdinc.h
    repo/libpkgconf/libpkgconf-api.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/pkgconf/libpkgconf)
install(FILES ${GEN}/libpkgconf.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
install(FILES repo/pkg.m4 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/aclocal)
install(FILES repo/man/bomtool.1 repo/man/pkgconf.1 repo/man/spdxtool.1 repo/man/pccritic.1
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)
install(CODE "file(CREATE_LINK pkgconf.1 \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/share/man/man1/pkg-config.1\" SYMBOLIC)")
install(FILES repo/man/pc.5 repo/man/pkgconf-personality.5 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man5)
install(FILES repo/man/pkg.m4.7 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man7)
install(FILES repo/CONTRIBUTING.md repo/DCO repo/README.md repo/AUTHORS repo/COPYING
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/doc/pkgconf)

add_custom_target(pkgconf)
add_dependencies(pkgconf libpkgconf pkgconf_exe bomtool spdxtool pccritic)
