project(bash VERSION 5.3)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "x86_64")
    set(BASH_HOSTTYPE x86_64)
    set(BASH_MACHTYPE x86_64-pc-moxnox)
elseif(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(BASH_HOSTTYPE aarch64)
    set(BASH_MACHTYPE aarch64-pc-moxnox)
endif()

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
file(MAKE_DIRECTORY ${GEN})
file(MAKE_DIRECTORY ${GEN}/lib)
file(MAKE_DIRECTORY ${GEN}/lib/sh)
file(MAKE_DIRECTORY ${GEN}/lib/glob)
file(MAKE_DIRECTORY ${GEN}/builtins)
file(MAKE_DIRECTORY ${GEN}/tools)

configure_file(config.h ${GEN}/config.h COPYONLY)

add_custom_command(
    OUTPUT ${GEN}/y.tab.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/y.tab.c ${GEN}/y.tab.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#    include <sys/param.h>\n|#    if defined (HAVE_SYS_PARAM_H)\n#    include <sys/param.h>\n#    endif\n|]]
            ${GEN}/y.tab.c
    DEPENDS repo/y.tab.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/jobs.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/jobs.c ${GEN}/jobs.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include <sys/ioctl.h>\n|#if defined (HAVE_SYS_IOCTL_H)\n#include <sys/ioctl.h>\n#endif\n|]]
            ${GEN}/jobs.c
    DEPENDS repo/jobs.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/sh/winsize.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/sh/winsize.c ${GEN}/lib/sh/winsize.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include <sys/ioctl.h>\n|#if defined (HAVE_SYS_IOCTL_H)\n#include <sys/ioctl.h>\n#endif\n|]]
            ${GEN}/lib/sh/winsize.c
    DEPENDS repo/lib/sh/winsize.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/glob/smatch.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/lib/glob/smatch.c ${GEN}/lib/glob/smatch.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#include "xmalloc.h"\n\n|#include "xmalloc.h"\n\n#if !defined (HAVE_BCOPY) \&\& !defined (bcopy)\n#  define bcopy(s, d, n) ((void) memcpy ((d), (s), (n)))\n#endif\n\n|]]
            ${GEN}/lib/glob/smatch.c
    DEPENDS repo/lib/glob/smatch.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/findcmd.c
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/findcmd.c ${GEN}/findcmd.c
    COMMAND ${HOST_SED} -z -i
            -e [[s|#ifdef __WIN32__\n|#if defined (__WIN32__) \|\| defined (__moxnox__)\n|]]
            -e [[s|  status = file_status (full_path);\n\n|  status = file_status (full_path);\n\n#if defined (__moxnox__)\n  /* Executables carry a .exe suffix; retry with it appended. */\n  if ((status \& FS_EXISTS) == 0)\n    {\n      char *dotexe;\n\n      dotexe = (char *)xmalloc (strlen (full_path) + 5);\n      strcpy (dotexe, full_path);\n      strcat (dotexe, ".exe");\n\n      status = file_status (dotexe);\n\n      if (status \& FS_EXISTS)\n\t{\n\t  free (full_path);\n\t  full_path = dotexe;\n\t}\n      else\n\tfree (dotexe);\n    }\n#endif\n\n|]]
            ${GEN}/findcmd.c
    DEPENDS repo/findcmd.c
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/signames.h ${GEN}/version.h ${GEN}/pathnames.h
        ${GEN}/builtins/pipesize.h
    COMMAND sh -c "cp '${CMAKE_CURRENT_SOURCE_DIR}/signames.h' '${CMAKE_CURRENT_SOURCE_DIR}/version.h' '${CMAKE_CURRENT_SOURCE_DIR}/pathnames.h' '${GEN}/' && cp '${CMAKE_CURRENT_SOURCE_DIR}/pipesize.h' '${GEN}/builtins/'"
    DEPENDS
        signames.h
        version.h
        pathnames.h
        pipesize.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/tools/mkbuiltins${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -O2 -DCROSS_COMPILING -I${CMAKE_CURRENT_SOURCE_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}/repo -I${CMAKE_CURRENT_SOURCE_DIR}/repo/include -o ${GEN}/tools/mkbuiltins${HOST_EXECUTABLE_SUFFIX} ${CMAKE_CURRENT_SOURCE_DIR}/repo/builtins/mkbuiltins.c
    DEPENDS
        repo/builtins/mkbuiltins.c
        buildconf.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/tools/mksyntax${HOST_EXECUTABLE_SUFFIX}
    COMMAND cc -O2 -DCROSS_COMPILING -I${CMAKE_CURRENT_SOURCE_DIR} -I${CMAKE_CURRENT_SOURCE_DIR}/repo -I${CMAKE_CURRENT_SOURCE_DIR}/repo/include -o ${GEN}/tools/mksyntax${HOST_EXECUTABLE_SUFFIX} ${CMAKE_CURRENT_SOURCE_DIR}/repo/mksyntax.c
    DEPENDS
        repo/mksyntax.c
        repo/syntax.h
        repo/include/chartypes.h
        buildconf.h
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/syntax.c
    COMMAND ${GEN}/tools/mksyntax${HOST_EXECUTABLE_SUFFIX} -o ${GEN}/syntax.c
    DEPENDS ${GEN}/tools/mksyntax${HOST_EXECUTABLE_SUFFIX}
    VERBATIM
)

set(BASH_BUILTIN_DEFS
    alias
    bind
    break
    builtin
    caller
    cd
    colon
    command
    complete
    declare
    echo
    enable
    eval
    exec
    exit
    fc
    fg_bg
    getopts
    hash
    help
    history
    jobs
    kill
    let
    mapfile
    printf
    pushd
    read
    return
    set
    setattr
    shift
    shopt
    source
    suspend
    test
    times
    trap
    type
    ulimit
    umask
    wait
)

set(BASH_BUILTIN_SRCS)
foreach(def ${BASH_BUILTIN_DEFS})
    add_custom_command(
        OUTPUT ${GEN}/builtins/${def}.c
        COMMAND ${GEN}/tools/mkbuiltins${HOST_EXECUTABLE_SUFFIX} -D ${CMAKE_CURRENT_SOURCE_DIR}/repo/builtins ${CMAKE_CURRENT_SOURCE_DIR}/repo/builtins/${def}.def
        WORKING_DIRECTORY ${GEN}/builtins
        DEPENDS
            ${GEN}/tools/mkbuiltins${HOST_EXECUTABLE_SUFFIX}
            repo/builtins/${def}.def
        VERBATIM
    )
    list(APPEND BASH_BUILTIN_SRCS ${GEN}/builtins/${def}.c)
endforeach()

set(BASH_ALL_DEF_FILES)
foreach(def
    alias bind break builtin cd colon command complete caller declare echo
    enable eval exec exit fc fg_bg hash help history jobs kill let read
    return set setattr shift source suspend test times trap type ulimit
    umask wait getopts reserved pushd shopt printf mapfile)
    list(APPEND BASH_ALL_DEF_FILES ${CMAKE_CURRENT_SOURCE_DIR}/repo/builtins/${def}.def)
endforeach()

add_custom_command(
    OUTPUT ${GEN}/builtins/builtext.h ${GEN}/builtins/builtins.c
    COMMAND ${GEN}/tools/mkbuiltins${HOST_EXECUTABLE_SUFFIX} -externfile ${GEN}/builtins/builtext.h -includefile builtext.h -structfile ${GEN}/builtins/builtins.c -noproduction -D ${CMAKE_CURRENT_SOURCE_DIR}/repo/builtins ${BASH_ALL_DEF_FILES}
    DEPENDS
        ${GEN}/tools/mkbuiltins${HOST_EXECUTABLE_SUFFIX}
        ${BASH_ALL_DEF_FILES}
    VERBATIM
)

add_executable(bash
    ${GEN}/signames.h
    ${GEN}/version.h
    ${GEN}/pathnames.h
    ${GEN}/builtins/pipesize.h
    ${GEN}/builtins/builtext.h
    ${GEN}/y.tab.c
    ${GEN}/jobs.c
    ${GEN}/syntax.c
    ${GEN}/builtins/builtins.c
    ${BASH_BUILTIN_SRCS}
    ${GEN}/lib/glob/smatch.c
    ${GEN}/lib/sh/winsize.c
    repo/alias.c
    repo/array.c
    repo/arrayfunc.c
    repo/assoc.c
    repo/bashhist.c
    repo/bashline.c
    repo/bracecomp.c
    repo/braces.c
    repo/copy_cmd.c
    repo/dispose_cmd.c
    repo/error.c
    repo/eval.c
    repo/execute_cmd.c
    repo/expr.c
    ${GEN}/findcmd.c
    repo/flags.c
    repo/general.c
    repo/hashcmd.c
    repo/hashlib.c
    repo/input.c
    repo/list.c
    repo/locale.c
    repo/mailcheck.c
    repo/make_cmd.c
    repo/pathexp.c
    repo/pcomplete.c
    repo/pcomplib.c
    repo/print_cmd.c
    repo/redir.c
    repo/shell.c
    repo/sig.c
    repo/stringlib.c
    repo/subst.c
    repo/test.c
    repo/trap.c
    repo/unwind_prot.c
    repo/variables.c
    repo/version.c
    repo/xmalloc.c
    repo/support/signames.c
    repo/builtins/bashgetopt.c
    repo/builtins/common.c
    repo/builtins/evalfile.c
    repo/builtins/evalstring.c
    repo/builtins/getopt.c
    repo/lib/glob/glob.c
    repo/lib/glob/gmisc.c
    repo/lib/glob/strmatch.c
    repo/lib/glob/xmbsrtowcs.c
    repo/lib/tilde/tilde.c
    repo/lib/sh/anonfile.c
    repo/lib/sh/casemod.c
    repo/lib/sh/clktck.c
    repo/lib/sh/clock.c
    repo/lib/sh/compat.c
    repo/lib/sh/dprintf.c
    repo/lib/sh/eaccess.c
    repo/lib/sh/fmtullong.c
    repo/lib/sh/fmtulong.c
    repo/lib/sh/fmtumax.c
    repo/lib/sh/fnxform.c
    repo/lib/sh/fpurge.c
    repo/lib/sh/getenv.c
    repo/lib/sh/gettimeofday.c
    repo/lib/sh/inet_aton.c
    repo/lib/sh/input_avail.c
    repo/lib/sh/itos.c
    repo/lib/sh/mailstat.c
    repo/lib/sh/makepath.c
    repo/lib/sh/mbscasecmp.c
    repo/lib/sh/mbschr.c
    repo/lib/sh/mbscmp.c
    repo/lib/sh/mbsncmp.c
    repo/lib/sh/netconn.c
    repo/lib/sh/netopen.c
    repo/lib/sh/oslib.c
    repo/lib/sh/pathcanon.c
    repo/lib/sh/pathphys.c
    repo/lib/sh/random.c
    repo/lib/sh/setlinebuf.c
    repo/lib/sh/shmatch.c
    repo/lib/sh/shmbchar.c
    repo/lib/sh/shquote.c
    repo/lib/sh/shtty.c
    repo/lib/sh/snprintf.c
    repo/lib/sh/spell.c
    repo/lib/sh/strcasestr.c
    repo/lib/sh/strchrnul.c
    repo/lib/sh/stringlist.c
    repo/lib/sh/stringvec.c
    repo/lib/sh/strlcpy.c
    repo/lib/sh/strnlen.c
    repo/lib/sh/strscpy.c
    repo/lib/sh/strtrans.c
    repo/lib/sh/strvis.c
    repo/lib/sh/timers.c
    repo/lib/sh/timeval.c
    repo/lib/sh/tmpfile.c
    repo/lib/sh/uconvert.c
    repo/lib/sh/ufuncs.c
    repo/lib/sh/unicode.c
    repo/lib/sh/utf8.c
    repo/lib/sh/wcsdup.c
    repo/lib/sh/wcsnwidth.c
    repo/lib/sh/zcatfd.c
    repo/lib/sh/zgetline.c
    repo/lib/sh/zmapfd.c
    repo/lib/sh/zread.c
    repo/lib/sh/zwrite.c
)

target_compile_definitions(bash PRIVATE
    HAVE_CONFIG_H
    SHELL
    CROSS_COMPILING
    PROGRAM="bash"
    CONF_HOSTTYPE="${BASH_HOSTTYPE}"
    CONF_OSTYPE="moxnox"
    CONF_MACHTYPE="${BASH_MACHTYPE}"
    CONF_VENDOR="pc"
    LOCALEDIR="/share/locale"
    PACKAGE="bash"
)

target_include_directories(bash PRIVATE
    ${GEN}
    ${GEN}/builtins
    ${CMAKE_CURRENT_SOURCE_DIR}
    repo
    repo/include
    repo/lib
    repo/lib/glob
    repo/builtins
)

target_compile_options(bash PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-char-subscripts
    -Wno-parentheses
    -Wno-sign-compare
    -Wno-unused-label
    -Wno-unused-but-set-variable
    -Wno-empty-body
    -Wno-missing-field-initializers
    -Wno-implicit-fallthrough
    -Wno-type-limits
    -Wno-cast-function-type
    -Wno-unterminated-string-initialization
    -Wno-unused-value
    -Wno-missing-braces
    -Wno-format-zero-length
    $<$<C_COMPILER_ID:GNU>:-Wno-discarded-qualifiers -Wno-clobbered -Wno-builtin-declaration-mismatch>
    $<$<C_COMPILER_ID:Clang>:-Wno-incompatible-pointer-types-discards-qualifiers -Wno-incompatible-library-redeclaration -Wno-sometimes-uninitialized -Wno-tautological-overlap-compare -Wno-tautological-constant-out-of-range-compare>
)

target_link_libraries(bash PRIVATE moxnox readline libiconv)

install(TARGETS bash DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(CODE "file(MAKE_DIRECTORY \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin\")")
install(CODE "file(CREATE_LINK bash.exe \"\$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/sh\" SYMBOLIC)")
