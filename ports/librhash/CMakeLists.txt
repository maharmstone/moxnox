add_library(librhash SHARED
    repo/librhash/aich.c
    repo/librhash/algorithms.c
    repo/librhash/blake2b.c
    repo/librhash/blake2s.c
    repo/librhash/byte_order.c
    repo/librhash/crc32.c
    repo/librhash/ed2k.c
    repo/librhash/edonr.c
    repo/librhash/gost12.c
    repo/librhash/gost94.c
    repo/librhash/has160.c
    repo/librhash/hex.c
    repo/librhash/md4.c
    repo/librhash/md5.c
    repo/librhash/rhash.c
    repo/librhash/rhash_torrent.c
    repo/librhash/ripemd-160.c
    repo/librhash/sha1.c
    repo/librhash/sha256.c
    repo/librhash/sha3.c
    repo/librhash/sha512.c
    repo/librhash/snefru.c
    repo/librhash/tiger.c
    repo/librhash/tiger_sbox.c
    repo/librhash/torrent.c
    repo/librhash/tth.c
    repo/librhash/util.c
    repo/librhash/whirlpool.c
    repo/librhash/whirlpool_sbox.c
)

target_include_directories(librhash PUBLIC repo/librhash)
target_compile_options(librhash PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default)
target_compile_options(librhash PRIVATE -Wno-sign-conversion -Wno-conversion)

file(STRINGS repo/version.h RHASH_VERSION_LINE REGEX "^#define VERSION ")
string(REGEX REPLACE "^#define VERSION \"([0-9]+)\\.([0-9]+)\\.([0-9]+)\".*" "\\1;\\2;\\3"
    RHASH_VERSION_PARTS "${RHASH_VERSION_LINE}")
list(GET RHASH_VERSION_PARTS 0 RHASH_VERSION_MAJOR)
list(GET RHASH_VERSION_PARTS 1 RHASH_VERSION_MINOR)
list(GET RHASH_VERSION_PARTS 2 RHASH_VERSION_PATCH)
math(EXPR RHASH_XVERSION
    "(${RHASH_VERSION_MAJOR} << 24) | (${RHASH_VERSION_MINOR} << 16) | (${RHASH_VERSION_PATCH} << 8)"
    OUTPUT_FORMAT HEXADECIMAL)
target_compile_definitions(librhash PRIVATE RHASH_XVERSION=${RHASH_XVERSION})

set_target_properties(librhash PROPERTIES ARCHIVE_OUTPUT_NAME rhash)

install(TARGETS librhash DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES repo/librhash/rhash.h repo/librhash/rhash_torrent.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
