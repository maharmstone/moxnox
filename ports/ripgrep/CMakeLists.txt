include(${CMAKE_SOURCE_DIR}/rust-port.cmake)

add_rust_port(ripgrep rg.exe PATCH moxnox.patch RUST_CRATES memmap2)
