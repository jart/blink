/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/bitscan.h"
#include "blink/bus.h"
#include "blink/debug.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/tunables.h"
#include "blink/types.h"
#include "blink/util.h"
#include "blink/vfs.h"

static long GetSystemPageSize(void) {
#ifdef __EMSCRIPTEN__
  // "pages" in Emscripten only refer to the granularity the memory
  // buffer can be grown at but does not affect functions like mmap
  return 4096;
#else
  long z;
  unassert((z = sysconf(_SC_PAGESIZE)) > 0);
  unassert(IS2POW(z));
  return MAX(4096, z);
#endif
}

static void *PortableMmap(void *addr,     //
                          size_t length,  //
                          int prot,       //
                          int flags,      //
                          int fd,         //
                          off_t offset) {
  void *res;
#ifdef HAVE_MAP_ANONYMOUS
  res = VfsMmap(addr, length, prot, flags, fd, offset);
#else
  // MAP_ANONYMOUS isn't defined by POSIX.1
  // they do however define the unlink hack
  _Static_assert(!(MAP_ANONYMOUS_ & MAP_FIXED), "");
  _Static_assert(!(MAP_ANONYMOUS_ & MAP_SHARED), "");
  _Static_assert(!(MAP_ANONYMOUS_ & MAP_PRIVATE), "");
  int tfd;
  char path[] = "/tmp/blink.dat.XXXXXX";
  if (~flags & MAP_ANONYMOUS_) {
    res = VfsMmap(addr, length, prot, flags, fd, offset);
  } else if ((tfd = mkstemp(path)) != -1) {
    unlink(path);
    if (!ftruncate(tfd, length)) {
      res = mmap(addr, length, prot, flags & ~MAP_ANONYMOUS_, tfd, 0);
    } else {
      res = MAP_FAILED;
    }
    close(tfd);
  } else {
    res = MAP_FAILED;
  }
#endif
#ifdef MAP_FIXED_NOREPLACE
  if ((flags & MAP_FIXED_NOREPLACE) && res == MAP_FAILED &&
      errno == EOPNOTSUPP) {
    res = VfsMmap(addr, length, prot, flags & ~MAP_FIXED_NOREPLACE, fd, offset);
    if (res != addr) {
      VfsMunmap(res, length);
      res = MAP_FAILED;
      errno = EEXIST;
    }
  }
#endif
  return res;
}

static int GetBitsInAddressSpace(void) {
  int i;
  void *ptr;
  uint64_t want;
  for (i = 16; i < 40; ++i) {
    want = UINT64_C(0x8123000000000000) >> i;
    if (want > UINTPTR_MAX) continue;
    ptr = PortableMmap((void *)(uintptr_t)want, 1, PROT_READ,
                       MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS_, -1, 0);
    if (ptr != MAP_FAILED) {
      VfsMunmap(ptr, 1);
      return 64 - i;
    }
  }
  Abort();
}

static u64 GetVirtualAddressSpace(int vabits, long pagesize) {
  u64 vaspace;
  vaspace = (u64)1 << (vabits - 1);  // 0000400000000000
  vaspace |= vaspace - 1;            // 00007fffffffffff
  vaspace &= ~(pagesize - 1);        // 00007ffffffff000
  return vaspace;
}

static u64 ScaleAddress(u64 address) {
  long pagesize;
  u64 result, vaspace;
  vaspace = FLAG_vaspace;
  pagesize = FLAG_pagesize;
  result = address;
  do result &= ~(pagesize - 1);
  while ((result & ~vaspace) && (result >>= 1));
  return result;
}

// if the guest used mmap(0, ...) to let blink decide the address,
// then the goal is to supply mmap(0, ...) to the host kernel too;
// but we can't do that on systems like rasberry pi, since they'll
// assign addresses greater than 2**47 which won't fit with x86_64
void InitMap(void) {
  FLAG_pagesize = GetSystemPageSize();
  FLAG_vabits = GetBitsInAddressSpace();
  FLAG_vaspace = GetVirtualAddressSpace(FLAG_vabits, FLAG_pagesize);
  FLAG_aslrmask = ScaleAddress(kAslrMask);
  FLAG_imagestart = ScaleAddress(kImageStart);
  FLAG_automapstart = ScaleAddress(kAutomapStart);
  FLAG_automapend = ScaleAddress(kAutomapEnd);
  FLAG_dyninterpaddr = ScaleAddress(kDynInterpAddr);
  FLAG_stacktop = ScaleAddress(kStackTop);
}

void *Mmap(void *addr,     //
           size_t length,  //
           int prot,       //
           int flags,      //
           int fd,         //
           off_t offset,   //
           const char *owner) {
  void *res;
#if LOG_MEM
  char szbuf[16];
#endif
  res = PortableMmap(addr, length, prot, flags, fd, offset);
#if LOG_MEM
  FormatSize(szbuf, length, 1024);
  if (res != MAP_FAILED) {
    MEM_LOGF("%s created %s byte %smap [%p,%p) as %s flags=%#x fd=%d "
             "offset=%#" PRIx64,
             owner, szbuf, (flags & MAP_SHARED) ? "shared " : "", res,
             (u8 *)res + length, DescribeProt(prot), flags, fd, (i64)offset);
  } else {
    MEM_LOGF("%s failed to create %s map [%p,%p) as %s flags %#x: %s "
             "(system page size is %ld)",
             owner, szbuf, (u8 *)addr, (u8 *)addr + length, DescribeProt(prot),
             flags, DescribeHostErrno(errno), FLAG_pagesize);
  }
#endif
  return res;
}

int Munmap(void *addr, size_t length) {
  int rc;
  rc = VfsMunmap(addr, length);
#if LOG_MEM
  char szbuf[16];
  FormatSize(szbuf, length, 1024);
  if (!rc) {
    MEM_LOGF("unmapped %s bytes at [%p,%p)", szbuf, addr, (u8 *)addr + length);
  } else {
    MEM_LOGF("failed to unmap %s bytes at [%p,%p): %s", szbuf, addr,
             (u8 *)addr + length, DescribeHostErrno(errno));
  }
#endif
  return rc;
}

int Mprotect(void *addr,     //
             size_t length,  //
             int prot,       //
             const char *owner) {
  int res = VfsMprotect(addr, length, prot);
#if LOG_MEM
  char szbuf[16];
  FormatSize(szbuf, length, 1024);
  if (res != -1) {
    MEM_LOGF("%s protected %s byte map [%p,%p) as %s", owner, szbuf, addr,
             (u8 *)addr + length, DescribeProt(prot));
  } else {
    MEM_LOGF("%s failed to protect %s byte map [%p,%p) as %s: %s", owner, szbuf,
             (u8 *)addr, (u8 *)addr + length, DescribeProt(prot),
             DescribeHostErrno(errno));
  }
#endif
  return res;
}

int Msync(void *addr,     //
          size_t length,  //
          int flags,      //
          const char *owner) {
  int res = VfsMsync(addr, length, flags);
#if LOG_MEM
  char szbuf[16];
  FormatSize(szbuf, length, 1024);
  if (res != -1) {
    MEM_LOGF("%s synced %s byte map [%p,%p) as %#x", owner, szbuf, addr,
             (u8 *)addr + length, flags);
  } else {
    MEM_LOGF("%s failed to sync %s byte map [%p,%p) as %#x: %s", owner, szbuf,
             (u8 *)addr, (u8 *)addr + length, flags, DescribeHostErrno(errno));
  }
#endif
  return res;
}
