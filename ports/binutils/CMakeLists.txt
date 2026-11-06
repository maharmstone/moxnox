project(binutils VERSION 2.47)

set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)

find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

set(LIBIBERTY_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/libiberty-gen)
file(MAKE_DIRECTORY ${LIBIBERTY_GEN_DIR})

# lrealpath.c: apply upstream bug fix (sequential #if fallbacks should be #elif)
add_custom_command(
    OUTPUT ${LIBIBERTY_GEN_DIR}/lrealpath.c
    COMMAND sh -c "cp '${REPO}/libiberty/lrealpath.c' '${LIBIBERTY_GEN_DIR}/lrealpath.c' && patch '${LIBIBERTY_GEN_DIR}/lrealpath.c' < '${CMAKE_CURRENT_SOURCE_DIR}/lrealpath.patch'"
    VERBATIM
    DEPENDS ${REPO}/libiberty/lrealpath.c ${CMAKE_CURRENT_SOURCE_DIR}/lrealpath.patch
)

# libiberty

add_library(libiberty STATIC
    ${REPO}/libiberty/cplus-dem.c
    ${REPO}/libiberty/cp-demangle.c
    ${REPO}/libiberty/md5.c
    ${REPO}/libiberty/sha1.c
    ${REPO}/libiberty/alloca.c
    ${REPO}/libiberty/asprintf.c
    ${REPO}/libiberty/argv.c
    ${REPO}/libiberty/bsearch_r.c
    ${REPO}/libiberty/choose-temp.c
    ${REPO}/libiberty/concat.c
    ${REPO}/libiberty/cp-demint.c
    ${REPO}/libiberty/crc32.c
    ${REPO}/libiberty/d-demangle.c
    ${REPO}/libiberty/dwarfnames.c
    ${REPO}/libiberty/dyn-string.c
    ${REPO}/libiberty/fdmatch.c
    ${REPO}/libiberty/fibheap.c
    ${REPO}/libiberty/filedescriptor.c
    ${REPO}/libiberty/filename_cmp.c
    ${REPO}/libiberty/floatformat.c
    ${REPO}/libiberty/fnmatch.c
    ${REPO}/libiberty/fopen_unlocked.c
    ${REPO}/libiberty/getopt.c
    ${REPO}/libiberty/getopt1.c
    ${REPO}/libiberty/getpwd.c
    ${REPO}/libiberty/getruntime.c
    ${REPO}/libiberty/hashtab.c
    ${REPO}/libiberty/hex.c
    ${REPO}/libiberty/lbasename.c
    ${REPO}/libiberty/ldirname.c
    ${LIBIBERTY_GEN_DIR}/lrealpath.c
    ${REPO}/libiberty/make-relative-prefix.c
    ${REPO}/libiberty/make-temp-file.c
    ${REPO}/libiberty/mempcpy.c
    ${REPO}/libiberty/objalloc.c
    ${REPO}/libiberty/obstack.c
    ${REPO}/libiberty/partition.c
    ${REPO}/libiberty/pexecute.c
    ${REPO}/libiberty/physmem.c
    ${REPO}/libiberty/pex-common.c
    ${REPO}/libiberty/pex-one.c
    ${REPO}/libiberty/pex-unix.c
    ${REPO}/libiberty/vprintf-support.c
    ${REPO}/libiberty/rust-demangle.c
    ${REPO}/libiberty/safe-ctype.c
    ${REPO}/libiberty/simple-object.c
    ${REPO}/libiberty/simple-object-coff.c
    ${REPO}/libiberty/simple-object-elf.c
    ${REPO}/libiberty/simple-object-mach-o.c
    ${REPO}/libiberty/simple-object-xcoff.c
    ${REPO}/libiberty/sort.c
    ${REPO}/libiberty/spaces.c
    ${REPO}/libiberty/splay-tree.c
    ${REPO}/libiberty/stack-limit.c
    ${REPO}/libiberty/strsignal.c
    ${REPO}/libiberty/timeval-utils.c
    ${REPO}/libiberty/unlink-if-ordinary.c
    ${REPO}/libiberty/vasprintf.c
    ${REPO}/libiberty/xasprintf.c
    ${REPO}/libiberty/xatexit.c
    ${REPO}/libiberty/xexit.c
    ${REPO}/libiberty/xmalloc.c
    ${REPO}/libiberty/xmemdup.c
    ${REPO}/libiberty/xstrdup.c
    ${REPO}/libiberty/xstrerror.c
    ${REPO}/libiberty/xstrndup.c
    ${REPO}/libiberty/xvasprintf.c
    ${REPO}/libiberty/basename.c
    ${REPO}/libiberty/bcmp.c
    ${REPO}/libiberty/bcopy.c
    ${REPO}/libiberty/bzero.c
    ${REPO}/libiberty/ffs.c
    ${REPO}/libiberty/getpagesize.c
    ${REPO}/libiberty/index.c
    ${REPO}/libiberty/insque.c
    ${REPO}/libiberty/memmem.c
    ${REPO}/libiberty/memrchr.c
    ${REPO}/libiberty/mkstemps.c
    ${REPO}/libiberty/random.c
    ${REPO}/libiberty/rindex.c
    ${REPO}/libiberty/sigsetmask.c
    ${REPO}/libiberty/stpcpy.c
    ${REPO}/libiberty/stpncpy.c
    ${REPO}/libiberty/strndup.c
    ${REPO}/libiberty/strverscmp.c
    ${REPO}/libiberty/waitpid.c
    ${REPO}/libiberty/setproctitle.c
)

target_compile_options(libiberty PRIVATE ${GENERAL_CFLAGS}
    -std=gnu11
    -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
)
target_include_directories(libiberty PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}   # config.h
    ${REPO}/include               # libiberty.h, ansidecl.h, demangle.h, getopt.h, etc.
    ${REPO}/libiberty             # pex-common.h, simple-object-common.h, etc.
)
target_include_directories(libiberty PUBLIC
    ${REPO}/include
)

target_compile_definitions(libiberty PRIVATE
    _CRTIMP=
    HAVE_CONFIG_H
    PACKAGE_NAME=libiberty
    "PACKAGE_STRING=\"libiberty ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=libiberty
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

# bfd

set(BFD_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/bfd-gen)
file(MAKE_DIRECTORY ${BFD_GEN_DIR})

# bfd.h - generated from bfd-in2.h
set(wordsize 64)
set(bfd_default_target_size 64)
set(bfd_file_ptr int64_t)
set(bfd_ufile_ptr uint64_t)
set(supports_plugins 0)
configure_file(${REPO}/bfd/bfd-in2.h ${BFD_GEN_DIR}/bfd.h @ONLY)

# bfdver.h - generated from version.h
set(bfd_version 247000000) # FIXME
set(bfd_version_string "\"${PROJECT_VERSION}\"")
set(bfd_version_package "\"(GNU Binutils) \"")
set(report_bugs_to "\"https://sourceware.org/bugzilla/\"")
configure_file(${REPO}/bfd/version.h ${BFD_GEN_DIR}/bfdver.h @ONLY)

add_custom_command(
    OUTPUT ${BFD_GEN_DIR}/elf32-target.h
    COMMAND sh -c "sed -e 's/NN/32/g' '${REPO}/bfd/elfxx-target.h' > '${BFD_GEN_DIR}/elf32-target.h'"
    VERBATIM
    DEPENDS ${REPO}/bfd/elfxx-target.h
)

add_custom_command(
    OUTPUT ${BFD_GEN_DIR}/elf64-target.h
    COMMAND sh -c "sed -e 's/NN/64/g' '${REPO}/bfd/elfxx-target.h' > '${BFD_GEN_DIR}/elf64-target.h'"
    VERBATIM
    DEPENDS ${REPO}/bfd/elfxx-target.h
)

add_custom_command(
    OUTPUT ${BFD_GEN_DIR}/peigen.c
    COMMAND sh -c "{ printf '#line 1 \"peXXigen.c\"\\n' && sed -e 's/XX/pe/g' '${REPO}/bfd/peXXigen.c'; } > '${BFD_GEN_DIR}/peigen.c'"
    VERBATIM
    DEPENDS ${REPO}/bfd/peXXigen.c
)

add_custom_command(
    OUTPUT ${BFD_GEN_DIR}/pex64igen.c
    COMMAND sh -c "{ printf '#line 1 \"peXXigen.c\"\\n' && sed -e 's/XX/pex64/g' '${REPO}/bfd/peXXigen.c'; } > '${BFD_GEN_DIR}/pex64igen.c'"
    VERBATIM
    DEPENDS ${REPO}/bfd/peXXigen.c
)

add_custom_command(
    OUTPUT ${BFD_GEN_DIR}/config.bfd
    COMMAND sh -c "cp '${REPO}/bfd/config.bfd' '${BFD_GEN_DIR}/config.bfd' && patch '${BFD_GEN_DIR}/config.bfd' < '${CMAKE_CURRENT_SOURCE_DIR}/config.bfd.patch'"
    VERBATIM
    DEPENDS ${REPO}/bfd/config.bfd ${CMAKE_CURRENT_SOURCE_DIR}/config.bfd.patch
)

add_custom_command(
    OUTPUT ${BFD_GEN_DIR}/targmatch.h
    COMMAND sh -c "sed -f '${REPO}/bfd/targmatch.sed' '${BFD_GEN_DIR}/config.bfd' > '${BFD_GEN_DIR}/targmatch.h'"
    VERBATIM
    DEPENDS ${BFD_GEN_DIR}/config.bfd ${REPO}/bfd/targmatch.sed
)

add_custom_target(bfd_gen_headers DEPENDS
    ${BFD_GEN_DIR}/elf32-target.h
    ${BFD_GEN_DIR}/elf64-target.h
    ${BFD_GEN_DIR}/targmatch.h
)

add_library(bfd STATIC
    ${REPO}/bfd/archive.c
    ${REPO}/bfd/archures.c
    ${REPO}/bfd/bfd.c
    ${REPO}/bfd/bfdio.c
    ${REPO}/bfd/cache.c
    ${REPO}/bfd/coff-bfd.c
    ${REPO}/bfd/compress.c
    ${REPO}/bfd/corefile.c
    ${REPO}/bfd/format.c
    ${REPO}/bfd/hash.c
    ${REPO}/bfd/libbfd.c
    ${REPO}/bfd/linker.c
    ${REPO}/bfd/merge.c
    ${REPO}/bfd/opncls.c
    ${REPO}/bfd/reloc.c
    ${REPO}/bfd/section.c
    ${REPO}/bfd/simple.c
    ${REPO}/bfd/stab-syms.c
    ${REPO}/bfd/stabs.c
    ${REPO}/bfd/syms.c
    ${REPO}/bfd/targets.c
    ${REPO}/bfd/binary.c
    ${REPO}/bfd/ihex.c
    ${REPO}/bfd/srec.c
    ${REPO}/bfd/tekhex.c
    ${REPO}/bfd/verilog.c
    ${REPO}/bfd/archive64.c
    ${REPO}/bfd/cpu-i386.c
    ${REPO}/bfd/coffgen.c
    ${REPO}/bfd/cofflink.c
    ${REPO}/bfd/dwarf1.c
    ${REPO}/bfd/dwarf2.c
    ${REPO}/bfd/pe-x86_64.c
    ${REPO}/bfd/pei-x86_64.c
    ${BFD_GEN_DIR}/pex64igen.c
    ${REPO}/bfd/pe-i386.c
    ${REPO}/bfd/pei-i386.c
    ${BFD_GEN_DIR}/peigen.c
    ${REPO}/bfd/elf.c
    ${REPO}/bfd/elflink.c
    ${REPO}/bfd/elf-strtab.c
    ${REPO}/bfd/elf-eh-frame.c
    ${REPO}/bfd/elf-properties.c
    ${REPO}/bfd/elf-ifunc.c
    ${REPO}/bfd/elf-sframe.c
    ${REPO}/bfd/elfxx-x86.c
    ${REPO}/bfd/elf32-i386.c
    ${REPO}/bfd/elf32.c
    ${REPO}/bfd/elf64-x86-64.c
    ${REPO}/bfd/elf64.c
    ${REPO}/bfd/pdb.c
)

add_dependencies(bfd bfd_gen_headers)

target_compile_options(bfd PRIVATE ${GENERAL_CFLAGS}
    -std=gnu11
    -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
    -Wno-format
)

target_compile_definitions(bfd PRIVATE
    _CRTIMP=
    HAVE_CONFIG_H
    DEFAULT_VECTOR=x86_64_pe_vec
    "SELECT_VECS=&x86_64_pe_vec,&x86_64_pei_vec,&x86_64_pe_big_vec,&x86_64_elf64_vec,&i386_pe_vec,&i386_pe_big_vec,&i386_pei_vec,&i386_elf32_vec,&pdb_vec"
    HAVE_x86_64_pe_vec
    HAVE_x86_64_pei_vec
    HAVE_x86_64_pe_big_vec
    HAVE_x86_64_elf64_vec
    HAVE_i386_pe_vec
    HAVE_i386_pe_big_vec
    HAVE_i386_pei_vec
    HAVE_i386_elf32_vec
    HAVE_pdb_vec
    "SELECT_ARCHITECTURES=&bfd_i386_arch"
    OBJ_MAYBE_ELF
    OBJ_MAYBE_ELF_SFRAME
    "DEBUGDIR=\"/lib/debug\""
    PACKAGE_NAME=bfd
    "PACKAGE_STRING=\"bfd ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=bfd
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

target_include_directories(bfd PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BFD_GEN_DIR}
    ${REPO}/bfd
    ${REPO}/include
)
target_include_directories(bfd PUBLIC
    ${BFD_GEN_DIR}
    ${REPO}/include
)

target_link_libraries(bfd PUBLIC libiberty zlib zstd)

# opcodes

add_library(opcodes STATIC
    ${REPO}/opcodes/dis-buf.c
    ${REPO}/opcodes/disassemble.c
    ${REPO}/opcodes/dis-init.c
    ${REPO}/opcodes/i386-dis.c
)

target_compile_options(opcodes PRIVATE ${GENERAL_CFLAGS}
    -std=gnu11
    -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
)

target_include_directories(opcodes PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BFD_GEN_DIR}
    ${REPO}/opcodes
    ${REPO}/bfd
    ${REPO}/include
)

target_compile_definitions(opcodes PRIVATE
    _CRTIMP=
    HAVE_CONFIG_H
    ARCH_i386
    PACKAGE_NAME=opcodes
    "PACKAGE_STRING=\"opcodes ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=opcodes
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

target_link_libraries(opcodes PUBLIC bfd libiberty)

# libsframe

add_library(sframe STATIC
    ${REPO}/libsframe/sframe.c
    ${REPO}/libsframe/sframe-dump.c
    ${REPO}/libsframe/sframe-error.c
)

target_compile_options(sframe PRIVATE ${GENERAL_CFLAGS}
    -std=gnu11
    -Wno-format
    -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
)

target_include_directories(sframe PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REPO}/libsframe
    ${REPO}/libctf
    ${REPO}/include
)

target_compile_definitions(sframe PRIVATE
    _CRTIMP=
    HAVE_CONFIG_H
    PACKAGE_NAME=libsframe
    "PACKAGE_STRING=\"libsframe ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=libsframe
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

target_link_libraries(bfd PUBLIC sframe)

# binutils tools

set(BINUTILS_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/binutils-gen)
file(MAKE_DIRECTORY ${BINUTILS_GEN_DIR})

bison_target(arparse
    ${REPO}/binutils/arparse.y
    ${BINUTILS_GEN_DIR}/arparse.c
    DEFINES_FILE ${BINUTILS_GEN_DIR}/arparse.h
)
flex_target(arlex
    ${REPO}/binutils/arlex.l
    ${BINUTILS_GEN_DIR}/arlex.c
)
add_flex_bison_dependency(arlex arparse)

set(BULIBS
    ${REPO}/binutils/bucomm.c
    ${REPO}/binutils/version.c
    ${REPO}/binutils/filemode.c
)
set(DEBUG_SRCS
    ${REPO}/binutils/rddbg.c
    ${REPO}/binutils/debug.c
    ${REPO}/binutils/stabs.c
    ${REPO}/binutils/rdcoff.c
)
set(WRITE_DEBUG_SRCS
    ${DEBUG_SRCS}
    ${REPO}/binutils/wrstabs.c
)
set(AR_SRCS
    ${BISON_arparse_OUTPUTS}
    ${FLEX_arlex_OUTPUTS}
    ${REPO}/binutils/arsup.c
    ${REPO}/binutils/rename.c
    ${REPO}/binutils/binemul.c
    ${REPO}/binutils/emul_vanilla.c
    ${BULIBS}
)

function(add_binutils_exe name)
    target_compile_options(${name} PRIVATE ${GENERAL_CFLAGS}
        -std=gnu11
        -Wno-format
        -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
    )
    target_include_directories(${name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${BFD_GEN_DIR}
        ${BINUTILS_GEN_DIR}
        ${REPO}/binutils
        ${REPO}/bfd
        ${REPO}/include
    )
    target_compile_definitions(${name} PRIVATE
        _CRTIMP=
        HAVE_CONFIG_H
        "TARGET=\"x86_64-pc-moxnox\""
        PACKAGE_NAME=binutils
        "PACKAGE_STRING=\"binutils ${PROJECT_VERSION}\""
        PACKAGE_TARNAME=binutils
        "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
        "VERSION=\"${PROJECT_VERSION}\""
    )
    target_link_libraries(${name} PRIVATE bfd libiberty zstd)

    install(TARGETS ${name} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        ARCHIVE EXCLUDE_FROM_ALL
    )
endfunction()

add_executable(addr2line
    ${REPO}/binutils/addr2line.c
    ${BULIBS}
)
add_binutils_exe(addr2line)

add_executable(nm
    ${REPO}/binutils/nm.c
    ${REPO}/binutils/demanguse.c
    ${BULIBS}
)
add_binutils_exe(nm)

add_executable(size
    ${REPO}/binutils/size.c
    ${BULIBS}
)
add_binutils_exe(size)

add_executable(strings
    ${REPO}/binutils/strings.c
    ${BULIBS}
)
add_binutils_exe(strings)

add_executable(objdump
    ${REPO}/binutils/objdump.c
    ${REPO}/binutils/dwarf.c
    ${REPO}/binutils/prdbg.c
    ${REPO}/binutils/demanguse.c
    ${DEBUG_SRCS}
    ${BULIBS}
    ${REPO}/binutils/elfcomm.c
    ${REPO}/binutils/od-pe.c
)
add_binutils_exe(objdump)

target_compile_definitions(objdump PRIVATE
    HAVE_CONFIG_H
    "OBJDUMP_PRIVATE_VECTORS=&objdump_private_desc_pe,"
    PACKAGE_NAME=binutils
    "PACKAGE_STRING=\"binutils ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=binutils
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

target_link_libraries(objdump PRIVATE opcodes)

add_executable(objcopy
    ${REPO}/binutils/not-strip.c
    ${REPO}/binutils/rename.c
    ${WRITE_DEBUG_SRCS}
    ${BULIBS}
)
add_binutils_exe(objcopy)

add_executable(strip
    ${REPO}/binutils/is-strip.c
    ${REPO}/binutils/rename.c
    ${WRITE_DEBUG_SRCS}
    ${BULIBS}
)
add_binutils_exe(strip)

add_executable(ar
    ${AR_SRCS}
    ${REPO}/binutils/not-ranlib.c
)
add_binutils_exe(ar)

target_compile_definitions(ar PRIVATE
    HAVE_CONFIG_H
    bin_dummy_emulation=bin_vanilla_emulation
    PACKAGE_NAME=binutils
    "PACKAGE_STRING=\"binutils ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=binutils
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

add_executable(ranlib
    ${AR_SRCS}
    ${REPO}/binutils/is-ranlib.c
)
add_binutils_exe(ranlib)

target_compile_definitions(ranlib PRIVATE
    HAVE_CONFIG_H
    bin_dummy_emulation=bin_vanilla_emulation
    PACKAGE_NAME=binutils
    "PACKAGE_STRING=\"binutils ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=binutils
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

add_executable(readelf
    ${REPO}/binutils/readelf.c
    ${REPO}/binutils/version.c
    ${REPO}/binutils/unwind-ia64.c
    ${REPO}/binutils/dwarf.c
    ${REPO}/binutils/demanguse.c
    ${REPO}/binutils/elfcomm.c
)
target_compile_options(readelf PRIVATE ${GENERAL_CFLAGS}
    -std=gnu11
    -Wno-format
    -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
)

target_compile_definitions(readelf PRIVATE
    _CRTIMP=
    HAVE_CONFIG_H
    "TARGET=\"x86_64-pc-moxnox\""
    PACKAGE_NAME=binutils
    "PACKAGE_STRING=\"binutils ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=binutils
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

target_include_directories(readelf PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BFD_GEN_DIR}
    ${REPO}/binutils
    ${REPO}/bfd
    ${REPO}/include
)
target_link_libraries(readelf PRIVATE libiberty zlib sframe zstd)

install(TARGETS readelf DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    ARCHIVE EXCLUDE_FROM_ALL
)

# gas

add_executable(as
    ${REPO}/gas/app.c
    ${REPO}/gas/as.c
    ${REPO}/gas/atof-generic.c
    ${REPO}/gas/codeview.c
    ${REPO}/gas/compress-debug.c
    ${REPO}/gas/cond.c
    ${REPO}/gas/depend.c
    ${REPO}/gas/dwarf2dbg.c
    ${REPO}/gas/dw2gencfi.c
    ${REPO}/gas/ecoff.c
    ${REPO}/gas/ehopt.c
    ${REPO}/gas/expr.c
    ${REPO}/gas/flonum-copy.c
    ${REPO}/gas/flonum-konst.c
    ${REPO}/gas/flonum-mult.c
    ${REPO}/gas/frags.c
    ${REPO}/gas/gen-sframe.c
    ${REPO}/gas/ginsn.c
    ${REPO}/gas/hash.c
    ${REPO}/gas/input-file.c
    ${REPO}/gas/input-scrub.c
    ${REPO}/gas/listing.c
    ${REPO}/gas/literal.c
    ${REPO}/gas/macro.c
    ${REPO}/gas/messages.c
    ${REPO}/gas/output-file.c
    ${REPO}/gas/read.c
    ${REPO}/gas/remap.c
    ${REPO}/gas/sb.c
    ${REPO}/gas/scfidw2gen.c
    ${REPO}/gas/scfi.c
    ${REPO}/gas/sframe-opt.c
    ${REPO}/gas/stabs.c
    ${REPO}/gas/subsegs.c
    ${REPO}/gas/symbols.c
    ${REPO}/gas/write.c
    ${REPO}/gas/config/tc-i386.c
    ${REPO}/gas/config/obj-coff.c
    ${REPO}/gas/config/atof-ieee.c
)
target_compile_options(as PRIVATE ${GENERAL_CFLAGS}
    -std=gnu11
    -Wno-format
    -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
)
target_compile_definitions(as PRIVATE
    _CRTIMP=
    HAVE_CONFIG_H
    "DEFAULT_ARCH=\"x86_64\""
    PACKAGE_NAME=gas
    "PACKAGE_STRING=\"gas ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=gas
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)
target_include_directories(as PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REPO}/gas
    ${REPO}/gas/config
    ${REPO}/include
    ${REPO}/bfd
    ${REPO}
    ${BFD_GEN_DIR}
)
target_link_libraries(as PRIVATE bfd opcodes libiberty zlib zstd)

install(TARGETS as DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    ARCHIVE EXCLUDE_FROM_ALL
)

# ld

set(LD_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/ld-gen)
file(MAKE_DIRECTORY ${LD_GEN_DIR})
file(MAKE_DIRECTORY ${LD_GEN_DIR}/ldscripts)

bison_target(ldgram
    ${REPO}/ld/ldgram.y
    ${LD_GEN_DIR}/ldgram.c
    DEFINES_FILE ${LD_GEN_DIR}/ldgram.h
)
bison_target(deffilep
    ${REPO}/ld/deffilep.y
    ${LD_GEN_DIR}/deffilep.c
    DEFINES_FILE ${LD_GEN_DIR}/deffilep.h
    OPTIONS -Wno-conflicts-sr
)
flex_target(ldlex
    ${REPO}/ld/ldlex.l
    ${LD_GEN_DIR}/ldlex.c
)
add_flex_bison_dependency(ldlex ldgram)

set_source_files_properties(${FLEX_ldlex_OUTPUTS} PROPERTIES HEADER_FILE_ONLY TRUE)

add_custom_command(
    OUTPUT ${LD_GEN_DIR}/ei386pep.c
    COMMAND sh ${REPO}/ld/genscripts.sh
        ${REPO}/ld /usr/lib /usr /usr
        x86_64-pc-moxnox x86_64-pc-moxnox x86_64-pc-moxnox
        "" "" "i386pep i386pe elf_i386" "" no "" i386pep ""
    WORKING_DIRECTORY ${LD_GEN_DIR}
    VERBATIM
    DEPENDS
        ${REPO}/ld/genscripts.sh
        ${REPO}/ld/emulparams/i386pep.sh
        ${REPO}/ld/emultempl/pep.em
        ${REPO}/ld/scripttempl/pe.sc
)

add_custom_command(
    OUTPUT ${LD_GEN_DIR}/ei386pe.c
    COMMAND sh ${REPO}/ld/genscripts.sh
        ${REPO}/ld /usr/lib /usr /usr
        x86_64-pc-moxnox x86_64-pc-moxnox x86_64-pc-moxnox
        "" "" "i386pep i386pe elf_i386" "" no "" i386pe ""
    WORKING_DIRECTORY ${LD_GEN_DIR}
    VERBATIM
    DEPENDS
        ${REPO}/ld/genscripts.sh
        ${REPO}/ld/emulparams/i386pe.sh
        ${REPO}/ld/emultempl/pe.em
        ${REPO}/ld/scripttempl/pe.sc
)

add_custom_command(
    OUTPUT ${LD_GEN_DIR}/eelf_i386.c
    COMMAND sh ${REPO}/ld/genscripts.sh
        ${REPO}/ld /usr/lib /usr /usr
        x86_64-pc-moxnox x86_64-pc-moxnox x86_64-pc-moxnox
        "" "" "i386pep i386pe elf_i386" "" no "" elf_i386 ""
    WORKING_DIRECTORY ${LD_GEN_DIR}
    VERBATIM
    DEPENDS
        ${REPO}/ld/genscripts.sh
        ${REPO}/ld/emulparams/elf_i386.sh
        ${REPO}/ld/emultempl/elf.em
        ${REPO}/ld/emultempl/elf-i386-glibc.em
        ${REPO}/ld/scripttempl/elf.sc
)

add_executable(ld
    ${BISON_ldgram_OUTPUTS}
    ${FLEX_ldlex_OUTPUTS}
    ${REPO}/ld/ldlex-wrapper.c
    ${REPO}/ld/lexsup.c
    ${REPO}/ld/ldlang.c
    ${REPO}/ld/mri.c
    ${REPO}/ld/ldctor.c
    ${REPO}/ld/ldmain.c
    ${REPO}/ld/ldwrite.c
    ${REPO}/ld/ldexp.c
    ${REPO}/ld/ldemul.c
    ${REPO}/ld/ldver.c
    ${REPO}/ld/ldmisc.c
    ${REPO}/ld/ldfile.c
    ${REPO}/ld/ldcref.c
    ${REPO}/ld/plugin.c
    ${REPO}/ld/ldbuildid.c
    ${REPO}/ld/ldelf.c
    ${REPO}/ld/ldelfgen.c
    ${LD_GEN_DIR}/ei386pep.c
    ${LD_GEN_DIR}/ei386pe.c
    ${LD_GEN_DIR}/eelf_i386.c
    ${BISON_deffilep_OUTPUTS}
    ${REPO}/ld/pdb.c
    ${REPO}/ld/pep-dll.c
    ${REPO}/ld/pe-dll.c
)
target_compile_options(ld PRIVATE ${GENERAL_CFLAGS}
    -std=gnu11
    -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
)
target_compile_definitions(ld PRIVATE
    _CRTIMP=
    HAVE_CONFIG_H
    HAVE_DLFCN_H=1
    "DEFAULT_EMULATION=\"i386pep\""
    "EMULATION_LIBPATH=\"i386pep i386pe elf_i386\""
    "SCRIPTDIR=\"/lib/ldscripts\""
    "BINDIR=\"/bin\""
    "TOOLBINDIR=\"/usr/x86_64-pc-moxnox/bin\""
    "TARGET=\"x86_64-pc-moxnox\""
    PACKAGE_NAME=ld
    "PACKAGE_STRING=\"ld ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=ld
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)
target_include_directories(ld PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${LD_GEN_DIR}
    ${BFD_GEN_DIR}
    ${REPO}/ld
    ${REPO}/bfd
    ${REPO}/include
)
target_link_libraries(ld PRIVATE bfd opcodes libiberty zlib zstd)

install(TARGETS ld DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
    ARCHIVE EXCLUDE_FROM_ALL
)

# dlltool

set(DLLTOOL_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/dlltool-gen)
file(MAKE_DIRECTORY ${DLLTOOL_GEN_DIR})

bison_target(defparse
    ${REPO}/binutils/defparse.y
    ${DLLTOOL_GEN_DIR}/defparse.c
    DEFINES_FILE ${DLLTOOL_GEN_DIR}/defparse.h
    OPTIONS -Wno-conflicts-sr
)
flex_target(deflex
    ${REPO}/binutils/deflex.l
    ${DLLTOOL_GEN_DIR}/deflex.c
)
add_flex_bison_dependency(deflex defparse)

add_executable(dlltool
    ${REPO}/binutils/dlltool.c
    ${BISON_defparse_OUTPUTS}
    ${FLEX_deflex_OUTPUTS}
    ${BULIBS}
)
add_binutils_exe(dlltool)

target_compile_definitions(dlltool PRIVATE
    HAVE_CONFIG_H
    DLLTOOL_MX86_64
    DLLTOOL_DEFAULT_MX86_64
    PACKAGE_NAME=binutils
    "PACKAGE_STRING=\"binutils ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=binutils
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

target_include_directories(dlltool PRIVATE
    ${DLLTOOL_GEN_DIR}
)

# windres

set(WINDRES_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/windres-gen)
file(MAKE_DIRECTORY ${WINDRES_GEN_DIR})

bison_target(rcparse
    ${REPO}/binutils/rcparse.y
    ${WINDRES_GEN_DIR}/rcparse.c
    DEFINES_FILE ${WINDRES_GEN_DIR}/rcparse.h
    OPTIONS -Wno-conflicts-sr -Wno-conflicts-rr
)

add_executable(windres
    ${REPO}/binutils/windres.c
    ${REPO}/binutils/resrc.c
    ${REPO}/binutils/rescoff.c
    ${REPO}/binutils/resbin.c
    ${BISON_rcparse_OUTPUTS}
    ${REPO}/binutils/rclex.c
    ${REPO}/binutils/winduni.c
    ${REPO}/binutils/resres.c
    ${BULIBS}
)

add_binutils_exe(windres)

target_link_libraries(windres PRIVATE libiconv)

target_include_directories(windres PRIVATE
    ${WINDRES_GEN_DIR}
)

# windmc

set(WINDMC_GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/windmc-gen)
file(MAKE_DIRECTORY ${WINDMC_GEN_DIR})

bison_target(mcparse
    ${REPO}/binutils/mcparse.y
    ${WINDMC_GEN_DIR}/mcparse.c
    DEFINES_FILE ${WINDMC_GEN_DIR}/mcparse.h
    OPTIONS -Wno-conflicts-sr
)

add_executable(windmc
    ${REPO}/binutils/windmc.c
    ${BISON_mcparse_OUTPUTS}
    ${REPO}/binutils/mclex.c
    ${REPO}/binutils/winduni.c
    ${BULIBS}
)

add_binutils_exe(windmc)

target_link_libraries(windmc PRIVATE libiconv)

target_include_directories(windmc PRIVATE
    ${WINDMC_GEN_DIR}
)

# dllwrap

add_executable(dllwrap
    ${REPO}/binutils/dllwrap.c
    ${REPO}/binutils/version.c
)
target_compile_options(dllwrap PRIVATE ${GENERAL_CFLAGS}
    -std=gnu11
    -include ${CMAKE_CURRENT_SOURCE_DIR}/binutils-shim.h
)

target_compile_definitions(dllwrap PRIVATE
    _CRTIMP=
    HAVE_CONFIG_H
    "TARGET=\"x86_64-pc-moxnox\""
    PACKAGE_NAME=binutils
    "PACKAGE_STRING=\"binutils ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=binutils
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

target_include_directories(dllwrap PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${BFD_GEN_DIR}
    ${REPO}/binutils
    ${REPO}/bfd
    ${REPO}/include
)
target_link_libraries(dllwrap PRIVATE libiberty)

# cxxfilt

add_executable(cxxfilt
    ${REPO}/binutils/cxxfilt.c
    ${BULIBS}
)
add_binutils_exe(cxxfilt)

target_compile_definitions(cxxfilt PRIVATE
    HAVE_CONFIG_H
    TARGET_PREPENDS_UNDERSCORE=0
    PACKAGE_NAME=binutils
    "PACKAGE_STRING=\"binutils ${PROJECT_VERSION}\""
    PACKAGE_TARNAME=binutils
    "PACKAGE_VERSION=\"${PROJECT_VERSION}\""
    "VERSION=\"${PROJECT_VERSION}\""
)

add_custom_target(binutils)
add_dependencies(binutils
    addr2line
    ar
    as
    cxxfilt
    dlltool
    dllwrap
    ld
    nm
    objcopy
    objdump
    ranlib
    readelf
    size
    strings
    strip
    windmc
    windres
)
