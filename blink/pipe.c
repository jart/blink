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
#include <fcntl.h>
#include <stdatomic.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/endian.h"
#include "blink/fds.h"
#include "blink/syscall.h"

int SysPipe(struct Machine *m, i64 pipefds_addr, i32 flags) {
  int rc;
  int fds[2];
  u8 fds_linux[2][4];
  if (pipe(fds) != -1) {
    LockFds(&m->system->fds);
    unassert(AddFd(&m->system->fds, fds[0], O_RDONLY));
    unassert(AddFd(&m->system->fds, fds[1], O_WRONLY));
    UnlockFds(&m->system->fds);
    Write32(fds_linux[0], fds[0]);
    Write32(fds_linux[1], fds[1]);
    CopyToUserWrite(m, pipefds_addr, fds_linux, sizeof(fds_linux));
    rc = 0;
  } else {
    rc = -1;
  }
  return rc;
}
