set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(PATCHED ${CMAKE_CURRENT_BINARY_DIR}/patched)

foreach(py_candidate python3 python3.14 python3.13 python3.12)
    execute_process(COMMAND ${py_candidate} -c "import mako"
        RESULT_VARIABLE MESA_PYTHON_RESULT OUTPUT_QUIET ERROR_QUIET)
    if(MESA_PYTHON_RESULT EQUAL 0)
        set(MESA_PYTHON ${py_candidate})
        break()
    endif()
endforeach()
if(NOT MESA_PYTHON)
    message(FATAL_ERROR "mesa: no python interpreter with the mako module found")
endif()

file(MAKE_DIRECTORY
    ${GEN}/src
    ${GEN}/src/util
    ${GEN}/src/util/format
    ${GEN}/src/compiler
    ${GEN}/src/compiler/nir
    ${GEN}/src/compiler/glsl
    ${GEN}/src/compiler/glsl/glcpp
    ${GEN}/src/compiler/spirv
    ${GEN}/src/mesa
    ${GEN}/src/mesa/program
    ${GEN}/src/mesa/glapi/glapi/gen
    ${GEN}/src/mesa/glapi/shared-glapi
    ${GEN}/src/gallium/auxiliary
)

# ---------------------------------------------------------------------------

function(mesa_patch var old new)
    string(FIND "${${var}}" "${old}" _pos)
    if(_pos EQUAL -1)
        message(FATAL_ERROR "mesa patch anchor not found: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" _out "${${var}}")
    set(${var} "${_out}" PARENT_SCOPE)
endfunction()

function(mesa_read var path)
    file(READ ${path} _out)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${path})
    set(${var} "${_out}" PARENT_SCOPE)
endfunction()

mesa_read(_src repo/src/util/detect_os.h)
mesa_patch(_src [=[#if defined(__managarm__)
#define DETECT_OS_MANAGARM 1
#define DETECT_OS_POSIX 1
#endif


/*]=] [=[#if defined(__managarm__)
#define DETECT_OS_MANAGARM 1
#define DETECT_OS_POSIX 1
#endif

#if defined(__moxnox__)
#define DETECT_OS_MOXNOX 1
#define DETECT_OS_POSIX 1
#endif


/*]=])
mesa_patch(_src [=[#ifndef DETECT_OS_MANAGARM
#define DETECT_OS_MANAGARM 0
#endif
]=] [=[#ifndef DETECT_OS_MANAGARM
#define DETECT_OS_MANAGARM 0
#endif
#ifndef DETECT_OS_MOXNOX
#define DETECT_OS_MOXNOX 0
#endif
]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/util/detect_os.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/include/EGL/eglplatform.h)
mesa_patch(_src "#elif defined(__unix__)"
                "#elif defined(__unix__) || defined(__moxnox__)")
file(CONFIGURE OUTPUT ${PATCHED}/include/EGL/eglplatform.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/include/KHR/khrplatform.h)
mesa_patch(_src "#elif defined(_WIN64)"
                "#elif defined(_WIN64) || defined(__moxnox__)")
mesa_patch(_src "#if defined(_WIN64)"
                "#if defined(_WIN64) || defined(__moxnox__)")
file(CONFIGURE OUTPUT ${PATCHED}/include/KHR/khrplatform.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/include/drm-uapi/drm.h)
mesa_patch(_src [=[#else /* One of the BSDs */]=] [=[#elif defined(__moxnox__)

/* Moxnox has no DRM; these definitions exist only so that this header
 * compiles. The ioctl encoding macros follow the BSD layout. */
#include <stdint.h>
#include <sys/types.h>
typedef int8_t   __s8;
typedef uint8_t  __u8;
typedef int16_t  __s16;
typedef uint16_t __u16;
typedef int32_t  __s32;
typedef uint32_t __u32;
typedef int64_t  __s64;
typedef uint64_t __u64;
typedef size_t   __kernel_size_t;
typedef unsigned long drm_handle_t;

#define _IOC(inout, group, num, len) \
    (((unsigned long)(inout)) | (((unsigned long)(len) & 0x1fff) << 16) | \
     ((unsigned long)(group) << 8) | (unsigned long)(num))
#define _IOC_VOID 0x20000000UL
#define _IOC_OUT 0x40000000UL
#define _IOC_IN 0x80000000UL
#define _IO(g, n) _IOC(_IOC_VOID, (g), (n), 0)
#define _IOR(g, n, t) _IOC(_IOC_OUT, (g), (n), sizeof(t))
#define _IOW(g, n, t) _IOC(_IOC_IN, (g), (n), sizeof(t))
#define _IOWR(g, n, t) _IOC(_IOC_IN | _IOC_OUT, (g), (n), sizeof(t))

#else /* One of the BSDs */]=])
file(CONFIGURE OUTPUT ${PATCHED}/include/drm-uapi/drm.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/gallium/auxiliary/rtasm/rtasm_x86sse.h)
mesa_patch(_src [=[#elif (DETECT_OS_CYGWIN || DETECT_OS_WINDOWS) && DETECT_ARCH_X86_64
   return X86_64_WIN64_ABI;]=] [=[#elif (DETECT_OS_CYGWIN || DETECT_OS_WINDOWS || DETECT_OS_MOXNOX) && DETECT_ARCH_X86_64
   /* Moxnox uses the Microsoft calling convention */
   return X86_64_WIN64_ABI;]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/gallium/auxiliary/rtasm/rtasm_x86sse.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/gallium/include/mesa_interface.h)
mesa_patch(_src [=[typedef void
(*__DRIblobCacheSet) (const void *key, signed long keySize,
                      const void *value, signed long valueSize);

typedef signed long
(*__DRIblobCacheGet) (const void *key, signed long keySize,
                      void *value, signed long valueSize);]=] [=[/* These must match EGLSetBlobFuncANDROID/EGLGetBlobFuncANDROID, whose
 * EGLsizeiANDROID parameters are khronos_ssize_t: long long on LLP64
 * platforms such as Moxnox, long everywhere else. */
#if defined(__moxnox__)
typedef void
(*__DRIblobCacheSet) (const void *key, signed long long keySize,
                      const void *value, signed long long valueSize);

typedef signed long long
(*__DRIblobCacheGet) (const void *key, signed long long keySize,
                      void *value, signed long long valueSize);
#else
typedef void
(*__DRIblobCacheSet) (const void *key, signed long keySize,
                      const void *value, signed long valueSize);

typedef signed long
(*__DRIblobCacheGet) (const void *key, signed long keySize,
                      void *value, signed long valueSize);
#endif]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/gallium/include/mesa_interface.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/util/disk_cache.h)
mesa_patch(_src [=[typedef void
(*disk_cache_put_cb) (const void *key, signed long keySize,
                      const void *value, signed long valueSize);

typedef signed long
(*disk_cache_get_cb) (const void *key, signed long keySize,
                      void *value, signed long valueSize);]=] [=[/* These must match __DRIblobCacheSet/__DRIblobCacheGet in mesa_interface.h,
 * whose sizes are long long on LLP64 platforms such as Moxnox. */
#if defined(__moxnox__)
typedef void
(*disk_cache_put_cb) (const void *key, signed long long keySize,
                      const void *value, signed long long valueSize);

typedef signed long long
(*disk_cache_get_cb) (const void *key, signed long long keySize,
                      void *value, signed long long valueSize);
#else
typedef void
(*disk_cache_put_cb) (const void *key, signed long keySize,
                      const void *value, signed long valueSize);

typedef signed long
(*disk_cache_get_cb) (const void *key, signed long keySize,
                      void *value, signed long valueSize);
#endif]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/util/disk_cache.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/mesa/glapi/glapi/glapi.h)
mesa_patch(_src [=[#if DETECT_OS_WINDOWS && !defined(MAPI_MODE_SHARED_GLAPI)]=]
[=[/* On Moxnox the TLS dispatch pointer cannot be referenced across DLLs, so
 * anything outside libgallium (which defines MESA_IN_LIBGALLIUM) must go
 * through the accessor functions instead. */
#if (DETECT_OS_WINDOWS || (DETECT_OS_MOXNOX && !defined(MESA_IN_LIBGALLIUM))) && !defined(MAPI_MODE_SHARED_GLAPI)]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/mesa/glapi/glapi/glapi.h CONTENT "${_src}" @ONLY)

# Moxnox has no fence fds
mesa_read(_src repo/src/util/libsync.h)
mesa_patch(_src [=[#include <sys/ioctl.h>
]=] [=[#if !defined(__moxnox__)
#include <sys/ioctl.h>
#endif
]=])
mesa_patch(_src [=[static inline int sync_merge(const char *name, int fd1, int fd2)
{
	struct sync_merge_data data = {{0}};]=] [=[#if defined(__moxnox__)

/* Moxnox has no fence fds; these stubs exist only so that callers compile. */
static inline int sync_merge(const char *name, int fd1, int fd2)
{
	(void)name;
	(void)fd1;
	(void)fd2;
	errno = ENOSYS;
	return -1;
}

static inline bool
sync_valid_fd(int fd)
{
	(void)fd;
	return false;
}

static inline struct sync_file_info* sync_file_info(int32_t fd)
{
	(void)fd;
	return NULL;
}

#else

static inline int sync_merge(const char *name, int fd1, int fd2)
{
	struct sync_merge_data data = {{0}};]=])
mesa_patch(_src [=[    return info;
}

#endif /* DETECT_OS_ANDROID */]=] [=[    return info;
}

#endif /* __moxnox__ */

#endif /* DETECT_OS_ANDROID */]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/util/libsync.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/util/u_thread.h)
mesa_patch(_src "#if defined(HAVE_PTHREAD) && !defined(__APPLE__) && !defined(__HAIKU__)"
                "#if defined(HAVE_PTHREAD) && !defined(__APPLE__) && !defined(__HAIKU__) && !defined(__moxnox__)")
file(CONFIGURE OUTPUT ${PATCHED}/src/util/u_thread.h CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/util/u_thread.c)
mesa_patch(_src "#if defined(HAVE_PTHREAD) && !defined(__APPLE__) && !defined(__HAIKU__) && !defined(__managarm__)"
                "#if defined(HAVE_PTHREAD) && !defined(__APPLE__) && !defined(__HAIKU__) && !defined(__managarm__) && !defined(__moxnox__)")
mesa_patch(_src [=[#if defined(HAVE_PTHREAD) && !defined(__APPLE__) && !defined(__HAIKU__)

void util_barrier_init]=] [=[#if defined(HAVE_PTHREAD) && !defined(__APPLE__) && !defined(__HAIKU__) && !defined(__moxnox__)

void util_barrier_init]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/util/u_thread.c CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/gallium/auxiliary/driver_ddebug/dd_draw.c)
mesa_patch(_src [=[#if DETECT_OS_POSIX
   sync();]=] [=[#if DETECT_OS_POSIX && !DETECT_OS_MOXNOX
   sync();]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/gallium/auxiliary/driver_ddebug/dd_draw.c CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/gallium/auxiliary/gallivm/lp_bld_init_orc.cpp)
mesa_patch(_src [=[#if DETECT_ARCH_RISCV64 == 1 || DETECT_ARCH_RISCV32 == 1 || DETECT_ARCH_LOONGARCH64 == 1 || (defined(_WIN32) && LLVM_VERSION_MAJOR >= 15)]=]
[=[/* x86_64 Moxnox uses JITLINK too: RuntimeDyldCOFFX86_64's registerEHFrames
 * misparses COFF .pdata as DWARF .eh_frame when the host is not _WIN32.
 * aarch64 stays on RuntimeDyld: JITLink has no COFF/aarch64 backend, and
 * RuntimeDyldCOFFAArch64::registerEHFrames is a no-op so does not hit the
 * same bug */
#if DETECT_ARCH_RISCV64 == 1 || DETECT_ARCH_RISCV32 == 1 || DETECT_ARCH_LOONGARCH64 == 1 || (defined(_WIN32) && LLVM_VERSION_MAJOR >= 15) || (defined(__moxnox__) && defined(__x86_64__))]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/gallium/auxiliary/gallivm/lp_bld_init_orc.cpp CONTENT "${_src}" @ONLY)

# no sys/param.h on Moxnox
mesa_read(_src repo/src/loader/loader.c)
mesa_patch(_src [=[#include <sys/param.h>]=] [=[#if __has_include(<sys/param.h>)
#include <sys/param.h>
#endif]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/loader/loader.c CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/util/log.c)
mesa_patch(_src [=[         snprintf(log_file, sizeof(log_file), "/tmp/mesa_%s_%d_XXXXXX.log", util_get_process_name(),
                  getpid());
         fd = mkstemps(log_file, 4);]=] [=[#if DETECT_OS_MOXNOX
         /* mkstemps is not POSIX */
         snprintf(log_file, sizeof(log_file), "/tmp/mesa_%s_%d_XXXXXX", util_get_process_name(),
                  getpid());
         fd = mkstemp(log_file);
#else
         snprintf(log_file, sizeof(log_file), "/tmp/mesa_%s_%d_XXXXXX.log", util_get_process_name(),
                  getpid());
         fd = mkstemps(log_file, 4);
#endif]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/util/log.c CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/util/os_misc.c)
mesa_patch(_src "#elif DETECT_OS_LINUX || DETECT_OS_CYGWIN || DETECT_OS_SOLARIS || DETECT_OS_HURD || DETECT_OS_MANAGARM"
                "#elif DETECT_OS_LINUX || DETECT_OS_CYGWIN || DETECT_OS_SOLARIS || DETECT_OS_HURD || DETECT_OS_MANAGARM || DETECT_OS_MOXNOX")
file(CONFIGURE OUTPUT ${PATCHED}/src/util/os_misc.c CONTENT "${_src}" @ONLY)

mesa_read(_src repo/src/util/os_time.c)
mesa_patch(_src [=[#elif DETECT_OS_POSIX
   usleep(usecs);]=] [=[#elif DETECT_OS_MOXNOX
   struct timespec time;
   time.tv_sec = usecs / 1000000;
   time.tv_nsec = (usecs % 1000000) * 1000;
   while (nanosleep(&time, &time) == -1 && errno == EINTR);

#elif DETECT_OS_POSIX
   usleep(usecs);]=])
file(CONFIGURE OUTPUT ${PATCHED}/src/util/os_time.c CONTENT "${_src}" @ONLY)

configure_file(repo/src/gallium/auxiliary/rtasm/rtasm_x86sse.c
               ${PATCHED}/src/gallium/auxiliary/rtasm/rtasm_x86sse.c COPYONLY)
configure_file(repo/src/util/disk_cache.c ${PATCHED}/src/util/disk_cache.c COPYONLY)
configure_file(repo/src/util/thread_sched.c ${PATCHED}/src/util/thread_sched.c COPYONLY)
configure_file(repo/src/util/thread_sched.h ${PATCHED}/src/util/thread_sched.h COPYONLY)
configure_file(repo/src/util/os_file_notify.c ${PATCHED}/src/util/os_file_notify.c COPYONLY)

foreach(drm_header
    amdgpu_drm.h
    dma-buf.h
    drm_fourcc.h
    drm_mode.h
    i915_drm.h
    nouveau_drm.h
    radeon_drm.h
    vc4_drm.h
    virtgpu_drm.h
)
    configure_file(repo/include/drm-uapi/${drm_header}
                   ${PATCHED}/include/drm-uapi/${drm_header} COPYONLY)
endforeach()

# ---------------------------------------------------------------------------

set(NIR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/nir)
set(NIR_DEPS ${NIR_DIR}/nir_opcodes.py ${NIR_DIR}/nir_intrinsics.py)

add_custom_command(OUTPUT ${GEN}/src/compiler/nir/nir_builder_opcodes.h
    COMMAND sh -c "${MESA_PYTHON} '${NIR_DIR}/nir_builder_opcodes_h.py' > '${GEN}/src/compiler/nir/nir_builder_opcodes.h'"
    DEPENDS ${NIR_DIR}/nir_builder_opcodes_h.py ${NIR_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/nir/nir_constant_expressions.c
    COMMAND sh -c "${MESA_PYTHON} '${NIR_DIR}/nir_constant_expressions.py' > '${GEN}/src/compiler/nir/nir_constant_expressions.c'"
    DEPENDS ${NIR_DIR}/nir_constant_expressions.py ${NIR_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/nir/nir_opcodes.h
    COMMAND sh -c "${MESA_PYTHON} '${NIR_DIR}/nir_opcodes_h.py' > '${GEN}/src/compiler/nir/nir_opcodes.h'"
    DEPENDS ${NIR_DIR}/nir_opcodes_h.py ${NIR_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/nir/nir_opcodes.c
    COMMAND sh -c "${MESA_PYTHON} '${NIR_DIR}/nir_opcodes_c.py' > '${GEN}/src/compiler/nir/nir_opcodes.c'"
    DEPENDS ${NIR_DIR}/nir_opcodes_c.py ${NIR_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/nir/nir_opt_algebraic.c
    COMMAND ${MESA_PYTHON} ${NIR_DIR}/nir_opt_algebraic.py --out ${GEN}/src/compiler/nir/nir_opt_algebraic.c
    DEPENDS ${NIR_DIR}/nir_opt_algebraic.py ${NIR_DIR}/nir_algebraic.py ${NIR_DIR}/nir_opcodes.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/nir/nir_intrinsics.h
    COMMAND ${MESA_PYTHON} ${NIR_DIR}/nir_intrinsics_h.py --out ${GEN}/src/compiler/nir/nir_intrinsics.h
    DEPENDS ${NIR_DIR}/nir_intrinsics_h.py ${NIR_DIR}/nir_intrinsics.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/nir/nir_intrinsics_indices.h
    COMMAND ${MESA_PYTHON} ${NIR_DIR}/nir_intrinsics_indices_h.py --out ${GEN}/src/compiler/nir/nir_intrinsics_indices.h
    DEPENDS ${NIR_DIR}/nir_intrinsics_indices_h.py ${NIR_DIR}/nir_intrinsics.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/nir/nir_intrinsics.c
    COMMAND ${MESA_PYTHON} ${NIR_DIR}/nir_intrinsics_c.py --out ${GEN}/src/compiler/nir/nir_intrinsics.c
    DEPENDS ${NIR_DIR}/nir_intrinsics_c.py ${NIR_DIR}/nir_intrinsics.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/util/format/u_format_gen.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/format/u_format_table.py' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/format/u_format.yaml' --enums > '${GEN}/src/util/format/u_format_gen.h'"
    DEPENDS repo/src/util/format/u_format_table.py repo/src/util/format/u_format.yaml
            repo/src/util/format/u_format_pack.py repo/src/util/format/u_format_parse.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/util/format/u_format_pack.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/format/u_format_table.py' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/format/u_format.yaml' --header > '${GEN}/src/util/format/u_format_pack.h'"
    DEPENDS repo/src/util/format/u_format_table.py repo/src/util/format/u_format.yaml
            repo/src/util/format/u_format_pack.py repo/src/util/format/u_format_parse.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/util/format/u_format_table.c
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/format/u_format_table.py' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/format/u_format.yaml' > '${GEN}/src/util/format/u_format_table.c'"
    DEPENDS repo/src/util/format/u_format_table.py repo/src/util/format/u_format.yaml
            repo/src/util/format/u_format_pack.py repo/src/util/format/u_format_parse.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/util/format_srgb.c
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/format_srgb.py' > '${GEN}/src/util/format_srgb.c'"
    DEPENDS repo/src/util/format_srgb.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/util/shader_stats.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/process_shader_stats.py' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/shader_stats.rnc' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/shader_stats.xml' > '${GEN}/src/util/shader_stats.h'"
    DEPENDS repo/src/util/process_shader_stats.py repo/src/util/shader_stats.rnc repo/src/util/shader_stats.xml VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/util/driconf_static.h
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/driconf_static.py ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/00-mesa-defaults.conf ${GEN}/src/util/driconf_static.h
    DEPENDS repo/src/util/driconf_static.py repo/src/util/00-mesa-defaults.conf VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/git_sha1.h
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/bin/git_sha1_gen.py --output ${GEN}/src/git_sha1.h
    DEPENDS repo/bin/git_sha1_gen.py VERBATIM)

# builtin_types.h includes u_format_gen.h, hence the extra dependency
add_custom_command(OUTPUT ${GEN}/src/compiler/builtin_types.h
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/builtin_types_h.py ${GEN}/src/compiler/builtin_types.h
    DEPENDS repo/src/compiler/builtin_types_h.py repo/src/compiler/builtin_types.py
            ${GEN}/src/util/format/u_format_gen.h VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/builtin_types.c
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/builtin_types_c.py ${GEN}/src/compiler/builtin_types.c
    DEPENDS repo/src/compiler/builtin_types_c.py repo/src/compiler/builtin_types.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/ir_expression_operation.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/glsl/ir_expression_operation.py' enum > '${GEN}/src/compiler/ir_expression_operation.h'"
    DEPENDS repo/src/compiler/glsl/ir_expression_operation.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/glsl/ir_expression_operation_constant.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/glsl/ir_expression_operation.py' constant > '${GEN}/src/compiler/glsl/ir_expression_operation_constant.h'"
    DEPENDS repo/src/compiler/glsl/ir_expression_operation.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/glsl/ir_expression_operation_strings.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/glsl/ir_expression_operation.py' strings > '${GEN}/src/compiler/glsl/ir_expression_operation_strings.h'"
    DEPENDS repo/src/compiler/glsl/ir_expression_operation.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/glsl/glsl_parser.cpp ${GEN}/src/compiler/glsl/glsl_parser.h
    COMMAND ${HOST_BISON} -o ${GEN}/src/compiler/glsl/glsl_parser.cpp -p _mesa_glsl_
            --defines=${GEN}/src/compiler/glsl/glsl_parser.h ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/glsl/glsl_parser.yy
    DEPENDS repo/src/compiler/glsl/glsl_parser.yy VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/glsl/glsl_lexer.cpp
    COMMAND ${HOST_FLEX} -o ${GEN}/src/compiler/glsl/glsl_lexer.cpp ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/glsl/glsl_lexer.ll
    DEPENDS repo/src/compiler/glsl/glsl_lexer.ll VERBATIM)

foreach(xxd_gen IN ITEMS
        "float64_glsl.h float64.glsl float64_source"
        "cross_platform_settings_piece_all.h CrossPlatformSettings_piece_all.glsl cross_platform_settings_piece_all_header"
        "bc1_glsl.h bc1.glsl bc1_source"
        "bc4_glsl.h bc4.glsl bc4_source"
        "etc2_rgba_stitch_glsl.h etc2_rgba_stitch.glsl etc2_rgba_stitch_source"
        "astc_glsl.h astc_decoder.glsl astc_source")
    separate_arguments(xxd_args UNIX_COMMAND ${xxd_gen})
    list(GET xxd_args 0 xxd_out)
    list(GET xxd_args 1 xxd_in)
    list(GET xxd_args 2 xxd_name)
    add_custom_command(OUTPUT ${GEN}/src/compiler/glsl/${xxd_out}
        COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/xxd.py ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/glsl/${xxd_in}
                ${GEN}/src/compiler/glsl/${xxd_out} -n ${xxd_name}
        DEPENDS repo/src/util/xxd.py repo/src/compiler/glsl/${xxd_in} VERBATIM)
endforeach()

add_custom_command(OUTPUT ${GEN}/src/compiler/glsl/glcpp/glcpp-parse.c ${GEN}/src/compiler/glsl/glcpp/glcpp-parse.h
    COMMAND ${HOST_BISON} -o ${GEN}/src/compiler/glsl/glcpp/glcpp-parse.c -p glcpp_parser_
            --defines=${GEN}/src/compiler/glsl/glcpp/glcpp-parse.h ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/glsl/glcpp/glcpp-parse.y
    DEPENDS repo/src/compiler/glsl/glcpp/glcpp-parse.y VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/glsl/glcpp/glcpp-lex.c
    COMMAND ${HOST_FLEX} -o ${GEN}/src/compiler/glsl/glcpp/glcpp-lex.c ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/glsl/glcpp/glcpp-lex.l
    DEPENDS repo/src/compiler/glsl/glcpp/glcpp-lex.l VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/spirv/vtn_gather_types.c
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/spirv/vtn_gather_types_c.py
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/spirv/spirv.core.grammar.json ${GEN}/src/compiler/spirv/vtn_gather_types.c
    DEPENDS repo/src/compiler/spirv/vtn_gather_types_c.py repo/src/compiler/spirv/spirv.core.grammar.json VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/spirv/spirv_info.h ${GEN}/src/compiler/spirv/spirv_info.c
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/spirv/spirv_info_gen.py
            --json ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/spirv/spirv.core.grammar.json
            --out-h ${GEN}/src/compiler/spirv/spirv_info.h --out-c ${GEN}/src/compiler/spirv/spirv_info.c
    DEPENDS repo/src/compiler/spirv/spirv_info_gen.py repo/src/compiler/spirv/spirv.core.grammar.json VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/compiler/spirv/vtn_generator_ids.h
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/spirv/vtn_generator_ids_h.py
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/compiler/spirv/spir-v.xml ${GEN}/src/compiler/spirv/vtn_generator_ids.h
    DEPENDS repo/src/compiler/spirv/vtn_generator_ids_h.py repo/src/compiler/spirv/spir-v.xml VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/mesa/format_fallback.c
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/main/format_fallback.py ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/main/formats.csv ${GEN}/src/mesa/format_fallback.c
    DEPENDS repo/src/mesa/main/format_fallback.py repo/src/mesa/main/formats.csv repo/src/mesa/main/format_parser.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/mesa/format_info.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/main/format_info.py' '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/main/formats.csv' > '${GEN}/src/mesa/format_info.h'"
    DEPENDS repo/src/mesa/main/format_info.py repo/src/mesa/main/formats.csv repo/src/mesa/main/format_parser.py VERBATIM)

set(GLAPI_GEN ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/glapi/glapi/gen)
set(GLAPI_GEN_DEPS
    ${GLAPI_GEN}/gl_and_es_API.xml
    ${GLAPI_GEN}/gl_API.xml
    ${GLAPI_GEN}/gl_XML.py
    ${GLAPI_GEN}/glX_XML.py
    ${GLAPI_GEN}/license.py
    ${GLAPI_GEN}/marshal_XML.py
    ${GLAPI_GEN}/static_data.py
    ${GLAPI_GEN}/typeexpr.py
    ${GLAPI_GEN}/apiexec.py
)

add_custom_command(OUTPUT ${GEN}/src/mesa/get_hash.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/main/get_hash_generator.py' -f '${GLAPI_GEN}/gl_and_es_API.xml' > '${GEN}/src/mesa/get_hash.h'"
    DEPENDS repo/src/mesa/main/get_hash_generator.py repo/src/mesa/main/get_hash_params.py ${GLAPI_GEN_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/mesa/program/lex.yy.c
    COMMAND ${HOST_FLEX} -o ${GEN}/src/mesa/program/lex.yy.c ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/program/program_lexer.l
    DEPENDS repo/src/mesa/program/program_lexer.l VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/mesa/program/program_parse.tab.c ${GEN}/src/mesa/program/program_parse.tab.h
    COMMAND ${HOST_BISON} -o ${GEN}/src/mesa/program/program_parse.tab.c
            --defines=${GEN}/src/mesa/program/program_parse.tab.h ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/program/program_parse.y
    DEPENDS repo/src/mesa/program/program_parse.y VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/mesa/glapi/glapi/gen/enums.c
    COMMAND sh -c "${MESA_PYTHON} '${GLAPI_GEN}/gl_enums.py' -f '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/glapi/glapi/registry/gl.xml' > '${GEN}/src/mesa/glapi/glapi/gen/enums.c'"
    DEPENDS ${GLAPI_GEN}/gl_enums.py repo/src/mesa/glapi/glapi/registry/gl.xml VERBATIM)

foreach(apigen IN ITEMS
        "api_exec_init.c api_exec_init.py"
        "api_exec_decl.h api_exec_decl_h.py"
        "api_save_init.h api_save_init_h.py"
        "api_save.h api_save_h.py"
        "api_beginend_init.h api_beginend_init_h.py")
    separate_arguments(apigen_args UNIX_COMMAND ${apigen})
    list(GET apigen_args 0 apigen_out)
    list(GET apigen_args 1 apigen_script)
    add_custom_command(OUTPUT ${GEN}/src/mesa/glapi/glapi/gen/${apigen_out}
        COMMAND sh -c "${MESA_PYTHON} '${GLAPI_GEN}/${apigen_script}' -f '${GLAPI_GEN}/gl_and_es_API.xml' > '${GEN}/src/mesa/glapi/glapi/gen/${apigen_out}'"
        DEPENDS ${GLAPI_GEN}/${apigen_script} ${GLAPI_GEN_DEPS} VERBATIM)
endforeach()

add_custom_command(OUTPUT ${GEN}/src/mesa/glapi/glapi/gen/api_hw_select_init.h
    COMMAND sh -c "${MESA_PYTHON} '${GLAPI_GEN}/api_hw_select_init_h.py' -f '${GLAPI_GEN}/gl_API.xml' > '${GEN}/src/mesa/glapi/glapi/gen/api_hw_select_init.h'"
    DEPENDS ${GLAPI_GEN}/api_hw_select_init_h.py ${GLAPI_GEN_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/mesa/glapi/glapi/gen/dispatch.h
    COMMAND sh -c "${MESA_PYTHON} '${GLAPI_GEN}/gl_table.py' -f '${GLAPI_GEN}/gl_and_es_API.xml' -m dispatch > '${GEN}/src/mesa/glapi/glapi/gen/dispatch.h'"
    DEPENDS ${GLAPI_GEN}/gl_table.py ${GLAPI_GEN_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated.h
    COMMAND sh -c "${MESA_PYTHON} '${GLAPI_GEN}/marshal_generated_h.py' '${GLAPI_GEN}/gl_and_es_API.xml' 8 > '${GEN}/src/mesa/glapi/glapi/gen/marshal_generated.h'"
    DEPENDS ${GLAPI_GEN}/marshal_generated_h.py ${GLAPI_GEN_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/mesa/glapi/glapi/gen/unmarshal_table.c
    COMMAND sh -c "${MESA_PYTHON} '${GLAPI_GEN}/unmarshal_table_c.py' '${GLAPI_GEN}/gl_and_es_API.xml' 8 > '${GEN}/src/mesa/glapi/glapi/gen/unmarshal_table.c'"
    DEPENDS ${GLAPI_GEN}/unmarshal_table_c.py ${GLAPI_GEN_DEPS} VERBATIM)

foreach(marshal_idx RANGE 7)
    add_custom_command(OUTPUT ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated${marshal_idx}.c
        COMMAND sh -c "${MESA_PYTHON} '${GLAPI_GEN}/marshal_generated_c.py' '${GLAPI_GEN}/gl_and_es_API.xml' ${marshal_idx} 8 8 > '${GEN}/src/mesa/glapi/glapi/gen/marshal_generated${marshal_idx}.c'"
        DEPENDS ${GLAPI_GEN}/marshal_generated_c.py ${GLAPI_GEN_DEPS} VERBATIM)
endforeach()

add_custom_command(OUTPUT ${GEN}/src/mesa/glapi/shared-glapi/shared_glapi_mapi_tmp.h
    COMMAND sh -c "${MESA_PYTHON} '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/mesa/glapi/mapi_abi.py' --printer shared-glapi --gl_symbols '${CMAKE_CURRENT_SOURCE_DIR}/repo/src/glx/libgl-symbols.txt' '${GLAPI_GEN}/gl_and_es_API.xml' > '${GEN}/src/mesa/glapi/shared-glapi/shared_glapi_mapi_tmp.h'"
    DEPENDS repo/src/mesa/glapi/mapi_abi.py repo/src/glx/libgl-symbols.txt ${GLAPI_GEN_DEPS} VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/gallium/auxiliary/tr_util.c ${GEN}/src/gallium/auxiliary/tr_util.h
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/gallium/auxiliary/driver_trace/enums2names.py
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/gallium/include/pipe/p_defines.h ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/gallium/include/pipe/p_video_enums.h
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/blend.h
            -C ${GEN}/src/gallium/auxiliary/tr_util.c -H ${GEN}/src/gallium/auxiliary/tr_util.h
    DEPENDS repo/src/gallium/auxiliary/driver_trace/enums2names.py
            repo/src/gallium/include/pipe/p_defines.h repo/src/gallium/include/pipe/p_video_enums.h
            repo/src/util/blend.h VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/gallium/auxiliary/u_tracepoints.c
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/gallium/auxiliary/util/u_tracepoints.py -p ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/perf/
            -C ${GEN}/src/gallium/auxiliary/u_tracepoints.c
    DEPENDS repo/src/gallium/auxiliary/util/u_tracepoints.py repo/src/util/perf/u_trace.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/gallium/auxiliary/u_tracepoints.h
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/gallium/auxiliary/util/u_tracepoints.py -p ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/util/perf/
            -H ${GEN}/src/gallium/auxiliary/u_tracepoints.h
    DEPENDS repo/src/gallium/auxiliary/util/u_tracepoints.py repo/src/util/perf/u_trace.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/gallium/auxiliary/u_indices_gen.c
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/gallium/auxiliary/indices/u_indices_gen.py ${GEN}/src/gallium/auxiliary/u_indices_gen.c
    DEPENDS repo/src/gallium/auxiliary/indices/u_indices_gen.py VERBATIM)

add_custom_command(OUTPUT ${GEN}/src/gallium/auxiliary/u_unfilled_gen.c
    COMMAND ${MESA_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/gallium/auxiliary/indices/u_unfilled_gen.py ${GEN}/src/gallium/auxiliary/u_unfilled_gen.c
    DEPENDS repo/src/gallium/auxiliary/indices/u_unfilled_gen.py VERBATIM)

set(MESA_GENERATED_HEADERS
    ${GEN}/src/compiler/nir/nir_builder_opcodes.h
    ${GEN}/src/compiler/nir/nir_opcodes.h
    ${GEN}/src/compiler/nir/nir_intrinsics.h
    ${GEN}/src/compiler/nir/nir_intrinsics_indices.h
    ${GEN}/src/util/format/u_format_gen.h
    ${GEN}/src/util/format/u_format_pack.h
    ${GEN}/src/util/shader_stats.h
    ${GEN}/src/util/driconf_static.h
    ${GEN}/src/git_sha1.h
    ${GEN}/src/compiler/builtin_types.h
    ${GEN}/src/compiler/ir_expression_operation.h
    ${GEN}/src/compiler/glsl/ir_expression_operation_constant.h
    ${GEN}/src/compiler/glsl/ir_expression_operation_strings.h
    ${GEN}/src/compiler/glsl/glsl_parser.h
    ${GEN}/src/compiler/glsl/float64_glsl.h
    ${GEN}/src/compiler/glsl/cross_platform_settings_piece_all.h
    ${GEN}/src/compiler/glsl/bc1_glsl.h
    ${GEN}/src/compiler/glsl/bc4_glsl.h
    ${GEN}/src/compiler/glsl/etc2_rgba_stitch_glsl.h
    ${GEN}/src/compiler/glsl/astc_glsl.h
    ${GEN}/src/compiler/glsl/glcpp/glcpp-parse.h
    ${GEN}/src/compiler/spirv/spirv_info.h
    ${GEN}/src/compiler/spirv/vtn_generator_ids.h
    ${GEN}/src/mesa/format_info.h
    ${GEN}/src/mesa/get_hash.h
    ${GEN}/src/mesa/program/program_parse.tab.h
    ${GEN}/src/mesa/glapi/glapi/gen/api_exec_decl.h
    ${GEN}/src/mesa/glapi/glapi/gen/api_save_init.h
    ${GEN}/src/mesa/glapi/glapi/gen/api_save.h
    ${GEN}/src/mesa/glapi/glapi/gen/api_beginend_init.h
    ${GEN}/src/mesa/glapi/glapi/gen/api_hw_select_init.h
    ${GEN}/src/mesa/glapi/glapi/gen/dispatch.h
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated.h
    ${GEN}/src/mesa/glapi/shared-glapi/shared_glapi_mapi_tmp.h
    ${GEN}/src/gallium/auxiliary/tr_util.h
    ${GEN}/src/gallium/auxiliary/u_tracepoints.h
)

set(MESA_GENERATED_SOURCES
    ${GEN}/src/compiler/nir/nir_constant_expressions.c
    ${GEN}/src/compiler/nir/nir_opcodes.c
    ${GEN}/src/compiler/nir/nir_opt_algebraic.c
    ${GEN}/src/compiler/nir/nir_intrinsics.c
    ${GEN}/src/util/format/u_format_table.c
    ${GEN}/src/util/format_srgb.c
    ${GEN}/src/compiler/builtin_types.c
    ${GEN}/src/compiler/glsl/glsl_parser.cpp
    ${GEN}/src/compiler/glsl/glsl_lexer.cpp
    ${GEN}/src/compiler/glsl/glcpp/glcpp-parse.c
    ${GEN}/src/compiler/glsl/glcpp/glcpp-lex.c
    ${GEN}/src/compiler/spirv/vtn_gather_types.c
    ${GEN}/src/compiler/spirv/spirv_info.c
    ${GEN}/src/mesa/format_fallback.c
    ${GEN}/src/mesa/program/lex.yy.c
    ${GEN}/src/mesa/program/program_parse.tab.c
    ${GEN}/src/mesa/glapi/glapi/gen/enums.c
    ${GEN}/src/mesa/glapi/glapi/gen/api_exec_init.c
    ${GEN}/src/mesa/glapi/glapi/gen/unmarshal_table.c
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated0.c
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated1.c
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated2.c
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated3.c
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated4.c
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated5.c
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated6.c
    ${GEN}/src/mesa/glapi/glapi/gen/marshal_generated7.c
    ${GEN}/src/gallium/auxiliary/tr_util.c
    ${GEN}/src/gallium/auxiliary/u_tracepoints.c
    ${GEN}/src/gallium/auxiliary/u_indices_gen.c
    ${GEN}/src/gallium/auxiliary/u_unfilled_gen.c
)
set(MESA_UTIL_SOURCES
    repo/src/util/anon_file.c
    repo/src/util/bitscan.c
    repo/src/util/blob.c
    repo/src/util/build_id.c
    repo/src/util/cnd_monotonic.c
    repo/src/util/compress.c
    ${PATCHED}/src/util/thread_sched.c
    repo/src/util/crc32.c
    repo/src/util/dag.c
    ${PATCHED}/src/util/disk_cache.c
    repo/src/util/disk_cache_os.c
    repo/src/util/double.c
    repo/src/util/fast_idiv_by_const.c
    repo/src/util/float8.c
    repo/src/util/fossilize_db.c
    repo/src/util/futex.c
    repo/src/util/half_float.c
    repo/src/util/hash_table.c
    repo/src/util/helpers.c
    repo/src/util/u_idalloc.c
    ${PATCHED}/src/util/log.c
    repo/src/util/lut.c
    repo/src/util/memstream.c
    repo/src/util/mesa-sha1.c
    repo/src/util/mesa-blake3.c
    ${PATCHED}/src/util/os_time.c
    repo/src/util/os_file.c
    ${PATCHED}/src/util/os_file_notify.c
    repo/src/util/os_memory_fd.c
    ${PATCHED}/src/util/os_misc.c
    repo/src/util/os_socket.c
    repo/src/util/pb_slab.c
    repo/src/util/perf/u_trace.c
    repo/src/util/u_process.c
    repo/src/util/u_qsort.cpp
    repo/src/util/rwlock.c
    repo/src/util/ralloc.c
    repo/src/util/rand_xor.c
    repo/src/util/range_minimum_query.c
    repo/src/util/rb_tree.c
    repo/src/util/register_allocate.c
    repo/src/util/rgtc.c
    repo/src/util/set.c
    repo/src/util/simple_mtx.c
    repo/src/util/slab.c
    repo/src/util/softfloat.c
    repo/src/util/sparse_array.c
    repo/src/util/string_buffer.c
    repo/src/util/strndup.c
    repo/src/util/strtod.c
    repo/src/util/texcompress_astc_luts.cpp
    repo/src/util/texcompress_astc_luts_wrap.cpp
    repo/src/util/texcompress_astc.cpp
    repo/src/util/u_atomic.c
    repo/src/util/u_call_once.c
    repo/src/util/u_dl.c
    repo/src/util/u_dynarray.c
    repo/src/util/u_hash_table.c
    repo/src/util/u_queue.c
    repo/src/util/u_range_remap.c
    repo/src/util/u_string.c
    ${PATCHED}/src/util/u_thread.c
    repo/src/util/u_vector.c
    repo/src/util/u_math.c
    repo/src/util/u_mm.c
    repo/src/util/u_debug.c
    repo/src/util/u_debug_memory.c
    repo/src/util/u_cpu_detect.c
    repo/src/util/u_printf.c
    repo/src/util/u_worklist.c
    repo/src/util/vl_zscan_data.c
    repo/src/util/vma.c
    repo/src/util/mesa_cache_db.c
    repo/src/util/mesa_cache_db_multipart.c
)

set(MESA_FORMAT_SOURCES
    repo/src/util/format/u_format.c
    repo/src/util/format/u_format_bptc.c
    repo/src/util/format/u_format_etc.c
    repo/src/util/format/u_format_fxt1.c
    repo/src/util/format/u_format_latc.c
    repo/src/util/format/u_format_other.c
    repo/src/util/format/u_format_rgtc.c
    repo/src/util/format/u_format_s3tc.c
    repo/src/util/format/u_format_tests.c
    repo/src/util/format/u_format_unpack_neon.c
    repo/src/util/format/u_format_yuv.c
    repo/src/util/format/u_format_zs.c
)

set(MESA_COMPILER_SOURCES
    repo/src/compiler/glsl_types.c
    repo/src/compiler/shader_enums.c
)

set(MESA_NIR_SOURCES
    repo/src/compiler/nir/nir.c
    repo/src/compiler/nir/nir_builder.c
    repo/src/compiler/nir/nir_builtin_builder.c
    repo/src/compiler/nir/nir_clip_cull_distance_io_utils.c
    repo/src/compiler/nir/nir_clone.c
    repo/src/compiler/nir/nir_control_flow.c
    repo/src/compiler/nir/nir_deref.c
    repo/src/compiler/nir/nir_divergence_analysis.c
    repo/src/compiler/nir/nir_dominance.c
    repo/src/compiler/nir/nir_dominance_lca.c
    repo/src/compiler/nir/nir_downgrade_pls_vars.c
    repo/src/compiler/nir/nir_fixup_is_exported.c
    repo/src/compiler/nir/nir_format_convert.c
    repo/src/compiler/nir/nir_from_ssa.c
    repo/src/compiler/nir/nir_functions.c
    repo/src/compiler/nir/nir_gather_info.c
    repo/src/compiler/nir/nir_gather_output_deps.c
    repo/src/compiler/nir/nir_gather_tcs_info.c
    repo/src/compiler/nir/nir_gather_types.c
    repo/src/compiler/nir/nir_gather_xfb_info.c
    repo/src/compiler/nir/nir_opt_group_loads.c
    repo/src/compiler/nir/nir_gs_count_vertices.c
    repo/src/compiler/nir/nir_inline_sysval.c
    repo/src/compiler/nir/nir_inline_uniforms.c
    repo/src/compiler/nir/nir_instr_set.c
    repo/src/compiler/nir/nir_io_add_xfb_info.c
    repo/src/compiler/nir/nir_legacy.c
    repo/src/compiler/nir/nir_linking_helpers.c
    repo/src/compiler/nir/nir_liveness.c
    repo/src/compiler/nir/nir_loop_analyze.c
    repo/src/compiler/nir/nir_lower_alu.c
    repo/src/compiler/nir/nir_lower_alu_width.c
    repo/src/compiler/nir/nir_lower_alpha.c
    repo/src/compiler/nir/nir_lower_amul.c
    repo/src/compiler/nir/nir_lower_array_deref_of_vec.c
    repo/src/compiler/nir/nir_lower_atomics_to_ssbo.c
    repo/src/compiler/nir/nir_lower_bitmap.c
    repo/src/compiler/nir/nir_lower_blend.c
    repo/src/compiler/nir/nir_lower_bool_to_bitsize.c
    repo/src/compiler/nir/nir_lower_bool_to_float.c
    repo/src/compiler/nir/nir_lower_bool_to_int32.c
    repo/src/compiler/nir/nir_lower_calls_to_builtins.c
    repo/src/compiler/nir/nir_lower_cl_images.c
    repo/src/compiler/nir/nir_lower_clamp_color_outputs.c
    repo/src/compiler/nir/nir_lower_clip.c
    repo/src/compiler/nir/nir_lower_clip_disable.c
    repo/src/compiler/nir/nir_lower_clip_halfz.c
    repo/src/compiler/nir/nir_lower_const_arrays_to_uniforms.c
    repo/src/compiler/nir/nir_lower_continue_constructs.c
    repo/src/compiler/nir/nir_lower_convert_alu_types.c
    repo/src/compiler/nir/nir_lower_cooperative_matrix.c
    repo/src/compiler/nir/nir_lower_variable_initializers.c
    repo/src/compiler/nir/nir_lower_discard_if.c
    repo/src/compiler/nir/nir_lower_double_ops.c
    repo/src/compiler/nir/nir_lower_explicit_io.c
    repo/src/compiler/nir/nir_lower_fb_read.c
    repo/src/compiler/nir/nir_lower_flatshade.c
    repo/src/compiler/nir/nir_lower_floats.c
    repo/src/compiler/nir/nir_lower_flrp.c
    repo/src/compiler/nir/nir_lower_fp16_conv.c
    repo/src/compiler/nir/nir_lower_fragcoord_wtrans.c
    repo/src/compiler/nir/nir_lower_frag_coord_to_pixel_coord.c
    repo/src/compiler/nir/nir_lower_fragcolor.c
    repo/src/compiler/nir/nir_lower_frexp.c
    repo/src/compiler/nir/nir_lower_global_vars_to_local.c
    repo/src/compiler/nir/nir_lower_goto_ifs.c
    repo/src/compiler/nir/nir_lower_gs_intrinsics.c
    repo/src/compiler/nir/nir_lower_halt_to_return.c
    repo/src/compiler/nir/nir_lower_helper_writes.c
    repo/src/compiler/nir/nir_lower_load_const_to_scalar.c
    repo/src/compiler/nir/nir_lower_locals_to_regs.c
    repo/src/compiler/nir/nir_lower_idiv.c
    repo/src/compiler/nir/nir_lower_image.c
    repo/src/compiler/nir/nir_lower_image_atomics_to_global.c
    repo/src/compiler/nir/nir_lower_indirect_derefs_to_if_else_trees.c
    repo/src/compiler/nir/nir_lower_input_attachments.c
    repo/src/compiler/nir/nir_lower_int64.c
    repo/src/compiler/nir/nir_lower_interpolation.c
    repo/src/compiler/nir/nir_lower_int_to_float.c
    repo/src/compiler/nir/nir_lower_io.c
    repo/src/compiler/nir/nir_lower_io_array_vars_to_elements.c
    repo/src/compiler/nir/nir_lower_io_indirect_loads.c
    repo/src/compiler/nir/nir_lower_io_vars_to_temporaries.c
    repo/src/compiler/nir/nir_lower_io_to_scalar.c
    repo/src/compiler/nir/nir_lower_io_vars_to_scalar.c
    repo/src/compiler/nir/nir_lower_is_helper_invocation.c
    repo/src/compiler/nir/nir_lower_multiview.c
    repo/src/compiler/nir/nir_lower_mediump.c
    repo/src/compiler/nir/nir_lower_mem_access_bit_sizes.c
    repo/src/compiler/nir/nir_lower_memcpy.c
    repo/src/compiler/nir/nir_lower_memory_model.c
    repo/src/compiler/nir/nir_lower_non_uniform_access.c
    repo/src/compiler/nir/nir_lower_packing.c
    repo/src/compiler/nir/nir_lower_passthrough_edgeflags.c
    repo/src/compiler/nir/nir_lower_patch_vertices.c
    repo/src/compiler/nir/nir_lower_phis_to_scalar.c
    repo/src/compiler/nir/nir_lower_pntc_ytransform.c
    repo/src/compiler/nir/nir_lower_point_size.c
    repo/src/compiler/nir/nir_lower_point_smooth.c
    repo/src/compiler/nir/nir_lower_poly_line_smooth.c
    repo/src/compiler/nir/nir_lower_printf.c
    repo/src/compiler/nir/nir_lower_reg_intrinsics_to_ssa.c
    repo/src/compiler/nir/nir_lower_readonly_images_to_tex.c
    repo/src/compiler/nir/nir_lower_returns.c
    repo/src/compiler/nir/nir_lower_robust_access.c
    repo/src/compiler/nir/nir_lower_samplers.c
    repo/src/compiler/nir/nir_lower_sample_shading.c
    repo/src/compiler/nir/nir_lower_scratch.c
    repo/src/compiler/nir/nir_lower_scratch_to_var.c
    repo/src/compiler/nir/nir_lower_shader_calls.c
    repo/src/compiler/nir/nir_lower_single_sampled.c
    repo/src/compiler/nir/nir_lower_ssbo.c
    repo/src/compiler/nir/nir_lower_subgroups.c
    repo/src/compiler/nir/nir_lower_system_values.c
    repo/src/compiler/nir/nir_lower_task_shader.c
    repo/src/compiler/nir/nir_lower_terminate_to_demote.c
    repo/src/compiler/nir/nir_lower_tess_coord_z.c
    repo/src/compiler/nir/nir_lower_tex_shadow.c
    repo/src/compiler/nir/nir_lower_tex.c
    repo/src/compiler/nir/nir_lower_texcoord_replace.c
    repo/src/compiler/nir/nir_lower_texcoord_replace_late.c
    repo/src/compiler/nir/nir_lower_two_sided_color.c
    repo/src/compiler/nir/nir_lower_undef_to_zero.c
    repo/src/compiler/nir/nir_lower_vars_to_ssa.c
    repo/src/compiler/nir/nir_lower_var_copies.c
    repo/src/compiler/nir/nir_lower_vec_to_regs.c
    repo/src/compiler/nir/nir_lower_vec3_to_vec4.c
    repo/src/compiler/nir/nir_lower_view_index_to_device_index.c
    repo/src/compiler/nir/nir_lower_viewport_transform.c
    repo/src/compiler/nir/nir_lower_wpos_center.c
    repo/src/compiler/nir/nir_lower_wpos_ytransform.c
    repo/src/compiler/nir/nir_lower_wrmasks.c
    repo/src/compiler/nir/nir_lower_bit_size.c
    repo/src/compiler/nir/nir_lower_ubo_vec4.c
    repo/src/compiler/nir/nir_lower_uniforms_to_ubo.c
    repo/src/compiler/nir/nir_lower_workgroup_size.c
    repo/src/compiler/nir/nir_lower_sysvals_to_varyings.c
    repo/src/compiler/nir/nir_metadata.c
    repo/src/compiler/nir/nir_mod_analysis.c
    repo/src/compiler/nir/nir_move_output_stores_to_end.c
    repo/src/compiler/nir/nir_move_vec_src_uses_to_dest.c
    repo/src/compiler/nir/nir_normalize_cubemap_coords.c
    repo/src/compiler/nir/nir_opt_access.c
    repo/src/compiler/nir/nir_opt_barriers.c
    repo/src/compiler/nir/nir_opt_barycentric.c
    repo/src/compiler/nir/nir_opt_call.c
    repo/src/compiler/nir/nir_opt_clip_cull_const.c
    repo/src/compiler/nir/nir_opt_combine_stores.c
    repo/src/compiler/nir/nir_opt_comparison_pre.c
    repo/src/compiler/nir/nir_opt_constant_folding.c
    repo/src/compiler/nir/nir_opt_copy_prop_vars.c
    repo/src/compiler/nir/nir_opt_copy_propagate.c
    repo/src/compiler/nir/nir_opt_cse.c
    repo/src/compiler/nir/nir_opt_dce.c
    repo/src/compiler/nir/nir_opt_dead_cf.c
    repo/src/compiler/nir/nir_opt_dead_write_vars.c
    repo/src/compiler/nir/nir_opt_find_array_copies.c
    repo/src/compiler/nir/nir_opt_frag_coord_to_pixel_coord.c
    repo/src/compiler/nir/nir_opt_fragdepth.c
    repo/src/compiler/nir/nir_opt_gcm.c
    repo/src/compiler/nir/nir_opt_generate_bfi.c
    repo/src/compiler/nir/nir_opt_idiv_const.c
    repo/src/compiler/nir/nir_opt_if.c
    repo/src/compiler/nir/nir_opt_intrinsics.c
    repo/src/compiler/nir/nir_opt_large_constants.c
    repo/src/compiler/nir/nir_opt_licm.c
    repo/src/compiler/nir/nir_opt_load_skip_helpers.c
    repo/src/compiler/nir/nir_opt_load_store_vectorize.c
    repo/src/compiler/nir/nir_opt_loop.c
    repo/src/compiler/nir/nir_opt_loop_unroll.c
    repo/src/compiler/nir/nir_opt_memcpy.c
    repo/src/compiler/nir/nir_opt_move.c
    repo/src/compiler/nir/nir_opt_move_discards_to_top.c
    repo/src/compiler/nir/nir_opt_move_to_top.c
    repo/src/compiler/nir/nir_opt_mqsad.c
    repo/src/compiler/nir/nir_opt_non_uniform_access.c
    repo/src/compiler/nir/nir_opt_offsets.c
    repo/src/compiler/nir/nir_opt_peephole_select.c
    repo/src/compiler/nir/nir_opt_phi_precision.c
    repo/src/compiler/nir/nir_opt_phi_to_bool.c
    repo/src/compiler/nir/nir_opt_preamble.c
    repo/src/compiler/nir/nir_opt_ray_queries.c
    repo/src/compiler/nir/nir_opt_reassociate.c
    repo/src/compiler/nir/nir_opt_reassociate_bfi.c
    repo/src/compiler/nir/nir_opt_rematerialize_compares.c
    repo/src/compiler/nir/nir_opt_remove_phis.c
    repo/src/compiler/nir/nir_opt_shrink_stores.c
    repo/src/compiler/nir/nir_opt_shrink_vectors.c
    repo/src/compiler/nir/nir_opt_sink.c
    repo/src/compiler/nir/nir_opt_undef.c
    repo/src/compiler/nir/nir_opt_uniform_atomics.c
    repo/src/compiler/nir/nir_opt_uniform_subgroup.c
    repo/src/compiler/nir/nir_opt_uub.c
    repo/src/compiler/nir/nir_opt_varyings.c
    repo/src/compiler/nir/nir_opt_vectorize.c
    repo/src/compiler/nir/nir_opt_vectorize_io.c
    repo/src/compiler/nir/nir_opt_vectorize_io_vars.c
    repo/src/compiler/nir/nir_passthrough_gs.c
    repo/src/compiler/nir/nir_passthrough_tcs.c
    repo/src/compiler/nir/nir_phi_builder.c
    repo/src/compiler/nir/nir_print.c
    repo/src/compiler/nir/nir_propagate_invariant.c
    repo/src/compiler/nir/nir_range_analysis.c
    repo/src/compiler/nir/nir_recompute_io_bases.c
    repo/src/compiler/nir/nir_remove_dead_variables.c
    repo/src/compiler/nir/nir_remove_outputs.c
    repo/src/compiler/nir/nir_remove_tex_shadow.c
    repo/src/compiler/nir/nir_repair_ssa.c
    repo/src/compiler/nir/nir_scale_fdiv.c
    repo/src/compiler/nir/nir_schedule.c
    repo/src/compiler/nir/nir_search.c
    repo/src/compiler/nir/nir_separate_merged_clip_cull_io.c
    repo/src/compiler/nir/nir_serialize.c
    repo/src/compiler/nir/nir_shader_bisect.c
    repo/src/compiler/nir/nir_split_64bit_vec3_and_vec4.c
    repo/src/compiler/nir/nir_split_conversions.c
    repo/src/compiler/nir/nir_split_per_member_structs.c
    repo/src/compiler/nir/nir_split_var_copies.c
    repo/src/compiler/nir/nir_split_vars.c
    repo/src/compiler/nir/nir_sweep.c
    repo/src/compiler/nir/nir_to_lcssa.c
    repo/src/compiler/nir/nir_trivialize_registers.c
    repo/src/compiler/nir/nir_unlower_io_to_vars.c
    repo/src/compiler/nir/nir_use_dominance.c
    repo/src/compiler/nir/nir_validate.c
    repo/src/compiler/nir/nir_worklist.c
    repo/src/compiler/nir/nir_lower_atomics.c
)

set(MESA_GLSL_SOURCES
    repo/src/compiler/glsl/ast_array_index.cpp
    repo/src/compiler/glsl/ast_expr.cpp
    repo/src/compiler/glsl/ast_function.cpp
    repo/src/compiler/glsl/ast_to_hir.cpp
    repo/src/compiler/glsl/ast_type.cpp
    repo/src/compiler/glsl/builtin_functions.cpp
    repo/src/compiler/glsl/builtin_types.cpp
    repo/src/compiler/glsl/builtin_variables.cpp
    repo/src/compiler/glsl/gl_nir_detect_function_recursion.c
    repo/src/compiler/glsl/gl_nir_lower_atomics.c
    repo/src/compiler/glsl/gl_nir_lower_images.c
    repo/src/compiler/glsl/gl_nir_lower_blend_equation_advanced.c
    repo/src/compiler/glsl/gl_nir_lower_buffers.c
    repo/src/compiler/glsl/gl_nir_lower_discard_flow.c
    repo/src/compiler/glsl/gl_nir_lower_named_interface_blocks.c
    repo/src/compiler/glsl/gl_nir_lower_packed_varyings.c
    repo/src/compiler/glsl/gl_nir_lower_samplers.c
    repo/src/compiler/glsl/gl_nir_lower_samplers_as_deref.c
    repo/src/compiler/glsl/gl_nir_lower_xfb_varying.c
    repo/src/compiler/glsl/gl_nir_link_atomics.c
    repo/src/compiler/glsl/gl_nir_link_functions.c
    repo/src/compiler/glsl/gl_nir_link_interface_blocks.c
    repo/src/compiler/glsl/gl_nir_link_uniform_blocks.c
    repo/src/compiler/glsl/gl_nir_link_uniform_initializers.c
    repo/src/compiler/glsl/gl_nir_link_uniforms.c
    repo/src/compiler/glsl/gl_nir_link_varyings.c
    repo/src/compiler/glsl/gl_nir_link_xfb.c
    repo/src/compiler/glsl/gl_nir_linker.c
    repo/src/compiler/glsl/glsl_parser_extras.cpp
    repo/src/compiler/glsl/glsl_symbol_table.cpp
    repo/src/compiler/glsl/glsl_to_nir.cpp
    repo/src/compiler/glsl/hir_field_selection.cpp
    repo/src/compiler/glsl/ir_basic_block.cpp
    repo/src/compiler/glsl/ir_builder.cpp
    repo/src/compiler/glsl/ir_clone.cpp
    repo/src/compiler/glsl/ir_constant_expression.cpp
    repo/src/compiler/glsl/ir.cpp
    repo/src/compiler/glsl/ir_expression_flattening.cpp
    repo/src/compiler/glsl/ir_function_detect_recursion.cpp
    repo/src/compiler/glsl/ir_function.cpp
    repo/src/compiler/glsl/ir_hierarchical_visitor.cpp
    repo/src/compiler/glsl/ir_hv_accept.cpp
    repo/src/compiler/glsl/ir_print_visitor.cpp
    repo/src/compiler/glsl/ir_rvalue_visitor.cpp
    repo/src/compiler/glsl/ir_validate.cpp
    repo/src/compiler/glsl/ir_variable_refcount.cpp
    repo/src/compiler/glsl/linker_util.cpp
    repo/src/compiler/glsl/lower_builtins.cpp
    repo/src/compiler/glsl/lower_instructions.cpp
    repo/src/compiler/glsl/lower_jumps.cpp
    repo/src/compiler/glsl/lower_mat_op_to_vec.cpp
    repo/src/compiler/glsl/lower_packing_builtins.cpp
    repo/src/compiler/glsl/lower_precision.cpp
    repo/src/compiler/glsl/lower_subroutine.cpp
    repo/src/compiler/glsl/lower_vec_index_to_cond_assign.cpp
    repo/src/compiler/glsl/lower_vector_derefs.cpp
    repo/src/compiler/glsl/opt_algebraic.cpp
    repo/src/compiler/glsl/opt_dead_builtin_variables.cpp
    repo/src/compiler/glsl/opt_dead_code.cpp
    repo/src/compiler/glsl/opt_flatten_nested_if_blocks.cpp
    repo/src/compiler/glsl/opt_function_inlining.cpp
    repo/src/compiler/glsl/opt_if_simplification.cpp
    repo/src/compiler/glsl/opt_minmax.cpp
    repo/src/compiler/glsl/opt_rebalance_tree.cpp
    repo/src/compiler/glsl/opt_tree_grafting.cpp
    repo/src/compiler/glsl/propagate_invariance.cpp
    repo/src/compiler/glsl/string_to_uint_map.cpp
    repo/src/compiler/glsl/serialize.cpp
    repo/src/compiler/glsl/shader_cache.cpp
)

set(MESA_VTN_SOURCES
    repo/src/compiler/spirv/gl_spirv.c
    repo/src/compiler/spirv/spirv_to_nir.c
    repo/src/compiler/spirv/vtn_alu.c
    repo/src/compiler/spirv/vtn_amd.c
    repo/src/compiler/spirv/vtn_cfg.c
    repo/src/compiler/spirv/vtn_cmat.c
    repo/src/compiler/spirv/vtn_debug.c
    repo/src/compiler/spirv/vtn_glsl450.c
    repo/src/compiler/spirv/vtn_opencl.c
    repo/src/compiler/spirv/vtn_structured_cfg.c
    repo/src/compiler/spirv/vtn_subgroup.c
    repo/src/compiler/spirv/vtn_variables.c
)

set(MESA_CORE_SOURCES
    repo/src/mesa/main/accum.c
    repo/src/mesa/main/api_arrayelt.c
    repo/src/mesa/main/arbprogram.c
    repo/src/mesa/main/arrayobj.c
    repo/src/mesa/main/atifragshader.c
    repo/src/mesa/main/attrib.c
    repo/src/mesa/main/barrier.c
    repo/src/mesa/main/bbox.c
    repo/src/mesa/main/blend.c
    repo/src/mesa/main/blit.c
    repo/src/mesa/main/bufferobj.c
    repo/src/mesa/main/buffers.c
    repo/src/mesa/main/clear.c
    repo/src/mesa/main/clip.c
    repo/src/mesa/main/compute.c
    repo/src/mesa/main/condrender.c
    repo/src/mesa/main/conservativeraster.c
    repo/src/mesa/main/context.c
    repo/src/mesa/main/copyimage.c
    repo/src/mesa/main/debug.c
    repo/src/mesa/main/debug_output.c
    repo/src/mesa/main/depth.c
    repo/src/mesa/main/dlist.c
    repo/src/mesa/main/draw.c
    repo/src/mesa/main/draw_validate.c
    repo/src/mesa/main/drawpix.c
    repo/src/mesa/main/drawtex.c
    repo/src/mesa/main/enable.c
    repo/src/mesa/main/errors.c
    repo/src/mesa/main/es1_conversion.c
    repo/src/mesa/main/eval.c
    repo/src/mesa/main/extensions.c
    repo/src/mesa/main/extensions_table.c
    repo/src/mesa/main/externalobjects.c
    repo/src/mesa/main/fbobject.c
    repo/src/mesa/main/feedback.c
    repo/src/mesa/main/ff_fragment_shader.c
    repo/src/mesa/main/ffvertex_prog.c
    repo/src/mesa/main/fog.c
    repo/src/mesa/main/format_utils.c
    repo/src/mesa/main/formatquery.c
    repo/src/mesa/main/formats.c
    repo/src/mesa/main/framebuffer.c
    repo/src/mesa/main/genmipmap.c
    repo/src/mesa/main/get.c
    repo/src/mesa/main/getstring.c
    repo/src/mesa/main/glformats.c
    repo/src/mesa/main/glspirv.c
    repo/src/mesa/main/glthread.c
    repo/src/mesa/main/glthread_bufferobj.c
    repo/src/mesa/main/glthread_draw.c
    repo/src/mesa/main/glthread_draw_unroll.c
    repo/src/mesa/main/glthread_get.c
    repo/src/mesa/main/glthread_list.c
    repo/src/mesa/main/glthread_pixels.c
    repo/src/mesa/main/glthread_shaderobj.c
    repo/src/mesa/main/glthread_varray.c
    repo/src/mesa/main/hash.c
    repo/src/mesa/main/hint.c
    repo/src/mesa/main/image.c
    repo/src/mesa/main/light.c
    repo/src/mesa/main/lines.c
    repo/src/mesa/main/matrix.c
    repo/src/mesa/main/mesh_shader.c
    repo/src/mesa/main/mipmap.c
    repo/src/mesa/main/multisample.c
    repo/src/mesa/main/objectlabel.c
    repo/src/mesa/main/pack.c
    repo/src/mesa/main/pbo.c
    repo/src/mesa/main/performance_monitor.c
    repo/src/mesa/main/performance_query.c
    repo/src/mesa/main/pipelineobj.c
    repo/src/mesa/main/pixel.c
    repo/src/mesa/main/pixelstore.c
    repo/src/mesa/main/pixeltransfer.c
    repo/src/mesa/main/points.c
    repo/src/mesa/main/polygon.c
    repo/src/mesa/main/program_binary.c
    repo/src/mesa/main/program_resource.c
    repo/src/mesa/main/querymatrix.c
    repo/src/mesa/main/queryobj.c
    repo/src/mesa/main/rastpos.c
    repo/src/mesa/main/readpix.c
    repo/src/mesa/main/renderbuffer.c
    repo/src/mesa/main/robustness.c
    repo/src/mesa/main/samplerobj.c
    repo/src/mesa/main/scissor.c
    repo/src/mesa/main/shader_query.cpp
    repo/src/mesa/main/shaderapi.c
    repo/src/mesa/main/shaderimage.c
    repo/src/mesa/main/shaderobj.c
    repo/src/mesa/main/shared.c
    repo/src/mesa/main/spirv_capabilities.c
    repo/src/mesa/main/spirv_extensions.c
    repo/src/mesa/main/state.c
    repo/src/mesa/main/stencil.c
    repo/src/mesa/main/syncobj.c
    repo/src/mesa/main/texcompress.c
    repo/src/mesa/main/texcompress_bptc.c
    repo/src/mesa/main/texcompress_cpal.c
    repo/src/mesa/main/texcompress_etc.c
    repo/src/mesa/main/texcompress_fxt1.c
    repo/src/mesa/main/texcompress_rgtc.c
    repo/src/mesa/main/texcompress_s3tc.c
    repo/src/mesa/main/texenv.c
    repo/src/mesa/main/texgen.c
    repo/src/mesa/main/texgetimage.c
    repo/src/mesa/main/teximage.c
    repo/src/mesa/main/texobj.c
    repo/src/mesa/main/texparam.c
    repo/src/mesa/main/texstate.c
    repo/src/mesa/main/texstorage.c
    repo/src/mesa/main/texstore.c
    repo/src/mesa/main/texturebindless.c
    repo/src/mesa/main/textureview.c
    repo/src/mesa/main/transformfeedback.c
    repo/src/mesa/main/uniform_query.cpp
    repo/src/mesa/main/uniforms.c
    repo/src/mesa/main/varray.c
    repo/src/mesa/main/version.c
    repo/src/mesa/main/viewport.c
    repo/src/mesa/main/es1_conversion.c
    repo/src/mesa/math/m_eval.c
    repo/src/mesa/math/m_matrix.c
    repo/src/mesa/program/arbprogparse.c
    repo/src/mesa/program/prog_cache.c
    repo/src/mesa/program/prog_instruction.c
    repo/src/mesa/program/prog_parameter.c
    repo/src/mesa/program/prog_parameter_layout.c
    repo/src/mesa/program/prog_print.c
    repo/src/mesa/program/prog_statevars.c
    repo/src/mesa/program/prog_to_nir.c
    repo/src/mesa/program/program.c
    repo/src/mesa/program/program_parse_extra.c
    repo/src/mesa/program/symbol_table.c
    repo/src/mesa/state_tracker/st_atifs_to_nir.c
    repo/src/mesa/state_tracker/st_atom_array.cpp
    repo/src/mesa/state_tracker/st_atom_atomicbuf.c
    repo/src/mesa/state_tracker/st_atom_blend.c
    repo/src/mesa/state_tracker/st_atom_clip.c
    repo/src/mesa/state_tracker/st_atom_constbuf.c
    repo/src/mesa/state_tracker/st_atom_depth.c
    repo/src/mesa/state_tracker/st_atom_framebuffer.c
    repo/src/mesa/state_tracker/st_atom_image.c
    repo/src/mesa/state_tracker/st_atom_msaa.c
    repo/src/mesa/state_tracker/st_atom_pixeltransfer.c
    repo/src/mesa/state_tracker/st_atom_rasterizer.c
    repo/src/mesa/state_tracker/st_atom_sampler.c
    repo/src/mesa/state_tracker/st_atom_scissor.c
    repo/src/mesa/state_tracker/st_atom_shader.c
    repo/src/mesa/state_tracker/st_atom_stipple.c
    repo/src/mesa/state_tracker/st_atom_storagebuf.c
    repo/src/mesa/state_tracker/st_atom_tess.c
    repo/src/mesa/state_tracker/st_atom_texture.c
    repo/src/mesa/state_tracker/st_atom_viewport.c
    repo/src/mesa/state_tracker/st_cb_bitmap.c
    repo/src/mesa/state_tracker/st_cb_clear.c
    repo/src/mesa/state_tracker/st_cb_copyimage.c
    repo/src/mesa/state_tracker/st_cb_drawpixels.c
    repo/src/mesa/state_tracker/st_cb_drawtex.c
    repo/src/mesa/state_tracker/st_cb_eglimage.c
    repo/src/mesa/state_tracker/st_cb_feedback.c
    repo/src/mesa/state_tracker/st_cb_flush.c
    repo/src/mesa/state_tracker/st_cb_rasterpos.c
    repo/src/mesa/state_tracker/st_cb_readpixels.c
    repo/src/mesa/state_tracker/st_cb_texture.c
    repo/src/mesa/state_tracker/st_context.c
    repo/src/mesa/state_tracker/st_copytex.c
    repo/src/mesa/state_tracker/st_debug.c
    repo/src/mesa/state_tracker/st_draw.c
    repo/src/mesa/state_tracker/st_draw_feedback.c
    repo/src/mesa/state_tracker/st_draw_hw_select.c
    repo/src/mesa/state_tracker/st_extensions.c
    repo/src/mesa/state_tracker/st_format.c
    repo/src/mesa/state_tracker/st_gen_mipmap.c
    repo/src/mesa/state_tracker/st_glsl_to_nir.cpp
    repo/src/mesa/state_tracker/st_interop.c
    repo/src/mesa/state_tracker/st_manager.c
    repo/src/mesa/state_tracker/st_nir_builtins.c
    repo/src/mesa/state_tracker/st_nir_lower_alpha_test.c
    repo/src/mesa/state_tracker/st_nir_lower_builtin.c
    repo/src/mesa/state_tracker/st_nir_lower_drawpixels.c
    repo/src/mesa/state_tracker/st_nir_lower_fog.c
    repo/src/mesa/state_tracker/st_nir_lower_point_size_mov.c
    repo/src/mesa/state_tracker/st_nir_lower_position_invariant.c
    repo/src/mesa/state_tracker/st_nir_lower_tex_src_plane.c
    repo/src/mesa/state_tracker/st_pbo.c
    repo/src/mesa/state_tracker/st_pbo_compute.c
    repo/src/mesa/state_tracker/st_program.c
    repo/src/mesa/state_tracker/st_sampler_view.c
    repo/src/mesa/state_tracker/st_scissor.c
    repo/src/mesa/state_tracker/st_shader_cache.c
    repo/src/mesa/state_tracker/st_texcompress_compute.c
    repo/src/mesa/state_tracker/st_texture.c
    repo/src/mesa/vbo/vbo_context.c
    repo/src/mesa/vbo/vbo_exec.c
    repo/src/mesa/vbo/vbo_exec_api.c
    repo/src/mesa/vbo/vbo_exec_draw.c
    repo/src/mesa/vbo/vbo_exec_eval.c
    repo/src/mesa/vbo/vbo_minmax_index.c
    repo/src/mesa/vbo/vbo_noop.c
    repo/src/mesa/vbo/vbo_save.c
    repo/src/mesa/vbo/vbo_save_api.c
    repo/src/mesa/vbo/vbo_save_draw.c
    repo/src/mesa/vbo/vbo_save_loopback.c
)

set(MESA_GALLIUM_AUX_SOURCES
    repo/src/gallium/auxiliary/cso_cache/cso_cache.c
    repo/src/gallium/auxiliary/cso_cache/cso_context.c
    repo/src/gallium/auxiliary/cso_cache/cso_hash.c
    repo/src/gallium/auxiliary/draw/draw_context.c
    repo/src/gallium/auxiliary/draw/draw_fs.c
    repo/src/gallium/auxiliary/draw/draw_gs.c
    repo/src/gallium/auxiliary/draw/draw_mesh.c
    repo/src/gallium/auxiliary/draw/draw_mesh_prim.c
    repo/src/gallium/auxiliary/draw/draw_pipe_aaline.c
    repo/src/gallium/auxiliary/draw/draw_pipe_aapoint.c
    repo/src/gallium/auxiliary/draw/draw_pipe.c
    repo/src/gallium/auxiliary/draw/draw_pipe_clip.c
    repo/src/gallium/auxiliary/draw/draw_pipe_cull.c
    repo/src/gallium/auxiliary/draw/draw_pipe_flatshade.c
    repo/src/gallium/auxiliary/draw/draw_pipe_offset.c
    repo/src/gallium/auxiliary/draw/draw_pipe_pstipple.c
    repo/src/gallium/auxiliary/draw/draw_pipe_stipple.c
    repo/src/gallium/auxiliary/draw/draw_pipe_twoside.c
    repo/src/gallium/auxiliary/draw/draw_pipe_unfilled.c
    repo/src/gallium/auxiliary/draw/draw_pipe_user_cull.c
    repo/src/gallium/auxiliary/draw/draw_pipe_util.c
    repo/src/gallium/auxiliary/draw/draw_pipe_validate.c
    repo/src/gallium/auxiliary/draw/draw_pipe_vbuf.c
    repo/src/gallium/auxiliary/draw/draw_pipe_wide_line.c
    repo/src/gallium/auxiliary/draw/draw_pipe_wide_point.c
    repo/src/gallium/auxiliary/draw/draw_prim_assembler.c
    repo/src/gallium/auxiliary/draw/draw_pt.c
    repo/src/gallium/auxiliary/draw/draw_pt_emit.c
    repo/src/gallium/auxiliary/draw/draw_pt_fetch.c
    repo/src/gallium/auxiliary/draw/draw_pt_fetch_shade_emit.c
    repo/src/gallium/auxiliary/draw/draw_pt_fetch_shade_pipeline.c
    repo/src/gallium/auxiliary/draw/draw_pt_mesh_pipeline.c
    repo/src/gallium/auxiliary/draw/draw_pt_post_vs.c
    repo/src/gallium/auxiliary/draw/draw_pt_so_emit.c
    repo/src/gallium/auxiliary/draw/draw_pt_util.c
    repo/src/gallium/auxiliary/draw/draw_pt_vsplit.c
    repo/src/gallium/auxiliary/draw/draw_tess.c
    repo/src/gallium/auxiliary/draw/draw_vertex.c
    repo/src/gallium/auxiliary/draw/draw_vs.c
    repo/src/gallium/auxiliary/draw/draw_vs_exec.c
    repo/src/gallium/auxiliary/draw/draw_vs_variant.c
    repo/src/gallium/auxiliary/driver_ddebug/dd_context.c
    ${PATCHED}/src/gallium/auxiliary/driver_ddebug/dd_draw.c
    repo/src/gallium/auxiliary/driver_ddebug/dd_screen.c
    repo/src/gallium/auxiliary/driver_noop/noop_pipe.c
    repo/src/gallium/auxiliary/driver_noop/noop_state.c
    repo/src/gallium/auxiliary/driver_trace/tr_context.c
    repo/src/gallium/auxiliary/driver_trace/tr_dump.c
    repo/src/gallium/auxiliary/driver_trace/tr_dump_state.c
    repo/src/gallium/auxiliary/driver_trace/tr_screen.c
    repo/src/gallium/auxiliary/driver_trace/tr_texture.c
    repo/src/gallium/auxiliary/driver_trace/tr_video.c
    repo/src/gallium/auxiliary/hud/font.c
    repo/src/gallium/auxiliary/hud/hud_context.c
    repo/src/gallium/auxiliary/hud/hud_cpu.c
    repo/src/gallium/auxiliary/hud/hud_nic.c
    repo/src/gallium/auxiliary/hud/hud_cpufreq.c
    repo/src/gallium/auxiliary/hud/hud_diskstat.c
    repo/src/gallium/auxiliary/hud/hud_sensors_temp.c
    repo/src/gallium/auxiliary/hud/hud_driver_query.c
    repo/src/gallium/auxiliary/hud/hud_fps.c
    repo/src/gallium/auxiliary/indices/u_primconvert.c
    repo/src/gallium/auxiliary/pipebuffer/pb_buffer_fenced.c
    repo/src/gallium/auxiliary/pipebuffer/pb_bufmgr_cache.c
    repo/src/gallium/auxiliary/pipebuffer/pb_bufmgr_debug.c
    repo/src/gallium/auxiliary/pipebuffer/pb_bufmgr_mm.c
    repo/src/gallium/auxiliary/pipebuffer/pb_bufmgr_slab.c
    repo/src/gallium/auxiliary/pipebuffer/pb_cache.c
    repo/src/gallium/auxiliary/pipebuffer/pb_validate.c
    repo/src/gallium/auxiliary/postprocess/pp_celshade.c
    repo/src/gallium/auxiliary/postprocess/pp_colors.c
    repo/src/gallium/auxiliary/postprocess/pp_init.c
    repo/src/gallium/auxiliary/postprocess/pp_mlaa.c
    repo/src/gallium/auxiliary/postprocess/pp_program.c
    repo/src/gallium/auxiliary/postprocess/pp_run.c
    repo/src/gallium/auxiliary/rtasm/rtasm_execmem.c
    ${PATCHED}/src/gallium/auxiliary/rtasm/rtasm_x86sse.c
    repo/src/gallium/auxiliary/tgsi/tgsi_aa_point.c
    repo/src/gallium/auxiliary/tgsi/tgsi_build.c
    repo/src/gallium/auxiliary/tgsi/tgsi_dump.c
    repo/src/gallium/auxiliary/tgsi/tgsi_dynamic_indexing.c
    repo/src/gallium/auxiliary/tgsi/tgsi_exec.c
    repo/src/gallium/auxiliary/tgsi/tgsi_from_mesa.c
    repo/src/gallium/auxiliary/tgsi/tgsi_info.c
    repo/src/gallium/auxiliary/tgsi/tgsi_iterate.c
    repo/src/gallium/auxiliary/tgsi/tgsi_lowering.c
    repo/src/gallium/auxiliary/tgsi/tgsi_parse.c
    repo/src/gallium/auxiliary/tgsi/tgsi_point_sprite.c
    repo/src/gallium/auxiliary/tgsi/tgsi_sanity.c
    repo/src/gallium/auxiliary/tgsi/tgsi_scan.c
    repo/src/gallium/auxiliary/tgsi/tgsi_strings.c
    repo/src/gallium/auxiliary/tgsi/tgsi_text.c
    repo/src/gallium/auxiliary/tgsi/tgsi_transform.c
    repo/src/gallium/auxiliary/tgsi/tgsi_two_side.c
    repo/src/gallium/auxiliary/tgsi/tgsi_ureg.c
    repo/src/gallium/auxiliary/tgsi/tgsi_util.c
    repo/src/gallium/auxiliary/tgsi/tgsi_vpos.c
    repo/src/gallium/auxiliary/translate/translate.c
    repo/src/gallium/auxiliary/translate/translate_cache.c
    repo/src/gallium/auxiliary/translate/translate_generic.c
    repo/src/gallium/auxiliary/translate/translate_sse.c
    repo/src/gallium/auxiliary/util/u_async_debug.c
    repo/src/gallium/auxiliary/util/u_bitmask.c
    repo/src/gallium/auxiliary/util/u_blitter.c
    repo/src/gallium/auxiliary/util/u_debug_describe.c
    repo/src/gallium/auxiliary/util/u_debug_flush.c
    repo/src/gallium/auxiliary/util/u_debug_image.c
    repo/src/gallium/auxiliary/util/u_debug_refcnt.c
    repo/src/gallium/auxiliary/util/u_draw.c
    repo/src/gallium/auxiliary/util/u_draw_quad.c
    repo/src/gallium/auxiliary/util/u_driconf.c
    repo/src/gallium/auxiliary/util/u_dump_defines.c
    repo/src/gallium/auxiliary/util/u_dump_state.c
    repo/src/gallium/auxiliary/util/u_framebuffer.c
    repo/src/gallium/auxiliary/util/u_gen_mipmap.c
    repo/src/gallium/auxiliary/util/u_handle_table.c
    repo/src/gallium/auxiliary/util/u_helpers.c
    repo/src/gallium/auxiliary/util/u_index_modify.c
    repo/src/gallium/auxiliary/util/u_live_shader_cache.c
    repo/src/gallium/auxiliary/util/u_log.c
    repo/src/gallium/auxiliary/util/u_prim.c
    repo/src/gallium/auxiliary/util/u_prim_restart.c
    repo/src/gallium/auxiliary/util/u_pstipple.c
    repo/src/gallium/auxiliary/util/u_resource.c
    repo/src/gallium/auxiliary/util/u_sample_positions.c
    repo/src/gallium/auxiliary/util/u_sampler.c
    repo/src/gallium/auxiliary/util/u_screen.c
    repo/src/gallium/auxiliary/util/u_simple_shaders.c
    repo/src/gallium/auxiliary/util/u_split_draw.c
    repo/src/gallium/auxiliary/util/u_suballoc.c
    repo/src/gallium/auxiliary/util/u_surface.c
    repo/src/gallium/auxiliary/util/u_tests.c
    repo/src/gallium/auxiliary/util/u_texture.c
    repo/src/gallium/auxiliary/util/u_tile.c
    repo/src/gallium/auxiliary/util/u_transfer.c
    repo/src/gallium/auxiliary/util/u_transfer_helper.c
    repo/src/gallium/auxiliary/util/u_threaded_context.c
    repo/src/gallium/auxiliary/util/u_trace_gallium.c
    repo/src/gallium/auxiliary/util/u_upload_mgr.c
    repo/src/gallium/auxiliary/util/u_vbuf.c
    repo/src/gallium/auxiliary/util/u_vertex_state_cache.c
    repo/src/gallium/auxiliary/nir/tgsi_to_nir.c
    repo/src/gallium/auxiliary/nir/nir_to_tgsi.c
    repo/src/gallium/auxiliary/nir/nir_draw_helpers.c
)

set(MESA_GALLIUM_VL_SOURCES
    repo/src/gallium/auxiliary/vl/vl_codec.c
    repo/src/gallium/auxiliary/vl/vl_compositor.c
    repo/src/gallium/auxiliary/vl/vl_compositor_gfx.c
    repo/src/gallium/auxiliary/vl/vl_compositor_cs.c
    repo/src/gallium/auxiliary/vl/vl_csc.c
    repo/src/gallium/auxiliary/vl/vl_deint_filter.c
    repo/src/gallium/auxiliary/vl/vl_deint_filter_cs.c
    repo/src/gallium/auxiliary/vl/vl_mpeg12_bitstream.c
    repo/src/gallium/auxiliary/vl/vl_vertex_buffers.c
    repo/src/gallium/auxiliary/vl/vl_video_buffer.c
)

set(MESA_SOFTPIPE_SOURCES
    repo/src/gallium/drivers/softpipe/sp_buffer.c
    repo/src/gallium/drivers/softpipe/sp_clear.c
    repo/src/gallium/drivers/softpipe/sp_context.c
    repo/src/gallium/drivers/softpipe/sp_compute.c
    repo/src/gallium/drivers/softpipe/sp_draw_arrays.c
    repo/src/gallium/drivers/softpipe/sp_fence.c
    repo/src/gallium/drivers/softpipe/sp_flush.c
    repo/src/gallium/drivers/softpipe/sp_fs_exec.c
    repo/src/gallium/drivers/softpipe/sp_image.c
    repo/src/gallium/drivers/softpipe/sp_prim_vbuf.c
    repo/src/gallium/drivers/softpipe/sp_quad_blend.c
    repo/src/gallium/drivers/softpipe/sp_quad_depth_test.c
    repo/src/gallium/drivers/softpipe/sp_quad_fs.c
    repo/src/gallium/drivers/softpipe/sp_quad_pipe.c
    repo/src/gallium/drivers/softpipe/sp_query.c
    repo/src/gallium/drivers/softpipe/sp_screen.c
    repo/src/gallium/drivers/softpipe/sp_setup.c
    repo/src/gallium/drivers/softpipe/sp_state_blend.c
    repo/src/gallium/drivers/softpipe/sp_state_clip.c
    repo/src/gallium/drivers/softpipe/sp_state_derived.c
    repo/src/gallium/drivers/softpipe/sp_state_image.c
    repo/src/gallium/drivers/softpipe/sp_state_rasterizer.c
    repo/src/gallium/drivers/softpipe/sp_state_sampler.c
    repo/src/gallium/drivers/softpipe/sp_state_shader.c
    repo/src/gallium/drivers/softpipe/sp_state_so.c
    repo/src/gallium/drivers/softpipe/sp_state_surface.c
    repo/src/gallium/drivers/softpipe/sp_state_vertex.c
    repo/src/gallium/drivers/softpipe/sp_surface.c
    repo/src/gallium/drivers/softpipe/sp_tex_sample.c
    repo/src/gallium/drivers/softpipe/sp_tex_tile_cache.c
    repo/src/gallium/drivers/softpipe/sp_texture.c
    repo/src/gallium/drivers/softpipe/sp_tile_cache.c
)

set(MESA_EGL_SOURCES
    repo/src/egl/main/eglapi.c
    repo/src/egl/main/eglarray.c
    repo/src/egl/main/eglconfigdebug.c
    repo/src/egl/main/eglconfig.c
    repo/src/egl/main/eglcontext.c
    repo/src/egl/main/eglcurrent.c
    repo/src/egl/main/egldevice.c
    repo/src/egl/main/egldisplay.c
    repo/src/egl/main/eglglobals.c
    repo/src/egl/main/eglimage.c
    repo/src/egl/main/egllog.c
    repo/src/egl/main/eglsurface.c
    repo/src/egl/main/eglsync.c
)

# ---------------------------------------------------------------------------

list(REMOVE_ITEM MESA_UTIL_SOURCES
    repo/src/util/mesa_cache_db.c
    repo/src/util/mesa_cache_db_multipart.c
)

set(MESA_EXTRA_SOURCES
    repo/src/c11/impl/time.c
    repo/src/c11/impl/threads_posix.c
    repo/src/util/blake3/blake3.c
    repo/src/util/blake3/blake3_dispatch.c
    repo/src/util/blake3/blake3_portable.c
    repo/src/util/u_debug_stack.c
    repo/src/util/u_debug_symbol.c
    repo/src/util/streaming-load-memcpy.c
    repo/src/util/xmlconfig.c
    repo/src/mesa/glapi/shared-glapi/core.c
    repo/src/gallium/winsys/sw/null/null_sw_winsys.c
    repo/src/gallium/winsys/sw/wrapper/wrapper_sw_winsys.c
    repo/src/gallium/winsys/sw/dri/dri_sw_winsys.c
    repo/src/gallium/frontends/dri/dri_context.c
    repo/src/gallium/frontends/dri/dri_drawable.c
    repo/src/gallium/frontends/dri/dri_helpers.c
    repo/src/gallium/frontends/dri/dri_query_renderer.c
    repo/src/gallium/frontends/dri/dri_screen.c
    repo/src/gallium/frontends/dri/dri_util.c
    repo/src/gallium/frontends/dri/dri2.c
    repo/src/gallium/frontends/dri/drisw.c
    repo/src/gallium/frontends/dri/kopper_stubs.c
    repo/src/gallium/auxiliary/pipe-loader/pipe_loader.c
    repo/src/gallium/auxiliary/pipe-loader/pipe_loader_sw.c
    ${PATCHED}/src/loader/loader.c
    repo/src/loader/loader_dri_helper.c
    repo/src/gallium/targets/dri/dri_target.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    list(APPEND MESA_EXTRA_SOURCES repo/src/util/cache_ops_x86.c)
    set(MESA_BLAKE3_DEFINES BLAKE3_NO_SSE2 BLAKE3_NO_SSE41 BLAKE3_NO_AVX2 BLAKE3_NO_AVX512)
else()
    list(APPEND MESA_EXTRA_SOURCES repo/src/util/cache_ops_aarch64.c)
    set(MESA_BLAKE3_DEFINES BLAKE3_USE_NEON=0)
endif()

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Moxnox")
    set(MESA_LLVM_DIR /lib/cmake/llvm)
    set(MESA_LLVM_PREFIX "")
    set(MESA_LLVM_LIBCXX ON)
else()
    if(CMAKE_SYSROOT)
        set(MESA_LLVM_SYSROOT ${CMAKE_SYSROOT})
    elseif(LIBCXX_HEADERS)
        get_filename_component(MESA_LLVM_SYSROOT ${LIBCXX_HEADERS}/../../../..
                               ABSOLUTE)
    else()
        execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-sysroot
            OUTPUT_VARIABLE MESA_LLVM_SYSROOT OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()

    if(EXISTS ${MESA_LLVM_SYSROOT}/usr/lib/llvm/22/lib/cmake/llvm/LLVMConfig.cmake)
        set(MESA_LLVM_DIR ${MESA_LLVM_SYSROOT}/usr/lib/llvm/22/lib/cmake/llvm)
        set(MESA_LLVM_PREFIX ${MESA_LLVM_SYSROOT}/usr)
    elseif(EXISTS ${MESA_LLVM_SYSROOT}/usr/lib/cmake/llvm/LLVMConfig.cmake)
        set(MESA_LLVM_DIR ${MESA_LLVM_SYSROOT}/usr/lib/cmake/llvm)
        set(MESA_LLVM_PREFIX ${MESA_LLVM_SYSROOT}/usr)
    endif()

    if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
        set(MESA_LLVM_LIBCXX OFF)
    else()
        set(MESA_LLVM_LIBCXX ON)
    endif()
endif()

if(EXISTS ${MESA_LLVM_DIR}/LLVMConfig.cmake)
    set(LLVM_DIR ${MESA_LLVM_DIR} CACHE PATH "LLVMConfig.cmake directory" FORCE)
    find_package(LLVM CONFIG REQUIRED PATHS ${MESA_LLVM_DIR} NO_DEFAULT_PATH)

    set(MESA_LLVM_LIB_PATHS "")
    foreach(llvm_lib ${LLVM_AVAILABLE_LIBS})
        if(llvm_lib MATCHES "^LLVM")
            list(APPEND MESA_LLVM_LIB_PATHS ${LLVM_LIBRARY_DIR}/lib${llvm_lib}.a)
        endif()
    endforeach()

    set(MESA_LLVMPIPE ON)
    message(STATUS "mesa: building llvmpipe against LLVM ${LLVM_PACKAGE_VERSION}")
else()
    set(MESA_LLVMPIPE OFF)
    message(STATUS "mesa: llvm-native not built, softpipe only")
endif()

if(MESA_LLVMPIPE)
    set(MESA_GALLIVM_SOURCES
        repo/src/gallium/auxiliary/gallivm/lp_bld_arit.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_arit_overflow.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_assert.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_bitarit.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_const.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_conv.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_coro.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_debug.cpp
        repo/src/gallium/auxiliary/gallivm/lp_bld_flow.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_format_aos_array.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_format_aos.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_format_float.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_format_s3tc.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_format.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_format_soa.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_format_srgb.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_format_yuv.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_gather.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_init_common.c
        ${PATCHED}/src/gallium/auxiliary/gallivm/lp_bld_init_orc.cpp
        repo/src/gallium/auxiliary/gallivm/lp_bld_intr.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_ir_common.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_jit_sample.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_jit_types.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_logic.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_misc.cpp
        repo/src/gallium/auxiliary/gallivm/lp_bld_nir.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_nir_aos.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_nir_soa.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_pack.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_passmgr.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_printf.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_quad.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_sample_aos.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_sample.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_sample_soa.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_struct.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_swizzle.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_tgsi_action.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_tgsi.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_tgsi_info.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_tgsi_soa.c
        repo/src/gallium/auxiliary/gallivm/lp_bld_type.c
        repo/src/gallium/auxiliary/draw/draw_llvm.c
        repo/src/gallium/auxiliary/draw/draw_pt_fetch_shade_pipeline_llvm.c
        repo/src/gallium/auxiliary/draw/draw_vs_llvm.c
        repo/src/gallium/auxiliary/tessellator/tessellator.cpp
        repo/src/gallium/auxiliary/tessellator/p_tessellator.cpp
        repo/src/gallium/auxiliary/nir/nir_to_tgsi_info.c
    )

    set(MESA_LLVMPIPE_SOURCES
        repo/src/gallium/drivers/llvmpipe/lp_bld_alpha.c
        repo/src/gallium/drivers/llvmpipe/lp_bld_blend_aos.c
        repo/src/gallium/drivers/llvmpipe/lp_bld_blend.c
        repo/src/gallium/drivers/llvmpipe/lp_bld_blend_logicop.c
        repo/src/gallium/drivers/llvmpipe/lp_bld_depth.c
        repo/src/gallium/drivers/llvmpipe/lp_bld_interp.c
        repo/src/gallium/drivers/llvmpipe/lp_clear.c
        repo/src/gallium/drivers/llvmpipe/lp_context.c
        repo/src/gallium/drivers/llvmpipe/lp_cs_tpool.c
        repo/src/gallium/drivers/llvmpipe/lp_draw_arrays.c
        repo/src/gallium/drivers/llvmpipe/lp_fence.c
        repo/src/gallium/drivers/llvmpipe/lp_flush.c
        repo/src/gallium/drivers/llvmpipe/lp_jit.c
        repo/src/gallium/drivers/llvmpipe/lp_linear.c
        repo/src/gallium/drivers/llvmpipe/lp_linear_fastpath.c
        repo/src/gallium/drivers/llvmpipe/lp_linear_interp.c
        repo/src/gallium/drivers/llvmpipe/lp_linear_sampler.c
        repo/src/gallium/drivers/llvmpipe/lp_memory.c
        repo/src/gallium/drivers/llvmpipe/lp_perf.c
        repo/src/gallium/drivers/llvmpipe/lp_query.c
        repo/src/gallium/drivers/llvmpipe/lp_rast.c
        repo/src/gallium/drivers/llvmpipe/lp_rast_debug.c
        repo/src/gallium/drivers/llvmpipe/lp_rast_linear.c
        repo/src/gallium/drivers/llvmpipe/lp_rast_linear_fallback.c
        repo/src/gallium/drivers/llvmpipe/lp_rast_rect.c
        repo/src/gallium/drivers/llvmpipe/lp_rast_tri.c
        repo/src/gallium/drivers/llvmpipe/lp_scene.c
        repo/src/gallium/drivers/llvmpipe/lp_scene_queue.c
        repo/src/gallium/drivers/llvmpipe/lp_screen.c
        repo/src/gallium/drivers/llvmpipe/lp_setup.c
        repo/src/gallium/drivers/llvmpipe/lp_setup_analysis.c
        repo/src/gallium/drivers/llvmpipe/lp_setup_line.c
        repo/src/gallium/drivers/llvmpipe/lp_setup_point.c
        repo/src/gallium/drivers/llvmpipe/lp_setup_rect.c
        repo/src/gallium/drivers/llvmpipe/lp_setup_tri.c
        repo/src/gallium/drivers/llvmpipe/lp_setup_vbuf.c
        repo/src/gallium/drivers/llvmpipe/lp_state_blend.c
        repo/src/gallium/drivers/llvmpipe/lp_state_clip.c
        repo/src/gallium/drivers/llvmpipe/lp_state_derived.c
        repo/src/gallium/drivers/llvmpipe/lp_state_cs.c
        repo/src/gallium/drivers/llvmpipe/lp_state_fs.c
        repo/src/gallium/drivers/llvmpipe/lp_state_fs_analysis.c
        repo/src/gallium/drivers/llvmpipe/lp_state_fs_fastpath.c
        repo/src/gallium/drivers/llvmpipe/lp_state_fs_linear.c
        repo/src/gallium/drivers/llvmpipe/lp_state_fs_linear_llvm.c
        repo/src/gallium/drivers/llvmpipe/lp_state_gs.c
        repo/src/gallium/drivers/llvmpipe/lp_state_rasterizer.c
        repo/src/gallium/drivers/llvmpipe/lp_state_sampler.c
        repo/src/gallium/drivers/llvmpipe/lp_state_setup.c
        repo/src/gallium/drivers/llvmpipe/lp_state_so.c
        repo/src/gallium/drivers/llvmpipe/lp_state_surface.c
        repo/src/gallium/drivers/llvmpipe/lp_state_tess.c
        repo/src/gallium/drivers/llvmpipe/lp_state_vertex.c
        repo/src/gallium/drivers/llvmpipe/lp_state_vs.c
        repo/src/gallium/drivers/llvmpipe/lp_surface.c
        repo/src/gallium/drivers/llvmpipe/lp_tex_sample.c
        repo/src/gallium/drivers/llvmpipe/lp_texture.c
        repo/src/gallium/drivers/llvmpipe/lp_texture_handle.c
    )
endif()

# ---------------------------------------------------------------------------

add_library(libgallium SHARED
    ${MESA_UTIL_SOURCES}
    ${MESA_FORMAT_SOURCES}
    ${MESA_COMPILER_SOURCES}
    ${MESA_NIR_SOURCES}
    ${MESA_GLSL_SOURCES}
    repo/src/compiler/glsl/glcpp/pp.c
    ${MESA_VTN_SOURCES}
    ${MESA_CORE_SOURCES}
    ${MESA_GALLIUM_AUX_SOURCES}
    ${MESA_GALLIUM_VL_SOURCES}
    ${MESA_SOFTPIPE_SOURCES}
    ${MESA_GALLIVM_SOURCES}
    ${MESA_LLVMPIPE_SOURCES}
    ${MESA_EXTRA_SOURCES}
    ${MESA_GENERATED_SOURCES}
    ${MESA_GENERATED_HEADERS}
)

set(MESA_COMMON_DEFINES
    PACKAGE_VERSION="26.0.7"
    PACKAGE_BUGREPORT="https://gitlab.freedesktop.org/mesa/mesa/-/issues"
    __STDC_CONSTANT_MACROS
    __STDC_FORMAT_MACROS
    __STDC_LIMIT_MACROS
    HAVE_OPENGL=1
    HAVE_OPENGL_ES_1=0
    HAVE_OPENGL_ES_2=0
    HAVE_DRI
    HAVE_SOFTPIPE
    HAVE_SWRAST
    MESA_SYSTEM_HAS_KMS_DRM=0
    BUILDING_MESA
    USE_LIBGLVND=0
    MESA_DEBUG=0
    GLAPI_EXPORT_PROTO_ENTRY_POINTS=0
    ETIME=ETIMEDOUT
    HAVE_PTHREAD
    HAS_SCHED_H
    USE_GCC_ATOMIC_BUILTINS
    HAVE_UINT128
    HAVE_STRUCT_TIMESPEC
    HAVE_TIMESPEC_GET
    HAVE_ENDIAN_H
    HAVE_DLFCN_H
    HAVE_POLL_H
    HAVE_STRTOF
    HAVE_SYSCONF
    HAVE_POSIX_MEMALIGN
    HAVE_DIRENT_D_TYPE
    HAVE_ZLIB
    HAVE_COMPRESSION
    HAVE___BUILTIN_BSWAP32
    HAVE___BUILTIN_BSWAP64
    HAVE___BUILTIN_CLZ
    HAVE___BUILTIN_CLZLL
    HAVE___BUILTIN_CTZ
    HAVE___BUILTIN_EXPECT
    HAVE___BUILTIN_FFS
    HAVE___BUILTIN_FFSLL
    HAVE___BUILTIN_POPCOUNT
    HAVE___BUILTIN_POPCOUNTLL
    HAVE___BUILTIN_UNREACHABLE
    HAVE___BUILTIN_TYPES_COMPATIBLE_P
    HAVE_FUNC_ATTRIBUTE_CONST
    HAVE_FUNC_ATTRIBUTE_FLATTEN
    HAVE_FUNC_ATTRIBUTE_MALLOC
    HAVE_FUNC_ATTRIBUTE_PURE
    HAVE_FUNC_ATTRIBUTE_UNUSED
    HAVE_FUNC_ATTRIBUTE_WARN_UNUSED_RESULT
    HAVE_FUNC_ATTRIBUTE_FORMAT
    HAVE_FUNC_ATTRIBUTE_PACKED
    HAVE_FUNC_ATTRIBUTE_RETURNS_NONNULL
    HAVE_FUNC_ATTRIBUTE_ALIAS
    HAVE_FUNC_ATTRIBUTE_NORETURN
    HAVE_FUNC_ATTRIBUTE_VISIBILITY
    VIDEO_CODEC_VC1DEC=0
    VIDEO_CODEC_H264DEC=0
    VIDEO_CODEC_H264ENC=0
    VIDEO_CODEC_H265DEC=0
    VIDEO_CODEC_H265ENC=0
    VIDEO_CODEC_AV1DEC=0
    VIDEO_CODEC_AV1ENC=0
    VIDEO_CODEC_VP9DEC=0
    VIDEO_CODEC_JPEGDEC=0
    VIDEO_CODEC_MPEG12DEC=0
    alloca=__builtin_alloca
)

target_compile_definitions(libgallium PRIVATE
    ${MESA_COMMON_DEFINES}
    ${MESA_BLAKE3_DEFINES}
    MESA_IN_LIBGALLIUM
    GALLIUM_SOFTPIPE
    USE_DRICONF
    WITH_XMLCONFIG=0
)

if(MESA_LLVMPIPE)
    target_compile_definitions(libgallium PRIVATE
        GALLIUM_LLVMPIPE
        HAVE_LLVMPIPE
        DRAW_LLVM_AVAILABLE=1
        AMD_LLVM_AVAILABLE=0
        GALLIVM_USE_ORCJIT=1
        LLVM_IS_SHARED=0
        MESA_LLVM_VERSION_STRING="${LLVM_PACKAGE_VERSION}"
    )

    if(NOT MESA_LLVM_LIBCXX)
        set(MESA_GALLIVM_CXX_OPTIONS "-frtti")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(MESA_GALLIVM_CXX_OPTIONS
            "-nostdinc++;-I${MESA_LLVM_PREFIX}/include/c++/v1;-frtti")
    else()
        set(MESA_GALLIVM_CXX_OPTIONS "-stdlib=libc++;-frtti")
    endif()

    set_source_files_properties(
        repo/src/gallium/auxiliary/gallivm/lp_bld_debug.cpp
        repo/src/gallium/auxiliary/gallivm/lp_bld_misc.cpp
        ${PATCHED}/src/gallium/auxiliary/gallivm/lp_bld_init_orc.cpp
        PROPERTIES
        COMPILE_OPTIONS "${MESA_GALLIVM_CXX_OPTIONS}"
        CXX_SCAN_FOR_MODULES OFF)

    target_include_directories(libgallium SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})

    if(MESA_LLVM_LIBCXX)
        target_link_libraries(libgallium PRIVATE
            -Wl,--start-group
            ${MESA_LLVM_LIB_PATHS}
            ${MESA_LLVM_PREFIX}/lib/libc++.a
            ${MESA_LLVM_PREFIX}/lib/libc++abi.a
            -Wl,--end-group
        )

        target_link_options(libgallium PRIVATE -Wl,--allow-multiple-definition)
    else()
        target_link_libraries(libgallium PRIVATE
            -Wl,--start-group
            ${MESA_LLVM_LIB_PATHS}
            -Wl,--end-group
        )
    endif()

    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_options(libgallium PRIVATE -Wl,--exclude-libs,ALL)
    endif()
endif()

set_source_files_properties(repo/src/mesa/glapi/shared-glapi/core.c PROPERTIES
    COMPILE_DEFINITIONS MAPI_MODE_SHARED_GLAPI)
set_source_files_properties(repo/src/util/xmlconfig.c PROPERTIES
    COMPILE_DEFINITIONS "SYSCONFDIR=\"/etc\";DATADIR=\"/usr/share\"")

set(MESA_WNO_FLAGS
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-unused-parameter
    -Wno-missing-field-initializers
    -Wno-unused-but-set-variable
    -Wno-unused-but-set-parameter
    -Wno-unused-function
    -Wno-sign-compare
    -Wno-type-limits
    $<$<C_COMPILER_ID:GNU>:-Wno-calloc-transposed-args>
    -Wno-format-truncation
    -Wno-shift-negative-value
    $<$<C_COMPILER_ID:Clang>:-Wno-gnu-null-pointer-arithmetic -Wno-null-pointer-subtraction>
    -Wno-pointer-arith
    $<$<AND:$<COMPILE_LANGUAGE:C>,$<CXX_COMPILER_ID:GNU>>:-Wno-old-style-declaration>
    $<$<COMPILE_LANGUAGE:C>:-Wno-unterminated-string-initialization>
)

set(MESA_CODEGEN_FLAGS
    -fno-math-errno -fno-trapping-math
    $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CXX_COMPILER_ID:GNU>>:-flifetime-dse=1>
)

target_compile_options(libgallium PRIVATE ${USERSPACE_CFLAGS} ${MESA_WNO_FLAGS} ${MESA_CODEGEN_FLAGS})
target_compile_options(libgallium PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)

target_compile_options(libgallium PRIVATE -fvisibility=default)

target_include_directories(libgallium PUBLIC ${PATCHED}/include)
target_include_directories(libgallium PRIVATE
    ${PATCHED}/src
    ${PATCHED}/src/util
    ${PATCHED}/src/mesa
    ${PATCHED}/src/gallium/include
    ${PATCHED}/src/gallium/auxiliary
)

target_compile_options(libgallium PRIVATE -include ${PATCHED}/src/util/detect_os.h)

target_include_directories(libgallium PUBLIC repo/include)
target_include_directories(libgallium PRIVATE
    repo/src
    ${GEN}/src
    repo/src/util
    ${GEN}/src/util
    repo/src/util/format
    ${GEN}/src/util/format
    repo/src/compiler
    ${GEN}/src/compiler
    repo/src/compiler/nir
    ${GEN}/src/compiler/nir
    repo/src/compiler/glsl
    ${GEN}/src/compiler/glsl
    repo/src/compiler/glsl/glcpp
    ${GEN}/src/compiler/glsl/glcpp
    repo/src/compiler/spirv
    ${GEN}/src/compiler/spirv
    repo/src/mesa
    ${GEN}/src/mesa
    repo/src/mesa/main
    repo/src/mesa/glapi
    ${GEN}/src/mesa/glapi/glapi/gen
    ${GEN}/src/mesa/glapi/shared-glapi
    ${GEN}/src/mesa/program
    repo/src/gallium/include
    repo/src/gallium/auxiliary
    ${GEN}/src/gallium/auxiliary
    repo/src/gallium/auxiliary/util
    repo/src/gallium/drivers
    repo/src/gallium/winsys
    repo/src/gallium/frontends/dri
    repo/src/loader
    repo/src/gallium/auxiliary/driver_ddebug
    repo/src/gallium/auxiliary/gallivm
    repo/src/gallium/auxiliary/rtasm
)

target_link_libraries(libgallium PRIVATE zlib)

# ---------------------------------------------------------------------------

add_library(libEGL SHARED
    ${MESA_EGL_SOURCES}
    repo/src/egl/drivers/dri2/egl_dri2.c
    repo/src/egl/drivers/dri2/platform_device.c
    repo/src/egl/drivers/dri2/platform_surfaceless.c
    ${GEN}/src/mesa/glapi/glapi/gen/dispatch.h
    ${GEN}/src/git_sha1.h
)

target_compile_definitions(libEGL PRIVATE
    ${MESA_COMMON_DEFINES}
    _EGL_NATIVE_PLATFORM=_EGL_PLATFORM_SURFACELESS
)

target_compile_options(libEGL PRIVATE ${USERSPACE_CFLAGS} ${MESA_WNO_FLAGS} ${MESA_CODEGEN_FLAGS})
target_compile_options(libEGL PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>)
target_compile_options(libEGL PRIVATE -fvisibility=default)

target_include_directories(libEGL PRIVATE
    ${PATCHED}/src
    ${PATCHED}/src/util
    ${PATCHED}/src/gallium/include
)
target_compile_options(libEGL PRIVATE -include ${PATCHED}/src/util/detect_os.h)

target_include_directories(libEGL PRIVATE
    repo/src
    ${GEN}/src
    repo/src/egl
    repo/src/egl/main
    repo/src/util
    ${GEN}/src/util
    repo/src/mesa
    ${GEN}/src/mesa/glapi/glapi/gen
    repo/src/gallium/include
    repo/src/gallium/auxiliary
    repo/src/gallium/frontends/dri
    repo/src/loader
)

target_link_libraries(libEGL PUBLIC libgallium)

install(TARGETS libgallium libEGL
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
)

install(FILES ${PATCHED}/include/KHR/khrplatform.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/KHR)
install(FILES
    repo/include/GL/gl.h
    repo/include/GL/glcorearb.h
    repo/include/GL/glext.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/GL
)
install(FILES
    repo/include/EGL/egl.h
    repo/include/EGL/eglext.h
    repo/include/EGL/eglext_angle.h
    repo/include/EGL/eglmesaext.h
    ${PATCHED}/include/EGL/eglplatform.h
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/EGL
)

# ---------------------------------------------------------------------------

add_executable(mesa-test mesa-test.cpp)

target_compile_options(mesa-test PRIVATE -fno-exceptions ${USERSPACE_CFLAGS})
target_link_libraries(mesa-test moxnox print unique_handle evdev)
target_link_libraries(mesa-test libEGL)

if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_link_options(mesa-test PUBLIC -mcrtdll=ucrtbase)
endif()

install(TARGETS mesa-test DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_custom_target(mesa)
add_dependencies(mesa
    libgallium
    libEGL
    mesa-test
)
