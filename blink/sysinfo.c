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
#include <string.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/types.h>

#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/linux.h"
#include "blink/macros.h"
#include "blink/timespec.h"
#include "blink/types.h"

#ifdef HAVE_SYSINFO
#include <sys/sysinfo.h>
#endif
#ifdef HAVE_SYSCTL
#include <sys/sysctl.h>
#endif

#ifdef HAVE_SYSCTL

static i64 GetUptime(void) {
  struct timeval x;
  size_t n = sizeof(x);
  int mib[] = {CTL_KERN, KERN_BOOTTIME};
  if (sysctl(mib, ARRAYLEN(mib), &x, &n, 0, 0) == -1) return 0;
  return GetTime().tv_sec - x.tv_sec;
}

static i64 GetPhysmem(void) {
  u64 x = 0;
  size_t n = sizeof(x);
  int mib[] = {
      CTL_HW,
#ifdef HW_MEMSIZE
      HW_MEMSIZE,  // u64 version of HW_PHYSMEM on Apple platforms
#else
      HW_PHYSMEM,
#endif
  };
  if (sysctl(mib, ARRAYLEN(mib), &x, &n, 0, 0) == -1) return 0;
  return x;
}

#endif /* HAVE_SYSCTL */

int sysinfo_linux(struct sysinfo_linux *si) {
  memset(si, 0, sizeof(*si));
#ifdef HAVE_SYSINFO
  struct sysinfo syssi;
  if (sysinfo(&syssi) == -1) return -1;
  Write64(si->uptime, syssi.uptime);
  Write64(si->loads[0], syssi.loads[0]);
  Write64(si->loads[1], syssi.loads[1]);
  Write64(si->loads[2], syssi.loads[2]);
  Write64(si->totalram, syssi.totalram);
  Write64(si->freeram, syssi.freeram);
  Write64(si->sharedram, syssi.sharedram);
  Write64(si->bufferram, syssi.bufferram);
  Write64(si->totalswap, syssi.totalswap);
  Write64(si->freeswap, syssi.freeswap);
  Write16(si->procs, syssi.procs);
  Write64(si->totalhigh, syssi.totalhigh);
  Write64(si->freehigh, syssi.freehigh);
  Write32(si->mem_unit, syssi.mem_unit);
#elif defined(HAVE_SYSCTL)
  Write64(si->uptime, GetUptime());
  Write64(si->totalram, GetPhysmem());
  Write16(si->procs, 1);
  Write32(si->mem_unit, 1);
#else
  Write64(si->totalram, 1 * 1024 * 1024 * 1024);
  Write16(si->procs, 1);
  Write32(si->mem_unit, 1);
#endif
  return 0;
}
