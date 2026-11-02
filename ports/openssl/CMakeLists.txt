set(GEN "${CMAKE_CURRENT_BINARY_DIR}/gen")

file(MAKE_DIRECTORY "${GEN}/providers/implementations/rands/seeding")
file(MAKE_DIRECTORY "${GEN}/include/internal")
file(MAKE_DIRECTORY "${GEN}/include/openssl")
file(MAKE_DIRECTORY "${GEN}/crypto/rand")
file(MAKE_DIRECTORY "${GEN}/crypto/bio")
file(MAKE_DIRECTORY "${GEN}/crypto")
file(MAKE_DIRECTORY "${GEN}/ssl")
file(MAKE_DIRECTORY "${GEN}/apps/lib")

add_custom_command(
    OUTPUT ${GEN}/configdata.pm
    COMMAND perl ${CMAKE_CURRENT_SOURCE_DIR}/repo/Configure mingw64
            --cross-compile-prefix=x86_64-w64-mingw32-
            no-asm no-async no-engine no-module no-dso
            no-thread-pool no-default-thread-pool
            no-tests no-docs no-quic no-ui-console
            no-winstore no-secure-memory no-deprecated
            --prefix=/usr --openssldir=/etc/ssl --api=1.1.1
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/repo/Configure
    WORKING_DIRECTORY ${GEN}
    VERBATIM
)

set(OPENSSL_GENERATED_FILES
    ${GEN}/apps/progs.c
    ${GEN}/apps/progs.h
    ${GEN}/crypto/buildinf.h
    ${GEN}/crypto/params_idx.c
    ${GEN}/include/crypto/bn_conf.h
    ${GEN}/include/crypto/dso_conf.h
    ${GEN}/include/internal/param_names.h
    ${GEN}/include/openssl/asn1.h
    ${GEN}/include/openssl/asn1t.h
    ${GEN}/include/openssl/bio.h
    ${GEN}/include/openssl/cmp.h
    ${GEN}/include/openssl/cms.h
    ${GEN}/include/openssl/comp.h
    ${GEN}/include/openssl/conf.h
    ${GEN}/include/openssl/configuration.h
    ${GEN}/include/openssl/core_names.h
    ${GEN}/include/openssl/crmf.h
    ${GEN}/include/openssl/crypto.h
    ${GEN}/include/openssl/ct.h
    ${GEN}/include/openssl/err.h
    ${GEN}/include/openssl/ess.h
    ${GEN}/include/openssl/fipskey.h
    ${GEN}/include/openssl/lhash.h
    ${GEN}/include/openssl/ocsp.h
    ${GEN}/include/openssl/opensslv.h
    ${GEN}/include/openssl/pkcs12.h
    ${GEN}/include/openssl/pkcs7.h
    ${GEN}/include/openssl/safestack.h
    ${GEN}/include/openssl/srp.h
    ${GEN}/include/openssl/ssl.h
    ${GEN}/include/openssl/ui.h
    ${GEN}/include/openssl/x509.h
    ${GEN}/include/openssl/x509_acert.h
    ${GEN}/include/openssl/x509_vfy.h
    ${GEN}/include/openssl/x509v3.h
    ${GEN}/libcrypto.def
    ${GEN}/libssl.def
    ${GEN}/providers/common/der/der_digests_gen.c
    ${GEN}/providers/common/der/der_dsa_gen.c
    ${GEN}/providers/common/der/der_ec_gen.c
    ${GEN}/providers/common/der/der_ecx_gen.c
    ${GEN}/providers/common/der/der_ml_dsa_gen.c
    ${GEN}/providers/common/der/der_rsa_gen.c
    ${GEN}/providers/common/der/der_slh_dsa_gen.c
    ${GEN}/providers/common/der/der_sm2_gen.c
    ${GEN}/providers/common/der/der_wrap_gen.c
    ${GEN}/providers/common/include/prov/der_digests.h
    ${GEN}/providers/common/include/prov/der_dsa.h
    ${GEN}/providers/common/include/prov/der_ec.h
    ${GEN}/providers/common/include/prov/der_ecx.h
    ${GEN}/providers/common/include/prov/der_ml_dsa.h
    ${GEN}/providers/common/include/prov/der_rsa.h
    ${GEN}/providers/common/include/prov/der_slh_dsa.h
    ${GEN}/providers/common/include/prov/der_sm2.h
    ${GEN}/providers/common/include/prov/der_wrap.h
)

add_custom_command(
    OUTPUT ${OPENSSL_GENERATED_FILES}
    COMMAND ${HOST_MAKE} -C ${GEN} build_all_generated libcrypto.def libssl.def
    COMMAND ${HOST_SED} -i [[s|^LIBRARY .*|LIBRARY         "libcrypto"|]] ${GEN}/libcrypto.def
    COMMAND ${HOST_SED} -i [[s|^LIBRARY .*|LIBRARY         "libssl"|]] ${GEN}/libssl.def
    DEPENDS ${GEN}/configdata.pm
    VERBATIM
)

add_custom_target(openssl_generate DEPENDS ${OPENSSL_GENERATED_FILES})

function(openssl_patch relpath)
    add_custom_command(
        OUTPUT ${GEN}/${relpath}
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_CURRENT_SOURCE_DIR}/repo/${relpath} ${GEN}/${relpath}
        COMMAND ${HOST_SED} -z -i ${ARGN} ${GEN}/${relpath}
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/repo/${relpath}
        VERBATIM
    )
endfunction()

# avoid io.h
openssl_patch(include/internal/e_os.h
    -e [[s|#include <io.h>\n#include <fcntl.h>\n\n#ifdef OPENSSL_SYS_WINCE|#ifdef MOXNOX\n#include <unistd.h>\n#else\n#include <io.h>\n#endif\n#include <fcntl.h>\n\n#ifdef OPENSSL_SYS_WINCE|g]]
)

# don't bring in winsock
openssl_patch(include/internal/e_winsock.h
    -e [[s|#include <winsock2.h>\n#include <ws2tcpip.h>|#ifdef MOXNOX\n#define _WINSOCKAPI_\n#include <sys/socket.h>\n#include <netinet/in.h>\n#include <netinet/tcp.h>\n#include <arpa/inet.h>\n#include <netdb.h>\n#include <poll.h>\n#include <unistd.h>\n#else\n#include <winsock2.h>\n#include <ws2tcpip.h>|g]]
    -e [[s|#include <wspiapi.h>\n#endif|#include <wspiapi.h>\n#endif\n#endif|g]]
)

openssl_patch(include/internal/sockets.h
    -e [[s|#if defined(OPENSSL_SYS_WINDOWS)\n#undef get_last_socket_error|#if defined(OPENSSL_SYS_WINDOWS)\n#ifndef MOXNOX\n#undef get_last_socket_error|g]]
    -e [[s|#define get_last_socket_error_is_eintr() (get_last_socket_error() == WSAEINTR)\n#define readsocket|#define get_last_socket_error_is_eintr() (get_last_socket_error() == WSAEINTR)\n#else\n#undef gai_strerror\nconst char *gai_strerror(int)\;\n#undef IN6_IS_ADDR_V4MAPPED\n#define IN6_IS_ADDR_V4MAPPED(a) ((a)->s6_addr[0] == 0 \&\& (a)->s6_addr[1] == 0 \&\& (a)->s6_addr[2] == 0 \&\& (a)->s6_addr[3] == 0 \&\& (a)->s6_addr[4] == 0 \&\& (a)->s6_addr[5] == 0 \&\& (a)->s6_addr[6] == 0 \&\& (a)->s6_addr[7] == 0 \&\& (a)->s6_addr[8] == 0 \&\& (a)->s6_addr[9] == 0 \&\& (a)->s6_addr[10] == 0xff \&\& (a)->s6_addr[11] == 0xff)\n#define closesocket(s) close(s)\n#ifndef INVALID_SOCKET\n#define INVALID_SOCKET (-1)\n#endif\n#endif\n#define readsocket|g]]
    -e [[s|#ifndef VMS\n#include <sys/ioctl.h>\n|#if defined(MOXNOX)\n#elif !defined(VMS)\n#include <sys/ioctl.h>\n|g]]
)

openssl_patch(include/openssl/e_os2.h
    -e [[s|#define OSSL_SSIZE_MAX _I64_MAX|#ifdef MOXNOX\n#define OSSL_SSIZE_MAX ((ossl_ssize_t)0x7fffffffffffffff)\n#else\n#define OSSL_SSIZE_MAX _I64_MAX\n#endif|g]]
)

# don't bring in winsock
openssl_patch(include/openssl/e_ostime.h
    -e [[s|#if defined(OPENSSL_SYS_WINDOWS)\n#if !defined(_WINSOCKAPI_)|#if defined(OPENSSL_SYS_WINDOWS) \&\& !defined(MOXNOX)\n#if !defined(_WINSOCKAPI_)|g]]
)

# bio_local.h: copied unpatched so that the patched bio sources beside it find it
add_custom_command(
    OUTPUT ${GEN}/crypto/bio/bio_local.h
    COMMAND ${CMAKE_COMMAND} -E copy
            ${CMAKE_CURRENT_SOURCE_DIR}/repo/crypto/bio/bio_local.h ${GEN}/crypto/bio/bio_local.h
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/repo/crypto/bio/bio_local.h
    VERBATIM
)

openssl_patch(crypto/bio/bss_file.c
    -e [[s|#if defined(OPENSSL_SYS_WINDOWS)\n            /\*\n             \* On Windows|#if defined(OPENSSL_SYS_WINDOWS) \&\& !defined(MOXNOX)\n            /*\n             * On Windows|g]]
    -e [[s|#if defined(OPENSSL_SYS_WINDOWS)\n            int fd = _fileno((FILE \*)ptr)\;|#if defined(OPENSSL_SYS_WINDOWS) \&\& !defined(MOXNOX)\n            int fd = _fileno((FILE *)ptr)\;|g]]
    -e [[s|#elif defined(OPENSSL_SYS_MSDOS)\n            int fd = fileno((FILE \*)ptr)\;|#elif defined(OPENSSL_SYS_MSDOS) \&\& !defined(MOXNOX)\n            int fd = fileno((FILE *)ptr)\;|g]]
)

openssl_patch(crypto/bio/bio_sock.c
    -e [[s|#ifdef OPENSSL_SYS_WINDOWS\n    static struct WSAData wsa_state\;|#if defined(OPENSSL_SYS_WINDOWS) \&\& !defined(MOXNOX)\n    static struct WSAData wsa_state\;|g]]
    -e [[s|#ifdef OPENSSL_SYS_WINDOWS\n    if (wsa_init_done) {|#if defined(OPENSSL_SYS_WINDOWS) \&\& !defined(MOXNOX)\n    if (wsa_init_done) {|g]]
    -e [[s@#if defined(OPENSSL_SYS_WINDOWS) || !defined(POLLIN)@#if (defined(OPENSSL_SYS_WINDOWS) \&\& !defined(MOXNOX)) || !defined(POLLIN)@g]]
    -e [[s|#ifdef __DJGPP__\n    i = ioctlsocket(fd, type, (char \*)arg)\;\n#else|#if defined(MOXNOX)\n    (void)fd\;\n    (void)type\;\n    (void)arg\;\n    errno = ENOSYS\;\n    i = -1\;\n#elif defined(__DJGPP__)\n    i = ioctlsocket(fd, type, (char *)arg)\;\n#else|g]]
)

# fix Clang's broken dead-code elimination still trying to bring in gethostbyname and getservbyname
openssl_patch(crypto/bio/bio_addr.c
    -e [[s|#endif\n        const struct hostent \*he\;|#endif\n#if 0\n        const struct hostent \*he\;|g]]
    -e [[s|    err:\n        CRYPTO_THREAD_unlock(bio_lookup_lock)\;\n|    err:\n        CRYPTO_THREAD_unlock(bio_lookup_lock)\;\n#endif\n|g]]
)


openssl_patch(crypto/time.c
    -e [[s|#include "internal/e_os.h"|#include "internal/e_os.h"\n\n#ifdef MOXNOX\n#include <sys/time.h>\n#endif|g]]
)

openssl_patch(crypto/o_dir.c
    -e [[s@#if defined OPENSSL_SYS_UNIX || defined DJGPP@#if defined OPENSSL_SYS_UNIX || defined MOXNOX || defined DJGPP@g]]
)

openssl_patch(providers/implementations/rands/seeding/rand_win.c
    -e [[s|#ifdef USE_BCRYPTGENRANDOM\n#include <bcrypt.h>|#if defined(MOXNOX)\n#include <unistd.h>\n#include <time.h>\n#elif defined(USE_BCRYPTGENRANDOM)\n#include <bcrypt.h>|g]]
    -e [[s|#ifndef USE_BCRYPTGENRANDOM\n    HCRYPTPROV hProvider\;\n#endif|#if !defined(USE_BCRYPTGENRANDOM) \&\& !defined(MOXNOX)\n    HCRYPTPROV hProvider\;\n#endif|g]]
    -e [[s|#ifdef USE_BCRYPTGENRANDOM\n    bytes_needed|#if defined(MOXNOX)\n    bytes_needed = ossl_rand_pool_bytes_needed(pool, 1 /*entropy_factor*/)\;\n    buffer = ossl_rand_pool_add_begin(pool, bytes_needed)\;\n    if (buffer != NULL) {\n        size_t bytes = 0\;\n\n        while (bytes < bytes_needed) {\n            size_t chunk = bytes_needed - bytes\;\n\n            if (chunk > 256)\n                chunk = 256\;\n\n            if (getentropy(buffer + bytes, chunk) != 0)\n                break\;\n\n            bytes += chunk\;\n        }\n\n        ossl_rand_pool_add_end(pool, bytes, 8 * bytes)\;\n        entropy_available = ossl_rand_pool_entropy_available(pool)\;\n    }\n    if (entropy_available > 0)\n        return entropy_available\;\n#elif defined(USE_BCRYPTGENRANDOM)\n    bytes_needed|g]]
    -e [[s|    data.pid = GetCurrentProcessId()\;|#ifdef MOXNOX\n    data.pid = (DWORD)getpid()\;\n#else\n    data.pid = GetCurrentProcessId()\;\n#endif|g]]
    -e [[s|    GetSystemTimeAsFileTime(&data.time)\;|#ifdef MOXNOX\n    {\n        struct timespec ts\;\n        clock_gettime(CLOCK_MONOTONIC, \&ts)\;\n        data.time.dwLowDateTime = (DWORD)ts.tv_nsec\;\n        data.time.dwHighDateTime = (DWORD)ts.tv_sec\;\n    }\n#else\n    GetSystemTimeAsFileTime(\&data.time)\;\n#endif|g]]
)

openssl_patch(providers/implementations/rands/seeding/rand_unix.c
    -e [[s|static ssize_t syscall_random(void \*buf, size_t buflen)\n{|static ssize_t syscall_random(void *buf, size_t buflen)\n{\n#ifdef MOXNOX\n    {\n        size_t bytes = 0\;\n\n        while (bytes < buflen) {\n            size_t chunk = buflen - bytes\;\n\n            if (chunk > 256)\n                chunk = 256\;\n\n            if (getentropy((unsigned char *)buf + bytes, chunk) != 0)\n                return -1\;\n\n            bytes += chunk\;\n        }\n\n        return (ssize_t)buflen\;\n    }\n#endif|g]]
)

# avoid WideCharToMultiByte
openssl_patch(crypto/rand/randfile.c
    -e [[s|#define fileno _fileno|//#define fileno _fileno|g]]
    -e [[s|#include <io.h>|#define NO_OLDNAMES\n#include <io.h>\n#undef NO_OLDNAMES|g]]
)

openssl_patch(crypto/threads_pthread.c
    -e [[s|#if defined(OPENSSL_THREADS) && !defined(CRYPTO_TDEBUG) && !defined(OPENSSL_SYS_WINDOWS)|#if defined(OPENSSL_THREADS) \&\& !defined(CRYPTO_TDEBUG) \&\& (!defined(OPENSSL_SYS_WINDOWS) \|\| defined(MOXNOX))|g]]
    -e [[s|#if defined(OPENSSL_SYS_UNIX)|#if defined(OPENSSL_SYS_UNIX) \|\| defined(MOXNOX)|g]]
)
openssl_patch(crypto/threads_win.c
    -e [[s|#if defined(OPENSSL_THREADS) && !defined(CRYPTO_TDEBUG) && defined(OPENSSL_SYS_WINDOWS)|#if defined(OPENSSL_THREADS) \&\& !defined(CRYPTO_TDEBUG) \&\& defined(OPENSSL_SYS_WINDOWS) \&\& !defined(MOXNOX)|g]]
)

openssl_patch(crypto/thread/arch/thread_win.c
    -e [[s|#if defined(OPENSSL_THREADS_WINNT)|#if defined(OPENSSL_THREADS_WINNT) \&\& !defined(MOXNOX)|g]]
)
openssl_patch(crypto/thread/arch/thread_posix.c
    -e [[s|#if defined(OPENSSL_THREADS_POSIX)|#if defined(OPENSSL_THREADS_POSIX) \|\| defined(MOXNOX)|g]]
)

add_library(libcrypto SHARED
    repo/crypto/aes/aes_cbc.c
    repo/crypto/aes/aes_cfb.c
    repo/crypto/aes/aes_core.c
    repo/crypto/aes/aes_ecb.c
    repo/crypto/aes/aes_ige.c
    repo/crypto/aes/aes_misc.c
    repo/crypto/aes/aes_ofb.c
    repo/crypto/aes/aes_wrap.c
    repo/crypto/aria/aria.c
    repo/crypto/asn1/a_bitstr.c
    repo/crypto/asn1/a_d2i_fp.c
    repo/crypto/asn1/a_digest.c
    repo/crypto/asn1/a_dup.c
    repo/crypto/asn1/a_gentm.c
    repo/crypto/asn1/a_i2d_fp.c
    repo/crypto/asn1/a_int.c
    repo/crypto/asn1/a_mbstr.c
    repo/crypto/asn1/a_object.c
    repo/crypto/asn1/a_octet.c
    repo/crypto/asn1/a_print.c
    repo/crypto/asn1/a_sign.c
    repo/crypto/asn1/a_strex.c
    repo/crypto/asn1/a_strnid.c
    repo/crypto/asn1/a_time.c
    repo/crypto/asn1/a_type.c
    repo/crypto/asn1/a_utctm.c
    repo/crypto/asn1/a_utf8.c
    repo/crypto/asn1/a_verify.c
    repo/crypto/asn1/ameth_lib.c
    repo/crypto/asn1/asn1_err.c
    repo/crypto/asn1/asn1_gen.c
    repo/crypto/asn1/asn1_item_list.c
    repo/crypto/asn1/asn1_lib.c
    repo/crypto/asn1/asn1_parse.c
    repo/crypto/asn1/asn_mime.c
    repo/crypto/asn1/asn_moid.c
    repo/crypto/asn1/asn_mstbl.c
    repo/crypto/asn1/asn_pack.c
    repo/crypto/asn1/bio_asn1.c
    repo/crypto/asn1/bio_ndef.c
    repo/crypto/asn1/d2i_param.c
    repo/crypto/asn1/d2i_pr.c
    repo/crypto/asn1/d2i_pu.c
    repo/crypto/asn1/evp_asn1.c
    repo/crypto/asn1/f_int.c
    repo/crypto/asn1/f_string.c
    repo/crypto/asn1/i2d_evp.c
    repo/crypto/asn1/n_pkey.c
    repo/crypto/asn1/nsseq.c
    repo/crypto/asn1/p5_pbe.c
    repo/crypto/asn1/p5_pbev2.c
    repo/crypto/asn1/p5_scrypt.c
    repo/crypto/asn1/p8_pkey.c
    repo/crypto/asn1/t_bitst.c
    repo/crypto/asn1/t_pkey.c
    repo/crypto/asn1/t_spki.c
    repo/crypto/asn1/tasn_dec.c
    repo/crypto/asn1/tasn_enc.c
    repo/crypto/asn1/tasn_fre.c
    repo/crypto/asn1/tasn_new.c
    repo/crypto/asn1/tasn_prn.c
    repo/crypto/asn1/tasn_scn.c
    repo/crypto/asn1/tasn_typ.c
    repo/crypto/asn1/tasn_utl.c
    repo/crypto/asn1/x_algor.c
    repo/crypto/asn1/x_bignum.c
    repo/crypto/asn1/x_info.c
    repo/crypto/asn1/x_int64.c
    repo/crypto/asn1/x_long.c
    repo/crypto/asn1/x_pkey.c
    repo/crypto/asn1/x_sig.c
    repo/crypto/asn1/x_spki.c
    repo/crypto/asn1/x_val.c
    repo/crypto/asn1_dsa.c
    repo/crypto/async/arch/async_null.c
    repo/crypto/async/arch/async_posix.c
    repo/crypto/async/arch/async_win.c
    repo/crypto/async/async.c
    repo/crypto/async/async_err.c
    repo/crypto/async/async_wait.c
    repo/crypto/bf/bf_cfb64.c
    repo/crypto/bf/bf_ecb.c
    repo/crypto/bf/bf_enc.c
    repo/crypto/bf/bf_ofb64.c
    repo/crypto/bf/bf_skey.c
    repo/crypto/bio/bf_buff.c
    repo/crypto/bio/bf_lbuf.c
    repo/crypto/bio/bf_nbio.c
    repo/crypto/bio/bf_null.c
    repo/crypto/bio/bf_prefix.c
    repo/crypto/bio/bf_readbuff.c
    ${GEN}/crypto/bio/bio_addr.c
    repo/crypto/bio/bio_cb.c
    repo/crypto/bio/bio_dump.c
    repo/crypto/bio/bio_err.c
    repo/crypto/bio/bio_lib.c
    repo/crypto/bio/bio_meth.c
    repo/crypto/bio/bio_print.c
    ${GEN}/crypto/bio/bio_sock.c
    repo/crypto/bio/bio_sock2.c
    repo/crypto/bio/bss_acpt.c
    repo/crypto/bio/bss_bio.c
    repo/crypto/bio/bss_conn.c
    repo/crypto/bio/bss_core.c
    repo/crypto/bio/bss_dgram.c
    repo/crypto/bio/bss_dgram_pair.c
    repo/crypto/bio/bss_fd.c
    ${GEN}/crypto/bio/bss_file.c
    repo/crypto/bio/bss_log.c
    repo/crypto/bio/bss_mem.c
    repo/crypto/bio/bss_null.c
    repo/crypto/bio/bss_sock.c
    repo/crypto/bio/ossl_core_bio.c
    repo/crypto/bn/bn_add.c
    repo/crypto/bn/bn_asm.c
    repo/crypto/bn/bn_blind.c
    repo/crypto/bn/bn_const.c
    repo/crypto/bn/bn_conv.c
    repo/crypto/bn/bn_ctx.c
    repo/crypto/bn/bn_dh.c
    repo/crypto/bn/bn_div.c
    repo/crypto/bn/bn_err.c
    repo/crypto/bn/bn_exp.c
    repo/crypto/bn/bn_exp2.c
    repo/crypto/bn/bn_gcd.c
    repo/crypto/bn/bn_gf2m.c
    repo/crypto/bn/bn_intern.c
    repo/crypto/bn/bn_kron.c
    repo/crypto/bn/bn_lib.c
    repo/crypto/bn/bn_mod.c
    repo/crypto/bn/bn_mont.c
    repo/crypto/bn/bn_mpi.c
    repo/crypto/bn/bn_mul.c
    repo/crypto/bn/bn_nist.c
    repo/crypto/bn/bn_prime.c
    repo/crypto/bn/bn_print.c
    repo/crypto/bn/bn_rand.c
    repo/crypto/bn/bn_recp.c
    repo/crypto/bn/bn_rsa_fips186_4.c
    repo/crypto/bn/bn_shift.c
    repo/crypto/bn/bn_sqr.c
    repo/crypto/bn/bn_sqrt.c
    repo/crypto/bn/bn_srp.c
    repo/crypto/bn/bn_word.c
    repo/crypto/bn/bn_x931p.c
    repo/crypto/bsearch.c
    repo/crypto/buffer/buf_err.c
    repo/crypto/buffer/buffer.c
    repo/crypto/camellia/camellia.c
    repo/crypto/camellia/cmll_cbc.c
    repo/crypto/camellia/cmll_cfb.c
    repo/crypto/camellia/cmll_ctr.c
    repo/crypto/camellia/cmll_ecb.c
    repo/crypto/camellia/cmll_misc.c
    repo/crypto/camellia/cmll_ofb.c
    repo/crypto/cast/c_cfb64.c
    repo/crypto/cast/c_ecb.c
    repo/crypto/cast/c_enc.c
    repo/crypto/cast/c_ofb64.c
    repo/crypto/cast/c_skey.c
    repo/crypto/chacha/chacha_enc.c
    repo/crypto/cmac/cmac.c
    repo/crypto/cmp/cmp_asn.c
    repo/crypto/cmp/cmp_client.c
    repo/crypto/cmp/cmp_ctx.c
    repo/crypto/cmp/cmp_err.c
    repo/crypto/cmp/cmp_genm.c
    repo/crypto/cmp/cmp_hdr.c
    repo/crypto/cmp/cmp_http.c
    repo/crypto/cmp/cmp_msg.c
    repo/crypto/cmp/cmp_protect.c
    repo/crypto/cmp/cmp_server.c
    repo/crypto/cmp/cmp_status.c
    repo/crypto/cmp/cmp_util.c
    repo/crypto/cmp/cmp_vfy.c
    repo/crypto/cms/cms_asn1.c
    repo/crypto/cms/cms_att.c
    repo/crypto/cms/cms_cd.c
    repo/crypto/cms/cms_dd.c
    repo/crypto/cms/cms_dh.c
    repo/crypto/cms/cms_ec.c
    repo/crypto/cms/cms_enc.c
    repo/crypto/cms/cms_env.c
    repo/crypto/cms/cms_err.c
    repo/crypto/cms/cms_ess.c
    repo/crypto/cms/cms_io.c
    repo/crypto/cms/cms_kari.c
    repo/crypto/cms/cms_lib.c
    repo/crypto/cms/cms_pwri.c
    repo/crypto/cms/cms_rsa.c
    repo/crypto/cms/cms_sd.c
    repo/crypto/cms/cms_smime.c
    repo/crypto/comp/c_brotli.c
    repo/crypto/comp/c_zlib.c
    repo/crypto/comp/c_zstd.c
    repo/crypto/comp/comp_err.c
    repo/crypto/comp/comp_lib.c
    repo/crypto/comp_methods.c
    repo/crypto/conf/conf_api.c
    repo/crypto/conf/conf_def.c
    repo/crypto/conf/conf_err.c
    repo/crypto/conf/conf_lib.c
    repo/crypto/conf/conf_mall.c
    repo/crypto/conf/conf_mod.c
    repo/crypto/conf/conf_sap.c
    repo/crypto/conf/conf_ssl.c
    repo/crypto/context.c
    repo/crypto/core_algorithm.c
    repo/crypto/core_fetch.c
    repo/crypto/core_namemap.c
    repo/crypto/cpt_err.c
    repo/crypto/cpuid.c
    repo/crypto/crmf/crmf_asn.c
    repo/crypto/crmf/crmf_err.c
    repo/crypto/crmf/crmf_lib.c
    repo/crypto/crmf/crmf_pbm.c
    repo/crypto/cryptlib.c
    repo/crypto/ct/ct_b64.c
    repo/crypto/ct/ct_err.c
    repo/crypto/ct/ct_log.c
    repo/crypto/ct/ct_oct.c
    repo/crypto/ct/ct_policy.c
    repo/crypto/ct/ct_prn.c
    repo/crypto/ct/ct_sct.c
    repo/crypto/ct/ct_sct_ctx.c
    repo/crypto/ct/ct_vfy.c
    repo/crypto/ct/ct_x509v3.c
    repo/crypto/ctype.c
    repo/crypto/cversion.c
    repo/crypto/defaults.c
    repo/crypto/der_writer.c
    repo/crypto/des/cbc_cksm.c
    repo/crypto/des/cbc_enc.c
    repo/crypto/des/cfb64ede.c
    repo/crypto/des/cfb64enc.c
    repo/crypto/des/cfb_enc.c
    repo/crypto/des/des_enc.c
    repo/crypto/des/ecb3_enc.c
    repo/crypto/des/ecb_enc.c
    repo/crypto/des/fcrypt.c
    repo/crypto/des/fcrypt_b.c
    repo/crypto/des/ofb64ede.c
    repo/crypto/des/ofb64enc.c
    repo/crypto/des/ofb_enc.c
    repo/crypto/des/pcbc_enc.c
    repo/crypto/des/qud_cksm.c
    repo/crypto/des/rand_key.c
    repo/crypto/des/set_key.c
    repo/crypto/des/str2key.c
    repo/crypto/des/xcbc_enc.c
    repo/crypto/deterministic_nonce.c
    repo/crypto/dh/dh_ameth.c
    repo/crypto/dh/dh_asn1.c
    repo/crypto/dh/dh_backend.c
    repo/crypto/dh/dh_check.c
    repo/crypto/dh/dh_err.c
    repo/crypto/dh/dh_gen.c
    repo/crypto/dh/dh_group_params.c
    repo/crypto/dh/dh_kdf.c
    repo/crypto/dh/dh_key.c
    repo/crypto/dh/dh_lib.c
    repo/crypto/dh/dh_meth.c
    repo/crypto/dh/dh_pmeth.c
    repo/crypto/dh/dh_prn.c
    repo/crypto/dh/dh_rfc5114.c
    repo/crypto/dsa/dsa_ameth.c
    repo/crypto/dsa/dsa_asn1.c
    repo/crypto/dsa/dsa_backend.c
    repo/crypto/dsa/dsa_check.c
    repo/crypto/dsa/dsa_err.c
    repo/crypto/dsa/dsa_gen.c
    repo/crypto/dsa/dsa_key.c
    repo/crypto/dsa/dsa_lib.c
    repo/crypto/dsa/dsa_meth.c
    repo/crypto/dsa/dsa_ossl.c
    repo/crypto/dsa/dsa_pmeth.c
    repo/crypto/dsa/dsa_prn.c
    repo/crypto/dsa/dsa_sign.c
    repo/crypto/dsa/dsa_vrf.c
    repo/crypto/dso/dso_dl.c
    repo/crypto/dso/dso_dlfcn.c
    repo/crypto/dso/dso_err.c
    repo/crypto/dso/dso_lib.c
    repo/crypto/dso/dso_openssl.c
    repo/crypto/dso/dso_vms.c
    repo/crypto/dso/dso_win32.c
    repo/crypto/ebcdic.c
    repo/crypto/ec/curve25519.c
    repo/crypto/ec/curve448/arch_32/f_impl32.c
    repo/crypto/ec/curve448/arch_64/f_impl64.c
    repo/crypto/ec/curve448/curve448.c
    repo/crypto/ec/curve448/curve448_tables.c
    repo/crypto/ec/curve448/eddsa.c
    repo/crypto/ec/curve448/f_generic.c
    repo/crypto/ec/curve448/scalar.c
    repo/crypto/ec/ec2_oct.c
    repo/crypto/ec/ec2_smpl.c
    repo/crypto/ec/ec_ameth.c
    repo/crypto/ec/ec_asn1.c
    repo/crypto/ec/ec_backend.c
    repo/crypto/ec/ec_check.c
    repo/crypto/ec/ec_curve.c
    repo/crypto/ec/ec_cvt.c
    repo/crypto/ec/ec_deprecated.c
    repo/crypto/ec/ec_err.c
    repo/crypto/ec/ec_key.c
    repo/crypto/ec/ec_kmeth.c
    repo/crypto/ec/ec_lib.c
    repo/crypto/ec/ec_mult.c
    repo/crypto/ec/ec_oct.c
    repo/crypto/ec/ec_pmeth.c
    repo/crypto/ec/ec_print.c
    repo/crypto/ec/ecdh_kdf.c
    repo/crypto/ec/ecdh_ossl.c
    repo/crypto/ec/ecdsa_ossl.c
    repo/crypto/ec/ecdsa_sign.c
    repo/crypto/ec/ecdsa_vrf.c
    repo/crypto/ec/eck_prn.c
    repo/crypto/ec/ecp_mont.c
    repo/crypto/ec/ecp_nist.c
    repo/crypto/ec/ecp_oct.c
    repo/crypto/ec/ecp_smpl.c
    repo/crypto/ec/ecx_backend.c
    repo/crypto/ec/ecx_key.c
    repo/crypto/ec/ecx_meth.c
    repo/crypto/encode_decode/decoder_err.c
    repo/crypto/encode_decode/decoder_lib.c
    repo/crypto/encode_decode/decoder_meth.c
    repo/crypto/encode_decode/decoder_pkey.c
    repo/crypto/encode_decode/encoder_err.c
    repo/crypto/encode_decode/encoder_lib.c
    repo/crypto/encode_decode/encoder_meth.c
    repo/crypto/encode_decode/encoder_pkey.c
    repo/crypto/err/err.c
    repo/crypto/err/err_all.c
    repo/crypto/err/err_all_legacy.c
    repo/crypto/err/err_blocks.c
    repo/crypto/err/err_mark.c
    repo/crypto/err/err_prn.c
    repo/crypto/err/err_save.c
    repo/crypto/ess/ess_asn1.c
    repo/crypto/ess/ess_err.c
    repo/crypto/ess/ess_lib.c
    repo/crypto/evp/asymcipher.c
    repo/crypto/evp/bio_b64.c
    repo/crypto/evp/bio_enc.c
    repo/crypto/evp/bio_md.c
    repo/crypto/evp/bio_ok.c
    repo/crypto/evp/c_allc.c
    repo/crypto/evp/c_alld.c
    repo/crypto/evp/cmeth_lib.c
    repo/crypto/evp/ctrl_params_translate.c
    repo/crypto/evp/dh_ctrl.c
    repo/crypto/evp/dh_support.c
    repo/crypto/evp/digest.c
    repo/crypto/evp/dsa_ctrl.c
    repo/crypto/evp/e_aes.c
    repo/crypto/evp/e_aes_cbc_hmac_sha1.c
    repo/crypto/evp/e_aes_cbc_hmac_sha256.c
    repo/crypto/evp/e_aria.c
    repo/crypto/evp/e_bf.c
    repo/crypto/evp/e_camellia.c
    repo/crypto/evp/e_cast.c
    repo/crypto/evp/e_chacha20_poly1305.c
    repo/crypto/evp/e_des.c
    repo/crypto/evp/e_des3.c
    repo/crypto/evp/e_idea.c
    repo/crypto/evp/e_null.c
    repo/crypto/evp/e_rc2.c
    repo/crypto/evp/e_rc4.c
    repo/crypto/evp/e_rc4_hmac_md5.c
    repo/crypto/evp/e_rc5.c
    repo/crypto/evp/e_seed.c
    repo/crypto/evp/e_sm4.c
    repo/crypto/evp/e_xcbc_d.c
    repo/crypto/evp/ec_ctrl.c
    repo/crypto/evp/ec_support.c
    repo/crypto/evp/encode.c
    repo/crypto/evp/evp_cnf.c
    repo/crypto/evp/evp_enc.c
    repo/crypto/evp/evp_err.c
    repo/crypto/evp/evp_fetch.c
    repo/crypto/evp/evp_key.c
    repo/crypto/evp/evp_lib.c
    repo/crypto/evp/evp_pbe.c
    repo/crypto/evp/evp_pkey.c
    repo/crypto/evp/evp_rand.c
    repo/crypto/evp/evp_utils.c
    repo/crypto/evp/exchange.c
    repo/crypto/evp/kdf_lib.c
    repo/crypto/evp/kdf_meth.c
    repo/crypto/evp/kem.c
    repo/crypto/evp/keymgmt_lib.c
    repo/crypto/evp/keymgmt_meth.c
    repo/crypto/evp/legacy_blake2.c
    repo/crypto/evp/legacy_md4.c
    repo/crypto/evp/legacy_md5.c
    repo/crypto/evp/legacy_md5_sha1.c
    repo/crypto/evp/legacy_mdc2.c
    repo/crypto/evp/legacy_ripemd.c
    repo/crypto/evp/legacy_sha.c
    repo/crypto/evp/legacy_wp.c
    repo/crypto/evp/m_null.c
    repo/crypto/evp/m_sigver.c
    repo/crypto/evp/mac_lib.c
    repo/crypto/evp/mac_meth.c
    repo/crypto/evp/names.c
    repo/crypto/evp/p5_crpt.c
    repo/crypto/evp/p5_crpt2.c
    repo/crypto/evp/p_dec.c
    repo/crypto/evp/p_enc.c
    repo/crypto/evp/p_legacy.c
    repo/crypto/evp/p_lib.c
    repo/crypto/evp/p_open.c
    repo/crypto/evp/p_seal.c
    repo/crypto/evp/p_sign.c
    repo/crypto/evp/p_verify.c
    repo/crypto/evp/pbe_scrypt.c
    repo/crypto/evp/pmeth_check.c
    repo/crypto/evp/pmeth_gn.c
    repo/crypto/evp/pmeth_lib.c
    repo/crypto/evp/s_lib.c
    repo/crypto/evp/signature.c
    repo/crypto/evp/skeymgmt_meth.c
    repo/crypto/ex_data.c
    repo/crypto/ffc/ffc_backend.c
    repo/crypto/ffc/ffc_dh.c
    repo/crypto/ffc/ffc_key_generate.c
    repo/crypto/ffc/ffc_key_validate.c
    repo/crypto/ffc/ffc_params.c
    repo/crypto/ffc/ffc_params_generate.c
    repo/crypto/ffc/ffc_params_validate.c
    repo/crypto/getenv.c
    repo/crypto/hashtable/hashfunc.c
    repo/crypto/hashtable/hashtable.c
    repo/crypto/hmac/hmac.c
    repo/crypto/hpke/hpke.c
    repo/crypto/hpke/hpke_util.c
    repo/crypto/http/http_client.c
    repo/crypto/http/http_err.c
    repo/crypto/http/http_lib.c
    repo/crypto/idea/i_cbc.c
    repo/crypto/idea/i_cfb64.c
    repo/crypto/idea/i_ecb.c
    repo/crypto/idea/i_ofb64.c
    repo/crypto/idea/i_skey.c
    repo/crypto/indicator_core.c
    repo/crypto/info.c
    repo/crypto/init.c
    repo/crypto/initthread.c
    repo/crypto/kdf/kdf_err.c
    repo/crypto/lhash/lh_stats.c
    repo/crypto/lhash/lhash.c
    repo/crypto/md4/md4_dgst.c
    repo/crypto/md4/md4_one.c
    repo/crypto/md5/md5_dgst.c
    repo/crypto/md5/md5_one.c
    repo/crypto/md5/md5_sha1.c
    repo/crypto/mdc2/mdc2_one.c
    repo/crypto/mdc2/mdc2dgst.c
    repo/crypto/mem.c
    repo/crypto/mem_clr.c
    repo/crypto/mem_sec.c
    repo/crypto/ml_dsa/ml_dsa_encoders.c
    repo/crypto/ml_dsa/ml_dsa_key.c
    repo/crypto/ml_dsa/ml_dsa_key_compress.c
    repo/crypto/ml_dsa/ml_dsa_matrix.c
    repo/crypto/ml_dsa/ml_dsa_ntt.c
    repo/crypto/ml_dsa/ml_dsa_params.c
    repo/crypto/ml_dsa/ml_dsa_sample.c
    repo/crypto/ml_dsa/ml_dsa_sign.c
    repo/crypto/ml_kem/ml_kem.c
    repo/crypto/modes/cbc128.c
    repo/crypto/modes/ccm128.c
    repo/crypto/modes/cfb128.c
    repo/crypto/modes/ctr128.c
    repo/crypto/modes/cts128.c
    repo/crypto/modes/gcm128.c
    repo/crypto/modes/ocb128.c
    repo/crypto/modes/ofb128.c
    repo/crypto/modes/siv128.c
    repo/crypto/modes/wrap128.c
    repo/crypto/modes/xts128.c
    repo/crypto/modes/xts128gb.c
    ${GEN}/crypto/o_dir.c
    repo/crypto/o_fopen.c
    repo/crypto/o_init.c
    repo/crypto/o_str.c
    repo/crypto/o_time.c
    repo/crypto/objects/o_names.c
    repo/crypto/objects/obj_dat.c
    repo/crypto/objects/obj_err.c
    repo/crypto/objects/obj_lib.c
    repo/crypto/objects/obj_xref.c
    repo/crypto/ocsp/ocsp_asn.c
    repo/crypto/ocsp/ocsp_cl.c
    repo/crypto/ocsp/ocsp_err.c
    repo/crypto/ocsp/ocsp_ext.c
    repo/crypto/ocsp/ocsp_http.c
    repo/crypto/ocsp/ocsp_lib.c
    repo/crypto/ocsp/ocsp_prn.c
    repo/crypto/ocsp/ocsp_srv.c
    repo/crypto/ocsp/ocsp_vfy.c
    repo/crypto/ocsp/v3_ocsp.c
    repo/crypto/packet.c
    repo/crypto/param_build.c
    repo/crypto/param_build_set.c
    repo/crypto/params.c
    repo/crypto/params_dup.c
    repo/crypto/params_from_text.c
    repo/crypto/passphrase.c
    repo/crypto/pem/pem_all.c
    repo/crypto/pem/pem_err.c
    repo/crypto/pem/pem_info.c
    repo/crypto/pem/pem_lib.c
    repo/crypto/pem/pem_oth.c
    repo/crypto/pem/pem_pk8.c
    repo/crypto/pem/pem_pkey.c
    repo/crypto/pem/pem_sign.c
    repo/crypto/pem/pem_x509.c
    repo/crypto/pem/pem_xaux.c
    repo/crypto/pem/pvkfmt.c
    repo/crypto/pkcs12/p12_add.c
    repo/crypto/pkcs12/p12_asn.c
    repo/crypto/pkcs12/p12_attr.c
    repo/crypto/pkcs12/p12_crpt.c
    repo/crypto/pkcs12/p12_crt.c
    repo/crypto/pkcs12/p12_decr.c
    repo/crypto/pkcs12/p12_init.c
    repo/crypto/pkcs12/p12_key.c
    repo/crypto/pkcs12/p12_kiss.c
    repo/crypto/pkcs12/p12_mutl.c
    repo/crypto/pkcs12/p12_npas.c
    repo/crypto/pkcs12/p12_p8d.c
    repo/crypto/pkcs12/p12_p8e.c
    repo/crypto/pkcs12/p12_sbag.c
    repo/crypto/pkcs12/p12_utl.c
    repo/crypto/pkcs12/pk12err.c
    repo/crypto/pkcs7/bio_pk7.c
    repo/crypto/pkcs7/pk7_asn1.c
    repo/crypto/pkcs7/pk7_attr.c
    repo/crypto/pkcs7/pk7_doit.c
    repo/crypto/pkcs7/pk7_lib.c
    repo/crypto/pkcs7/pk7_mime.c
    repo/crypto/pkcs7/pk7_smime.c
    repo/crypto/pkcs7/pkcs7err.c
    repo/crypto/poly1305/poly1305.c
    repo/crypto/property/defn_cache.c
    repo/crypto/property/property.c
    repo/crypto/property/property_err.c
    repo/crypto/property/property_parse.c
    repo/crypto/property/property_query.c
    repo/crypto/property/property_string.c
    repo/crypto/provider.c
    repo/crypto/provider_child.c
    repo/crypto/provider_conf.c
    repo/crypto/provider_core.c
    repo/crypto/provider_predefined.c
    repo/crypto/punycode.c
    repo/crypto/quic_vlint.c
    repo/crypto/rand/prov_seed.c
    repo/crypto/rand/rand_deprecated.c
    repo/crypto/rand/rand_err.c
    repo/crypto/rand/rand_lib.c
    repo/crypto/rand/rand_meth.c
    repo/crypto/rand/rand_pool.c
    repo/crypto/rand/rand_uniform.c
    ${GEN}/crypto/rand/randfile.c
    repo/crypto/rc2/rc2_cbc.c
    repo/crypto/rc2/rc2_ecb.c
    repo/crypto/rc2/rc2_skey.c
    repo/crypto/rc2/rc2cfb64.c
    repo/crypto/rc2/rc2ofb64.c
    repo/crypto/rc4/rc4_enc.c
    repo/crypto/rc4/rc4_skey.c
    repo/crypto/ripemd/rmd_dgst.c
    repo/crypto/ripemd/rmd_one.c
    repo/crypto/rsa/rsa_ameth.c
    repo/crypto/rsa/rsa_asn1.c
    repo/crypto/rsa/rsa_backend.c
    repo/crypto/rsa/rsa_chk.c
    repo/crypto/rsa/rsa_crpt.c
    repo/crypto/rsa/rsa_err.c
    repo/crypto/rsa/rsa_gen.c
    repo/crypto/rsa/rsa_lib.c
    repo/crypto/rsa/rsa_meth.c
    repo/crypto/rsa/rsa_mp.c
    repo/crypto/rsa/rsa_mp_names.c
    repo/crypto/rsa/rsa_none.c
    repo/crypto/rsa/rsa_oaep.c
    repo/crypto/rsa/rsa_ossl.c
    repo/crypto/rsa/rsa_pk1.c
    repo/crypto/rsa/rsa_pmeth.c
    repo/crypto/rsa/rsa_prn.c
    repo/crypto/rsa/rsa_pss.c
    repo/crypto/rsa/rsa_saos.c
    repo/crypto/rsa/rsa_schemes.c
    repo/crypto/rsa/rsa_sign.c
    repo/crypto/rsa/rsa_sp800_56b_check.c
    repo/crypto/rsa/rsa_sp800_56b_gen.c
    repo/crypto/rsa/rsa_x931.c
    repo/crypto/rsa/rsa_x931g.c
    repo/crypto/seed/seed.c
    repo/crypto/seed/seed_cbc.c
    repo/crypto/seed/seed_cfb.c
    repo/crypto/seed/seed_ecb.c
    repo/crypto/seed/seed_ofb.c
    repo/crypto/self_test_core.c
    repo/crypto/sha/keccak1600.c
    repo/crypto/sha/sha1_one.c
    repo/crypto/sha/sha1dgst.c
    repo/crypto/sha/sha256.c
    repo/crypto/sha/sha3.c
    repo/crypto/sha/sha512.c
    repo/crypto/siphash/siphash.c
    repo/crypto/sleep.c
    repo/crypto/slh_dsa/slh_adrs.c
    repo/crypto/slh_dsa/slh_dsa.c
    repo/crypto/slh_dsa/slh_dsa_hash_ctx.c
    repo/crypto/slh_dsa/slh_dsa_key.c
    repo/crypto/slh_dsa/slh_fors.c
    repo/crypto/slh_dsa/slh_hash.c
    repo/crypto/slh_dsa/slh_hypertree.c
    repo/crypto/slh_dsa/slh_params.c
    repo/crypto/slh_dsa/slh_wots.c
    repo/crypto/slh_dsa/slh_xmss.c
    repo/crypto/sm2/sm2_crypt.c
    repo/crypto/sm2/sm2_err.c
    repo/crypto/sm2/sm2_key.c
    repo/crypto/sm2/sm2_sign.c
    repo/crypto/sm3/legacy_sm3.c
    repo/crypto/sm3/sm3.c
    repo/crypto/sm4/sm4.c
    repo/crypto/sparse_array.c
    repo/crypto/srp/srp_lib.c
    repo/crypto/srp/srp_vfy.c
    repo/crypto/ssl_err.c
    repo/crypto/stack/stack.c
    repo/crypto/store/store_err.c
    repo/crypto/store/store_init.c
    repo/crypto/store/store_lib.c
    repo/crypto/store/store_meth.c
    repo/crypto/store/store_register.c
    repo/crypto/store/store_result.c
    repo/crypto/store/store_strings.c
    repo/crypto/thread/api.c
    ${GEN}/crypto/thread/arch/thread_posix.c
    ${GEN}/crypto/thread/arch/thread_win.c
    repo/crypto/threads_lib.c
    repo/crypto/threads_none.c
    ${GEN}/crypto/threads_pthread.c
    ${GEN}/crypto/threads_win.c
    ${GEN}/crypto/time.c
    repo/crypto/trace.c
    repo/crypto/ts/ts_asn1.c
    repo/crypto/ts/ts_conf.c
    repo/crypto/ts/ts_err.c
    repo/crypto/ts/ts_lib.c
    repo/crypto/ts/ts_req_print.c
    repo/crypto/ts/ts_req_utils.c
    repo/crypto/ts/ts_rsp_print.c
    repo/crypto/ts/ts_rsp_sign.c
    repo/crypto/ts/ts_rsp_utils.c
    repo/crypto/ts/ts_rsp_verify.c
    repo/crypto/ts/ts_verify_ctx.c
    repo/crypto/txt_db/txt_db.c
    repo/crypto/ui/ui_err.c
    repo/crypto/ui/ui_lib.c
    repo/crypto/ui/ui_null.c
    repo/crypto/ui/ui_openssl.c
    repo/crypto/ui/ui_util.c
    repo/crypto/uid.c
    repo/crypto/whrlpool/wp_block.c
    repo/crypto/whrlpool/wp_dgst.c
    repo/crypto/x509/by_dir.c
    repo/crypto/x509/by_file.c
    repo/crypto/x509/by_store.c
    repo/crypto/x509/pcy_cache.c
    repo/crypto/x509/pcy_data.c
    repo/crypto/x509/pcy_lib.c
    repo/crypto/x509/pcy_map.c
    repo/crypto/x509/pcy_node.c
    repo/crypto/x509/pcy_tree.c
    repo/crypto/x509/t_acert.c
    repo/crypto/x509/t_crl.c
    repo/crypto/x509/t_req.c
    repo/crypto/x509/t_x509.c
    repo/crypto/x509/v3_aaa.c
    repo/crypto/x509/v3_ac_tgt.c
    repo/crypto/x509/v3_addr.c
    repo/crypto/x509/v3_admis.c
    repo/crypto/x509/v3_akeya.c
    repo/crypto/x509/v3_akid.c
    repo/crypto/x509/v3_asid.c
    repo/crypto/x509/v3_attrdesc.c
    repo/crypto/x509/v3_attrmap.c
    repo/crypto/x509/v3_audit_id.c
    repo/crypto/x509/v3_authattid.c
    repo/crypto/x509/v3_battcons.c
    repo/crypto/x509/v3_bcons.c
    repo/crypto/x509/v3_bitst.c
    repo/crypto/x509/v3_conf.c
    repo/crypto/x509/v3_cpols.c
    repo/crypto/x509/v3_crld.c
    repo/crypto/x509/v3_enum.c
    repo/crypto/x509/v3_extku.c
    repo/crypto/x509/v3_genn.c
    repo/crypto/x509/v3_group_ac.c
    repo/crypto/x509/v3_ia5.c
    repo/crypto/x509/v3_ind_iss.c
    repo/crypto/x509/v3_info.c
    repo/crypto/x509/v3_int.c
    repo/crypto/x509/v3_iobo.c
    repo/crypto/x509/v3_ist.c
    repo/crypto/x509/v3_lib.c
    repo/crypto/x509/v3_ncons.c
    repo/crypto/x509/v3_no_ass.c
    repo/crypto/x509/v3_no_rev_avail.c
    repo/crypto/x509/v3_pci.c
    repo/crypto/x509/v3_pcia.c
    repo/crypto/x509/v3_pcons.c
    repo/crypto/x509/v3_pku.c
    repo/crypto/x509/v3_pmaps.c
    repo/crypto/x509/v3_prn.c
    repo/crypto/x509/v3_purp.c
    repo/crypto/x509/v3_rolespec.c
    repo/crypto/x509/v3_san.c
    repo/crypto/x509/v3_sda.c
    repo/crypto/x509/v3_single_use.c
    repo/crypto/x509/v3_skid.c
    repo/crypto/x509/v3_soa_id.c
    repo/crypto/x509/v3_sxnet.c
    repo/crypto/x509/v3_timespec.c
    repo/crypto/x509/v3_tlsf.c
    repo/crypto/x509/v3_usernotice.c
    repo/crypto/x509/v3_utf8.c
    repo/crypto/x509/v3_utl.c
    repo/crypto/x509/v3err.c
    repo/crypto/x509/x509_acert.c
    repo/crypto/x509/x509_att.c
    repo/crypto/x509/x509_cmp.c
    repo/crypto/x509/x509_d2.c
    repo/crypto/x509/x509_def.c
    repo/crypto/x509/x509_err.c
    repo/crypto/x509/x509_ext.c
    repo/crypto/x509/x509_lu.c
    repo/crypto/x509/x509_meth.c
    repo/crypto/x509/x509_obj.c
    repo/crypto/x509/x509_r2x.c
    repo/crypto/x509/x509_req.c
    repo/crypto/x509/x509_set.c
    repo/crypto/x509/x509_trust.c
    repo/crypto/x509/x509_txt.c
    repo/crypto/x509/x509_v3.c
    repo/crypto/x509/x509_vfy.c
    repo/crypto/x509/x509_vpm.c
    repo/crypto/x509/x509aset.c
    repo/crypto/x509/x509cset.c
    repo/crypto/x509/x509name.c
    repo/crypto/x509/x509rset.c
    repo/crypto/x509/x509spki.c
    repo/crypto/x509/x509type.c
    repo/crypto/x509/x_all.c
    repo/crypto/x509/x_attrib.c
    repo/crypto/x509/x_crl.c
    repo/crypto/x509/x_exten.c
    repo/crypto/x509/x_ietfatt.c
    repo/crypto/x509/x_name.c
    repo/crypto/x509/x_pubkey.c
    repo/crypto/x509/x_req.c
    repo/crypto/x509/x_x509.c
    repo/crypto/x509/x_x509a.c
    repo/providers/baseprov.c
    repo/providers/common/bio_prov.c
    repo/providers/common/capabilities.c
    repo/providers/common/der/der_dsa_key.c
    repo/providers/common/der/der_dsa_sig.c
    repo/providers/common/der/der_ec_key.c
    repo/providers/common/der/der_ec_sig.c
    repo/providers/common/der/der_ecx_key.c
    repo/providers/common/der/der_ml_dsa_key.c
    repo/providers/common/der/der_rsa_key.c
    repo/providers/common/der/der_rsa_sig.c
    repo/providers/common/der/der_slh_dsa_key.c
    repo/providers/common/der/der_sm2_key.c
    repo/providers/common/der/der_sm2_sig.c
    repo/providers/common/digest_to_nid.c
    repo/providers/common/provider_ctx.c
    repo/providers/common/provider_err.c
    repo/providers/common/provider_seeding.c
    repo/providers/common/provider_util.c
    repo/providers/common/securitycheck.c
    repo/providers/common/securitycheck_default.c
    repo/providers/defltprov.c
    repo/providers/implementations/asymciphers/rsa_enc.c
    repo/providers/implementations/asymciphers/sm2_enc.c
    repo/providers/implementations/ciphers/cipher_aes.c
    repo/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha.c
    repo/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha1_hw.c
    repo/providers/implementations/ciphers/cipher_aes_cbc_hmac_sha256_hw.c
    repo/providers/implementations/ciphers/cipher_aes_ccm.c
    repo/providers/implementations/ciphers/cipher_aes_ccm_hw.c
    repo/providers/implementations/ciphers/cipher_aes_gcm.c
    repo/providers/implementations/ciphers/cipher_aes_gcm_hw.c
    repo/providers/implementations/ciphers/cipher_aes_gcm_siv.c
    repo/providers/implementations/ciphers/cipher_aes_gcm_siv_hw.c
    repo/providers/implementations/ciphers/cipher_aes_gcm_siv_polyval.c
    repo/providers/implementations/ciphers/cipher_aes_hw.c
    repo/providers/implementations/ciphers/cipher_aes_ocb.c
    repo/providers/implementations/ciphers/cipher_aes_ocb_hw.c
    repo/providers/implementations/ciphers/cipher_aes_siv.c
    repo/providers/implementations/ciphers/cipher_aes_siv_hw.c
    repo/providers/implementations/ciphers/cipher_aes_wrp.c
    repo/providers/implementations/ciphers/cipher_aes_xts.c
    repo/providers/implementations/ciphers/cipher_aes_xts_fips.c
    repo/providers/implementations/ciphers/cipher_aes_xts_hw.c
    repo/providers/implementations/ciphers/cipher_aria.c
    repo/providers/implementations/ciphers/cipher_aria_ccm.c
    repo/providers/implementations/ciphers/cipher_aria_ccm_hw.c
    repo/providers/implementations/ciphers/cipher_aria_gcm.c
    repo/providers/implementations/ciphers/cipher_aria_gcm_hw.c
    repo/providers/implementations/ciphers/cipher_aria_hw.c
    repo/providers/implementations/ciphers/cipher_blowfish.c
    repo/providers/implementations/ciphers/cipher_blowfish_hw.c
    repo/providers/implementations/ciphers/cipher_camellia.c
    repo/providers/implementations/ciphers/cipher_camellia_hw.c
    repo/providers/implementations/ciphers/cipher_cast5.c
    repo/providers/implementations/ciphers/cipher_cast5_hw.c
    repo/providers/implementations/ciphers/cipher_chacha20.c
    repo/providers/implementations/ciphers/cipher_chacha20_hw.c
    repo/providers/implementations/ciphers/cipher_chacha20_poly1305.c
    repo/providers/implementations/ciphers/cipher_chacha20_poly1305_hw.c
    repo/providers/implementations/ciphers/cipher_cts.c
    repo/providers/implementations/ciphers/cipher_des.c
    repo/providers/implementations/ciphers/cipher_des_hw.c
    repo/providers/implementations/ciphers/cipher_desx.c
    repo/providers/implementations/ciphers/cipher_desx_hw.c
    repo/providers/implementations/ciphers/cipher_idea.c
    repo/providers/implementations/ciphers/cipher_idea_hw.c
    repo/providers/implementations/ciphers/cipher_null.c
    repo/providers/implementations/ciphers/cipher_rc2.c
    repo/providers/implementations/ciphers/cipher_rc2_hw.c
    repo/providers/implementations/ciphers/cipher_rc4.c
    repo/providers/implementations/ciphers/cipher_rc4_hmac_md5.c
    repo/providers/implementations/ciphers/cipher_rc4_hmac_md5_hw.c
    repo/providers/implementations/ciphers/cipher_rc4_hw.c
    repo/providers/implementations/ciphers/cipher_seed.c
    repo/providers/implementations/ciphers/cipher_seed_hw.c
    repo/providers/implementations/ciphers/cipher_sm4.c
    repo/providers/implementations/ciphers/cipher_sm4_ccm.c
    repo/providers/implementations/ciphers/cipher_sm4_ccm_hw.c
    repo/providers/implementations/ciphers/cipher_sm4_gcm.c
    repo/providers/implementations/ciphers/cipher_sm4_gcm_hw.c
    repo/providers/implementations/ciphers/cipher_sm4_hw.c
    repo/providers/implementations/ciphers/cipher_sm4_xts.c
    repo/providers/implementations/ciphers/cipher_sm4_xts_hw.c
    repo/providers/implementations/ciphers/cipher_tdes.c
    repo/providers/implementations/ciphers/cipher_tdes_common.c
    repo/providers/implementations/ciphers/cipher_tdes_default.c
    repo/providers/implementations/ciphers/cipher_tdes_default_hw.c
    repo/providers/implementations/ciphers/cipher_tdes_hw.c
    repo/providers/implementations/ciphers/cipher_tdes_wrap.c
    repo/providers/implementations/ciphers/cipher_tdes_wrap_hw.c
    repo/providers/implementations/ciphers/ciphercommon.c
    repo/providers/implementations/ciphers/ciphercommon_block.c
    repo/providers/implementations/ciphers/ciphercommon_ccm.c
    repo/providers/implementations/ciphers/ciphercommon_ccm_hw.c
    repo/providers/implementations/ciphers/ciphercommon_gcm.c
    repo/providers/implementations/ciphers/ciphercommon_gcm_hw.c
    repo/providers/implementations/ciphers/ciphercommon_hw.c
    repo/providers/implementations/digests/blake2_prov.c
    repo/providers/implementations/digests/blake2b_prov.c
    repo/providers/implementations/digests/blake2s_prov.c
    repo/providers/implementations/digests/digestcommon.c
    repo/providers/implementations/digests/md4_prov.c
    repo/providers/implementations/digests/md5_prov.c
    repo/providers/implementations/digests/md5_sha1_prov.c
    repo/providers/implementations/digests/mdc2_prov.c
    repo/providers/implementations/digests/null_prov.c
    repo/providers/implementations/digests/ripemd_prov.c
    repo/providers/implementations/digests/sha2_prov.c
    repo/providers/implementations/digests/sha3_prov.c
    repo/providers/implementations/digests/sm3_prov.c
    repo/providers/implementations/digests/wp_prov.c
    repo/providers/implementations/encode_decode/decode_der2key.c
    repo/providers/implementations/encode_decode/decode_epki2pki.c
    repo/providers/implementations/encode_decode/decode_msblob2key.c
    repo/providers/implementations/encode_decode/decode_pem2der.c
    repo/providers/implementations/encode_decode/decode_pvk2key.c
    repo/providers/implementations/encode_decode/decode_spki2typespki.c
    repo/providers/implementations/encode_decode/encode_key2any.c
    repo/providers/implementations/encode_decode/encode_key2blob.c
    repo/providers/implementations/encode_decode/encode_key2ms.c
    repo/providers/implementations/encode_decode/encode_key2text.c
    repo/providers/implementations/encode_decode/endecoder_common.c
    repo/providers/implementations/encode_decode/ml_common_codecs.c
    repo/providers/implementations/encode_decode/ml_dsa_codecs.c
    repo/providers/implementations/encode_decode/ml_kem_codecs.c
    repo/providers/implementations/exchange/dh_exch.c
    repo/providers/implementations/exchange/ecdh_exch.c
    repo/providers/implementations/exchange/ecx_exch.c
    repo/providers/implementations/exchange/kdf_exch.c
    repo/providers/implementations/kdfs/argon2.c
    repo/providers/implementations/kdfs/hkdf.c
    repo/providers/implementations/kdfs/hmacdrbg_kdf.c
    repo/providers/implementations/kdfs/kbkdf.c
    repo/providers/implementations/kdfs/krb5kdf.c
    repo/providers/implementations/kdfs/pbkdf1.c
    repo/providers/implementations/kdfs/pbkdf2.c
    repo/providers/implementations/kdfs/pbkdf2_fips.c
    repo/providers/implementations/kdfs/pkcs12kdf.c
    repo/providers/implementations/kdfs/pvkkdf.c
    repo/providers/implementations/kdfs/scrypt.c
    repo/providers/implementations/kdfs/sshkdf.c
    repo/providers/implementations/kdfs/sskdf.c
    repo/providers/implementations/kdfs/tls1_prf.c
    repo/providers/implementations/kdfs/x942kdf.c
    repo/providers/implementations/kem/ec_kem.c
    repo/providers/implementations/kem/ecx_kem.c
    repo/providers/implementations/kem/kem_util.c
    repo/providers/implementations/kem/ml_kem_kem.c
    repo/providers/implementations/kem/mlx_kem.c
    repo/providers/implementations/kem/rsa_kem.c
    repo/providers/implementations/keymgmt/dh_kmgmt.c
    repo/providers/implementations/keymgmt/dsa_kmgmt.c
    repo/providers/implementations/keymgmt/ec_kmgmt.c
    repo/providers/implementations/keymgmt/ecx_kmgmt.c
    repo/providers/implementations/keymgmt/kdf_legacy_kmgmt.c
    repo/providers/implementations/keymgmt/mac_legacy_kmgmt.c
    repo/providers/implementations/keymgmt/ml_dsa_kmgmt.c
    repo/providers/implementations/keymgmt/ml_kem_kmgmt.c
    repo/providers/implementations/keymgmt/mlx_kmgmt.c
    repo/providers/implementations/keymgmt/rsa_kmgmt.c
    repo/providers/implementations/keymgmt/slh_dsa_kmgmt.c
    repo/providers/implementations/macs/blake2b_mac.c
    repo/providers/implementations/macs/blake2s_mac.c
    repo/providers/implementations/macs/cmac_prov.c
    repo/providers/implementations/macs/gmac_prov.c
    repo/providers/implementations/macs/hmac_prov.c
    repo/providers/implementations/macs/kmac_prov.c
    repo/providers/implementations/macs/poly1305_prov.c
    repo/providers/implementations/macs/siphash_prov.c
    repo/providers/implementations/rands/drbg.c
    repo/providers/implementations/rands/drbg_ctr.c
    repo/providers/implementations/rands/drbg_hash.c
    repo/providers/implementations/rands/drbg_hmac.c
    repo/providers/implementations/rands/seed_src.c
    repo/providers/implementations/rands/seed_src_jitter.c
    repo/providers/implementations/rands/seeding/rand_cpu_x86.c
    repo/providers/implementations/rands/seeding/rand_tsc.c
    ${GEN}/providers/implementations/rands/seeding/rand_unix.c
    ${GEN}/providers/implementations/rands/seeding/rand_win.c
    repo/providers/implementations/rands/test_rng.c
    repo/providers/implementations/signature/dsa_sig.c
    repo/providers/implementations/signature/ecdsa_sig.c
    repo/providers/implementations/signature/eddsa_sig.c
    repo/providers/implementations/signature/mac_legacy_sig.c
    repo/providers/implementations/signature/ml_dsa_sig.c
    repo/providers/implementations/signature/rsa_sig.c
    repo/providers/implementations/signature/slh_dsa_sig.c
    repo/providers/implementations/signature/sm2_sig.c
    repo/providers/implementations/skeymgmt/aes_skmgmt.c
    repo/providers/implementations/skeymgmt/generic.c
    repo/providers/implementations/storemgmt/file_store.c
    repo/providers/implementations/storemgmt/file_store_any2obj.c
    repo/providers/legacyprov.c
    repo/providers/nullprov.c
    repo/providers/prov_running.c
    repo/ssl/record/methods/ssl3_cbc.c
    repo/ssl/record/methods/tls_pad.c
    ${GEN}/crypto/params_idx.c
    ${GEN}/providers/common/der/der_digests_gen.c
    ${GEN}/providers/common/der/der_dsa_gen.c
    ${GEN}/providers/common/der/der_ec_gen.c
    ${GEN}/providers/common/der/der_ecx_gen.c
    ${GEN}/providers/common/der/der_ml_dsa_gen.c
    ${GEN}/providers/common/der/der_rsa_gen.c
    ${GEN}/providers/common/der/der_slh_dsa_gen.c
    ${GEN}/providers/common/der/der_sm2_gen.c
    ${GEN}/providers/common/der/der_wrap_gen.c
    ${GEN}/libcrypto.def
)

target_include_directories(libcrypto PRIVATE
    ${GEN}/crypto
    ${GEN}/providers/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/crypto
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/providers/common/include
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/providers/implementations/include
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/providers/fips/include
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/crypto/bn
)

target_include_directories(libcrypto PUBLIC
    ${GEN}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/include
)

target_compile_definitions(libcrypto PRIVATE
    OPENSSL_BUILDING_OPENSSL
    STATIC_LEGACY
    L_ENDIAN
    WIN32_LEAN_AND_MEAN
    OPENSSLDIR="/etc/ssl"
    ENGINESDIR="/lib/engines-3"
    MODULESDIR="/lib/ossl-modules"
    NO_SYSLOG
    NO_SYS_PARAM_H
)

target_compile_options(libcrypto PRIVATE
    ${USERSPACE_CFLAGS}
    -fvisibility=default
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-missing-field-initializers
    -Wno-unused-but-set-variable
    -Wno-unused-function
    $<$<CXX_COMPILER_ID:GNU>:-Wno-discarded-qualifiers>
    -Wno-incompatible-pointer-types-discards-qualifiers
)

set_target_properties(libcrypto PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libcrypto"
)

target_link_libraries(libcrypto PRIVATE moxnox)

install(TARGETS libcrypto DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

add_library(libssl SHARED
    repo/crypto/packet.c
    repo/crypto/quic_vlint.c
    ${GEN}/crypto/time.c
    repo/ssl/bio_ssl.c
    repo/ssl/d1_lib.c
    repo/ssl/d1_msg.c
    repo/ssl/d1_srtp.c
    repo/ssl/methods.c
    repo/ssl/pqueue.c
    repo/ssl/quic/quic_tls.c
    repo/ssl/quic/quic_tls_api.c
    repo/ssl/record/methods/dtls_meth.c
    repo/ssl/record/methods/ssl3_cbc.c
    repo/ssl/record/methods/ssl3_meth.c
    repo/ssl/record/methods/tls13_meth.c
    repo/ssl/record/methods/tls1_meth.c
    repo/ssl/record/methods/tls_common.c
    repo/ssl/record/methods/tls_multib.c
    repo/ssl/record/methods/tls_pad.c
    repo/ssl/record/methods/tlsany_meth.c
    repo/ssl/record/rec_layer_d1.c
    repo/ssl/record/rec_layer_s3.c
    repo/ssl/rio/poll_immediate.c
    repo/ssl/s3_enc.c
    repo/ssl/s3_lib.c
    repo/ssl/s3_msg.c
    repo/ssl/ssl_asn1.c
    repo/ssl/ssl_cert.c
    repo/ssl/ssl_cert_comp.c
    repo/ssl/ssl_ciph.c
    repo/ssl/ssl_conf.c
    repo/ssl/ssl_err_legacy.c
    repo/ssl/ssl_init.c
    repo/ssl/ssl_lib.c
    repo/ssl/ssl_mcnf.c
    repo/ssl/ssl_rsa.c
    repo/ssl/ssl_rsa_legacy.c
    repo/ssl/ssl_sess.c
    repo/ssl/ssl_stat.c
    repo/ssl/ssl_txt.c
    repo/ssl/ssl_utst.c
    repo/ssl/statem/extensions.c
    repo/ssl/statem/extensions_clnt.c
    repo/ssl/statem/extensions_cust.c
    repo/ssl/statem/extensions_srvr.c
    repo/ssl/statem/statem.c
    repo/ssl/statem/statem_clnt.c
    repo/ssl/statem/statem_dtls.c
    repo/ssl/statem/statem_lib.c
    repo/ssl/statem/statem_srvr.c
    repo/ssl/t1_enc.c
    repo/ssl/t1_lib.c
    repo/ssl/t1_trce.c
    repo/ssl/tls13_enc.c
    repo/ssl/tls_depr.c
    repo/ssl/tls_srp.c
    ${GEN}/libssl.def
)

target_include_directories(libssl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/ssl
)

target_compile_definitions(libssl PRIVATE
    OPENSSL_BUILDING_OPENSSL
    L_ENDIAN
    WIN32_LEAN_AND_MEAN
    OPENSSLDIR="/etc/ssl"
    ENGINESDIR="/lib/engines-3"
    MODULESDIR="/lib/ossl-modules"
    RIO_POLL_METHOD=RIO_POLL_METHOD_POLL
    NO_SYS_PARAM_H
)

target_compile_options(libssl PRIVATE
    ${USERSPACE_CFLAGS}
    -fvisibility=default
    -Wno-sign-conversion
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-missing-field-initializers
)

set_target_properties(libssl PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libssl"
    IMPORT_PREFIX ""
)

target_link_libraries(libssl PRIVATE libcrypto moxnox)

install(TARGETS libssl DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)

install(DIRECTORY repo/include/openssl/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include/openssl
    PATTERN "*.in" EXCLUDE
    PATTERN "__DECC_INCLUDE_*" EXCLUDE
)

set(OPENSSL_GENERATED_HEADERS ${OPENSSL_GENERATED_FILES})
list(FILTER OPENSSL_GENERATED_HEADERS INCLUDE REGEX "/include/openssl/")
install(FILES ${OPENSSL_GENERATED_HEADERS} DESTINATION ${CMAKE_INSTALL_PREFIX}/include/openssl)

openssl_patch(apps/openssl.c
    -e [[s|    win32_utf8argv(&argc, &argv)\;|#ifndef MOXNOX\n    win32_utf8argv(\&argc, \&argv)\;\n#endif|g]]
)

openssl_patch(apps/cmp.c
    -e [[s|#if defined(_WIN32) && !defined(__BORLANDC__)\n#define access _access|#if defined(MOXNOX)\n#include <unistd.h>\n#elif defined(_WIN32) \&\& !defined(__BORLANDC__)\n#define access _access|g]]
)

openssl_patch(apps/lib/apps.c
    -e [[s|#include <sys/stat.h>|#include <sys/stat.h>\nint _access(const char *, int)\;|g]]
    -e [[s@#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)\n#include <conio.h>@#if (defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)) \&\& !defined(MOXNOX)\n#include <conio.h>@g]]
    -e [[s@#elif defined(OPENSSL_SYS_VXWORKS)\n#include <time.h>@#elif defined(OPENSSL_SYS_VXWORKS) || defined(MOXNOX)\n#include <time.h>@g]]
    -e [[s@#ifdef CLOCK_REALTIME@#if defined(CLOCK_REALTIME) || defined(MOXNOX)@g]]
    -e [[s|    clock_gettime(CLOCK_REALTIME, &now)\;|#ifdef MOXNOX\n    clock_gettime(CLOCK_MONOTONIC, \&now)\;\n#else\n    clock_gettime(CLOCK_REALTIME, \&now)\;\n#endif|g]]
    -e [[s|#if defined(OPENSSL_SYS_MSDOS)\nint has_stdin_waiting(void)|#if defined(OPENSSL_SYS_MSDOS) \&\& !defined(MOXNOX)\nint has_stdin_waiting(void)|g]]
)

openssl_patch(apps/s_client.c
    -e [[s@#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_VMS)@#if (defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS) || defined(OPENSSL_SYS_VMS)) \&\& !defined(MOXNOX)@g]]
    -e [[s@#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)@#if (defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)) \&\& !defined(MOXNOX)@g]]
    -e [[s@#if !defined(OPENSSL_SYS_WINDOWS) && !defined(OPENSSL_SYS_MSDOS)@#if !defined(OPENSSL_SYS_WINDOWS) \&\& !defined(OPENSSL_SYS_MSDOS) || defined(MOXNOX)@g]]
    -e [[s|/\* OPENSSL_SYS_MSDOS includes OPENSSL_SYS_WINDOWS \*/\n#if defined(OPENSSL_SYS_MSDOS)|/* OPENSSL_SYS_MSDOS includes OPENSSL_SYS_WINDOWS */\n#if defined(OPENSSL_SYS_MSDOS) \&\& !defined(MOXNOX)|g]]
)

openssl_patch(apps/s_server.c
    -e [[s@#if !(defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS))@#if !(defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)) || defined(MOXNOX)@g]]
    -e [[s@#if !defined(OPENSSL_SYS_WINDOWS) && !defined(OPENSSL_SYS_MSDOS)@#if !defined(OPENSSL_SYS_WINDOWS) \&\& !defined(OPENSSL_SYS_MSDOS) || defined(MOXNOX)@g]]
    -e [[s@#if defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)@#if (defined(OPENSSL_SYS_WINDOWS) || defined(OPENSSL_SYS_MSDOS)) \&\& !defined(MOXNOX)@g]]
)

openssl_patch(apps/speed.c
    -e [[s|#elif defined(OPENSSL_SYS_WINDOWS)\n            if (num_job_fds == 1|#elif defined(OPENSSL_SYS_WINDOWS) \&\& !defined(MOXNOX)\n            if (num_job_fds == 1|g]]
)

set(OPENSSL_APPS_SOURCES
    repo/apps/asn1parse.c
    repo/apps/ca.c
    repo/apps/ciphers.c
    ${GEN}/apps/cmp.c
    repo/apps/cms.c
    repo/apps/crl.c
    repo/apps/crl2pkcs7.c
    repo/apps/dgst.c
    repo/apps/dhparam.c
    repo/apps/dsa.c
    repo/apps/dsaparam.c
    repo/apps/ec.c
    repo/apps/ecparam.c
    repo/apps/enc.c
    repo/apps/errstr.c
    repo/apps/fipsinstall.c
    repo/apps/gendsa.c
    repo/apps/genpkey.c
    repo/apps/genrsa.c
    repo/apps/info.c
    repo/apps/kdf.c
    repo/apps/lib/app_libctx.c
    repo/apps/lib/app_params.c
    repo/apps/lib/app_provider.c
    repo/apps/lib/app_rand.c
    repo/apps/lib/app_x509.c
    ${GEN}/apps/lib/apps.c
    repo/apps/lib/apps_opt_printf.c
    repo/apps/lib/apps_ui.c
    repo/apps/lib/cmp_mock_srv.c
    repo/apps/lib/columns.c
    repo/apps/lib/engine.c
    repo/apps/lib/engine_loader.c
    repo/apps/lib/fmt.c
    repo/apps/lib/http_server.c
    repo/apps/lib/log.c
    repo/apps/lib/names.c
    repo/apps/lib/opt.c
    repo/apps/lib/s_cb.c
    repo/apps/lib/s_socket.c
    repo/apps/lib/tlssrp_depr.c
    repo/apps/list.c
    repo/apps/mac.c
    repo/apps/nseq.c
    repo/apps/ocsp.c
    ${GEN}/apps/openssl.c
    repo/apps/passwd.c
    repo/apps/pkcs12.c
    repo/apps/pkcs7.c
    repo/apps/pkcs8.c
    repo/apps/pkey.c
    repo/apps/pkeyparam.c
    repo/apps/pkeyutl.c
    repo/apps/prime.c
    repo/apps/rand.c
    repo/apps/rehash.c
    repo/apps/req.c
    repo/apps/rsa.c
    repo/apps/rsautl.c
    ${GEN}/apps/s_client.c
    ${GEN}/apps/s_server.c
    repo/apps/s_time.c
    repo/apps/sess_id.c
    repo/apps/skeyutl.c
    repo/apps/smime.c
    ${GEN}/apps/speed.c
    repo/apps/spkac.c
    repo/apps/srp.c
    repo/apps/storeutl.c
    repo/apps/ts.c
    repo/apps/verify.c
    repo/apps/version.c
    repo/apps/x509.c
    ${GEN}/apps/progs.c
)

add_executable(openssl_exe ${OPENSSL_APPS_SOURCES})

target_include_directories(openssl_exe PRIVATE
    ${GEN}/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/repo
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/apps
    ${CMAKE_CURRENT_SOURCE_DIR}/repo/apps/include
)

target_compile_definitions(openssl_exe PRIVATE
    L_ENDIAN
    WIN32_LEAN_AND_MEAN
    OPENSSLDIR="/etc/ssl"
    ENGINESDIR="/usr/lib/engines-3"
    MODULESDIR="/usr/lib/ossl-modules"
    HAVE_FORK=0
    NO_SYS_PARAM_H
)

target_compile_options(openssl_exe PRIVATE
    ${USERSPACE_CFLAGS}
    -Wno-conversion
    -Wno-unused-parameter
    -Wno-unused-variable
    -Wno-missing-field-initializers
    "SHELL:-include ${CMAKE_SOURCE_DIR}/include/sys/select.h"
    "SHELL:-include ${CMAKE_SOURCE_DIR}/include/sys/socket.h"
    "SHELL:-include ${CMAKE_SOURCE_DIR}/include/arpa/inet.h"
)

set_target_properties(openssl_exe PROPERTIES OUTPUT_NAME openssl)

target_link_libraries(openssl_exe PRIVATE libssl libcrypto moxnox)

install(TARGETS openssl_exe DESTINATION ${CMAKE_INSTALL_PREFIX}/bin)

add_custom_target(openssl_gen_headers DEPENDS
    ${GEN}/include/internal/e_os.h
    ${GEN}/include/internal/e_winsock.h
    ${GEN}/include/internal/sockets.h
    ${GEN}/include/openssl/e_os2.h
    ${GEN}/include/openssl/e_ostime.h
    ${GEN}/crypto/bio/bio_local.h
)
add_dependencies(libcrypto openssl_gen_headers openssl_generate)
add_dependencies(libssl openssl_gen_headers openssl_generate)
add_dependencies(openssl_exe openssl_gen_headers openssl_generate)

install(FILES cert.pem DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/ssl)

add_custom_target(openssl)
add_dependencies(openssl libcrypto libssl openssl_exe)
