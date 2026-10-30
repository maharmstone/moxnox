cmake_minimum_required(VERSION 3.28)

project(kernelbase)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

set(SRC_FILES src/kernelbase.def
    src/kernelbase.cpp
    src/sync.cpp
    src/debug.cpp
    src/thread.cpp
    src/process.cpp
    src/loader.cpp
    src/memory.cpp
)

add_library(kernelbase SHARED ${SRC_FILES})

target_compile_options(kernelbase PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_options(kernelbase PRIVATE -nostartfiles -nostdlib)

target_link_libraries(kernelbase PRIVATE format print unique_handle pe pagesize unicode win32error)
target_link_libraries(kernelbase PRIVATE moxnox)

install(TARGETS kernelbase
    COMPONENT core
    RUNTIME DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
    ARCHIVE EXCLUDE_FROM_ALL
)

add_custom_command(TARGET kernelbase POST_BUILD
    COMMAND sh -c "! \"${CMAKE_OBJDUMP}\" -p \"$1\" | grep -q 'DLL Name: kernelbase' || { echo 'Error: kernelbase.dll imports itself' >&2; exit 1; }" -- "$<TARGET_FILE:kernelbase>"
    VERBATIM
    COMMENT "Checking kernelbase.dll does not import itself"
)
