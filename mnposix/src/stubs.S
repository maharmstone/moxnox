#include "dll_stubs.S"

stubs_start

do_stub "mlock"
do_stub "munlock"
do_stub "posix_spawnattr_setschedpolicy"
do_stub "posix_spawnattr_setschedparam"
do_stub "pthread_cancel"
do_stub "pthread_atfork"
do_stub "pipe2"
do_stub "fdatasync"
do_stub "secure_getenv"
do_stub "sig2str"
do_stub "str2sig"
do_stub "strptime"
do_stub "psignal"
do_stub "dprintf"
do_stub "getlocalename_l"
do_stub "alphasort"
do_stub "insque"
do_stub "remque"
do_stub "random"
do_stub "srandom"

stubs_end
