set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

file(STRINGS repo/CMakeLists.txt _version_line REGEX "^project\\(nghttp2 VERSION ")
string(REGEX REPLACE "^project\\(nghttp2 VERSION ([0-9.]+).*" "\\1" PACKAGE_VERSION "${_version_line}")
string(REPLACE "." ";" _version_parts "${PACKAGE_VERSION}")
list(GET _version_parts 0 _version_major)
list(GET _version_parts 1 _version_minor)
list(GET _version_parts 2 _version_patch)
include(repo/cmake/Version.cmake)
HexVersion(PACKAGE_VERSION_NUM ${_version_major} ${_version_minor} ${_version_patch})

configure_file(repo/lib/includes/nghttp2/nghttp2ver.h.in ${GEN}/include/nghttp2/nghttp2ver.h @ONLY)
configure_file(repo/lib/includes/nghttp2/nghttp2.h ${GEN}/include/nghttp2/nghttp2.h COPYONLY)

add_library(nghttp2 SHARED
    repo/lib/nghttp2_pq.c
    repo/lib/nghttp2_map.c
    repo/lib/nghttp2_queue.c
    repo/lib/nghttp2_frame.c
    repo/lib/nghttp2_buf.c
    repo/lib/nghttp2_stream.c
    repo/lib/nghttp2_outbound_item.c
    repo/lib/nghttp2_session.c
    repo/lib/nghttp2_submit.c
    repo/lib/nghttp2_helper.c
    repo/lib/nghttp2_alpn.c
    repo/lib/nghttp2_hd.c
    repo/lib/nghttp2_hd_huffman.c
    repo/lib/nghttp2_hd_huffman_data.c
    repo/lib/nghttp2_version.c
    repo/lib/nghttp2_priority_spec.c
    repo/lib/nghttp2_option.c
    repo/lib/nghttp2_callbacks.c
    repo/lib/nghttp2_mem.c
    repo/lib/nghttp2_http.c
    repo/lib/nghttp2_rcbuf.c
    repo/lib/nghttp2_extpri.c
    repo/lib/nghttp2_ratelim.c
    repo/lib/nghttp2_time.c
    repo/lib/nghttp2_debug.c
    repo/lib/sfparse.c
)

target_compile_definitions(nghttp2 PRIVATE BUILDING_NGHTTP2 HAVE_CONFIG_H)
target_include_directories(nghttp2 PUBLIC ${GEN}/include)
target_include_directories(nghttp2 PRIVATE repo/lib ${CMAKE_CURRENT_SOURCE_DIR})
set_target_properties(nghttp2 PROPERTIES C_VISIBILITY_PRESET hidden)
target_compile_options(nghttp2 PRIVATE ${USERSPACE_CFLAGS})

install(TARGETS nghttp2 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES ${GEN}/include/nghttp2/nghttp2.h ${GEN}/include/nghttp2/nghttp2ver.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/nghttp2)

set(prefix "${CMAKE_INSTALL_PREFIX}")
set(exec_prefix "${CMAKE_INSTALL_PREFIX}")
set(libdir /lib)
set(includedir /include)
set(VERSION ${PACKAGE_VERSION})
configure_file(repo/lib/libnghttp2.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libnghttp2.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libnghttp2.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
