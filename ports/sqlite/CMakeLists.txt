project(sqlite)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)
file(MAKE_DIRECTORY ${GEN}/tsrc)

file(READ repo/VERSION PROJECT_VERSION)
string(STRIP "${PROJECT_VERSION}" PROJECT_VERSION)

set(SQLITE_FEATURES
    SQLITE_ENABLE_API_ARMOR
    SQLITE_ENABLE_BYTECODE_VTAB
    SQLITE_ENABLE_COLUMN_METADATA
    SQLITE_ENABLE_DBPAGE_VTAB
    SQLITE_ENABLE_DBSTAT_VTAB
    SQLITE_ENABLE_DESERIALIZE
    SQLITE_ENABLE_EXPLAIN_COMMENTS
    SQLITE_ENABLE_FTS3
    SQLITE_ENABLE_FTS3_PARENTHESIS
    SQLITE_ENABLE_FTS4
    SQLITE_ENABLE_FTS5
    SQLITE_ENABLE_GEOPOLY
    SQLITE_ENABLE_HIDDEN_COLUMNS
    SQLITE_ENABLE_MATH_FUNCTIONS
    SQLITE_ENABLE_MEMSYS5
    SQLITE_ENABLE_NORMALIZE
    SQLITE_ENABLE_OFFSET_SQL_FUNC
    SQLITE_ENABLE_PERCENTILE
    SQLITE_ENABLE_PREUPDATE_HOOK
    SQLITE_ENABLE_RBU
    SQLITE_ENABLE_RTREE
    SQLITE_ENABLE_SESSION
    SQLITE_ENABLE_STMT_SCANSTATUS
    SQLITE_ENABLE_STMTVTAB
    SQLITE_ENABLE_UNKNOWN_SQL_FUNCTION
    SQLITE_ENABLE_UNLOCK_NOTIFY
    SQLITE_ENABLE_UPDATE_DELETE_LIMIT
    SQLITE_SECURE_DELETE
    SQLITE_SOUNDEX
    SQLITE_THREADSAFE=1
    SQLITE_USE_URI
)
list(TRANSFORM SQLITE_FEATURES PREPEND -D OUTPUT_VARIABLE SQLITE_FEATURE_FLAGS)

add_custom_command(
    OUTPUT ${GEN}/jimsh${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -O1 -DHAVE_REALPATH -o ${GEN}/jimsh${HOST_EXECUTABLE_SUFFIX} ${REPO}/autosetup/jimsh0.c
    DEPENDS repo/autosetup/jimsh0.c
    VERBATIM
)
set(JIMSH ${GEN}/jimsh${HOST_EXECUTABLE_SUFFIX})

add_custom_command(
    OUTPUT ${GEN}/lemon${HOST_EXECUTABLE_SUFFIX} ${GEN}/lempar.c
    COMMAND cc -o ${GEN}/lemon${HOST_EXECUTABLE_SUFFIX} ${REPO}/tool/lemon.c
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/tool/lempar.c ${GEN}/lempar.c
    DEPENDS repo/tool/lemon.c repo/tool/lempar.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/mkkeywordhash${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc ${SQLITE_FEATURE_FLAGS} -o ${GEN}/mkkeywordhash${HOST_EXECUTABLE_SUFFIX} ${REPO}/tool/mkkeywordhash.c
    DEPENDS repo/tool/mkkeywordhash.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/mksourceid${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -o ${GEN}/mksourceid${HOST_EXECUTABLE_SUFFIX} ${REPO}/tool/mksourceid.c
    DEPENDS repo/tool/mksourceid.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/src-verify${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -o ${GEN}/src-verify${HOST_EXECUTABLE_SUFFIX} ${REPO}/tool/src-verify.c
    DEPENDS repo/tool/src-verify.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/parse.c ${GEN}/parse.h
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/src/parse.y ${GEN}/parse.y
    COMMAND ${GEN}/lemon${HOST_EXECUTABLE_SUFFIX} ${SQLITE_FEATURE_FLAGS} -S parse.y
    DEPENDS repo/src/parse.y ${GEN}/lemon${HOST_EXECUTABLE_SUFFIX} ${GEN}/lempar.c
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/opcodes.h
    COMMAND sh -c "cat '${GEN}/parse.h' '${REPO}/src/vdbe.c' | '${JIMSH}' '${REPO}/tool/mkopcodeh.tcl' > '${GEN}/opcodes.h'"
    DEPENDS ${GEN}/parse.h repo/src/vdbe.c repo/tool/mkopcodeh.tcl ${JIMSH}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/opcodes.c
    COMMAND sh -c "'${JIMSH}' '${REPO}/tool/mkopcodec.tcl' '${GEN}/opcodes.h' > '${GEN}/opcodes.c'"
    DEPENDS ${GEN}/opcodes.h repo/tool/mkopcodec.tcl ${JIMSH}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/keywordhash.h
    COMMAND sh -c "'${GEN}/mkkeywordhash${HOST_EXECUTABLE_SUFFIX}' > '${GEN}/keywordhash.h'"
    DEPENDS ${GEN}/mkkeywordhash${HOST_EXECUTABLE_SUFFIX}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/pragma.h
    COMMAND ${JIMSH} ${REPO}/tool/mkpragmatab.tcl ${GEN}/pragma.h
    DEPENDS repo/tool/mkpragmatab.tcl ${JIMSH}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/ctime.c
    COMMAND ${JIMSH} ${REPO}/tool/mkctimec.tcl ${GEN}/ctime.c
    DEPENDS repo/tool/mkctimec.tcl ${JIMSH}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/sqlite3.h
    COMMAND ${JIMSH} ${REPO}/tool/mksqlite3h.tcl ${REPO} -o ${GEN}/sqlite3.h
    DEPENDS repo/tool/mksqlite3h.tcl repo/src/sqlite.h.in repo/VERSION repo/manifest repo/manifest.uuid ${GEN}/mksourceid${HOST_EXECUTABLE_SUFFIX} ${JIMSH}
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/fts5parse.c ${GEN}/fts5parse.h
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/ext/fts5/fts5parse.y ${GEN}/fts5parse.y
    COMMAND ${CMAKE_COMMAND} -E rm -f ${GEN}/fts5parse.h
    COMMAND ${GEN}/lemon${HOST_EXECUTABLE_SUFFIX} -S fts5parse.y
    DEPENDS repo/ext/fts5/fts5parse.y ${GEN}/lemon${HOST_EXECUTABLE_SUFFIX} ${GEN}/lempar.c
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

set(SQLITE_FTS5_SRC
    repo/ext/fts5/fts5.h
    repo/ext/fts5/fts5Int.h
    repo/ext/fts5/fts5_aux.c
    repo/ext/fts5/fts5_buffer.c
    repo/ext/fts5/fts5_main.c
    repo/ext/fts5/fts5_config.c
    repo/ext/fts5/fts5_expr.c
    repo/ext/fts5/fts5_hash.c
    repo/ext/fts5/fts5_index.c
    repo/ext/fts5/fts5_storage.c
    repo/ext/fts5/fts5_tokenize.c
    repo/ext/fts5/fts5_unicode2.c
    repo/ext/fts5/fts5_varint.c
    repo/ext/fts5/fts5_vocab.c
)

add_custom_command(
    OUTPUT ${GEN}/fts5.c
    COMMAND ${JIMSH} ${REPO}/ext/fts5/tool/mkfts5c.tcl
    DEPENDS ${SQLITE_FTS5_SRC} ${GEN}/fts5parse.c ${GEN}/fts5parse.h repo/ext/fts5/tool/mkfts5c.tcl ${JIMSH}
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

set(SQLITE_SRC
    ${REPO}/src/alter.c
    ${REPO}/src/analyze.c
    ${REPO}/src/attach.c
    ${REPO}/src/auth.c
    ${REPO}/src/backup.c
    ${REPO}/src/bitvec.c
    ${REPO}/src/btmutex.c
    ${REPO}/src/btree.c
    ${REPO}/src/btree.h
    ${REPO}/src/btreeInt.h
    ${REPO}/src/build.c
    ${REPO}/src/callback.c
    ${REPO}/src/carray.c
    ${REPO}/src/complete.c
    ${REPO}/src/date.c
    ${REPO}/src/dbpage.c
    ${REPO}/src/dbstat.c
    ${REPO}/src/delete.c
    ${REPO}/src/expr.c
    ${REPO}/src/fault.c
    ${REPO}/src/fkey.c
    ${REPO}/src/func.c
    ${REPO}/src/global.c
    ${REPO}/src/hash.c
    ${REPO}/src/hash.h
    ${REPO}/src/hwtime.h
    ${REPO}/src/insert.c
    ${REPO}/src/json.c
    ${REPO}/src/legacy.c
    ${REPO}/src/loadext.c
    ${REPO}/src/main.c
    ${REPO}/src/malloc.c
    ${REPO}/src/mem0.c
    ${REPO}/src/mem1.c
    ${REPO}/src/mem2.c
    ${REPO}/src/mem3.c
    ${REPO}/src/mem5.c
    ${REPO}/src/memdb.c
    ${REPO}/src/memjournal.c
    ${REPO}/src/msvc.h
    ${REPO}/src/mutex.c
    ${REPO}/src/mutex.h
    ${REPO}/src/mutex_noop.c
    ${REPO}/src/mutex_unix.c
    ${REPO}/src/mutex_w32.c
    ${REPO}/src/notify.c
    ${REPO}/src/os.c
    ${REPO}/src/os.h
    ${REPO}/src/os_common.h
    ${REPO}/src/os_setup.h
    ${REPO}/src/os_kv.c
    ${REPO}/src/os_unix.c
    ${REPO}/src/os_win.c
    ${REPO}/src/os_win.h
    ${REPO}/src/pager.c
    ${REPO}/src/pager.h
    ${REPO}/src/pcache.c
    ${REPO}/src/pcache.h
    ${REPO}/src/pcache1.c
    ${REPO}/src/pragma.c
    ${REPO}/src/prepare.c
    ${REPO}/src/printf.c
    ${REPO}/src/random.c
    ${REPO}/src/resolve.c
    ${REPO}/src/rowset.c
    ${REPO}/src/select.c
    ${REPO}/src/status.c
    ${REPO}/src/shell.c.in
    ${REPO}/src/sqlite3ext.h
    ${REPO}/src/sqliteInt.h
    ${REPO}/src/sqliteLimit.h
    ${REPO}/src/table.c
    ${REPO}/src/threads.c
    ${REPO}/src/tokenize.c
    ${REPO}/src/treeview.c
    ${REPO}/src/trigger.c
    ${REPO}/src/utf.c
    ${REPO}/src/update.c
    ${REPO}/src/upsert.c
    ${REPO}/src/util.c
    ${REPO}/src/vacuum.c
    ${REPO}/src/vdbe.c
    ${REPO}/src/vdbe.h
    ${REPO}/src/vdbeapi.c
    ${REPO}/src/vdbeaux.c
    ${REPO}/src/vdbeblob.c
    ${REPO}/src/vdbemem.c
    ${REPO}/src/vdbesort.c
    ${REPO}/src/vdbetrace.c
    ${REPO}/src/vdbevtab.c
    ${REPO}/src/vdbeInt.h
    ${REPO}/src/vtab.c
    ${REPO}/src/vxworks.h
    ${REPO}/src/wal.c
    ${REPO}/src/wal.h
    ${REPO}/src/walker.c
    ${REPO}/src/where.c
    ${REPO}/src/wherecode.c
    ${REPO}/src/whereexpr.c
    ${REPO}/src/whereInt.h
    ${REPO}/src/window.c
    ${REPO}/ext/fts3/fts3.c
    ${REPO}/ext/fts3/fts3.h
    ${REPO}/ext/fts3/fts3Int.h
    ${REPO}/ext/fts3/fts3_aux.c
    ${REPO}/ext/fts3/fts3_expr.c
    ${REPO}/ext/fts3/fts3_hash.c
    ${REPO}/ext/fts3/fts3_hash.h
    ${REPO}/ext/fts3/fts3_icu.c
    ${REPO}/ext/fts3/fts3_porter.c
    ${REPO}/ext/fts3/fts3_snippet.c
    ${REPO}/ext/fts3/fts3_tokenizer.h
    ${REPO}/ext/fts3/fts3_tokenizer.c
    ${REPO}/ext/fts3/fts3_tokenizer1.c
    ${REPO}/ext/fts3/fts3_tokenize_vtab.c
    ${REPO}/ext/fts3/fts3_unicode.c
    ${REPO}/ext/fts3/fts3_unicode2.c
    ${REPO}/ext/fts3/fts3_write.c
    ${REPO}/ext/icu/sqliteicu.h
    ${REPO}/ext/icu/icu.c
    ${REPO}/ext/rtree/rtree.h
    ${REPO}/ext/rtree/rtree.c
    ${REPO}/ext/rtree/geopoly.c
    ${REPO}/ext/session/sqlite3session.c
    ${REPO}/ext/session/sqlite3session.h
    ${REPO}/ext/rbu/sqlite3rbu.h
    ${REPO}/ext/rbu/sqlite3rbu.c
    ${REPO}/ext/misc/stmt.c
)

set(SQLITE_GENERATED_SRC
    ${GEN}/ctime.c
    ${GEN}/keywordhash.h
    ${GEN}/opcodes.c
    ${GEN}/opcodes.h
    ${GEN}/parse.c
    ${GEN}/parse.h
    ${GEN}/pragma.h
    ${GEN}/sqlite3.h
    ${GEN}/fts5.c
    ${REPO}/ext/fts5/fts5.h
)

add_custom_command(
    OUTPUT ${GEN}/tsrc.stamp
    COMMAND ${CMAKE_COMMAND} -E rm -rf ${GEN}/tsrc
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GEN}/tsrc
    COMMAND ${CMAKE_COMMAND} -E copy ${SQLITE_SRC} ${SQLITE_GENERATED_SRC} ${GEN}/tsrc
    COMMAND sh -c "'${JIMSH}' '${REPO}/tool/vdbe-compress.tcl' < '${GEN}/tsrc/vdbe.c' > '${GEN}/vdbe.new'"
    COMMAND ${CMAKE_COMMAND} -E rename ${GEN}/vdbe.new ${GEN}/tsrc/vdbe.c
    COMMAND ${CMAKE_COMMAND} -E touch ${GEN}/tsrc.stamp
    DEPENDS ${SQLITE_SRC} ${SQLITE_GENERATED_SRC} repo/tool/vdbe-compress.tcl ${JIMSH}
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/sqlite3.c ${GEN}/sqlite3ext.h
    COMMAND ${JIMSH} ${REPO}/tool/mksqlite3c.tcl --linemacros=0
    COMMAND ${CMAKE_COMMAND} -E copy ${GEN}/tsrc/sqlite3ext.h ${GEN}/sqlite3ext.h
    DEPENDS ${GEN}/tsrc.stamp repo/tool/mksqlite3c.tcl ${GEN}/src-verify${HOST_EXECUTABLE_SUFFIX} repo/manifest ${JIMSH}
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

set(SQLITE_SHELL_SRC
    repo/src/shell.c.in
    repo/ext/qrf/qrf.c
    repo/ext/qrf/qrf.h
    repo/ext/expert/sqlite3expert.c
    repo/ext/expert/sqlite3expert.h
    repo/ext/intck/sqlite3intck.c
    repo/ext/intck/sqlite3intck.h
    repo/ext/misc/appendvfs.c
    repo/ext/misc/base64.c
    repo/ext/misc/base85.c
    repo/ext/misc/completion.c
    repo/ext/misc/decimal.c
    repo/ext/misc/fileio.c
    repo/ext/misc/ieee754.c
    repo/ext/misc/memtrace.c
    repo/ext/misc/pcachetrace.c
    repo/ext/misc/regexp.c
    repo/ext/misc/series.c
    repo/ext/misc/sha1.c
    repo/ext/misc/shathree.c
    repo/ext/misc/sqlar.c
    repo/ext/misc/uint.c
    repo/ext/misc/vfstrace.c
    repo/ext/misc/windirent.h
    repo/ext/misc/zipfile.c
    repo/ext/recover/dbdata.c
    repo/ext/recover/sqlite3recover.c
    repo/ext/recover/sqlite3recover.h
)

add_custom_command(
    OUTPUT ${GEN}/shell.c
    COMMAND ${JIMSH} ${REPO}/tool/mkshellc.tcl ${GEN}/shell.c
    COMMAND ${HOST_SED} -i
        -e [[s|# include <sys/ioctl.h>|# include <termios.h>|]]
        -e [[s|#include <memory.h>|#include <string.h>|]]
        -e [[s|#elif defined(TIOCGWINSZ)|#elif defined(__moxnox__)|]]
        -e [[s|ioctl(\(STD[A-Z]*_FILENO\), TIOCGWINSZ, &ws)>=0|tcgetwinsize(\1, \&ws)==0|g]]
        ${GEN}/shell.c
    DEPENDS ${SQLITE_SHELL_SRC} repo/tool/mkshellc.tcl ${JIMSH}
    VERBATIM
)

add_library(sqlite3 SHARED ${GEN}/sqlite3.c ${GEN}/sqlite3.h ${GEN}/sqlite3ext.h)

set(SQLITE_PLATFORM
    HAVE_FCHMOD=1
    HAVE_FCHOWN=1
    HAVE_FDATASYNC=1
    HAVE_GMTIME_R=1
    HAVE_ISNAN=1
    HAVE_LOCALTIME_R=1
    HAVE_LOG2=1
    HAVE_LOG10=1
    HAVE_LSTAT=1
    HAVE_READLINK=1
    HAVE_STRERROR_R=1
    HAVE_UTIME=1
)

target_compile_definitions(sqlite3 PRIVATE ${SQLITE_FEATURES} ${SQLITE_PLATFORM})
target_include_directories(sqlite3 PUBLIC ${GEN})
target_include_directories(sqlite3 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

set(SQLITE_CFLAGS
    ${USERSPACE_CFLAGS}
    -Wno-conversion
    -Wno-sign-conversion
    -Wno-unused-parameter
    -Wno-sign-compare
    $<$<C_COMPILER_ID:GNU>:-Wno-missing-braces -Wno-implicit-fallthrough -Wno-unused-but-set-variable>
    $<$<C_COMPILER_ID:Clang>:-Wno-implicit-int-conversion -Wno-shorten-64-to-32 -Wno-implicit-int-float-conversion -Wno-implicit-float-conversion>
)

target_compile_options(sqlite3 PRIVATE ${SQLITE_CFLAGS} -fvisibility=default)
target_link_libraries(sqlite3 PRIVATE moxnox)

add_executable(sqlite3_exe ${GEN}/shell.c)
set_target_properties(sqlite3_exe PROPERTIES OUTPUT_NAME sqlite3)

target_compile_definitions(sqlite3_exe PRIVATE
    ${SQLITE_FEATURES}
    ${SQLITE_PLATFORM}
    SQLITE_DQS=0
    SQLITE_STRICT_SUBTYPE=1
    SQLITE_HAVE_ZLIB=1
    HAVE_READLINE=1
)

target_compile_options(sqlite3_exe PRIVATE ${SQLITE_CFLAGS})
target_link_libraries(sqlite3_exe PRIVATE sqlite3 readline zlib moxnox)

install(TARGETS sqlite3 DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
install(TARGETS sqlite3_exe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(FILES ${GEN}/sqlite3.h ${GEN}/sqlite3ext.h DESTINATION ${CMAKE_INSTALL_PREFIX}/include)
install(FILES repo/sqlite3.1 DESTINATION ${CMAKE_INSTALL_PREFIX}/share/man/man1)

set(prefix "")
set(exec_prefix "\${prefix}")
set(libdir "\${exec_prefix}/lib")
set(includedir "\${prefix}/include")
set(PACKAGE_VERSION ${PROJECT_VERSION})
set(LDFLAGS_MATH "")
set(LDFLAGS_ZLIB "")
set(LDFLAGS_DLOPEN "")
set(LDFLAGS_PTHREAD "")
set(LDFLAGS_ICU "")
configure_file(repo/sqlite3.pc.in ${CMAKE_CURRENT_BINARY_DIR}/sqlite3.pc @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/sqlite3.pc DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/pkgconfig)

add_custom_target(sqlite)
add_dependencies(sqlite sqlite3 sqlite3_exe)
