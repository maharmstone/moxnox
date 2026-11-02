add_library(bzip2 SHARED
    repo/blocksort.c
    repo/bzlib.c
    repo/compress.c
    repo/crctable.c
    repo/decompress.c
    repo/huffman.c
    repo/randtable.c
)

target_include_directories(bzip2 PUBLIC repo)
target_compile_options(bzip2 PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default)

target_compile_options(bzip2 PRIVATE
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-implicit-fallthrough
)

install(TARGETS bzip2 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES repo/bzlib.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

set_target_properties(bzip2 PROPERTIES OUTPUT_NAME bz2)

add_executable(bzip2_exe repo/bzip2.c)

target_compile_options(bzip2_exe PRIVATE ${USERSPACE_CFLAGS})

target_compile_options(bzip2_exe PRIVATE
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
)

set_target_properties(bzip2_exe PROPERTIES OUTPUT_NAME bzip2)

target_link_libraries(bzip2_exe PRIVATE bzip2 moxnox)

install(TARGETS bzip2_exe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
