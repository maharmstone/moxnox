set(CMAKE_TRY_COMPILE_TARGET_TYPE EXECUTABLE)

set(BUILD_TESTING OFF)
set(CMake_ENABLE_DEBUGGER OFF)
set(BUILD_CursesDialog OFF)
set(CMAKE_USE_OPENSSL OFF)

set(CMAKE_DISABLE_FIND_PACKAGE_PkgConfig TRUE)

set(CURL_NO_CURL_CMAKE ON)

set(CMAKE_USE_SYSTEM_LIBRARY_CURL ON)
set(CMAKE_USE_SYSTEM_LIBRARY_EXPAT ON)
set(CMAKE_USE_SYSTEM_LIBRARY_JSONCPP ON)
set(CMAKE_USE_SYSTEM_LIBRARY_LIBRHASH ON)
set(CMAKE_USE_SYSTEM_LIBRARY_LIBUV ON)
set(CMAKE_USE_SYSTEM_LIBRARY_ZLIB ON)
set(CMAKE_USE_SYSTEM_LIBRARY_LIBARCHIVE ON)

set(CURL_LIBRARY ${CMAKE_BINARY_DIR}/ports/curl/repo/lib/libcurl.dll.a)
set(CURL_LIBRARY_RELEASE ${CMAKE_BINARY_DIR}/ports/curl/repo/lib/libcurl.dll.a)
set(CURL_INCLUDE_DIR ${CMAKE_BINARY_DIR}/ports/curl/gen/include)
set(EXPAT_LIBRARY ${CMAKE_BINARY_DIR}/ports/expat/repo/expat/libexpat.dll.a)
set(EXPAT_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/expat/repo/expat/lib)
set(JsonCpp_LIBRARY ${CMAKE_BINARY_DIR}/ports/jsoncpp/repo/src/lib_json/libjsoncpp.dll.a)
set(JsonCpp_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/jsoncpp/repo/include)
set(LibRHash_LIBRARY ${CMAKE_BINARY_DIR}/ports/librhash/librhash.dll.a)
set(LibRHash_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/librhash/repo/librhash)
set(LibUV_LIBRARY ${CMAKE_BINARY_DIR}/ports/libuv/repo/libuv.dll.a)
set(LibUV_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/libuv/repo/include)
set(ZLIB_LIBRARY ${CMAKE_BINARY_DIR}/ports/zlib/libz.dll.a)
set(ZLIB_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/zlib/repo)
set(LibArchive_LIBRARY ${CMAKE_BINARY_DIR}/ports/libarchive/repo/libarchive/libarchive.dll.a)
set(LibArchive_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/ports/libarchive/repo/libarchive)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
file(MAKE_DIRECTORY ${GEN}/Source/kwsys ${GEN}/Modules)

add_custom_command(
    OUTPUT ${GEN}/Source/kwsys/ProcessUNIX.c
        ${GEN}/Source/kwsys/SystemInformation.cxx
        ${GEN}/Source/kwsys/SystemTools.cxx
        ${GEN}/Source/cmFindProgramCommand.cxx
        ${GEN}/Source/cmInstallCommand.cxx
        ${GEN}/Modules/CMakeDetermineSystem.cmake
    COMMAND sh -c "cp '${REPO}/Source/kwsys/ProcessUNIX.c' '${REPO}/Source/kwsys/SystemInformation.cxx' '${REPO}/Source/kwsys/SystemTools.cxx' '${GEN}/Source/kwsys/' && cp '${REPO}/Source/cmFindProgramCommand.cxx' '${REPO}/Source/cmInstallCommand.cxx' '${GEN}/Source/' && cp '${REPO}/Modules/CMakeDetermineSystem.cmake' '${GEN}/Modules/' && patch -p1 -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch'"
    DEPENDS
        ${REPO}/Source/kwsys/ProcessUNIX.c
        ${REPO}/Source/kwsys/SystemInformation.cxx
        ${REPO}/Source/kwsys/SystemTools.cxx
        ${REPO}/Source/cmFindProgramCommand.cxx
        ${REPO}/Source/cmInstallCommand.cxx
        ${REPO}/Modules/CMakeDetermineSystem.cmake
        ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    VERBATIM
)

add_custom_target(cmake_patched DEPENDS
    ${GEN}/Source/kwsys/ProcessUNIX.c
    ${GEN}/Source/kwsys/SystemInformation.cxx
    ${GEN}/Source/kwsys/SystemTools.cxx
    ${GEN}/Source/cmFindProgramCommand.cxx
    ${GEN}/Source/cmInstallCommand.cxx
    ${GEN}/Modules/CMakeDetermineSystem.cmake
)

set(CMAKE_MESSAGE_LOG_LEVEL NOTICE) # cmake is very chatty

set(CMAKE_DOC_DIR share/doc/cmake)

add_subdirectory(repo)

function(cmake_port_swap_source target name_regex patched)
    get_target_property(_sources ${target} SOURCES)
    list(TRANSFORM _sources REPLACE "^(.*/)?${name_regex}$" "${patched}")
    set_target_properties(${target} PROPERTIES SOURCES "${_sources}")
    set_source_files_properties(${patched} TARGET_DIRECTORY ${target}
        PROPERTIES GENERATED TRUE)
    add_dependencies(${target} cmake_patched)
endfunction()

cmake_port_swap_source(cmsys_c "ProcessUNIX\\.c" ${GEN}/Source/kwsys/ProcessUNIX.c)
cmake_port_swap_source(cmsys "ProcessUNIX\\.c" ${GEN}/Source/kwsys/ProcessUNIX.c)
cmake_port_swap_source(cmsys "SystemInformation\\.cxx" ${GEN}/Source/kwsys/SystemInformation.cxx)
cmake_port_swap_source(cmsys "SystemTools\\.cxx" ${GEN}/Source/kwsys/SystemTools.cxx)
cmake_port_swap_source(CMakeLib "cmFindProgramCommand\\.cxx" ${GEN}/Source/cmFindProgramCommand.cxx)
cmake_port_swap_source(CMakeLib "cmInstallCommand\\.cxx" ${GEN}/Source/cmInstallCommand.cxx)

get_property(_process_flags SOURCE ${REPO}/Source/kwsys/ProcessUNIX.c DIRECTORY ${REPO}/Source/kwsys PROPERTY COMPILE_FLAGS)
set_property(SOURCE ${GEN}/Source/kwsys/ProcessUNIX.c DIRECTORY ${REPO}/Source/kwsys PROPERTY COMPILE_FLAGS "${_process_flags}")
get_property(_systemtools_defs SOURCE ${REPO}/Source/kwsys/SystemTools.cxx DIRECTORY ${REPO}/Source/kwsys PROPERTY COMPILE_DEFINITIONS)
set_property(SOURCE ${GEN}/Source/kwsys/SystemTools.cxx DIRECTORY ${REPO}/Source/kwsys PROPERTY COMPILE_DEFINITIONS "${_systemtools_defs}")

target_include_directories(cmsys PRIVATE ${REPO}/Source/kwsys)
target_include_directories(cmsys_c PRIVATE ${REPO}/Source/kwsys)

get_directory_property(CMAKE_PORT_DATA_DIR DIRECTORY ${REPO} DEFINITION CMAKE_DATA_DIR)
install(FILES
    Modules/Platform/Moxnox.cmake
    Modules/Platform/Moxnox-Clang-ASM.cmake
    Modules/Platform/Moxnox-Clang-C.cmake
    Modules/Platform/Moxnox-Clang-CXX.cmake
    Modules/Platform/Moxnox-GNU.cmake
    Modules/Platform/Moxnox-GNU-ASM.cmake
    Modules/Platform/Moxnox-GNU-C.cmake
    Modules/Platform/Moxnox-GNU-CXX.cmake
    DESTINATION ${CMAKE_PORT_DATA_DIR}/Modules/Platform)

# overrides the copy installed by the upstream install(DIRECTORY Modules)
install(FILES ${GEN}/Modules/CMakeDetermineSystem.cmake
    DESTINATION ${CMAKE_PORT_DATA_DIR}/Modules)

foreach(target cmake CMakeLib ctest CTestLib cpack CPackLib)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/ports/libuv)
    target_compile_options(${target} PRIVATE "SHELL:-include ${CMAKE_SOURCE_DIR}/ports/libuv/repo/include/uv/posix.h")
endforeach()

target_compile_options(cmake PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion -Wno-implicit-int-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(CMakeLib PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion -Wno-implicit-int-conversion -Wno-float-conversion -Wno-implicit-int-float-conversion -Wno-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(cmllpkgc PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(cmstd PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(cmsys PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion -Wno-conversion -Wno-shorten-64-to-32 $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(cmsys_c PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(cpack PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(CPackLib PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(ctest PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)
target_compile_options(CTestLib PRIVATE ${USERSPACE_CFLAGS} -g0 -Wno-sign-conversion -Wno-float-conversion $<$<COMPILE_LANGUAGE:CXX>:-frtti>)

cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies CMakeLib libcurl_shared expat jsoncpp_lib librhash uv zlib archive)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies CTestLib libcurl_shared expat jsoncpp_lib librhash uv zlib archive)
cmake_language(DEFER DIRECTORY ${CMAKE_SOURCE_DIR}
    CALL add_dependencies CPackLib libcurl_shared expat jsoncpp_lib librhash uv zlib archive)
