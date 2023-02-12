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
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/errno.h"
#include "blink/log.h"
#include "blink/random.h"
#include "blink/types.h"

#if defined(HAVE_GETENTROPY) || \
    (defined(HAVE_GETRANDOM) && !defined(HAVE_SYS_GETRANDOM))
#include <sys/random.h>
#endif
#ifdef HAVE_SYS_GETRANDOM
#include <sys/syscall.h>
#endif
#ifdef HAVE_KERN_ARND
#include <sys/sysctl.h>
#endif

#ifdef HAVE_SYS_RTLGENRANDOM
#include <w32api/_mingw.h>
bool __stdcall SystemFunction036(void *, __LONG32);
#endif

#ifdef HAVE_DEV_URANDOM
static ssize_t GetDevRandom(char *p, size_t n) {
  int fd;
  ssize_t rc;
  if ((fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC)) == -1) return -1;
  rc = read(fd, p, n);
  close(fd);
  return rc;
}
#endif

#ifdef HAVE_KERN_ARND
static ssize_t GetKernArnd(char *p, size_t n) {
  size_t m, i = 0;
  int cmd[2] = {CTL_KERN, KERN_ARND};
  for (;;) {
    m = n - i;
    if (sysctl(cmd, 2, p + i, &m, 0, 0) != -1) {
      if ((i += m) == n) {
        return n;
      }
    } else {
      return i ? i : -1;
    }
  }
}
#endif

ssize_t GetRandom(void *p, size_t n) {
#ifdef HAVE_SYS_GETRANDOM
  // we need to favor SYS_getrandom right now since our ./configure
  // script isn't smart enough to accommodate our bundled toolchain
  return syscall(SYS_getrandom, p, n, 0);
#elif defined(HAVE_GETRANDOM)
  return getrandom(p, n, 0);
#elif defined(HAVE_GETENTROPY)
  return !getentropy((char *)p, n) ? n : -1;
#elif defined(HAVE_RTLGENRANDOM)
  return SystemFunction036(p, n) ? n : -1;
#elif defined(HAVE_KERN_ARND)
  return GetKernArnd((char *)p, n);
#elif defined(HAVE_DEV_URANDOM)
  return GetDevRandom((char *)p, n);
#else
  LOGF("no suitable entropy source on this platform");
  return enosys();
#endif
}
