project(gawk VERSION 5.3.2)

add_executable(gawk
    repo/array.c
    repo/awkgram.c
    repo/builtin.c
    repo/cint_array.c
    repo/command.c
    repo/debug.c
    repo/eval.c
    repo/ext.c
    repo/field.c
    repo/floatcomp.c
    repo/gawkapi.c
    repo/gawkmisc.c
    repo/int_array.c
    repo/io.c
    repo/main.c
    repo/mpfr.c
    repo/msg.c
    repo/node.c
    repo/printf.c
    repo/profile.c
    repo/re.c
    repo/replace.c
    repo/str_array.c
    repo/symbol.c
    repo/version.c
    repo/support/dfa.c
    repo/support/localeinfo.c
    repo/support/random.c
    repo/support/regex.c
    repo/support/malloc/dynarray_emplace_enlarge.c
    repo/support/malloc/dynarray_finalize.c
    repo/support/malloc/dynarray_resize.c
)

target_compile_definitions(gawk PRIVATE
    HAVE_CONFIG_H
    GAWK
    MPFR_USE_INTMAX_T
    DEFPATH=\".:/share/awk\"
    DEFLIBPATH=\"/lib/gawk\"
    SHLIBEXT=\"dll\"
)

target_include_directories(gawk PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/support
)

target_compile_options(gawk PRIVATE ${USERSPACE_CFLAGS}
    -std=gnu23
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-unused-parameter
    -Wno-implicit-fallthrough
    -Wno-sign-compare
    -Wno-empty-body
    -Wno-missing-field-initializers
    -Wno-cast-function-type
    -Wno-unused-label
    -Wno-unused-function
    -Wno-unused-but-set-variable
    -Wno-ignored-attributes
    -Wno-bitwise-instead-of-logical
    $<$<C_COMPILER_ID:GNU>:-Wno-calloc-transposed-args>
)

target_link_libraries(gawk PRIVATE mpfr readline moxnox)

install(TARGETS gawk DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(CODE "file(CREATE_LINK gawk.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/awk.exe\" SYMBOLIC)")
