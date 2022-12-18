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
#include <time.h>
#include <unistd.h>
#if defined(__linux)
#include <sys/syscall.h>
#endif
#if defined(__APPLE__)
#include <sys/random.h>
#endif
#if defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__)
#include <sys/param.h>
#endif
#if defined(__FreeBSD__)
#include <sys/sysctl.h>
#include <sys/types.h>
#endif
#include "blink/assert.h"
#include "blink/errno.h"
#include "blink/random.h"
#include "blink/types.h"

static ssize_t GetDevRandom(char *p, size_t n) {
  int fd;
  ssize_t rc;
  if ((fd = open("/dev/urandom", O_RDONLY | O_CLOEXEC)) == -1) return -1;
  rc = read(fd, p, n);
  close(fd);
  return rc;
}

#if (defined(__FreeBSD__) || defined(__NetBSD__)) && defined(KERN_ARND)
static ssize_t GetKernArnd(char *p, size_t n) {
  size_t m, i = 0;
  int cmd[2] = {CTL_KERN, KERN_ARND};
#if defined(__FreeBSD__)
  if (n % sizeof(long)) return enosys();
#endif
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
  ssize_t rc;
#if defined(__linux) && defined(SYS_getrandom)
  rc = syscall(SYS_getrandom, p, n, 0);
#elif defined(__OpenBSD__) || defined(__APPLE__)
  rc = !getentropy((char *)p, n) ? n : -1;
#elif defined(KERN_ARND) &&  \
    (defined(__FreeBSD__) || \
     (defined(__NetBSD__) && __NetBSD_Version__ >= 400000000))
  rc = GetKernArnd((char *)p, n);
#else
  rc = -1;
#endif
  if (rc == -1 && errno == ENOSYS) {
    rc = GetDevRandom((char *)p, n);
  }
  return rc;
}
