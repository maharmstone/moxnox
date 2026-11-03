/**
 * This file has no copyright assigned and is placed in the Public Domain.
 * This file is part of the mingw-w64 runtime package.
 * No warranty is given; refer to the file DISCLAIMER.PD within this package.
 */

#include <internal.h>
#include <process.h>
#include <signal.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>
#include <sect_attribs.h>
#include <locale.h>
#include <float.h>
#include <corecrt_startup.h>

extern "C"
NTSTATUS NtDelayExecution(bool alertable, const LARGE_INTEGER* delay_interval);

#if defined(__SEH__) && (!defined(__clang__) || __clang_major__ >= 7)
#define SEH_INLINE_ASM
#endif

extern IMAGE_DOS_HEADER __ImageBase;

extern "C" int *__cdecl __p__commode(void);

#undef _fmode
extern int _fmode;
#undef _commode
extern int _commode;
extern int _dowildcard;
extern int __globallocalestatus;

extern _PIFV __xi_a[];
extern _PIFV __xi_z[];
extern _PVFV __xc_a[];
extern _PVFV __xc_z[];


/* TLS initialization hook.  */
extern const PIMAGE_TLS_CALLBACK __dyn_tls_init_callback;

extern int __mingw_app_type;

static int argc;
extern "C" void __main(void);
static _TCHAR **argv;
static _TCHAR **envp;

static int has_cctor = 0;

extern "C" void _pei386_runtime_relocator (void);
extern "C" long CALLBACK _gnu_exception_handler (EXCEPTION_POINTERS * exception_data);
static int duplicate_ppstrings (int ac, _TCHAR ***av);

extern int _MINGW_INSTALL_DEBUG_MATHERR;

static void
safe_flush (void)
{
  fflush (NULL);
}

static int __tmainCRTStartup (void);

extern "C"
int mainCRTStartup (void);

extern "C"
__attribute__((used)) /* required due to GNU LD bug: https://sourceware.org/bugzilla/show_bug.cgi?id=30300 */
int mainCRTStartup (void)
{
  int ret = 255;
#ifdef SEH_INLINE_ASM
  asm ("\t.l_start:\n");
#endif
  __mingw_app_type = 0;
  ret = __tmainCRTStartup ();
#ifdef SEH_INLINE_ASM
  asm ("\tnop\n"
    "\t.l_end: nop\n"
#ifdef __arm__
    "\t.seh_handler __C_specific_handler, %except\n"
#else
    "\t.seh_handler __C_specific_handler, @except\n"
#endif
    "\t.seh_handlerdata\n"
    "\t.long 1\n"
    "\t.rva .l_start, .l_end, _gnu_exception_handler ,.l_end\n"
    "\t.text");
#endif
  return ret;
}

static
#if defined(__i386__) || defined(_X86_)
/* We need to make sure that we align the stack to 16 bytes for the sake of SSE
   opts in main or in functions called from main.  */
__attribute__((force_align_arg_pointer))
#endif
__declspec(noinline) int
__tmainCRTStartup (void)
{
    void *lock_free = NULL;
    void *fiberid = ((PNT_TIB)NtCurrentTeb())->StackBase;
    BOOL nested = FALSE;
    _startupinfo startinfo;
    int ret = 0;
    while((lock_free = InterlockedCompareExchangePointer (&__native_startup_lock,
							  fiberid, NULL)) != 0)
      {
	LARGE_INTEGER t;

	if (lock_free == fiberid)
	  {
	    nested = TRUE;
	    break;
	  }

	t.QuadPart = -10000000; // 1 second

	NtDelayExecution(false, &t);
      }
    if (__native_startup_state == __initializing)
      {
	_amsg_exit (31);
      }
    else if (__native_startup_state == __uninitialized)
      {
	__native_startup_state = __initializing;

	/* Before the UCRT stderr could be opened in full buffering
	 * mode, for example when output goes to a pipe.
	 *
	 * The C standard disallows full buffering on stderr. Note
	 * that line buffering is the same as full buffering in the
	 * Windows CRT, so we have to disable buffering altogether.
	 */
	setvbuf (stderr, NULL, _IONBF, 0);

	/* The C RunTime library flushes stdio streams in response to
	 * DLL_PROCESS_DETACH. This is not entirely safe; other DLLs
	 * may cause instant termination during process shutdown.
	 * Here we add an exit handler to flush streams safely.
	 */
	if (atexit (safe_flush) != 0)
	    abort ();

	_pei386_runtime_relocator ();

	_fpreset ();

	if (__mingw_app_type)
	  __set_app_type (_GUI_APP);
	else
	  __set_app_type (_CONSOLE_APP);

	*__p__fmode () = _fmode;
	*__p__commode () = _commode;

	ret = _setargv ();
	if (ret < 0)
	  _amsg_exit (8); /* _RT_SPACEARG */

	if (_MINGW_INSTALL_DEBUG_MATHERR == 1)
	  __setusermatherr (_matherr);

	if (__globallocalestatus == -1)
	  _configthreadlocale (-1);

	if (_initterm_e (__xi_a, __xi_z) != 0)
	  _amsg_exit (10); /* _RT_ABORT */

	startinfo.newmode = _newmode;

	ret = __getmainargs (&argc, &argv, &envp, _dowildcard, &startinfo);
	if (ret < 0)
	  _amsg_exit (8); /* _RT_SPACEARG */

	ret = duplicate_ppstrings (argc, &argv);
	if (ret != 0)
	  _amsg_exit (8); /* _RT_SPACEARG */

	_initterm (__xc_a, __xc_z);
	__main (); /* C++ initialization. */

	__native_startup_state = __initialized;
      }
    else
      has_cctor = 1;
    if (! nested)
      (VOID)InterlockedExchangePointer (&__native_startup_lock, NULL);

    if (__dyn_tls_init_callback != NULL)
      __dyn_tls_init_callback (NULL, DLL_THREAD_ATTACH, NULL);

    __initenv = envp;

    ret = _tmain (argc, argv, envp);

    exit (ret);

    if (has_cctor == 0)
      _cexit ();

  return ret;
}

extern int __mingw_initltsdrot_force;
extern int __mingw_initltsdyn_force;
extern int __mingw_initltssuo_force;

static int duplicate_ppstrings (int ac, _TCHAR ***av)
{
	_TCHAR **avl;
	int i;
	_TCHAR **n = (_TCHAR **) malloc (sizeof (_TCHAR *) * (ac + 1));
	if (!n) return 1;

	avl=*av;
	for (i=0; i < ac; i++)
	  {
		size_t l = sizeof (_TCHAR) * (_tcslen (avl[i]) + 1);
		n[i] = (_TCHAR *) malloc (l);
		if (!n[i]) return 1;
		memcpy (n[i], avl[i], l);
	  }
	n[i] = NULL;
	*av = n;
	return 0;
}

extern "C"
int __cdecl atexit (_PVFV func)
{
    /*
     * msvcrt def file renames the real atexit() function to _crt_atexit().
     * UCRT provides atexit() function only under name _crt_atexit().
     * So redirect call to _crt_atexit() function.
     */
    return _crt_atexit(func);
}

char __mingw_module_is_dll = 0;
