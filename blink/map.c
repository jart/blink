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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/debug.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/types.h"
#include "blink/util.h"

long GetSystemPageSize(void) {
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

static const char *DescribeSync(int prot) {
  char *p;
  bool gotsome;
  _Thread_local static char buf[64];
  p = buf;
  gotsome = false;
  if (prot & MS_SYNC) {
    if (gotsome) *p++ = '|';
    p = stpcpy(p, "MS_SYNC");
    prot &= ~MS_SYNC;
    gotsome = true;
  }
  if (prot & MS_ASYNC) {
    if (gotsome) *p++ = '|';
    p = stpcpy(p, "MS_ASYNC");
    prot &= ~MS_ASYNC;
    gotsome = true;
  }
  if (prot & MS_INVALIDATE) {
    if (gotsome) *p++ = '|';
    p = stpcpy(p, "MS_INVALIDATE");
    prot &= ~MS_INVALIDATE;
    gotsome = true;
  }
  if (prot) {
    if (gotsome) *p++ = '|';
    p = FormatInt64(p, prot);
  }
  return buf;
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
#ifdef HAVE_MAP_ANONYMOUS
  res = mmap(addr, length, prot, flags, fd, offset);
  if (res == MAP_FAILED)
    LOGF("wut %p %p %zu, %d, %d, %d, %ld", res, addr, length, prot, flags, fd,
         (long)offset);
#else
  // MAP_ANONYMOUS isn't defined by POSIX.1
  // they do however define the unlink hack
  _Static_assert(!(MAP_ANONYMOUS_ & MAP_FIXED), "");
  _Static_assert(!(MAP_ANONYMOUS_ & MAP_SHARED), "");
  _Static_assert(!(MAP_ANONYMOUS_ & MAP_PRIVATE), "");
  FILE *f;
  if (~flags & MAP_ANONYMOUS_) {
    res = mmap(addr, length, prot, flags, fd, offset);
  } else if ((f = tmpfile())) {
    if (!ftruncate(fileno(f), length)) {
      flags &= ~MAP_ANONYMOUS_;
      res = mmap(addr, length, prot, flags, fileno(f), 0);
    } else {
      res = MAP_FAILED;
    }
    fclose(f);
  } else {
    res = MAP_FAILED;
  }
#endif
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
             flags, DescribeHostErrno(errno), GetSystemPageSize());
  }
#endif
  return res;
}

int Munmap(void *addr, size_t length) {
  int rc;
  rc = munmap(addr, length);
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
  int res = mprotect(addr, length, prot);
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
  int res = msync(addr, length, flags);
#if LOG_MEM
  char szbuf[16];
  FormatSize(szbuf, length, 1024);
  if (res != -1) {
    MEM_LOGF("%s synced %s byte map [%p,%p) as %s", owner, szbuf, addr,
             (u8 *)addr + length, DescribeSync(flags));
  } else {
    MEM_LOGF("%s failed to sync %s byte map [%p,%p) as %s: %s", owner, szbuf,
             (u8 *)addr, (u8 *)addr + length, DescribeSync(flags),
             DescribeHostErrno(errno));
  }
#endif
  return res;
}
