#include "dll_stubs.S"

stubs_start

do_stub "mlock"
do_stub "munlock"
do_stub "posix_spawnattr_setschedpolicy"
do_stub "posix_spawnattr_setschedparam"
do_stub "pthread_cancel"
do_stub "pthread_atfork"
do_stub "timer_settime"

stubs_end
