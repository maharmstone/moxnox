project(perl VERSION 5.42.0)

set(GEN ${CMAKE_CURRENT_BINARY_DIR}/gen)
set(REPO ${CMAKE_CURRENT_SOURCE_DIR}/repo)

if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
    set(PERL_ARCHNAME aarch64-moxnox)
    set(PERL_TARGETARCH aarch64-pc-moxnox)
    set(PERL_CC clang)
    set(PERL_CCNAME clang)
    set(PERL_GCCVERSION "")
    set(PERL_CPPSYMBOLS "__aarch64__=1 __ARM_64BIT_STATE=1 __BYTE_ORDER__=1234 __GNUC__=4 __GNUC_MINOR__=2 __clang__=1 __moxnox__=1 __STDC__=1")
else()
    set(PERL_ARCHNAME x86_64-moxnox)
    set(PERL_TARGETARCH x86_64-pc-moxnox)
    set(PERL_CC gcc)
    set(PERL_CCNAME gcc)
    set(PERL_GCCVERSION 16.2.0)
    set(PERL_CPPSYMBOLS "__amd64=1 __amd64__=1 __BYTE_ORDER__=1234 __GNUC__=16 __GNUC_MINOR__=2 __moxnox__=1 __pic__=1 __PIC__=1 __STDC__=1 __x86_64=1 __x86_64__=1")
endif()

set(PERL_PRIVLIB ${CMAKE_INSTALL_PREFIX}/lib/perl5/${PROJECT_VERSION})
set(PERL_ARCHLIB ${PERL_PRIVLIB}/${PERL_ARCHNAME})

set(PERL_CFLAGS ${USERSPACE_CFLAGS} -fvisibility=default -fwrapv -fno-strict-aliasing
    -Wno-sign-conversion
    -Wno-unused-parameter
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-attributes
    -Wno-conversion
)

file(MAKE_DIRECTORY ${GEN}/lib)
file(MAKE_DIRECTORY ${GEN}/lib/unicore)
file(MAKE_DIRECTORY ${GEN}/Porting)
file(MAKE_DIRECTORY ${GEN}/pod)
file(MAKE_DIRECTORY ${GEN}/pm/DynaLoader)
file(MAKE_DIRECTORY ${GEN}/pm/XSLoader)
file(MAKE_DIRECTORY ${GEN}/pm/lib)
file(MAKE_DIRECTORY ${GEN}/pm/Pod-Functions)
file(MAKE_DIRECTORY ${GEN}/pm/Devel-PPPort)
file(MAKE_DIRECTORY ${GEN}/pm/Unicode-Normalize)
file(MAKE_DIRECTORY ${GEN}/pm/Unicode-Collate/Collate)
file(MAKE_DIRECTORY ${GEN}/utils)

configure_file(config.h.in ${GEN}/config.h @ONLY)
configure_file(config.sh.in ${GEN}/config.sh @ONLY)

file(CONFIGURE OUTPUT ${GEN}/git_version.h CONTENT "#define PERL_PATCHNUM \"v5.42.0\"
#define PERL_GIT_UNPUSHED_COMMITS		\\
    /*leave-this-comment*/
" @ONLY)

file(CONFIGURE OUTPUT ${GEN}/lib/Config_git.pl CONTENT "$Config::Git_Data=<<'ENDOFGIT';
git_commit_id='5551b97c3be30b899397b99d57993a39d13294f1'
git_describe='v5.42.0'
git_branch=''
git_uncommitted_changes=''
git_commit_id_title='Commit id:'
git_commit_date='2025-07-02 09:05:08 +0200'
ENDOFGIT
" @ONLY)

###

add_custom_command(
    OUTPUT ${GEN}/Configure
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/Configure ${GEN}/Configure
    COMMAND ${HOST_SED} -z -i
            -e [[s|elif test -d c:/. -o -n "\$is_os2" ; then\n: OS/2 or cygwin\n    _exe=".exe"\nfi\n|elif test -d c:/. -o -n "$is_os2" ; then\n: OS/2 or cygwin\n    _exe=".exe"\nelif uname -s 2>/dev/null \| grep -i moxnox >/dev/null; then\n: Moxnox\n    _exe=".exe"\nfi\n|]]
            ${GEN}/Configure
    DEPENDS repo/Configure
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/host/miniperl${HOST_EXECUTABLE_SUFFIX} ${GEN}/host/generate_uudmap${HOST_EXECUTABLE_SUFFIX}
    COMMAND sh -c "rm -rf '${GEN}/host' && mkdir -p '${GEN}/host' && cd '${GEN}/host' && sh '${GEN}/Configure' -des -Dmksymlinks -Dsrc='${REPO}' -Uusenm -Duse64bitint -Dprefix=/nonexistent > configure.log 2>&1 && rm -f unixish.h pp_sys.c && cp '${GEN}/unixish.h' '${GEN}/pp_sys.c' . && '${HOST_MAKE}' miniperl${HOST_EXECUTABLE_SUFFIX} generate_uudmap${HOST_EXECUTABLE_SUFFIX} > make.log 2>&1 || (cat configure.log make.log; exit 1)"
    DEPENDS ${GEN}/Configure ${GEN}/unixish.h ${GEN}/pp_sys.c repo/Makefile.SH repo/miniperlmain.c
    VERBATIM
)

set(HOST_MINIPERL ${GEN}/host/miniperl${HOST_EXECUTABLE_SUFFIX})
set(MINIPERL ${HOST_MINIPERL} -I${GEN}/lib -I${REPO}/lib)

add_custom_command(
    OUTPUT ${GEN}/uudmap.h ${GEN}/bitcount.h ${GEN}/mg_data.h
    COMMAND ${GEN}/host/generate_uudmap${HOST_EXECUTABLE_SUFFIX} uudmap.h bitcount.h mg_data.h
    WORKING_DIRECTORY ${GEN}
    DEPENDS ${GEN}/host/generate_uudmap${HOST_EXECUTABLE_SUFFIX}
    VERBATIM
)

###

function(perl_patch relpath)
    get_filename_component(_name ${relpath} NAME)
    add_custom_command(
        OUTPUT ${GEN}/${_name}
        COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/${relpath} ${GEN}/${_name}
        COMMAND ${HOST_SED} -z -i ${ARGN} ${GEN}/${_name}
        DEPENDS repo/${relpath}
        VERBATIM
    )
endfunction()

add_custom_command(
    OUTPUT ${GEN}/perl.h
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/perl.h ${GEN}/perl.h
    DEPENDS repo/perl.h
    VERBATIM
)

perl_patch(unixish.h
    -e [[s|#define HAS_IOCTL               /\*\*/|#ifndef __moxnox__\n#define HAS_IOCTL               /**/\n#endif|]]
)

perl_patch(pp_sys.c
    -e [[s|#include "perl.h"\n|#include "perl.h"\n\n#ifndef h_addr\n# define h_addr h_addr_list[0]\n#endif\n|]]
)

add_custom_command(
    OUTPUT ${GEN}/ppport.h ${GEN}/lib/Devel/PPPort.pm ${GEN}/ext/Devel-PPPort/RealPPPort.xs
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${REPO}/dist/Devel-PPPort ${GEN}/pm/Devel-PPPort
    COMMAND ${MINIPERL} PPPort_pm.PL
    COMMAND ${MINIPERL} ppport_h.PL
    COMMAND ${MINIPERL} RealPPPort_xs.PL
    COMMAND ${CMAKE_COMMAND} -E copy ppport.h ${GEN}/ppport.h
    COMMAND ${CMAKE_COMMAND} -E copy PPPort.pm ${GEN}/lib/Devel/PPPort.pm
    COMMAND ${CMAKE_COMMAND} -E copy RealPPPort.xs ${GEN}/ext/Devel-PPPort/RealPPPort.xs
    WORKING_DIRECTORY ${GEN}/pm/Devel-PPPort
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/dist/Devel-PPPort/PPPort_pm.PL repo/dist/Devel-PPPort/ppport_h.PL repo/dist/Devel-PPPort/RealPPPort_xs.PL
    VERBATIM
)

add_custom_target(perl_headers DEPENDS
    ${GEN}/uudmap.h
    ${GEN}/bitcount.h
    ${GEN}/mg_data.h
    ${GEN}/perl.h
    ${GEN}/unixish.h
    ${GEN}/ppport.h
)

###

macro(perl_lib_file dest src)
    add_custom_command(
        OUTPUT ${GEN}/${dest}
        COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/${src} ${GEN}/${dest}
        DEPENDS repo/${src}
        VERBATIM
    )
    list(APPEND PERL_LIB_OUTPUTS ${GEN}/${dest})
endmacro()

perl_lib_file(lib/App/Cpan.pm cpan/CPAN/lib/App/Cpan.pm)
perl_lib_file(lib/App/Prove.pm cpan/Test-Harness/lib/App/Prove.pm)
perl_lib_file(lib/App/Prove/State.pm cpan/Test-Harness/lib/App/Prove/State.pm)
perl_lib_file(lib/App/Prove/State/Result.pm cpan/Test-Harness/lib/App/Prove/State/Result.pm)
perl_lib_file(lib/App/Prove/State/Result/Test.pm cpan/Test-Harness/lib/App/Prove/State/Result/Test.pm)
perl_lib_file(lib/Archive/Tar.pm cpan/Archive-Tar/lib/Archive/Tar.pm)
perl_lib_file(lib/Archive/Tar/Constant.pm cpan/Archive-Tar/lib/Archive/Tar/Constant.pm)
perl_lib_file(lib/Archive/Tar/File.pm cpan/Archive-Tar/lib/Archive/Tar/File.pm)
perl_lib_file(lib/Attribute/Handlers.pm dist/Attribute-Handlers/lib/Attribute/Handlers.pm)
perl_lib_file(lib/AutoLoader.pm cpan/AutoLoader/lib/AutoLoader.pm)
perl_lib_file(lib/AutoSplit.pm cpan/AutoLoader/lib/AutoSplit.pm)
perl_lib_file(lib/B.pm ext/B/B.pm)
perl_lib_file(lib/B/Concise.pm ext/B/B/Concise.pm)
perl_lib_file(lib/B/Showlex.pm ext/B/B/Showlex.pm)
perl_lib_file(lib/B/Terse.pm ext/B/B/Terse.pm)
perl_lib_file(lib/B/Xref.pm ext/B/B/Xref.pm)
perl_lib_file(lib/CPAN.pm cpan/CPAN/lib/CPAN.pm)
perl_lib_file(lib/CPAN/API/HOWTO.pod cpan/CPAN/lib/CPAN/API/HOWTO.pod)
perl_lib_file(lib/CPAN/Author.pm cpan/CPAN/lib/CPAN/Author.pm)
perl_lib_file(lib/CPAN/Bundle.pm cpan/CPAN/lib/CPAN/Bundle.pm)
perl_lib_file(lib/CPAN/CacheMgr.pm cpan/CPAN/lib/CPAN/CacheMgr.pm)
perl_lib_file(lib/CPAN/Complete.pm cpan/CPAN/lib/CPAN/Complete.pm)
perl_lib_file(lib/CPAN/Debug.pm cpan/CPAN/lib/CPAN/Debug.pm)
perl_lib_file(lib/CPAN/DeferredCode.pm cpan/CPAN/lib/CPAN/DeferredCode.pm)
perl_lib_file(lib/CPAN/Distribution.pm cpan/CPAN/lib/CPAN/Distribution.pm)
perl_lib_file(lib/CPAN/Distroprefs.pm cpan/CPAN/lib/CPAN/Distroprefs.pm)
perl_lib_file(lib/CPAN/Distrostatus.pm cpan/CPAN/lib/CPAN/Distrostatus.pm)
perl_lib_file(lib/CPAN/Exception/RecursiveDependency.pm cpan/CPAN/lib/CPAN/Exception/RecursiveDependency.pm)
perl_lib_file(lib/CPAN/Exception/blocked_urllist.pm cpan/CPAN/lib/CPAN/Exception/blocked_urllist.pm)
perl_lib_file(lib/CPAN/Exception/yaml_not_installed.pm cpan/CPAN/lib/CPAN/Exception/yaml_not_installed.pm)
perl_lib_file(lib/CPAN/Exception/yaml_process_error.pm cpan/CPAN/lib/CPAN/Exception/yaml_process_error.pm)
perl_lib_file(lib/CPAN/FTP.pm cpan/CPAN/lib/CPAN/FTP.pm)
perl_lib_file(lib/CPAN/FTP/netrc.pm cpan/CPAN/lib/CPAN/FTP/netrc.pm)
perl_lib_file(lib/CPAN/FirstTime.pm cpan/CPAN/lib/CPAN/FirstTime.pm)
perl_lib_file(lib/CPAN/HTTP/Client.pm cpan/CPAN/lib/CPAN/HTTP/Client.pm)
perl_lib_file(lib/CPAN/HTTP/Credentials.pm cpan/CPAN/lib/CPAN/HTTP/Credentials.pm)
perl_lib_file(lib/CPAN/HandleConfig.pm cpan/CPAN/lib/CPAN/HandleConfig.pm)
perl_lib_file(lib/CPAN/Index.pm cpan/CPAN/lib/CPAN/Index.pm)
perl_lib_file(lib/CPAN/InfoObj.pm cpan/CPAN/lib/CPAN/InfoObj.pm)
perl_lib_file(lib/CPAN/Kwalify.pm cpan/CPAN/lib/CPAN/Kwalify.pm)
perl_lib_file(lib/CPAN/Kwalify/distroprefs.dd cpan/CPAN/lib/CPAN/Kwalify/distroprefs.dd)
perl_lib_file(lib/CPAN/Kwalify/distroprefs.yml cpan/CPAN/lib/CPAN/Kwalify/distroprefs.yml)
perl_lib_file(lib/CPAN/LWP/UserAgent.pm cpan/CPAN/lib/CPAN/LWP/UserAgent.pm)
perl_lib_file(lib/CPAN/Meta.pm cpan/CPAN-Meta/lib/CPAN/Meta.pm)
perl_lib_file(lib/CPAN/Meta/Converter.pm cpan/CPAN-Meta/lib/CPAN/Meta/Converter.pm)
perl_lib_file(lib/CPAN/Meta/Feature.pm cpan/CPAN-Meta/lib/CPAN/Meta/Feature.pm)
perl_lib_file(lib/CPAN/Meta/History.pm cpan/CPAN-Meta/lib/CPAN/Meta/History.pm)
perl_lib_file(lib/CPAN/Meta/History/Meta_1_0.pod cpan/CPAN-Meta/lib/CPAN/Meta/History/Meta_1_0.pod)
perl_lib_file(lib/CPAN/Meta/History/Meta_1_1.pod cpan/CPAN-Meta/lib/CPAN/Meta/History/Meta_1_1.pod)
perl_lib_file(lib/CPAN/Meta/History/Meta_1_2.pod cpan/CPAN-Meta/lib/CPAN/Meta/History/Meta_1_2.pod)
perl_lib_file(lib/CPAN/Meta/History/Meta_1_3.pod cpan/CPAN-Meta/lib/CPAN/Meta/History/Meta_1_3.pod)
perl_lib_file(lib/CPAN/Meta/History/Meta_1_4.pod cpan/CPAN-Meta/lib/CPAN/Meta/History/Meta_1_4.pod)
perl_lib_file(lib/CPAN/Meta/Merge.pm cpan/CPAN-Meta/lib/CPAN/Meta/Merge.pm)
perl_lib_file(lib/CPAN/Meta/Prereqs.pm cpan/CPAN-Meta/lib/CPAN/Meta/Prereqs.pm)
perl_lib_file(lib/CPAN/Meta/Requirements.pm cpan/CPAN-Meta-Requirements/lib/CPAN/Meta/Requirements.pm)
perl_lib_file(lib/CPAN/Meta/Requirements/Range.pm cpan/CPAN-Meta-Requirements/lib/CPAN/Meta/Requirements/Range.pm)
perl_lib_file(lib/CPAN/Meta/Spec.pm cpan/CPAN-Meta/lib/CPAN/Meta/Spec.pm)
perl_lib_file(lib/CPAN/Meta/Validator.pm cpan/CPAN-Meta/lib/CPAN/Meta/Validator.pm)
perl_lib_file(lib/CPAN/Meta/YAML.pm cpan/CPAN-Meta-YAML/lib/CPAN/Meta/YAML.pm)
perl_lib_file(lib/CPAN/Mirrors.pm cpan/CPAN/lib/CPAN/Mirrors.pm)
perl_lib_file(lib/CPAN/Module.pm cpan/CPAN/lib/CPAN/Module.pm)
perl_lib_file(lib/CPAN/Nox.pm cpan/CPAN/lib/CPAN/Nox.pm)
perl_lib_file(lib/CPAN/Plugin.pm cpan/CPAN/lib/CPAN/Plugin.pm)
perl_lib_file(lib/CPAN/Plugin/Specfile.pm cpan/CPAN/lib/CPAN/Plugin/Specfile.pm)
perl_lib_file(lib/CPAN/Prompt.pm cpan/CPAN/lib/CPAN/Prompt.pm)
perl_lib_file(lib/CPAN/Queue.pm cpan/CPAN/lib/CPAN/Queue.pm)
perl_lib_file(lib/CPAN/Shell.pm cpan/CPAN/lib/CPAN/Shell.pm)
perl_lib_file(lib/CPAN/Tarzip.pm cpan/CPAN/lib/CPAN/Tarzip.pm)
perl_lib_file(lib/CPAN/URL.pm cpan/CPAN/lib/CPAN/URL.pm)
perl_lib_file(lib/CPAN/Version.pm cpan/CPAN/lib/CPAN/Version.pm)
perl_lib_file(lib/Carp.pm dist/Carp/lib/Carp.pm)
perl_lib_file(lib/Carp/Heavy.pm dist/Carp/lib/Carp/Heavy.pm)
perl_lib_file(lib/Compress/Raw/Bzip2.pm cpan/Compress-Raw-Bzip2/lib/Compress/Raw/Bzip2.pm)
perl_lib_file(lib/Compress/Raw/Zlib.pm cpan/Compress-Raw-Zlib/lib/Compress/Raw/Zlib.pm)
perl_lib_file(lib/Compress/Zlib.pm cpan/IO-Compress/lib/Compress/Zlib.pm)
perl_lib_file(lib/Config/Perl/V.pm cpan/Config-Perl-V/V.pm)
perl_lib_file(lib/Cwd.pm dist/PathTools/Cwd.pm)
perl_lib_file(lib/Data/Dumper.pm dist/Data-Dumper/Dumper.pm)
perl_lib_file(lib/Devel/Peek.pm ext/Devel-Peek/Peek.pm)
perl_lib_file(lib/Devel/SelfStubber.pm dist/Devel-SelfStubber/lib/Devel/SelfStubber.pm)
perl_lib_file(lib/Digest.pm cpan/Digest/lib/Digest.pm)
perl_lib_file(lib/Digest/MD5.pm cpan/Digest-MD5/MD5.pm)
perl_lib_file(lib/Digest/SHA.pm cpan/Digest-SHA/lib/Digest/SHA.pm)
perl_lib_file(lib/Digest/base.pm cpan/Digest/lib/Digest/base.pm)
perl_lib_file(lib/Digest/file.pm cpan/Digest/lib/Digest/file.pm)
perl_lib_file(lib/Dumpvalue.pm dist/Dumpvalue/lib/Dumpvalue.pm)
perl_lib_file(lib/Encode.pm cpan/Encode/Encode.pm)
perl_lib_file(lib/Encode/Alias.pm cpan/Encode/lib/Encode/Alias.pm)
perl_lib_file(lib/Encode/Byte.pm cpan/Encode/Byte/Byte.pm)
perl_lib_file(lib/Encode/CJKConstants.pm cpan/Encode/lib/Encode/CJKConstants.pm)
perl_lib_file(lib/Encode/CN.pm cpan/Encode/CN/CN.pm)
perl_lib_file(lib/Encode/CN/HZ.pm cpan/Encode/lib/Encode/CN/HZ.pm)
perl_lib_file(lib/Encode/Changes.e2x cpan/Encode/Encode/Changes.e2x)
perl_lib_file(lib/Encode/Config.pm cpan/Encode/lib/Encode/Config.pm)
perl_lib_file(lib/Encode/ConfigLocal_PM.e2x cpan/Encode/Encode/ConfigLocal_PM.e2x)
perl_lib_file(lib/Encode/EBCDIC.pm cpan/Encode/EBCDIC/EBCDIC.pm)
perl_lib_file(lib/Encode/Encoder.pm cpan/Encode/lib/Encode/Encoder.pm)
perl_lib_file(lib/Encode/Encoding.pm cpan/Encode/lib/Encode/Encoding.pm)
perl_lib_file(lib/Encode/GSM0338.pm cpan/Encode/lib/Encode/GSM0338.pm)
perl_lib_file(lib/Encode/Guess.pm cpan/Encode/lib/Encode/Guess.pm)
perl_lib_file(lib/Encode/JP.pm cpan/Encode/JP/JP.pm)
perl_lib_file(lib/Encode/JP/H2Z.pm cpan/Encode/lib/Encode/JP/H2Z.pm)
perl_lib_file(lib/Encode/JP/JIS7.pm cpan/Encode/lib/Encode/JP/JIS7.pm)
perl_lib_file(lib/Encode/KR.pm cpan/Encode/KR/KR.pm)
perl_lib_file(lib/Encode/KR/2022_KR.pm cpan/Encode/lib/Encode/KR/2022_KR.pm)
perl_lib_file(lib/Encode/MIME/Header.pm cpan/Encode/lib/Encode/MIME/Header.pm)
perl_lib_file(lib/Encode/MIME/Header/ISO_2022_JP.pm cpan/Encode/lib/Encode/MIME/Header/ISO_2022_JP.pm)
perl_lib_file(lib/Encode/MIME/Name.pm cpan/Encode/lib/Encode/MIME/Name.pm)
perl_lib_file(lib/Encode/Makefile_PL.e2x cpan/Encode/Encode/Makefile_PL.e2x)
perl_lib_file(lib/Encode/PerlIO.pod cpan/Encode/lib/Encode/PerlIO.pod)
perl_lib_file(lib/Encode/README.e2x cpan/Encode/Encode/README.e2x)
perl_lib_file(lib/Encode/Supported.pod cpan/Encode/lib/Encode/Supported.pod)
perl_lib_file(lib/Encode/Symbol.pm cpan/Encode/Symbol/Symbol.pm)
perl_lib_file(lib/Encode/TW.pm cpan/Encode/TW/TW.pm)
perl_lib_file(lib/Encode/Unicode.pm cpan/Encode/Unicode/Unicode.pm)
perl_lib_file(lib/Encode/Unicode/UTF7.pm cpan/Encode/lib/Encode/Unicode/UTF7.pm)
perl_lib_file(lib/Encode/_PM.e2x cpan/Encode/Encode/_PM.e2x)
perl_lib_file(lib/Encode/_T.e2x cpan/Encode/Encode/_T.e2x)
perl_lib_file(lib/Encode/encode.h cpan/Encode/Encode/encode.h)
perl_lib_file(lib/Env.pm dist/Env/lib/Env.pm)
perl_lib_file(lib/Exporter.pm dist/Exporter/lib/Exporter.pm)
perl_lib_file(lib/Exporter/Heavy.pm dist/Exporter/lib/Exporter/Heavy.pm)
perl_lib_file(lib/ExtUtils/CBuilder.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Base.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Base.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/Unix.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/Unix.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/VMS.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/VMS.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/Windows.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/Windows.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/Windows/BCC.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/Windows/BCC.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/Windows/GCC.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/Windows/GCC.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/Windows/MSVC.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/Windows/MSVC.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/aix.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/aix.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/android.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/android.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/cygwin.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/cygwin.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/darwin.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/darwin.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/dec_osf.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/dec_osf.pm)
perl_lib_file(lib/ExtUtils/CBuilder/Platform/os2.pm dist/ExtUtils-CBuilder/lib/ExtUtils/CBuilder/Platform/os2.pm)
perl_lib_file(lib/ExtUtils/Command.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/Command.pm)
perl_lib_file(lib/ExtUtils/Command/MM.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/Command/MM.pm)
perl_lib_file(lib/ExtUtils/Constant.pm cpan/ExtUtils-Constant/lib/ExtUtils/Constant.pm)
perl_lib_file(lib/ExtUtils/Constant/Base.pm cpan/ExtUtils-Constant/lib/ExtUtils/Constant/Base.pm)
perl_lib_file(lib/ExtUtils/Constant/ProxySubs.pm cpan/ExtUtils-Constant/lib/ExtUtils/Constant/ProxySubs.pm)
perl_lib_file(lib/ExtUtils/Constant/Utils.pm cpan/ExtUtils-Constant/lib/ExtUtils/Constant/Utils.pm)
perl_lib_file(lib/ExtUtils/Constant/XS.pm cpan/ExtUtils-Constant/lib/ExtUtils/Constant/XS.pm)
perl_lib_file(lib/ExtUtils/Install.pm cpan/ExtUtils-Install/lib/ExtUtils/Install.pm)
perl_lib_file(lib/ExtUtils/Installed.pm cpan/ExtUtils-Install/lib/ExtUtils/Installed.pm)
perl_lib_file(lib/ExtUtils/Liblist.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/Liblist.pm)
perl_lib_file(lib/ExtUtils/Liblist/Kid.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/Liblist/Kid.pm)
perl_lib_file(lib/ExtUtils/MANIFEST.SKIP cpan/ExtUtils-Manifest/lib/ExtUtils/MANIFEST.SKIP)
perl_lib_file(lib/ExtUtils/MM.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM.pm)
perl_lib_file(lib/ExtUtils/MM_AIX.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_AIX.pm)
perl_lib_file(lib/ExtUtils/MM_Any.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_Any.pm)
perl_lib_file(lib/ExtUtils/MM_BeOS.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_BeOS.pm)
perl_lib_file(lib/ExtUtils/MM_Cygwin.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_Cygwin.pm)
perl_lib_file(lib/ExtUtils/MM_DOS.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_DOS.pm)
perl_lib_file(lib/ExtUtils/MM_Darwin.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_Darwin.pm)
perl_lib_file(lib/ExtUtils/MM_MacOS.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_MacOS.pm)
perl_lib_file(lib/ExtUtils/MM_NW5.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_NW5.pm)
perl_lib_file(lib/ExtUtils/MM_OS2.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_OS2.pm)
perl_lib_file(lib/ExtUtils/MM_OS390.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_OS390.pm)
perl_lib_file(lib/ExtUtils/MM_QNX.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_QNX.pm)
perl_lib_file(lib/ExtUtils/MM_UWIN.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_UWIN.pm)
perl_lib_file(lib/ExtUtils/MM_Unix.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_Unix.pm)
perl_lib_file(lib/ExtUtils/MM_VMS.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_VMS.pm)
perl_lib_file(lib/ExtUtils/MM_VOS.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_VOS.pm)
perl_lib_file(lib/ExtUtils/MM_Win32.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_Win32.pm)
perl_lib_file(lib/ExtUtils/MM_Win95.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MM_Win95.pm)
perl_lib_file(lib/ExtUtils/MY.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MY.pm)
perl_lib_file(lib/ExtUtils/MakeMaker.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MakeMaker.pm)
perl_lib_file(lib/ExtUtils/MakeMaker/Config.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MakeMaker/Config.pm)
perl_lib_file(lib/ExtUtils/MakeMaker/FAQ.pod cpan/ExtUtils-MakeMaker/lib/ExtUtils/MakeMaker/FAQ.pod)
perl_lib_file(lib/ExtUtils/MakeMaker/Locale.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MakeMaker/Locale.pm)
perl_lib_file(lib/ExtUtils/MakeMaker/Tutorial.pod cpan/ExtUtils-MakeMaker/lib/ExtUtils/MakeMaker/Tutorial.pod)
perl_lib_file(lib/ExtUtils/MakeMaker/version.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MakeMaker/version.pm)
perl_lib_file(lib/ExtUtils/MakeMaker/version/regex.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/MakeMaker/version/regex.pm)
perl_lib_file(lib/ExtUtils/Manifest.pm cpan/ExtUtils-Manifest/lib/ExtUtils/Manifest.pm)
perl_lib_file(lib/ExtUtils/Miniperl.pm ext/ExtUtils-Miniperl/lib/ExtUtils/Miniperl.pm)
perl_lib_file(lib/ExtUtils/Mkbootstrap.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/Mkbootstrap.pm)
perl_lib_file(lib/ExtUtils/Mksymlists.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/Mksymlists.pm)
perl_lib_file(lib/ExtUtils/PL2Bat.pm cpan/ExtUtils-PL2Bat/lib/ExtUtils/PL2Bat.pm)
perl_lib_file(lib/ExtUtils/Packlist.pm cpan/ExtUtils-Install/lib/ExtUtils/Packlist.pm)
perl_lib_file(lib/ExtUtils/ParseXS.pm dist/ExtUtils-ParseXS/lib/ExtUtils/ParseXS.pm)
perl_lib_file(lib/ExtUtils/ParseXS.pod dist/ExtUtils-ParseXS/lib/ExtUtils/ParseXS.pod)
perl_lib_file(lib/ExtUtils/ParseXS/Constants.pm dist/ExtUtils-ParseXS/lib/ExtUtils/ParseXS/Constants.pm)
perl_lib_file(lib/ExtUtils/ParseXS/CountLines.pm dist/ExtUtils-ParseXS/lib/ExtUtils/ParseXS/CountLines.pm)
perl_lib_file(lib/ExtUtils/ParseXS/Eval.pm dist/ExtUtils-ParseXS/lib/ExtUtils/ParseXS/Eval.pm)
perl_lib_file(lib/ExtUtils/ParseXS/Node.pm dist/ExtUtils-ParseXS/lib/ExtUtils/ParseXS/Node.pm)
perl_lib_file(lib/ExtUtils/ParseXS/Utilities.pm dist/ExtUtils-ParseXS/lib/ExtUtils/ParseXS/Utilities.pm)
perl_lib_file(lib/ExtUtils/Typemaps.pm dist/ExtUtils-ParseXS/lib/ExtUtils/Typemaps.pm)
perl_lib_file(lib/ExtUtils/Typemaps/Cmd.pm dist/ExtUtils-ParseXS/lib/ExtUtils/Typemaps/Cmd.pm)
perl_lib_file(lib/ExtUtils/Typemaps/InputMap.pm dist/ExtUtils-ParseXS/lib/ExtUtils/Typemaps/InputMap.pm)
perl_lib_file(lib/ExtUtils/Typemaps/OutputMap.pm dist/ExtUtils-ParseXS/lib/ExtUtils/Typemaps/OutputMap.pm)
perl_lib_file(lib/ExtUtils/Typemaps/Type.pm dist/ExtUtils-ParseXS/lib/ExtUtils/Typemaps/Type.pm)
perl_lib_file(lib/ExtUtils/testlib.pm cpan/ExtUtils-MakeMaker/lib/ExtUtils/testlib.pm)
perl_lib_file(lib/ExtUtils/xsubpp dist/ExtUtils-ParseXS/lib/ExtUtils/xsubpp)
perl_lib_file(lib/Fatal.pm cpan/autodie/lib/Fatal.pm)
perl_lib_file(lib/Fcntl.pm ext/Fcntl/Fcntl.pm)
perl_lib_file(lib/File/DosGlob.pm ext/File-DosGlob/lib/File/DosGlob.pm)
perl_lib_file(lib/File/Fetch.pm cpan/File-Fetch/lib/File/Fetch.pm)
perl_lib_file(lib/File/Find.pm ext/File-Find/lib/File/Find.pm)
perl_lib_file(lib/File/Glob.pm ext/File-Glob/Glob.pm)
perl_lib_file(lib/File/GlobMapper.pm cpan/IO-Compress/lib/File/GlobMapper.pm)
perl_lib_file(lib/File/Path.pm cpan/File-Path/lib/File/Path.pm)
perl_lib_file(lib/File/Spec.pm dist/PathTools/lib/File/Spec.pm)
perl_lib_file(lib/File/Spec/AmigaOS.pm dist/PathTools/lib/File/Spec/AmigaOS.pm)
perl_lib_file(lib/File/Spec/Cygwin.pm dist/PathTools/lib/File/Spec/Cygwin.pm)
perl_lib_file(lib/File/Spec/Epoc.pm dist/PathTools/lib/File/Spec/Epoc.pm)
perl_lib_file(lib/File/Spec/Functions.pm dist/PathTools/lib/File/Spec/Functions.pm)
perl_lib_file(lib/File/Spec/Mac.pm dist/PathTools/lib/File/Spec/Mac.pm)
perl_lib_file(lib/File/Spec/OS2.pm dist/PathTools/lib/File/Spec/OS2.pm)
perl_lib_file(lib/File/Spec/Unix.pm dist/PathTools/lib/File/Spec/Unix.pm)
perl_lib_file(lib/File/Spec/VMS.pm dist/PathTools/lib/File/Spec/VMS.pm)
perl_lib_file(lib/File/Spec/Win32.pm dist/PathTools/lib/File/Spec/Win32.pm)
perl_lib_file(lib/File/Temp.pm cpan/File-Temp/lib/File/Temp.pm)
perl_lib_file(lib/FileCache.pm ext/FileCache/lib/FileCache.pm)
perl_lib_file(lib/Filter/Simple.pm dist/Filter-Simple/lib/Filter/Simple.pm)
perl_lib_file(lib/Filter/Util/Call.pm cpan/Filter-Util-Call/Call.pm)
perl_lib_file(lib/Filter/Util/filter-util.pl cpan/Filter-Util-Call/filter-util.pl)
perl_lib_file(lib/FindBin.pm dist/FindBin/lib/FindBin.pm)
perl_lib_file(lib/Getopt/Long.pm cpan/Getopt-Long/lib/Getopt/Long.pm)
perl_lib_file(lib/Getopt/Long/Parser.pm cpan/Getopt-Long/lib/Getopt/Long/Parser.pm)
perl_lib_file(lib/HTTP/Tiny.pm cpan/HTTP-Tiny/lib/HTTP/Tiny.pm)
perl_lib_file(lib/Hash/Util.pm ext/Hash-Util/lib/Hash/Util.pm)
perl_lib_file(lib/Hash/Util/FieldHash.pm ext/Hash-Util-FieldHash/lib/Hash/Util/FieldHash.pm)
perl_lib_file(lib/I18N/Collate.pm dist/I18N-Collate/lib/I18N/Collate.pm)
perl_lib_file(lib/I18N/LangTags.pm dist/I18N-LangTags/lib/I18N/LangTags.pm)
perl_lib_file(lib/I18N/LangTags/Detect.pm dist/I18N-LangTags/lib/I18N/LangTags/Detect.pm)
perl_lib_file(lib/I18N/LangTags/List.pm dist/I18N-LangTags/lib/I18N/LangTags/List.pm)
perl_lib_file(lib/I18N/Langinfo.pm ext/I18N-Langinfo/Langinfo.pm)
perl_lib_file(lib/IO.pm dist/IO/IO.pm)
perl_lib_file(lib/IO/Compress.pm cpan/IO-Compress/lib/IO/Compress.pm)
perl_lib_file(lib/IO/Compress/Adapter/Bzip2.pm cpan/IO-Compress/lib/IO/Compress/Adapter/Bzip2.pm)
perl_lib_file(lib/IO/Compress/Adapter/Deflate.pm cpan/IO-Compress/lib/IO/Compress/Adapter/Deflate.pm)
perl_lib_file(lib/IO/Compress/Adapter/Identity.pm cpan/IO-Compress/lib/IO/Compress/Adapter/Identity.pm)
perl_lib_file(lib/IO/Compress/Base.pm cpan/IO-Compress/lib/IO/Compress/Base.pm)
perl_lib_file(lib/IO/Compress/Base/Common.pm cpan/IO-Compress/lib/IO/Compress/Base/Common.pm)
perl_lib_file(lib/IO/Compress/Bzip2.pm cpan/IO-Compress/lib/IO/Compress/Bzip2.pm)
perl_lib_file(lib/IO/Compress/Deflate.pm cpan/IO-Compress/lib/IO/Compress/Deflate.pm)
perl_lib_file(lib/IO/Compress/FAQ.pod cpan/IO-Compress/lib/IO/Compress/FAQ.pod)
perl_lib_file(lib/IO/Compress/Gzip.pm cpan/IO-Compress/lib/IO/Compress/Gzip.pm)
perl_lib_file(lib/IO/Compress/Gzip/Constants.pm cpan/IO-Compress/lib/IO/Compress/Gzip/Constants.pm)
perl_lib_file(lib/IO/Compress/RawDeflate.pm cpan/IO-Compress/lib/IO/Compress/RawDeflate.pm)
perl_lib_file(lib/IO/Compress/Zip.pm cpan/IO-Compress/lib/IO/Compress/Zip.pm)
perl_lib_file(lib/IO/Compress/Zip/Constants.pm cpan/IO-Compress/lib/IO/Compress/Zip/Constants.pm)
perl_lib_file(lib/IO/Compress/Zlib/Constants.pm cpan/IO-Compress/lib/IO/Compress/Zlib/Constants.pm)
perl_lib_file(lib/IO/Compress/Zlib/Extra.pm cpan/IO-Compress/lib/IO/Compress/Zlib/Extra.pm)
perl_lib_file(lib/IO/Dir.pm dist/IO/lib/IO/Dir.pm)
perl_lib_file(lib/IO/File.pm dist/IO/lib/IO/File.pm)
perl_lib_file(lib/IO/Handle.pm dist/IO/lib/IO/Handle.pm)
perl_lib_file(lib/IO/Pipe.pm dist/IO/lib/IO/Pipe.pm)
perl_lib_file(lib/IO/Poll.pm dist/IO/lib/IO/Poll.pm)
perl_lib_file(lib/IO/Seekable.pm dist/IO/lib/IO/Seekable.pm)
perl_lib_file(lib/IO/Select.pm dist/IO/lib/IO/Select.pm)
perl_lib_file(lib/IO/Socket.pm dist/IO/lib/IO/Socket.pm)
perl_lib_file(lib/IO/Socket/INET.pm dist/IO/lib/IO/Socket/INET.pm)
perl_lib_file(lib/IO/Socket/IP.pm cpan/IO-Socket-IP/lib/IO/Socket/IP.pm)
perl_lib_file(lib/IO/Socket/UNIX.pm dist/IO/lib/IO/Socket/UNIX.pm)
perl_lib_file(lib/IO/Uncompress/Adapter/Bunzip2.pm cpan/IO-Compress/lib/IO/Uncompress/Adapter/Bunzip2.pm)
perl_lib_file(lib/IO/Uncompress/Adapter/Identity.pm cpan/IO-Compress/lib/IO/Uncompress/Adapter/Identity.pm)
perl_lib_file(lib/IO/Uncompress/Adapter/Inflate.pm cpan/IO-Compress/lib/IO/Uncompress/Adapter/Inflate.pm)
perl_lib_file(lib/IO/Uncompress/AnyInflate.pm cpan/IO-Compress/lib/IO/Uncompress/AnyInflate.pm)
perl_lib_file(lib/IO/Uncompress/AnyUncompress.pm cpan/IO-Compress/lib/IO/Uncompress/AnyUncompress.pm)
perl_lib_file(lib/IO/Uncompress/Base.pm cpan/IO-Compress/lib/IO/Uncompress/Base.pm)
perl_lib_file(lib/IO/Uncompress/Bunzip2.pm cpan/IO-Compress/lib/IO/Uncompress/Bunzip2.pm)
perl_lib_file(lib/IO/Uncompress/Gunzip.pm cpan/IO-Compress/lib/IO/Uncompress/Gunzip.pm)
perl_lib_file(lib/IO/Uncompress/Inflate.pm cpan/IO-Compress/lib/IO/Uncompress/Inflate.pm)
perl_lib_file(lib/IO/Uncompress/RawInflate.pm cpan/IO-Compress/lib/IO/Uncompress/RawInflate.pm)
perl_lib_file(lib/IO/Uncompress/Unzip.pm cpan/IO-Compress/lib/IO/Uncompress/Unzip.pm)
perl_lib_file(lib/IO/Zlib.pm cpan/IO-Zlib/Zlib.pm)
perl_lib_file(lib/IPC/Cmd.pm cpan/IPC-Cmd/lib/IPC/Cmd.pm)
perl_lib_file(lib/IPC/Msg.pm cpan/IPC-SysV/lib/IPC/Msg.pm)
perl_lib_file(lib/IPC/Open2.pm ext/IPC-Open3/lib/IPC/Open2.pm)
perl_lib_file(lib/IPC/Open3.pm ext/IPC-Open3/lib/IPC/Open3.pm)
perl_lib_file(lib/IPC/Semaphore.pm cpan/IPC-SysV/lib/IPC/Semaphore.pm)
perl_lib_file(lib/IPC/SharedMem.pm cpan/IPC-SysV/lib/IPC/SharedMem.pm)
perl_lib_file(lib/IPC/SysV.pm cpan/IPC-SysV/lib/IPC/SysV.pm)
perl_lib_file(lib/JSON/PP.pm cpan/JSON-PP/lib/JSON/PP.pm)
perl_lib_file(lib/JSON/PP/Boolean.pm cpan/JSON-PP/lib/JSON/PP/Boolean.pm)
perl_lib_file(lib/List/Util.pm cpan/Scalar-List-Utils/lib/List/Util.pm)
perl_lib_file(lib/List/Util/XS.pm cpan/Scalar-List-Utils/lib/List/Util/XS.pm)
perl_lib_file(lib/Locale/Maketext.pm dist/Locale-Maketext/lib/Locale/Maketext.pm)
perl_lib_file(lib/Locale/Maketext.pod dist/Locale-Maketext/lib/Locale/Maketext.pod)
perl_lib_file(lib/Locale/Maketext/Cookbook.pod dist/Locale-Maketext/lib/Locale/Maketext/Cookbook.pod)
perl_lib_file(lib/Locale/Maketext/Guts.pm dist/Locale-Maketext/lib/Locale/Maketext/Guts.pm)
perl_lib_file(lib/Locale/Maketext/GutsLoader.pm dist/Locale-Maketext/lib/Locale/Maketext/GutsLoader.pm)
perl_lib_file(lib/Locale/Maketext/Simple.pm cpan/Locale-Maketext-Simple/lib/Locale/Maketext/Simple.pm)
perl_lib_file(lib/Locale/Maketext/TPJ13.pod dist/Locale-Maketext/lib/Locale/Maketext/TPJ13.pod)
perl_lib_file(lib/MIME/Base64.pm cpan/MIME-Base64/lib/MIME/Base64.pm)
perl_lib_file(lib/MIME/QuotedPrint.pm cpan/MIME-Base64/lib/MIME/QuotedPrint.pm)
perl_lib_file(lib/Math/BigFloat.pm cpan/Math-BigInt/lib/Math/BigFloat.pm)
perl_lib_file(lib/Math/BigFloat/Trace.pm cpan/bignum/lib/Math/BigFloat/Trace.pm)
perl_lib_file(lib/Math/BigInt.pm cpan/Math-BigInt/lib/Math/BigInt.pm)
perl_lib_file(lib/Math/BigInt/Calc.pm cpan/Math-BigInt/lib/Math/BigInt/Calc.pm)
perl_lib_file(lib/Math/BigInt/FastCalc.pm cpan/Math-BigInt-FastCalc/lib/Math/BigInt/FastCalc.pm)
perl_lib_file(lib/Math/BigInt/Lib.pm cpan/Math-BigInt/lib/Math/BigInt/Lib.pm)
perl_lib_file(lib/Math/BigInt/Trace.pm cpan/bignum/lib/Math/BigInt/Trace.pm)
perl_lib_file(lib/Math/BigRat.pm cpan/Math-BigInt/lib/Math/BigRat.pm)
perl_lib_file(lib/Math/BigRat/Trace.pm cpan/bignum/lib/Math/BigRat/Trace.pm)
perl_lib_file(lib/Math/Complex.pm dist/Math-Complex/lib/Math/Complex.pm)
perl_lib_file(lib/Math/Trig.pm dist/Math-Complex/lib/Math/Trig.pm)
perl_lib_file(lib/Memoize.pm cpan/Memoize/Memoize.pm)
perl_lib_file(lib/Memoize/AnyDBM_File.pm cpan/Memoize/Memoize/AnyDBM_File.pm)
perl_lib_file(lib/Memoize/Expire.pm cpan/Memoize/Memoize/Expire.pm)
perl_lib_file(lib/Memoize/NDBM_File.pm cpan/Memoize/Memoize/NDBM_File.pm)
perl_lib_file(lib/Memoize/SDBM_File.pm cpan/Memoize/Memoize/SDBM_File.pm)
perl_lib_file(lib/Memoize/Storable.pm cpan/Memoize/Memoize/Storable.pm)
perl_lib_file(lib/Module/CoreList.pm dist/Module-CoreList/lib/Module/CoreList.pm)
perl_lib_file(lib/Module/CoreList.pod dist/Module-CoreList/lib/Module/CoreList.pod)
perl_lib_file(lib/Module/CoreList/Utils.pm dist/Module-CoreList/lib/Module/CoreList/Utils.pm)
perl_lib_file(lib/Module/Load.pm cpan/Module-Load/lib/Module/Load.pm)
perl_lib_file(lib/Module/Load/Conditional.pm cpan/Module-Load-Conditional/lib/Module/Load/Conditional.pm)
perl_lib_file(lib/Module/Loaded.pm cpan/Module-Loaded/lib/Module/Loaded.pm)
perl_lib_file(lib/Module/Metadata.pm cpan/Module-Metadata/lib/Module/Metadata.pm)
perl_lib_file(lib/NEXT.pm cpan/NEXT/lib/NEXT.pm)
perl_lib_file(lib/Net/Cmd.pm cpan/libnet/lib/Net/Cmd.pm)
perl_lib_file(lib/Net/Config.pm cpan/libnet/lib/Net/Config.pm)
perl_lib_file(lib/Net/Domain.pm cpan/libnet/lib/Net/Domain.pm)
perl_lib_file(lib/Net/FTP.pm cpan/libnet/lib/Net/FTP.pm)
perl_lib_file(lib/Net/FTP/A.pm cpan/libnet/lib/Net/FTP/A.pm)
perl_lib_file(lib/Net/FTP/E.pm cpan/libnet/lib/Net/FTP/E.pm)
perl_lib_file(lib/Net/FTP/I.pm cpan/libnet/lib/Net/FTP/I.pm)
perl_lib_file(lib/Net/FTP/L.pm cpan/libnet/lib/Net/FTP/L.pm)
perl_lib_file(lib/Net/FTP/dataconn.pm cpan/libnet/lib/Net/FTP/dataconn.pm)
perl_lib_file(lib/Net/NNTP.pm cpan/libnet/lib/Net/NNTP.pm)
perl_lib_file(lib/Net/Netrc.pm cpan/libnet/lib/Net/Netrc.pm)
perl_lib_file(lib/Net/POP3.pm cpan/libnet/lib/Net/POP3.pm)
perl_lib_file(lib/Net/Ping.pm dist/Net-Ping/lib/Net/Ping.pm)
perl_lib_file(lib/Net/SMTP.pm cpan/libnet/lib/Net/SMTP.pm)
perl_lib_file(lib/Net/Time.pm cpan/libnet/lib/Net/Time.pm)
perl_lib_file(lib/Net/libnetFAQ.pod cpan/libnet/lib/Net/libnetFAQ.pod)
perl_lib_file(lib/O.pm ext/B/O.pm)
perl_lib_file(lib/Opcode.pm ext/Opcode/Opcode.pm)
perl_lib_file(lib/POSIX.pm ext/POSIX/lib/POSIX.pm)
perl_lib_file(lib/POSIX.pod ext/POSIX/lib/POSIX.pod)
perl_lib_file(lib/Params/Check.pm cpan/Params-Check/lib/Params/Check.pm)
perl_lib_file(lib/Parse/CPAN/Meta.pm cpan/CPAN-Meta/lib/Parse/CPAN/Meta.pm)
perl_lib_file(lib/Perl/OSType.pm cpan/Perl-OSType/lib/Perl/OSType.pm)
perl_lib_file(lib/PerlIO/encoding.pm ext/PerlIO-encoding/encoding.pm)
perl_lib_file(lib/PerlIO/mmap.pm ext/PerlIO-mmap/mmap.pm)
perl_lib_file(lib/PerlIO/scalar.pm ext/PerlIO-scalar/scalar.pm)
perl_lib_file(lib/PerlIO/via.pm ext/PerlIO-via/via.pm)
perl_lib_file(lib/PerlIO/via/QuotedPrint.pm cpan/PerlIO-via-QuotedPrint/lib/PerlIO/via/QuotedPrint.pm)
perl_lib_file(lib/Pod/Checker.pm cpan/Pod-Checker/lib/Pod/Checker.pm)
perl_lib_file(lib/Pod/Escapes.pm cpan/Pod-Escapes/lib/Pod/Escapes.pm)
perl_lib_file(lib/Pod/Html.pm ext/Pod-Html/lib/Pod/Html.pm)
perl_lib_file(lib/Pod/Html/Util.pm ext/Pod-Html/lib/Pod/Html/Util.pm)
perl_lib_file(lib/Pod/Man.pm cpan/podlators/lib/Pod/Man.pm)
perl_lib_file(lib/Pod/ParseLink.pm cpan/podlators/lib/Pod/ParseLink.pm)
perl_lib_file(lib/Pod/Perldoc.pm cpan/Pod-Perldoc/lib/Pod/Perldoc.pm)
perl_lib_file(lib/Pod/Perldoc/BaseTo.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/BaseTo.pm)
perl_lib_file(lib/Pod/Perldoc/GetOptsOO.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/GetOptsOO.pm)
perl_lib_file(lib/Pod/Perldoc/ToANSI.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToANSI.pm)
perl_lib_file(lib/Pod/Perldoc/ToChecker.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToChecker.pm)
perl_lib_file(lib/Pod/Perldoc/ToMan.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToMan.pm)
perl_lib_file(lib/Pod/Perldoc/ToNroff.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToNroff.pm)
perl_lib_file(lib/Pod/Perldoc/ToPod.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToPod.pm)
perl_lib_file(lib/Pod/Perldoc/ToRtf.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToRtf.pm)
perl_lib_file(lib/Pod/Perldoc/ToTerm.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToTerm.pm)
perl_lib_file(lib/Pod/Perldoc/ToText.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToText.pm)
perl_lib_file(lib/Pod/Perldoc/ToTk.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToTk.pm)
perl_lib_file(lib/Pod/Perldoc/ToXml.pm cpan/Pod-Perldoc/lib/Pod/Perldoc/ToXml.pm)
perl_lib_file(lib/Pod/Simple.pm cpan/Pod-Simple/lib/Pod/Simple.pm)
perl_lib_file(lib/Pod/Simple.pod cpan/Pod-Simple/lib/Pod/Simple.pod)
perl_lib_file(lib/Pod/Simple/BlackBox.pm cpan/Pod-Simple/lib/Pod/Simple/BlackBox.pm)
perl_lib_file(lib/Pod/Simple/Checker.pm cpan/Pod-Simple/lib/Pod/Simple/Checker.pm)
perl_lib_file(lib/Pod/Simple/Debug.pm cpan/Pod-Simple/lib/Pod/Simple/Debug.pm)
perl_lib_file(lib/Pod/Simple/DumpAsText.pm cpan/Pod-Simple/lib/Pod/Simple/DumpAsText.pm)
perl_lib_file(lib/Pod/Simple/DumpAsXML.pm cpan/Pod-Simple/lib/Pod/Simple/DumpAsXML.pm)
perl_lib_file(lib/Pod/Simple/HTML.pm cpan/Pod-Simple/lib/Pod/Simple/HTML.pm)
perl_lib_file(lib/Pod/Simple/HTMLBatch.pm cpan/Pod-Simple/lib/Pod/Simple/HTMLBatch.pm)
perl_lib_file(lib/Pod/Simple/HTMLLegacy.pm cpan/Pod-Simple/lib/Pod/Simple/HTMLLegacy.pm)
perl_lib_file(lib/Pod/Simple/JustPod.pm cpan/Pod-Simple/lib/Pod/Simple/JustPod.pm)
perl_lib_file(lib/Pod/Simple/LinkSection.pm cpan/Pod-Simple/lib/Pod/Simple/LinkSection.pm)
perl_lib_file(lib/Pod/Simple/Methody.pm cpan/Pod-Simple/lib/Pod/Simple/Methody.pm)
perl_lib_file(lib/Pod/Simple/Progress.pm cpan/Pod-Simple/lib/Pod/Simple/Progress.pm)
perl_lib_file(lib/Pod/Simple/PullParser.pm cpan/Pod-Simple/lib/Pod/Simple/PullParser.pm)
perl_lib_file(lib/Pod/Simple/PullParserEndToken.pm cpan/Pod-Simple/lib/Pod/Simple/PullParserEndToken.pm)
perl_lib_file(lib/Pod/Simple/PullParserStartToken.pm cpan/Pod-Simple/lib/Pod/Simple/PullParserStartToken.pm)
perl_lib_file(lib/Pod/Simple/PullParserTextToken.pm cpan/Pod-Simple/lib/Pod/Simple/PullParserTextToken.pm)
perl_lib_file(lib/Pod/Simple/PullParserToken.pm cpan/Pod-Simple/lib/Pod/Simple/PullParserToken.pm)
perl_lib_file(lib/Pod/Simple/RTF.pm cpan/Pod-Simple/lib/Pod/Simple/RTF.pm)
perl_lib_file(lib/Pod/Simple/Search.pm cpan/Pod-Simple/lib/Pod/Simple/Search.pm)
perl_lib_file(lib/Pod/Simple/SimpleTree.pm cpan/Pod-Simple/lib/Pod/Simple/SimpleTree.pm)
perl_lib_file(lib/Pod/Simple/Subclassing.pod cpan/Pod-Simple/lib/Pod/Simple/Subclassing.pod)
perl_lib_file(lib/Pod/Simple/Text.pm cpan/Pod-Simple/lib/Pod/Simple/Text.pm)
perl_lib_file(lib/Pod/Simple/TextContent.pm cpan/Pod-Simple/lib/Pod/Simple/TextContent.pm)
perl_lib_file(lib/Pod/Simple/TiedOutFH.pm cpan/Pod-Simple/lib/Pod/Simple/TiedOutFH.pm)
perl_lib_file(lib/Pod/Simple/Transcode.pm cpan/Pod-Simple/lib/Pod/Simple/Transcode.pm)
perl_lib_file(lib/Pod/Simple/TranscodeDumb.pm cpan/Pod-Simple/lib/Pod/Simple/TranscodeDumb.pm)
perl_lib_file(lib/Pod/Simple/TranscodeSmart.pm cpan/Pod-Simple/lib/Pod/Simple/TranscodeSmart.pm)
perl_lib_file(lib/Pod/Simple/XHTML.pm cpan/Pod-Simple/lib/Pod/Simple/XHTML.pm)
perl_lib_file(lib/Pod/Simple/XMLOutStream.pm cpan/Pod-Simple/lib/Pod/Simple/XMLOutStream.pm)
perl_lib_file(lib/Pod/Text.pm cpan/podlators/lib/Pod/Text.pm)
perl_lib_file(lib/Pod/Text/Color.pm cpan/podlators/lib/Pod/Text/Color.pm)
perl_lib_file(lib/Pod/Text/Overstrike.pm cpan/podlators/lib/Pod/Text/Overstrike.pm)
perl_lib_file(lib/Pod/Text/Termcap.pm cpan/podlators/lib/Pod/Text/Termcap.pm)
perl_lib_file(lib/Pod/Usage.pm cpan/Pod-Usage/lib/Pod/Usage.pm)
perl_lib_file(lib/SDBM_File.pm ext/SDBM_File/SDBM_File.pm)
perl_lib_file(lib/Safe.pm dist/Safe/Safe.pm)
perl_lib_file(lib/Scalar/List/Utils.pm cpan/Scalar-List-Utils/lib/Scalar/List/Utils.pm)
perl_lib_file(lib/Scalar/Util.pm cpan/Scalar-List-Utils/lib/Scalar/Util.pm)
perl_lib_file(lib/Search/Dict.pm dist/Search-Dict/lib/Search/Dict.pm)
perl_lib_file(lib/SelfLoader.pm dist/SelfLoader/lib/SelfLoader.pm)
perl_lib_file(lib/Socket.pm cpan/Socket/Socket.pm)
perl_lib_file(lib/Storable.pm dist/Storable/lib/Storable.pm)
perl_lib_file(lib/Sub/Util.pm cpan/Scalar-List-Utils/lib/Sub/Util.pm)
perl_lib_file(lib/Sys/Hostname.pm ext/Sys-Hostname/Hostname.pm)
perl_lib_file(lib/Sys/Syslog.pm cpan/Sys-Syslog/Syslog.pm)
perl_lib_file(lib/TAP/Base.pm cpan/Test-Harness/lib/TAP/Base.pm)
perl_lib_file(lib/TAP/Formatter/Base.pm cpan/Test-Harness/lib/TAP/Formatter/Base.pm)
perl_lib_file(lib/TAP/Formatter/Color.pm cpan/Test-Harness/lib/TAP/Formatter/Color.pm)
perl_lib_file(lib/TAP/Formatter/Console.pm cpan/Test-Harness/lib/TAP/Formatter/Console.pm)
perl_lib_file(lib/TAP/Formatter/Console/ParallelSession.pm cpan/Test-Harness/lib/TAP/Formatter/Console/ParallelSession.pm)
perl_lib_file(lib/TAP/Formatter/Console/Session.pm cpan/Test-Harness/lib/TAP/Formatter/Console/Session.pm)
perl_lib_file(lib/TAP/Formatter/File.pm cpan/Test-Harness/lib/TAP/Formatter/File.pm)
perl_lib_file(lib/TAP/Formatter/File/Session.pm cpan/Test-Harness/lib/TAP/Formatter/File/Session.pm)
perl_lib_file(lib/TAP/Formatter/Session.pm cpan/Test-Harness/lib/TAP/Formatter/Session.pm)
perl_lib_file(lib/TAP/Harness.pm cpan/Test-Harness/lib/TAP/Harness.pm)
perl_lib_file(lib/TAP/Harness/Beyond.pod cpan/Test-Harness/lib/TAP/Harness/Beyond.pod)
perl_lib_file(lib/TAP/Harness/Env.pm cpan/Test-Harness/lib/TAP/Harness/Env.pm)
perl_lib_file(lib/TAP/Object.pm cpan/Test-Harness/lib/TAP/Object.pm)
perl_lib_file(lib/TAP/Parser.pm cpan/Test-Harness/lib/TAP/Parser.pm)
perl_lib_file(lib/TAP/Parser/Aggregator.pm cpan/Test-Harness/lib/TAP/Parser/Aggregator.pm)
perl_lib_file(lib/TAP/Parser/Grammar.pm cpan/Test-Harness/lib/TAP/Parser/Grammar.pm)
perl_lib_file(lib/TAP/Parser/Iterator.pm cpan/Test-Harness/lib/TAP/Parser/Iterator.pm)
perl_lib_file(lib/TAP/Parser/Iterator/Array.pm cpan/Test-Harness/lib/TAP/Parser/Iterator/Array.pm)
perl_lib_file(lib/TAP/Parser/Iterator/Process.pm cpan/Test-Harness/lib/TAP/Parser/Iterator/Process.pm)
perl_lib_file(lib/TAP/Parser/Iterator/Stream.pm cpan/Test-Harness/lib/TAP/Parser/Iterator/Stream.pm)
perl_lib_file(lib/TAP/Parser/IteratorFactory.pm cpan/Test-Harness/lib/TAP/Parser/IteratorFactory.pm)
perl_lib_file(lib/TAP/Parser/Multiplexer.pm cpan/Test-Harness/lib/TAP/Parser/Multiplexer.pm)
perl_lib_file(lib/TAP/Parser/Result.pm cpan/Test-Harness/lib/TAP/Parser/Result.pm)
perl_lib_file(lib/TAP/Parser/Result/Bailout.pm cpan/Test-Harness/lib/TAP/Parser/Result/Bailout.pm)
perl_lib_file(lib/TAP/Parser/Result/Comment.pm cpan/Test-Harness/lib/TAP/Parser/Result/Comment.pm)
perl_lib_file(lib/TAP/Parser/Result/Plan.pm cpan/Test-Harness/lib/TAP/Parser/Result/Plan.pm)
perl_lib_file(lib/TAP/Parser/Result/Pragma.pm cpan/Test-Harness/lib/TAP/Parser/Result/Pragma.pm)
perl_lib_file(lib/TAP/Parser/Result/Test.pm cpan/Test-Harness/lib/TAP/Parser/Result/Test.pm)
perl_lib_file(lib/TAP/Parser/Result/Unknown.pm cpan/Test-Harness/lib/TAP/Parser/Result/Unknown.pm)
perl_lib_file(lib/TAP/Parser/Result/Version.pm cpan/Test-Harness/lib/TAP/Parser/Result/Version.pm)
perl_lib_file(lib/TAP/Parser/Result/YAML.pm cpan/Test-Harness/lib/TAP/Parser/Result/YAML.pm)
perl_lib_file(lib/TAP/Parser/ResultFactory.pm cpan/Test-Harness/lib/TAP/Parser/ResultFactory.pm)
perl_lib_file(lib/TAP/Parser/Scheduler.pm cpan/Test-Harness/lib/TAP/Parser/Scheduler.pm)
perl_lib_file(lib/TAP/Parser/Scheduler/Job.pm cpan/Test-Harness/lib/TAP/Parser/Scheduler/Job.pm)
perl_lib_file(lib/TAP/Parser/Scheduler/Spinner.pm cpan/Test-Harness/lib/TAP/Parser/Scheduler/Spinner.pm)
perl_lib_file(lib/TAP/Parser/Source.pm cpan/Test-Harness/lib/TAP/Parser/Source.pm)
perl_lib_file(lib/TAP/Parser/SourceHandler.pm cpan/Test-Harness/lib/TAP/Parser/SourceHandler.pm)
perl_lib_file(lib/TAP/Parser/SourceHandler/Executable.pm cpan/Test-Harness/lib/TAP/Parser/SourceHandler/Executable.pm)
perl_lib_file(lib/TAP/Parser/SourceHandler/File.pm cpan/Test-Harness/lib/TAP/Parser/SourceHandler/File.pm)
perl_lib_file(lib/TAP/Parser/SourceHandler/Handle.pm cpan/Test-Harness/lib/TAP/Parser/SourceHandler/Handle.pm)
perl_lib_file(lib/TAP/Parser/SourceHandler/Perl.pm cpan/Test-Harness/lib/TAP/Parser/SourceHandler/Perl.pm)
perl_lib_file(lib/TAP/Parser/SourceHandler/RawTAP.pm cpan/Test-Harness/lib/TAP/Parser/SourceHandler/RawTAP.pm)
perl_lib_file(lib/TAP/Parser/YAMLish/Reader.pm cpan/Test-Harness/lib/TAP/Parser/YAMLish/Reader.pm)
perl_lib_file(lib/TAP/Parser/YAMLish/Writer.pm cpan/Test-Harness/lib/TAP/Parser/YAMLish/Writer.pm)
perl_lib_file(lib/Term/ANSIColor.pm cpan/Term-ANSIColor/lib/Term/ANSIColor.pm)
perl_lib_file(lib/Term/Cap.pm cpan/Term-Cap/Cap.pm)
perl_lib_file(lib/Term/Complete.pm dist/Term-Complete/lib/Term/Complete.pm)
perl_lib_file(lib/Term/ReadLine.pm dist/Term-ReadLine/lib/Term/ReadLine.pm)
perl_lib_file(lib/Term/Table.pm cpan/Term-Table/lib/Term/Table.pm)
perl_lib_file(lib/Term/Table/Cell.pm cpan/Term-Table/lib/Term/Table/Cell.pm)
perl_lib_file(lib/Term/Table/CellStack.pm cpan/Term-Table/lib/Term/Table/CellStack.pm)
perl_lib_file(lib/Term/Table/HashBase.pm cpan/Term-Table/lib/Term/Table/HashBase.pm)
perl_lib_file(lib/Term/Table/LineBreak.pm cpan/Term-Table/lib/Term/Table/LineBreak.pm)
perl_lib_file(lib/Term/Table/Spacer.pm cpan/Term-Table/lib/Term/Table/Spacer.pm)
perl_lib_file(lib/Term/Table/Util.pm cpan/Term-Table/lib/Term/Table/Util.pm)
perl_lib_file(lib/Test.pm dist/Test/lib/Test.pm)
perl_lib_file(lib/Test/Builder.pm cpan/Test-Simple/lib/Test/Builder.pm)
perl_lib_file(lib/Test/Builder/Formatter.pm cpan/Test-Simple/lib/Test/Builder/Formatter.pm)
perl_lib_file(lib/Test/Builder/Module.pm cpan/Test-Simple/lib/Test/Builder/Module.pm)
perl_lib_file(lib/Test/Builder/Tester.pm cpan/Test-Simple/lib/Test/Builder/Tester.pm)
perl_lib_file(lib/Test/Builder/Tester/Color.pm cpan/Test-Simple/lib/Test/Builder/Tester/Color.pm)
perl_lib_file(lib/Test/Builder/TodoDiag.pm cpan/Test-Simple/lib/Test/Builder/TodoDiag.pm)
perl_lib_file(lib/Test/Harness.pm cpan/Test-Harness/lib/Test/Harness.pm)
perl_lib_file(lib/Test/More.pm cpan/Test-Simple/lib/Test/More.pm)
perl_lib_file(lib/Test/Simple.pm cpan/Test-Simple/lib/Test/Simple.pm)
perl_lib_file(lib/Test/Tester.pm cpan/Test-Simple/lib/Test/Tester.pm)
perl_lib_file(lib/Test/Tester/Capture.pm cpan/Test-Simple/lib/Test/Tester/Capture.pm)
perl_lib_file(lib/Test/Tester/CaptureRunner.pm cpan/Test-Simple/lib/Test/Tester/CaptureRunner.pm)
perl_lib_file(lib/Test/Tester/Delegate.pm cpan/Test-Simple/lib/Test/Tester/Delegate.pm)
perl_lib_file(lib/Test/Tutorial.pod cpan/Test-Simple/lib/Test/Tutorial.pod)
perl_lib_file(lib/Test/use/ok.pm cpan/Test-Simple/lib/Test/use/ok.pm)
perl_lib_file(lib/Test2.pm cpan/Test-Simple/lib/Test2.pm)
perl_lib_file(lib/Test2/API.pm cpan/Test-Simple/lib/Test2/API.pm)
perl_lib_file(lib/Test2/API/Breakage.pm cpan/Test-Simple/lib/Test2/API/Breakage.pm)
perl_lib_file(lib/Test2/API/Context.pm cpan/Test-Simple/lib/Test2/API/Context.pm)
perl_lib_file(lib/Test2/API/Instance.pm cpan/Test-Simple/lib/Test2/API/Instance.pm)
perl_lib_file(lib/Test2/API/InterceptResult.pm cpan/Test-Simple/lib/Test2/API/InterceptResult.pm)
perl_lib_file(lib/Test2/API/InterceptResult/Event.pm cpan/Test-Simple/lib/Test2/API/InterceptResult/Event.pm)
perl_lib_file(lib/Test2/API/InterceptResult/Facet.pm cpan/Test-Simple/lib/Test2/API/InterceptResult/Facet.pm)
perl_lib_file(lib/Test2/API/InterceptResult/Hub.pm cpan/Test-Simple/lib/Test2/API/InterceptResult/Hub.pm)
perl_lib_file(lib/Test2/API/InterceptResult/Squasher.pm cpan/Test-Simple/lib/Test2/API/InterceptResult/Squasher.pm)
perl_lib_file(lib/Test2/API/Stack.pm cpan/Test-Simple/lib/Test2/API/Stack.pm)
perl_lib_file(lib/Test2/AsyncSubtest.pm cpan/Test-Simple/lib/Test2/AsyncSubtest.pm)
perl_lib_file(lib/Test2/AsyncSubtest/Event/Attach.pm cpan/Test-Simple/lib/Test2/AsyncSubtest/Event/Attach.pm)
perl_lib_file(lib/Test2/AsyncSubtest/Event/Detach.pm cpan/Test-Simple/lib/Test2/AsyncSubtest/Event/Detach.pm)
perl_lib_file(lib/Test2/AsyncSubtest/Formatter.pm cpan/Test-Simple/lib/Test2/AsyncSubtest/Formatter.pm)
perl_lib_file(lib/Test2/AsyncSubtest/Hub.pm cpan/Test-Simple/lib/Test2/AsyncSubtest/Hub.pm)
perl_lib_file(lib/Test2/Bundle.pm cpan/Test-Simple/lib/Test2/Bundle.pm)
perl_lib_file(lib/Test2/Bundle/Extended.pm cpan/Test-Simple/lib/Test2/Bundle/Extended.pm)
perl_lib_file(lib/Test2/Bundle/More.pm cpan/Test-Simple/lib/Test2/Bundle/More.pm)
perl_lib_file(lib/Test2/Bundle/Simple.pm cpan/Test-Simple/lib/Test2/Bundle/Simple.pm)
perl_lib_file(lib/Test2/Compare.pm cpan/Test-Simple/lib/Test2/Compare.pm)
perl_lib_file(lib/Test2/Compare/Array.pm cpan/Test-Simple/lib/Test2/Compare/Array.pm)
perl_lib_file(lib/Test2/Compare/Bag.pm cpan/Test-Simple/lib/Test2/Compare/Bag.pm)
perl_lib_file(lib/Test2/Compare/Base.pm cpan/Test-Simple/lib/Test2/Compare/Base.pm)
perl_lib_file(lib/Test2/Compare/Bool.pm cpan/Test-Simple/lib/Test2/Compare/Bool.pm)
perl_lib_file(lib/Test2/Compare/Custom.pm cpan/Test-Simple/lib/Test2/Compare/Custom.pm)
perl_lib_file(lib/Test2/Compare/DeepRef.pm cpan/Test-Simple/lib/Test2/Compare/DeepRef.pm)
perl_lib_file(lib/Test2/Compare/Delta.pm cpan/Test-Simple/lib/Test2/Compare/Delta.pm)
perl_lib_file(lib/Test2/Compare/Event.pm cpan/Test-Simple/lib/Test2/Compare/Event.pm)
perl_lib_file(lib/Test2/Compare/EventMeta.pm cpan/Test-Simple/lib/Test2/Compare/EventMeta.pm)
perl_lib_file(lib/Test2/Compare/Float.pm cpan/Test-Simple/lib/Test2/Compare/Float.pm)
perl_lib_file(lib/Test2/Compare/Hash.pm cpan/Test-Simple/lib/Test2/Compare/Hash.pm)
perl_lib_file(lib/Test2/Compare/Isa.pm cpan/Test-Simple/lib/Test2/Compare/Isa.pm)
perl_lib_file(lib/Test2/Compare/Meta.pm cpan/Test-Simple/lib/Test2/Compare/Meta.pm)
perl_lib_file(lib/Test2/Compare/Negatable.pm cpan/Test-Simple/lib/Test2/Compare/Negatable.pm)
perl_lib_file(lib/Test2/Compare/Number.pm cpan/Test-Simple/lib/Test2/Compare/Number.pm)
perl_lib_file(lib/Test2/Compare/Object.pm cpan/Test-Simple/lib/Test2/Compare/Object.pm)
perl_lib_file(lib/Test2/Compare/OrderedSubset.pm cpan/Test-Simple/lib/Test2/Compare/OrderedSubset.pm)
perl_lib_file(lib/Test2/Compare/Pattern.pm cpan/Test-Simple/lib/Test2/Compare/Pattern.pm)
perl_lib_file(lib/Test2/Compare/Ref.pm cpan/Test-Simple/lib/Test2/Compare/Ref.pm)
perl_lib_file(lib/Test2/Compare/Regex.pm cpan/Test-Simple/lib/Test2/Compare/Regex.pm)
perl_lib_file(lib/Test2/Compare/Scalar.pm cpan/Test-Simple/lib/Test2/Compare/Scalar.pm)
perl_lib_file(lib/Test2/Compare/Set.pm cpan/Test-Simple/lib/Test2/Compare/Set.pm)
perl_lib_file(lib/Test2/Compare/String.pm cpan/Test-Simple/lib/Test2/Compare/String.pm)
perl_lib_file(lib/Test2/Compare/Undef.pm cpan/Test-Simple/lib/Test2/Compare/Undef.pm)
perl_lib_file(lib/Test2/Compare/Wildcard.pm cpan/Test-Simple/lib/Test2/Compare/Wildcard.pm)
perl_lib_file(lib/Test2/Env.pm cpan/Test-Simple/lib/Test2/Env.pm)
perl_lib_file(lib/Test2/Event.pm cpan/Test-Simple/lib/Test2/Event.pm)
perl_lib_file(lib/Test2/Event/Bail.pm cpan/Test-Simple/lib/Test2/Event/Bail.pm)
perl_lib_file(lib/Test2/Event/Diag.pm cpan/Test-Simple/lib/Test2/Event/Diag.pm)
perl_lib_file(lib/Test2/Event/Encoding.pm cpan/Test-Simple/lib/Test2/Event/Encoding.pm)
perl_lib_file(lib/Test2/Event/Exception.pm cpan/Test-Simple/lib/Test2/Event/Exception.pm)
perl_lib_file(lib/Test2/Event/Fail.pm cpan/Test-Simple/lib/Test2/Event/Fail.pm)
perl_lib_file(lib/Test2/Event/Generic.pm cpan/Test-Simple/lib/Test2/Event/Generic.pm)
perl_lib_file(lib/Test2/Event/Note.pm cpan/Test-Simple/lib/Test2/Event/Note.pm)
perl_lib_file(lib/Test2/Event/Ok.pm cpan/Test-Simple/lib/Test2/Event/Ok.pm)
perl_lib_file(lib/Test2/Event/Pass.pm cpan/Test-Simple/lib/Test2/Event/Pass.pm)
perl_lib_file(lib/Test2/Event/Plan.pm cpan/Test-Simple/lib/Test2/Event/Plan.pm)
perl_lib_file(lib/Test2/Event/Skip.pm cpan/Test-Simple/lib/Test2/Event/Skip.pm)
perl_lib_file(lib/Test2/Event/Subtest.pm cpan/Test-Simple/lib/Test2/Event/Subtest.pm)
perl_lib_file(lib/Test2/Event/TAP/Version.pm cpan/Test-Simple/lib/Test2/Event/TAP/Version.pm)
perl_lib_file(lib/Test2/Event/V2.pm cpan/Test-Simple/lib/Test2/Event/V2.pm)
perl_lib_file(lib/Test2/Event/Waiting.pm cpan/Test-Simple/lib/Test2/Event/Waiting.pm)
perl_lib_file(lib/Test2/EventFacet.pm cpan/Test-Simple/lib/Test2/EventFacet.pm)
perl_lib_file(lib/Test2/EventFacet/About.pm cpan/Test-Simple/lib/Test2/EventFacet/About.pm)
perl_lib_file(lib/Test2/EventFacet/Amnesty.pm cpan/Test-Simple/lib/Test2/EventFacet/Amnesty.pm)
perl_lib_file(lib/Test2/EventFacet/Assert.pm cpan/Test-Simple/lib/Test2/EventFacet/Assert.pm)
perl_lib_file(lib/Test2/EventFacet/Control.pm cpan/Test-Simple/lib/Test2/EventFacet/Control.pm)
perl_lib_file(lib/Test2/EventFacet/Error.pm cpan/Test-Simple/lib/Test2/EventFacet/Error.pm)
perl_lib_file(lib/Test2/EventFacet/Hub.pm cpan/Test-Simple/lib/Test2/EventFacet/Hub.pm)
perl_lib_file(lib/Test2/EventFacet/Info.pm cpan/Test-Simple/lib/Test2/EventFacet/Info.pm)
perl_lib_file(lib/Test2/EventFacet/Info/Table.pm cpan/Test-Simple/lib/Test2/EventFacet/Info/Table.pm)
perl_lib_file(lib/Test2/EventFacet/Meta.pm cpan/Test-Simple/lib/Test2/EventFacet/Meta.pm)
perl_lib_file(lib/Test2/EventFacet/Parent.pm cpan/Test-Simple/lib/Test2/EventFacet/Parent.pm)
perl_lib_file(lib/Test2/EventFacet/Plan.pm cpan/Test-Simple/lib/Test2/EventFacet/Plan.pm)
perl_lib_file(lib/Test2/EventFacet/Render.pm cpan/Test-Simple/lib/Test2/EventFacet/Render.pm)
perl_lib_file(lib/Test2/EventFacet/Trace.pm cpan/Test-Simple/lib/Test2/EventFacet/Trace.pm)
perl_lib_file(lib/Test2/Formatter.pm cpan/Test-Simple/lib/Test2/Formatter.pm)
perl_lib_file(lib/Test2/Formatter/TAP.pm cpan/Test-Simple/lib/Test2/Formatter/TAP.pm)
perl_lib_file(lib/Test2/Hub.pm cpan/Test-Simple/lib/Test2/Hub.pm)
perl_lib_file(lib/Test2/Hub/Interceptor.pm cpan/Test-Simple/lib/Test2/Hub/Interceptor.pm)
perl_lib_file(lib/Test2/Hub/Interceptor/Terminator.pm cpan/Test-Simple/lib/Test2/Hub/Interceptor/Terminator.pm)
perl_lib_file(lib/Test2/Hub/Subtest.pm cpan/Test-Simple/lib/Test2/Hub/Subtest.pm)
perl_lib_file(lib/Test2/IPC.pm cpan/Test-Simple/lib/Test2/IPC.pm)
perl_lib_file(lib/Test2/IPC/Driver.pm cpan/Test-Simple/lib/Test2/IPC/Driver.pm)
perl_lib_file(lib/Test2/IPC/Driver/Files.pm cpan/Test-Simple/lib/Test2/IPC/Driver/Files.pm)
perl_lib_file(lib/Test2/Manual.pm cpan/Test-Simple/lib/Test2/Manual.pm)
perl_lib_file(lib/Test2/Manual/Anatomy.pm cpan/Test-Simple/lib/Test2/Manual/Anatomy.pm)
perl_lib_file(lib/Test2/Manual/Anatomy/API.pm cpan/Test-Simple/lib/Test2/Manual/Anatomy/API.pm)
perl_lib_file(lib/Test2/Manual/Anatomy/Context.pm cpan/Test-Simple/lib/Test2/Manual/Anatomy/Context.pm)
perl_lib_file(lib/Test2/Manual/Anatomy/EndToEnd.pm cpan/Test-Simple/lib/Test2/Manual/Anatomy/EndToEnd.pm)
perl_lib_file(lib/Test2/Manual/Anatomy/Event.pm cpan/Test-Simple/lib/Test2/Manual/Anatomy/Event.pm)
perl_lib_file(lib/Test2/Manual/Anatomy/Hubs.pm cpan/Test-Simple/lib/Test2/Manual/Anatomy/Hubs.pm)
perl_lib_file(lib/Test2/Manual/Anatomy/IPC.pm cpan/Test-Simple/lib/Test2/Manual/Anatomy/IPC.pm)
perl_lib_file(lib/Test2/Manual/Anatomy/Utilities.pm cpan/Test-Simple/lib/Test2/Manual/Anatomy/Utilities.pm)
perl_lib_file(lib/Test2/Manual/Concurrency.pm cpan/Test-Simple/lib/Test2/Manual/Concurrency.pm)
perl_lib_file(lib/Test2/Manual/Contributing.pm cpan/Test-Simple/lib/Test2/Manual/Contributing.pm)
perl_lib_file(lib/Test2/Manual/Testing.pm cpan/Test-Simple/lib/Test2/Manual/Testing.pm)
perl_lib_file(lib/Test2/Manual/Testing/Introduction.pm cpan/Test-Simple/lib/Test2/Manual/Testing/Introduction.pm)
perl_lib_file(lib/Test2/Manual/Testing/Migrating.pm cpan/Test-Simple/lib/Test2/Manual/Testing/Migrating.pm)
perl_lib_file(lib/Test2/Manual/Testing/Planning.pm cpan/Test-Simple/lib/Test2/Manual/Testing/Planning.pm)
perl_lib_file(lib/Test2/Manual/Testing/Todo.pm cpan/Test-Simple/lib/Test2/Manual/Testing/Todo.pm)
perl_lib_file(lib/Test2/Manual/Tooling.pm cpan/Test-Simple/lib/Test2/Manual/Tooling.pm)
perl_lib_file(lib/Test2/Manual/Tooling/FirstTool.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/FirstTool.pm)
perl_lib_file(lib/Test2/Manual/Tooling/Formatter.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/Formatter.pm)
perl_lib_file(lib/Test2/Manual/Tooling/Nesting.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/Nesting.pm)
perl_lib_file(lib/Test2/Manual/Tooling/Plugin/TestExit.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/Plugin/TestExit.pm)
perl_lib_file(lib/Test2/Manual/Tooling/Plugin/TestingDone.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/Plugin/TestingDone.pm)
perl_lib_file(lib/Test2/Manual/Tooling/Plugin/ToolCompletes.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/Plugin/ToolCompletes.pm)
perl_lib_file(lib/Test2/Manual/Tooling/Plugin/ToolStarts.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/Plugin/ToolStarts.pm)
perl_lib_file(lib/Test2/Manual/Tooling/Subtest.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/Subtest.pm)
perl_lib_file(lib/Test2/Manual/Tooling/TestBuilder.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/TestBuilder.pm)
perl_lib_file(lib/Test2/Manual/Tooling/Testing.pm cpan/Test-Simple/lib/Test2/Manual/Tooling/Testing.pm)
perl_lib_file(lib/Test2/Mock.pm cpan/Test-Simple/lib/Test2/Mock.pm)
perl_lib_file(lib/Test2/Plugin.pm cpan/Test-Simple/lib/Test2/Plugin.pm)
perl_lib_file(lib/Test2/Plugin/BailOnFail.pm cpan/Test-Simple/lib/Test2/Plugin/BailOnFail.pm)
perl_lib_file(lib/Test2/Plugin/DieOnFail.pm cpan/Test-Simple/lib/Test2/Plugin/DieOnFail.pm)
perl_lib_file(lib/Test2/Plugin/ExitSummary.pm cpan/Test-Simple/lib/Test2/Plugin/ExitSummary.pm)
perl_lib_file(lib/Test2/Plugin/SRand.pm cpan/Test-Simple/lib/Test2/Plugin/SRand.pm)
perl_lib_file(lib/Test2/Plugin/Times.pm cpan/Test-Simple/lib/Test2/Plugin/Times.pm)
perl_lib_file(lib/Test2/Plugin/UTF8.pm cpan/Test-Simple/lib/Test2/Plugin/UTF8.pm)
perl_lib_file(lib/Test2/Require.pm cpan/Test-Simple/lib/Test2/Require.pm)
perl_lib_file(lib/Test2/Require/AuthorTesting.pm cpan/Test-Simple/lib/Test2/Require/AuthorTesting.pm)
perl_lib_file(lib/Test2/Require/AutomatedTesting.pm cpan/Test-Simple/lib/Test2/Require/AutomatedTesting.pm)
perl_lib_file(lib/Test2/Require/EnvVar.pm cpan/Test-Simple/lib/Test2/Require/EnvVar.pm)
perl_lib_file(lib/Test2/Require/ExtendedTesting.pm cpan/Test-Simple/lib/Test2/Require/ExtendedTesting.pm)
perl_lib_file(lib/Test2/Require/Fork.pm cpan/Test-Simple/lib/Test2/Require/Fork.pm)
perl_lib_file(lib/Test2/Require/Module.pm cpan/Test-Simple/lib/Test2/Require/Module.pm)
perl_lib_file(lib/Test2/Require/NonInteractiveTesting.pm cpan/Test-Simple/lib/Test2/Require/NonInteractiveTesting.pm)
perl_lib_file(lib/Test2/Require/Perl.pm cpan/Test-Simple/lib/Test2/Require/Perl.pm)
perl_lib_file(lib/Test2/Require/RealFork.pm cpan/Test-Simple/lib/Test2/Require/RealFork.pm)
perl_lib_file(lib/Test2/Require/ReleaseTesting.pm cpan/Test-Simple/lib/Test2/Require/ReleaseTesting.pm)
perl_lib_file(lib/Test2/Require/Threads.pm cpan/Test-Simple/lib/Test2/Require/Threads.pm)
perl_lib_file(lib/Test2/Suite.pm cpan/Test-Simple/lib/Test2/Suite.pm)
perl_lib_file(lib/Test2/Todo.pm cpan/Test-Simple/lib/Test2/Todo.pm)
perl_lib_file(lib/Test2/Tools.pm cpan/Test-Simple/lib/Test2/Tools.pm)
perl_lib_file(lib/Test2/Tools/AsyncSubtest.pm cpan/Test-Simple/lib/Test2/Tools/AsyncSubtest.pm)
perl_lib_file(lib/Test2/Tools/Basic.pm cpan/Test-Simple/lib/Test2/Tools/Basic.pm)
perl_lib_file(lib/Test2/Tools/Class.pm cpan/Test-Simple/lib/Test2/Tools/Class.pm)
perl_lib_file(lib/Test2/Tools/ClassicCompare.pm cpan/Test-Simple/lib/Test2/Tools/ClassicCompare.pm)
perl_lib_file(lib/Test2/Tools/Compare.pm cpan/Test-Simple/lib/Test2/Tools/Compare.pm)
perl_lib_file(lib/Test2/Tools/Defer.pm cpan/Test-Simple/lib/Test2/Tools/Defer.pm)
perl_lib_file(lib/Test2/Tools/Encoding.pm cpan/Test-Simple/lib/Test2/Tools/Encoding.pm)
perl_lib_file(lib/Test2/Tools/Event.pm cpan/Test-Simple/lib/Test2/Tools/Event.pm)
perl_lib_file(lib/Test2/Tools/Exception.pm cpan/Test-Simple/lib/Test2/Tools/Exception.pm)
perl_lib_file(lib/Test2/Tools/Exports.pm cpan/Test-Simple/lib/Test2/Tools/Exports.pm)
perl_lib_file(lib/Test2/Tools/GenTemp.pm cpan/Test-Simple/lib/Test2/Tools/GenTemp.pm)
perl_lib_file(lib/Test2/Tools/Grab.pm cpan/Test-Simple/lib/Test2/Tools/Grab.pm)
perl_lib_file(lib/Test2/Tools/Mock.pm cpan/Test-Simple/lib/Test2/Tools/Mock.pm)
perl_lib_file(lib/Test2/Tools/Ref.pm cpan/Test-Simple/lib/Test2/Tools/Ref.pm)
perl_lib_file(lib/Test2/Tools/Refcount.pm cpan/Test-Simple/lib/Test2/Tools/Refcount.pm)
perl_lib_file(lib/Test2/Tools/Spec.pm cpan/Test-Simple/lib/Test2/Tools/Spec.pm)
perl_lib_file(lib/Test2/Tools/Subtest.pm cpan/Test-Simple/lib/Test2/Tools/Subtest.pm)
perl_lib_file(lib/Test2/Tools/Target.pm cpan/Test-Simple/lib/Test2/Tools/Target.pm)
perl_lib_file(lib/Test2/Tools/Tester.pm cpan/Test-Simple/lib/Test2/Tools/Tester.pm)
perl_lib_file(lib/Test2/Tools/Tiny.pm cpan/Test-Simple/lib/Test2/Tools/Tiny.pm)
perl_lib_file(lib/Test2/Tools/Warnings.pm cpan/Test-Simple/lib/Test2/Tools/Warnings.pm)
perl_lib_file(lib/Test2/Transition.pod cpan/Test-Simple/lib/Test2/Transition.pod)
perl_lib_file(lib/Test2/Util.pm cpan/Test-Simple/lib/Test2/Util.pm)
perl_lib_file(lib/Test2/Util/ExternalMeta.pm cpan/Test-Simple/lib/Test2/Util/ExternalMeta.pm)
perl_lib_file(lib/Test2/Util/Facets2Legacy.pm cpan/Test-Simple/lib/Test2/Util/Facets2Legacy.pm)
perl_lib_file(lib/Test2/Util/Grabber.pm cpan/Test-Simple/lib/Test2/Util/Grabber.pm)
perl_lib_file(lib/Test2/Util/Guard.pm cpan/Test-Simple/lib/Test2/Util/Guard.pm)
perl_lib_file(lib/Test2/Util/HashBase.pm cpan/Test-Simple/lib/Test2/Util/HashBase.pm)
perl_lib_file(lib/Test2/Util/Importer.pm cpan/Test-Simple/lib/Test2/Util/Importer.pm)
perl_lib_file(lib/Test2/Util/Ref.pm cpan/Test-Simple/lib/Test2/Util/Ref.pm)
perl_lib_file(lib/Test2/Util/Sig.pm cpan/Test-Simple/lib/Test2/Util/Sig.pm)
perl_lib_file(lib/Test2/Util/Stash.pm cpan/Test-Simple/lib/Test2/Util/Stash.pm)
perl_lib_file(lib/Test2/Util/Sub.pm cpan/Test-Simple/lib/Test2/Util/Sub.pm)
perl_lib_file(lib/Test2/Util/Table.pm cpan/Test-Simple/lib/Test2/Util/Table.pm)
perl_lib_file(lib/Test2/Util/Table/Cell.pm cpan/Test-Simple/lib/Test2/Util/Table/Cell.pm)
perl_lib_file(lib/Test2/Util/Table/LineBreak.pm cpan/Test-Simple/lib/Test2/Util/Table/LineBreak.pm)
perl_lib_file(lib/Test2/Util/Term.pm cpan/Test-Simple/lib/Test2/Util/Term.pm)
perl_lib_file(lib/Test2/Util/Times.pm cpan/Test-Simple/lib/Test2/Util/Times.pm)
perl_lib_file(lib/Test2/Util/Trace.pm cpan/Test-Simple/lib/Test2/Util/Trace.pm)
perl_lib_file(lib/Test2/V0.pm cpan/Test-Simple/lib/Test2/V0.pm)
perl_lib_file(lib/Test2/Workflow.pm cpan/Test-Simple/lib/Test2/Workflow.pm)
perl_lib_file(lib/Test2/Workflow/BlockBase.pm cpan/Test-Simple/lib/Test2/Workflow/BlockBase.pm)
perl_lib_file(lib/Test2/Workflow/Build.pm cpan/Test-Simple/lib/Test2/Workflow/Build.pm)
perl_lib_file(lib/Test2/Workflow/Runner.pm cpan/Test-Simple/lib/Test2/Workflow/Runner.pm)
perl_lib_file(lib/Test2/Workflow/Task.pm cpan/Test-Simple/lib/Test2/Workflow/Task.pm)
perl_lib_file(lib/Test2/Workflow/Task/Action.pm cpan/Test-Simple/lib/Test2/Workflow/Task/Action.pm)
perl_lib_file(lib/Test2/Workflow/Task/Group.pm cpan/Test-Simple/lib/Test2/Workflow/Task/Group.pm)
perl_lib_file(lib/Text/Abbrev.pm dist/Text-Abbrev/lib/Text/Abbrev.pm)
perl_lib_file(lib/Text/Balanced.pm cpan/Text-Balanced/lib/Text/Balanced.pm)
perl_lib_file(lib/Text/ParseWords.pm cpan/Text-ParseWords/lib/Text/ParseWords.pm)
perl_lib_file(lib/Text/Tabs.pm cpan/Text-Tabs/lib/Text/Tabs.pm)
perl_lib_file(lib/Text/Wrap.pm cpan/Text-Tabs/lib/Text/Wrap.pm)
perl_lib_file(lib/Thread/Queue.pm dist/Thread-Queue/lib/Thread/Queue.pm)
perl_lib_file(lib/Thread/Semaphore.pm dist/Thread-Semaphore/lib/Thread/Semaphore.pm)
perl_lib_file(lib/Tie/File.pm dist/Tie-File/lib/Tie/File.pm)
perl_lib_file(lib/Tie/Hash/NamedCapture.pm ext/Tie-Hash-NamedCapture/NamedCapture.pm)
perl_lib_file(lib/Tie/Memoize.pm ext/Tie-Memoize/lib/Tie/Memoize.pm)
perl_lib_file(lib/Tie/RefHash.pm cpan/Tie-RefHash/lib/Tie/RefHash.pm)
perl_lib_file(lib/Time/HiRes.pm dist/Time-HiRes/HiRes.pm)
perl_lib_file(lib/Time/Local.pm cpan/Time-Local/lib/Time/Local.pm)
perl_lib_file(lib/Time/Piece.pm cpan/Time-Piece/Piece.pm)
perl_lib_file(lib/Time/Seconds.pm cpan/Time-Piece/Seconds.pm)
perl_lib_file(lib/Unicode/Collate.pm cpan/Unicode-Collate/Collate.pm)
perl_lib_file(lib/Unicode/Collate/CJK/Big5.pm cpan/Unicode-Collate/Collate/CJK/Big5.pm)
perl_lib_file(lib/Unicode/Collate/CJK/GB2312.pm cpan/Unicode-Collate/Collate/CJK/GB2312.pm)
perl_lib_file(lib/Unicode/Collate/CJK/JISX0208.pm cpan/Unicode-Collate/Collate/CJK/JISX0208.pm)
perl_lib_file(lib/Unicode/Collate/CJK/Korean.pm cpan/Unicode-Collate/Collate/CJK/Korean.pm)
perl_lib_file(lib/Unicode/Collate/CJK/Pinyin.pm cpan/Unicode-Collate/Collate/CJK/Pinyin.pm)
perl_lib_file(lib/Unicode/Collate/CJK/Stroke.pm cpan/Unicode-Collate/Collate/CJK/Stroke.pm)
perl_lib_file(lib/Unicode/Collate/CJK/Zhuyin.pm cpan/Unicode-Collate/Collate/CJK/Zhuyin.pm)
perl_lib_file(lib/Unicode/Collate/Locale.pm cpan/Unicode-Collate/Collate/Locale.pm)
perl_lib_file(lib/Unicode/Collate/Locale/af.pl cpan/Unicode-Collate/Collate/Locale/af.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ar.pl cpan/Unicode-Collate/Collate/Locale/ar.pl)
perl_lib_file(lib/Unicode/Collate/Locale/as.pl cpan/Unicode-Collate/Collate/Locale/as.pl)
perl_lib_file(lib/Unicode/Collate/Locale/az.pl cpan/Unicode-Collate/Collate/Locale/az.pl)
perl_lib_file(lib/Unicode/Collate/Locale/be.pl cpan/Unicode-Collate/Collate/Locale/be.pl)
perl_lib_file(lib/Unicode/Collate/Locale/bn.pl cpan/Unicode-Collate/Collate/Locale/bn.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ca.pl cpan/Unicode-Collate/Collate/Locale/ca.pl)
perl_lib_file(lib/Unicode/Collate/Locale/cs.pl cpan/Unicode-Collate/Collate/Locale/cs.pl)
perl_lib_file(lib/Unicode/Collate/Locale/cu.pl cpan/Unicode-Collate/Collate/Locale/cu.pl)
perl_lib_file(lib/Unicode/Collate/Locale/cy.pl cpan/Unicode-Collate/Collate/Locale/cy.pl)
perl_lib_file(lib/Unicode/Collate/Locale/da.pl cpan/Unicode-Collate/Collate/Locale/da.pl)
perl_lib_file(lib/Unicode/Collate/Locale/de_at_ph.pl cpan/Unicode-Collate/Collate/Locale/de_at_ph.pl)
perl_lib_file(lib/Unicode/Collate/Locale/de_phone.pl cpan/Unicode-Collate/Collate/Locale/de_phone.pl)
perl_lib_file(lib/Unicode/Collate/Locale/dsb.pl cpan/Unicode-Collate/Collate/Locale/dsb.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ee.pl cpan/Unicode-Collate/Collate/Locale/ee.pl)
perl_lib_file(lib/Unicode/Collate/Locale/eo.pl cpan/Unicode-Collate/Collate/Locale/eo.pl)
perl_lib_file(lib/Unicode/Collate/Locale/es.pl cpan/Unicode-Collate/Collate/Locale/es.pl)
perl_lib_file(lib/Unicode/Collate/Locale/es_trad.pl cpan/Unicode-Collate/Collate/Locale/es_trad.pl)
perl_lib_file(lib/Unicode/Collate/Locale/et.pl cpan/Unicode-Collate/Collate/Locale/et.pl)
perl_lib_file(lib/Unicode/Collate/Locale/fa.pl cpan/Unicode-Collate/Collate/Locale/fa.pl)
perl_lib_file(lib/Unicode/Collate/Locale/fi.pl cpan/Unicode-Collate/Collate/Locale/fi.pl)
perl_lib_file(lib/Unicode/Collate/Locale/fi_phone.pl cpan/Unicode-Collate/Collate/Locale/fi_phone.pl)
perl_lib_file(lib/Unicode/Collate/Locale/fil.pl cpan/Unicode-Collate/Collate/Locale/fil.pl)
perl_lib_file(lib/Unicode/Collate/Locale/fo.pl cpan/Unicode-Collate/Collate/Locale/fo.pl)
perl_lib_file(lib/Unicode/Collate/Locale/fr_ca.pl cpan/Unicode-Collate/Collate/Locale/fr_ca.pl)
perl_lib_file(lib/Unicode/Collate/Locale/gu.pl cpan/Unicode-Collate/Collate/Locale/gu.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ha.pl cpan/Unicode-Collate/Collate/Locale/ha.pl)
perl_lib_file(lib/Unicode/Collate/Locale/haw.pl cpan/Unicode-Collate/Collate/Locale/haw.pl)
perl_lib_file(lib/Unicode/Collate/Locale/he.pl cpan/Unicode-Collate/Collate/Locale/he.pl)
perl_lib_file(lib/Unicode/Collate/Locale/hi.pl cpan/Unicode-Collate/Collate/Locale/hi.pl)
perl_lib_file(lib/Unicode/Collate/Locale/hr.pl cpan/Unicode-Collate/Collate/Locale/hr.pl)
perl_lib_file(lib/Unicode/Collate/Locale/hu.pl cpan/Unicode-Collate/Collate/Locale/hu.pl)
perl_lib_file(lib/Unicode/Collate/Locale/hy.pl cpan/Unicode-Collate/Collate/Locale/hy.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ig.pl cpan/Unicode-Collate/Collate/Locale/ig.pl)
perl_lib_file(lib/Unicode/Collate/Locale/is.pl cpan/Unicode-Collate/Collate/Locale/is.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ja.pl cpan/Unicode-Collate/Collate/Locale/ja.pl)
perl_lib_file(lib/Unicode/Collate/Locale/kk.pl cpan/Unicode-Collate/Collate/Locale/kk.pl)
perl_lib_file(lib/Unicode/Collate/Locale/kl.pl cpan/Unicode-Collate/Collate/Locale/kl.pl)
perl_lib_file(lib/Unicode/Collate/Locale/kn.pl cpan/Unicode-Collate/Collate/Locale/kn.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ko.pl cpan/Unicode-Collate/Collate/Locale/ko.pl)
perl_lib_file(lib/Unicode/Collate/Locale/kok.pl cpan/Unicode-Collate/Collate/Locale/kok.pl)
perl_lib_file(lib/Unicode/Collate/Locale/lkt.pl cpan/Unicode-Collate/Collate/Locale/lkt.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ln.pl cpan/Unicode-Collate/Collate/Locale/ln.pl)
perl_lib_file(lib/Unicode/Collate/Locale/lt.pl cpan/Unicode-Collate/Collate/Locale/lt.pl)
perl_lib_file(lib/Unicode/Collate/Locale/lv.pl cpan/Unicode-Collate/Collate/Locale/lv.pl)
perl_lib_file(lib/Unicode/Collate/Locale/mk.pl cpan/Unicode-Collate/Collate/Locale/mk.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ml.pl cpan/Unicode-Collate/Collate/Locale/ml.pl)
perl_lib_file(lib/Unicode/Collate/Locale/mr.pl cpan/Unicode-Collate/Collate/Locale/mr.pl)
perl_lib_file(lib/Unicode/Collate/Locale/mt.pl cpan/Unicode-Collate/Collate/Locale/mt.pl)
perl_lib_file(lib/Unicode/Collate/Locale/nb.pl cpan/Unicode-Collate/Collate/Locale/nb.pl)
perl_lib_file(lib/Unicode/Collate/Locale/nn.pl cpan/Unicode-Collate/Collate/Locale/nn.pl)
perl_lib_file(lib/Unicode/Collate/Locale/nso.pl cpan/Unicode-Collate/Collate/Locale/nso.pl)
perl_lib_file(lib/Unicode/Collate/Locale/om.pl cpan/Unicode-Collate/Collate/Locale/om.pl)
perl_lib_file(lib/Unicode/Collate/Locale/or.pl cpan/Unicode-Collate/Collate/Locale/or.pl)
perl_lib_file(lib/Unicode/Collate/Locale/pa.pl cpan/Unicode-Collate/Collate/Locale/pa.pl)
perl_lib_file(lib/Unicode/Collate/Locale/pl.pl cpan/Unicode-Collate/Collate/Locale/pl.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ro.pl cpan/Unicode-Collate/Collate/Locale/ro.pl)
perl_lib_file(lib/Unicode/Collate/Locale/sa.pl cpan/Unicode-Collate/Collate/Locale/sa.pl)
perl_lib_file(lib/Unicode/Collate/Locale/se.pl cpan/Unicode-Collate/Collate/Locale/se.pl)
perl_lib_file(lib/Unicode/Collate/Locale/si.pl cpan/Unicode-Collate/Collate/Locale/si.pl)
perl_lib_file(lib/Unicode/Collate/Locale/si_dict.pl cpan/Unicode-Collate/Collate/Locale/si_dict.pl)
perl_lib_file(lib/Unicode/Collate/Locale/sk.pl cpan/Unicode-Collate/Collate/Locale/sk.pl)
perl_lib_file(lib/Unicode/Collate/Locale/sl.pl cpan/Unicode-Collate/Collate/Locale/sl.pl)
perl_lib_file(lib/Unicode/Collate/Locale/sq.pl cpan/Unicode-Collate/Collate/Locale/sq.pl)
perl_lib_file(lib/Unicode/Collate/Locale/sr.pl cpan/Unicode-Collate/Collate/Locale/sr.pl)
perl_lib_file(lib/Unicode/Collate/Locale/sv.pl cpan/Unicode-Collate/Collate/Locale/sv.pl)
perl_lib_file(lib/Unicode/Collate/Locale/sv_refo.pl cpan/Unicode-Collate/Collate/Locale/sv_refo.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ta.pl cpan/Unicode-Collate/Collate/Locale/ta.pl)
perl_lib_file(lib/Unicode/Collate/Locale/te.pl cpan/Unicode-Collate/Collate/Locale/te.pl)
perl_lib_file(lib/Unicode/Collate/Locale/th.pl cpan/Unicode-Collate/Collate/Locale/th.pl)
perl_lib_file(lib/Unicode/Collate/Locale/tn.pl cpan/Unicode-Collate/Collate/Locale/tn.pl)
perl_lib_file(lib/Unicode/Collate/Locale/to.pl cpan/Unicode-Collate/Collate/Locale/to.pl)
perl_lib_file(lib/Unicode/Collate/Locale/tr.pl cpan/Unicode-Collate/Collate/Locale/tr.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ug_cyrl.pl cpan/Unicode-Collate/Collate/Locale/ug_cyrl.pl)
perl_lib_file(lib/Unicode/Collate/Locale/uk.pl cpan/Unicode-Collate/Collate/Locale/uk.pl)
perl_lib_file(lib/Unicode/Collate/Locale/ur.pl cpan/Unicode-Collate/Collate/Locale/ur.pl)
perl_lib_file(lib/Unicode/Collate/Locale/vi.pl cpan/Unicode-Collate/Collate/Locale/vi.pl)
perl_lib_file(lib/Unicode/Collate/Locale/vo.pl cpan/Unicode-Collate/Collate/Locale/vo.pl)
perl_lib_file(lib/Unicode/Collate/Locale/wae.pl cpan/Unicode-Collate/Collate/Locale/wae.pl)
perl_lib_file(lib/Unicode/Collate/Locale/wo.pl cpan/Unicode-Collate/Collate/Locale/wo.pl)
perl_lib_file(lib/Unicode/Collate/Locale/yo.pl cpan/Unicode-Collate/Collate/Locale/yo.pl)
perl_lib_file(lib/Unicode/Collate/Locale/zh.pl cpan/Unicode-Collate/Collate/Locale/zh.pl)
perl_lib_file(lib/Unicode/Collate/Locale/zh_big5.pl cpan/Unicode-Collate/Collate/Locale/zh_big5.pl)
perl_lib_file(lib/Unicode/Collate/Locale/zh_gb.pl cpan/Unicode-Collate/Collate/Locale/zh_gb.pl)
perl_lib_file(lib/Unicode/Collate/Locale/zh_pin.pl cpan/Unicode-Collate/Collate/Locale/zh_pin.pl)
perl_lib_file(lib/Unicode/Collate/Locale/zh_strk.pl cpan/Unicode-Collate/Collate/Locale/zh_strk.pl)
perl_lib_file(lib/Unicode/Collate/Locale/zh_zhu.pl cpan/Unicode-Collate/Collate/Locale/zh_zhu.pl)
perl_lib_file(lib/Unicode/Collate/allkeys.txt cpan/Unicode-Collate/Collate/allkeys.txt)
perl_lib_file(lib/Unicode/Collate/keys.txt cpan/Unicode-Collate/Collate/keys.txt)
perl_lib_file(lib/Unicode/Normalize.pm dist/Unicode-Normalize/Normalize.pm)
perl_lib_file(lib/attributes.pm ext/attributes/attributes.pm)
perl_lib_file(lib/autodie.pm cpan/autodie/lib/autodie.pm)
perl_lib_file(lib/autodie/Scope/Guard.pm cpan/autodie/lib/autodie/Scope/Guard.pm)
perl_lib_file(lib/autodie/Scope/GuardStack.pm cpan/autodie/lib/autodie/Scope/GuardStack.pm)
perl_lib_file(lib/autodie/Util.pm cpan/autodie/lib/autodie/Util.pm)
perl_lib_file(lib/autodie/exception.pm cpan/autodie/lib/autodie/exception.pm)
perl_lib_file(lib/autodie/exception/system.pm cpan/autodie/lib/autodie/exception/system.pm)
perl_lib_file(lib/autodie/hints.pm cpan/autodie/lib/autodie/hints.pm)
perl_lib_file(lib/autodie/skip.pm cpan/autodie/lib/autodie/skip.pm)
perl_lib_file(lib/autouse.pm dist/autouse/lib/autouse.pm)
perl_lib_file(lib/base.pm dist/base/lib/base.pm)
perl_lib_file(lib/bigfloat.pm cpan/bignum/lib/bigfloat.pm)
perl_lib_file(lib/bigint.pm cpan/bignum/lib/bigint.pm)
perl_lib_file(lib/bignum.pm cpan/bignum/lib/bignum.pm)
perl_lib_file(lib/bigrat.pm cpan/bignum/lib/bigrat.pm)
perl_lib_file(lib/constant.pm dist/constant/lib/constant.pm)
perl_lib_file(lib/encoding.pm cpan/Encode/encoding.pm)
perl_lib_file(lib/encoding/warnings.pm dist/encoding-warnings/lib/encoding/warnings.pm)
perl_lib_file(lib/experimental.pm cpan/experimental/lib/experimental.pm)
perl_lib_file(lib/fields.pm dist/base/lib/fields.pm)
perl_lib_file(lib/if.pm dist/if/if.pm)
perl_lib_file(lib/mro.pm ext/mro/mro.pm)
perl_lib_file(lib/ok.pm cpan/Test-Simple/lib/ok.pm)
perl_lib_file(lib/ops.pm ext/Opcode/ops.pm)
perl_lib_file(lib/parent.pm cpan/parent/lib/parent.pm)
perl_lib_file(lib/perldoc.pod cpan/Pod-Perldoc/perldoc.pod)
perl_lib_file(lib/perlfaq.pm cpan/perlfaq/lib/perlfaq.pm)
perl_lib_file(lib/perlfaq.pod cpan/perlfaq/lib/perlfaq.pod)
perl_lib_file(lib/perlfaq1.pod cpan/perlfaq/lib/perlfaq1.pod)
perl_lib_file(lib/perlfaq2.pod cpan/perlfaq/lib/perlfaq2.pod)
perl_lib_file(lib/perlfaq3.pod cpan/perlfaq/lib/perlfaq3.pod)
perl_lib_file(lib/perlfaq4.pod cpan/perlfaq/lib/perlfaq4.pod)
perl_lib_file(lib/perlfaq5.pod cpan/perlfaq/lib/perlfaq5.pod)
perl_lib_file(lib/perlfaq6.pod cpan/perlfaq/lib/perlfaq6.pod)
perl_lib_file(lib/perlfaq7.pod cpan/perlfaq/lib/perlfaq7.pod)
perl_lib_file(lib/perlfaq8.pod cpan/perlfaq/lib/perlfaq8.pod)
perl_lib_file(lib/perlfaq9.pod cpan/perlfaq/lib/perlfaq9.pod)
perl_lib_file(lib/perlglossary.pod cpan/perlfaq/lib/perlglossary.pod)
perl_lib_file(lib/perlxs.pod dist/ExtUtils-ParseXS/lib/perlxs.pod)
perl_lib_file(lib/perlxstut.pod dist/ExtUtils-ParseXS/lib/perlxstut.pod)
perl_lib_file(lib/perlxstypemap.pod dist/ExtUtils-ParseXS/lib/perlxstypemap.pod)
perl_lib_file(lib/re.pm ext/re/re.pm)
perl_lib_file(lib/stable.pm cpan/experimental/lib/stable.pm)
perl_lib_file(lib/unicore/ArabicShaping.txt lib/unicore/ArabicShaping.txt)
perl_lib_file(lib/unicore/BidiBrackets.txt lib/unicore/BidiBrackets.txt)
perl_lib_file(lib/unicore/BidiMirroring.txt lib/unicore/BidiMirroring.txt)
perl_lib_file(lib/unicore/Blocks.txt lib/unicore/Blocks.txt)
perl_lib_file(lib/unicore/CJKRadicals.txt lib/unicore/CJKRadicals.txt)
perl_lib_file(lib/unicore/CaseFolding.txt lib/unicore/CaseFolding.txt)
perl_lib_file(lib/unicore/CompositionExclusions.txt lib/unicore/CompositionExclusions.txt)
perl_lib_file(lib/unicore/DAge.txt lib/unicore/DAge.txt)
perl_lib_file(lib/unicore/DCoreProperties.txt lib/unicore/DCoreProperties.txt)
perl_lib_file(lib/unicore/DNormalizationProps.txt lib/unicore/DNormalizationProps.txt)
perl_lib_file(lib/unicore/DoNotEmit.txt lib/unicore/DoNotEmit.txt)
perl_lib_file(lib/unicore/EastAsianWidth.txt lib/unicore/EastAsianWidth.txt)
perl_lib_file(lib/unicore/EmojiSources.txt lib/unicore/EmojiSources.txt)
perl_lib_file(lib/unicore/EquivalentUnifiedIdeograph.txt lib/unicore/EquivalentUnifiedIdeograph.txt)
perl_lib_file(lib/unicore/HangulSyllableType.txt lib/unicore/HangulSyllableType.txt)
perl_lib_file(lib/unicore/IdStatus.txt lib/unicore/IdStatus.txt)
perl_lib_file(lib/unicore/IdType.txt lib/unicore/IdType.txt)
perl_lib_file(lib/unicore/Index.txt lib/unicore/Index.txt)
perl_lib_file(lib/unicore/IndicPositionalCategory.txt lib/unicore/IndicPositionalCategory.txt)
perl_lib_file(lib/unicore/IndicSyllabicCategory.txt lib/unicore/IndicSyllabicCategory.txt)
perl_lib_file(lib/unicore/Jamo.txt lib/unicore/Jamo.txt)
perl_lib_file(lib/unicore/LineBreak.txt lib/unicore/LineBreak.txt)
perl_lib_file(lib/unicore/NameAliases.txt lib/unicore/NameAliases.txt)
perl_lib_file(lib/unicore/NamedSequences.txt lib/unicore/NamedSequences.txt)
perl_lib_file(lib/unicore/NamedSqProv.txt lib/unicore/NamedSqProv.txt)
perl_lib_file(lib/unicore/NamesList.txt lib/unicore/NamesList.txt)
perl_lib_file(lib/unicore/NormTest.txt lib/unicore/NormTest.txt)
perl_lib_file(lib/unicore/NormalizationCorrections.txt lib/unicore/NormalizationCorrections.txt)
perl_lib_file(lib/unicore/PropList.txt lib/unicore/PropList.txt)
perl_lib_file(lib/unicore/PropValueAliases.txt lib/unicore/PropValueAliases.txt)
perl_lib_file(lib/unicore/PropertyAliases.txt lib/unicore/PropertyAliases.txt)
perl_lib_file(lib/unicore/ReadMe.txt lib/unicore/ReadMe.txt)
perl_lib_file(lib/unicore/ScriptExtensions.txt lib/unicore/ScriptExtensions.txt)
perl_lib_file(lib/unicore/Scripts.txt lib/unicore/Scripts.txt)
perl_lib_file(lib/unicore/SpecialCasing.txt lib/unicore/SpecialCasing.txt)
perl_lib_file(lib/unicore/StandardizedVariants.txt lib/unicore/StandardizedVariants.txt)
perl_lib_file(lib/unicore/UnicodeData.txt lib/unicore/UnicodeData.txt)
perl_lib_file(lib/unicore/Unikemet.txt lib/unicore/Unikemet.txt)
perl_lib_file(lib/unicore/VerticalOrientation.txt lib/unicore/VerticalOrientation.txt)
perl_lib_file(lib/unicore/auxiliary/GCBTest.txt lib/unicore/auxiliary/GCBTest.txt)
perl_lib_file(lib/unicore/auxiliary/GraphemeBreakProperty.txt lib/unicore/auxiliary/GraphemeBreakProperty.txt)
perl_lib_file(lib/unicore/auxiliary/LBTest.txt lib/unicore/auxiliary/LBTest.txt)
perl_lib_file(lib/unicore/auxiliary/SBTest.txt lib/unicore/auxiliary/SBTest.txt)
perl_lib_file(lib/unicore/auxiliary/SentenceBreakProperty.txt lib/unicore/auxiliary/SentenceBreakProperty.txt)
perl_lib_file(lib/unicore/auxiliary/WBTest.txt lib/unicore/auxiliary/WBTest.txt)
perl_lib_file(lib/unicore/auxiliary/WordBreakProperty.txt lib/unicore/auxiliary/WordBreakProperty.txt)
perl_lib_file(lib/unicore/emoji/emoji.txt lib/unicore/emoji/emoji.txt)
perl_lib_file(lib/unicore/extracted/DBidiClass.txt lib/unicore/extracted/DBidiClass.txt)
perl_lib_file(lib/unicore/extracted/DBinaryProperties.txt lib/unicore/extracted/DBinaryProperties.txt)
perl_lib_file(lib/unicore/extracted/DCombiningClass.txt lib/unicore/extracted/DCombiningClass.txt)
perl_lib_file(lib/unicore/extracted/DDecompositionType.txt lib/unicore/extracted/DDecompositionType.txt)
perl_lib_file(lib/unicore/extracted/DEastAsianWidth.txt lib/unicore/extracted/DEastAsianWidth.txt)
perl_lib_file(lib/unicore/extracted/DGeneralCategory.txt lib/unicore/extracted/DGeneralCategory.txt)
perl_lib_file(lib/unicore/extracted/DJoinGroup.txt lib/unicore/extracted/DJoinGroup.txt)
perl_lib_file(lib/unicore/extracted/DJoinType.txt lib/unicore/extracted/DJoinType.txt)
perl_lib_file(lib/unicore/extracted/DLineBreak.txt lib/unicore/extracted/DLineBreak.txt)
perl_lib_file(lib/unicore/extracted/DNumType.txt lib/unicore/extracted/DNumType.txt)
perl_lib_file(lib/unicore/extracted/DNumValues.txt lib/unicore/extracted/DNumValues.txt)
perl_lib_file(lib/unicore/intentional.txt lib/unicore/intentional.txt)
perl_lib_file(lib/unicore/version lib/unicore/version)
perl_lib_file(lib/version.pm cpan/version/lib/version.pm)
perl_lib_file(lib/version.pod cpan/version/lib/version.pod)
perl_lib_file(lib/version/Internals.pod cpan/version/lib/version/Internals.pod)
perl_lib_file(lib/version/regex.pm cpan/version/lib/version/regex.pm)
perl_lib_file(lib/unicore/ArabicShaping.txt lib/unicore/ArabicShaping.txt)
perl_lib_file(lib/unicore/BidiBrackets.txt lib/unicore/BidiBrackets.txt)
perl_lib_file(lib/unicore/BidiMirroring.txt lib/unicore/BidiMirroring.txt)
perl_lib_file(lib/unicore/Blocks.txt lib/unicore/Blocks.txt)
perl_lib_file(lib/unicore/CJKRadicals.txt lib/unicore/CJKRadicals.txt)
perl_lib_file(lib/unicore/CaseFolding.txt lib/unicore/CaseFolding.txt)
perl_lib_file(lib/unicore/CompositionExclusions.txt lib/unicore/CompositionExclusions.txt)
perl_lib_file(lib/unicore/DAge.txt lib/unicore/DAge.txt)
perl_lib_file(lib/unicore/DCoreProperties.txt lib/unicore/DCoreProperties.txt)
perl_lib_file(lib/unicore/DNormalizationProps.txt lib/unicore/DNormalizationProps.txt)
perl_lib_file(lib/unicore/DoNotEmit.txt lib/unicore/DoNotEmit.txt)
perl_lib_file(lib/unicore/EastAsianWidth.txt lib/unicore/EastAsianWidth.txt)
perl_lib_file(lib/unicore/EmojiSources.txt lib/unicore/EmojiSources.txt)
perl_lib_file(lib/unicore/EquivalentUnifiedIdeograph.txt lib/unicore/EquivalentUnifiedIdeograph.txt)
perl_lib_file(lib/unicore/HangulSyllableType.txt lib/unicore/HangulSyllableType.txt)
perl_lib_file(lib/unicore/IdStatus.txt lib/unicore/IdStatus.txt)
perl_lib_file(lib/unicore/IdType.txt lib/unicore/IdType.txt)
perl_lib_file(lib/unicore/Index.txt lib/unicore/Index.txt)
perl_lib_file(lib/unicore/IndicPositionalCategory.txt lib/unicore/IndicPositionalCategory.txt)
perl_lib_file(lib/unicore/IndicSyllabicCategory.txt lib/unicore/IndicSyllabicCategory.txt)
perl_lib_file(lib/unicore/Jamo.txt lib/unicore/Jamo.txt)
perl_lib_file(lib/unicore/LineBreak.txt lib/unicore/LineBreak.txt)
perl_lib_file(lib/unicore/NameAliases.txt lib/unicore/NameAliases.txt)
perl_lib_file(lib/unicore/NamedSequences.txt lib/unicore/NamedSequences.txt)
perl_lib_file(lib/unicore/NamedSqProv.txt lib/unicore/NamedSqProv.txt)
perl_lib_file(lib/unicore/NamesList.txt lib/unicore/NamesList.txt)
perl_lib_file(lib/unicore/NormTest.txt lib/unicore/NormTest.txt)
perl_lib_file(lib/unicore/NormalizationCorrections.txt lib/unicore/NormalizationCorrections.txt)
perl_lib_file(lib/unicore/PropList.txt lib/unicore/PropList.txt)
perl_lib_file(lib/unicore/PropValueAliases.txt lib/unicore/PropValueAliases.txt)
perl_lib_file(lib/unicore/PropertyAliases.txt lib/unicore/PropertyAliases.txt)
perl_lib_file(lib/unicore/ReadMe.txt lib/unicore/ReadMe.txt)
perl_lib_file(lib/unicore/ScriptExtensions.txt lib/unicore/ScriptExtensions.txt)
perl_lib_file(lib/unicore/Scripts.txt lib/unicore/Scripts.txt)
perl_lib_file(lib/unicore/SpecialCasing.txt lib/unicore/SpecialCasing.txt)
perl_lib_file(lib/unicore/StandardizedVariants.txt lib/unicore/StandardizedVariants.txt)
perl_lib_file(lib/unicore/UnicodeData.txt lib/unicore/UnicodeData.txt)
perl_lib_file(lib/unicore/Unikemet.txt lib/unicore/Unikemet.txt)
perl_lib_file(lib/unicore/VerticalOrientation.txt lib/unicore/VerticalOrientation.txt)
perl_lib_file(lib/unicore/auxiliary/GCBTest.txt lib/unicore/auxiliary/GCBTest.txt)
perl_lib_file(lib/unicore/auxiliary/GraphemeBreakProperty.txt lib/unicore/auxiliary/GraphemeBreakProperty.txt)
perl_lib_file(lib/unicore/auxiliary/LBTest.txt lib/unicore/auxiliary/LBTest.txt)
perl_lib_file(lib/unicore/auxiliary/SBTest.txt lib/unicore/auxiliary/SBTest.txt)
perl_lib_file(lib/unicore/auxiliary/SentenceBreakProperty.txt lib/unicore/auxiliary/SentenceBreakProperty.txt)
perl_lib_file(lib/unicore/auxiliary/WBTest.txt lib/unicore/auxiliary/WBTest.txt)
perl_lib_file(lib/unicore/auxiliary/WordBreakProperty.txt lib/unicore/auxiliary/WordBreakProperty.txt)
perl_lib_file(lib/unicore/emoji/emoji.txt lib/unicore/emoji/emoji.txt)
perl_lib_file(lib/unicore/extracted/DBidiClass.txt lib/unicore/extracted/DBidiClass.txt)
perl_lib_file(lib/unicore/extracted/DBinaryProperties.txt lib/unicore/extracted/DBinaryProperties.txt)
perl_lib_file(lib/unicore/extracted/DCombiningClass.txt lib/unicore/extracted/DCombiningClass.txt)
perl_lib_file(lib/unicore/extracted/DDecompositionType.txt lib/unicore/extracted/DDecompositionType.txt)
perl_lib_file(lib/unicore/extracted/DEastAsianWidth.txt lib/unicore/extracted/DEastAsianWidth.txt)
perl_lib_file(lib/unicore/extracted/DGeneralCategory.txt lib/unicore/extracted/DGeneralCategory.txt)
perl_lib_file(lib/unicore/extracted/DJoinGroup.txt lib/unicore/extracted/DJoinGroup.txt)
perl_lib_file(lib/unicore/extracted/DJoinType.txt lib/unicore/extracted/DJoinType.txt)
perl_lib_file(lib/unicore/extracted/DLineBreak.txt lib/unicore/extracted/DLineBreak.txt)
perl_lib_file(lib/unicore/extracted/DNumType.txt lib/unicore/extracted/DNumType.txt)
perl_lib_file(lib/unicore/extracted/DNumValues.txt lib/unicore/extracted/DNumValues.txt)
perl_lib_file(lib/unicore/intentional.txt lib/unicore/intentional.txt)
perl_lib_file(lib/unicore/version lib/unicore/version)

add_custom_target(perl_lib_copy DEPENDS ${PERL_LIB_OUTPUTS})

add_custom_command(
    OUTPUT ${GEN}/lib/Config.pm ${GEN}/lib/Config_heavy.pl ${GEN}/lib/Config.pod
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/Porting/Glossary ${GEN}/Porting/Glossary
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/myconfig.SH ${GEN}/myconfig.SH
    COMMAND ${MINIPERL} ${REPO}/configpm
    COMMAND ${CMAKE_COMMAND} -E touch ${GEN}/lib/Config.pm ${GEN}/lib/Config_heavy.pl ${GEN}/lib/Config.pod
    WORKING_DIRECTORY ${GEN}
    DEPENDS ${HOST_MINIPERL} ${GEN}/config.sh ${GEN}/lib/Config_git.pl repo/configpm repo/Porting/Glossary repo/myconfig.SH
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/DynaLoader.pm
    COMMAND ${MINIPERL} ${REPO}/ext/DynaLoader/DynaLoader_pm.PL DynaLoader.pm
    COMMAND ${CMAKE_COMMAND} -E copy DynaLoader.pm ${GEN}/lib/DynaLoader.pm
    WORKING_DIRECTORY ${GEN}/pm/DynaLoader
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/ext/DynaLoader/DynaLoader_pm.PL
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/XSLoader.pm
    COMMAND ${MINIPERL} ${REPO}/dist/XSLoader/XSLoader_pm.PL XSLoader.pm
    COMMAND ${CMAKE_COMMAND} -E copy XSLoader.pm ${GEN}/lib/XSLoader.pm
    WORKING_DIRECTORY ${GEN}/pm/XSLoader
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/dist/XSLoader/XSLoader_pm.PL
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/lib.pm
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/dist/lib/lib_pm.PL ${GEN}/pm/lib/lib_pm.PL
    COMMAND ${MINIPERL} ${GEN}/pm/lib/lib_pm.PL lib.pm
    COMMAND ${CMAKE_COMMAND} -E copy lib.pm ${GEN}/lib/lib.pm
    WORKING_DIRECTORY ${GEN}/pm/lib
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/dist/lib/lib_pm.PL
    VERBATIM
)

add_custom_command(
    OUTPUT ${GEN}/lib/Pod/Functions.pm
    COMMAND ${MINIPERL} ${REPO}/ext/Pod-Functions/Functions_pm.PL ${REPO}/pod/perlfunc.pod
    COMMAND ${CMAKE_COMMAND} -E copy Functions.pm ${GEN}/lib/Pod/Functions.pm
    WORKING_DIRECTORY ${GEN}/pm/Pod-Functions
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/ext/Pod-Functions/Functions_pm.PL repo/pod/perlfunc.pod
    VERBATIM
)

configure_file(Errno.pm.in ${GEN}/lib/Errno.pm @ONLY)

add_custom_command(
    OUTPUT ${GEN}/lib/unicore/mktables.lst ${GEN}/pod/perluniprops.pod
    COMMAND ${MINIPERL} ${REPO}/lib/unicore/mktables -C ${GEN}/lib/unicore -P ${GEN}/pod -makelist
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/lib/unicore/mktables
    VERBATIM
)

add_custom_target(perl_lib DEPENDS
    perl_lib_copy
    ${GEN}/lib/Config.pm
    ${GEN}/lib/DynaLoader.pm
    ${GEN}/lib/XSLoader.pm
    ${GEN}/lib/lib.pm
    ${GEN}/lib/Pod/Functions.pm
    ${GEN}/lib/Errno.pm
    ${GEN}/lib/Devel/PPPort.pm
    ${GEN}/lib/unicore/mktables.lst
)

###

add_custom_command(
    OUTPUT ${GEN}/DynaLoader.c
    COMMAND ${MINIPERL} ${GEN}/lib/ExtUtils/xsubpp -noprototypes -typemap ${REPO}/lib/ExtUtils/typemap -output ${GEN}/DynaLoader.c ${REPO}/ext/DynaLoader/dl_dlopen.xs
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/ext/DynaLoader/dl_dlopen.xs repo/lib/ExtUtils/typemap
    VERBATIM
)

set(PERL_CORE_SOURCES
    repo/av.c
    repo/builtin.c
    repo/caretx.c
    repo/class.c
    repo/deb.c
    repo/doio.c
    repo/doop.c
    repo/dquote.c
    repo/dump.c
    repo/globals.c
    repo/gv.c
    repo/hv.c
    repo/keywords.c
    repo/locale.c
    repo/mathoms.c
    repo/mg.c
    repo/mro_core.c
    repo/numeric.c
    repo/op.c
    repo/pad.c
    repo/peep.c
    repo/perl.c
    repo/perlio.c
    repo/perly.c
    repo/pp.c
    repo/pp_ctl.c
    repo/pp_hot.c
    repo/pp_pack.c
    repo/pp_sort.c
    ${GEN}/pp_sys.c
    repo/reentr.c
    repo/regcomp.c
    repo/regcomp_debug.c
    repo/regcomp_invlist.c
    repo/regcomp_study.c
    repo/regcomp_trie.c
    repo/regexec.c
    repo/run.c
    repo/scope.c
    repo/sv.c
    repo/taint.c
    repo/time64.c
    repo/toke.c
    repo/universal.c
    repo/utf8.c
    repo/util.c
)

add_library(libperl SHARED
    ${PERL_CORE_SOURCES}
    ${GEN}/DynaLoader.c
)

set_source_files_properties(${PERL_CORE_SOURCES} PROPERTIES COMPILE_DEFINITIONS PERL_CORE)
set_source_files_properties(${GEN}/DynaLoader.c PROPERTIES COMPILE_DEFINITIONS "VERSION=\"1.57\";XS_VERSION=\"1.57\"")

set_target_properties(libperl PROPERTIES OUTPUT_NAME perl PREFIX lib)

target_include_directories(libperl PRIVATE
    ${GEN}
    ${REPO}
    ${REPO}/ext/DynaLoader
)

target_compile_options(libperl PRIVATE ${PERL_CFLAGS})

target_link_libraries(libperl PRIVATE moxnox)

add_dependencies(libperl perl_headers)

install(TARGETS libperl DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
    ARCHIVE DESTINATION ${PERL_ARCHLIB}/CORE
)

###

add_custom_command(
    OUTPUT ${GEN}/perlmain.c
    COMMAND ${MINIPERL} -MExtUtils::Miniperl -e [[writemain(\shift, @ARGV)]] ${GEN}/perlmain.c DynaLoader
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy
    VERBATIM
)

add_executable(perl_exe ${GEN}/perlmain.c)

set_target_properties(perl_exe PROPERTIES OUTPUT_NAME perl)

target_include_directories(perl_exe PRIVATE ${GEN} ${REPO})

target_compile_options(perl_exe PRIVATE ${PERL_CFLAGS})

target_link_libraries(perl_exe PRIVATE libperl moxnox)

add_dependencies(perl_exe perl_headers perl_lib perl_utils)

install(TARGETS perl_exe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

###

function(perl_xs_module name)
    cmake_parse_arguments(XS "NOPROTOTYPES;TYPEMAP;GENERATED_XS" "FULLEXT;DIR;XS;VERSION;CONST;CONST_SCRIPT" "SOURCES;DEFINES;INCLUDES;LIBS;DEPENDS;XS_INCLUDES;PATCH" ${ARGN})

    set(_gen ${GEN}/ext/${name})
    file(MAKE_DIRECTORY ${_gen})

    get_filename_component(_base ${XS_XS} NAME_WE)
    get_filename_component(_dll ${XS_FULLEXT} NAME)

    set(_args -typemap ${REPO}/lib/ExtUtils/typemap)
    set(_deps ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/lib/ExtUtils/typemap ${XS_DEPENDS})

    foreach(_inc ${XS_XS_INCLUDES})
        get_filename_component(_inc_name ${_inc} NAME)
        add_custom_command(
            OUTPUT ${_gen}/${_inc_name}
            COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/${XS_DIR}/${_inc} ${_gen}/${_inc_name}
            DEPENDS repo/${XS_DIR}/${_inc}
            VERBATIM
        )
        list(APPEND _deps ${_gen}/${_inc_name})
    endforeach()

    if(XS_GENERATED_XS)
        list(APPEND _deps ${_gen}/${XS_XS})
        set(_copy_xs)
    else()
        list(APPEND _deps ${REPO}/${XS_DIR}/${XS_XS})
        set(_copy_xs COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/${XS_DIR}/${XS_XS} ${_gen}/${XS_XS})

        if(XS_PATCH)
            list(APPEND _copy_xs COMMAND ${HOST_SED} -z -i ${XS_PATCH} ${_gen}/${XS_XS})
        endif()
    endif()

    if(XS_NOPROTOTYPES)
        list(PREPEND _args -noprototypes)
    endif()

    if(XS_TYPEMAP)
        list(APPEND _args -typemap ${REPO}/${XS_DIR}/typemap)
        list(APPEND _deps repo/${XS_DIR}/typemap)
    endif()

    if(XS_CONST)
        add_custom_command(
            OUTPUT ${_gen}/const-c.inc
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${XS_CONST}/const-c.inc ${_gen}/const-c.inc
            DEPENDS ${XS_CONST}/const-c.inc
            VERBATIM
        )
        add_custom_command(
            OUTPUT ${_gen}/const-xs.inc
            COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_CURRENT_SOURCE_DIR}/${XS_CONST}/const-xs.inc ${_gen}/const-xs.inc
            DEPENDS ${XS_CONST}/const-xs.inc
            VERBATIM
        )
        list(APPEND _deps ${_gen}/const-c.inc ${_gen}/const-xs.inc)
    elseif(XS_CONST_SCRIPT)
        add_custom_command(
            OUTPUT ${_gen}/const-c.inc ${_gen}/const-xs.inc
            COMMAND ${MINIPERL} ${CMAKE_CURRENT_SOURCE_DIR}/genconst.pl ${_gen} ${XS_CONST_SCRIPT}
            WORKING_DIRECTORY ${REPO}/${XS_DIR}
            DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy
                ${CMAKE_CURRENT_SOURCE_DIR}/genconst.pl repo/${XS_DIR}/${XS_CONST_SCRIPT}
            VERBATIM
        )
        list(APPEND _deps ${_gen}/const-c.inc ${_gen}/const-xs.inc)
    endif()

    add_custom_command(
        OUTPUT ${_gen}/${_base}.c
        ${_copy_xs}
        COMMAND ${MINIPERL} ${GEN}/lib/ExtUtils/xsubpp ${_args} -output ${_gen}/${_base}.c ${_gen}/${XS_XS}
        WORKING_DIRECTORY ${_gen}
        DEPENDS ${_deps}
        VERBATIM
    )

    add_library(perl_${name} SHARED ${_gen}/${_base}.c ${XS_SOURCES})

    set_target_properties(perl_${name} PROPERTIES
        OUTPUT_NAME ${_dll}
        PREFIX ""
        RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/auto/${XS_FULLEXT}
        ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/auto/${XS_FULLEXT}
    )

    target_include_directories(perl_${name} PRIVATE
        ${_gen}
        ${GEN}
        ${REPO}
        ${XS_INCLUDES}
    )

    target_compile_definitions(perl_${name} PRIVATE
        VERSION=\"${XS_VERSION}\"
        XS_VERSION=\"${XS_VERSION}\"
        ${XS_DEFINES}
    )

    target_compile_options(perl_${name} PRIVATE ${PERL_CFLAGS} -iquote ${REPO}/${XS_DIR})

    target_link_libraries(perl_${name} PRIVATE libperl ${XS_LIBS} moxnox)

    add_dependencies(perl_${name} perl_headers)

    install(TARGETS perl_${name}
        DESTINATION ${PERL_ARCHLIB}/auto/${XS_FULLEXT}
        ARCHIVE EXCLUDE_FROM_ALL
    )
endfunction()

perl_xs_module(Fcntl FULLEXT Fcntl DIR ext/Fcntl XS Fcntl.xs VERSION 1.20 NOPROTOTYPES CONST_SCRIPT Makefile.PL)

perl_xs_module(POSIX FULLEXT POSIX DIR ext/POSIX XS POSIX.xs VERSION 2.23 NOPROTOTYPES CONST_SCRIPT Makefile.PL TYPEMAP
    PATCH -e [[s|#  if !defined(WIN32) \&\& !defined(__CYGWIN__)\nextern char \*tzname\[\]|#  if !defined(WIN32) \&\& !defined(__CYGWIN__) \&\& !defined(__moxnox__)\nextern char *tzname[]|]]
)

perl_xs_module(IO FULLEXT IO DIR dist/IO XS IO.xs VERSION 1.55
    SOURCES repo/dist/IO/poll.c
)

perl_xs_module(File-Glob FULLEXT File/Glob DIR ext/File-Glob XS Glob.xs VERSION 1.42 CONST_SCRIPT Makefile.PL
    SOURCES repo/ext/File-Glob/bsd_glob.c
)

perl_xs_module(Cwd FULLEXT Cwd DIR dist/PathTools XS Cwd.xs VERSION 3.94
    DEFINES DOUBLE_SLASHES_SPECIAL=0
)

perl_xs_module(List-Util FULLEXT List/Util DIR cpan/Scalar-List-Utils XS ListUtil.xs VERSION 1.68_01
    DEFINES PERL_EXT
)

perl_xs_module(Data-Dumper FULLEXT Data/Dumper DIR dist/Data-Dumper XS Dumper.xs VERSION 2.192)

perl_xs_module(Storable FULLEXT Storable DIR dist/Storable XS Storable.xs VERSION 3.37)

perl_xs_module(Time-HiRes FULLEXT Time/HiRes DIR dist/Time-HiRes XS HiRes.xs VERSION 1.9778 CONST repo/dist/Time-HiRes/fallback TYPEMAP
    DEFINES
        TIME_HIRES_NANOSLEEP
        TIME_HIRES_CLOCKID_T
        TIME_HIRES_CLOCK_GETTIME
        TIME_HIRES_CLOCK_GETRES
        HAS_FUTIMENS
        HAS_UTIMENSAT
        TIME_HIRES_UTIME
        TIME_HIRES_STAT_XTIM
        TIME_HIRES_STAT=4
)

perl_xs_module(Socket FULLEXT Socket DIR cpan/Socket XS Socket.xs VERSION 2.038 NOPROTOTYPES CONST_SCRIPT Makefile.PL TYPEMAP)

perl_xs_module(Digest-MD5 FULLEXT Digest/MD5 DIR cpan/Digest-MD5 XS MD5.xs VERSION 2.59 TYPEMAP)

perl_xs_module(Digest-SHA FULLEXT Digest/SHA DIR cpan/Digest-SHA XS SHA.xs VERSION 6.04 TYPEMAP)

perl_xs_module(MIME-Base64 FULLEXT MIME/Base64 DIR cpan/MIME-Base64 XS Base64.xs VERSION 3.16_01)

perl_xs_module(re FULLEXT re DIR ext/re XS re.xs VERSION 0.48 NOPROTOTYPES
    SOURCES
        repo/regcomp.c
        repo/regcomp_invlist.c
        repo/regcomp_study.c
        repo/regcomp_trie.c
        repo/regcomp_debug.c
        repo/regexec.c
    DEFINES PERL_EXT_RE_BUILD PERL_EXT_RE_DEBUG PERL_EXT
)

perl_xs_module(mro FULLEXT mro DIR ext/mro XS mro.xs VERSION 1.29)

perl_xs_module(attributes FULLEXT attributes DIR ext/attributes XS attributes.xs VERSION 0.36)

perl_xs_module(Hash-Util FULLEXT Hash/Util DIR ext/Hash-Util XS Util.xs VERSION 0.32
    DEFINES PERL_EXT
)

perl_xs_module(Sys-Hostname FULLEXT Sys/Hostname DIR ext/Sys-Hostname XS Hostname.xs VERSION 1.25)

###

function(perl_encode_table dir table)
    set(_gen ${GEN}/ext/${dir})
    set(_fnm "")
    set(_deps)

    foreach(_ucm ${ARGN})
        string(APPEND _fnm "${REPO}/cpan/Encode/ucm/${_ucm}\n")
        list(APPEND _deps repo/cpan/Encode/ucm/${_ucm})
    endforeach()

    file(CONFIGURE OUTPUT ${_gen}/${table}.fnm CONTENT "${_fnm}" @ONLY)

    add_custom_command(
        OUTPUT ${_gen}/${table}.c ${_gen}/${table}.h ${_gen}/${table}.exh
        COMMAND ${MINIPERL} ${REPO}/cpan/Encode/bin/enc2xs -Q -O -o ${_gen}/${table}.c -f ${_gen}/${table}.fnm
        DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/cpan/Encode/bin/enc2xs ${_gen}/${table}.fnm ${_deps}
        VERBATIM
    )
endfunction()

function(perl_encode_xs name)
    set(ENCODE_MODULE ${name})
    set(ENCODE_HEADERS "")
    set(ENCODE_BOOT "")

    foreach(_table ${ARGN})
        string(APPEND ENCODE_HEADERS "#include \"${_table}.h\"\n")
        string(APPEND ENCODE_BOOT "#include \"${_table}.exh\"\n")
    endforeach()

    configure_file(Encode.xs.in ${GEN}/ext/Encode-${name}/${name}.xs @ONLY)
endfunction()

perl_encode_table(Encode def_t ascii.ucm 8859-1.ucm cp1252.ucm null.ucm ctrl.ucm)

perl_xs_module(Encode FULLEXT Encode DIR cpan/Encode XS Encode.xs VERSION 3.21
    SOURCES repo/cpan/Encode/encengine.c ${GEN}/ext/Encode/def_t.c
    INCLUDES ${REPO}/cpan/Encode/Encode
    DEPENDS ${GEN}/ext/Encode/def_t.c
)

perl_encode_table(Encode-Byte byte_t
    nextstep.ucm hp-roman8.ucm viscii.ucm adobeStdenc.ucm koi8-f.ucm koi8-r.ucm koi8-u.ucm
    macArabic.ucm macCentEuro.ucm macCroatian.ucm macCyrillic.ucm macFarsi.ucm macGreek.ucm
    macHebrew.ucm macIceland.ucm macRoman.ucm macROMnn.ucm macRUMnn.ucm macSami.ucm macThai.ucm
    macTurkish.ucm macUkraine.ucm 8859-10.ucm 8859-11.ucm 8859-13.ucm 8859-14.ucm 8859-15.ucm
    8859-16.ucm 8859-2.ucm 8859-3.ucm 8859-4.ucm 8859-5.ucm 8859-6.ucm 8859-7.ucm 8859-8.ucm
    8859-9.ucm cp1006.ucm cp1250.ucm cp1251.ucm cp1253.ucm cp1254.ucm cp1255.ucm cp1256.ucm
    cp1257.ucm cp1258.ucm cp424.ucm cp437.ucm cp737.ucm cp775.ucm cp850.ucm cp852.ucm cp855.ucm
    cp856.ucm cp857.ucm cp858.ucm cp860.ucm cp861.ucm cp862.ucm cp863.ucm cp864.ucm cp865.ucm
    cp866.ucm cp869.ucm cp874.ucm
)
perl_encode_xs(Byte byte_t)
perl_xs_module(Encode-Byte FULLEXT Encode/Byte DIR cpan/Encode/Byte XS Byte.xs VERSION 2.04 GENERATED_XS
    SOURCES ${GEN}/ext/Encode-Byte/byte_t.c
    INCLUDES ${REPO}/cpan/Encode/Encode
    DEPENDS ${GEN}/ext/Encode-Byte/byte_t.c
)

perl_encode_table(Encode-CN cp_00_t cp936.ucm)
perl_encode_table(Encode-CN eu_01_t euc-cn.ucm)
perl_encode_table(Encode-CN gb_02_t gb12345.ucm)
perl_encode_table(Encode-CN gb_03_t gb2312.ucm)
perl_encode_table(Encode-CN ir_04_t ir-165.ucm)
perl_encode_table(Encode-CN ma_05_t macChinsimp.ucm)
perl_encode_xs(CN cp_00_t eu_01_t gb_02_t gb_03_t ir_04_t ma_05_t)
perl_xs_module(Encode-CN FULLEXT Encode/CN DIR cpan/Encode/CN XS CN.xs VERSION 2.03 GENERATED_XS NOPROTOTYPES
    SOURCES
        ${GEN}/ext/Encode-CN/cp_00_t.c
        ${GEN}/ext/Encode-CN/eu_01_t.c
        ${GEN}/ext/Encode-CN/gb_02_t.c
        ${GEN}/ext/Encode-CN/gb_03_t.c
        ${GEN}/ext/Encode-CN/ir_04_t.c
        ${GEN}/ext/Encode-CN/ma_05_t.c
    INCLUDES ${REPO}/cpan/Encode/Encode
    DEPENDS
        ${GEN}/ext/Encode-CN/cp_00_t.c
        ${GEN}/ext/Encode-CN/eu_01_t.c
        ${GEN}/ext/Encode-CN/gb_02_t.c
        ${GEN}/ext/Encode-CN/gb_03_t.c
        ${GEN}/ext/Encode-CN/ir_04_t.c
        ${GEN}/ext/Encode-CN/ma_05_t.c
)

perl_encode_table(Encode-EBCDIC ebcdic_t posix-bc.ucm cp037.ucm cp1026.ucm cp1047.ucm cp500.ucm cp875.ucm)
perl_encode_xs(EBCDIC ebcdic_t)
perl_xs_module(Encode-EBCDIC FULLEXT Encode/EBCDIC DIR cpan/Encode/EBCDIC XS EBCDIC.xs VERSION 2.02 GENERATED_XS
    SOURCES ${GEN}/ext/Encode-EBCDIC/ebcdic_t.c
    INCLUDES ${REPO}/cpan/Encode/Encode
    DEPENDS ${GEN}/ext/Encode-EBCDIC/ebcdic_t.c
)

perl_encode_table(Encode-JP cp_00_t cp932.ucm)
perl_encode_table(Encode-JP eu_01_t euc-jp.ucm)
perl_encode_table(Encode-JP ji_02_t jis0201.ucm)
perl_encode_table(Encode-JP ji_03_t jis0208.ucm)
perl_encode_table(Encode-JP ji_04_t jis0212.ucm)
perl_encode_table(Encode-JP ma_05_t macJapanese.ucm)
perl_encode_table(Encode-JP sh_06_t shiftjis.ucm)
perl_encode_xs(JP cp_00_t eu_01_t ji_02_t ji_03_t ji_04_t ma_05_t sh_06_t)
perl_xs_module(Encode-JP FULLEXT Encode/JP DIR cpan/Encode/JP XS JP.xs VERSION 2.05 GENERATED_XS
    SOURCES
        ${GEN}/ext/Encode-JP/cp_00_t.c
        ${GEN}/ext/Encode-JP/eu_01_t.c
        ${GEN}/ext/Encode-JP/ji_02_t.c
        ${GEN}/ext/Encode-JP/ji_03_t.c
        ${GEN}/ext/Encode-JP/ji_04_t.c
        ${GEN}/ext/Encode-JP/ma_05_t.c
        ${GEN}/ext/Encode-JP/sh_06_t.c
    INCLUDES ${REPO}/cpan/Encode/Encode
    DEPENDS
        ${GEN}/ext/Encode-JP/cp_00_t.c
        ${GEN}/ext/Encode-JP/eu_01_t.c
        ${GEN}/ext/Encode-JP/ji_02_t.c
        ${GEN}/ext/Encode-JP/ji_03_t.c
        ${GEN}/ext/Encode-JP/ji_04_t.c
        ${GEN}/ext/Encode-JP/ma_05_t.c
        ${GEN}/ext/Encode-JP/sh_06_t.c
)

perl_encode_table(Encode-KR cp_00_t cp949.ucm)
perl_encode_table(Encode-KR eu_01_t euc-kr.ucm)
perl_encode_table(Encode-KR jo_02_t johab.ucm)
perl_encode_table(Encode-KR ks_03_t ksc5601.ucm)
perl_encode_table(Encode-KR ma_04_t macKorean.ucm)
perl_encode_xs(KR cp_00_t eu_01_t jo_02_t ks_03_t ma_04_t)
perl_xs_module(Encode-KR FULLEXT Encode/KR DIR cpan/Encode/KR XS KR.xs VERSION 2.03 GENERATED_XS
    SOURCES
        ${GEN}/ext/Encode-KR/cp_00_t.c
        ${GEN}/ext/Encode-KR/eu_01_t.c
        ${GEN}/ext/Encode-KR/jo_02_t.c
        ${GEN}/ext/Encode-KR/ks_03_t.c
        ${GEN}/ext/Encode-KR/ma_04_t.c
    INCLUDES ${REPO}/cpan/Encode/Encode
    DEPENDS
        ${GEN}/ext/Encode-KR/cp_00_t.c
        ${GEN}/ext/Encode-KR/eu_01_t.c
        ${GEN}/ext/Encode-KR/jo_02_t.c
        ${GEN}/ext/Encode-KR/ks_03_t.c
        ${GEN}/ext/Encode-KR/ma_04_t.c
)

perl_encode_table(Encode-Symbol symbol_t symbol.ucm dingbats.ucm adobeSymbol.ucm adobeZdingbat.ucm macSymbol.ucm macDingbats.ucm)
perl_encode_xs(Symbol symbol_t)
perl_xs_module(Encode-Symbol FULLEXT Encode/Symbol DIR cpan/Encode/Symbol XS Symbol.xs VERSION 2.02 GENERATED_XS
    SOURCES ${GEN}/ext/Encode-Symbol/symbol_t.c
    INCLUDES ${REPO}/cpan/Encode/Encode
    DEPENDS ${GEN}/ext/Encode-Symbol/symbol_t.c
)

perl_encode_table(Encode-TW bi_00_t big5-eten.ucm)
perl_encode_table(Encode-TW bi_01_t big5-hkscs.ucm)
perl_encode_table(Encode-TW cp_02_t cp950.ucm)
perl_encode_table(Encode-TW ma_03_t macChintrad.ucm)
perl_encode_xs(TW bi_00_t bi_01_t cp_02_t ma_03_t)
perl_xs_module(Encode-TW FULLEXT Encode/TW DIR cpan/Encode/TW XS TW.xs VERSION 2.03 GENERATED_XS
    SOURCES
        ${GEN}/ext/Encode-TW/bi_00_t.c
        ${GEN}/ext/Encode-TW/bi_01_t.c
        ${GEN}/ext/Encode-TW/cp_02_t.c
        ${GEN}/ext/Encode-TW/ma_03_t.c
    INCLUDES ${REPO}/cpan/Encode/Encode
    DEPENDS
        ${GEN}/ext/Encode-TW/bi_00_t.c
        ${GEN}/ext/Encode-TW/bi_01_t.c
        ${GEN}/ext/Encode-TW/cp_02_t.c
        ${GEN}/ext/Encode-TW/ma_03_t.c
)

perl_xs_module(Encode-Unicode FULLEXT Encode/Unicode DIR cpan/Encode/Unicode XS Unicode.xs VERSION 2.20
    INCLUDES ${REPO}/cpan/Encode/Encode
)

###

perl_xs_module(Compress-Raw-Zlib FULLEXT Compress/Raw/Zlib DIR cpan/Compress-Raw-Zlib XS Zlib.xs VERSION 2.213 TYPEMAP
    DEFINES NO_VIZ GZIP_OS_CODE=3 Perl_crz_BUILD_ZLIB=0 register=
    LIBS zlib
    XS_INCLUDES fallback/constants.xs fallback/constants.h
)

perl_xs_module(Compress-Raw-Bzip2 FULLEXT Compress/Raw/Bzip2 DIR cpan/Compress-Raw-Bzip2 XS Bzip2.xs VERSION 2.213 TYPEMAP
    DEFINES BZ_NO_STDIO
    LIBS bzip2
    XS_INCLUDES fallback/constants.xs fallback/constants.h
)

###

add_custom_command(
    OUTPUT
        ${GEN}/pm/Unicode-Normalize/unfcan.h
        ${GEN}/pm/Unicode-Normalize/unfcmb.h
        ${GEN}/pm/Unicode-Normalize/unfcmp.h
        ${GEN}/pm/Unicode-Normalize/unfcpt.h
        ${GEN}/pm/Unicode-Normalize/unfexc.h
    COMMAND ${MINIPERL} ${REPO}/dist/Unicode-Normalize/mkheader
    WORKING_DIRECTORY ${GEN}/pm/Unicode-Normalize
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy ${GEN}/lib/unicore/mktables.lst repo/dist/Unicode-Normalize/mkheader
    VERBATIM
)

perl_xs_module(Unicode-Normalize FULLEXT Unicode/Normalize DIR dist/Unicode-Normalize XS Normalize.xs VERSION 1.32
    INCLUDES ${GEN}/pm/Unicode-Normalize
    DEPENDS
        ${GEN}/pm/Unicode-Normalize/unfcan.h
        ${GEN}/pm/Unicode-Normalize/unfcmb.h
        ${GEN}/pm/Unicode-Normalize/unfcmp.h
        ${GEN}/pm/Unicode-Normalize/unfcpt.h
        ${GEN}/pm/Unicode-Normalize/unfexc.h
)

add_custom_command(
    OUTPUT ${GEN}/pm/Unicode-Collate/ucatbl.h
    COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/cpan/Unicode-Collate/Collate/allkeys.txt ${GEN}/pm/Unicode-Collate/Collate/allkeys.txt
    COMMAND ${MINIPERL} ${REPO}/cpan/Unicode-Collate/mkheader
    WORKING_DIRECTORY ${GEN}/pm/Unicode-Collate
    DEPENDS ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/cpan/Unicode-Collate/mkheader repo/cpan/Unicode-Collate/Collate/allkeys.txt
    VERBATIM
)

perl_xs_module(Unicode-Collate FULLEXT Unicode/Collate DIR cpan/Unicode-Collate XS Collate.xs VERSION 1.31
    INCLUDES ${GEN}/pm/Unicode-Collate
    DEPENDS ${GEN}/pm/Unicode-Collate/ucatbl.h
)

perl_xs_module(Math-BigInt-FastCalc FULLEXT Math/BigInt/FastCalc DIR cpan/Math-BigInt-FastCalc XS FastCalc.xs VERSION 0.5020)

perl_xs_module(Opcode FULLEXT Opcode DIR ext/Opcode XS Opcode.xs VERSION 1.69)

perl_xs_module(B FULLEXT B DIR ext/B XS B.xs VERSION 1.89 CONST_SCRIPT Makefile.PL TYPEMAP)

perl_xs_module(Time-Piece FULLEXT Time/Piece DIR cpan/Time-Piece XS Piece.xs VERSION 1.36)

perl_xs_module(PerlIO-encoding FULLEXT PerlIO/encoding DIR ext/PerlIO-encoding XS encoding.xs VERSION 0.31)

perl_xs_module(PerlIO-via FULLEXT PerlIO/via DIR ext/PerlIO-via XS via.xs VERSION 0.19)

perl_xs_module(PerlIO-mmap FULLEXT PerlIO/mmap DIR ext/PerlIO-mmap XS mmap.xs VERSION 0.017)

perl_xs_module(Filter-Util-Call FULLEXT Filter/Util/Call DIR cpan/Filter-Util-Call XS Call.xs VERSION 1.64)

perl_xs_module(Hash-Util-FieldHash FULLEXT Hash/Util/FieldHash DIR ext/Hash-Util-FieldHash XS FieldHash.xs VERSION 1.27)

perl_xs_module(I18N-Langinfo FULLEXT I18N/Langinfo DIR ext/I18N-Langinfo XS Langinfo.xs VERSION 0.24 CONST_SCRIPT Makefile.PL)

perl_xs_module(SDBM_File FULLEXT SDBM_File DIR ext/SDBM_File XS SDBM_File.xs VERSION 1.17 TYPEMAP
    SOURCES
        repo/ext/SDBM_File/hash.c
        repo/ext/SDBM_File/pair.c
        repo/ext/SDBM_File/sdbm.c
    DEFINES SDBM DUFF
)

perl_xs_module(File-DosGlob FULLEXT File/DosGlob DIR ext/File-DosGlob XS DosGlob.xs VERSION 1.12)

perl_xs_module(Devel-Peek FULLEXT Devel/Peek DIR ext/Devel-Peek XS Peek.xs VERSION 1.36)

perl_xs_module(Devel-PPPort FULLEXT Devel/PPPort DIR dist/Devel-PPPort XS RealPPPort.xs VERSION 3.73 GENERATED_XS NOPROTOTYPES TYPEMAP
    SOURCES
        repo/dist/Devel-PPPort/module2.c
        repo/dist/Devel-PPPort/module3.c
)

perl_xs_module(Sys-Syslog FULLEXT Sys/Syslog DIR cpan/Sys-Syslog XS Syslog.xs VERSION 0.36 NOPROTOTYPES CONST repo/cpan/Sys-Syslog/fallback)

perl_xs_module(IPC-SysV FULLEXT IPC/SysV DIR cpan/IPC-SysV XS SysV.xs VERSION 2.09 CONST_SCRIPT regen.pl TYPEMAP
    DEFINES NO_PPPORT_H
)

###

macro(perl_mirror_file path)
    add_custom_command(
        OUTPUT ${GEN}/${path}
        COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/${path} ${GEN}/${path}
        DEPENDS repo/${path}
        VERBATIM
    )
endmacro()

macro(perl_util name pl)
    set(_deps ${HOST_MINIPERL} ${GEN}/lib/Config.pm perl_lib_copy repo/${pl})

    foreach(_f ${ARGN})
        perl_mirror_file(${_f})
        list(APPEND _deps ${GEN}/${_f})
    endforeach()

    add_custom_command(
        OUTPUT ${GEN}/utils/${name}
        COMMAND ${CMAKE_COMMAND} -E copy ${REPO}/${pl} ${GEN}/utils/${name}.PL
        COMMAND ${MINIPERL} ${GEN}/utils/${name}.PL
        WORKING_DIRECTORY ${GEN}/utils
        DEPENDS ${_deps}
        VERBATIM
    )

    list(APPEND PERL_UTILS ${GEN}/utils/${name})
endmacro()

perl_util(cpan utils/cpan.PL cpan/CPAN/scripts/cpan)
perl_util(corelist utils/corelist.PL dist/Module-CoreList/corelist)
perl_util(enc2xs utils/enc2xs.PL cpan/Encode/bin/enc2xs)
perl_util(encguess utils/encguess.PL cpan/Encode/bin/encguess)
perl_util(h2ph utils/h2ph.PL)
perl_util(h2xs utils/h2xs.PL)
perl_util(instmodsh utils/instmodsh.PL cpan/ExtUtils-MakeMaker/bin/instmodsh)
perl_util(json_pp utils/json_pp.PL cpan/JSON-PP/bin/json_pp)
perl_util(libnetcfg utils/libnetcfg.PL)
perl_util(perlbug utils/perlbug.PL patchlevel.h)
perl_util(perldoc utils/perldoc.PL)
perl_util(perlivp utils/perlivp.PL)
perl_util(piconv utils/piconv.PL cpan/Encode/bin/piconv)
perl_util(pl2pm utils/pl2pm.PL)
perl_util(pod2html utils/pod2html.PL ext/Pod-Html/bin/pod2html)
perl_util(prove utils/prove.PL cpan/Test-Harness/bin/prove)
perl_util(ptar utils/ptar.PL cpan/Archive-Tar/bin/ptar)
perl_util(ptardiff utils/ptardiff.PL cpan/Archive-Tar/bin/ptardiff)
perl_util(ptargrep utils/ptargrep.PL cpan/Archive-Tar/bin/ptargrep)
perl_util(shasum utils/shasum.PL cpan/Digest-SHA/shasum)
perl_util(splain utils/splain.PL lib/diagnostics.pm)
perl_util(streamzip utils/streamzip.PL cpan/IO-Compress/bin/streamzip)
perl_util(xsubpp utils/xsubpp.PL dist/ExtUtils-ParseXS/lib/ExtUtils/xsubpp)
perl_util(zipdetails utils/zipdetails.PL cpan/IO-Compress/bin/zipdetails)
perl_util(pod2man cpan/podlators/scripts/pod2man.PL)
perl_util(pod2text cpan/podlators/scripts/pod2text.PL)
perl_util(pod2usage cpan/Pod-Usage/scripts/pod2usage.PL)
perl_util(podchecker cpan/Pod-Checker/scripts/podchecker.PL)

add_custom_target(perl_utils DEPENDS ${PERL_UTILS})

install(PROGRAMS ${PERL_UTILS} DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

install(PROGRAMS ${GEN}/utils/perlbug DESTINATION ${CMAKE_INSTALL_PREFIX}/bin RENAME perlthanks)

###

install(DIRECTORY repo/lib/
    DESTINATION ${PERL_PRIVLIB}
    PATTERN "*/t" EXCLUDE
    PATTERN "*.t" EXCLUDE
    PATTERN "*/unicore" EXCLUDE
)

install(DIRECTORY ${GEN}/lib/
    DESTINATION ${PERL_PRIVLIB}
    REGEX "/unicore/.*\\.txt$" EXCLUDE
    PATTERN "mktables.lst" EXCLUDE
)

install(DIRECTORY repo/pod/
    DESTINATION ${PERL_PRIVLIB}/pod
    FILES_MATCHING PATTERN "*.pod"
)

install(FILES ${GEN}/pod/perluniprops.pod DESTINATION ${PERL_PRIVLIB}/pod)

install(FILES
    ${GEN}/bitcount.h
    ${GEN}/config.h
    ${GEN}/git_version.h
    ${GEN}/mg_data.h
    ${GEN}/unixish.h
    ${GEN}/uudmap.h
    repo/EXTERN.h
    repo/INTERN.h
    repo/XSUB.h
    repo/av.h
    repo/cop.h
    repo/cv.h
    repo/dosish.h
    repo/ebcdic_tables.h
    repo/embed.h
    repo/embedvar.h
    repo/fakesdio.h
    repo/feature.h
    repo/form.h
    repo/gv.h
    repo/handy.h
    repo/hv.h
    repo/hv_func.h
    repo/hv_macro.h
    repo/inline.h
    repo/intrpvar.h
    repo/invlist_inline.h
    repo/iperlsys.h
    repo/keywords.h
    repo/l1_char_class_tab.h
    repo/locale_table.h
    repo/malloc_ctl.h
    repo/metaconfig.h
    repo/mg.h
    repo/mg_raw.h
    repo/mg_vtable.h
    repo/mydtrace.h
    repo/nostdio.h
    repo/op.h
    repo/op_reg_common.h
    repo/opcode.h
    repo/opnames.h
    repo/overload.h
    repo/pad.h
    repo/parser.h
    repo/patchlevel.h
    repo/perl.h
    repo/perl_inc_macro.h
    repo/perl_langinfo.h
    repo/perl_siphash.h
    repo/perlapi.h
    repo/perlio.h
    repo/perliol.h
    repo/perlsdio.h
    repo/perlstatic.h
    repo/perlvars.h
    repo/perly.h
    repo/pp.h
    repo/pp_proto.h
    repo/proto.h
    repo/reentr.h
    repo/regcharclass.h
    repo/regcomp.h
    repo/regcomp_internal.h
    repo/regexp.h
    repo/regexp_constants.h
    repo/reginline.h
    repo/regnodes.h
    repo/sbox32_hash.h
    repo/scope.h
    repo/scope_types.h
    repo/sv.h
    repo/sv_inline.h
    repo/thread.h
    repo/time64.h
    repo/time64_config.h
    repo/uni_keywords.h
    repo/unicode_constants.h
    repo/utf8.h
    repo/utfebcdic.h
    repo/util.h
    repo/vutil.h
    repo/warnings.h
    repo/zaphod32_hash.h
    DESTINATION ${PERL_ARCHLIB}/CORE
)

###

add_custom_target(perl)
add_dependencies(perl
    libperl
    perl_exe
    perl_lib
    perl_utils
    perl_Fcntl
    perl_POSIX
    perl_IO
    perl_File-Glob
    perl_Cwd
    perl_List-Util
    perl_Data-Dumper
    perl_Storable
    perl_Time-HiRes
    perl_Socket
    perl_Digest-MD5
    perl_Digest-SHA
    perl_MIME-Base64
    perl_re
    perl_mro
    perl_attributes
    perl_Hash-Util
    perl_Sys-Hostname
    perl_Encode
    perl_Encode-Byte
    perl_Encode-CN
    perl_Encode-EBCDIC
    perl_Encode-JP
    perl_Encode-KR
    perl_Encode-Symbol
    perl_Encode-TW
    perl_Encode-Unicode
    perl_Compress-Raw-Zlib
    perl_Compress-Raw-Bzip2
    perl_Unicode-Normalize
    perl_Unicode-Collate
    perl_Math-BigInt-FastCalc
    perl_Opcode
    perl_B
    perl_Time-Piece
    perl_PerlIO-encoding
    perl_PerlIO-via
    perl_PerlIO-mmap
    perl_Filter-Util-Call
    perl_Hash-Util-FieldHash
    perl_I18N-Langinfo
    perl_SDBM_File
    perl_File-DosGlob
    perl_Devel-Peek
    perl_Devel-PPPort
    perl_Sys-Syslog
    perl_IPC-SysV
)
