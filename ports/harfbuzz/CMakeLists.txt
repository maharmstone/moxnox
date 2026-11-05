set(BUILD_SHARED_LIBS ON)
set(HB_HAVE_FREETYPE ON)
set(HB_BUILD_SUBSET OFF)
set(HB_BUILD_UTILS OFF)
set(SKIP_INSTALL_ALL ON)

set(CMAKE_MESSAGE_LOG_LEVEL WARNING)
add_subdirectory(repo)

target_sources(harfbuzz PUBLIC FILE_SET CXX_MODULES FILES harfbuzz.cpp)

set_target_properties(harfbuzz PROPERTIES LINKER_LANGUAGE CXX)
target_compile_features(harfbuzz PUBLIC cxx_std_${CMAKE_CXX_STANDARD})

target_compile_definitions(harfbuzz PRIVATE HB_LEAN HB_NO_AAT HB_NO_MT)

get_target_property(libs harfbuzz LINK_LIBRARIES)
list(REMOVE_ITEM libs Threads::Threads)
set_target_properties(harfbuzz PROPERTIES LINK_LIBRARIES "${libs}")

get_target_property(libs harfbuzz INTERFACE_LINK_LIBRARIES)
list(REMOVE_ITEM libs Threads::Threads)
set_target_properties(harfbuzz PROPERTIES INTERFACE_LINK_LIBRARIES "${libs}")

target_compile_options(harfbuzz PRIVATE
    ${USERSPACE_CFLAGS}
    -g0
    -mno-stack-arg-probe
    -Wno-float-conversion
    -Wno-attributes
    -include ${CMAKE_CURRENT_SOURCE_DIR}/hbshim.h
    $<$<CXX_COMPILER_ID:GNU>:-Wa,-mbig-obj>
)

install(TARGETS harfbuzz DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

install(FILES
    repo/src/hb-aat-layout.h
    repo/src/hb-aat.h
    repo/src/hb-blob.h
    repo/src/hb-buffer.h
    repo/src/hb-common.h
    repo/src/hb-cplusplus.hh
    repo/src/hb-deprecated.h
    repo/src/hb-draw.h
    repo/src/hb-paint.h
    repo/src/hb-face.h
    repo/src/hb-font.h
    repo/src/hb-ft.h
    repo/src/hb-map.h
    repo/src/hb-ot-color.h
    repo/src/hb-ot-deprecated.h
    repo/src/hb-ot-font.h
    repo/src/hb-ot-layout.h
    repo/src/hb-ot-math.h
    repo/src/hb-ot-meta.h
    repo/src/hb-ot-metrics.h
    repo/src/hb-ot-name.h
    repo/src/hb-ot-shape.h
    repo/src/hb-ot-var.h
    repo/src/hb-ot.h
    repo/src/hb-script-list.h
    repo/src/hb-set.h
    repo/src/hb-shape-plan.h
    repo/src/hb-shape.h
    repo/src/hb-style.h
    repo/src/hb-unicode.h
    repo/src/hb-version.h
    repo/src/hb.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/harfbuzz
)

file(STRINGS repo/src/hb-version.h HB_VERSION_LINE REGEX "^#define HB_VERSION_STRING ")
string(REGEX REPLACE "^#define HB_VERSION_STRING \"([0-9.]+)\".*" "\\1" HB_VERSION "${HB_VERSION_LINE}")
configure_file(harfbuzz.pc.in ${CMAKE_CURRENT_BINARY_DIR}/harfbuzz.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/harfbuzz.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
