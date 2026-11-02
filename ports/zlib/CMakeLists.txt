add_library(zlib SHARED
    repo/adler32.c
    repo/compress.c
    repo/crc32.c
    repo/deflate.c
    repo/gzclose.c
    repo/gzlib.c
    repo/gzread.c
    repo/gzwrite.c
    repo/inffast.c
    repo/inflate.c
    repo/inftrees.c
    repo/trees.c
    repo/uncompr.c
    repo/zutil.c
)

target_include_directories(zlib PUBLIC repo)
target_compile_options(zlib PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>)
target_compile_options(zlib PRIVATE -Wno-sign-conversion -Wno-conversion)
target_compile_definitions(zlib PRIVATE Z_HAVE_UNISTD_H)

install(TARGETS zlib DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES repo/zlib.h repo/zconf.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
