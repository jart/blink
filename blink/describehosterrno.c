/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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

#include "blink/util.h"

const char *DescribeHostErrno(int x) {
  _Thread_local static char buf[21];
  if (x == EPERM) return "EPERM";
  if (x == ENOENT) return "ENOENT";
  if (x == ESRCH) return "ESRCH";
  if (x == EINTR) return "EINTR";
  if (x == EIO) return "EIO";
  if (x == ENXIO) return "ENXIO";
  if (x == E2BIG) return "E2BIG";
  if (x == ENOEXEC) return "ENOEXEC";
  if (x == EBADF) return "EBADF";
  if (x == ECHILD) return "ECHILD";
  if (x == EAGAIN) return "EAGAIN";
#if EWOULDBLOCK != EAGAIN
  if (x == EWOULDBLOCK) return "EWOULDBLOCK";
#endif
  if (x == ENOMEM) return "ENOMEM";
  if (x == EACCES) return "EACCES";
  if (x == EFAULT) return "EFAULT";
#ifdef ENOTBLK
  if (x == ENOTBLK) return "ENOTBLK";
#endif
  if (x == EBUSY) return "EBUSY";
  if (x == EEXIST) return "EEXIST";
  if (x == EXDEV) return "EXDEV";
  if (x == ENODEV) return "ENODEV";
  if (x == ENOTDIR) return "ENOTDIR";
  if (x == EISDIR) return "EISDIR";
  if (x == EINVAL) return "EINVAL";
  if (x == ENFILE) return "ENFILE";
  if (x == EMFILE) return "EMFILE";
  if (x == ENOTTY) return "ENOTTY";
  if (x == ETXTBSY) return "ETXTBSY";
  if (x == EFBIG) return "EFBIG";
  if (x == ENOSPC) return "ENOSPC";
  if (x == ESPIPE) return "ESPIPE";
  if (x == EROFS) return "EROFS";
  if (x == EMLINK) return "EMLINK";
  if (x == EPIPE) return "EPIPE";
  if (x == EDOM) return "EDOM";
  if (x == ERANGE) return "ERANGE";
  if (x == EDEADLK) return "EDEADLK";
  if (x == ENAMETOOLONG) return "ENAMETOOLONG";
  if (x == ENOLCK) return "ENOLCK";
  if (x == ENOSYS) return "ENOSYS";
  if (x == ENOTEMPTY) return "ENOTEMPTY";
  if (x == ELOOP) return "ELOOP";
  if (x == ENOMSG) return "ENOMSG";
  if (x == EIDRM) return "EIDRM";
#ifdef EREMOTE
  if (x == EREMOTE) return "EREMOTE";
#endif
  if (x == EPROTO) return "EPROTO";
  if (x == EBADMSG) return "EBADMSG";
  if (x == EOVERFLOW) return "EOVERFLOW";
  if (x == EILSEQ) return "EILSEQ";
#ifdef EUSERS
  if (x == EUSERS) return "EUSERS";
#endif
  if (x == ENOTSOCK) return "ENOTSOCK";
  if (x == EDESTADDRREQ) return "EDESTADDRREQ";
  if (x == EMSGSIZE) return "EMSGSIZE";
  if (x == EPROTOTYPE) return "EPROTOTYPE";
  if (x == ENOPROTOOPT) return "ENOPROTOOPT";
  if (x == EPROTONOSUPPORT) return "EPROTONOSUPPORT";
#ifdef ESOCKTNOSUPPORT
  if (x == ESOCKTNOSUPPORT) return "ESOCKTNOSUPPORT";
#endif
  if (x == ENOTSUP) return "ENOTSUP";
#if EOPNOTSUPP != ENOTSUP
  if (x == EOPNOTSUPP) return "EOPNOTSUPP";
#endif
#ifdef EPFNOSUPPORT
  if (x == EPFNOSUPPORT) return "EPFNOSUPPORT";
#endif
  if (x == EAFNOSUPPORT) return "EAFNOSUPPORT";
  if (x == EADDRINUSE) return "EADDRINUSE";
  if (x == EADDRNOTAVAIL) return "EADDRNOTAVAIL";
  if (x == ENETDOWN) return "ENETDOWN";
  if (x == ENETUNREACH) return "ENETUNREACH";
  if (x == ENETRESET) return "ENETRESET";
  if (x == ECONNABORTED) return "ECONNABORTED";
  if (x == ECONNRESET) return "ECONNRESET";
  if (x == ENOBUFS) return "ENOBUFS";
  if (x == EISCONN) return "EISCONN";
  if (x == ENOTCONN) return "ENOTCONN";
#ifdef ESHUTDOWN
  if (x == ESHUTDOWN) return "ESHUTDOWN";
#endif
#ifdef ETOOMANYREFS
  if (x == ETOOMANYREFS) return "ETOOMANYREFS";
#endif
  if (x == ETIMEDOUT) return "ETIMEDOUT";
  if (x == ECONNREFUSED) return "ECONNREFUSED";
#ifdef EHOSTDOWN
  if (x == EHOSTDOWN) return "EHOSTDOWN";
#endif
  if (x == EHOSTUNREACH) return "EHOSTUNREACH";
  if (x == EALREADY) return "EALREADY";
  if (x == EINPROGRESS) return "EINPROGRESS";
  if (x == ESTALE) return "ESTALE";
  if (x == EDQUOT) return "EDQUOT";
  if (x == ECANCELED) return "ECANCELED";
#ifdef EOWNERDEAD
  if (x == EOWNERDEAD) return "EOWNERDEAD";
#endif
#ifdef ENOTRECOVERABLE
  if (x == ENOTRECOVERABLE) return "ENOTRECOVERABLE";
#endif
#ifdef ETIME
  if (x == ETIME) return "ETIME";
#endif
#ifdef ENONET
  if (x == ENONET) return "ENONET";
#endif
#ifdef ERESTART
  if (x == ERESTART) return "ERESTART";
#endif
#ifdef ENOSR
  if (x == ENOSR) return "ENOSR";
#endif
#ifdef ENOSTR
  if (x == ENOSTR) return "ENOSTR";
#endif
#ifdef ENODATA
  if (x == ENODATA) return "ENODATA";
#endif
#ifdef EMULTIHOP
  if (x == EMULTIHOP) return "EMULTIHOP";
#endif
#ifdef ENOLINK
  if (x == ENOLINK) return "ENOLINK";
#endif
#ifdef ENOMEDIUM
  if (x == ENOMEDIUM) return "ENOMEDIUM";
#endif
#ifdef EMEDIUMTYPE
  if (x == EMEDIUMTYPE) return "EMEDIUMTYPE";
#endif
  FormatInt64(buf, x);
  return buf;
}
