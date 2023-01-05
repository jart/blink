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
#include "blink/jitdump.h"
#if SUPPORT_JITDUMP

#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/timespec.h"
#include "blink/util.h"

#ifdef __COSMOPOLITAN__
#define GETTID() gettid()
#else
#define GETTID() syscall(__NR_gettid)
#endif

static struct JitDump {
  int pid;
  atomic_uint gen;
  atomic_uint ids;
  struct timespec started;
} g_jitdump;

static u64 GetJitDumpTimestamp(void) {
  return ToNanoseconds(SubtractTime(GetMonotonic(), g_jitdump.started));
}

int OpenJitDump(void) {
  int fd;
  int gen;
  char path[40], *p;
  struct JitDumpHeader h;
  gen = atomic_fetch_add_explicit(&g_jitdump.gen, 1, memory_order_relaxed);
  if (!gen) {
    g_jitdump.pid = getpid();
    g_jitdump.started = GetMonotonic();
  }
  h.magic = JITDUMP_MAGIC;
  h.version = JITDUMP_VERSION;
  h.total_size = sizeof(h);
  h.elf_mach = 0;
  h.pad1 = 0;
  h.pid = g_jitdump.pid;
  h.timestamp = GetJitDumpTimestamp();
  h.flags = 0;
  p = path;
  p = stpcpy(p, "jit-");
  p = FormatInt64(p, h.pid);
  p = stpcpy(p, ".dump");
  if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644)) != -1) {
    unassert(write(fd, &h, sizeof(h)) == sizeof(h));
  }
  return fd;
}

int LoadJitDump(int fd, const u8 *code, size_t size) {
  u32 id;
  size_t namelen;
  char name[32], *p;
  struct iovec iov[3];
  struct JitDumpLoad r;
  static _Thread_local int tid;
  if (fd < 0) return -1;
  id = atomic_fetch_add_explicit(&g_jitdump.ids, 1, memory_order_relaxed);
  p = name;
  p = stpcpy(p, "JIT#");
  p = FormatInt64(p, id);
  namelen = p - name;
  if (!tid) unassert((tid = GETTID()) > 0);
  r.p.id = JIT_CODE_CLOSE;
  r.p.total_size = sizeof(r) + namelen + 1 + size;
  r.p.timestamp = GetJitDumpTimestamp();
  r.pid = g_jitdump.pid;
  r.tid = tid;
  r.vma = (intptr_t)code;
  r.code_addr = (intptr_t)code;
  r.code_size = size;
  r.code_index = id;
  iov[0].iov_base = &r;
  iov[0].iov_len = sizeof(r);
  iov[1].iov_base = name;
  iov[1].iov_len = namelen + 1;
  iov[2].iov_base = (void *)code;
  iov[2].iov_len = size;
  unassert(writev(fd, iov, 3) == r.p.total_size);
  return 0;
}

int CloseJitDump(int fd) {
  struct JitDumpClose r;
  if (fd < 0) return -1;
  r.p.id = JIT_CODE_CLOSE;
  r.p.total_size = sizeof(r);
  r.p.timestamp = GetJitDumpTimestamp();
  unassert(write(fd, &r, sizeof(r)) == sizeof(r));
  unassert(!close(fd));
  return 0;
}

#endif
