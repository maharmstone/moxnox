add_library(llhttp SHARED
    repo/src/api.c
    repo/src/http.c
    repo/src/llhttp.c
)

target_include_directories(llhttp PUBLIC repo/include)
target_compile_options(llhttp PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default)

target_compile_options(llhttp PRIVATE
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-unused-parameter
    $<$<C_COMPILER_ID:Clang>:-Wno-implicit-int-conversion>
)

install(TARGETS llhttp DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES repo/include/llhttp.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

foreach(part MAJOR MINOR PATCH)
    file(STRINGS repo/include/llhttp.h version_line REGEX "^#define LLHTTP_VERSION_${part} ")
    string(REGEX REPLACE "^#define LLHTTP_VERSION_${part} ([0-9]+).*" "\\1" LLHTTP_VERSION_${part} "${version_line}")
endforeach()

set(PROJECT_VERSION ${LLHTTP_VERSION_MAJOR}.${LLHTTP_VERSION_MINOR}.${LLHTTP_VERSION_PATCH})
set(CMAKE_INSTALL_FULL_BINDIR /bin)
set(CMAKE_INSTALL_FULL_LIBDIR /lib)
set(CMAKE_INSTALL_FULL_INCLUDEDIR /include)

configure_file(repo/libllhttp.pc.in ${CMAKE_CURRENT_BINARY_DIR}/libllhttp.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/libllhttp.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
