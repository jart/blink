#ifndef BLINK_MAP_H_
#define BLINK_MAP_H_
#include <errno.h>
#include <sys/mman.h>

#ifndef MAP_NORESERVE
#define MAP_NORESERVE 0
#endif

#ifndef MAP_ANONYMOUS
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#define MAP_ANONYMOUS 0x00001000
#else
#error "MAP_ANONYMOUS isn't defined by your platform"
#endif
#endif

#ifdef MAP_FIXED_NOREPLACE
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
#define MAP_DENIED 0x100000000  // will prune errno compare branch
#endif

void *Mmap(void *, size_t, int, int, int, off_t, const char *);

#endif /* BLINK_MAP_H_ */
