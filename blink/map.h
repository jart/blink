#ifndef BLINK_MAP_H_
#define BLINK_MAP_H_
#include <errno.h>
#include <sys/mman.h>

#include "blink/builtin.h"
#include "blink/thread.h"
#include "blink/types.h"

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

#if defined(__APPLE__) && defined(__aarch64__)
#include <libkern/OSCacheControl.h>
#else
#define pthread_jit_write_protect_supported_np() 0
#define pthread_jit_write_protect_np(x)          (void)(x)
#define sys_icache_invalidate(addr, size) \
  __builtin___clear_cache((char *)(addr), (char *)(addr) + (size));
#endif

#ifdef HAVE_MAP_ANONYMOUS
#define MAP_ANONYMOUS_ MAP_ANONYMOUS
#else
#define MAP_ANONYMOUS_ 0x10000000
#endif

// MAP_DEMAND means use MAP_FIXED only if it won't clobber other maps
#if defined(MAP_FIXED_NOREPLACE) && !defined(__SANITIZE_THREAD__)
// The mmap() address parameter without MAP_FIXED is documented by
// Linux as a hint for locality. However our testing indicates the
// kernel is still likely to assign addresses that're outrageously
// far away from what was requested. So we're just going to choose
// something that's past the program break, and hope for the best.
#define MAP_DEMAND MAP_FIXED_NOREPLACE
#define MAP_DENIED EEXIST
#elif defined(MAP_EXCL)
// FreeBSD also supports this feature but it uses a clumsier errno
#define MAP_DEMAND (MAP_FIXED | MAP_EXCL)
#define MAP_DENIED EINVAL
#else
// Otherwise detect overlap when mmap() returns an unexpected address
#define MAP_DEMAND 0
#define MAP_DENIED 0
#endif

// from the xnu codebase
#define _COMM_PAGE_START_ADDRESS      (0x0000000FFFFFC000ULL)
#define _COMM_PAGE_APRR_SUPPORT       (_COMM_PAGE_START_ADDRESS + 0x10C)
#define _COMM_PAGE_APRR_WRITE_ENABLE  (_COMM_PAGE_START_ADDRESS + 0x110)
#define _COMM_PAGE_APRR_WRITE_DISABLE (_COMM_PAGE_START_ADDRESS + 0x118)

void InitMap(void);
int Munmap(void *, size_t);
int Msync(void *, size_t, int, const char *);
void *Mmap(void *, size_t, int, int, int, off_t, const char *);
int Mprotect(void *, size_t, int, const char *);
void OverridePageSize(long);

#endif /* BLINK_MAP_H_ */
