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
#include <limits.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <signal.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "blink/builtin.h"
#include "blink/case.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/map.h"
#include "blink/sigwinch.h"
#include "blink/xlat.h"

int XlatErrno(int x) {
  if (x == EPERM) return EPERM_LINUX;
  if (x == ENOENT) return ENOENT_LINUX;
  if (x == ESRCH) return ESRCH_LINUX;
  if (x == EINTR) return EINTR_LINUX;
  if (x == EIO) return EIO_LINUX;
  if (x == ENXIO) return ENXIO_LINUX;
  if (x == E2BIG) return E2BIG_LINUX;
  if (x == ENOEXEC) return ENOEXEC_LINUX;
  if (x == EBADF) return EBADF_LINUX;
  if (x == ECHILD) return ECHILD_LINUX;
  if (x == EAGAIN) return EAGAIN_LINUX;
#if EWOULDBLOCK != EAGAIN
  if (x == EWOULDBLOCK) return EWOULDBLOCK_LINUX;
#endif
  if (x == ENOMEM) return ENOMEM_LINUX;
  if (x == EACCES) return EACCES_LINUX;
  if (x == EFAULT) return EFAULT_LINUX;
#ifdef ENOTBLK
  if (x == ENOTBLK) return ENOTBLK_LINUX;
#endif
  if (x == EBUSY) return EBUSY_LINUX;
  if (x == EEXIST) return EEXIST_LINUX;
  if (x == EXDEV) return EXDEV_LINUX;
  if (x == ENODEV) return ENODEV_LINUX;
  if (x == ENOTDIR) return ENOTDIR_LINUX;
  if (x == EISDIR) return EISDIR_LINUX;
  if (x == EINVAL) return EINVAL_LINUX;
  if (x == ENFILE) return ENFILE_LINUX;
  if (x == EMFILE) return EMFILE_LINUX;
  if (x == ENOTTY) return ENOTTY_LINUX;
  if (x == ETXTBSY) return ETXTBSY_LINUX;
  if (x == EFBIG) return EFBIG_LINUX;
  if (x == ENOSPC) return ENOSPC_LINUX;
  if (x == ESPIPE) return ESPIPE_LINUX;
  if (x == EROFS) return EROFS_LINUX;
  if (x == EMLINK) return EMLINK_LINUX;
  if (x == EPIPE) return EPIPE_LINUX;
  if (x == EDOM) return EDOM_LINUX;
  if (x == ERANGE) return ERANGE_LINUX;
  if (x == EDEADLK) return EDEADLK_LINUX;
  if (x == ENAMETOOLONG) return ENAMETOOLONG_LINUX;
  if (x == ENOLCK) return ENOLCK_LINUX;
  if (x == ENOSYS) return ENOSYS_LINUX;
  if (x == ENOTEMPTY) return ENOTEMPTY_LINUX;
  if (x == ELOOP) return ELOOP_LINUX;
  if (x == ENOMSG) return ENOMSG_LINUX;
  if (x == EIDRM) return EIDRM_LINUX;
#ifdef EREMOTE
  if (x == EREMOTE) return EREMOTE_LINUX;
#endif
  if (x == EPROTO) return EPROTO_LINUX;
  if (x == EBADMSG) return EBADMSG_LINUX;
  if (x == EOVERFLOW) return EOVERFLOW_LINUX;
  if (x == EILSEQ) return EILSEQ_LINUX;
#ifdef EUSERS
  if (x == EUSERS) return EUSERS_LINUX;
#endif
  if (x == ENOTSOCK) return ENOTSOCK_LINUX;
  if (x == EDESTADDRREQ) return EDESTADDRREQ_LINUX;
  if (x == EMSGSIZE) return EMSGSIZE_LINUX;
  if (x == EPROTOTYPE) return EPROTOTYPE_LINUX;
  if (x == ENOPROTOOPT) return ENOPROTOOPT_LINUX;
  if (x == EPROTONOSUPPORT) return EPROTONOSUPPORT_LINUX;
#ifdef ESOCKTNOSUPPORT
  if (x == ESOCKTNOSUPPORT) return ESOCKTNOSUPPORT_LINUX;
#endif
  if (x == ENOTSUP) return ENOTSUP_LINUX;
#if EOPNOTSUPP != ENOTSUP
  if (x == EOPNOTSUPP) return EOPNOTSUPP_LINUX;
#endif
#ifdef EPFNOSUPPORT
  if (x == EPFNOSUPPORT) return EPFNOSUPPORT_LINUX;
#endif
  if (x == EAFNOSUPPORT) return EAFNOSUPPORT_LINUX;
  if (x == EADDRINUSE) return EADDRINUSE_LINUX;
  if (x == EADDRNOTAVAIL) return EADDRNOTAVAIL_LINUX;
  if (x == ENETDOWN) return ENETDOWN_LINUX;
  if (x == ENETUNREACH) return ENETUNREACH_LINUX;
  if (x == ENETRESET) return ENETRESET_LINUX;
  if (x == ECONNABORTED) return ECONNABORTED_LINUX;
  if (x == ECONNRESET) return ECONNRESET_LINUX;
  if (x == ENOBUFS) return ENOBUFS_LINUX;
  if (x == EISCONN) return EISCONN_LINUX;
  if (x == ENOTCONN) return ENOTCONN_LINUX;
#ifdef ESHUTDOWN
  if (x == ESHUTDOWN) return ESHUTDOWN_LINUX;
#endif
#ifdef ETOOMANYREFS
  if (x == ETOOMANYREFS) return ETOOMANYREFS_LINUX;
#endif
  if (x == ETIMEDOUT) return ETIMEDOUT_LINUX;
  if (x == ECONNREFUSED) return ECONNREFUSED_LINUX;
#ifdef EHOSTDOWN
  if (x == EHOSTDOWN) return EHOSTDOWN_LINUX;
#endif
  if (x == EHOSTUNREACH) return EHOSTUNREACH_LINUX;
  if (x == EALREADY) return EALREADY_LINUX;
  if (x == EINPROGRESS) return EINPROGRESS_LINUX;
  if (x == ESTALE) return ESTALE_LINUX;
  if (x == EDQUOT) return EDQUOT_LINUX;
  if (x == ECANCELED) return ECANCELED_LINUX;
  if (x == EOWNERDEAD) return EOWNERDEAD_LINUX;
  if (x == ENOTRECOVERABLE) return ENOTRECOVERABLE_LINUX;
#ifdef ETIME
  if (x == ETIME) return ETIME_LINUX;
#endif
#ifdef ENONET
  if (x == ENONET) return ENONET_LINUX;
#endif
#ifdef ERESTART
  if (x == ERESTART) return ERESTART_LINUX;
#endif
#ifdef ENOSR
  if (x == ENOSR) return ENOSR_LINUX;
#endif
#ifdef ENOSTR
  if (x == ENOSTR) return ENOSTR_LINUX;
#endif
#ifdef ENODATA
  if (x == ENODATA) return ENODATA_LINUX;
#endif
#ifdef EMULTIHOP
  if (x == EMULTIHOP) return EMULTIHOP_LINUX;
#endif
#ifdef ENOLINK
  if (x == ENOLINK) return ENOLINK_LINUX;
#endif
#ifdef ENOMEDIUM
  if (x == ENOMEDIUM) return ENOMEDIUM_LINUX;
#endif
#ifdef EMEDIUMTYPE
  if (x == EMEDIUMTYPE) return EMEDIUMTYPE_LINUX;
#endif
  return x;
}

int XlatSignal(int x) {
  switch (x) {
    XLAT(SIGHUP_LINUX, SIGHUP);
    XLAT(SIGINT_LINUX, SIGINT);
    XLAT(SIGQUIT_LINUX, SIGQUIT);
    XLAT(SIGILL_LINUX, SIGILL);
    XLAT(SIGTRAP_LINUX, SIGTRAP);
    XLAT(SIGABRT_LINUX, SIGABRT);
    XLAT(SIGBUS_LINUX, SIGBUS);
    XLAT(SIGFPE_LINUX, SIGFPE);
    XLAT(SIGKILL_LINUX, SIGKILL);
    XLAT(SIGUSR1_LINUX, SIGUSR1);
    XLAT(SIGSEGV_LINUX, SIGSEGV);
    XLAT(SIGUSR2_LINUX, SIGUSR2);
    XLAT(SIGPIPE_LINUX, SIGPIPE);
    XLAT(SIGALRM_LINUX, SIGALRM);
    XLAT(SIGTERM_LINUX, SIGTERM);
#ifdef SIGSTKFLT
    XLAT(SIGSTKFLT_LINUX, SIGSTKFLT);
#endif
    XLAT(SIGCHLD_LINUX, SIGCHLD);
    XLAT(SIGCONT_LINUX, SIGCONT);
    XLAT(SIGSTOP_LINUX, SIGSTOP);
    XLAT(SIGTSTP_LINUX, SIGTSTP);
    XLAT(SIGTTIN_LINUX, SIGTTIN);
    XLAT(SIGTTOU_LINUX, SIGTTOU);
    XLAT(SIGURG_LINUX, SIGURG);
    XLAT(SIGXCPU_LINUX, SIGXCPU);
    XLAT(SIGXFSZ_LINUX, SIGXFSZ);
    XLAT(SIGVTALRM_LINUX, SIGVTALRM);
    XLAT(SIGPROF_LINUX, SIGPROF);
    XLAT(SIGWINCH_LINUX, SIGWINCH);
#ifdef SIGIO
    XLAT(SIGIO_LINUX, SIGIO);
#endif
    XLAT(SIGSYS_LINUX, SIGSYS);
    default:
      return einval();
  }
}

int XlatResource(int x) {
  switch (x) {
    XLAT(0, RLIMIT_CPU);
    XLAT(1, RLIMIT_FSIZE);
    XLAT(2, RLIMIT_DATA);
    XLAT(3, RLIMIT_STACK);
    XLAT(4, RLIMIT_CORE);
    XLAT(5, RLIMIT_RSS);
    XLAT(6, RLIMIT_NPROC);
    XLAT(7, RLIMIT_NOFILE);
    XLAT(8, RLIMIT_MEMLOCK);
#ifdef RLIMIT_AS
    XLAT(9, RLIMIT_AS);
#endif
#ifdef RLIMIT_LOCKS
    XLAT(10, RLIMIT_LOCKS);
#endif
#ifdef RLIMIT_SIGPENDING
    XLAT(11, RLIMIT_SIGPENDING);
#endif
#ifdef RLIMIT_MSGQUEUE
    XLAT(12, RLIMIT_MSGQUEUE);
#endif
#ifdef RLIMIT_NICE
    XLAT(13, RLIMIT_NICE);
#endif
#ifdef RLIMIT_RTPRIO
    XLAT(14, RLIMIT_RTPRIO);
#endif
#ifdef RLIMIT_RTTIME
    XLAT(15, RLIMIT_RTTIME);
#endif
    default:
      LOGF("rlimit %d not supported yet", x);
      return einval();
  }
}

int UnXlatSignal(int x) {
  if (x == SIGHUP) return SIGHUP_LINUX;
  if (x == SIGINT) return SIGINT_LINUX;
  if (x == SIGQUIT) return SIGQUIT_LINUX;
  if (x == SIGILL) return SIGILL_LINUX;
  if (x == SIGTRAP) return SIGTRAP_LINUX;
  if (x == SIGABRT) return SIGABRT_LINUX;
  if (x == SIGBUS) return SIGBUS_LINUX;
  if (x == SIGFPE) return SIGFPE_LINUX;
  if (x == SIGKILL) return SIGKILL_LINUX;
  if (x == SIGUSR1) return SIGUSR1_LINUX;
  if (x == SIGSEGV) return SIGSEGV_LINUX;
  if (x == SIGUSR2) return SIGUSR2_LINUX;
  if (x == SIGPIPE) return SIGPIPE_LINUX;
  if (x == SIGALRM) return SIGALRM_LINUX;
  if (x == SIGTERM) return SIGTERM_LINUX;
  if (x == SIGCHLD) return SIGCHLD_LINUX;
  if (x == SIGCONT) return SIGCONT_LINUX;
  if (x == SIGTTIN) return SIGTTIN_LINUX;
  if (x == SIGTTOU) return SIGTTOU_LINUX;
  if (x == SIGXCPU) return SIGXCPU_LINUX;
  if (x == SIGXFSZ) return SIGXFSZ_LINUX;
  if (x == SIGVTALRM) return SIGVTALRM_LINUX;
  if (x == SIGPROF) return SIGPROF_LINUX;
  if (x == SIGWINCH) return SIGWINCH_LINUX;
#ifdef SIGIO
  if (x == SIGIO) return SIGIO_LINUX;
#endif
#ifdef SIGSTKFLT
  if (x == SIGSTKFLT) return SIGSTKFLT_LINUX;
#endif
  if (x == SIGSTOP) return SIGSTOP_LINUX;
  if (x == SIGSYS) return SIGSYS_LINUX;
  if (x == SIGTSTP) return SIGTSTP_LINUX;
  if (x == SIGURG) return SIGURG_LINUX;
  return einval();
}

int XlatRusage(int x) {
  switch (x) {
    XLAT(0, RUSAGE_SELF);
    XLAT(-1, RUSAGE_CHILDREN);
    default:
      LOGF("%s %d not supported yet", "rusage", x);
      return einval();
  }
}

int XlatSocketFamily(int x) {
  switch (x) {
    XLAT(AF_UNSPEC_LINUX, AF_UNSPEC);
    XLAT(AF_UNIX_LINUX, AF_UNIX);
    XLAT(AF_INET_LINUX, AF_INET);
    default:
      LOGF("%s %d not supported yet", "socket family", x);
      errno = ENOPROTOOPT;
      return -1;
  }
}

int UnXlatSocketFamily(int x) {
  if (x == AF_UNSPEC) return AF_UNSPEC_LINUX;
  if (x == AF_UNIX) return AF_UNIX_LINUX;
  if (x == AF_INET) return AF_INET_LINUX;
  LOGF("don't know how to translate %s %d", "socket family", x);
  return x;
}

int XlatSocketType(int x) {
  switch (x) {
    XLAT(1, SOCK_STREAM);
    XLAT(2, SOCK_DGRAM);
    default:
      LOGF("%s %d not supported yet", "socket type", x);
      return einval();
  }
}

int XlatSocketProtocol(int x) {
  switch (x) {
    XLAT(0, 0);
    XLAT(6, IPPROTO_TCP);
    XLAT(17, IPPROTO_UDP);
    default:
      LOGF("%s %d not supported yet", "socket protocol", x);
      return einval();
  }
}

int XlatSocketLevel(int x) {
  switch (x) {
    XLAT(1, SOL_SOCKET);
    XLAT(6, IPPROTO_TCP);
    XLAT(17, IPPROTO_UDP);
    default:
      LOGF("%s %d not supported yet", "socket level", x);
      return einval();
  }
}

int XlatSocketOptname(int level, int optname) {
  switch (level) {
    case SOL_SOCKET_LINUX:
      switch (optname) {
        XLAT(2, SO_REUSEADDR);
        XLAT(5, SO_DONTROUTE);
        XLAT(7, SO_SNDBUF);
        XLAT(8, SO_RCVBUF);
        XLAT(9, SO_KEEPALIVE);
        XLAT(15, SO_REUSEPORT);
        XLAT(20, SO_RCVTIMEO);
        XLAT(21, SO_SNDTIMEO);
#ifdef SO_RCVLOWAT
        XLAT(18, SO_RCVLOWAT);
#endif
#ifdef SO_SNDLOWAT
        XLAT(19, SO_SNDLOWAT);
#endif
        default:
          break;
      }
    case SOL_TCP_LINUX:
      switch (optname) {
        XLAT(1, TCP_NODELAY);
#ifdef TCP_MAXSEG
        XLAT(2, TCP_MAXSEG);
#endif
#if defined(TCP_CORK)
        XLAT(3, TCP_CORK);
#elif defined(TCP_NOPUSH)
        XLAT(3, TCP_NOPUSH);
#endif
#ifdef TCP_KEEPIDLE
        XLAT(4, TCP_KEEPIDLE);
#endif
#ifdef TCP_KEEPINTVL
        XLAT(5, TCP_KEEPINTVL);
#endif
#ifdef TCP_KEEPCNT
        XLAT(6, TCP_KEEPCNT);
#endif
#ifdef TCP_SYNCNT
        XLAT(7, TCP_SYNCNT);
#endif
#ifdef TCP_DEFER_ACCEPT
        XLAT(9, TCP_DEFER_ACCEPT);
#endif
#ifdef TCP_WINDOW_CLAMP
        XLAT(10, TCP_WINDOW_CLAMP);
#endif
#ifdef TCP_FASTOPEN
        XLAT(23, TCP_FASTOPEN);
#endif
#ifdef TCP_NOTSENT_LOWAT
        XLAT(25, TCP_NOTSENT_LOWAT);
#endif
#ifdef TCP_FASTOPEN_CONNECT
        XLAT(30, TCP_FASTOPEN_CONNECT);
#endif
#ifdef TCP_QUICKACK
        XLAT(12, TCP_QUICKACK);
#endif
#ifdef TCP_SAVE_SYN
        XLAT(27, TCP_SAVE_SYN);
#endif
        default:
          break;
      }
    default:
      break;
  }
  LOGF("socket level %d optname %d not supported yet", level, optname);
  return einval();
}

int XlatAccess(int x) {
  int r = 0;
  if (x == F_OK_LINUX) return F_OK;
  if (x & X_OK_LINUX) r |= X_OK, x &= ~X_OK_LINUX;
  if (x & W_OK_LINUX) r |= W_OK, x &= ~W_OK_LINUX;
  if (x & R_OK_LINUX) r |= R_OK, x &= ~R_OK_LINUX;
  if (x) return einval();
  return r;
}

int XlatShutdown(int x) {
  if (x == SHUT_RD_LINUX) return SHUT_RD;
  if (x == SHUT_WR_LINUX) return SHUT_WR;
  if (x == SHUT_RDWR_LINUX) return SHUT_RDWR;
  return einval();
}

int XlatLock(int x) {
  int r = 0;
  if (x & 1) r |= LOCK_SH;
  if (x & 2) r |= LOCK_EX;
  if (x & 4) r |= LOCK_NB;
  if (x & 8) r |= LOCK_UN;
  return r;
}

int XlatWait(int x) {
  int r = 0;
  if (x & 1) r |= WNOHANG, x &= ~1;
  if (x & 2) r |= WUNTRACED, x &= ~2;
  if (x & 8) r |= WCONTINUED, x &= ~8;
  if (x) LOGF("%s %d not supported yet", "wait", x);
  return r;
}

int XlatClock(int x) {
  switch (x) {
    XLAT(0, CLOCK_REALTIME);
    XLAT(1, CLOCK_MONOTONIC);
    XLAT(2, CLOCK_PROCESS_CPUTIME_ID);
    XLAT(3, CLOCK_THREAD_CPUTIME_ID);
#ifdef CLOCK_REALTIME_COARSE
    XLAT(5, CLOCK_REALTIME_COARSE);
#elif defined(CLOCK_REALTIME_FAST)
    XLAT(5, CLOCK_REALTIME_FAST);
#endif
#ifdef CLOCK_MONOTONIC_COARSE
    XLAT(6, CLOCK_MONOTONIC_COARSE);
#elif defined(CLOCK_REALTIME_FAST)
    XLAT(6, CLOCK_MONOTONIC_FAST);
#endif
#ifdef CLOCK_MONOTONIC_RAW
    XLAT(4, CLOCK_MONOTONIC_RAW);
#else
    XLAT(4, CLOCK_MONOTONIC);
#endif
#ifdef CLOCK_BOOTTIME
    XLAT(7, CLOCK_BOOTTIME);
#endif
#ifdef CLOCK_TAI
    XLAT(11, CLOCK_TAI);
#endif
    default:
      LOGF("%s %d not supported yet", "clock", x);
      return einval();
  }
}

int XlatAtf(int x) {
  int res = 0;
  if (x & 0x0100) res |= AT_SYMLINK_NOFOLLOW, x &= ~0x0100;
  if (x & 0x0200) res |= AT_REMOVEDIR, x &= ~0x0200;
  if (x & 0x0400) res |= AT_SYMLINK_FOLLOW, x &= ~0x0400;
  if (x) {
    LOGF("%s %d not supported yet", "atf", x);
    return einval();
  }
  return res;
}

int XlatAccMode(int x) {
  switch (x & O_ACCMODE_LINUX) {
    case O_RDONLY_LINUX:
      return O_RDONLY;
    case O_WRONLY_LINUX:
      return O_WRONLY;
    case O_RDWR_LINUX:
      return O_RDWR;
    default:
      return einval();
  }
}

int UnXlatAccMode(int flags) {
  switch (flags & O_ACCMODE) {
    case O_RDONLY:
      return O_RDONLY_LINUX;
    case O_WRONLY:
      return O_WRONLY_LINUX;
    case O_RDWR:
      return O_RDWR_LINUX;
    default:
      return einval();
  }
}

int XlatOpenFlags(int x) {
  int res;
  res = XlatAccMode(x);
  x &= ~O_ACCMODE_LINUX;
  if (x & O_APPEND_LINUX) res |= O_APPEND, x &= ~O_APPEND_LINUX;
  if (x & O_CREAT_LINUX) res |= O_CREAT, x &= ~O_CREAT_LINUX;
  if (x & O_EXCL_LINUX) res |= O_EXCL, x &= ~O_EXCL_LINUX;
  if (x & O_TRUNC_LINUX) res |= O_TRUNC, x &= ~O_TRUNC_LINUX;
#ifdef O_PATH
  if (x & O_PATH_LINUX) res |= O_PATH, x &= ~O_PATH_LINUX;
#elif defined(O_EXEC)
  if (x & O_PATH_LINUX) res |= O_EXEC, x &= ~O_PATH_LINUX;
#else
  x &= ~O_PATH_LINUX;
#endif
#ifdef O_LARGEFILE
  if (x & O_LARGEFILE_LINUX) res |= O_LARGEFILE, x &= ~O_LARGEFILE_LINUX;
#else
  x &= ~O_LARGEFILE_LINUX;
#endif
#ifdef O_NDELAY
  if (x & O_NDELAY_LINUX) res |= O_NDELAY, x &= ~O_NDELAY_LINUX;
#endif
#ifdef O_DIRECT
  if (x & O_DIRECT_LINUX) res |= O_DIRECT, x &= ~O_DIRECT_LINUX;
#endif
  if (x & O_DIRECTORY_LINUX) {
    res |= O_DIRECTORY;
    x &= ~O_DIRECTORY_LINUX;
  }
#ifdef O_NOFOLLOW
  if (x & O_NOFOLLOW_LINUX) res |= O_NOFOLLOW, x &= ~O_NOFOLLOW_LINUX;
#endif
  if (x & O_CLOEXEC_LINUX) res |= O_CLOEXEC, x &= ~O_CLOEXEC_LINUX;
  if (x & O_NOCTTY_LINUX) res |= O_NOCTTY, x &= ~O_NOCTTY_LINUX;
#ifdef O_ASYNC
  if (x & O_ASYNC_LINUX) res |= O_ASYNC, x &= ~O_ASYNC_LINUX;
#endif
#ifdef O_NOATIME
  if (x & O_NOATIME_LINUX) res |= O_NOATIME, x &= ~O_NOATIME_LINUX;
#endif
#ifdef O_DSYNC
  if (x & O_DSYNC_LINUX) res |= O_DSYNC, x &= ~O_DSYNC_LINUX;
#endif
  if (x) {
    LOGF("%s %#x not supported", "open flags", x);
    return einval();
  }
  return res;
}

int UnXlatOpenFlags(int x) {
  int res;
  res = UnXlatAccMode(x);
  if (x & O_APPEND) res |= O_APPEND_LINUX;
  if (x & O_CREAT) res |= O_CREAT_LINUX;
  if (x & O_EXCL) res |= O_EXCL_LINUX;
  if (x & O_TRUNC) res |= O_TRUNC_LINUX;
#ifdef O_PATH
  if (x & O_PATH) res |= O_PATH_LINUX;
#elif defined(O_EXEC)
  if (x & O_EXEC) res |= O_PATH_LINUX;
#endif
#ifdef O_LARGEFILE
  if (x & O_LARGEFILE) res |= O_LARGEFILE_LINUX;
#endif
#ifdef O_NDELAY
  if (x & O_NDELAY) res |= O_NDELAY_LINUX;
#endif
#ifdef O_DIRECT
  if (x & O_DIRECT) res |= O_DIRECT_LINUX;
#endif
  if (x & O_DIRECTORY) res |= O_DIRECTORY_LINUX;
#ifdef O_NOFOLLOW
  if (x & O_NOFOLLOW) res |= O_NOFOLLOW_LINUX;
#endif
  if (x & O_CLOEXEC) res |= O_CLOEXEC_LINUX;
  if (x & O_NOCTTY) res |= O_NOCTTY_LINUX;
#ifdef O_ASYNC
  if (x & O_ASYNC) res |= O_ASYNC_LINUX;
#endif
#ifdef O_NOATIME
  if (x & O_NOATIME) res |= O_NOATIME_LINUX;
#endif
#ifdef O_DSYNC
  if (x & O_DSYNC) res |= O_DSYNC_LINUX;
#endif
  return res;
}

int XlatSockaddrToHost(struct sockaddr_in *dst,
                       const struct sockaddr_in_linux *src) {
  int family;
  if ((family = XlatSocketFamily(Read16(src->sin_family))) == -1) return -1;
  memset(dst, 0, sizeof(*dst));
  dst->sin_family = family;
  dst->sin_port = src->sin_port;
  dst->sin_addr.s_addr = src->sin_addr;
  return 0;
}

void XlatSockaddrToLinux(struct sockaddr_in_linux *dst,
                         const struct sockaddr_in *src) {
  memset(dst, 0, sizeof(*dst));
  Write16(dst->sin_family, UnXlatSocketFamily(src->sin_family));
  dst->sin_port = src->sin_port;
  dst->sin_addr = src->sin_addr.s_addr;
}

void XlatStatToLinux(struct stat_linux *dst, const struct stat *src) {
  Write64(dst->st_dev, src->st_dev);
  Write64(dst->st_ino, src->st_ino);
  Write64(dst->st_nlink, src->st_nlink);
  Write32(dst->st_mode, src->st_mode);
  Write32(dst->st_uid, src->st_uid);
  Write32(dst->st_gid, src->st_gid);
  Write32(dst->__pad, 0);
  Write64(dst->st_rdev, src->st_rdev);
  Write64(dst->st_size, src->st_size);
  Write64(dst->st_blksize, src->st_blksize);
  Write64(dst->st_blocks, src->st_blocks);
  Write64(dst->st_dev, src->st_dev);
  Write64(dst->st_atim.tv_sec, src->st_atime);
  Write64(dst->st_atim.tv_nsec, 0);
  Write64(dst->st_mtim.tv_sec, src->st_mtime);
  Write64(dst->st_mtim.tv_nsec, 0);
  Write64(dst->st_ctim.tv_sec, src->st_ctime);
  Write64(dst->st_ctim.tv_nsec, 0);
}

void XlatRusageToLinux(struct rusage_linux *dst, const struct rusage *src) {
  Write64(dst->ru_utime.tv_sec, src->ru_utime.tv_sec);
  Write64(dst->ru_utime.tv_usec, src->ru_utime.tv_usec);
  Write64(dst->ru_stime.tv_sec, src->ru_stime.tv_sec);
  Write64(dst->ru_stime.tv_usec, src->ru_stime.tv_usec);
  Write64(dst->ru_maxrss, src->ru_maxrss);
  Write64(dst->ru_ixrss, src->ru_ixrss);
  Write64(dst->ru_idrss, src->ru_idrss);
  Write64(dst->ru_isrss, src->ru_isrss);
  Write64(dst->ru_minflt, src->ru_minflt);
  Write64(dst->ru_majflt, src->ru_majflt);
  Write64(dst->ru_nswap, src->ru_nswap);
  Write64(dst->ru_inblock, src->ru_inblock);
  Write64(dst->ru_oublock, src->ru_oublock);
  Write64(dst->ru_msgsnd, src->ru_msgsnd);
  Write64(dst->ru_msgrcv, src->ru_msgrcv);
  Write64(dst->ru_nsignals, src->ru_nsignals);
  Write64(dst->ru_nvcsw, src->ru_nvcsw);
  Write64(dst->ru_nivcsw, src->ru_nivcsw);
}

static u64 XlatRlimitToLinuxScalar(u64 x) {
  if (x == RLIM_INFINITY) x = RLIM_INFINITY_LINUX;
  return x;
}

void XlatRlimitToLinux(struct rlimit_linux *dst, const struct rlimit *src) {
  Write64(dst->rlim_cur, XlatRlimitToLinuxScalar(src->rlim_cur));
  Write64(dst->rlim_max, XlatRlimitToLinuxScalar(src->rlim_max));
}

static u64 XlatLinuxToRlimitScalar(int r, u64 x) {
  if (x == RLIM_INFINITY_LINUX) {
    x = RLIM_INFINITY;
  } else if (r == RLIMIT_NOFILE) {
    x += 4;
  } else if (r == RLIMIT_CPU ||   //
             r == RLIMIT_DATA ||  //
#ifdef RLIMIT_RSS
             r == RLIMIT_RSS ||  //
#endif
#ifdef RLIMIT_AS
             r == RLIMIT_AS ||  //
#endif
             0) {
    x *= 10;
  }
  return x;
}

void XlatLinuxToRlimit(int sysresource, struct rlimit *dst,
                       const struct rlimit_linux *src) {
  dst->rlim_cur = XlatLinuxToRlimitScalar(sysresource, Read64(src->rlim_cur));
  dst->rlim_max = XlatLinuxToRlimitScalar(sysresource, Read64(src->rlim_max));
}

void XlatItimervalToLinux(struct itimerval_linux *dst,
                          const struct itimerval *src) {
  Write64(dst->it_interval.tv_sec, src->it_interval.tv_sec);
  Write64(dst->it_interval.tv_usec, src->it_interval.tv_usec);
  Write64(dst->it_value.tv_sec, src->it_value.tv_sec);
  Write64(dst->it_value.tv_usec, src->it_value.tv_usec);
}

void XlatLinuxToItimerval(struct itimerval *dst,
                          const struct itimerval_linux *src) {
  dst->it_interval.tv_sec = Read64(src->it_interval.tv_sec);
  dst->it_interval.tv_usec = Read64(src->it_interval.tv_usec);
  dst->it_value.tv_sec = Read64(src->it_value.tv_sec);
  dst->it_value.tv_usec = Read64(src->it_value.tv_usec);
}

void XlatWinsizeToLinux(struct winsize_linux *dst, const struct winsize *src) {
  memset(dst, 0, sizeof(*dst));
  Write16(dst->ws_row, src->ws_row);
  Write16(dst->ws_col, src->ws_col);
}

void XlatSigsetToLinux(u8 dst[8], const sigset_t *src) {
  u64 set = 0;
  int syssig, linuxsig;
  for (syssig = 1; syssig <= 32; ++syssig) {
    if (sigismember(src, syssig) == 1 &&
        (linuxsig = UnXlatSignal(syssig)) != -1) {
      set |= 1ull << (linuxsig - 1);
    }
  }
  Write64(dst, set);
}

void XlatLinuxToSigset(sigset_t *dst, const u8 src[8]) {
  u64 set;
  int syssig, linuxsig;
  set = Read64(src);
  sigemptyset(dst);
  for (linuxsig = 1; linuxsig <= 32; ++linuxsig) {
    if (((1ull << (linuxsig - 1)) & set) &&
        (syssig = XlatSignal(linuxsig)) != -1) {
      sigaddset(dst, syssig);
    }
  }
}

static int XlatTermiosCflag(int x) {
  int r = 0;
  if (x & 0x0040) r |= CSTOPB;
  if (x & 0x0080) r |= CREAD;
  if (x & 0x0100) r |= PARENB;
  if (x & 0x0200) r |= PARODD;
  if (x & 0x0400) r |= HUPCL;
  if (x & 0x0800) r |= CLOCAL;
  if ((x & 0x0030) == 0x0010) {
    r |= CS6;
  } else if ((x & 0x0030) == 0x0020) {
    r |= CS7;
  } else if ((x & 0x0030) == 0x0030) {
    r |= CS8;
  }
  return r;
}

static int UnXlatTermiosCflag(int x) {
  int r = 0;
  if (x & CSTOPB) r |= 0x0040;
  if (x & CREAD) r |= 0x0080;
  if (x & PARENB) r |= 0x0100;
  if (x & PARODD) r |= 0x0200;
  if (x & HUPCL) r |= 0x0400;
  if (x & CLOCAL) r |= 0x0800;
  if ((x & CSIZE) == CS5) {
    r |= 0x0000;
  } else if ((x & CSIZE) == CS6) {
    r |= 0x0010;
  } else if ((x & CSIZE) == CS7) {
    r |= 0x0020;
  } else if ((x & CSIZE) == CS8) {
    r |= 0x0030;
  }
  return r;
}

static int XlatTermiosLflag(int x) {
  int r = 0;
  if (x & 0x0001) r |= ISIG;
  if (x & 0x0002) r |= ICANON;
  if (x & 0x0008) r |= ECHO;
  if (x & 0x0010) r |= ECHOE;
  if (x & 0x0020) r |= ECHOK;
  if (x & 0x0040) r |= ECHONL;
  if (x & 0x0080) r |= NOFLSH;
  if (x & 0x0100) r |= TOSTOP;
  if (x & 0x8000) r |= IEXTEN;
#ifdef ECHOCTL
  if (x & 0x0200) r |= ECHOCTL;
#endif
#ifdef ECHOPRT
  if (x & 0x0400) r |= ECHOPRT;
#endif
#ifdef ECHOKE
  if (x & 0x0800) r |= ECHOKE;
#endif
#ifdef FLUSHO
  if (x & 0x1000) r |= FLUSHO;
#endif
#ifdef PENDIN
  if (x & 0x4000) r |= PENDIN;
#endif
#ifdef XCASE
  if (x & 0x0004) r |= XCASE;
#endif
  return r;
}

static int UnXlatTermiosLflag(int x) {
  int r = 0;
  if (x & ISIG) r |= 0x0001;
  if (x & ICANON) r |= 0x0002;
  if (x & ECHO) r |= 0x0008;
  if (x & ECHOE) r |= 0x0010;
  if (x & ECHOK) r |= 0x0020;
  if (x & ECHONL) r |= 0x0040;
  if (x & NOFLSH) r |= 0x0080;
  if (x & TOSTOP) r |= 0x0100;
  if (x & IEXTEN) r |= 0x8000;
#ifdef ECHOCTL
  if (x & ECHOCTL) r |= 0x0200;
#endif
#ifdef ECHOPRT
  if (x & ECHOPRT) r |= 0x0400;
#endif
#ifdef ECHOKE
  if (x & ECHOKE) r |= 0x0800;
#endif
#ifdef FLUSHO
  if (x & FLUSHO) r |= 0x1000;
#endif
#ifdef PENDIN
  if (x & PENDIN) r |= 0x4000;
#endif
#ifdef XCASE
  if (x & XCASE) r |= 0x0004;
#endif
  return r;
}

static int XlatTermiosIflag(int x) {
  int r = 0;
  if (x & 0x0001) r |= IGNBRK;
  if (x & 0x0002) r |= BRKINT;
  if (x & 0x0004) r |= IGNPAR;
  if (x & 0x0008) r |= PARMRK;
  if (x & 0x0010) r |= INPCK;
  if (x & 0x0020) r |= ISTRIP;
  if (x & 0x0040) r |= INLCR;
  if (x & 0x0080) r |= IGNCR;
  if (x & 0x0100) r |= ICRNL;
  if (x & 0x0400) r |= IXON;
#ifdef IXANY
  if (x & 0x0800) r |= IXANY;
#endif
  if (x & 0x1000) r |= IXOFF;
#ifdef IMAXBEL
  if (x & 0x2000) r |= IMAXBEL;
#endif
#ifdef IUTF8
  if (x & 0x4000) r |= IUTF8;
#endif
#ifdef IUCLC
  if (x & 0x0200) r |= IUCLC;
#endif
  return r;
}

static int UnXlatTermiosIflag(int x) {
  int r = 0;
  if (x & IGNBRK) r |= 0x0001;
  if (x & BRKINT) r |= 0x0002;
  if (x & IGNPAR) r |= 0x0004;
  if (x & PARMRK) r |= 0x0008;
  if (x & INPCK) r |= 0x0010;
  if (x & ISTRIP) r |= 0x0020;
  if (x & INLCR) r |= 0x0040;
  if (x & IGNCR) r |= 0x0080;
  if (x & ICRNL) r |= 0x0100;
  if (x & IXON) r |= 0x0400;
#ifdef IXANY
  if (x & IXANY) r |= 0x0800;
#endif
  if (x & IXOFF) r |= 0x1000;
#ifdef IMAXBEL
  if (x & IMAXBEL) r |= 0x2000;
#endif
#ifdef IUTF8
  if (x & IUTF8) r |= 0x4000;
#endif
#ifdef IUCLC
  if (x & IUCLC) r |= 0x0200;
#endif
  return r;
}

static int XlatTermiosOflag(int x) {
  int r = 0;
  if (x & 0x0001) r |= OPOST;
#ifdef ONLCR
  if (x & 0x0004) r |= ONLCR;
#endif
#ifdef OCRNL
  if (x & 0x0008) r |= OCRNL;
#endif
#ifdef ONOCR
  if (x & 0x0010) r |= ONOCR;
#endif
#ifdef ONLRET
  if (x & 0x0020) r |= ONLRET;
#endif
#ifdef OFILL
  if (x & 0x0040) r |= OFILL;
#endif
#ifdef OFDEL
  if (x & 0x0080) r |= OFDEL;
#endif
#ifdef NLDLY
  if ((x & 0x0100) == 0x0000) {
    r |= NL0;
  } else if ((x & 0x0100) == 0x0100) {
    r |= NL1;
#ifdef NL2
  } else if ((x & 0x0100) == 0x0000) {
    r |= NL2;
#endif
#ifdef NL3
  } else if ((x & 0x0100) == 0x0000) {
    r |= NL3;
#endif
  }
#endif
#ifdef CRDLY
  if ((x & 0x0600) == 0x0000) {
    r |= CR0;
  } else if ((x & 0x0600) == 0x0200) {
    r |= CR1;
  } else if ((x & 0x0600) == 0x0400) {
    r |= CR2;
  } else if ((x & 0x0600) == 0x0600) {
    r |= CR3;
  }
#endif
#ifdef TABDLY
  if ((x & 0x1800) == 0x0000) {
    r |= TAB0;
#ifdef TAB1
  } else if ((x & 0x1800) == 0x0800) {
    r |= TAB1;
#endif
#ifdef TAB1
  } else if ((x & 0x1800) == 0x1000) {
    r |= TAB2;
#endif
  } else if ((x & 0x1800) == 0x1800) {
    r |= TAB3;
  }
#endif
#ifdef BSDLY
  if ((x & 0x2000) == 0x0000) {
    r |= BS0;
  } else if ((x & 0x2000) == 0x2000) {
    r |= BS1;
  }
#endif
#ifdef VTBDLY
  if ((x & 0x4000) == 0x0000) {
    r |= VT0;
  } else if ((x & 0x4000) == 0x4000) {
    r |= VT1;
  }
#endif
#ifdef FFBDLY
  if ((x & 0x8000) == 0x0000) {
    r |= FF0;
  } else if ((x & 0x8000) == 0x8000) {
    r |= FF1;
  }
#endif
#ifdef OLCUC
  if (x & 0x0002) r |= OLCUC;
#endif
  return r;
}

static int UnXlatTermiosOflag(int x) {
  int r = 0;
  if (x & OPOST) r |= 0x0001;
#ifdef ONLCR
  if (x & ONLCR) r |= 0x0004;
#endif
#ifdef OCRNL
  if (x & OCRNL) r |= 0x0008;
#endif
#ifdef ONOCR
  if (x & ONOCR) r |= 0x0010;
#endif
#ifdef ONLRET
  if (x & ONLRET) r |= 0x0020;
#endif
#ifdef OFILL
  if (x & OFILL) r |= 0x0040;
#endif
#ifdef OFDEL
  if (x & OFDEL) r |= 0x0080;
#endif
#ifdef NLDLY
  if ((x & NLDLY) == NL0) {
    r |= 0x0000;
  } else if ((x & NLDLY) == NL1) {
    r |= 0x0100;
#ifdef NL2
  } else if ((x & NLDLY) == NL2) {
    r |= 0x0000;
#endif
#ifdef NL3
  } else if ((x & NLDLY) == NL3) {
    r |= 0x0000;
#endif
  }
#endif
#ifdef CRDLY
  if ((x & CRDLY) == CR0) {
    r |= 0x0000;
  } else if ((x & CRDLY) == CR1) {
    r |= 0x0200;
  } else if ((x & CRDLY) == CR2) {
    r |= 0x0400;
  } else if ((x & CRDLY) == CR3) {
    r |= 0x0600;
  }
#endif
#ifdef TABDLY
  if ((x & TABDLY) == TAB0) {
    r |= 0x0000;
#ifdef TAB1
  } else if ((x & TABDLY) == TAB1) {
    r |= 0x0800;
#endif
#ifdef TAB2
  } else if ((x & TABDLY) == TAB2) {
    r |= 0x1000;
#endif
  } else if ((x & TABDLY) == TAB3) {
    r |= 0x1800;
  }
#endif
#ifdef BSDLY
  if ((x & BSDLY) == BS0) {
    r |= 0x0000;
  } else if ((x & BSDLY) == BS1) {
    r |= 0x2000;
  }
#endif
#ifdef VTDLY
  if ((x & VTDLY) == VT0) {
    r |= 0x0000;
  } else if ((x & VTDLY) == VT1) {
    r |= 0x4000;
  }
#endif
#ifdef FFDLY
  if ((x & FFDLY) == FF0) {
    r |= 0x0000;
  } else if ((x & FFDLY) == FF1) {
    r |= 0x8000;
  }
#endif
#ifdef OLCUC
  if (x & OLCUC) r |= 0x0002;
#endif
  return r;
}

static void XlatTermiosCc(struct termios *dst,
                          const struct termios_linux *src) {
  dst->c_cc[VINTR] = src->c_cc[VINTR_LINUX];
  dst->c_cc[VQUIT] = src->c_cc[VQUIT_LINUX];
  dst->c_cc[VERASE] = src->c_cc[VERASE_LINUX];
  dst->c_cc[VKILL] = src->c_cc[VKILL_LINUX];
  dst->c_cc[VEOF] = src->c_cc[VEOF_LINUX];
  dst->c_cc[VTIME] = src->c_cc[VTIME_LINUX];
  dst->c_cc[VMIN] = src->c_cc[VMIN_LINUX];
  dst->c_cc[VSTART] = src->c_cc[VSTART_LINUX];
  dst->c_cc[VSTOP] = src->c_cc[VSTOP_LINUX];
  dst->c_cc[VSUSP] = src->c_cc[VSUSP_LINUX];
  dst->c_cc[VEOL] = src->c_cc[VEOL_LINUX];
#ifdef VSWTC
  dst->c_cc[VSWTC] = src->c_cc[VSWTC_LINUX];
#endif
#ifdef VREPRINT
  dst->c_cc[VREPRINT] = src->c_cc[VREPRINT_LINUX];
#endif
#ifdef VDISCARD
  dst->c_cc[VDISCARD] = src->c_cc[VDISCARD_LINUX];
#endif
#ifdef VWERASE
  dst->c_cc[VWERASE] = src->c_cc[VWERASE_LINUX];
#endif
#ifdef VLNEXT
  dst->c_cc[VLNEXT] = src->c_cc[VLNEXT_LINUX];
#endif
#ifdef VEOL2
  dst->c_cc[VEOL2] = src->c_cc[VEOL2_LINUX];
#endif
}

static void UnXlatTermiosCc(struct termios_linux *dst,
                            const struct termios *src) {
  dst->c_cc[VINTR_LINUX] = src->c_cc[VINTR];
  dst->c_cc[VQUIT_LINUX] = src->c_cc[VQUIT];
  dst->c_cc[VERASE_LINUX] = src->c_cc[VERASE];
  dst->c_cc[VKILL_LINUX] = src->c_cc[VKILL];
  dst->c_cc[VEOF_LINUX] = src->c_cc[VEOF];
  dst->c_cc[VTIME_LINUX] = src->c_cc[VTIME];
  dst->c_cc[VMIN_LINUX] = src->c_cc[VMIN];
  dst->c_cc[VSTART_LINUX] = src->c_cc[VSTART];
  dst->c_cc[VSTOP_LINUX] = src->c_cc[VSTOP];
  dst->c_cc[VSUSP_LINUX] = src->c_cc[VSUSP];
  dst->c_cc[VEOL_LINUX] = src->c_cc[VEOL];
#ifdef VSWTC
  dst->c_cc[VSWTC_LINUX] = src->c_cc[VSWTC];
#endif
#ifdef VREPRINT
  dst->c_cc[VREPRINT_LINUX] = src->c_cc[VREPRINT];
#endif
#ifdef VDISCARD
  dst->c_cc[VDISCARD_LINUX] = src->c_cc[VDISCARD];
#endif
#ifdef VWERASE
  dst->c_cc[VWERASE_LINUX] = src->c_cc[VWERASE];
#endif
#ifdef VLNEXT
  dst->c_cc[VLNEXT_LINUX] = src->c_cc[VLNEXT];
#endif
#ifdef VEOL2
  dst->c_cc[VEOL2_LINUX] = src->c_cc[VEOL2];
#endif
}

void XlatLinuxToTermios(struct termios *dst, const struct termios_linux *src) {
  memset(dst, 0, sizeof(*dst));
  dst->c_iflag = XlatTermiosIflag(Read32(src->c_iflag));
  dst->c_oflag = XlatTermiosOflag(Read32(src->c_oflag));
  dst->c_cflag = XlatTermiosCflag(Read32(src->c_cflag));
  dst->c_lflag = XlatTermiosLflag(Read32(src->c_lflag));
  XlatTermiosCc(dst, src);
}

void XlatTermiosToLinux(struct termios_linux *dst, const struct termios *src) {
  memset(dst, 0, sizeof(*dst));
  Write32(dst->c_iflag, UnXlatTermiosIflag(src->c_iflag));
  Write32(dst->c_oflag, UnXlatTermiosOflag(src->c_oflag));
  Write32(dst->c_cflag, UnXlatTermiosCflag(src->c_cflag));
  Write32(dst->c_lflag, UnXlatTermiosLflag(src->c_lflag));
  UnXlatTermiosCc(dst, src);
}

int XlatWhence(int x) {
  switch (x) {
    XLAT(SEEK_SET_LINUX, SEEK_SET);
    XLAT(SEEK_CUR_LINUX, SEEK_CUR);
    XLAT(SEEK_END_LINUX, SEEK_END);
    default:
      LOGF("unrecognized whence: %d", x);
      return einval();
  }
}
