/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 *
 * Written by Kai Tietz  <kai.tietz@onevision.com>
 *
 * This file is used by if gcc is built with --enable-threads=win32.
 *
 * Based on version created by Mumit Khan  <khan@nanotech.wisc.edu>
 *
 */

#include <stdlib.h>
#include <stdint.h>

import nt;
import ntdll;

extern "C" void __cdecl __MINGW_NOTHROW _fpreset (void);

extern "C"
int
__mingw_TLScallback (HANDLE, uint32_t reason, void*)
{
  switch (reason)
    {
    case DLL_THREAD_ATTACH:
      _fpreset();
      break;
    }
  return 1;
}

