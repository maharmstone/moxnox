include_guard(GLOBAL)

set_property(GLOBAL APPEND PROPERTY JOB_POOLS cargo_pool=1)

set(RUST_CRATES_DIR ${CMAKE_BINARY_DIR}/ports/rust/crates CACHE INTERNAL "")

function(rust_export_tree target src dest patch)
    add_custom_command(
        OUTPUT ${dest}.stamp
        COMMAND sh -c "rm -rf '${dest}' && mkdir -p '${dest}' && git -C '${src}' archive HEAD | tar -x -C '${dest}' && patch -p1 -d '${dest}' < '${patch}'"
        COMMAND ${CMAKE_COMMAND} -E touch ${dest}.stamp
        DEPENDS ${patch}
        COMMENT "Exporting and patching ${src}"
        VERBATIM
    )

    add_custom_target(${target} DEPENDS ${dest}.stamp)
endfunction()

function(rust_export_crate name src patch)
    rust_export_tree(rust_crate_${name} ${src} ${RUST_CRATES_DIR}/${name} ${patch})
endfunction()

function(init_rust_crate_submodules)
    set(uninit "")

    foreach(path ${ARGN})
        if(NOT EXISTS ${CMAKE_SOURCE_DIR}/${path}/.git)
            list(APPEND uninit ${path})
        endif()
    endforeach()

    if(uninit)
        execute_process(
            COMMAND git -C ${CMAKE_SOURCE_DIR} submodule update --init -- ${uninit}
            RESULT_VARIABLE result
        )

        if(NOT result EQUAL 0)
            message(FATAL_ERROR "git submodule update --init failed for the Rust crate forks")
        endif()
    endif()
endfunction()

function(add_rust_port name binary)
    cmake_parse_arguments(ARG "" "PATCH" "PATCHED_CRATES;RUST_CRATES" ${ARGN})

    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(RUST_TARGET x86_64-pc-moxnox)
    elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
        set(RUST_TARGET aarch64-pc-moxnox)
    else()
        message(FATAL_ERROR "no Rust target for ${CMAKE_SYSTEM_PROCESSOR}")
    endif()

    file(RELATIVE_PATH port_rel ${CMAKE_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR})

    set(crate_paths ports/rust/libc)
    foreach(crate ${ARG_PATCHED_CRATES})
        list(APPEND crate_paths ${port_rel}/${crate})
    endforeach()

    init_rust_crate_submodules(${crate_paths})

    set(cargo_args --config
        "patch.crates-io.libc.path=\"${RUST_CRATES_DIR}/libc\"")
    foreach(crate ${ARG_PATCHED_CRATES} ${ARG_RUST_CRATES})
        list(APPEND cargo_args --config
            "patch.crates-io.${crate}.path=\"${RUST_CRATES_DIR}/${crate}\"")
    endforeach()

    if(ARG_PATCH)
        set(manifest_dir ${CMAKE_CURRENT_BINARY_DIR}/gen/repo)
        rust_export_tree(${name}_patched ${CMAKE_CURRENT_SOURCE_DIR}/repo ${manifest_dir}
            ${CMAKE_CURRENT_SOURCE_DIR}/${ARG_PATCH})
    else()
        set(manifest_dir ${CMAKE_CURRENT_SOURCE_DIR}/repo)
    endif()

    set(cargo_target_dir ${CMAKE_CURRENT_BINARY_DIR}/cargo)
    set(exe ${cargo_target_dir}/${RUST_TARGET}/release/${binary})
    get_filename_component(base ${binary} NAME_WE)

    add_custom_command(
        OUTPUT ${exe}
        COMMAND ${CMAKE_COMMAND} -E env CARGO_TARGET_DIR=${cargo_target_dir}
            cargo build --release --target ${RUST_TARGET} ${cargo_args}
            --manifest-path ${manifest_dir}/Cargo.toml
        DEPFILE ${cargo_target_dir}/${RUST_TARGET}/release/${base}.d
        JOB_POOL cargo_pool
        COMMENT "Building Rust port ${name} (${RUST_TARGET})"
        VERBATIM
    )

    # FIXME - make it so rust ports don't have to be specified by name in order
    #         to be built, once cargo is ported
    add_custom_target(${name} DEPENDS ${exe})

    add_dependencies(${name} rust_crate_libc)
    foreach(crate ${ARG_PATCHED_CRATES} ${ARG_RUST_CRATES})
        add_dependencies(${name} rust_crate_${crate})
    endforeach()
    if(ARG_PATCH)
        add_dependencies(${name} ${name}_patched)
    endif()

    install(PROGRAMS ${exe} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin OPTIONAL)
endfunction()
