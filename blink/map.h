#ifndef BLINK_MAP_H_
#define BLINK_MAP_H_
#include <errno.h>
#include <pthread.h>
#include <sys/mman.h>

#include "blink/builtin.h"

#ifndef MAP_32BIT
#define MAP_32BIT 0
#endif

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

#ifndef MAP_JIT
#define MAP_JIT 0
#endif

#if defined(__APPLE__) && defined(__aarch64__)
#include <libkern/OSCacheControl.h>
#else
#define pthread_jit_write_protect_supported_np() 0
#define pthread_jit_write_protect_np(x)          (void)(x)
#define sys_icache_invalidate(addr, size) \
  __builtin___clear_cache((char *)(addr), (char *)(addr) + (size));
#endif

#ifndef MAP_ANONYMOUS
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define MAP_ANONYMOUS 0x00001000
#else
#error "MAP_ANONYMOUS isn't defined by your platform"
#endif
#endif

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
#define MAP_DEMAND 0
#define MAP_DENIED 0
#endif

void *Mmap(void *, size_t, int, int, int, off_t, const char *);
int Mprotect(void *, size_t, int, const char *);
long GetSystemPageSize(void) pureconst;

#endif /* BLINK_MAP_H_ */
