project(meson VERSION 1.12.1 LANGUAGES NONE)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
file(MAKE_DIRECTORY ${GEN}/bin)

add_custom_command(
    OUTPUT ${GEN}/mesonbuild.stamp
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${REPO}/mesonbuild ${GEN}/mesonbuild
    COMMAND ${HOST_PATCH} -p1 -d ${GEN} < ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    COMMAND ${CMAKE_COMMAND} -E touch ${GEN}/mesonbuild.stamp
    DEPENDS ${REPO}/mesonbuild/coredata.py ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/bin/meson
    COMMAND ${HOST_SED} -e [[1s|^#!.*|#!/bin/python|]] ${REPO}/meson.py > ${GEN}/bin/meson
    DEPENDS ${REPO}/meson.py
    VERBATIM
)

add_custom_target(meson ALL DEPENDS ${GEN}/mesonbuild.stamp ${GEN}/bin/meson)

install(DIRECTORY ${GEN}/mesonbuild
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13/site-packages
    PATTERN "__pycache__" EXCLUDE)
install(PROGRAMS ${GEN}/bin/meson DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES repo/man/meson.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)

install(FILES repo/data/shell-completions/bash/meson
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/bash-completion/completions)
install(FILES repo/data/shell-completions/zsh/_meson
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/zsh/site-functions)
install(FILES repo/data/syntax-highlighting/vim/ftdetect/meson.vim
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/vim/vimfiles/ftdetect)
install(FILES repo/data/syntax-highlighting/vim/indent/meson.vim
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/vim/vimfiles/indent)
install(FILES repo/data/syntax-highlighting/vim/syntax/meson.vim
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/vim/vimfiles/syntax)
