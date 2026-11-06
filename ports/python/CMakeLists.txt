set(GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")

function(python_patch relpath)
    add_custom_command(
        OUTPUT ${GEN}/${relpath}
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/repo/${relpath} ${GEN}/${relpath}
        COMMAND ${HOST_SED} -z -i ${ARGN} ${GEN}/${relpath}
        DEPENDS repo/${relpath}
        VERBATIM
    )
endfunction()

add_custom_command(
    OUTPUT ${GEN}/Include/Python.h
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/Include/Python.h ${GEN}/Include/Python.h
    DEPENDS repo/Include/Python.h
    VERBATIM
)

python_patch(Python/pytime.c
    -e [[s|#ifdef MS_WINDOWS\n// Convert PyTime_t to long.\n// Return 0 on success. Return -1 and clamp the value on overflow.|#if defined(MS_WINDOWS) \|\| defined(MOXNOX)\n// Convert PyTime_t to long.\n// Return 0 on success. Return -1 and clamp the value on overflow.|g]]
)

python_patch(Modules/faulthandler.c
    -e [[s|static const size_t faulthandler_nsignals = \\\n    Py_ARRAY_LENGTH(faulthandler_handlers)\;|static const size_t faulthandler_nsignals =\n    sizeof(faulthandler_handlers) / sizeof(faulthandler_handlers[0])\;|g]]
)

python_patch(Modules/termios.c
    -e [[s|#include <sys/ioctl.h>\n||g]]
    -e [[s|#if defined(TIOCGWINSZ)\n    termiosmodulestate \*state = PyModule_GetState(module)\;|#if 1\n    termiosmodulestate *state = PyModule_GetState(module)\;|g]]
    -e [[s|    r = ioctl(fd, TIOCGWINSZ, &w)\;|    r = tcgetwinsize(fd, \&w)\;|g]]
    -e [[s|#if defined(TIOCGWINSZ) && defined(TIOCSWINSZ)|#if 1|g]]
    -e [[s|    if (ioctl(fd, TIOCGWINSZ, &w) == -1) {|    if (tcgetwinsize(fd, \&w) == -1) {|g]]
    -e [[s|    r = ioctl(fd, TIOCSWINSZ, &w)\;|    r = tcsetwinsize(fd, \&w)\;|g]]
)

python_patch(Modules/fcntlmodule.c
    -e [[s|#include <sys/ioctl.h>            // ioctl()\n||g]]
    -e [[s|#define IOCTL_BUFSZ 1024\n|#define IOCTL_BUFSZ 1024\n    PyErr_SetString(PyExc_OSError, "ioctl() is not supported on Moxnox")\;\n    return NULL\;\n#if 0\n|g]]
    -e [[s|    return PyLong_FromLong((long)ret)\;\n#undef IOCTL_BUFSZ|    return PyLong_FromLong((long)ret)\;\n#endif\n#undef IOCTL_BUFSZ|g]]
)

python_patch(Modules/socketmodule.c
    -e [[s|    name = sock_decode_hostname(hbuf)\;|    name = PyUnicode_DecodeFSDefault(hbuf)\;|g]]
)

python_patch(Python/dynload_shlib.c
    -e [[s|#ifdef __CYGWIN__|#if defined(__CYGWIN__) \|\| defined(MOXNOX)|g]]
)

add_custom_target(python_patched_headers DEPENDS
    ${GEN}/Include/Python.h
)

include(ExternalProject)

set(PYTHON_FREEZE_HOST_DIR ${CMAKE_CURRENT_BINARY_DIR}/freeze-host)
set(PYTHON_FREEZE_MODULE ${PYTHON_FREEZE_HOST_DIR}/Programs/_freeze_module${HOST_EXECUTABLE_SUFFIX})

include(ProcessorCount)
ProcessorCount(PYTHON_FREEZE_JOBS)
if(PYTHON_FREEZE_JOBS EQUAL 0)
    set(PYTHON_FREEZE_JOBS 1)
endif()

# FIXME - make's jobserver needs working FIFOs, which Moxnox doesn't have yet
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Moxnox")
    set(PYTHON_FREEZE_JOBS 1)
endif()

execute_process(COMMAND cc -dumpmachine
    OUTPUT_VARIABLE PYTHON_BUILD_TRIPLE OUTPUT_STRIP_TRAILING_WHITESPACE)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Moxnox")
    set(PYTHON_HOST_CONFIGURE_ARGS --with-libm=no --with-mimalloc=no)
else()
    set(PYTHON_HOST_CONFIGURE_ARGS "")
endif()

ExternalProject_Add(python_freeze_host
    SOURCE_DIR        ${CMAKE_CURRENT_SOURCE_DIR}/repo
    BINARY_DIR        ${PYTHON_FREEZE_HOST_DIR}
    BUILD_IN_SOURCE   OFF
    CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env
                          --unset=CFLAGS --unset=CPPFLAGS --unset=LDFLAGS
                          CC=cc CXX=c++ ac_cv_build=${PYTHON_BUILD_TRIPLE}
                          ac_executable_extensions=${HOST_EXECUTABLE_SUFFIX}
                          sh ${CMAKE_CURRENT_SOURCE_DIR}/repo/configure
                          --without-ensurepip --disable-test-modules
                          --with-suffix=${HOST_EXECUTABLE_SUFFIX} ${PYTHON_HOST_CONFIGURE_ARGS}
    BUILD_COMMAND     ${CMAKE_COMMAND} -E env
                          --unset=CFLAGS --unset=CPPFLAGS --unset=LDFLAGS
                          CC=cc CXX=c++ CFLAGS=-g0
                          make -j${PYTHON_FREEZE_JOBS} Programs/_freeze_module
    BUILD_BYPRODUCTS  ${PYTHON_FREEZE_MODULE}
    INSTALL_COMMAND   ""
    USES_TERMINAL_BUILD ON
)

set(PYTHON_FROZEN_MODULES
    "importlib._bootstrap|Lib/importlib/_bootstrap.py"
    "importlib._bootstrap_external|Lib/importlib/_bootstrap_external.py"
    "zipimport|Lib/zipimport.py"
    "abc|Lib/abc.py"
    "codecs|Lib/codecs.py"
    "io|Lib/io.py"
    "_collections_abc|Lib/_collections_abc.py"
    "_sitebuiltins|Lib/_sitebuiltins.py"
    "genericpath|Lib/genericpath.py"
    "ntpath|Lib/ntpath.py"
    "posixpath|Lib/posixpath.py"
    "os|Lib/os.py"
    "site|Lib/site.py"
    "stat|Lib/stat.py"
    "importlib.util|Lib/importlib/util.py"
    "importlib.machinery|Lib/importlib/machinery.py"
    "runpy|Lib/runpy.py"
    "__hello__|Lib/__hello__.py"
    "__phello__|Lib/__phello__/__init__.py"
    "__phello__.ham|Lib/__phello__/ham/__init__.py"
    "__phello__.ham.eggs|Lib/__phello__/ham/eggs.py"
    "__phello__.spam|Lib/__phello__/spam.py"
    "frozen_only|Tools/freeze/flag.py"
    "getpath|Modules/getpath.py"
)

set(PYTHON_FROZEN_HEADERS)
foreach(_entry ${PYTHON_FROZEN_MODULES})
    string(REPLACE "|" ";" _parts "${_entry}")
    list(GET _parts 0 _name)
    list(GET _parts 1 _src)
    set(_out ${GEN}/Python/frozen_modules/${_name}.h)
    add_custom_command(
        OUTPUT ${_out}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN}/Python/frozen_modules
        COMMAND ${PYTHON_FREEZE_MODULE} ${_name}
                ${CMAKE_CURRENT_SOURCE_DIR}/repo/${_src} ${_out}
        DEPENDS ${PYTHON_FREEZE_MODULE}
                ${CMAKE_CURRENT_SOURCE_DIR}/repo/${_src}
        VERBATIM
    )
    list(APPEND PYTHON_FROZEN_HEADERS ${_out})
endforeach()
add_custom_target(python_frozen DEPENDS ${PYTHON_FROZEN_HEADERS})

set(PYTHON_CORE_SOURCES
    repo/Modules/gcmodule.c
    repo/Modules/getbuildinfo.c
    repo/Modules/main.c
    repo/Modules/_hashopenssl.c
    repo/Objects/abstract.c
    repo/Objects/boolobject.c
    repo/Objects/bytearrayobject.c
    repo/Objects/bytes_methods.c
    repo/Objects/bytesobject.c
    repo/Objects/call.c
    repo/Objects/capsule.c
    repo/Objects/cellobject.c
    repo/Objects/classobject.c
    repo/Objects/codeobject.c
    repo/Objects/complexobject.c
    repo/Objects/descrobject.c
    repo/Objects/dictobject.c
    repo/Objects/enumobject.c
    repo/Objects/exceptions.c
    repo/Objects/fileobject.c
    repo/Objects/floatobject.c
    repo/Objects/frameobject.c
    repo/Objects/funcobject.c
    repo/Objects/genericaliasobject.c
    repo/Objects/genobject.c
    repo/Objects/iterobject.c
    repo/Objects/listobject.c
    repo/Objects/longobject.c
    repo/Objects/memoryobject.c
    repo/Objects/methodobject.c
    repo/Objects/moduleobject.c
    repo/Objects/namespaceobject.c
    repo/Objects/object.c
    repo/Objects/obmalloc.c
    repo/Objects/odictobject.c
    repo/Objects/picklebufobject.c
    repo/Objects/rangeobject.c
    repo/Objects/setobject.c
    repo/Objects/sliceobject.c
    repo/Objects/structseq.c
    repo/Objects/tupleobject.c
    repo/Objects/typeobject.c
    repo/Objects/typevarobject.c
    repo/Objects/unicodectype.c
    repo/Objects/unicodeobject.c
    repo/Objects/unionobject.c
    repo/Objects/weakrefobject.c
    repo/Parser/action_helpers.c
    repo/Parser/lexer/buffer.c
    repo/Parser/lexer/lexer.c
    repo/Parser/lexer/state.c
    repo/Parser/myreadline.c
    repo/Parser/parser.c
    repo/Parser/peg_api.c
    repo/Parser/pegen_errors.c
    repo/Parser/pegen.c
    repo/Parser/string_parser.c
    repo/Parser/tokenizer/file_tokenizer.c
    repo/Parser/tokenizer/helpers.c
    repo/Parser/tokenizer/readline_tokenizer.c
    repo/Parser/tokenizer/string_tokenizer.c
    repo/Parser/tokenizer/utf8_tokenizer.c
    repo/Parser/token.c
    repo/Python/asdl.c
    repo/Python/assemble.c
    repo/Python/ast.c
    repo/Python/ast_opt.c
    repo/Python/ast_unparse.c
    repo/Python/bltinmodule.c
    repo/Python/bootstrap_hash.c
    repo/Python/brc.c
    repo/Python/ceval_gil.c
    repo/Python/ceval.c
    repo/Python/codecs.c
    repo/Python/compile.c
    repo/Python/context.c
    repo/Python/critical_section.c
    repo/Python/crossinterp.c
    repo/Python/dtoa.c
    repo/Python/dynamic_annotations.c
    ${GEN}/Python/dynload_shlib.c
    repo/Python/errors.c
    repo/Python/fileutils.c
    repo/Python/flowgraph.c
    repo/Python/formatter_unicode.c
    repo/Python/frame.c
    repo/Python/frozenmain.c
    repo/Python/future.c
    repo/Python/gc_free_threading.c
    repo/Python/gc_gil.c
    repo/Python/gc.c
    repo/Python/getargs.c
    repo/Python/getcompiler.c
    repo/Python/getcopyright.c
    repo/Python/getopt.c
    repo/Python/getplatform.c
    repo/Python/getversion.c
    repo/Python/hamt.c
    repo/Python/hashtable.c
    repo/Python/importdl.c
    repo/Python/import.c
    repo/Python/instruction_sequence.c
    repo/Python/instrumentation.c
    repo/Python/intrinsics.c
    repo/Python/jit.c
    repo/Python/legacy_tracing.c
    repo/Python/lock.c
    repo/Python/marshal.c
    repo/Python/modsupport.c
    repo/Python/mysnprintf.c
    repo/Python/mystrtoul.c
    repo/Python/object_stack.c
    repo/Python/optimizer_analysis.c
    repo/Python/optimizer.c
    repo/Python/optimizer_symbols.c
    repo/Python/parking_lot.c
    repo/Python/pyarena.c
    repo/Python/pyctype.c
    repo/Python/pyfpe.c
    repo/Python/pyhash.c
    repo/Python/pylifecycle.c
    repo/Python/pymath.c
    repo/Python/pystate.c
    repo/Python/pystrcmp.c
    repo/Python/pystrhex.c
    repo/Python/pystrtod.c
    repo/Python/Python-ast.c
    repo/Python/pythonrun.c
    repo/Python/Python-tokenize.c
    ${GEN}/Python/pytime.c
    repo/Python/qsbr.c
    repo/Python/specialize.c
    repo/Python/structmember.c
    repo/Python/suggestions.c
    repo/Python/symtable.c
    repo/Python/sysmodule.c
    repo/Python/thread.c
    repo/Python/traceback.c
    repo/Python/tracemalloc.c
    repo/Python/_warnings.c
    repo/Python/initconfig.c
    repo/Python/interpconfig.c
    repo/Python/pathconfig.c
    repo/Python/perf_trampoline.c
    repo/Python/perf_jit_trampoline.c
    repo/Python/preconfig.c
    repo/Python/frozen.c
)

set(PYTHON_BUILTIN_MODULES
    repo/Modules/_abc.c
    repo/Modules/atexitmodule.c
    repo/Modules/_codecsmodule.c
    repo/Modules/_collectionsmodule.c
    repo/Modules/errnomodule.c
    ${GEN}/Modules/faulthandler.c
    repo/Modules/_functoolsmodule.c
    repo/Modules/_io/bufferedio.c
    repo/Modules/_io/bytesio.c
    repo/Modules/_io/fileio.c
    repo/Modules/_io/iobase.c
    repo/Modules/_io/_iomodule.c
    repo/Modules/_io/stringio.c
    repo/Modules/_io/textio.c
    repo/Modules/_cursesmodule.c
    ${GEN}/Modules/fcntlmodule.c
    repo/Modules/itertoolsmodule.c
    repo/Modules/_localemodule.c
    repo/Modules/_opcode.c
    repo/Modules/_operator.c
    repo/Modules/posixmodule.c
    repo/Modules/pwdmodule.c
    repo/Modules/selectmodule.c
    ${GEN}/Modules/socketmodule.c
    repo/Modules/_ssl.c
    repo/Modules/signalmodule.c
    repo/Modules/_sre/sre.c
    repo/Modules/_stat.c
    repo/Modules/_struct.c
    repo/Modules/_suggestions.c
    repo/Modules/symtablemodule.c
    repo/Modules/_threadmodule.c
    ${GEN}/Modules/termios.c
    repo/Modules/timemodule.c
    repo/Modules/_tracemalloc.c
    repo/Modules/_typingmodule.c
    repo/Modules/unicodedata.c
    repo/Modules/_weakref.c
    repo/Modules/_sysconfig.c
)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(PYTHON_MPD_ASM "ANSI")
else()
    set(PYTHON_MPD_ASM "ASM")
endif()

add_library(libpython SHARED
    ${PYTHON_CORE_SOURCES}
    ${PYTHON_BUILTIN_MODULES}
    config.c
    repo/Modules/getpath.c
    libpython.def
)

add_dependencies(libpython python_frozen python_patched_headers)

set_source_files_properties(repo/Modules/getpath.c PROPERTIES COMPILE_DEFINITIONS
    "PYTHONPATH=\"\";PREFIX=\"/\";EXEC_PREFIX=\"/\";VERSION=\"3.13\";VPATH=\"\";PLATLIBDIR=\"lib\";PYTHONFRAMEWORK=\"\"")

set_source_files_properties(repo/Modules/selectmodule.c PROPERTIES
    COMPILE_OPTIONS "-include;sys/select.h")

set_source_files_properties(${GEN}/Modules/termios.c PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules")

set_source_files_properties(${GEN}/Modules/fcntlmodule.c PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules")

set_source_files_properties(${GEN}/Modules/socketmodule.c PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules")

set_source_files_properties(repo/Modules/_ssl.c PROPERTIES
    COMPILE_DEFINITIONS "OPENSSL_NO_TLS1_METHOD;OPENSSL_NO_TLS1_1_METHOD;OPENSSL_NO_TLS1_2_METHOD")

target_include_directories(libpython PRIVATE
    repo/Include/internal
    ${GEN}
    ${GEN}/Python
    repo
    repo/Modules/_hacl/include
)

target_include_directories(libpython PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/Include
    repo/Include
)

target_compile_definitions(libpython PRIVATE
    Py_BUILD_CORE
    PLATFORM="moxnox"
)

target_compile_options(libpython PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-missing-field-initializers
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-type-limits
    -Wno-sign-compare
    -Wno-format
    -Wno-tautological-constant-out-of-range-compare
)

target_link_libraries(libpython PRIVATE moxnox ncurses libssl libcrypto)

set_target_properties(libpython PROPERTIES ARCHIVE_OUTPUT_NAME python3.13)

install(TARGETS libpython DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(DIRECTORY repo/Include/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include/python3.13
    PATTERN "README.rst" EXCLUDE
    PATTERN "pydtrace.d" EXCLUDE
)
install(FILES pyconfig.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include/python3.13)

###

add_executable(python_exe repo/Programs/python.c)

target_compile_definitions(python_exe PRIVATE Py_BUILD_CORE)

target_compile_options(python_exe PRIVATE ${USERSPACE_CFLAGS})

set_target_properties(python_exe PROPERTIES OUTPUT_NAME python)

target_link_libraries(python_exe PRIVATE libpython moxnox)

add_dependencies(python_exe python_patched_headers)

install(TARGETS python_exe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(CODE "file(CREATE_LINK python.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/python3.exe\" SYMBOLIC)")
install(CODE "file(CREATE_LINK python.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/python3.13.exe\" SYMBOLIC)")

set(prefix "")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(VERSION 3.13)
set(ABIFLAGS "")
set(LIBS "")
set(LIBPYTHON "")
configure_file(repo/Misc/python.pc.in ${GEN}/python-3.13.pc @ONLY)
configure_file(repo/Misc/python-embed.pc.in ${GEN}/python-3.13-embed.pc @ONLY)
install(FILES ${GEN}/python-3.13.pc ${GEN}/python-3.13-embed.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)
install(CODE "file(CREATE_LINK python-3.13.pc \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/python3.pc\" SYMBOLIC)")
install(CODE "file(CREATE_LINK python-3.13-embed.pc \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/pkgconfig/python3-embed.pc\" SYMBOLIC)")

install(DIRECTORY repo/Lib/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13
    PATTERN "__pycache__" EXCLUDE
    PATTERN "test" EXCLUDE
    PATTERN "tests" EXCLUDE)

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13/lib-dynload)

###

function(add_python_module name)
    add_library(python_${name} SHARED ${ARGN})

    target_link_libraries(python_${name} PRIVATE libpython)

    target_include_directories(python_${name} PUBLIC
        repo/Include/internal
        ${GEN}
        ${GEN}/Python
        repo
        repo/Modules/_hacl/include
    )

    target_compile_definitions(python_${name} PRIVATE
        Py_BUILD_CORE
        PLATFORM="moxnox"
    )

    install(TARGETS python_${name}
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python3.13/lib-dynload
        ARCHIVE EXCLUDE_FROM_ALL
    )

    set_target_properties(python_${name} PROPERTIES OUTPUT_NAME ${name})
endfunction()

add_python_module(array repo/Modules/arraymodule.c)
add_python_module(binascii repo/Modules/binascii.c)
add_python_module(_bisect repo/Modules/_bisectmodule.c)
add_python_module(cmath repo/Modules/cmathmodule.c)
add_python_module(_contextvars repo/Modules/_contextvarsmodule.c)
add_python_module(_csv repo/Modules/_csv.c)
add_python_module(_datetime repo/Modules/_datetimemodule.c)
add_python_module(_heapq repo/Modules/_heapqmodule.c)
add_python_module(_json repo/Modules/_json.c)
add_python_module(math repo/Modules/mathmodule.c)

add_python_module(mmap repo/Modules/mmapmodule.c)

set_source_files_properties(repo/Modules/mmapmodule.c PROPERTIES
    COMPILE_DEFINITIONS "my_getallocationgranularity=my_getpagesize")


add_python_module(_pickle repo/Modules/_pickle.c)
add_python_module(_posixsubprocess repo/Modules/_posixsubprocess.c)
add_python_module(_queue repo/Modules/_queuemodule.c)
add_python_module(_random repo/Modules/_randommodule.c)
add_python_module(_statistics repo/Modules/_statisticsmodule.c)
add_python_module(_zoneinfo repo/Modules/_zoneinfo.c)
add_python_module(_asyncio repo/Modules/_asynciomodule.c)
add_python_module(_lsprof repo/Modules/_lsprof.c repo/Modules/rotatingtree.c)

add_python_module(_bz2 repo/Modules/_bz2module.c)
target_link_libraries(python__bz2 PRIVATE bzip2)

add_python_module(_lzma repo/Modules/_lzmamodule.c)
target_link_libraries(python__lzma PRIVATE liblzma)

add_python_module(_curses_panel repo/Modules/_curses_panel.c)
target_link_libraries(python__curses_panel PRIVATE panel ncurses)

add_python_module(zlib repo/Modules/zlibmodule.c)
target_link_libraries(python_zlib PRIVATE zlib)

add_python_module(readline repo/Modules/readline.c)
target_link_libraries(python_readline PRIVATE readline)

add_python_module(_multibytecodec repo/Modules/cjkcodecs/multibytecodec.c)
add_python_module(_codecs_cn repo/Modules/cjkcodecs/_codecs_cn.c)
add_python_module(_codecs_hk repo/Modules/cjkcodecs/_codecs_hk.c)
add_python_module(_codecs_iso2022 repo/Modules/cjkcodecs/_codecs_iso2022.c)
add_python_module(_codecs_jp repo/Modules/cjkcodecs/_codecs_jp.c)
add_python_module(_codecs_kr repo/Modules/cjkcodecs/_codecs_kr.c)
add_python_module(_codecs_tw repo/Modules/cjkcodecs/_codecs_tw.c)
add_python_module(_md5 repo/Modules/md5module.c repo/Modules/_hacl/Hacl_Hash_MD5.c)
add_python_module(_sha1 repo/Modules/sha1module.c repo/Modules/_hacl/Hacl_Hash_SHA1.c)
add_python_module(_sha2 repo/Modules/sha2module.c repo/Modules/_hacl/Hacl_Hash_SHA2.c)
add_python_module(_sha3 repo/Modules/sha3module.c repo/Modules/_hacl/Hacl_Hash_SHA3.c)
add_python_module(_elementtree repo/Modules/_elementtree.c)

add_python_module(pyexpat repo/Modules/pyexpat.c)
target_link_libraries(python_pyexpat PRIVATE expat)
target_link_libraries(python__elementtree PRIVATE expat)

add_python_module(_blake2
    repo/Modules/_blake2/blake2module.c
    repo/Modules/_blake2/blake2b_impl.c
    repo/Modules/_blake2/blake2s_impl.c
)

set_source_files_properties(
    repo/Modules/_blake2/blake2module.c
    repo/Modules/_blake2/blake2b_impl.c
    repo/Modules/_blake2/blake2s_impl.c
    PROPERTIES COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules/_blake2/impl")

set(PYTHON_DECIMAL_SOURCES
    repo/Modules/_decimal/_decimal.c
    repo/Modules/_decimal/libmpdec/basearith.c
    repo/Modules/_decimal/libmpdec/constants.c
    repo/Modules/_decimal/libmpdec/context.c
    repo/Modules/_decimal/libmpdec/convolute.c
    repo/Modules/_decimal/libmpdec/crt.c
    repo/Modules/_decimal/libmpdec/difradix2.c
    repo/Modules/_decimal/libmpdec/fnt.c
    repo/Modules/_decimal/libmpdec/fourstep.c
    repo/Modules/_decimal/libmpdec/io.c
    repo/Modules/_decimal/libmpdec/mpalloc.c
    repo/Modules/_decimal/libmpdec/mpdecimal.c
    repo/Modules/_decimal/libmpdec/mpsignal.c
    repo/Modules/_decimal/libmpdec/numbertheory.c
    repo/Modules/_decimal/libmpdec/sixstep.c
    repo/Modules/_decimal/libmpdec/transpose.c
)
set_source_files_properties(${PYTHON_DECIMAL_SOURCES} PROPERTIES
    COMPILE_DEFINITIONS "CONFIG_64;${PYTHON_MPD_ASM}"
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/repo/Modules/_decimal/libmpdec")

add_python_module(_decimal ${PYTHON_DECIMAL_SOURCES})

add_custom_target(python)
add_dependencies(python
    libpython
    python_exe
    python_array
    python_binascii
    python__bisect
    python_cmath
    python__contextvars
    python__csv
    python__datetime
    python__heapq
    python__json
    python_math
    python_mmap
    python__pickle
    python__posixsubprocess
    python__queue
    python__random
    python__statistics
    python__zoneinfo
    python__asyncio
    python__lsprof
    python__bz2
    python__lzma
    python__curses_panel
    python_zlib
    python_readline
    python__multibytecodec
    python__codecs_cn
    python__codecs_hk
    python__codecs_iso2022
    python__codecs_jp
    python__codecs_kr
    python__codecs_tw
    python__md5
    python__sha1
    python__sha2
    python__sha3
    python__elementtree
    python_pyexpat
    python__blake2
    python__decimal
)
