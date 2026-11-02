add_library(zstd SHARED
    repo/lib/common/entropy_common.c
    repo/lib/common/error_private.c
    repo/lib/common/fse_decompress.c
    repo/lib/common/xxhash.c
    repo/lib/common/zstd_common.c
    repo/lib/compress/fse_compress.c
    repo/lib/compress/hist.c
    repo/lib/compress/huf_compress.c
    repo/lib/compress/zstd_compress.c
    repo/lib/compress/zstd_compress_literals.c
    repo/lib/compress/zstd_compress_sequences.c
    repo/lib/compress/zstd_compress_superblock.c
    repo/lib/compress/zstd_double_fast.c
    repo/lib/compress/zstd_fast.c
    repo/lib/compress/zstd_lazy.c
    repo/lib/compress/zstd_ldm.c
    repo/lib/compress/zstd_opt.c
    repo/lib/compress/zstd_preSplit.c
    repo/lib/decompress/huf_decompress.c
    repo/lib/decompress/zstd_ddict.c
    repo/lib/decompress/zstd_decompress.c
    repo/lib/decompress/zstd_decompress_block.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    target_sources(zstd PRIVATE repo/lib/decompress/huf_decompress_amd64.S)
endif()

target_compile_options(zstd PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_compile_options(zstd PRIVATE -Wno-sign-conversion -Wno-conversion)
target_include_directories(zstd PUBLIC repo/lib)

install(TARGETS zstd DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES repo/lib/zstd.h repo/lib/zdict.h repo/lib/zstd_errors.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

# FIXME - zlib dependency?

set(ZSTD_PROG ${CMAKE_CURRENT_SOURCE_DIR}/repo/programs)

add_executable(zstdcli
    repo/lib/common/pool.c
    repo/programs/fileio_asyncio.c
    repo/programs/timefn.c
    repo/programs/zstdcli.c
    repo/programs/fileio.c
    repo/programs/util.c
)

set_target_properties(zstdcli PROPERTIES OUTPUT_NAME zstd)

target_compile_options(zstdcli PRIVATE ${USERSPACE_CFLAGS} -include ${CMAKE_CURRENT_SOURCE_DIR}/zstdshim.h)
target_compile_options(zstdcli PRIVATE -Wno-sign-conversion)

target_include_directories(zstdcli PRIVATE ${ZSTD_PROG})
target_compile_definitions(zstdcli PRIVATE -DZSTD_NOBENCH -DZSTD_NODICT -DZSTD_NOTRACE -DZSTD_LEGACY_SUPPORT=0 -D_CRTIMP=)

target_link_libraries(zstdcli PRIVATE zstd)
target_link_libraries(zstdcli PRIVATE moxnox) # FIXME

install(TARGETS zstdcli DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    ARCHIVE EXCLUDE_FROM_ALL
)
