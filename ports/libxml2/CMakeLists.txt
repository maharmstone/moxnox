set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)

file(MAKE_DIRECTORY ${GEN})

add_custom_command(
    OUTPUT ${GEN}/dict.c
    COMMAND sh -c "sed -e 's|^    #include <sys/random.h>$|    #if !defined(__moxnox__)\\n    #include <sys/random.h>\\n    #endif|' '${REPO}/dict.c' > '${GEN}/dict.c'"
    DEPENDS ${REPO}/dict.c
    VERBATIM
)

add_custom_target(libxml2_patched DEPENDS ${GEN}/dict.c)

set(HAVE_DECL_GETENTROPY 1 CACHE INTERNAL "")

set(BUILD_SHARED_LIBS ON)
set(LIBXML2_WITH_PROGRAMS ON)
set(LIBXML2_WITH_TESTS OFF)
set(LIBXML2_WITH_DOCS OFF)
set(LIBXML2_WITH_PYTHON OFF)
set(LIBXML2_WITH_ICU OFF)
set(LIBXML2_WITH_READLINE ON)
set(LIBXML2_WITH_HISTORY ON)
set(HAVE_LIBREADLINE 1 CACHE INTERNAL "")
set(HAVE_LIBHISTORY 1 CACHE INTERNAL "")
set(LIBXML2_WITH_LEGACY OFF)
set(LIBXML2_WITH_HTTP OFF)
set(LIBXML2_WITH_TLS OFF)
set(LIBXML2_WITH_ICONV ON)
set(LIBXML2_WITH_THREADS ON)
set(LIBXML2_WITH_MODULES ON)
set(LIBXML2_WITH_SCHEMATRON ON)

set(LIBXML2_WITH_ZLIB ON)
set(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/zlib/repo)
set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libz.dll.a)

set(Iconv_IS_BUILT_IN FALSE)
set(Iconv_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/libiconv/gen/include)
set(Iconv_LIBRARY ${CMAKE_BINARY_DIR}/ports/libiconv/libiconv.dll.a)

set(CMAKE_INSTALL_PREFIX /)
set(CMAKE_INSTALL_BINDIR /bin)
set(CMAKE_INSTALL_LIBDIR /lib)
set(CMAKE_INSTALL_INCLUDEDIR /include)
set(CMAKE_INSTALL_DATAROOTDIR /share)
set(CMAKE_INSTALL_SYSCONFDIR /etc)

add_subdirectory(repo)

get_target_property(_sources LibXml2 SOURCES)
list(TRANSFORM _sources REPLACE "^(.*/)?dict\\.c$" "${GEN}/dict.c")
set_target_properties(LibXml2 PROPERTIES SOURCES "${_sources}")
set_source_files_properties(${GEN}/dict.c TARGET_DIRECTORY LibXml2 PROPERTIES GENERATED TRUE)
add_dependencies(LibXml2 libxml2_patched)
target_include_directories(LibXml2 PRIVATE ${REPO})

foreach(_prop LINK_LIBRARIES INTERFACE_LINK_LIBRARIES)
    get_target_property(_libs LibXml2 ${_prop})
    if(_libs)
        list(REMOVE_ITEM _libs m)
        set_target_properties(LibXml2 PROPERTIES ${_prop} "${_libs}")
    endif()
endforeach()

target_compile_options(LibXml2 PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default)

set(LIBXML2_WARNINGS
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-attributes
    -Wno-shorten-64-to-32
    -Wno-implicit-int-conversion
    -Wno-float-conversion
)

target_compile_options(LibXml2 PRIVATE ${LIBXML2_WARNINGS})

foreach(_prog xmllint xmlcatalog)
    target_compile_options(${_prog} PRIVATE ${USERSPACE_CFLAGS} ${LIBXML2_WARNINGS})

    get_target_property(_libs ${_prog} LINK_LIBRARIES)
    list(REMOVE_ITEM _libs history)
    set_target_properties(${_prog} PROPERTIES LINK_LIBRARIES "${_libs}")
endforeach()

add_custom_target(libxml2)
add_dependencies(libxml2 LibXml2 xmllint xmlcatalog)

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies LibXml2 zlib libiconv)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies xmllint readline)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies xmlcatalog readline)
