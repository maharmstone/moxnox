#include "dll_stubs.S"

stubs_start

do_stub "mlock"
do_stub "munlock"
do_stub "posix_spawnattr_setschedpolicy"
do_stub "posix_spawnattr_setschedparam"
do_stub "pthread_cancel"
do_stub "pthread_atfork"
do_stub "strlcat"
do_stub "stpcpy"
do_stub "stpncpy"
do_stub "reallocarray"
do_stub "getdelim"
do_stub "pipe2"
do_stub "pselect"
do_stub "fdatasync"
do_stub "secure_getenv"
do_stub "sig2str"
do_stub "str2sig"
do_stub "strptime"
do_stub "psignal"
do_stub "dprintf"
do_stub "getlocalename_l"
do_stub "alphasort"

stubs_end
