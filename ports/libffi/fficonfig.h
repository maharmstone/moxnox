#pragma once

#define STDC_HEADERS 1
#define HAVE_ALLOCA_H 1
#define HAVE_MEMCPY 1
#define HAVE_MKOSTEMP 1
#define HAVE_GETPAGESIZE 1
#define HAVE_MMAP 1

#define HAVE_AS_CFI_PSEUDO_OP 1

#define FFI_MMAP_EXEC_WRIT 1

#ifdef LIBFFI_ASM
#define FFI_HIDDEN(name)
#else
#define FFI_HIDDEN
#endif
