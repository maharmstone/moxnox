cmake_minimum_required(VERSION 3.28)

project(moxnox)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_language(ASM)

configure_file(src/font.S.in ${CMAKE_CURRENT_BINARY_DIR}/font.S)

set(GLOBAL_MODULES src/cmdline.cpp
    src/mem.cpp
    src/config.cpp
    src/thread.cpp
    src/object_ref.cpp
    src/objects.cpp
    src/visit.cpp
    src/print.cpp
    src/console.cpp
    src/bsod.cpp
    src/acpi.cpp
    src/module_init.cpp
    src/pipe.cpp
    src/event.cpp
    src/chardev.cpp
    src/interrupts.cpp
    src/timer.cpp
    src/disable_interrupts.cpp
    src/scheduler.cpp
    src/null.cpp
    src/semaphore.cpp
    src/rwsem.cpp
    src/rangelock.cpp
    src/volume.cpp
    src/guid.cpp
    src/page_cache.cpp
    src/vfs.cpp
    src/cpio.cpp
    src/section.cpp
    src/image.cpp
    src/evdev_device.cpp
    src/process.cpp
    src/power.cpp
    src/smp.cpp
    src/kernel_images.cpp
    src/cpu_block.cpp
    src/spinlock.cpp
    src/pci.cpp
    src/crc32c.cpp
    src/crc32.cpp
    src/xhci.cpp
    src/usb.cpp
    src/virtio.cpp
    src/efi.cpp
    src/framebuffer.cpp
    src/handle.cpp
    src/mapping.cpp
    src/debug.cpp
    src/version.cpp
    src/kernel_time_page.cpp
    src/dma.cpp
    src/mapping_type.cpp
    src/net.cpp
    src/unwind.cpp
    src/smbios_parser.cpp
    src/pty.cpp
    src/tty.cpp
    src/trace.cpp
    src/vec.cpp
    src/urandom.cpp
    src/chacha20.cpp
    src/fifo.cpp
)

set(AMD64_MODULES ${GLOBAL_MODULES}
    src/x86/inout.cpp
    src/x86/apic.cpp
    src/x86/text_console.cpp
    src/x86/rtc.cpp
)

set(AARCH64_MODULES ${GLOBAL_MODULES}
    src/aarch64/psci.cpp
)

set(SRC_FILES src/kernel.cpp
    src/bsod2.cpp
    src/visit2.cpp
    src/thread2.cpp
    src/process2.cpp
    src/section2.cpp
    src/mapping2.cpp
    src/handle2.cpp
    src/spinlock2.cpp
    src/rangelock2.cpp
    src/scheduler2.cpp
    src/vfs2.cpp
    src/syscalls.cpp
    src/rwsem2.cpp
    src/virtio_block.cpp
    src/virtio_console.cpp
    src/virtio_gpu.cpp
    src/virtio_input.cpp
    src/sata.cpp
    src/btrfs.cpp
    src/nvme.cpp
    src/usbhub.cpp
    src/usbhid.cpp
    src/sdhci.cpp
    src/usbms.cpp
    src/devfs.cpp
    src/tmpfs.cpp
    src/virtio_net.cpp
    src/net2.cpp
    src/framebuffer2.cpp
    src/interrupts2.cpp
    src/mem2.cpp
    src/tcp.cpp
    src/udp.cpp
    src/rtl8168.cpp
    src/xhci2.cpp
    src/acpi2.cpp
    src/pci2.cpp
    src/object_ref2.cpp
    src/timer2.cpp
    src/console2.cpp
    src/page_cache2.cpp
    src/volume2.cpp
    src/sysinfo.cpp
    src/trace2.cpp
    src/afunix.cpp
    src/dhcp.cpp
    src/pipe2.cpp
    src/event2.cpp
    src/fifo2.cpp
)

set(AMD64_SRC_FILES ${SRC_FILES}
    src/x86/ps2.cpp
    src/x86/serial.cpp
    src/x86/framebuffer_avx.cpp
    src/x86/framebuffer_avx512.cpp
)

set(AARCH64_SRC_FILES ${SRC_FILES}
    src/aarch64/pl011.cpp
)

set(CMAKE_CXX_LINK_EXECUTABLE "<CMAKE_CXX_COMPILER> <FLAGS> <CMAKE_CXX_LINK_FLAGS> <LINK_FLAGS> <OBJECTS> -o <TARGET> <LINK_LIBRARIES>")

set(kernel_libgcc_query ${CMAKE_CXX_COMPILER})

if(CMAKE_CXX_COMPILER_TARGET)
    list(APPEND kernel_libgcc_query --target=${CMAKE_CXX_COMPILER_TARGET})
elseif(USE_COMPILER_RT)
    list(APPEND kernel_libgcc_query --rtlib=compiler-rt)
endif()

execute_process(COMMAND ${kernel_libgcc_query} --print-libgcc-file-name
    OUTPUT_VARIABLE KERNEL_LIBGCC
    OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY
)

if(NOT EXISTS ${KERNEL_LIBGCC})
    message(FATAL_ERROR "compiler runtime library ${KERNEL_LIBGCC} not found")
endif()

if(CMAKE_SYSTEM_PROCESSOR STREQUAL aarch64)
    set(SRC_ASM_FILES src/aarch64/ints.S
        src/aarch64/timer.S
        src/aarch64/smp.S
        src/aarch64/framebuffer.S
        ${CMAKE_CURRENT_BINARY_DIR}/font.S)

    add_executable(kernel ${AARCH64_SRC_FILES} ${SRC_ASM_FILES})
    target_sources(kernel PUBLIC FILE_SET CXX_MODULES FILES ${AARCH64_MODULES})
    target_link_libraries(kernel aarch64)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(SRC_ASM_FILES src/x86/ints.S
        src/x86/timer.S
        src/x86/syscalls.S
        src/x86/mem.S
        src/x86/smp.S
        src/x86/tlb.S
        src/x86/framebuffer.S
        ${CMAKE_CURRENT_BINARY_DIR}/font.S)

    add_executable(kernel ${AMD64_SRC_FILES} ${SRC_ASM_FILES})
    target_sources(kernel PUBLIC FILE_SET CXX_MODULES FILES ${AMD64_MODULES})
    target_link_libraries(kernel x86)

    set_source_files_properties(src/x86/framebuffer_avx.cpp PROPERTIES COMPILE_OPTIONS "-mavx2")
    set_source_files_properties(src/x86/framebuffer_avx512.cpp PROPERTIES COMPILE_OPTIONS "-mavx512f")
endif()

target_link_libraries(kernel format_kernel dwarf_kernel list_kernel unicode_kernel rbtree_kernel vt100_kernel cp437_kernel evdev_kernel nt_kernel ntdll_kernel pe_kernel loaderblock crt_kernel pagesize_kernel timepage_kernel moxnox_ioctls_kernel ws2_32_kernel ipv4_kernel ipv6_kernel smbios termios_kernel)

target_compile_options(kernel PRIVATE ${KERNEL_CFLAGS} -Wno-unused-const-variable)
# target_compile_options(kernel PRIVATE -ffile-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/= -ffile-prefix-map=${CMAKE_CURRENT_BINARY_DIR}=)
target_link_options(kernel PUBLIC -nostartfiles -nostdlib -Wl,--entry=startup -Wl,--image-base=0xfffff80000000000 -Wl,--file-alignment=0x1000)

target_compile_options(kernel PRIVATE $<$<CXX_COMPILER_ID:GNU>:-fmodules-ts>)
target_compile_options(kernel PRIVATE $<$<COMPILE_LANGUAGE:ASM>:-g0>)
target_compile_options(kernel PRIVATE $<$<CONFIG:DEBUG>:-fno-inline -fno-omit-frame-pointer>)
target_compile_options(kernel PRIVATE $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:Clang>>:-fno-auto-import>)

target_link_libraries(kernel acpica)
target_link_libraries(kernel freetype_kernel)
target_link_libraries(kernel ${KERNEL_LIBGCC})

target_compile_definitions(kernel PUBLIC "$<$<CONFIG:DEBUG>:DEBUG>")

add_custom_command(TARGET kernel POST_BUILD
    COMMAND sh -c "! \"${CMAKE_OBJDUMP}\" -p \"$1\" | grep -q 'DLL Name' || { echo 'Error: kernel has imports' >&2; exit 1; }" -- "$<TARGET_FILE:kernel>"
    VERBATIM
    COMMENT "Checking kernel has no imports"
)

install(TARGETS kernel DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    COMPONENT core
    CXX_MODULES_BMI EXCLUDE_FROM_ALL
)
