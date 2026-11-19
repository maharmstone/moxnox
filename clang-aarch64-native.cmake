set(CMAKE_SYSTEM_NAME Moxnox)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)
set(DLLTOOL llvm-dlltool)
set(AR llvm-ar)
set(CMAKE_OBJCOPY objcopy)

set(CLANG_TARGET_TRIPLE aarch64-pc-moxnox)
set(CMAKE_C_COMPILER_TARGET aarch64-pc-moxnox)
set(CMAKE_CXX_COMPILER_TARGET aarch64-pc-moxnox)
set(CMAKE_ASM_COMPILER_TARGET aarch64-pc-moxnox)

set(USE_COMPILER_RT ON CACHE BOOL "" FORCE)

set(CMAKE_INCDIRS_BEFORE_SYSROOT ON)
set(LIBCXX_HEADERS /usr/aarch64-pc-moxnox/usr/include/c++/v1)

set(CMAKE_FIND_ROOT_PATH /usr/aarch64-pc-moxnox)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

set(CMAKE_CXX_STDLIB_MODULES_JSON /usr/aarch64-pc-moxnox/usr/lib/libc++.modules.json)

add_compile_options(--sysroot=/usr/aarch64-pc-moxnox -rtlib=compiler-rt)
add_link_options(--sysroot=/usr/aarch64-pc-moxnox -fuse-ld=lld)
