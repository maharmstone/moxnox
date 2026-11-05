set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN}/lib ${GEN}/include)

add_custom_command(
    OUTPUT ${GEN}/lib/aliases.h ${GEN}/lib/canonical.h ${GEN}/lib/canonical_local.h
    COMMAND sh -c "cd '${GEN}/lib' && cc '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/genaliases.c' -o genaliases${HOST_EXECUTABLE_SUFFIX} && ./genaliases${HOST_EXECUTABLE_SUFFIX} aliases.gperf canonical.sh canonical_local.sh && gperf -m 10 aliases.gperf | sed -e 's/^\\(const struct alias \\)/static \\1/' > aliases.h && sh canonical.sh > canonical.h && sh canonical_local.sh > canonical_local.h && rm -f genaliases${HOST_EXECUTABLE_SUFFIX} aliases.gperf canonical.sh canonical_local.sh"
    DEPENDS repo/lib/genaliases.c repo/lib/encodings.def repo/lib/encodings_local.def
    VERBATIM
)

foreach(extra aix osf1 dos zos)
    string(TOUPPER ${extra} extra_upper)
    add_custom_command(
        OUTPUT ${GEN}/lib/aliases_${extra}.h ${GEN}/lib/canonical_${extra}.h
        COMMAND sh -c "cd '${GEN}/lib' && cc -DUSE_${extra_upper} '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/genaliases2.c' -o genaliases2_${extra}${HOST_EXECUTABLE_SUFFIX} && ./genaliases2_${extra}${HOST_EXECUTABLE_SUFFIX} ${extra} aliases_${extra}.h canonical_${extra}.h && rm -f genaliases2_${extra}${HOST_EXECUTABLE_SUFFIX}"
        DEPENDS repo/lib/genaliases2.c repo/lib/encodings_${extra}.def
        VERBATIM
    )
    list(APPEND ICONV_EXTRA_TABLES
        ${GEN}/lib/aliases_${extra}.h ${GEN}/lib/canonical_${extra}.h)
endforeach()

add_custom_command(
    OUTPUT ${GEN}/lib/flags.h
    COMMAND sh -c "cd '${GEN}/lib' && cc '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/genflags.c' -o genflags${HOST_EXECUTABLE_SUFFIX} && ./genflags${HOST_EXECUTABLE_SUFFIX} > flags.h && rm -f genflags${HOST_EXECUTABLE_SUFFIX}"
    DEPENDS repo/lib/genflags.c repo/lib/converters.h repo/lib/encodings.def
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/translit.h
    COMMAND sh -c "cd '${GEN}/lib' && cc '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/gentranslit.c' -o gentranslit${HOST_EXECUTABLE_SUFFIX} && ./gentranslit${HOST_EXECUTABLE_SUFFIX} < '${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/translit.def' > translit.h && rm -f gentranslit${HOST_EXECUTABLE_SUFFIX}"
    DEPENDS repo/lib/gentranslit.c repo/lib/translit.def
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/include/localcharset.h
    COMMAND sh -c "sed -e 's/extern \\([^\"]\\)/extern LIBCHARSET_DLL_EXPORTED \\1/' -e '/#define _LOCALCHARSET_H/r ${CMAKE_CURRENT_SOURCE_DIR}/repo/libcharset/include/export.h' '${CMAKE_CURRENT_SOURCE_DIR}/repo/libcharset/include/localcharset.h.in' | sed -e 's/@HAVE_VISIBILITY@/1/' > '${GEN}/include/localcharset.h'"
    DEPENDS repo/libcharset/include/localcharset.h.in repo/libcharset/include/export.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/include/iconv.h
    COMMAND sh -c "sed -e 's/@DLL_VARIABLE@//' -e 's/@EILSEQ@//' -e 's/@ICONV_CONST@//' -e 's/@USE_MBSTATE_T@/1/' -e 's/@BROKEN_WCHAR_H@/0/' -e 's/@HAVE_WCHAR_T@/1/' '${CMAKE_CURRENT_SOURCE_DIR}/repo/include/iconv.h.in' > '${GEN}/include/iconv.h'"
    DEPENDS repo/include/iconv.h.in
    VERBATIM
)

add_library(libiconv SHARED
    repo/lib/iconv.c
    repo/libcharset/lib/localcharset.c
    ${GEN}/lib/aliases.h
    ${GEN}/lib/canonical.h
    ${GEN}/lib/canonical_local.h
    ${GEN}/lib/flags.h
    ${GEN}/lib/translit.h
    ${ICONV_EXTRA_TABLES}
    ${GEN}/include/iconv.h
    ${GEN}/include/localcharset.h
)

set_target_properties(libiconv PROPERTIES OUTPUT_NAME "iconv-2" ARCHIVE_OUTPUT_NAME "iconv" PREFIX "lib")

target_compile_options(libiconv PRIVATE ${USERSPACE_CFLAGS} -fvisibility=default -std=gnu17)

target_compile_options(libiconv PRIVATE
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-implicit-fallthrough
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-type-limits
    -Wno-unused-const-variable
    -Wno-misleading-indentation
    -Wno-parentheses-equality
    -Wno-deprecated-non-prototype
)

target_compile_definitions(libiconv PRIVATE -DHAVE_CONFIG_H -DBUILDING_LIBICONV -DBUILDING_LIBCHARSET)
target_include_directories(libiconv PUBLIC ${GEN}/include)
target_include_directories(libiconv PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GEN}/lib
    repo/lib
    repo/libcharset/include
)

install(TARGETS libiconv DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(FILES ${GEN}/include/iconv.h ${GEN}/include/localcharset.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES repo/libcharset/include/libcharset.h.in DESTINATION ${CMAKE_INSTALL_PREFIX}/include RENAME libcharset.h)
