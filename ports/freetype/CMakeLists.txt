set(FREETYPE_FILES repo/src/autofit/autofit.c
    repo/src/base/ftbase.c
    repo/src/base/ftbbox.c
    repo/src/base/ftbdf.c
    repo/src/base/ftbitmap.c
    repo/src/base/ftcid.c
    repo/src/base/ftdebug.c
    repo/src/base/ftfstype.c
    repo/src/base/ftgasp.c
    repo/src/base/ftglyph.c
    repo/src/base/ftgxval.c
    repo/src/base/ftinit.c
    repo/src/base/ftmm.c
    repo/src/base/ftotval.c
    repo/src/base/ftpatent.c
    repo/src/base/ftpfr.c
    repo/src/base/ftstroke.c
    repo/src/base/ftsynth.c
    repo/src/base/fttype1.c
    repo/src/base/ftwinfnt.c
    repo/src/bzip2/ftbzip2.c
    repo/src/cache/ftcache.c
    repo/src/gzip/ftgzip.c
    repo/src/lzw/ftlzw.c
    repo/src/pcf/pcf.c
    repo/src/pfr/pfr.c
    repo/src/pshinter/pshinter.c
    repo/src/raster/raster.c
    repo/src/sfnt/sfnt.c
    repo/src/smooth/smooth.c
    repo/src/truetype/truetype.c
    repo/src/winfonts/winfnt.c)

add_library(freetype_kernel STATIC ${FREETYPE_FILES})
target_sources(freetype_kernel PUBLIC FILE_SET CXX_MODULES FILES freetype.cpp)

target_compile_definitions(freetype_kernel PRIVATE -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT)
target_compile_definitions(freetype_kernel PRIVATE -DFT_CONFIG_MODULES_H=\"${CMAKE_CURRENT_SOURCE_DIR}/ftmodule.h\")
target_compile_definitions(freetype_kernel PRIVATE -DFT_CONFIG_STANDARD_LIBRARY_H=\"${CMAKE_CURRENT_SOURCE_DIR}/ftstdlib.h\")
target_compile_options(freetype_kernel PRIVATE -mno-stack-arg-probe ${KERNEL_CFLAGS} -Wno-sign-conversion -Wno-unused-function -Wno-conversion -Wno-attributes -Wno-unused-variable -Wno-unused-but-set-variable)
target_include_directories(freetype_kernel PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(freetype_kernel PRIVATE repo/include)

# undo freetype's dllexport markings
add_custom_command(
    TARGET freetype_kernel
    POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} --remove-section .drectve "${CMAKE_CURRENT_BINARY_DIR}/libfreetype_kernel.a"
)

add_library(freetype SHARED
    ${FREETYPE_FILES}
    repo/src/base/ftsystem.c
)

add_library(freetype_module STATIC)
target_sources(freetype_module PUBLIC FILE_SET CXX_MODULES FILES freetype.cpp)
target_include_directories(freetype_module PRIVATE repo/include)
target_compile_options(freetype_module PRIVATE -mno-stack-arg-probe ${GENERAL_CFLAGS} $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_link_libraries(freetype_module PUBLIC freetype)

target_compile_definitions(freetype PRIVATE -DFT2_BUILD_LIBRARY -DFT_CONFIG_OPTION_DISABLE_STREAM_SUPPORT -DFT_CONFIG_OPTION_USE_PNG -DFT_CONFIG_OPTION_ERROR_STRINGS)
target_compile_definitions(freetype PRIVATE -DFT_CONFIG_MODULES_H=\"${CMAKE_CURRENT_SOURCE_DIR}/ftmodule.h\")
target_compile_definitions(freetype PRIVATE -DFT_CONFIG_STANDARD_LIBRARY_H=\"${CMAKE_CURRENT_SOURCE_DIR}/ftstdlib.h\")
target_compile_options(freetype PRIVATE -mno-stack-arg-probe ${USERSPACE_CFLAGS} -fvisibility=default $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_compile_options(freetype PRIVATE -Wno-unused-function -Wno-conversion -Wno-unused-variable -Wno-unused-but-set-variable -Wno-attributes)
target_include_directories(freetype PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(freetype PUBLIC repo/include)
target_link_libraries(freetype PRIVATE png_shared)

install(TARGETS freetype DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES repo/include/ft2build.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/freetype2)
install(DIRECTORY repo/include/freetype/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include/freetype2/freetype
    PATTERN "internal" EXCLUDE
    PATTERN "ftmodule.h" EXCLUDE
    PATTERN "ftoption.h" EXCLUDE
    PATTERN "ftstdlib.h" EXCLUDE
)
install(FILES ftmodule.h ftoption.h ftstdlib.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/freetype2/freetype/config)
