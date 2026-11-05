set(JSONCPP_WITH_TESTS OFF)
set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF)
set(JSONCPP_WITH_PKGCONFIG_SUPPORT ON)
set(JSONCPP_WITH_CMAKE_PACKAGE OFF)
set(BUILD_SHARED_LIBS ON)
set(BUILD_STATIC_LIBS OFF)
set(BUILD_OBJECT_LIBS OFF)

# work round broken jsoncpp check
set(ITK_BINARY_DIR "unused")

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE) # jsoncpp is chatty
add_subdirectory(repo)

add_custom_target(jsoncpp)
add_dependencies(jsoncpp jsoncpp_lib)

target_compile_options(jsoncpp_lib PRIVATE ${USERSPACE_CFLAGS})
