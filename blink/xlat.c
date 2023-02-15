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
#include <stddef.h>
#include <string.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "blink/builtin.h"
#include "blink/case.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/sigwinch.h"
#include "blink/xlat.h"

#if defined(__APPLE__) || defined(__NetBSD__)
#define st_atim st_atimespec
#define st_ctim st_ctimespec
#define st_mtim st_mtimespec
#endif

// NSIG isn't POSIX but it's more common than SIGRTMAX which is.
// Note: OpenBSD defines NSIG as 33, even though it only has 32.
#ifdef NSIG
#define TOPSIG NSIG
#elif defined(SIGRTMAX)
#define TOPSIG SIGRTMAX
#else
#define TOPSIG 32
#endif

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
#ifdef SIGPWR
    XLAT(SIGPWR_LINUX, SIGPWR);
#endif
    XLAT(SIGSYS_LINUX, SIGSYS);
    default:
      break;
  }
#ifdef SIGRTMIN
  if ((SIGRTMIN_LINUX <= x && x <= SIGRTMAX_LINUX) &&
      SIGRTMIN + (x - SIGRTMIN_LINUX) <= SIGRTMAX) {
    return SIGRTMIN + (x - SIGRTMIN_LINUX);
  }
#endif
  return einval();
}

int XlatResource(int x) {
  switch (x) {
    XLAT(RLIMIT_CPU_LINUX, RLIMIT_CPU);
    XLAT(RLIMIT_FSIZE_LINUX, RLIMIT_FSIZE);
    XLAT(RLIMIT_DATA_LINUX, RLIMIT_DATA);
    XLAT(RLIMIT_STACK_LINUX, RLIMIT_STACK);
    XLAT(RLIMIT_CORE_LINUX, RLIMIT_CORE);
#ifdef RLIMIT_RSS
    XLAT(RLIMIT_RSS_LINUX, RLIMIT_RSS);
#endif
#ifdef RLIMIT_NPROC
    XLAT(RLIMIT_NPROC_LINUX, RLIMIT_NPROC);
#endif
    XLAT(RLIMIT_NOFILE_LINUX, RLIMIT_NOFILE);
#ifdef RLIMIT_MEMLOCK
    XLAT(RLIMIT_MEMLOCK_LINUX, RLIMIT_MEMLOCK);
#endif
#ifdef RLIMIT_AS
    XLAT(RLIMIT_AS_LINUX, RLIMIT_AS);
#endif
#ifdef RLIMIT_LOCKS
    XLAT(RLIMIT_LOCKS_LINUX, RLIMIT_LOCKS);
#endif
#ifdef RLIMIT_SIGPENDING
    XLAT(RLIMIT_SIGPENDING_LINUX, RLIMIT_SIGPENDING);
#endif
#ifdef RLIMIT_MSGQUEUE
    XLAT(RLIMIT_MSGQUEUE_LINUX, RLIMIT_MSGQUEUE);
#endif
#ifdef RLIMIT_NICE
    XLAT(RLIMIT_NICE_LINUX, RLIMIT_NICE);
#endif
#ifdef RLIMIT_RTPRIO
    XLAT(RLIMIT_RTPRIO_LINUX, RLIMIT_RTPRIO);
#endif
#ifdef RLIMIT_RTTIME
    XLAT(RLIMIT_RTTIME_LINUX, RLIMIT_RTTIME);
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
#ifdef SIGPWR
  if (x == SIGPWR) return SIGPWR_LINUX;
#endif
#ifdef SIGSTKFLT
  if (x == SIGSTKFLT) return SIGSTKFLT_LINUX;
#endif
  if (x == SIGSTOP) return SIGSTOP_LINUX;
  if (x == SIGSYS) return SIGSYS_LINUX;
  if (x == SIGTSTP) return SIGTSTP_LINUX;
  if (x == SIGURG) return SIGURG_LINUX;
#ifdef SIGRTMIN
  if ((SIGRTMIN <= x && x <= SIGRTMAX) &&
      SIGRTMIN_LINUX + (x - SIGRTMIN) <= SIGRTMAX_LINUX) {
    return SIGRTMIN_LINUX + (x - SIGRTMIN);
  }
#endif
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
    XLAT(AF_INET6_LINUX, AF_INET6);
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
  if (x == AF_INET6) return AF_INET6_LINUX;
  LOGF("don't know how to translate %s %d", "socket family", x);
  return x;
}

int XlatSocketType(int x) {
  switch (x) {
    XLAT(SOCK_STREAM_LINUX, SOCK_STREAM);
    XLAT(SOCK_DGRAM_LINUX, SOCK_DGRAM);
    XLAT(SOCK_RAW_LINUX, SOCK_RAW);
    default:
      LOGF("%s %d not supported yet", "socket type", x);
      return einval();
  }
}

int XlatSocketProtocol(int x) {
  switch (x) {
    XLAT(0, 0);
    XLAT(IPPROTO_RAW_LINUX, IPPROTO_RAW);
    XLAT(IPPROTO_TCP_LINUX, IPPROTO_TCP);
    XLAT(IPPROTO_UDP_LINUX, IPPROTO_UDP);
    XLAT(IPPROTO_ICMP_LINUX, IPPROTO_ICMP);
#ifdef IPPROTO_ICMPV6
    XLAT(IPPROTO_ICMPV6_LINUX, IPPROTO_ICMPV6);
#endif
    default:
      LOGF("%s %d not supported yet", "socket protocol", x);
      return einval();
  }
}

int XlatSocketLevel(int x, int *level) {
  // Haiku defines SOL_SOCKET as -1
  int res;
  switch (x) {
    CASE(SOL_SOCKET_LINUX, res = SOL_SOCKET);
    CASE(SOL_IP_LINUX, res = IPPROTO_IP);
    CASE(SOL_IPV6_LINUX, res = IPPROTO_IPV6);
    CASE(SOL_TCP_LINUX, res = IPPROTO_TCP);
    CASE(SOL_UDP_LINUX, res = IPPROTO_UDP);
    default:
      LOGF("%s %d not supported yet", "socket level", x);
      return einval();
  }
  *level = res;
  return 0;
}

int XlatSocketOptname(int level, int optname) {
  switch (level) {
    case SOL_SOCKET_LINUX:
      switch (optname) {
        XLAT(SO_TYPE_LINUX, SO_TYPE);
        XLAT(SO_ERROR_LINUX, SO_ERROR);
        XLAT(SO_DEBUG_LINUX, SO_DEBUG);
        XLAT(SO_REUSEADDR_LINUX, SO_REUSEADDR);
        XLAT(SO_DONTROUTE_LINUX, SO_DONTROUTE);
        XLAT(SO_SNDBUF_LINUX, SO_SNDBUF);
        XLAT(SO_RCVBUF_LINUX, SO_RCVBUF);
        XLAT(SO_BROADCAST_LINUX, SO_BROADCAST);
        XLAT(SO_KEEPALIVE_LINUX, SO_KEEPALIVE);
#ifdef SO_REUSEPORT
        XLAT(SO_REUSEPORT_LINUX, SO_REUSEPORT);
#endif
        XLAT(SO_RCVTIMEO_LINUX, SO_RCVTIMEO);
        XLAT(SO_SNDTIMEO_LINUX, SO_SNDTIMEO);
#ifdef SO_RCVLOWAT
        XLAT(SO_RCVLOWAT_LINUX, SO_RCVLOWAT);
#endif
#ifdef SO_SNDLOWAT
        XLAT(SO_SNDLOWAT_LINUX, SO_SNDLOWAT);
#endif
        default:
          break;
      }
      break;
    case SOL_IP_LINUX:
      switch (optname) {
        XLAT(IP_TOS_LINUX, IP_TOS);
        XLAT(IP_TTL_LINUX, IP_TTL);
        XLAT(IP_HDRINCL_LINUX, IP_HDRINCL);
        XLAT(IP_OPTIONS_LINUX, IP_OPTIONS);
        XLAT(IP_RECVTTL_LINUX, IP_RECVTTL);
#ifdef IP_RECVERR
        XLAT(IP_RECVERR_LINUX, IP_RECVERR);
#endif
#ifdef IP_RETOPTS
        XLAT(IP_RETOPTS_LINUX, IP_RETOPTS);
#endif
        default:
          break;
      }
      break;
    case SOL_IPV6_LINUX:
      switch (optname) {
#ifdef IPV6_RECVERR
        XLAT(IPV6_RECVERR_LINUX, IPV6_RECVERR);
#endif
        default:
          break;
      }
      break;
    case SOL_TCP_LINUX:
      switch (optname) {
        XLAT(TCP_NODELAY_LINUX, TCP_NODELAY);
#ifdef TCP_MAXSEG
        XLAT(TCP_MAXSEG_LINUX, TCP_MAXSEG);
#endif
#if defined(TCP_CORK)
        XLAT(TCP_CORK_LINUX, TCP_CORK);
#elif defined(TCP_NOPUSH)
        XLAT(TCP_NOPUSH_LINUX, TCP_NOPUSH);
#endif
#ifdef TCP_KEEPIDLE
        XLAT(TCP_KEEPIDLE_LINUX, TCP_KEEPIDLE);
#endif
#ifdef TCP_KEEPINTVL
        XLAT(TCP_KEEPINTVL_LINUX, TCP_KEEPINTVL);
#endif
#ifdef TCP_KEEPCNT
        XLAT(TCP_KEEPCNT_LINUX, TCP_KEEPCNT);
#endif
#ifdef TCP_SYNCNT
        XLAT(TCP_SYNCNT_LINUX, TCP_SYNCNT);
#endif
#ifdef TCP_DEFER_ACCEPT
        XLAT(TCP_DEFER_ACCEPT_LINUX, TCP_DEFER_ACCEPT);
#endif
#ifdef TCP_WINDOW_CLAMP
        XLAT(TCP_WINDOW_CLAMP_LINUX, TCP_WINDOW_CLAMP);
#endif
#ifdef TCP_FASTOPEN
        XLAT(TCP_FASTOPEN_LINUX, TCP_FASTOPEN);
#endif
#ifdef TCP_NOTSENT_LOWAT
        XLAT(TCP_NOTSENT_LOWAT_LINUX, TCP_NOTSENT_LOWAT);
#endif
#ifdef TCP_FASTOPEN_CONNECT
        XLAT(TCP_FASTOPEN_CONNECT_LINUX, TCP_FASTOPEN_CONNECT);
#endif
#ifdef TCP_QUICKACK
        XLAT(TCP_QUICKACK_LINUX, TCP_QUICKACK);
#endif
#ifdef TCP_SAVE_SYN
        XLAT(TCP_SAVE_SYN_LINUX, TCP_SAVE_SYN);
#endif
        default:
          break;
      }
      break;
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

int XlatWait(int x) {
  int r = 0;
  if (x & WNOHANG_LINUX) {
    r |= WNOHANG;
    x &= ~WNOHANG_LINUX;
  }
  if (x & WUNTRACED_LINUX) {
    r |= WUNTRACED;
    x &= ~WUNTRACED_LINUX;
  }
#ifdef WCONTINUED
  if (x & WCONTINUED_LINUX) {
    r |= WCONTINUED;
    x &= ~WCONTINUED_LINUX;
  }
#endif
  if (x) {
    LOGF("%s %d not supported yet", "wait", x);
    return einval();
  }
  return r;
}

int XlatClock(int x, clock_t *clock) {
  // Haiku defines CLOCK_REALTIME as -1
  clock_t res;
  switch (x) {
    CASE(0, res = CLOCK_REALTIME);
    CASE(1, res = CLOCK_MONOTONIC);
    CASE(2, res = CLOCK_PROCESS_CPUTIME_ID);
    CASE(3, res = CLOCK_THREAD_CPUTIME_ID);
#ifdef CLOCK_REALTIME_COARSE
    CASE(5, res = CLOCK_REALTIME_COARSE);
#elif defined(CLOCK_REALTIME_FAST)
    CASE(5, res = CLOCK_REALTIME_FAST);
#endif
#ifdef CLOCK_MONOTONIC_COARSE
    CASE(6, res = CLOCK_MONOTONIC_COARSE);
#elif defined(CLOCK_REALTIME_FAST)
    CASE(6, res = CLOCK_MONOTONIC_FAST);
#endif
#ifdef CLOCK_MONOTONIC_RAW
    CASE(4, res = CLOCK_MONOTONIC_RAW);
#else
    CASE(4, res = CLOCK_MONOTONIC);
#endif
#ifdef CLOCK_BOOTTIME
    CASE(7, res = CLOCK_BOOTTIME);
#endif
#ifdef CLOCK_TAI
    CASE(11, res = CLOCK_TAI);
#endif
    default:
      LOGF("%s %d not supported yet", "clock", x);
      return einval();
  }
  *clock = res;
  return 0;
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
#ifdef O_SYNC
  if ((x & O_SYNC_LINUX) == O_SYNC_LINUX) {
    res |= O_SYNC;
    x &= ~O_SYNC_LINUX;
  }
  // order matters: O_DSYNC ⊂ O_SYNC
#endif
#ifdef O_DSYNC
  if (x & O_DSYNC_LINUX) {
    res |= O_DSYNC;
    x &= ~O_DSYNC_LINUX;
  }
#endif
#ifdef O_TMPFILE
  if ((x & O_TMPFILE_LINUX) == O_TMPFILE_LINUX) {
    res |= O_TMPFILE;
    x &= ~O_TMPFILE_LINUX;
  }
  // order matters: O_DIRECTORY ⊂ O_TMPFILE
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
  if ((x & O_APPEND) == O_APPEND) {
    res |= O_APPEND_LINUX;
    x &= ~O_APPEND;
  }
  if ((x & O_CREAT) == O_CREAT) {
    res |= O_CREAT_LINUX;
    x &= ~O_CREAT;
  }
  if ((x & O_EXCL) == O_EXCL) {
    res |= O_EXCL_LINUX;
    x &= ~O_EXCL;
  }
  if ((x & O_TRUNC) == O_TRUNC) {
    res |= O_TRUNC_LINUX;
    x &= ~O_TRUNC;
  }
#ifdef O_PATH
  if ((x & O_PATH) == O_PATH) {
    res |= O_PATH_LINUX;
    x &= ~O_PATH;
  }
#elif defined(O_EXEC)
  if ((x & O_EXEC) == O_EXEC) {
    res |= O_PATH_LINUX;
    x &= ~O_EXEC;
  }
#endif
#ifdef O_LARGEFILE
  if ((x & O_LARGEFILE) == O_LARGEFILE) {
    res |= O_LARGEFILE_LINUX;
    x &= ~O_LARGEFILE;
  }
#endif
#ifdef O_NDELAY
  if ((x & O_NDELAY) == O_NDELAY) {
    res |= O_NDELAY_LINUX;
    x &= ~O_NDELAY;
  }
#endif
#ifdef O_DIRECT
  if ((x & O_DIRECT) == O_DIRECT) {
    res |= O_DIRECT_LINUX;
    x &= ~O_DIRECT;
  }
#endif
#ifdef O_TMPFILE
  if ((x & O_TMPFILE) == O_TMPFILE) {
    res |= O_TMPFILE_LINUX;
    x &= ~O_TMPFILE;
  }
#endif
  if ((x & O_DIRECTORY) == O_DIRECTORY) {
    res |= O_DIRECTORY_LINUX;
    x &= ~O_DIRECTORY;
  }
#ifdef O_SYNC
  if ((x & O_SYNC) == O_SYNC) {
    res |= O_SYNC_LINUX;
    x &= ~O_SYNC;
  }
#endif
#ifdef O_NOFOLLOW
  if ((x & O_NOFOLLOW) == O_NOFOLLOW) {
    res |= O_NOFOLLOW_LINUX;
    x &= ~O_NOFOLLOW;
  }
#endif
  if ((x & O_CLOEXEC) == O_CLOEXEC) {
    res |= O_CLOEXEC_LINUX;
    x &= ~O_CLOEXEC;
  }
  if ((x & O_NOCTTY) == O_NOCTTY) {
    res |= O_NOCTTY_LINUX;
    x &= ~O_NOCTTY;
  }
#ifdef O_ASYNC
  if ((x & O_ASYNC) == O_ASYNC) {
    res |= O_ASYNC_LINUX;
    x &= ~O_ASYNC;
  }
#endif
#ifdef O_NOATIME
  if ((x & O_NOATIME) == O_NOATIME) {
    res |= O_NOATIME_LINUX;
    x &= ~O_NOATIME;
  }
#endif
#ifdef O_DSYNC
  if ((x & O_DSYNC) == O_DSYNC) {
    res |= O_DSYNC_LINUX;
    x &= ~O_DSYNC;
  }
#endif
  return res;
}

int UnXlatItimer(int x) {
  switch (x) {
    case ITIMER_REAL_LINUX:
      return ITIMER_REAL;
    case ITIMER_VIRTUAL_LINUX:
      return ITIMER_VIRTUAL;
    case ITIMER_PROF_LINUX:
      return ITIMER_PROF;
    default:
      LOGF("%s %#x not supported", "itimer", x);
      return einval();
  }
}

int XlatSockaddrToHost(struct sockaddr_storage *dst,
                       const struct sockaddr_linux *src, u32 srclen) {
  if (srclen < 2) {
    LOGF("sockaddr size %d too small for %s", (int)srclen, "family");
    return einval();
  }
  switch (Read16(src->family)) {
    case AF_UNSPEC_LINUX:
      memset(dst, 0, sizeof(*dst));
      dst->ss_family = AF_UNSPEC;
      return sizeof(*dst);
    case AF_UNIX_LINUX: {
      size_t n;
      struct sockaddr_un *dst_un;
      const struct sockaddr_un_linux *src_un;
      if (srclen < offsetof(struct sockaddr_un_linux, path)) {
        LOGF("sockaddr size too small for %s", "sockaddr_un_linux");
        return einval();
      }
      dst_un = (struct sockaddr_un *)dst;
      src_un = (const struct sockaddr_un_linux *)src;
      n = strnlen(src_un->path,
                  MIN(srclen - offsetof(struct sockaddr_un_linux, path),
                      sizeof(src_un->path)));
      if (n >= sizeof(dst_un->sun_path)) {
        LOGF("sockaddr_un path too long for host");
        return einval();
      }
      memset(dst_un, 0, sizeof(*dst_un));
      dst_un->sun_family = AF_UNIX;
      if (n) memcpy(dst_un->sun_path, src_un->path, n);
      dst_un->sun_path[n] = 0;
      return sizeof(struct sockaddr_un);
    }
    case AF_INET_LINUX: {
      struct sockaddr_in *dst_in;
      const struct sockaddr_in_linux *src_in;
      if (srclen < sizeof(struct sockaddr_in_linux)) {
        LOGF("sockaddr size too small for %s", "sockaddr_in_linux");
        return einval();
      }
      dst_in = (struct sockaddr_in *)dst;
      src_in = (const struct sockaddr_in_linux *)src;
      memset(dst_in, 0, sizeof(*dst_in));
      dst_in->sin_family = AF_INET;
      dst_in->sin_port = src_in->port;
      dst_in->sin_addr.s_addr = src_in->addr;
      return sizeof(struct sockaddr_in);
    }
    case AF_INET6_LINUX: {
      struct sockaddr_in6 *dst_in;
      const struct sockaddr_in6_linux *src_in;
      _Static_assert(sizeof(dst_in->sin6_addr) == 16, "");
      _Static_assert(sizeof(src_in->addr) == 16, "");
      if (srclen < sizeof(struct sockaddr_in6_linux)) {
        LOGF("sockaddr size too small for %s", "sockaddr_in6_linux");
        return einval();
      }
      dst_in = (struct sockaddr_in6 *)dst;
      src_in = (const struct sockaddr_in6_linux *)src;
      memset(dst_in, 0, sizeof(*dst_in));
      dst_in->sin6_family = AF_INET6;
      dst_in->sin6_port = src_in->port;
      memcpy(&dst_in->sin6_addr, src_in->addr, 16);
      return sizeof(struct sockaddr_in6);
    }
    default:
      LOGF("%s %d not supported yet", "socket family", Read16(src->family));
      errno = ENOPROTOOPT;
      return -1;
  }
}

int XlatSockaddrToLinux(struct sockaddr_storage_linux *dst,
                        const struct sockaddr *src, socklen_t srclen) {
  if (srclen < 2) {
    LOGF("sockaddr size %d too small for %s", (int)srclen, "family");
    return einval();
  }
  if (src->sa_family == AF_UNIX) {
    size_t n;
    struct sockaddr_un_linux *dst_un;
    const struct sockaddr_un *src_un;
    if (srclen < offsetof(struct sockaddr_un, sun_path)) {
      LOGF("sockaddr size %d too small for %s", (int)srclen, "sockaddr_un");
      return einval();
    }
    dst_un = (struct sockaddr_un_linux *)dst;
    src_un = (const struct sockaddr_un *)src;
    n = strnlen(src_un->sun_path,
                MIN(srclen - offsetof(struct sockaddr_un, sun_path),
                    sizeof(src_un->sun_path)));
    if (n >= sizeof(dst_un->path)) {
      LOGF("sockaddr_un path too long for linux");
      return einval();
    }
    memset(dst_un, 0, sizeof(*dst_un));
    Write16(dst_un->family, AF_UNIX_LINUX);
    if (n) memcpy(dst_un->path, src_un->sun_path, n);
    dst_un->path[n] = 0;
    return offsetof(struct sockaddr_un, sun_path) + n + 1;
  } else if (src->sa_family == AF_INET) {
    struct sockaddr_in_linux *dst_in;
    const struct sockaddr_in *src_in;
    if (srclen < sizeof(struct sockaddr_in)) {
      LOGF("sockaddr size %d too small for %s", (int)srclen, "sockaddr_in");
      return einval();
    }
    dst_in = (struct sockaddr_in_linux *)dst;
    src_in = (const struct sockaddr_in *)src;
    memset(dst_in, 0, sizeof(*dst_in));
    Write16(dst_in->family, AF_INET_LINUX);
    dst_in->port = src_in->sin_port;
    dst_in->addr = src_in->sin_addr.s_addr;
    return sizeof(struct sockaddr_in_linux);
  } else if (src->sa_family == AF_INET6) {
    struct sockaddr_in6_linux *dst_in;
    const struct sockaddr_in6 *src_in;
    if (srclen < sizeof(struct sockaddr_in6)) {
      LOGF("sockaddr size %d too small for %s", (int)srclen, "sockaddr_in6");
      return einval();
    }
    dst_in = (struct sockaddr_in6_linux *)dst;
    src_in = (const struct sockaddr_in6 *)src;
    memset(dst_in, 0, sizeof(*dst_in));
    Write16(dst_in->family, AF_INET6_LINUX);
    dst_in->port = src_in->sin6_port;
    memcpy(dst_in->addr, &src_in->sin6_addr, 16);
    return sizeof(struct sockaddr_in6_linux);
  } else {
    LOGF("%s %d not supported yet", "socket family", src->sa_family);
    errno = ENOPROTOOPT;
    return -1;
  }
}

void XlatStatToLinux(struct stat_linux *dst, const struct stat *src) {
  Write64(dst->dev, src->st_dev);
  Write64(dst->ino, src->st_ino);
  Write64(dst->nlink, src->st_nlink);
  Write32(dst->mode, src->st_mode);
  Write32(dst->uid, src->st_uid);
  Write32(dst->gid, src->st_gid);
  Write32(dst->pad_, 0);
  Write64(dst->rdev, src->st_rdev);
  Write64(dst->size, src->st_size);
  Write64(dst->blksize, src->st_blksize);
  Write64(dst->blocks, src->st_blocks);
  Write64(dst->dev, src->st_dev);
  Write64(dst->atim.sec, src->st_atim.tv_sec);
  Write64(dst->atim.nsec, src->st_atim.tv_nsec);
  Write64(dst->mtim.sec, src->st_mtim.tv_sec);
  Write64(dst->mtim.nsec, src->st_mtim.tv_nsec);
  Write64(dst->ctim.sec, src->st_ctim.tv_sec);
  Write64(dst->ctim.nsec, src->st_ctim.tv_nsec);
}

void XlatRusageToLinux(struct rusage_linux *dst, const struct rusage *src) {
  Write64(dst->utime.sec, src->ru_utime.tv_sec);
  Write64(dst->utime.usec, src->ru_utime.tv_usec);
  Write64(dst->stime.sec, src->ru_stime.tv_sec);
  Write64(dst->stime.usec, src->ru_stime.tv_usec);
  Write64(dst->maxrss, src->ru_maxrss);
  Write64(dst->ixrss, src->ru_ixrss);
  Write64(dst->idrss, src->ru_idrss);
  Write64(dst->isrss, src->ru_isrss);
  Write64(dst->minflt, src->ru_minflt);
  Write64(dst->majflt, src->ru_majflt);
  Write64(dst->nswap, src->ru_nswap);
  Write64(dst->inblock, src->ru_inblock);
  Write64(dst->oublock, src->ru_oublock);
  Write64(dst->msgsnd, src->ru_msgsnd);
  Write64(dst->msgrcv, src->ru_msgrcv);
  Write64(dst->nsignals, src->ru_nsignals);
  Write64(dst->nvcsw, src->ru_nvcsw);
  Write64(dst->nivcsw, src->ru_nivcsw);
}

static u64 XlatRlimitToLinuxScalar(u64 x) {
  if (x == RLIM_INFINITY) x = RLIM_INFINITY_LINUX;
  return x;
}

void XlatRlimitToLinux(struct rlimit_linux *dst, const struct rlimit *src) {
  Write64(dst->cur, XlatRlimitToLinuxScalar(src->rlim_cur));
  Write64(dst->max, XlatRlimitToLinuxScalar(src->rlim_max));
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
  dst->rlim_cur = XlatLinuxToRlimitScalar(sysresource, Read64(src->cur));
  dst->rlim_max = XlatLinuxToRlimitScalar(sysresource, Read64(src->max));
}

void XlatItimervalToLinux(struct itimerval_linux *dst,
                          const struct itimerval *src) {
  Write64(dst->interval.sec, src->it_interval.tv_sec);
  Write64(dst->interval.usec, src->it_interval.tv_usec);
  Write64(dst->value.sec, src->it_value.tv_sec);
  Write64(dst->value.usec, src->it_value.tv_usec);
}

void XlatLinuxToItimerval(struct itimerval *dst,
                          const struct itimerval_linux *src) {
  dst->it_interval.tv_sec = Read64(src->interval.sec);
  dst->it_interval.tv_usec = Read64(src->interval.usec);
  dst->it_value.tv_sec = Read64(src->value.sec);
  dst->it_value.tv_usec = Read64(src->value.usec);
}

void XlatWinsizeToLinux(struct winsize_linux *dst, const struct winsize *src) {
  memset(dst, 0, sizeof(*dst));
  Write16(dst->row, src->ws_row);
  Write16(dst->col, src->ws_col);
}

void XlatWinsizeToHost(struct winsize *dst, const struct winsize_linux *src) {
  memset(dst, 0, sizeof(*dst));
  dst->ws_row = Read16(src->row);
  dst->ws_col = Read16(src->col);
}

void XlatSigsetToLinux(u8 dst[8], const sigset_t *src) {
  u64 set = 0;
  int syssig, linuxsig;
  for (syssig = 1; syssig <= MIN(64, TOPSIG); ++syssig) {
    if (sigismember(src, syssig) == 1 &&
        (linuxsig = UnXlatSignal(syssig)) != -1) {
      set |= 1ull << (linuxsig - 1);
    }
  }
  Write64(dst, set);
}

void XlatLinuxToSigset(sigset_t *dst, u64 set) {
  int syssig, linuxsig;
  sigemptyset(dst);
  for (linuxsig = 1; linuxsig <= MIN(64, TOPSIG); ++linuxsig) {
    if (((1ull << (linuxsig - 1)) & set) &&
        (syssig = XlatSignal(linuxsig)) != -1) {
      sigaddset(dst, syssig);
    }
  }
}

static int XlatTermiosCflag(int x) {
  int r = 0;
  if (x & CSTOPB_LINUX) r |= CSTOPB;
  if (x & CREAD_LINUX) r |= CREAD;
  if (x & PARENB_LINUX) r |= PARENB;
  if (x & PARODD_LINUX) r |= PARODD;
  if (x & HUPCL_LINUX) r |= HUPCL;
  if (x & CLOCAL_LINUX) r |= CLOCAL;
  if ((x & CSIZE_LINUX) == CS5_LINUX) {
    r |= CS5;
  } else if ((x & CSIZE_LINUX) == CS6_LINUX) {
    r |= CS6;
  } else if ((x & CSIZE_LINUX) == CS7_LINUX) {
    r |= CS7;
  } else if ((x & CSIZE_LINUX) == CS8_LINUX) {
    r |= CS8;
  }
  return r;
}

static int UnXlatTermiosCflag(int x) {
  int r = 0;
  if (x & CSTOPB) r |= CSTOPB_LINUX;
  if (x & CREAD) r |= CREAD_LINUX;
  if (x & PARENB) r |= PARENB_LINUX;
  if (x & PARODD) r |= PARODD_LINUX;
  if (x & HUPCL) r |= HUPCL_LINUX;
  if (x & CLOCAL) r |= CLOCAL_LINUX;
  if ((x & CSIZE) == CS5) {
    r |= CS5_LINUX;
  } else if ((x & CSIZE) == CS6) {
    r |= CS6_LINUX;
  } else if ((x & CSIZE) == CS7) {
    r |= CS7_LINUX;
  } else if ((x & CSIZE) == CS8) {
    r |= CS8_LINUX;
  }
  return r;
}

static int XlatTermiosLflag(int x) {
  int r = 0;
  if (x & ISIG_LINUX) r |= ISIG;
  if (x & ICANON_LINUX) r |= ICANON;
  if (x & ECHO_LINUX) r |= ECHO;
  if (x & ECHOE_LINUX) r |= ECHOE;
  if (x & ECHOK_LINUX) r |= ECHOK;
  if (x & ECHONL_LINUX) r |= ECHONL;
  if (x & NOFLSH_LINUX) r |= NOFLSH;
  if (x & TOSTOP_LINUX) r |= TOSTOP;
  if (x & IEXTEN_LINUX) r |= IEXTEN;
#ifdef ECHOCTL
  if (x & ECHOCTL_LINUX) r |= ECHOCTL;
#endif
#ifdef ECHOPRT
  if (x & ECHOPRT_LINUX) r |= ECHOPRT;
#endif
#ifdef ECHOKE
  if (x & ECHOKE_LINUX) r |= ECHOKE;
#endif
#ifdef FLUSHO
  if (x & FLUSHO_LINUX) r |= FLUSHO;
#endif
#ifdef PENDIN
  if (x & PENDIN_LINUX) r |= PENDIN;
#endif
#ifdef XCASE
  if (x & XCASE_LINUX) r |= XCASE;
#endif
  return r;
}

static int UnXlatTermiosLflag(int x) {
  int r = 0;
  if (x & ISIG) r |= ISIG_LINUX;
  if (x & ICANON) r |= ICANON_LINUX;
  if (x & ECHO) r |= ECHO_LINUX;
  if (x & ECHOE) r |= ECHOE_LINUX;
  if (x & ECHOK) r |= ECHOK_LINUX;
  if (x & ECHONL) r |= ECHONL_LINUX;
  if (x & NOFLSH) r |= NOFLSH_LINUX;
  if (x & TOSTOP) r |= TOSTOP_LINUX;
  if (x & IEXTEN) r |= IEXTEN_LINUX;
#ifdef ECHOCTL
  if (x & ECHOCTL) r |= ECHOCTL_LINUX;
#endif
#ifdef ECHOPRT
  if (x & ECHOPRT) r |= ECHOPRT_LINUX;
#endif
#ifdef ECHOKE
  if (x & ECHOKE) r |= ECHOKE_LINUX;
#endif
#ifdef FLUSHO
  if (x & FLUSHO) r |= FLUSHO_LINUX;
#endif
#ifdef PENDIN
  if (x & PENDIN) r |= PENDIN_LINUX;
#endif
#ifdef XCASE
  if (x & XCASE) r |= XCASE_LINUX;
#endif
  return r;
}

static int XlatTermiosIflag(int x) {
  int r = 0;
  if (x & IGNBRK_LINUX) r |= IGNBRK;
  if (x & BRKINT_LINUX) r |= BRKINT;
  if (x & IGNPAR_LINUX) r |= IGNPAR;
  if (x & PARMRK_LINUX) r |= PARMRK;
  if (x & INPCK_LINUX) r |= INPCK;
  if (x & ISTRIP_LINUX) r |= ISTRIP;
  if (x & INLCR_LINUX) r |= INLCR;
  if (x & IGNCR_LINUX) r |= IGNCR;
  if (x & ICRNL_LINUX) r |= ICRNL;
  if (x & IXON_LINUX) r |= IXON;
#ifdef IXANY
  if (x & IXANY_LINUX) r |= IXANY;
#endif
  if (x & IXOFF_LINUX) r |= IXOFF;
#ifdef IMAXBEL
  if (x & IMAXBEL_LINUX) r |= IMAXBEL;
#endif
#ifdef IUTF8
  if (x & IUTF8_LINUX) r |= IUTF8;
#endif
#ifdef IUCLC
  if (x & IUCLC_LINUX) r |= IUCLC;
#endif
  return r;
}

static int UnXlatTermiosIflag(int x) {
  int r = 0;
  if (x & IGNBRK) r |= IGNBRK_LINUX;
  if (x & BRKINT) r |= BRKINT_LINUX;
  if (x & IGNPAR) r |= IGNPAR_LINUX;
  if (x & PARMRK) r |= PARMRK_LINUX;
  if (x & INPCK) r |= INPCK_LINUX;
  if (x & ISTRIP) r |= ISTRIP_LINUX;
  if (x & INLCR) r |= INLCR_LINUX;
  if (x & IGNCR) r |= IGNCR_LINUX;
  if (x & ICRNL) r |= ICRNL_LINUX;
  if (x & IXON) r |= IXON_LINUX;
#ifdef IXANY
  if (x & IXANY) r |= IXANY_LINUX;
#endif
  if (x & IXOFF) r |= IXOFF_LINUX;
#ifdef IMAXBEL
  if (x & IMAXBEL) r |= IMAXBEL_LINUX;
#endif
#ifdef IUTF8
  if (x & IUTF8) r |= IUTF8_LINUX;
#endif
#ifdef IUCLC
  if (x & IUCLC) r |= IUCLC_LINUX;
#endif
  return r;
}

static int XlatTermiosOflag(int x) {
  int r = 0;
  if (x & OPOST_LINUX) r |= OPOST;
#ifdef ONLCR
  if (x & ONLCR_LINUX) r |= ONLCR;
#endif
#ifdef OCRNL
  if (x & OCRNL_LINUX) r |= OCRNL;
#endif
#ifdef ONOCR
  if (x & ONOCR_LINUX) r |= ONOCR;
#endif
#ifdef ONLRET
  if (x & ONLRET_LINUX) r |= ONLRET;
#endif
#ifdef OFILL
  if (x & OFILL_LINUX) r |= OFILL;
#endif
#ifdef OFDEL
  if (x & OFDEL_LINUX) r |= OFDEL;
#endif
#ifdef NLDLY
  if ((x & NLDLY_LINUX) == NL0_LINUX) {
    r |= NL0;
  } else if ((x & NLDLY_LINUX) == NL1_LINUX) {
    r |= NL1;
  }
#endif
#ifdef CRDLY
  if ((x & CRDLY_LINUX) == CR0_LINUX) {
    r |= CR0;
  } else if ((x & CRDLY_LINUX) == CR1_LINUX) {
    r |= CR1;
  } else if ((x & CRDLY_LINUX) == CR2_LINUX) {
    r |= CR2;
  } else if ((x & CRDLY_LINUX) == CR3_LINUX) {
    r |= CR3;
  }
#endif
#ifdef TABDLY
  if ((x & TABDLY_LINUX) == TAB0_LINUX) {
    r |= TAB0;
#ifdef TAB1
  } else if ((x & TABDLY_LINUX) == TAB1_LINUX) {
    r |= TAB1;
#endif
#ifdef TAB1
  } else if ((x & TABDLY_LINUX) == TAB2_LINUX) {
    r |= TAB2;
#endif
  } else if ((x & TABDLY_LINUX) == TAB3_LINUX) {
    r |= TAB3;
  }
#endif
#ifdef BSDLY
  if ((x & BSDLY_LINUX) == BS0_LINUX) {
    r |= BS0;
  } else if ((x & BSDLY_LINUX) == BS1_LINUX) {
    r |= BS1;
  }
#endif
#ifdef VTBDLY
  if ((x & VTDLY_LINUX) == VT0_LINUX) {
    r |= VT0;
  } else if ((x & VTDLY_LINUX) == VT1_LINUX) {
    r |= VT1;
  }
#endif
#ifdef FFBDLY
  if ((x & FFDLY_LINUX) == FF0_LINUX) {
    r |= FF0;
  } else if ((x & FFDLY_LINUX) == FF1_LINUX) {
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
  if (x & OPOST) r |= OPOST_LINUX;
#ifdef ONLCR
  if (x & ONLCR) r |= ONLCR_LINUX;
#endif
#ifdef OCRNL
  if (x & OCRNL) r |= OCRNL_LINUX;
#endif
#ifdef ONOCR
  if (x & ONOCR) r |= ONOCR_LINUX;
#endif
#ifdef ONLRET
  if (x & ONLRET) r |= ONLRET_LINUX;
#endif
#ifdef OFILL
  if (x & OFILL) r |= OFILL_LINUX;
#endif
#ifdef OFDEL
  if (x & OFDEL) r |= OFDEL_LINUX;
#endif
#ifdef NLDLY
  if ((x & NLDLY) == NL0) {
    r |= NL0_LINUX;
  } else if ((x & NLDLY) == NL1) {
    r |= NL1_LINUX;
#ifdef NL2
  } else if ((x & NLDLY) == NL2) {
    r |= 0x0000;  // ???
#endif
#ifdef NL3
  } else if ((x & NLDLY) == NL3) {
    r |= 0x0000;  // ???
#endif
  }
#endif
#ifdef CRDLY
  if ((x & CRDLY) == CR0) {
    r |= CR0_LINUX;
  } else if ((x & CRDLY) == CR1) {
    r |= CR1_LINUX;
  } else if ((x & CRDLY) == CR2) {
    r |= CR2_LINUX;
  } else if ((x & CRDLY) == CR3) {
    r |= CR3_LINUX;
  }
#endif
#ifdef TABDLY
  if ((x & TABDLY) == TAB0) {
    r |= TAB0_LINUX;
#ifdef TAB1
  } else if ((x & TABDLY) == TAB1) {
    r |= TAB1_LINUX;
#endif
#ifdef TAB2
  } else if ((x & TABDLY) == TAB2) {
    r |= TAB2_LINUX;
#endif
  } else if ((x & TABDLY) == TAB3) {
    r |= TAB3_LINUX;
  }
#endif
#ifdef BSDLY
  if ((x & BSDLY) == BS0) {
    r |= BS0_LINUX;
  } else if ((x & BSDLY) == BS1) {
    r |= BS1_LINUX;
  }
#endif
#ifdef VTDLY
  if ((x & VTDLY) == VT0) {
    r |= VT0_LINUX;
  } else if ((x & VTDLY) == VT1) {
    r |= VT1_LINUX;
  }
#endif
#ifdef FFDLY
  if ((x & FFDLY) == FF0) {
    r |= FF0_LINUX;
  } else if ((x & FFDLY) == FF1) {
    r |= FF1_LINUX;
  }
#endif
#ifdef OLCUC
  if (x & OLCUC) r |= OLCUC_LINUX;
#endif
  return r;
}

static void XlatTermiosCc(struct termios *dst,
                          const struct termios_linux *src) {
  dst->c_cc[VINTR] = src->cc[VINTR_LINUX];
  dst->c_cc[VQUIT] = src->cc[VQUIT_LINUX];
  dst->c_cc[VERASE] = src->cc[VERASE_LINUX];
  dst->c_cc[VKILL] = src->cc[VKILL_LINUX];
  dst->c_cc[VEOF] = src->cc[VEOF_LINUX];
  dst->c_cc[VTIME] = src->cc[VTIME_LINUX];
  dst->c_cc[VMIN] = src->cc[VMIN_LINUX];
  dst->c_cc[VSTART] = src->cc[VSTART_LINUX];
  dst->c_cc[VSTOP] = src->cc[VSTOP_LINUX];
  dst->c_cc[VSUSP] = src->cc[VSUSP_LINUX];
  dst->c_cc[VEOL] = src->cc[VEOL_LINUX];
#ifdef VSWTC
  dst->c_cc[VSWTC] = src->cc[VSWTC_LINUX];
#endif
#ifdef VREPRINT
  dst->c_cc[VREPRINT] = src->cc[VREPRINT_LINUX];
#endif
#ifdef VDISCARD
  dst->c_cc[VDISCARD] = src->cc[VDISCARD_LINUX];
#endif
#ifdef VWERASE
  dst->c_cc[VWERASE] = src->cc[VWERASE_LINUX];
#endif
#ifdef VLNEXT
  dst->c_cc[VLNEXT] = src->cc[VLNEXT_LINUX];
#endif
#ifdef VEOL2
  dst->c_cc[VEOL2] = src->cc[VEOL2_LINUX];
#endif
}

static void UnXlatTermiosCc(struct termios_linux *dst,
                            const struct termios *src) {
  dst->cc[VINTR_LINUX] = src->c_cc[VINTR];
  dst->cc[VQUIT_LINUX] = src->c_cc[VQUIT];
  dst->cc[VERASE_LINUX] = src->c_cc[VERASE];
  dst->cc[VKILL_LINUX] = src->c_cc[VKILL];
  dst->cc[VEOF_LINUX] = src->c_cc[VEOF];
  dst->cc[VTIME_LINUX] = src->c_cc[VTIME];
  dst->cc[VMIN_LINUX] = src->c_cc[VMIN];
  dst->cc[VSTART_LINUX] = src->c_cc[VSTART];
  dst->cc[VSTOP_LINUX] = src->c_cc[VSTOP];
  dst->cc[VSUSP_LINUX] = src->c_cc[VSUSP];
  dst->cc[VEOL_LINUX] = src->c_cc[VEOL];
#ifdef VSWTC
  dst->cc[VSWTC_LINUX] = src->c_cc[VSWTC];
#endif
#ifdef VREPRINT
  dst->cc[VREPRINT_LINUX] = src->c_cc[VREPRINT];
#endif
#ifdef VDISCARD
  dst->cc[VDISCARD_LINUX] = src->c_cc[VDISCARD];
#endif
#ifdef VWERASE
  dst->cc[VWERASE_LINUX] = src->c_cc[VWERASE];
#endif
#ifdef VLNEXT
  dst->cc[VLNEXT_LINUX] = src->c_cc[VLNEXT];
#endif
#ifdef VEOL2
  dst->cc[VEOL2_LINUX] = src->c_cc[VEOL2];
#endif
}

void XlatLinuxToTermios(struct termios *dst, const struct termios_linux *src) {
  speed_t speed;
  memset(dst, 0, sizeof(*dst));
  dst->c_iflag = XlatTermiosIflag(Read32(src->iflag));
  dst->c_oflag = XlatTermiosOflag(Read32(src->oflag));
  dst->c_cflag = XlatTermiosCflag(Read32(src->cflag));
  dst->c_lflag = XlatTermiosLflag(Read32(src->lflag));
  switch (Read32(src->cflag) & CBAUD_LINUX) {
    case B0_LINUX:
      speed = B0;
      break;
    case B50_LINUX:
      speed = B50;
      break;
    case B75_LINUX:
      speed = B75;
      break;
    case B110_LINUX:
      speed = B110;
      break;
    case B134_LINUX:
      speed = B134;
      break;
    case B150_LINUX:
      speed = B150;
      break;
    case B200_LINUX:
      speed = B200;
      break;
    case B300_LINUX:
      speed = B300;
      break;
    case B600_LINUX:
      speed = B600;
      break;
    case B1200_LINUX:
      speed = B1200;
      break;
    case B1800_LINUX:
      speed = B1800;
      break;
    case B2400_LINUX:
      speed = B2400;
      break;
    case B4800_LINUX:
      speed = B4800;
      break;
    case B9600_LINUX:
      speed = B9600;
      break;
    case B19200_LINUX:
      speed = B19200;
      break;
    case B38400_LINUX:
      speed = B38400;
      break;
#ifdef B57600
    case B57600_LINUX:
      speed = B57600;
      break;
#endif
#ifdef B115200
    case B115200_LINUX:
      speed = B115200;
      break;
#endif
#ifdef B230400
    case B230400_LINUX:
      speed = B230400;
      break;
#endif
#ifdef B460800
    case B460800_LINUX:
      speed = B460800;
      break;
#endif
#ifdef B500000
    case B500000_LINUX:
      speed = B500000;
      break;
#endif
#ifdef B576000
    case B576000_LINUX:
      speed = B576000;
      break;
#endif
#ifdef B921600
    case B921600_LINUX:
      speed = B921600;
      break;
#endif
#ifdef B1000000
    case B1000000_LINUX:
      speed = B1000000;
      break;
#endif
#ifdef B1152000
    case B1152000_LINUX:
      speed = B1152000;
      break;
#endif
#ifdef B1500000
    case B1500000_LINUX:
      speed = B1500000;
      break;
#endif
#ifdef B2000000
    case B2000000_LINUX:
      speed = B2000000;
      break;
#endif
#ifdef B2500000
    case B2500000_LINUX:
      speed = B2500000;
      break;
#endif
#ifdef B3000000
    case B3000000_LINUX:
      speed = B3000000;
      break;
#endif
#ifdef B3500000
    case B3500000_LINUX:
      speed = B3500000;
      break;
#endif
#ifdef B4000000
    case B4000000_LINUX:
      speed = B4000000;
      break;
#endif
    default:
      LOGF("unknown baud rate: %#x", Read32(src->cflag) & CBAUD_LINUX);
      speed = B38400;
      break;
  }
  cfsetispeed(dst, speed);
  cfsetospeed(dst, speed);
  XlatTermiosCc(dst, src);
}

void XlatTermiosToLinux(struct termios_linux *dst, const struct termios *src) {
  int speed;
  speed_t srcspeed;
  memset(dst, 0, sizeof(*dst));
  Write32(dst->iflag, UnXlatTermiosIflag(src->c_iflag));
  Write32(dst->oflag, UnXlatTermiosOflag(src->c_oflag));
  Write32(dst->cflag, UnXlatTermiosCflag(src->c_cflag));
  Write32(dst->lflag, UnXlatTermiosLflag(src->c_lflag));
  if ((srcspeed = cfgetospeed(src)) != (speed_t)-1) {
    if (srcspeed == B0) {
      speed = B0_LINUX;
    } else if (srcspeed == B50) {
      speed = B50_LINUX;
    } else if (srcspeed == B75) {
      speed = B75_LINUX;
    } else if (srcspeed == B110) {
      speed = B110_LINUX;
    } else if (srcspeed == B134) {
      speed = B134_LINUX;
    } else if (srcspeed == B150) {
      speed = B150_LINUX;
    } else if (srcspeed == B200) {
      speed = B200_LINUX;
    } else if (srcspeed == B300) {
      speed = B300_LINUX;
    } else if (srcspeed == B600) {
      speed = B600_LINUX;
    } else if (srcspeed == B1200) {
      speed = B1200_LINUX;
    } else if (srcspeed == B1800) {
      speed = B1800_LINUX;
    } else if (srcspeed == B2400) {
      speed = B2400_LINUX;
    } else if (srcspeed == B4800) {
      speed = B4800_LINUX;
    } else if (srcspeed == B9600) {
      speed = B9600_LINUX;
    } else if (srcspeed == B19200) {
      speed = B19200_LINUX;
    } else if (srcspeed == B38400) {
      speed = B38400_LINUX;
#ifdef B57600
    } else if (srcspeed == B57600) {
      speed = B57600_LINUX;
#endif
#ifdef B115200
    } else if (srcspeed == B115200) {
      speed = B115200_LINUX;
#endif
#ifdef B230400
    } else if (srcspeed == B230400) {
      speed = B230400_LINUX;
#endif
#ifdef B460800
    } else if (srcspeed == B460800) {
      speed = B460800_LINUX;
#endif
#ifdef B500000
    } else if (srcspeed == B500000) {
      speed = B500000_LINUX;
#endif
#ifdef B576000
    } else if (srcspeed == B576000) {
      speed = B576000_LINUX;
#endif
#ifdef B921600
    } else if (srcspeed == B921600) {
      speed = B921600_LINUX;
#endif
#ifdef B1000000
    } else if (srcspeed == B1000000) {
      speed = B1000000_LINUX;
#endif
#ifdef B1152000
    } else if (srcspeed == B1152000) {
      speed = B1152000_LINUX;
#endif
#ifdef B1500000
    } else if (srcspeed == B1500000) {
      speed = B1500000_LINUX;
#endif
#ifdef B2000000
    } else if (srcspeed == B2000000) {
      speed = B2000000_LINUX;
#endif
#ifdef B2500000
    } else if (srcspeed == B2500000) {
      speed = B2500000_LINUX;
#endif
#ifdef B3000000
    } else if (srcspeed == B3000000) {
      speed = B3000000_LINUX;
#endif
#ifdef B3500000
    } else if (srcspeed == B3500000) {
      speed = B3500000_LINUX;
#endif
#ifdef B4000000
    } else if (srcspeed == B4000000) {
      speed = B4000000_LINUX;
#endif
    } else {
      LOGF("unrecognized baud rate: %ld", (long)srcspeed);
      speed = B38400;
    }
  } else {
    LOGF("failed to get baud rate: %s", DescribeHostErrno(errno));
    speed = B38400;
  }
  Write32(dst->cflag, (Read32(dst->cflag) & ~CBAUD_LINUX) | speed);
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
