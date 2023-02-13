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
#include <fcntl.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <termios.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/fds.h"
#include "blink/linux.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/syscall.h"
#include "blink/types.h"
#include "blink/xlat.h"

#ifdef __HAIKU__
#include <OS.h>
#include <sys/sockio.h>
#endif

static int IoctlTiocgwinsz(struct Machine *m, int fd, i64 addr,
                           int fn(int, struct winsize *)) {
  int rc;
  struct winsize ws;
  struct winsize_linux gws;
  if ((rc = fn(fd, &ws)) != -1) {
    XlatWinsizeToLinux(&gws, &ws);
    if (CopyToUserWrite(m, addr, &gws, sizeof(gws)) == -1) rc = -1;
  }
  return rc;
}

static int IoctlTiocswinsz(struct Machine *m, int fd, i64 addr,
                           int fn(int, const struct winsize *)) {
  struct winsize ws;
  struct winsize_linux gws;
  if (CopyFromUserRead(m, &gws, addr, sizeof(gws)) == -1) return -1;
  XlatWinsizeToHost(&ws, &gws);
  return fn(fd, &ws);
}

static int IoctlTcgets(struct Machine *m, int fd, i64 addr,
                       int fn(int, struct termios *)) {
  int rc;
  struct termios tio;
  struct termios_linux gtio;
  if ((rc = fn(fd, &tio)) != -1) {
    XlatTermiosToLinux(&gtio, &tio);
    if (CopyToUserWrite(m, addr, &gtio, sizeof(gtio)) == -1) rc = -1;
  }
  return rc;
}

static int IoctlTcsets(struct Machine *m, int fd, int request, i64 addr,
                       int fn(int, int, const struct termios *)) {
  struct termios tio;
  struct termios_linux gtio;
  if (CopyFromUserRead(m, &gtio, addr, sizeof(gtio)) == -1) return -1;
  XlatLinuxToTermios(&tio, &gtio);
  return fn(fd, request, &tio);
}

static int IoctlTiocgpgrp(struct Machine *m, int fd, i64 addr) {
  int rc;
  u8 *pgrp;
#ifdef __EMSCRIPTEN__
  // Force shells to disable job control in emscripten
  errno = ENOTTY;
  return -1;
#endif
  if (!(pgrp = (u8 *)SchlepW(m, addr, 4))) return -1;
  if ((rc = tcgetpgrp(fd)) == -1) return -1;
  Write32(pgrp, rc);
  return 0;
}

static int IoctlTiocspgrp(struct Machine *m, int fd, i64 addr) {
  u8 *pgrp;
  if (!(pgrp = (u8 *)SchlepR(m, addr, 4))) return -1;
  return tcsetpgrp(fd, Read32(pgrp));
}

#ifdef HAVE_SIOCGIFCONF

static int IoctlSiocgifconf(struct Machine *m, int systemfd, i64 ifconf_addr) {
  size_t i;
  char *buf;
  size_t len;
  size_t bufsize;
  char *buf_linux;
  size_t len_linux;
  struct ifreq *ifreq;
  struct ifconf ifconf;
  struct ifreq_linux ifreq_linux;
  struct ifconf_linux ifconf_linux;
  const struct ifconf_linux *ifconf_linuxp;
  memset(&ifreq_linux, 0, sizeof(ifreq_linux));
  if (!(ifconf_linuxp = (const struct ifconf_linux *)SchlepRW(
            m, ifconf_addr, sizeof(*ifconf_linuxp))) ||
      !IsValidMemory(m, Read64(ifconf_linuxp->buf), Read32(ifconf_linuxp->len),
                     PROT_WRITE)) {
    return efault();
  }
  bufsize = MIN(16384, Read32(ifconf_linuxp->len));
  if (!(buf = (char *)AddToFreeList(m, malloc(bufsize)))) return -1;
  if (!(buf_linux = (char *)AddToFreeList(m, malloc(bufsize)))) return -1;
  ifconf.ifc_len = bufsize;
  ifconf.ifc_buf = buf;
  if (ioctl(systemfd, SIOCGIFCONF, &ifconf)) return -1;
  len_linux = 0;
  ifreq = ifconf.ifc_req;
  for (i = 0; i < ifconf.ifc_len;) {
    if (len_linux + sizeof(ifreq_linux) > bufsize) break;
#ifdef HAVE_SA_LEN
    len = IFNAMSIZ + ifreq->ifr_addr.sa_len;
#else
    len = sizeof(*ifreq);
#endif
    if (ifreq->ifr_addr.sa_family == AF_INET) {
      memset(ifreq_linux.name, 0, sizeof(ifreq_linux.name));
      memcpy(ifreq_linux.name, ifreq->ifr_name,
             MIN(sizeof(ifreq_linux.name) - 1, sizeof(ifreq->ifr_name)));
      unassert(XlatSockaddrToLinux(
                   (struct sockaddr_storage_linux *)&ifreq_linux.addr,
                   (const struct sockaddr *)&ifreq->ifr_addr,
                   sizeof(ifreq->ifr_addr)) ==
               sizeof(struct sockaddr_in_linux));
      memcpy(buf_linux + len_linux, &ifreq_linux, sizeof(ifreq_linux));
      len_linux += sizeof(ifreq_linux);
    }
    ifreq = (struct ifreq *)((char *)ifreq + len);
    i += len;
  }
  Write32(ifconf_linux.len, len_linux);
  Write32(ifconf_linux.pad, 0);
  Write64(ifconf_linux.buf, Read64(ifconf_linuxp->buf));
  CopyToUserWrite(m, Read64(ifconf_linux.buf), buf_linux, len_linux);
  CopyToUserWrite(m, ifconf_addr, &ifconf_linux, sizeof(ifconf_linux));
  return 0;
}

static int IoctlSiocgifaddr(struct Machine *m, int systemfd, i64 ifreq_addr,
                            unsigned long kind) {
  struct ifreq ifreq;
  struct ifreq_linux ifreq_linux;
  if (!IsValidMemory(m, ifreq_addr, sizeof(ifreq_linux),
                     PROT_READ | PROT_WRITE)) {
    return efault();
  }
  CopyFromUserRead(m, &ifreq_linux, ifreq_addr, sizeof(ifreq_linux));
  memset(ifreq.ifr_name, 0, sizeof(ifreq.ifr_name));
  memcpy(ifreq.ifr_name, ifreq_linux.name,
         MIN(sizeof(ifreq_linux.name) - 1, sizeof(ifreq.ifr_name)));
  if (Read16(ifreq_linux.addr.family) != AF_INET_LINUX) return einval();
  unassert(XlatSockaddrToHost((struct sockaddr_storage *)&ifreq.ifr_addr,
                              (const struct sockaddr_linux *)&ifreq_linux.addr,
                              sizeof(struct sockaddr_in_linux)) ==
           sizeof(struct sockaddr_in));
  if (ioctl(systemfd, kind, &ifreq)) return -1;
  memset(ifreq_linux.name, 0, sizeof(ifreq_linux.name));
  memcpy(ifreq_linux.name, ifreq.ifr_name,
         MIN(sizeof(ifreq_linux.name) - 1, sizeof(ifreq.ifr_name)));
  unassert(XlatSockaddrToLinux(
               (struct sockaddr_storage_linux *)&ifreq_linux.addr,
               (struct sockaddr *)&ifreq.ifr_addr,
               sizeof(ifreq.ifr_addr)) == sizeof(struct sockaddr_in_linux));
  CopyToUserWrite(m, ifreq_addr, &ifreq_linux, sizeof(ifreq_linux));
  return 0;
}

#endif /* HAVE_SIOCGIFCONF */

static int IoctlFionbio(struct Machine *m, int fildes) {
  int oflags;
  if ((oflags = GetOflags(m, fildes)) == -1) return -1;
  return fcntl(fildes, F_SETFL, (oflags & SETFL_FLAGS) | O_NDELAY);
}

static int IoctlFioclex(struct Machine *m, int fildes) {
  return fcntl(fildes, F_SETFD, FD_CLOEXEC);
}

static int IoctlFionclex(struct Machine *m, int fildes) {
  return fcntl(fildes, F_SETFD, 0);
}

static int IoctlTcsbrk(struct Machine *m, int fildes, int drain) {
  int rc;
  if (drain) {
    RESTARTABLE(rc = tcdrain(fildes));
  } else {
    rc = tcsendbreak(fildes, 0);
  }
  return rc;
}

static int IoctlTcxonc(struct Machine *m, int fildes, int arg) {
  return tcflow(fildes, arg);
}

static int IoctlTiocgsid(struct Machine *m, int fildes, i64 addr) {
  int rc;
  u8 *sid;
  if (!(sid = (u8 *)SchlepW(m, addr, 4))) return -1;
  if ((rc = tcgetsid(fildes)) != -1) {
    Write32(sid, rc);
    rc = 0;
  }
  return rc;
}

static int XlatFlushQueue(int queue) {
  switch (queue) {
    case TCIFLUSH_LINUX:
      return TCIFLUSH;
    case TCOFLUSH_LINUX:
      return TCOFLUSH;
    case TCIOFLUSH_LINUX:
      return TCIOFLUSH;
    default:
      return einval();
  }
}

static int IoctlTcflsh(struct Machine *m, int fildes, int queue) {
  if ((queue = XlatFlushQueue(queue)) == -1) return -1;
  return tcflush(fildes, queue);
}

static int IoctlSiocatmark(struct Machine *m, int fildes, i64 addr) {
  u8 *p;
  int rc;
  if (!(p = (u8 *)SchlepW(m, addr, 4))) return -1;
  if ((rc = sockatmark(fildes)) != -1) {
    Write32(p, rc);
    rc = 0;
  }
  return rc;
}

static int IoctlGetInt32(struct Machine *m, int fildes, unsigned long cmd,
                         i64 addr) {
  u8 *p;
  int rc, val;
  if (!(p = (u8 *)SchlepW(m, addr, 4))) return -1;
  if ((rc = ioctl(fildes, cmd, &val)) != -1) {
    Write32(p, val);
  }
  return rc;
}

static int IoctlSetInt32(struct Machine *m, int fildes, unsigned long cmd,
                         i64 addr) {
  int val;
  const u8 *p;
  if (!(p = (const u8 *)SchlepR(m, addr, 4))) return -1;
  val = Read32(p);
  return ioctl(fildes, cmd, &val);
}

#ifdef TIOCSTI
static int IoctlTiocsti(struct Machine *m, int fildes, i64 addr) {
  const u8 *bytep;
  if (!(bytep = LookupAddress(m, addr))) return efault();
  return ioctl(fildes, TIOCSTI, bytep);
}
#endif

int SysIoctl(struct Machine *m, int fildes, u64 request, i64 addr) {
  struct Fd *fd;
  int (*tcgetattr_impl)(int, struct termios *);
  int (*tcsetattr_impl)(int, int, const struct termios *);
  int (*tcgetwinsize_impl)(int, struct winsize *);
  int (*tcsetwinsize_impl)(int, const struct winsize *);
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(tcgetattr_impl = fd->cb->tcgetattr);
    unassert(tcsetattr_impl = fd->cb->tcsetattr);
    unassert(tcgetwinsize_impl = fd->cb->tcgetwinsize);
    unassert(tcsetwinsize_impl = fd->cb->tcsetwinsize);
  } else {
    tcsetattr_impl = 0;
    tcgetattr_impl = 0;
    tcgetwinsize_impl = 0;
    tcsetwinsize_impl = 0;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return -1;
  switch (request) {
    case TIOCGWINSZ_LINUX:
      return IoctlTiocgwinsz(m, fildes, addr, tcgetwinsize_impl);
    case TIOCSWINSZ_LINUX:
      return IoctlTiocswinsz(m, fildes, addr, tcsetwinsize_impl);
    case TCGETS_LINUX:
      return IoctlTcgets(m, fildes, addr, tcgetattr_impl);
    case TCSETS_LINUX:
      return IoctlTcsets(m, fildes, TCSANOW, addr, tcsetattr_impl);
    case TCSETSW_LINUX:
      return IoctlTcsets(m, fildes, TCSADRAIN, addr, tcsetattr_impl);
    case TCSETSF_LINUX:
      return IoctlTcsets(m, fildes, TCSAFLUSH, addr, tcsetattr_impl);
    case TIOCGPGRP_LINUX:
      return IoctlTiocgpgrp(m, fildes, addr);
    case TIOCSPGRP_LINUX:
      return IoctlTiocspgrp(m, fildes, addr);
    case FIONBIO_LINUX:
      return IoctlFionbio(m, fildes);
    case FIOCLEX_LINUX:
      return IoctlFioclex(m, fildes);
    case FIONCLEX_LINUX:
      return IoctlFionclex(m, fildes);
    case TCSBRK_LINUX:
      return IoctlTcsbrk(m, fildes, addr);
    case TCXONC_LINUX:
      return IoctlTcxonc(m, fildes, addr);
    case TIOCGSID_LINUX:
      return IoctlTiocgsid(m, fildes, addr);
    case TCFLSH_LINUX:
      return IoctlTcflsh(m, fildes, addr);
    case SIOCATMARK_LINUX:
      return IoctlSiocatmark(m, fildes, addr);
#ifdef FIONREAD
    case FIONREAD_LINUX:
      return IoctlGetInt32(m, fildes, FIONREAD, addr);
#endif
#ifdef TIOCOUTQ
    case TIOCOUTQ_LINUX:
      return IoctlGetInt32(m, fildes, TIOCOUTQ, addr);
#endif
#ifdef TIOCSTI
    case TIOCSTI_LINUX:
      return IoctlTiocsti(m, fildes, addr);
#endif
#ifdef FIOGETOWN
    case FIOGETOWN_LINUX:
      return IoctlGetInt32(m, fildes, FIOGETOWN, addr);
#endif
#ifdef FIOSETOWN
    case FIOSETOWN_LINUX:
      return IoctlSetInt32(m, fildes, FIOSETOWN, addr);
#endif
#ifdef SIOCSPGRP
    case SIOCSPGRP_LINUX:
      return IoctlSetInt32(m, fildes, SIOCSPGRP, addr);
#endif
#ifdef SIOCGPGRP
    case SIOCGPGRP_LINUX:
      return IoctlGetInt32(m, fildes, SIOCGPGRP, addr);
#endif
#ifdef HAVE_SIOCGIFCONF
    case SIOCGIFCONF_LINUX:
      return IoctlSiocgifconf(m, fildes, addr);
    case SIOCGIFADDR_LINUX:
      return IoctlSiocgifaddr(m, fildes, addr, SIOCGIFADDR);
    case SIOCGIFNETMASK_LINUX:
      return IoctlSiocgifaddr(m, fildes, addr, SIOCGIFNETMASK);
    case SIOCGIFBRDADDR_LINUX:
      return IoctlSiocgifaddr(m, fildes, addr, SIOCGIFBRDADDR);
    case SIOCGIFDSTADDR_LINUX:
      return IoctlSiocgifaddr(m, fildes, addr, SIOCGIFDSTADDR);
#endif
    default:
      LOGF("missing ioctl %#" PRIx64, request);
      return einval();
  }
}
