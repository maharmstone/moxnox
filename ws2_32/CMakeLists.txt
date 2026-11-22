cmake_minimum_required(VERSION 3.28)

project(ws2_32)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/ws2_32.def
    src/ws2_32.cpp
    src/getaddrinfo.cpp
)

add_library(ws2_32 SHARED ${SRC_FILES})

target_compile_options(ws2_32 PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})

target_link_libraries(ws2_32 PRIVATE unique_handle errno print ipv4 timepage)
target_link_libraries(ws2_32 PRIVATE moxnox)

install(TARGETS ws2_32
    COMPONENT core
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)
