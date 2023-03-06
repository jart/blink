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

#include "blink/builtin.h"
#include "blink/errno.h"

static dontinline long ReturnErrno(int e) {
  errno = e;
  return -1;
}

long ebadf(void) {
  return ReturnErrno(EBADF);
}

long einval(void) {
  return ReturnErrno(EINVAL);
}

long eagain(void) {
  return ReturnErrno(EAGAIN);
}

long enomem(void) {
  return ReturnErrno(ENOMEM);
}

long enosys(void) {
  return ReturnErrno(ENOSYS);
}

long emfile(void) {
  return ReturnErrno(EMFILE);
}

long efault(void) {
  return ReturnErrno(EFAULT);
}

void *efault0(void) {
  efault();
  return 0;
}

long eintr(void) {
  return ReturnErrno(EINTR);
}

long eoverflow(void) {
  return ReturnErrno(EOVERFLOW);
}

long enfile(void) {
  return ReturnErrno(ENFILE);
}

long esrch(void) {
  return ReturnErrno(ESRCH);
}

long eperm(void) {
  return ReturnErrno(EPERM);
}

long enotsup(void) {
  return ReturnErrno(ENOTSUP);
}

long enoent(void) {
  return ReturnErrno(ENOENT);
}

long enotdir(void) {
  return ReturnErrno(ENOTDIR);
}

long erange(void) {
  return ReturnErrno(ERANGE);
}

long eopnotsupp(void) {
  return ReturnErrno(EOPNOTSUPP);
}

long enodev(void) {
  return ReturnErrno(ENODEV);
}

long eacces(void) {
  return ReturnErrno(EACCES);
}

long eisdir(void) {
  return ReturnErrno(EISDIR);
}

long eexist(void) {
  return ReturnErrno(EEXIST);
}

long eloop(void) {
  return ReturnErrno(ELOOP);
}

long exdev(void) {
  return ReturnErrno(EXDEV);
}
