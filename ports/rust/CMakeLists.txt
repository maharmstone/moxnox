include(${CMAKE_SOURCE_DIR}/rust-port.cmake)

init_rust_crate_submodules(
    ports/rust/errno
    ports/rust/filetime
    ports/rust/getrandom
    ports/rust/getrandom-0.2
    ports/rust/jobserver
    ports/rust/libc
    ports/rust/libloading
    ports/rust/libloading-0.9
    ports/rust/memmap2
    ports/rust/nix
    ports/rust/rand
    ports/rust/rustix
    ports/rust/socket2
    ports/rust/stacker
    ports/rust/termize
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(RUST_TARGET x86_64-pc-moxnox)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(RUST_TARGET aarch64-pc-moxnox)
else()
    message(FATAL_ERROR "no Rust target for ${CMAKE_SYSTEM_PROCESSOR}")
endif()

if(CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(RUST_BUILD_TRIPLE x86_64-unknown-linux-gnu)
else()
    message(FATAL_ERROR "no Rust build triple for ${CMAKE_HOST_SYSTEM_PROCESSOR}")
endif()

find_program(RUSTC_STAGE0 rustc HINTS /usr/bin REQUIRED)
find_program(CARGO_STAGE0 cargo HINTS /usr/bin REQUIRED)
find_program(RUST_HOST_LLVM_CONFIG llvm-config REQUIRED)
find_program(RUST_PYTHON python3 REQUIRED)

set(RUSTC_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/build)

configure_file(bootstrap.toml.in ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.toml @ONLY)

set(RUSTC_STAGE1 ${RUSTC_BUILD_DIR}/${RUST_TARGET}/stage1)
set(rustc_main ${RUSTC_BUILD_DIR}/${RUST_BUILD_TRIPLE}/stage1-rustc/${RUST_TARGET}/release/rustc-main.exe)

rust_export_crate(libc ${CMAKE_CURRENT_SOURCE_DIR}/libc ${CMAKE_CURRENT_SOURCE_DIR}/libc.patch)
rust_export_crate(rustix ${CMAKE_CURRENT_SOURCE_DIR}/rustix ${CMAKE_CURRENT_SOURCE_DIR}/rustix.patch)
rust_export_crate(nix ${CMAKE_CURRENT_SOURCE_DIR}/nix ${CMAKE_CURRENT_SOURCE_DIR}/nix.patch)
rust_export_crate(errno ${CMAKE_CURRENT_SOURCE_DIR}/errno ${CMAKE_CURRENT_SOURCE_DIR}/errno.patch)
rust_export_crate(jobserver ${CMAKE_CURRENT_SOURCE_DIR}/jobserver ${CMAKE_CURRENT_SOURCE_DIR}/jobserver.patch)
rust_export_crate(termize ${CMAKE_CURRENT_SOURCE_DIR}/termize ${CMAKE_CURRENT_SOURCE_DIR}/termize.patch)
rust_export_crate(rand ${CMAKE_CURRENT_SOURCE_DIR}/rand ${CMAKE_CURRENT_SOURCE_DIR}/rand.patch)
rust_export_crate(libloading ${CMAKE_CURRENT_SOURCE_DIR}/libloading ${CMAKE_CURRENT_SOURCE_DIR}/libloading.patch)
rust_export_crate(libloading-0.9 ${CMAKE_CURRENT_SOURCE_DIR}/libloading-0.9 ${CMAKE_CURRENT_SOURCE_DIR}/libloading-0.9.patch)
rust_export_crate(memmap2 ${CMAKE_CURRENT_SOURCE_DIR}/memmap2 ${CMAKE_CURRENT_SOURCE_DIR}/memmap2.patch)
rust_export_crate(getrandom ${CMAKE_CURRENT_SOURCE_DIR}/getrandom ${CMAKE_CURRENT_SOURCE_DIR}/getrandom.patch)
rust_export_crate(getrandom-0.2 ${CMAKE_CURRENT_SOURCE_DIR}/getrandom-0.2 ${CMAKE_CURRENT_SOURCE_DIR}/getrandom-0.2.patch)
rust_export_crate(stacker ${CMAKE_CURRENT_SOURCE_DIR}/stacker ${CMAKE_CURRENT_SOURCE_DIR}/stacker.patch)
rust_export_crate(filetime ${CMAKE_CURRENT_SOURCE_DIR}/filetime ${CMAKE_CURRENT_SOURCE_DIR}/filetime.patch)
rust_export_crate(socket2 ${CMAKE_CURRENT_SOURCE_DIR}/socket2 ${CMAKE_CURRENT_SOURCE_DIR}/socket2.patch)

set(rust_patched_trees
    repo:moxnox.patch
    repo/src/tools/cargo:cargo.patch
    libc:libc.patch
    rustix:rustix.patch
    nix:nix.patch
    errno:errno.patch
    jobserver:jobserver.patch
    termize:termize.patch
    rand:rand.patch
    libloading:libloading.patch
    libloading-0.9:libloading-0.9.patch
    memmap2:memmap2.patch
    getrandom:getrandom.patch
    getrandom-0.2:getrandom-0.2.patch
    stacker:stacker.patch
    filetime:filetime.patch
    socket2:socket2.patch
)

set(rust_apply_commands)
set(rust_revert_commands)
set(rust_patch_files)

foreach(entry ${rust_patched_trees})
    string(REPLACE ":" ";" entry ${entry})
    list(GET entry 0 dir)
    list(GET entry 1 patch)
    set(dir ${CMAKE_CURRENT_SOURCE_DIR}/${dir})
    set(patch ${CMAKE_CURRENT_SOURCE_DIR}/${patch})

    list(APPEND rust_apply_commands COMMAND sh -c "cd '${dir}' && ( git apply --check -R '${patch}' 2>/dev/null || ( ! git status --porcelain | grep -q . && git apply '${patch}' ) || ( echo '${dir}: cannot apply ${patch}, tree status:' >&2 && git status --porcelain >&2 && false ) )")
    list(APPEND rust_revert_commands COMMAND sh -c "cd '${dir}' && git apply -R '${patch}'")
    list(APPEND rust_patch_files ${patch})
endforeach()

set(RUST_PKGCONFIG ${CMAKE_CURRENT_BINARY_DIR}/pkgconfig)
foreach(pc libcurl libgit2 libssh2 libllhttp libnghttp2 openssl libssl libcrypto libpcre2-8 sqlite3 zlib libzstd)
    configure_file(pkgconfig/${pc}.pc.in ${RUST_PKGCONFIG}/${pc}.pc @ONLY)
endforeach()

string(REPLACE "-" "_" RUST_TARGET_UNDERSCORED ${RUST_TARGET})

add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/rust.stamp
    BYPRODUCTS ${rustc_main}
    ${rust_apply_commands}
    COMMAND ${CMAKE_COMMAND} -E env
        PKG_CONFIG_LIBDIR_${RUST_TARGET_UNDERSCORED}=${RUST_PKGCONFIG}
        PKG_CONFIG_ALLOW_CROSS_${RUST_TARGET_UNDERSCORED}=1
        LIBGIT2_SYS_USE_PKG_CONFIG=1
        LIBSSH2_SYS_USE_PKG_CONFIG=1
        ${RUST_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/x.py build
        --stage 1
        --config ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.toml
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${RUSTC_BUILD_DIR}/${RUST_BUILD_TRIPLE}/stage1/lib/rustlib/${RUST_TARGET}/lib
        ${RUSTC_STAGE1}/lib/rustlib/${RUST_TARGET}/lib
    ${rust_revert_commands}
    COMMAND ${CMAKE_COMMAND} -E touch ${CMAKE_CURRENT_BINARY_DIR}/rust.stamp
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/bootstrap.toml ${rust_patch_files}
        $<TARGET_FILE:libcurl_shared>
        $<TARGET_FILE:libgit2package>
        $<TARGET_FILE:libssh2_shared>
        $<TARGET_FILE:llhttp>
        $<TARGET_FILE:nghttp2>
        $<TARGET_FILE:libssl>
        $<TARGET_FILE:libcrypto>
        $<TARGET_FILE:pcre2-8-shared>
        $<TARGET_FILE:sqlite3>
        $<TARGET_FILE:zlib>
        $<TARGET_FILE:zstd>
    JOB_POOL cargo_pool
    COMMENT "Building rustc (${RUST_TARGET})"
    VERBATIM
)

add_custom_target(rust DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/rust.stamp)

add_dependencies(rust moxnox ucrtbase mingwex mingw32)

install(PROGRAMS ${RUSTC_STAGE1}/bin/rustc.exe ${RUSTC_STAGE1}/bin/rustdoc.exe
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin OPTIONAL)

install(PROGRAMS ${RUSTC_BUILD_DIR}/${RUST_BUILD_TRIPLE}/stage2-tools-bin/cargo.exe
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin OPTIONAL)

install(DIRECTORY ${RUSTC_STAGE1}/lib/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    USE_SOURCE_PERMISSIONS OPTIONAL
    PATTERN rustlib EXCLUDE
)
install(CODE "
    file(GLOB _rustlib_contents \"${RUSTC_STAGE1}/lib/rustlib/${RUST_TARGET}/lib/*\")
    if(_rustlib_contents)
        file(REMOVE_RECURSE \"${CMAKE_INSTALL_PREFIX}/lib/rustlib/${RUST_TARGET}\")
    endif()")
install(DIRECTORY ${RUSTC_STAGE1}/lib/rustlib/${RUST_TARGET}
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/rustlib
    USE_SOURCE_PERMISSIONS OPTIONAL
)
