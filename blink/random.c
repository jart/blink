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
#include "blink/random.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/errno.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/types.h"
#include "blink/util.h"

#ifdef HAVE_KERN_ARND
#include <sys/sysctl.h>
#endif

#ifdef HAVE_SYS_GETRANDOM
#include <sys/syscall.h>
#endif

#ifdef HAVE_RTLGENRANDOM
#include <w32api/_mingw.h>
bool __stdcall SystemFunction036(void *, __LONG32);
#endif

#if !defined(HAVE_SYS_GETRANDOM) && \
    (defined(HAVE_GETRANDOM) || defined(HAVE_SYS_GETENTROPY))
#include <sys/random.h>
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

static ssize_t GetWeakRandom(char *p, size_t n) {
  u64 word;
  size_t i, j;
  static bool once;
  static u64 state;
  if (!once) {
    LOGF("generating weak random data");
    state = time(0);
    once = true;
  }
  for (i = 0; i < n;) {
    word = Vigna(&state);
    for (j = 0; j < 8 && i < n; ++i, ++j) {
      p[i] = word;
      word >>= 8;
    }
  }
  return n;
}

// "The user of getrandom() must always check the return value, to
// determine whether either an error occurred or fewer bytes than
// requested were returned. In the case where GRND_RANDOM is not
// specified and buflen is less than or equal to 256, a return of fewer
// bytes than requested should never happen, but the careful programmer
// will check for this anyway!" -Quoth the Linux Programmer's Manual §
// getrandom().

ssize_t GetRandom(void *p, size_t n, int flags) {
#ifdef GRND_RANDOM
  _Static_assert(GRND_RANDOM == GRND_RANDOM_LINUX, "");
#endif
#ifdef GRND_NONBLOCK
  _Static_assert(GRND_NONBLOCK == GRND_NONBLOCK_LINUX, "");
#endif
#ifdef HAVE_SYS_GETRANDOM
  // we need to favor SYS_getrandom right now since our ./configure
  // script isn't smart enough to accommodate our bundled toolchain
  return syscall(SYS_getrandom, p, n, flags);
#elif defined(HAVE_GETRANDOM)
  return getrandom(p, n, flags);
#elif defined(HAVE_RTLGENRANDOM)
  return SystemFunction036(p, n) ? n : -1;
#elif defined(HAVE_GETENTROPY) || defined(HAVE_SYS_GETENTROPY)
  return !getentropy((char *)p, MIN(256, n)) ? n : -1;
#elif defined(HAVE_KERN_ARND)
  return GetKernArnd((char *)p, n);
#elif defined(HAVE_DEV_URANDOM)
  return GetDevRandom((char *)p, n);
#else
  return GetWeakRandom((char *)p, n);
#endif
}
