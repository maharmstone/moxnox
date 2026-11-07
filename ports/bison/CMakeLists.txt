project(bison VERSION 3.8.2)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)

configure_file(config.h.in ${GEN}/config.h @ONLY)

include(${CMAKE_CURRENT_SOURCE_DIR}/../gnulib/gnulib.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-vars.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/gnulib-headers.cmake)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/malloc/scratch_buffer.h SCRATCH_H)
string(REPLACE "__always_inline" "inline _GL_ATTRIBUTE_ALWAYS_INLINE" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_likely" "_GL_LIKELY" SCRATCH_H "${SCRATCH_H}")
string(REPLACE "__glibc_unlikely" "_GL_UNLIKELY" SCRATCH_H "${SCRATCH_H}")
string(REGEX REPLACE "[^\n]*libc_hidden_proto[^\n]*\n" "" SCRATCH_H "${SCRATCH_H}")
file(CONFIGURE OUTPUT ${GEN}/gnu/malloc/scratch_buffer.gl.h CONTENT "${SCRATCH_H}" @ONLY)

add_custom_command(
    OUTPUT ${GEN}/gnu/timevar.h
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/timevar.h ${GEN}/gnu/timevar.h
    DEPENDS repo/gnulib/lib/timevar.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/gnu/timevar.c
    COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib/timevar.c ${GEN}/gnu/timevar.c
    DEPENDS repo/gnulib/lib/timevar.c
    VERBATIM
)

add_custom_target(bison_timevar DEPENDS ${GEN}/gnu/timevar.h)

foreach(_scanner gram code skel)
    file(MAKE_DIRECTORY ${GEN}/flex/${_scanner})
    add_custom_command(
        OUTPUT ${GEN}/src/scan-${_scanner}.c
        COMMAND ${HOST_FLEX} ${CMAKE_CURRENT_SOURCE_DIR}/repo/src/scan-${_scanner}.l
        COMMAND ${CMAKE_COMMAND} -E copy lex.yy.c ${GEN}/src/scan-${_scanner}.c
        WORKING_DIRECTORY ${GEN}/flex/${_scanner}
        DEPENDS repo/src/scan-${_scanner}.l
        VERBATIM
    )
endforeach()

add_custom_target(bison_scanners DEPENDS
    ${GEN}/src/scan-gram.c
    ${GEN}/src/scan-code.c
    ${GEN}/src/scan-skel.c
)

file(CONFIGURE OUTPUT ${GEN}/yacc CONTENT "#! /bin/sh

exec /bin/bison -y \"$@\"
" @ONLY)

set(BISON_INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}
    ${GEN}/gnu
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
)

set(BISON_CFLAGS
    ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-sign-compare
    -Wno-unused-parameter
    -Wno-type-limits
    -Wno-alloc-size
    -Wno-unused-but-set-variable
    -Wno-unterminated-string-initialization
    -Wno-unused-value
    -Wno-format
    $<$<C_COMPILER_ID:Clang>:-Wno-tautological-constant-out-of-range-compare>
)

add_executable(bison
    repo/src/AnnotationList.c
    repo/src/InadequacyList.c
    repo/src/Sbitset.c
    repo/src/assoc.c
    repo/src/closure.c
    repo/src/complain.c
    repo/src/conflicts.c
    repo/src/counterexample.c
    repo/src/derivation.c
    repo/src/derives.c
    repo/src/files.c
    repo/src/fixits.c
    repo/src/getargs.c
    repo/src/glyphs.c
    repo/src/gram.c
    repo/src/graphviz.c
    repo/src/ielr.c
    repo/src/lalr.c
    repo/src/location.c
    repo/src/lr0.c
    repo/src/lssi.c
    repo/src/main.c
    repo/src/muscle-tab.c
    repo/src/named-ref.c
    repo/src/nullable.c
    repo/src/output.c
    repo/src/parse-gram.c
    repo/src/parse-simulation.c
    repo/src/print-graph.c
    repo/src/print-xml.c
    repo/src/print.c
    repo/src/reader.c
    repo/src/reduce.c
    repo/src/relation.c
    repo/src/scan-code-c.c
    repo/src/scan-gram-c.c
    repo/src/scan-skel-c.c
    repo/src/state.c
    repo/src/state-item.c
    repo/src/strversion.c
    repo/src/symlist.c
    repo/src/symtab.c
    repo/src/tables.c
    repo/src/uniqstr.c
    repo/lib/get-errno.c
    repo/lib/path-join.c
    repo/gnulib/lib/argmatch.c
    repo/gnulib/lib/asnprintf.c
    repo/gnulib/lib/basename-lgpl.c
    repo/gnulib/lib/bitrotate.c
    repo/gnulib/lib/bitset.c
    repo/gnulib/lib/bitset/array.c
    repo/gnulib/lib/bitset/list.c
    repo/gnulib/lib/bitset/stats.c
    repo/gnulib/lib/bitset/table.c
    repo/gnulib/lib/bitset/vector.c
    repo/gnulib/lib/bitsetv.c
    repo/gnulib/lib/c-ctype.c
    repo/gnulib/lib/c-strcasecmp.c
    repo/gnulib/lib/canonicalize-lgpl.c
    repo/gnulib/lib/canonicalize.c
    repo/gnulib/lib/close-stream.c
    repo/gnulib/lib/closeout.c
    repo/gnulib/lib/concat-filename.c
    repo/gnulib/lib/dup-safer.c
    repo/gnulib/lib/dup2.c
    repo/gnulib/lib/error.c
    repo/gnulib/lib/execute.c
    repo/gnulib/lib/exitfail.c
    repo/gnulib/lib/fatal-signal.c
    repo/gnulib/lib/fcntl.c
    repo/gnulib/lib/fd-safer.c
    repo/gnulib/lib/ffsl.c
    repo/gnulib/lib/file-set.c
    repo/gnulib/lib/findprog-in.c
    repo/gnulib/lib/fopen-safer.c
    repo/gnulib/lib/fopen.c
    repo/gnulib/lib/fprintf.c
    repo/gnulib/lib/free.c
    repo/gnulib/lib/frexpl.c
    repo/gnulib/lib/fstrcmp.c
    repo/gnulib/lib/getcwd.c
    repo/gnulib/lib/getdtablesize.c
    repo/gnulib/lib/gethrxtime.c
    repo/gnulib/lib/getopt.c
    repo/gnulib/lib/getopt1.c
    repo/gnulib/lib/gettime.c
    repo/gnulib/lib/gl_array_list.c
    repo/gnulib/lib/gl_hash_map.c
    repo/gnulib/lib/gl_linked_list.c
    repo/gnulib/lib/gl_list.c
    repo/gnulib/lib/gl_map.c
    repo/gnulib/lib/gl_oset.c
    repo/gnulib/lib/gl_rbtree_oset.c
    repo/gnulib/lib/gl_rbtreehash_list.c
    repo/gnulib/lib/gl_xlist.c
    repo/gnulib/lib/gl_xmap.c
    repo/gnulib/lib/glthread/lock.c
    repo/gnulib/lib/hash-pjw.c
    repo/gnulib/lib/hash-triple-simple.c
    repo/gnulib/lib/hash.c
    repo/gnulib/lib/ialloc.c
    repo/gnulib/lib/integer_length_l.c
    repo/gnulib/lib/malloc/scratch_buffer_dupfree.c
    repo/gnulib/lib/malloc/scratch_buffer_grow.c
    repo/gnulib/lib/malloc/scratch_buffer_grow_preserve.c
    repo/gnulib/lib/mbchar.c
    repo/gnulib/lib/mbfile.c
    repo/gnulib/lib/mbswidth.c
    repo/gnulib/lib/mempcpy.c
    repo/gnulib/lib/obstack.c
    repo/gnulib/lib/obstack_printf.c
    repo/gnulib/lib/perror.c
    repo/gnulib/lib/pipe-safer.c
    repo/gnulib/lib/printf-args.c
    repo/gnulib/lib/printf-frexp.c
    repo/gnulib/lib/printf-frexpl.c
    repo/gnulib/lib/printf-parse.c
    repo/gnulib/lib/printf.c
    repo/gnulib/lib/progname.c
    repo/gnulib/lib/quotearg.c
    repo/gnulib/lib/rawmemchr.c
    repo/gnulib/lib/readlink.c
    repo/gnulib/lib/reallocarray.c
    repo/gnulib/lib/rename.c
    repo/gnulib/lib/rmdir.c
    repo/gnulib/lib/sig-handler.c
    repo/gnulib/lib/snprintf.c
    repo/gnulib/lib/spawn-pipe.c
    repo/gnulib/lib/spawn.c
    repo/gnulib/lib/spawn_faction_addchdir.c
    repo/gnulib/lib/spawn_faction_addclose.c
    repo/gnulib/lib/spawn_faction_adddup2.c
    repo/gnulib/lib/spawn_faction_addopen.c
    repo/gnulib/lib/spawn_faction_destroy.c
    repo/gnulib/lib/spawn_faction_init.c
    repo/gnulib/lib/spawnattr_destroy.c
    repo/gnulib/lib/spawnattr_init.c
    repo/gnulib/lib/spawnattr_setflags.c
    repo/gnulib/lib/spawnattr_setsigmask.c
    repo/gnulib/lib/spawni.c
    repo/gnulib/lib/spawnp.c
    repo/gnulib/lib/sprintf.c
    repo/gnulib/lib/strchrnul.c
    repo/gnulib/lib/strerror_r.c
    repo/gnulib/lib/stripslash.c
    repo/gnulib/lib/strtod.c
    repo/gnulib/lib/strverscmp.c
    ${GEN}/gnu/timevar.c
    repo/gnulib/lib/unicodeio.c
    repo/gnulib/lib/unistr/u8-uctomb-aux.c
    repo/gnulib/lib/unlink.c
    repo/gnulib/lib/vasnprintf.c
    repo/gnulib/lib/vasprintf.c
    repo/gnulib/lib/vfprintf.c
    repo/gnulib/lib/wait-process.c
    repo/gnulib/lib/xalloc-die.c
    repo/gnulib/lib/xconcat-filename.c
    repo/gnulib/lib/xhash.c
    repo/gnulib/lib/xmalloc.c
    repo/gnulib/lib/xmemdup0.c
    repo/gnulib/lib/xsize.c
    repo/gnulib/lib/xstrndup.c
    repo/gnulib/lib/xtime.c
)

target_compile_definitions(bison PRIVATE HAVE_CONFIG_H EXEEXT=\".exe\" INSTALLDIR=\"/bin\")

target_include_directories(bison PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}
    ${GEN}/gnu
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/gnulib/lib
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
)

target_compile_options(bison PRIVATE ${BISON_CFLAGS})
target_link_libraries(bison PRIVATE libiconv moxnox)

add_dependencies(bison bison_timevar bison_scanners)

install(TARGETS bison DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_library(liby STATIC
    repo/lib/main.c
    repo/lib/yyerror.c
)

set_target_properties(liby PROPERTIES OUTPUT_NAME y PREFIX lib)

target_compile_definitions(liby PRIVATE HAVE_CONFIG_H)
target_include_directories(liby PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${GEN})
target_compile_options(liby PRIVATE ${BISON_CFLAGS})

install(TARGETS liby DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(PROGRAMS ${GEN}/yacc DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(FILES
    repo/data/README.md
    repo/data/bison-default.css
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/bison
)

install(FILES
    repo/data/skeletons/bison.m4
    repo/data/skeletons/c++-skel.m4
    repo/data/skeletons/c++.m4
    repo/data/skeletons/c-like.m4
    repo/data/skeletons/c-skel.m4
    repo/data/skeletons/c.m4
    repo/data/skeletons/glr.c
    repo/data/skeletons/glr.cc
    repo/data/skeletons/glr2.cc
    repo/data/skeletons/java-skel.m4
    repo/data/skeletons/java.m4
    repo/data/skeletons/lalr1.cc
    repo/data/skeletons/lalr1.java
    repo/data/skeletons/location.cc
    repo/data/skeletons/stack.hh
    repo/data/skeletons/traceon.m4
    repo/data/skeletons/variant.hh
    repo/data/skeletons/yacc.c
    repo/data/skeletons/d-skel.m4
    repo/data/skeletons/d.m4
    repo/data/skeletons/lalr1.d
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/bison/skeletons
)

install(FILES
    repo/submodules/autoconf/lib/m4sugar/foreach.m4
    repo/submodules/autoconf/lib/m4sugar/m4sugar.m4
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/bison/m4sugar
)

install(FILES
    repo/data/xslt/bison.xsl
    repo/data/xslt/xml2dot.xsl
    repo/data/xslt/xml2text.xsl
    repo/data/xslt/xml2xhtml.xsl
    DESTINATION ${CMAKE_INSTALL_PREFIX}/share/bison/xslt
)
