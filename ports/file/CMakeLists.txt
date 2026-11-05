project(file VERSION 5.48)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})
file(MAKE_DIRECTORY ${GEN}/src)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/magic.h.in MAGIC_H)
string(REPLACE "X.YY" "548" MAGIC_H "${MAGIC_H}")
file(CONFIGURE OUTPUT ${GEN}/magic.h CONTENT "${MAGIC_H}" @ONLY)

add_custom_command(
    OUTPUT ${GEN}/src/file.h
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/file.h ${GEN}/src/file.h
    COMMAND ${HOST_SED} -z -i [[s|#ifndef WIN32\n#include <sys/param.h>\n#endif|#ifndef WIN32\n#ifdef HAVE_SYS_PARAM_H\n#include <sys/param.h>\n#endif\n#endif|]] ${GEN}/src/file.h
    DEPENDS repo/src/file.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src/apprentice.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/apprentice.c ${GEN}/src/apprentice.c
    COMMAND ${HOST_SED} -z -i [[s|\t\t\tfile_magwarn1("Duplicate magic entry `%s'",\n\t\t\t    ma->mp->desc);\n#ifndef COMPILE_ONLY\n\t\t\tfile_mdump(ma->mp);\n\t\t\tfile_mdump(mb->mp);\n#endif\n|#if 0\n\t\t\tfile_magwarn1("Duplicate magic entry `%s'",\n\t\t\t    ma->mp->desc);\n#ifndef COMPILE_ONLY\n\t\t\tfile_mdump(ma->mp);\n\t\t\tfile_mdump(mb->mp);\n#endif\n#endif\n|]] ${GEN}/src/apprentice.c
    DEPENDS repo/src/apprentice.c
    VERBATIM
)

set(MAGIC_FRAGMENTS
    repo/magic/Header
    repo/magic/Localstuff
    repo/magic/Magdir/a2ml
    repo/magic/Magdir/acorn
    repo/magic/Magdir/adi
    repo/magic/Magdir/adventure
    repo/magic/Magdir/aes
    repo/magic/Magdir/algol68
    repo/magic/Magdir/allegro
    repo/magic/Magdir/alliant
    repo/magic/Magdir/amanda
    repo/magic/Magdir/amigaos
    repo/magic/Magdir/andrew
    repo/magic/Magdir/android
    repo/magic/Magdir/animation
    repo/magic/Magdir/aout
    repo/magic/Magdir/apache
    repo/magic/Magdir/apl
    repo/magic/Magdir/apple
    repo/magic/Magdir/application
    repo/magic/Magdir/applix
    repo/magic/Magdir/apt
    repo/magic/Magdir/archive
    repo/magic/Magdir/aria
    repo/magic/Magdir/arm
    repo/magic/Magdir/asf
    repo/magic/Magdir/assembler
    repo/magic/Magdir/asterix
    repo/magic/Magdir/atari
    repo/magic/Magdir/att3b
    repo/magic/Magdir/audio
    repo/magic/Magdir/avm
    repo/magic/Magdir/basis
    repo/magic/Magdir/beetle
    repo/magic/Magdir/ber
    repo/magic/Magdir/bflt
    repo/magic/Magdir/bgcode
    repo/magic/Magdir/bhl
    repo/magic/Magdir/bioinformatics
    repo/magic/Magdir/biosig
    repo/magic/Magdir/blackberry
    repo/magic/Magdir/blcr
    repo/magic/Magdir/blender
    repo/magic/Magdir/blit
    repo/magic/Magdir/bm
    repo/magic/Magdir/bout
    repo/magic/Magdir/bsdi
    repo/magic/Magdir/bsi
    repo/magic/Magdir/btsnoop
    repo/magic/Magdir/burp
    repo/magic/Magdir/bytecode
    repo/magic/Magdir/c-lang
    repo/magic/Magdir/c64
    repo/magic/Magdir/cad
    repo/magic/Magdir/cafebabe
    repo/magic/Magdir/cbor
    repo/magic/Magdir/ccf
    repo/magic/Magdir/cddb
    repo/magic/Magdir/chord
    repo/magic/Magdir/cisco
    repo/magic/Magdir/citrus
    repo/magic/Magdir/clarion
    repo/magic/Magdir/claris
    repo/magic/Magdir/clipper
    repo/magic/Magdir/clojure
    repo/magic/Magdir/coff
    repo/magic/Magdir/commands
    repo/magic/Magdir/communications
    repo/magic/Magdir/compress
    repo/magic/Magdir/console
    repo/magic/Magdir/convex
    repo/magic/Magdir/coverage
    repo/magic/Magdir/cracklib
    repo/magic/Magdir/creativeassembly
    repo/magic/Magdir/crypto
    repo/magic/Magdir/ctags
    repo/magic/Magdir/ctf
    repo/magic/Magdir/cubemap
    repo/magic/Magdir/cups
    repo/magic/Magdir/dact
    repo/magic/Magdir/database
    repo/magic/Magdir/dataone
    repo/magic/Magdir/dbpf
    repo/magic/Magdir/der
    repo/magic/Magdir/diamond
    repo/magic/Magdir/dif
    repo/magic/Magdir/diff
    repo/magic/Magdir/digital
    repo/magic/Magdir/dolby
    repo/magic/Magdir/dump
    repo/magic/Magdir/dwarfs
    repo/magic/Magdir/dyadic
    repo/magic/Magdir/ebml
    repo/magic/Magdir/edid
    repo/magic/Magdir/editors
    repo/magic/Magdir/efi
    repo/magic/Magdir/elf
    repo/magic/Magdir/encore
    repo/magic/Magdir/epoc
    repo/magic/Magdir/erlang
    repo/magic/Magdir/espressif
    repo/magic/Magdir/esri
    repo/magic/Magdir/fcs
    repo/magic/Magdir/filesystems
    repo/magic/Magdir/finger
    repo/magic/Magdir/firmware
    repo/magic/Magdir/flash
    repo/magic/Magdir/flif
    repo/magic/Magdir/fonts
    repo/magic/Magdir/forth
    repo/magic/Magdir/fortran
    repo/magic/Magdir/frame
    repo/magic/Magdir/freebsd
    repo/magic/Magdir/fsav
    repo/magic/Magdir/fusecompress
    repo/magic/Magdir/games
    repo/magic/Magdir/gcc
    repo/magic/Magdir/gconv
    repo/magic/Magdir/gentoo
    repo/magic/Magdir/geo
    repo/magic/Magdir/geos
    repo/magic/Magdir/gguf
    repo/magic/Magdir/gimp
    repo/magic/Magdir/git
    repo/magic/Magdir/glibc
    repo/magic/Magdir/gnome
    repo/magic/Magdir/gnu
    repo/magic/Magdir/gnumeric
    repo/magic/Magdir/gpt
    repo/magic/Magdir/gpu
    repo/magic/Magdir/grace
    repo/magic/Magdir/graphviz
    repo/magic/Magdir/gringotts
    repo/magic/Magdir/hardware
    repo/magic/Magdir/hitachi-sh
    repo/magic/Magdir/hp
    repo/magic/Magdir/human68k
    repo/magic/Magdir/ibm370
    repo/magic/Magdir/ibm6000
    repo/magic/Magdir/icc
    repo/magic/Magdir/iff
    repo/magic/Magdir/images
    repo/magic/Magdir/inform
    repo/magic/Magdir/intel
    repo/magic/Magdir/interleaf
    repo/magic/Magdir/island
    repo/magic/Magdir/ispell
    repo/magic/Magdir/isz
    repo/magic/Magdir/java
    repo/magic/Magdir/javascript
    repo/magic/Magdir/jpeg
    repo/magic/Magdir/k9
    repo/magic/Magdir/karma
    repo/magic/Magdir/kde
    repo/magic/Magdir/keepass
    repo/magic/Magdir/kerberos
    repo/magic/Magdir/keyman
    repo/magic/Magdir/kicad
    repo/magic/Magdir/kml
    repo/magic/Magdir/lammps
    repo/magic/Magdir/lauterbach
    repo/magic/Magdir/lecter
    repo/magic/Magdir/lex
    repo/magic/Magdir/lif
    repo/magic/Magdir/linux
    repo/magic/Magdir/lisp
    repo/magic/Magdir/llvm
    repo/magic/Magdir/locoscript
    repo/magic/Magdir/lua
    repo/magic/Magdir/luks
    repo/magic/Magdir/m4
    repo/magic/Magdir/mach
    repo/magic/Magdir/macintosh
    repo/magic/Magdir/macos
    repo/magic/Magdir/magic
    repo/magic/Magdir/mail.news
    repo/magic/Magdir/make
    repo/magic/Magdir/map
    repo/magic/Magdir/maple
    repo/magic/Magdir/marc21
    repo/magic/Magdir/mathcad
    repo/magic/Magdir/mathematica
    repo/magic/Magdir/matroska
    repo/magic/Magdir/mcrypt
    repo/magic/Magdir/measure
    repo/magic/Magdir/mercurial
    repo/magic/Magdir/metastore
    repo/magic/Magdir/meteorological
    repo/magic/Magdir/microfocus
    repo/magic/Magdir/mime
    repo/magic/Magdir/mips
    repo/magic/Magdir/mirage
    repo/magic/Magdir/misctools
    repo/magic/Magdir/mkid
    repo/magic/Magdir/mlssa
    repo/magic/Magdir/mmdf
    repo/magic/Magdir/modem
    repo/magic/Magdir/modulefile
    repo/magic/Magdir/motorola
    repo/magic/Magdir/mozilla
    repo/magic/Magdir/msdos
    repo/magic/Magdir/msooxml
    repo/magic/Magdir/msvc
    repo/magic/Magdir/msx
    repo/magic/Magdir/mup
    repo/magic/Magdir/music
    repo/magic/Magdir/nasa
    repo/magic/Magdir/natinst
    repo/magic/Magdir/ncr
    repo/magic/Magdir/netbsd
    repo/magic/Magdir/netscape
    repo/magic/Magdir/netware
    repo/magic/Magdir/news
    repo/magic/Magdir/nifty
    repo/magic/Magdir/nim-lang
    repo/magic/Magdir/nitpicker
    repo/magic/Magdir/nix
    repo/magic/Magdir/numpy
    repo/magic/Magdir/oasis
    repo/magic/Magdir/ocaml
    repo/magic/Magdir/octave
    repo/magic/Magdir/ole2compounddocs
    repo/magic/Magdir/olf
    repo/magic/Magdir/openfst
    repo/magic/Magdir/opentimestamps
    repo/magic/Magdir/oric
    repo/magic/Magdir/os2
    repo/magic/Magdir/os400
    repo/magic/Magdir/os9
    repo/magic/Magdir/osf1
    repo/magic/Magdir/pack
    repo/magic/Magdir/palm
    repo/magic/Magdir/parix
    repo/magic/Magdir/parrot
    repo/magic/Magdir/partclone
    repo/magic/Magdir/pascal
    repo/magic/Magdir/pbf
    repo/magic/Magdir/pbm
    repo/magic/Magdir/pc98
    repo/magic/Magdir/pci_ids
    repo/magic/Magdir/pcjr
    repo/magic/Magdir/pdf
    repo/magic/Magdir/pdp
    repo/magic/Magdir/perl
    repo/magic/Magdir/pgf
    repo/magic/Magdir/pgp
    repo/magic/Magdir/pgp-binary-keys
    repo/magic/Magdir/pkgadd
    repo/magic/Magdir/plan9
    repo/magic/Magdir/playdate
    repo/magic/Magdir/plus5
    repo/magic/Magdir/pmem
    repo/magic/Magdir/polyml
    repo/magic/Magdir/printer
    repo/magic/Magdir/project
    repo/magic/Magdir/psdbms
    repo/magic/Magdir/psl
    repo/magic/Magdir/pulsar
    repo/magic/Magdir/puzzle
    repo/magic/Magdir/pwsafe
    repo/magic/Magdir/pyramid
    repo/magic/Magdir/python
    repo/magic/Magdir/qt
    repo/magic/Magdir/r
    repo/magic/Magdir/revision
    repo/magic/Magdir/riff
    repo/magic/Magdir/ringdove
    repo/magic/Magdir/rpi
    repo/magic/Magdir/rpm
    repo/magic/Magdir/rpmsg
    repo/magic/Magdir/rst
    repo/magic/Magdir/rtf
    repo/magic/Magdir/ruby
    repo/magic/Magdir/rust
    repo/magic/Magdir/sc
    repo/magic/Magdir/sccs
    repo/magic/Magdir/scientific
    repo/magic/Magdir/securitycerts
    repo/magic/Magdir/selinux
    repo/magic/Magdir/sendmail
    repo/magic/Magdir/sequent
    repo/magic/Magdir/sereal
    repo/magic/Magdir/sf3
    repo/magic/Magdir/sgi
    repo/magic/Magdir/sgml
    repo/magic/Magdir/sharc
    repo/magic/Magdir/sinclair
    repo/magic/Magdir/sisu
    repo/magic/Magdir/sketch
    repo/magic/Magdir/smalltalk
    repo/magic/Magdir/smile
    repo/magic/Magdir/sniffer
    repo/magic/Magdir/softquad
    repo/magic/Magdir/sosi
    repo/magic/Magdir/spec
    repo/magic/Magdir/spectrum
    repo/magic/Magdir/sql
    repo/magic/Magdir/ssh
    repo/magic/Magdir/ssl
    repo/magic/Magdir/statistics
    repo/magic/Magdir/subtitle
    repo/magic/Magdir/sun
    repo/magic/Magdir/svf
    repo/magic/Magdir/syd
    repo/magic/Magdir/sylk
    repo/magic/Magdir/symbos
    repo/magic/Magdir/sysex
    repo/magic/Magdir/tapebackup
    repo/magic/Magdir/tcl
    repo/magic/Magdir/teapot
    repo/magic/Magdir/terminfo
    repo/magic/Magdir/tex
    repo/magic/Magdir/tgif
    repo/magic/Magdir/ti-8x
    repo/magic/Magdir/timezone
    repo/magic/Magdir/tplink
    repo/magic/Magdir/troff
    repo/magic/Magdir/tuxedo
    repo/magic/Magdir/typeset
    repo/magic/Magdir/uf2
    repo/magic/Magdir/unicode
    repo/magic/Magdir/unisig
    repo/magic/Magdir/unknown
    repo/magic/Magdir/usd
    repo/magic/Magdir/uterus
    repo/magic/Magdir/uuencode
    repo/magic/Magdir/uxn
    repo/magic/Magdir/vacuum-cleaner
    repo/magic/Magdir/varied.out
    repo/magic/Magdir/varied.script
    repo/magic/Magdir/vax
    repo/magic/Magdir/vicar
    repo/magic/Magdir/virtual
    repo/magic/Magdir/virtutech
    repo/magic/Magdir/visx
    repo/magic/Magdir/vms
    repo/magic/Magdir/vmware
    repo/magic/Magdir/vorbis
    repo/magic/Magdir/vxl
    repo/magic/Magdir/warc
    repo/magic/Magdir/weak
    repo/magic/Magdir/web
    repo/magic/Magdir/webassembly
    repo/magic/Magdir/windows
    repo/magic/Magdir/wireless
    repo/magic/Magdir/wordprocessors
    repo/magic/Magdir/wsdl
    repo/magic/Magdir/x68000
    repo/magic/Magdir/xdelta
    repo/magic/Magdir/xenix
    repo/magic/Magdir/xilinx
    repo/magic/Magdir/xo65
    repo/magic/Magdir/xwindows
    repo/magic/Magdir/yara
    repo/magic/Magdir/zfs
    repo/magic/Magdir/zilog
    repo/magic/Magdir/zip
    repo/magic/Magdir/zyxel
)

set(MAGIC_FRAGMENT_PATHS)
foreach(frag ${MAGIC_FRAGMENTS})
    list(APPEND MAGIC_FRAGMENT_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/${frag})
endforeach()
list(JOIN MAGIC_FRAGMENT_PATHS "' '" MAGIC_FRAGMENT_ARGS)

# patch out entry which breaks our regcomp because of invalid UTF-8
add_custom_command(
    OUTPUT ${GEN}/magic
    COMMAND sh -c "cat '${MAGIC_FRAGMENT_ARGS}' | ${HOST_SED} '/AppleWorks Word Processor$/,/inch left margin$/s/^/#/' > '${GEN}/magic'"
    DEPENDS ${MAGIC_FRAGMENT_PATHS}
    VERBATIM
)

add_custom_target(file_magic ALL DEPENDS ${GEN}/magic)

add_library(libmagic SHARED
    ${GEN}/src/file.h
    repo/src/asctime_r.c
    repo/src/ctime_r.c
    repo/src/dprintf.c
    repo/src/fmtcheck.c
    repo/src/strcasestr.c
    repo/src/strlcat.c
    repo/src/strlcpy.c
    repo/src/buffer.c
    repo/src/magic.c
    ${GEN}/src/apprentice.c
    repo/src/softmagic.c
    repo/src/ascmagic.c
    repo/src/encoding.c
    repo/src/compress.c
    repo/src/is_csv.c
    repo/src/is_json.c
    repo/src/is_simh.c
    repo/src/is_tar.c
    repo/src/readelf.c
    repo/src/print.c
    repo/src/fsmagic.c
    repo/src/funcs.c
    repo/src/apptype.c
    repo/src/der.c
    repo/src/cdf.c
    repo/src/cdf_time.c
    repo/src/readcdf.c
    repo/src/swap.c
)

set_target_properties(libmagic PROPERTIES OUTPUT_NAME "magic-1" ARCHIVE_OUTPUT_NAME "magic" PREFIX "lib")

set(FILE_CFLAGS
    ${USERSPACE_CFLAGS}
    -include ${GEN}/src/file.h
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-attributes
)

target_compile_definitions(libmagic PRIVATE HAVE_CONFIG_H MAGIC="/share/misc/magic")
target_include_directories(libmagic PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GEN}
                           ${CMAKE_CURRENT_SOURCE_DIR}/repo/src)
target_compile_options(libmagic PRIVATE ${FILE_CFLAGS})
target_link_libraries(libmagic PRIVATE zlib zstd liblzma bzip2 moxnox)

add_executable(file
    ${GEN}/src/file.h
    repo/src/file.c
)

target_compile_definitions(file PRIVATE HAVE_CONFIG_H MAGIC="/share/misc/magic")
target_include_directories(file PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GEN})
target_compile_options(file PRIVATE ${FILE_CFLAGS})
target_link_libraries(file PRIVATE libmagic moxnox)

install(TARGETS file DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS libmagic DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(FILES ${GEN}/magic.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)

install(FILES ${GEN}/magic DESTINATION ${CMAKE_INSTALL_PREFIX}/share/misc)
