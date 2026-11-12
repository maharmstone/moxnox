cmake_minimum_required(VERSION 3.28)

project(init)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

add_executable(init src/init.cpp)

add_compile_definitions(_GLIBCXX_NO_ASSERTIONS)

target_compile_options(init PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(init format unique_handle print evdev unicode win32error pe)
target_link_libraries(init moxnox)

install(TARGETS init DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
