add_library(cairo SHARED
    repo/src/cairo-analysis-surface.c
    repo/src/cairo-arc.c
    repo/src/cairo-array.c
    repo/src/cairo-atomic.c
    repo/src/cairo-base64-stream.c
    repo/src/cairo-base85-stream.c
    repo/src/cairo-bentley-ottmann-rectangular.c
    repo/src/cairo-bentley-ottmann-rectilinear.c
    repo/src/cairo-bentley-ottmann.c
    repo/src/cairo-botor-scan-converter.c
    repo/src/cairo-boxes-intersect.c
    repo/src/cairo-boxes.c
    repo/src/cairo-cache.c
    repo/src/cairo-clip-boxes.c
    repo/src/cairo-clip-polygon.c
    repo/src/cairo-clip-region.c
    repo/src/cairo-clip-surface.c
    repo/src/cairo-clip-tor-scan-converter.c
    repo/src/cairo-clip.c
    repo/src/cairo-color.c
    repo/src/cairo-composite-rectangles.c
    repo/src/cairo-compositor.c
    repo/src/cairo-contour.c
    repo/src/cairo-damage.c
    repo/src/cairo-debug.c
    repo/src/cairo-default-context.c
    repo/src/cairo-device.c
    repo/src/cairo-error.c
    repo/src/cairo-fallback-compositor.c
    repo/src/cairo-fixed.c
    repo/src/cairo-font-face-twin-data.c
    repo/src/cairo-font-face-twin.c
    repo/src/cairo-font-face.c
    repo/src/cairo-font-options.c
    repo/src/cairo-freed-pool.c
    repo/src/cairo-freelist.c
    repo/src/cairo-gstate.c
    repo/src/cairo-hash.c
    repo/src/cairo-hull.c
    repo/src/cairo-image-compositor.c
    repo/src/cairo-image-info.c
    repo/src/cairo-image-source.c
    repo/src/cairo-image-surface.c
    repo/src/cairo-line.c
    repo/src/cairo-lzw.c
    repo/src/cairo-mask-compositor.c
    repo/src/cairo-matrix.c
    repo/src/cairo-mempool.c
    repo/src/cairo-mesh-pattern-rasterizer.c
    repo/src/cairo-misc.c
    repo/src/cairo-mono-scan-converter.c
    repo/src/cairo-mutex.c
    repo/src/cairo-no-compositor.c
    repo/src/cairo-observer.c
    repo/src/cairo-output-stream.c
    repo/src/cairo-paginated-surface.c
    repo/src/cairo-path-bounds.c
    repo/src/cairo-path-fill.c
    repo/src/cairo-path-fixed.c
    repo/src/cairo-path-in-fill.c
    repo/src/cairo-path-stroke-boxes.c
    repo/src/cairo-path-stroke-polygon.c
    repo/src/cairo-path-stroke-traps.c
    repo/src/cairo-path-stroke-tristrip.c
    repo/src/cairo-path-stroke.c
    repo/src/cairo-path.c
    repo/src/cairo-pattern.c
    repo/src/cairo-pen.c
    repo/src/cairo-polygon-intersect.c
    repo/src/cairo-polygon-reduce.c
    repo/src/cairo-polygon.c
    repo/src/cairo-raster-source-pattern.c
    repo/src/cairo-recording-surface.c
    repo/src/cairo-rectangle.c
    repo/src/cairo-rectangular-scan-converter.c
    repo/src/cairo-region.c
    repo/src/cairo-rtree.c
    repo/src/cairo-scaled-font.c
    repo/src/cairo-shape-mask-compositor.c
    repo/src/cairo-slope.c
    repo/src/cairo-spans-compositor.c
    repo/src/cairo-spans.c
    repo/src/cairo-spline.c
    repo/src/cairo-stroke-dash.c
    repo/src/cairo-stroke-style.c
    repo/src/cairo-surface-clipper.c
    repo/src/cairo-surface-fallback.c
    repo/src/cairo-surface-observer.c
    repo/src/cairo-surface-offset.c
    repo/src/cairo-surface-snapshot.c
    repo/src/cairo-surface-subsurface.c
    repo/src/cairo-surface-wrapper.c
    repo/src/cairo-surface.c
    repo/src/cairo-time.c
    repo/src/cairo-tor-scan-converter.c
    repo/src/cairo-tor22-scan-converter.c
    repo/src/cairo-toy-font-face.c
    repo/src/cairo-traps-compositor.c
    repo/src/cairo-traps.c
    repo/src/cairo-tristrip.c
    repo/src/cairo-unicode.c
    repo/src/cairo-user-font.c
    repo/src/cairo-version.c
    repo/src/cairo-wideint.c
    repo/src/cairo.c
    repo/src/cairo-cff-subset.c
    repo/src/cairo-scaled-font-subsets.c
    repo/src/cairo-truetype-subset.c
    repo/src/cairo-type1-fallback.c
    repo/src/cairo-type1-glyph-names.c
    repo/src/cairo-type1-subset.c
    repo/src/cairo-type3-glyph-surface.c
    repo/src/cairo-pdf-operators.c
    repo/src/cairo-pdf-shading.c
    repo/src/cairo-tag-attributes.c
    repo/src/cairo-tag-stack.c
    repo/src/cairo-deflate-stream.c
    repo/src/cairo-png.c
    repo/src/cairo-ft-font.c
    repo/src/cairo-colr-glyph-render.c
    repo/src/cairo-svg-glyph-render.c
    repo/src/cairo-ps-surface.c
    repo/src/cairo-pdf-surface.c
    repo/src/cairo-pdf-interchange.c
    repo/src/cairo-svg-surface.c
    repo/src/cairo-script-surface.c
    repo/src/cairo-tee-surface.c
)
target_sources(cairo PUBLIC FILE_SET CXX_MODULES FILES cairo.cpp)

target_compile_definitions(cairo PRIVATE CAIRO_COMPILATION HAVE_CONFIG_H)

target_compile_options(cairo PRIVATE ${USERSPACE_CFLAGS})
target_compile_options(cairo PRIVATE -Wno-float-conversion -Wno-enum-conversion -Wno-unused-parameter -Wno-sign-conversion -Wno-conversion -Wno-unused-but-set-variable -Wno-missing-field-initializers -Wno-unused-variable $<$<COMPILE_LANGUAGE:C>:-Wno-unterminated-string-initialization> -Wno-unused-function -Wno-return-type -Wno-array-bounds $<$<C_COMPILER_ID:GNU>:-Wno-maybe-uninitialized -Wno-alloc-size-larger-than -Wno-stringop-overflow> -Wno-missing-braces)
target_compile_options(cairo PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)

target_include_directories(cairo PUBLIC repo/src)
target_include_directories(cairo PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(cairo PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(cairo PRIVATE pixman freetype png_shared zlib)

install(TARGETS cairo
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)

install(FILES
    repo/src/cairo.h
    repo/src/cairo-deprecated.h
    repo/src/cairo-ft.h
    repo/src/cairo-pdf.h
    repo/src/cairo-ps.h
    repo/src/cairo-script.h
    repo/src/cairo-svg.h
    repo/src/cairo-tee.h
    repo/src/cairo-version.h
    cairo-features.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/cairo
)

add_executable(cairo-test cairo-test.cpp)

target_compile_options(cairo-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(cairo-test moxnox print unique_handle evdev)
target_link_libraries(cairo-test cairo)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(cairo-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS cairo-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
