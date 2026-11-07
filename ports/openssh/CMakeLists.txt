set(GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")

set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/repo")

add_custom_command(
    OUTPUT ${GEN}/auth-passwd.c ${GEN}/auth.c ${GEN}/auth2.c ${GEN}/authfile.c ${GEN}/channels.c ${GEN}/clientloop.c ${GEN}/misc-agent.c ${GEN}/misc.c ${GEN}/monitor_fdpass.c ${GEN}/monitor_wrap.c ${GEN}/mux.c ${GEN}/openbsd-compat/bsd-openpty.c ${GEN}/openbsd-compat/getrrsetbyname.c ${GEN}/openbsd-compat/readpassphrase.c ${GEN}/openbsd-compat/setproctitle.c ${GEN}/openbsd-compat/xcrypt.c ${GEN}/platform.c ${GEN}/progressmeter.c ${GEN}/session.c ${GEN}/sftp.c ${GEN}/sshd-auth.c ${GEN}/sshd-session.c ${GEN}/sshd.c ${GEN}/ssherr-libcrypto.c ${GEN}/sshpty.c ${GEN}/uidswap.c
    COMMAND sh -c "mkdir -p '${GEN}/openbsd-compat' && cp '${REPO}/auth-passwd.c' '${REPO}/auth.c' '${REPO}/auth2.c' '${REPO}/authfile.c' '${REPO}/channels.c' '${REPO}/clientloop.c' '${REPO}/misc-agent.c' '${REPO}/misc.c' '${REPO}/monitor_fdpass.c' '${REPO}/monitor_wrap.c' '${REPO}/mux.c' '${REPO}/platform.c' '${REPO}/progressmeter.c' '${REPO}/session.c' '${REPO}/sftp.c' '${REPO}/sshd-auth.c' '${REPO}/sshd-session.c' '${REPO}/sshd.c' '${REPO}/ssherr-libcrypto.c' '${REPO}/sshpty.c' '${REPO}/uidswap.c' '${GEN}/' && cp '${REPO}/openbsd-compat/bsd-openpty.c' '${REPO}/openbsd-compat/getrrsetbyname.c' '${REPO}/openbsd-compat/readpassphrase.c' '${REPO}/openbsd-compat/setproctitle.c' '${REPO}/openbsd-compat/xcrypt.c' '${GEN}/openbsd-compat/' && patch -p1 -s -d '${GEN}' < '${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch'"
    DEPENDS
        repo/auth-passwd.c
        repo/auth2.c
        repo/auth.c
        repo/authfile.c
        repo/channels.c
        repo/clientloop.c
        repo/misc-agent.c
        repo/misc.c
        repo/monitor_fdpass.c
        repo/monitor_wrap.c
        repo/mux.c
        repo/openbsd-compat/bsd-openpty.c
        repo/openbsd-compat/getrrsetbyname.c
        repo/openbsd-compat/readpassphrase.c
        repo/openbsd-compat/setproctitle.c
        repo/openbsd-compat/xcrypt.c
        repo/platform.c
        repo/progressmeter.c
        repo/session.c
        repo/sftp.c
        repo/sshd-auth.c
        repo/sshd-session.c
        repo/sshd.c
        repo/ssherr-libcrypto.c
        repo/sshpty.c
        repo/uidswap.c
        ${CMAKE_CURRENT_SOURCE_DIR}/moxnox.patch
    VERBATIM
)

add_library(openssh_core STATIC
    repo/addr.c
    repo/addrmatch.c
    repo/atomicio.c
    repo/authfd.c
    ${GEN}/authfile.c
    repo/bitmap.c
    repo/canohost.c
    repo/chacha.c
    ${GEN}/channels.c
    repo/cipher-aes.c
    repo/cipher-aesctr.c
    repo/cipher-chachapoly-libcrypto.c
    repo/cipher-chachapoly.c
    repo/cipher.c
    repo/cleanup.c
    repo/compat.c
    repo/dh.c
    repo/digest-libc.c
    repo/digest-openssl.c
    repo/dispatch.c
    repo/dns.c
    repo/ed25519-openssl.c
    repo/ed25519.c
    repo/entropy.c
    repo/fatal.c
    repo/gss-genr.c
    repo/hmac.c
    repo/hostfile.c
    repo/kex-names.c
    repo/kex.c
    repo/kexc25519.c
    repo/kexdh.c
    repo/kexecdh.c
    repo/kexgen.c
    repo/kexgex.c
    repo/kexgexc.c
    repo/kexgexs.c
    repo/kexmlkem768x25519.c
    repo/kexsntrup761x25519.c
    repo/krl.c
    repo/log.c
    repo/mac.c
    repo/match.c
    ${GEN}/misc-agent.c
    ${GEN}/misc.c
    repo/moduli.c
    ${GEN}/monitor_fdpass.c
    repo/msg.c
    repo/nchan.c
    repo/openbsd-compat/arc4random.c
    repo/openbsd-compat/arc4random_uniform.c
    repo/openbsd-compat/base64.c
    repo/openbsd-compat/bcrypt_pbkdf.c
    repo/openbsd-compat/blowfish.c
    repo/openbsd-compat/bsd-closefrom.c
    repo/openbsd-compat/bsd-getentropy.c
    repo/openbsd-compat/bsd-getpeereid.c
    repo/openbsd-compat/bsd-misc.c
    ${GEN}/openbsd-compat/bsd-openpty.c
    repo/openbsd-compat/bsd-timegm.c
    repo/openbsd-compat/daemon.c
    repo/openbsd-compat/explicit_bzero.c
    repo/openbsd-compat/fmt_scaled.c
    repo/openbsd-compat/freezero.c
    repo/openbsd-compat/getgrouplist.c
    repo/openbsd-compat/getopt_long.c
    ${GEN}/openbsd-compat/getrrsetbyname.c
    repo/openbsd-compat/glob.c
    repo/openbsd-compat/libressl-api-compat.c
    repo/openbsd-compat/mktemp.c
    repo/openbsd-compat/openssl-compat.c
    repo/openbsd-compat/pwcache.c
    ${GEN}/openbsd-compat/readpassphrase.c
    repo/openbsd-compat/recallocarray.c
    ${GEN}/openbsd-compat/setproctitle.c
    repo/openbsd-compat/strcasestr.c
    repo/openbsd-compat/strlcat.c
    repo/openbsd-compat/strmode.c
    repo/openbsd-compat/strptime.c
    repo/openbsd-compat/strsep.c
    repo/openbsd-compat/strtonum.c
    repo/openbsd-compat/timingsafe_bcmp.c
    repo/openbsd-compat/vis.c
    ${GEN}/openbsd-compat/xcrypt.c
    repo/packet.c
    repo/platform-misc.c
    repo/platform-pledge.c
    repo/platform-tracing.c
    repo/poly1305.c
    repo/readpass.c
    repo/rijndael.c
    repo/sftp-realpath.c
    repo/smult_curve25519_ref.c
    repo/sntrup761.c
    repo/ssh-ecdsa-sk.c
    repo/ssh-ecdsa.c
    repo/ssh-ed25519-sk.c
    repo/ssh-ed25519.c
    repo/ssh-rsa.c
    repo/ssh_api.c
    repo/sshbuf-getput-basic.c
    repo/sshbuf-getput-crypto.c
    repo/sshbuf-io.c
    repo/sshbuf-misc.c
    repo/sshbuf.c
    ${GEN}/ssherr-libcrypto.c
    repo/ssherr.c
    repo/sshkey.c
    repo/ttymodes.c
    repo/umac.c
    repo/umac128.c
    repo/utf8.c
    repo/xmalloc.c
)

target_include_directories(openssh_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/compat-include
    repo
)

set_source_files_properties(
    ${GEN}/openbsd-compat/bsd-openpty.c
    ${GEN}/openbsd-compat/getrrsetbyname.c
    ${GEN}/openbsd-compat/readpassphrase.c
    ${GEN}/openbsd-compat/setproctitle.c
    ${GEN}/openbsd-compat/xcrypt.c
    PROPERTIES COMPILE_OPTIONS "-iquote${REPO}/openbsd-compat"
)

target_compile_definitions(openssh_core PUBLIC
    HAVE_CONFIG_H
    OPENSSL_API_COMPAT=0x10100000L
    SSHDIR=\"/etc/ssh\"
    _PATH_SSH_PROGRAM=\"/bin/ssh.exe\"
    _PATH_SSH_ASKPASS_DEFAULT=\"/libexec/ssh-askpass.exe\"
    _PATH_SFTP_SERVER=\"/libexec/sftp-server.exe\"
    _PATH_SSH_KEY_SIGN=\"/libexec/ssh-keysign.exe\"
    _PATH_SSHD_SESSION=\"/libexec/sshd-session.exe\"
    _PATH_SSHD_AUTH=\"/libexec/sshd-auth.exe\"
    _PATH_SSH_PKCS11_HELPER=\"/libexec/ssh-pkcs11-helper.exe\"
    _PATH_SSH_SK_HELPER=\"/libexec/ssh-sk-helper.exe\"
    _PATH_SSH_PIDDIR=\"/var/run\"
    _PATH_PRIVSEP_CHROOT_DIR=\"/var/empty\"
)

target_compile_options(openssh_core PUBLIC -fno-strict-aliasing -Wno-pointer-sign)

target_link_libraries(openssh_core PUBLIC libssl libcrypto zlib)

add_executable(ssh
    repo/ssh.c
    repo/readconf.c
    ${GEN}/clientloop.c
    repo/sshtty.c
    repo/sshconnect.c
    repo/sshconnect2.c
    ${GEN}/mux.c
    repo/ssh-pkcs11.c
    repo/ssh-sk-client.c
)
target_link_libraries(ssh openssh_core)

add_executable(sshd
    ${GEN}/sshd.c
    repo/platform-listen.c
    repo/servconf.c
    ${GEN}/sshpty.c
    repo/srclimit.c
    repo/groupaccess.c
    repo/auth2-methods.c
    repo/dns.c
    repo/fatal.c
    repo/compat.c
    repo/utf8.c
    repo/authfd.c
    repo/canohost.c
    repo/ssh-pkcs11-client.c
    repo/ssh-sk-client.c
)
target_link_libraries(sshd openssh_core)

add_executable(sshd-session
    ${GEN}/sshd-session.c
    repo/auth-rhosts.c
    ${GEN}/auth-passwd.c
    repo/audit.c
    repo/audit-bsm.c
    repo/audit-linux.c
    ${GEN}/platform.c
    ${GEN}/sshpty.c
    repo/sshlogin.c
    repo/servconf.c
    repo/serverloop.c
    ${GEN}/auth.c
    ${GEN}/auth2.c
    repo/auth2-methods.c
    repo/auth-options.c
    ${GEN}/session.c
    repo/auth2-chall.c
    repo/groupaccess.c
    repo/auth-bsdauth.c
    repo/auth2-hostbased.c
    repo/auth2-kbdint.c
    repo/auth2-none.c
    repo/auth2-passwd.c
    repo/auth2-pubkey.c
    repo/auth2-pubkeyfile.c
    repo/monitor.c
    ${GEN}/monitor_wrap.c
    repo/auth-krb5.c
    repo/auth2-gss.c
    repo/gss-serv.c
    repo/gss-serv-krb5.c
    repo/loginrec.c
    repo/auth-pam.c
    repo/auth-shadow.c
    repo/auth-sia.c
    repo/sftp-server.c
    repo/sftp-common.c
    ${GEN}/uidswap.c
    repo/platform-listen.c
    repo/ssh-pkcs11-client.c
    repo/ssh-sk-client.c
)
target_link_libraries(sshd-session openssh_core)

add_executable(sshd-auth
    ${GEN}/sshd-auth.c
    repo/auth2-methods.c
    repo/auth-rhosts.c
    ${GEN}/auth-passwd.c
    ${GEN}/sshpty.c
    repo/sshlogin.c
    repo/servconf.c
    repo/serverloop.c
    ${GEN}/auth.c
    ${GEN}/auth2.c
    repo/auth-options.c
    ${GEN}/session.c
    repo/auth2-chall.c
    repo/groupaccess.c
    repo/auth-bsdauth.c
    repo/auth2-hostbased.c
    repo/auth2-kbdint.c
    repo/auth2-none.c
    repo/auth2-passwd.c
    repo/auth2-pubkey.c
    repo/auth2-pubkeyfile.c
    repo/auth2-gss.c
    repo/gss-serv.c
    repo/gss-serv-krb5.c
    ${GEN}/monitor_wrap.c
    repo/auth-krb5.c
    repo/audit.c
    repo/audit-bsm.c
    repo/audit-linux.c
    ${GEN}/platform.c
    repo/loginrec.c
    repo/auth-pam.c
    repo/auth-shadow.c
    repo/auth-sia.c
    repo/sandbox-null.c
    repo/sandbox-rlimit.c
    repo/sandbox-darwin.c
    repo/sandbox-seccomp-filter.c
    repo/sandbox-capsicum.c
    repo/sandbox-solaris.c
    repo/sftp-server.c
    repo/sftp-common.c
    ${GEN}/uidswap.c
    repo/ssh-pkcs11-client.c
    repo/ssh-sk-client.c
)
target_link_libraries(sshd-auth openssh_core)

add_executable(scp
    repo/scp.c
    ${GEN}/progressmeter.c
    repo/sftp-common.c
    repo/sftp-client.c
    repo/sftp-glob.c
    repo/ssherr-nolibcrypto.c
)
target_link_libraries(scp openssh_core)

add_executable(ssh-add
    repo/ssh-add.c
    repo/ssh-pkcs11-client.c
    repo/ssh-sk-client.c
)
target_link_libraries(ssh-add openssh_core)

add_executable(ssh-agent
    repo/ssh-agent.c
    repo/ssh-pkcs11-client.c
    repo/ssh-sk-client.c
)
target_link_libraries(ssh-agent openssh_core)

add_executable(ssh-keygen
    repo/ssh-keygen.c
    repo/sshsig.c
    repo/ssh-pkcs11.c
    repo/ssh-sk-client.c
)
target_link_libraries(ssh-keygen openssh_core)

add_executable(ssh-keysign
    repo/ssh-keysign.c
    repo/readconf.c
    ${GEN}/uidswap.c
    repo/ssh-pkcs11-client.c
    repo/ssh-sk-client.c
)
target_link_libraries(ssh-keysign openssh_core)

add_executable(ssh-pkcs11-helper
    repo/ssh-pkcs11-helper.c
    repo/ssh-pkcs11.c
    repo/ssh-sk-client.c
)
target_link_libraries(ssh-pkcs11-helper openssh_core)

add_executable(ssh-sk-helper
    repo/ssh-sk-helper.c
    repo/ssh-sk.c
    repo/sk-usbhid.c
    repo/ssherr-nolibcrypto.c
)
target_link_libraries(ssh-sk-helper openssh_core)

add_executable(ssh-keyscan
    repo/ssh-keyscan.c
    repo/ssh-pkcs11-client.c
    repo/ssh-sk-client.c
)
target_link_libraries(ssh-keyscan openssh_core)

add_executable(sftp-server
    repo/sftp-common.c
    repo/sftp-server.c
    repo/sftp-server-main.c
    repo/ssherr-nolibcrypto.c
)
target_link_libraries(sftp-server openssh_core)

add_executable(sftp
    ${GEN}/sftp.c
    repo/sftp-usergroup.c
    ${GEN}/progressmeter.c
    repo/sftp-common.c
    repo/sftp-client.c
    repo/sftp-glob.c
    repo/ssherr-nolibcrypto.c
)
target_link_libraries(sftp openssh_core)

install(TARGETS ssh scp sftp ssh-add ssh-agent ssh-keygen ssh-keyscan sshd
    DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)
install(TARGETS sftp-server sshd-session sshd-auth ssh-keysign ssh-pkcs11-helper ssh-sk-helper
    DESTINATION ${CMAKE_INSTALL_PREFIX}/libexec)
install(FILES repo/ssh_config repo/sshd_config repo/moduli
    DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/ssh)

add_custom_target(openssh)
add_dependencies(openssh
    ssh
    sshd
    sshd-session
    sshd-auth
    scp
    ssh-add
    ssh-agent
    ssh-keygen
    ssh-keysign
    ssh-pkcs11-helper
    ssh-sk-helper
    ssh-keyscan
    sftp-server
    sftp
)
