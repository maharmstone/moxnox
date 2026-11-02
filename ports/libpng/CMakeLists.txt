set(PNG_SHARED ON)
set(PNG_STATIC OFF)
set(PNG_TESTS OFF)
set(PNG_TOOLS OFF)
set(SKIP_INSTALL_ALL ON)

set(ld-version-script OFF)

set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libzlib.dll.a)
set(ZLIB_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../zlib/repo)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE)
add_subdirectory(repo)

set_target_properties(png_shared PROPERTIES DEBUG_POSTFIX "")

target_compile_options(png_shared PRIVATE
    ${USERSPACE_CFLAGS}
    -fvisibility=default
    -Wno-sign-conversion
    -Wno-conversion
)

get_target_property(libs png_shared LINK_LIBRARIES)
list(REMOVE_ITEM libs m)
set_target_properties(png_shared PROPERTIES LINK_LIBRARIES "${libs}")

get_target_property(libs png_shared INTERFACE_LINK_LIBRARIES)
list(REMOVE_ITEM libs m)
set_target_properties(png_shared PROPERTIES INTERFACE_LINK_LIBRARIES "${libs}")

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies png_shared zlib)

install(TARGETS png_shared DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

set(LIBPNG_PUBLIC_HEADERS
    repo/png.h
    repo/pngconf.h
    ${CMAKE_CURRENT_BINARY_DIR}/repo/pnglibconf.h
)

install(FILES ${LIBPNG_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES ${LIBPNG_PUBLIC_HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/libpng16)

add_custom_target(libpng)
add_dependencies(libpng
    png_shared
)
