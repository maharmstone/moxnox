set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

set(EXPAT_BUILD_TOOLS OFF)
set(EXPAT_BUILD_EXAMPLES OFF)
set(EXPAT_BUILD_TESTS OFF)
set(EXPAT_BUILD_DOCS OFF)
set(EXPAT_BUILD_PKGCONFIG ON)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE) # expat is very chatty
add_subdirectory(repo/expat)

target_compile_options(expat PRIVATE ${USERSPACE_CFLAGS})

target_compile_options(expat PRIVATE
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-format
)
