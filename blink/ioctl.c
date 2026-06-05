/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
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
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/ndelay.h"
#include "blink/syscall.h"
#include "blink/thread.h"
#include "blink/types.h"
#include "blink/vfs.h"
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
  if ((rc = VfsTcgetpgrp(fd)) == -1) return -1;
  Write32(pgrp, rc);
  return 0;
}

static int IoctlTiocspgrp(struct Machine *m, int fd, i64 addr) {
  u8 *pgrp;
  if (!(pgrp = (u8 *)SchlepR(m, addr, 4))) return -1;
  return VfsTcsetpgrp(fd, Read32(pgrp));
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
  if (VfsIoctl(systemfd, SIOCGIFCONF, &ifconf)) return -1;
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
                   sizeof(ifreq->ifr_addr),
                   m->system->isfreebsd) ==
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
                              sizeof(struct sockaddr_in_linux),
                              m->system->isfreebsd) ==
           sizeof(struct sockaddr_in));
  if (VfsIoctl(systemfd, kind, &ifreq)) return -1;
  memset(ifreq_linux.name, 0, sizeof(ifreq_linux.name));
  memcpy(ifreq_linux.name, ifreq.ifr_name,
         MIN(sizeof(ifreq_linux.name) - 1, sizeof(ifreq.ifr_name)));
  unassert(XlatSockaddrToLinux(
               (struct sockaddr_storage_linux *)&ifreq_linux.addr,
               (struct sockaddr *)&ifreq.ifr_addr,
               sizeof(ifreq.ifr_addr),
               m->system->isfreebsd) == sizeof(struct sockaddr_in_linux));
  CopyToUserWrite(m, ifreq_addr, &ifreq_linux, sizeof(ifreq_linux));
  return 0;
}

#endif /* HAVE_SIOCGIFCONF */

static int IoctlFionbio(struct Machine *m, int fildes) {
  int oflags;
  if ((oflags = GetOflags(m, fildes)) == -1) return -1;
  return VfsFcntl(fildes, F_SETFL, (oflags & SETFL_FLAGS) | O_NDELAY);
}

static int IoctlFioclex(struct Machine *m, int fildes) {
  return VfsFcntl(fildes, F_SETFD, FD_CLOEXEC);
}

static int IoctlFionclex(struct Machine *m, int fildes) {
  return VfsFcntl(fildes, F_SETFD, 0);
}

static int IoctlTcsbrk(struct Machine *m, int fildes, int drain) {
  int rc;
  if (drain) {
    RESTARTABLE(rc = VfsTcdrain(fildes));
  } else {
    rc = VfsTcsendbreak(fildes, 0);
  }
  return rc;
}

static int IoctlTcxonc(struct Machine *m, int fildes, int arg) {
  return VfsTcflow(fildes, arg);
}

static int IoctlTiocgsid(struct Machine *m, int fildes, i64 addr) {
  int rc;
  u8 *sid;
  if (!(sid = (u8 *)SchlepW(m, addr, 4))) return -1;
  if ((rc = VfsTcgetsid(fildes)) != -1) {
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
  return VfsTcflush(fildes, queue);
}

#ifdef HAVE_SOCKATMARK
static int IoctlSiocatmark(struct Machine *m, int fildes, i64 addr) {
  u8 *p;
  int rc;
  if (!(p = (u8 *)SchlepW(m, addr, 4))) return -1;
  if ((rc = VfsSockatmark(fildes)) != -1) {
    Write32(p, rc);
    rc = 0;
  }
  return rc;
}
#endif

static int IoctlGetInt32(struct Machine *m, int fildes, unsigned long cmd,
                         i64 addr) {
  u8 *p;
  int rc, val;
  if (!(p = (u8 *)SchlepW(m, addr, 4))) return -1;
  if ((rc = VfsIoctl(fildes, cmd, &val)) != -1) {
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
  return VfsIoctl(fildes, cmd, &val);
}

#ifdef TIOCSTI
static int IoctlTiocsti(struct Machine *m, int fildes, i64 addr) {
  const u8 *bytep;
  if (!(bytep = LookupAddress(m, addr))) return efault();
  return VfsIoctl(fildes, TIOCSTI, (void *)bytep);
}
#endif

// FreeBSD struct termios (44 bytes) - different layout from Linux's 36 bytes.
// FreeBSD: [iflag:4][oflag:4][cflag:4][lflag:4][cc:20][ispeed:4][ospeed:4]
// Linux:   [iflag:4][oflag:4][cflag:4][lflag:4][line:1][cc:19]
struct fbsd_termios {
  u32 c_iflag;
  u32 c_oflag;
  u32 c_cflag;
  u32 c_lflag;
  u8  c_cc[20];
  u32 c_ispeed;
  u32 c_ospeed;
};

// FreeBSD iflag bits (differ from Linux: IXON=0x200 vs Linux=0x400)
#define FBSD_IXON     0x00000200u
#define FBSD_IXOFF    0x00000400u
// FreeBSD oflag bits (ONLCR=0x2 vs Linux=0x4)
#define FBSD_OPOST    0x00000001u
#define FBSD_ONLCR    0x00000002u
#define FBSD_OCRNL    0x00000010u
#define FBSD_ONOCR    0x00000020u
#define FBSD_ONLRET   0x00000040u
// FreeBSD cflag bits (CSIZE at bits 8-9 vs Linux bits 4-5)
#define FBSD_CSIZE    0x00000300u
#define FBSD_CS8      0x00000300u
#define FBSD_CS7      0x00000200u
#define FBSD_CS6      0x00000100u
#define FBSD_CS5      0x00000000u
#define FBSD_CSTOPB   0x00000400u
#define FBSD_CREAD    0x00000800u
#define FBSD_PARENB   0x00001000u
#define FBSD_PARODD   0x00002000u
#define FBSD_HUPCL    0x00004000u
#define FBSD_CLOCAL   0x00008000u
// FreeBSD lflag bits (ISIG=0x80 vs Linux=0x1, ICANON=0x100 vs Linux=0x2)
#define FBSD_ECHOKE   0x00000001u
#define FBSD_ECHOE    0x00000002u
#define FBSD_ECHOK    0x00000004u
#define FBSD_ECHO     0x00000008u
#define FBSD_ECHONL   0x00000010u
#define FBSD_ECHOPRT  0x00000020u
#define FBSD_ECHOCTL  0x00000040u
#define FBSD_ISIG     0x00000080u
#define FBSD_ICANON   0x00000100u
#define FBSD_IEXTEN   0x00000400u
#define FBSD_TOSTOP   0x00400000u
#define FBSD_FLUSHO   0x00800000u
#define FBSD_PENDIN   0x20000000u
#define FBSD_NOFLSH   0x80000000u
// FreeBSD c_cc indices (NCCS=20)
#define FBSD_VEOF     0
#define FBSD_VEOL     1
#define FBSD_VEOL2    2
#define FBSD_VERASE   3
#define FBSD_VWERASE  4
#define FBSD_VKILL    5
#define FBSD_VREPRINT 6
#define FBSD_VDISCARD 7
#define FBSD_VMIN     8
#define FBSD_VTIME    9
#define FBSD_VSTATUS  10
#define FBSD_VSUSP    11
#define FBSD_VDSUSP   12
#define FBSD_VSTART   13
#define FBSD_VSTOP    14
#define FBSD_VLNEXT   15
#define FBSD_VINTR    16
#define FBSD_VQUIT    17
// FreeBSD terminal ioctl numbers
#define FBSD_TIOCGETA   0x402c7413u
#define FBSD_TIOCSETA   0x802c7414u
#define FBSD_TIOCSETAW  0x802c7415u
#define FBSD_TIOCSETAF  0x802c7416u
#define FBSD_TIOCGWINSZ 0x40087468u
#define FBSD_TIOCSWINSZ 0x80087467u
#define FBSD_TIOCGPGRP  0x40047477u
#define FBSD_TIOCSPGRP  0x80047476u
#define FBSD_TIOCDRAIN  0x2000745eu
#define FBSD_TIOCFLUSH  0x80047410u
#define FBSD_TIOCGETD   0x4004741au  // get line discipline
#define FBSD_TIOCSETD   0x8004741bu  // set line discipline

static u32 LinuxToFreeBSDIflag(u32 x) {
  // Most bits identical; IXON and IXOFF differ
  u32 r = x & ~(IXON_LINUX | IXOFF_LINUX | IXANY_LINUX);
  if (x & IXON_LINUX)  r |= FBSD_IXON;
  if (x & IXOFF_LINUX) r |= FBSD_IXOFF;
  if (x & IXANY_LINUX) r |= 0x00000800u;  // FreeBSD IXANY = Linux IXANY
  return r;
}

static u32 FreeBSDToLinuxIflag(u32 x) {
  u32 r = x & ~(FBSD_IXON | FBSD_IXOFF | 0x00000800u);
  if (x & FBSD_IXON)      r |= IXON_LINUX;
  if (x & FBSD_IXOFF)     r |= IXOFF_LINUX;
  if (x & 0x00000800u)    r |= IXANY_LINUX;
  return r;
}

static u32 LinuxToFreeBSDOflag(u32 x) {
  u32 r = 0;
  if (x & OPOST_LINUX)  r |= FBSD_OPOST;
  if (x & ONLCR_LINUX)  r |= FBSD_ONLCR;
  if (x & OCRNL_LINUX)  r |= FBSD_OCRNL;
  if (x & ONOCR_LINUX)  r |= FBSD_ONOCR;
  if (x & ONLRET_LINUX) r |= FBSD_ONLRET;
  return r;
}

static u32 FreeBSDToLinuxOflag(u32 x) {
  u32 r = 0;
  if (x & FBSD_OPOST)  r |= OPOST_LINUX;
  if (x & FBSD_ONLCR)  r |= ONLCR_LINUX;
  if (x & FBSD_OCRNL)  r |= OCRNL_LINUX;
  if (x & FBSD_ONOCR)  r |= ONOCR_LINUX;
  if (x & FBSD_ONLRET) r |= ONLRET_LINUX;
  return r;
}

static u32 LinuxToFreeBSDCflag(u32 x) {
  u32 r = 0;
  switch (x & CSIZE_LINUX) {
    case CS5_LINUX: r |= FBSD_CS5; break;
    case CS6_LINUX: r |= FBSD_CS6; break;
    case CS7_LINUX: r |= FBSD_CS7; break;
    default:        r |= FBSD_CS8; break;
  }
  if (x & CSTOPB_LINUX) r |= FBSD_CSTOPB;
  if (x & CREAD_LINUX)  r |= FBSD_CREAD;
  if (x & PARENB_LINUX) r |= FBSD_PARENB;
  if (x & PARODD_LINUX) r |= FBSD_PARODD;
  if (x & HUPCL_LINUX)  r |= FBSD_HUPCL;
  if (x & CLOCAL_LINUX) r |= FBSD_CLOCAL;
  return r;
}

static u32 FreeBSDToLinuxCflag(u32 x) {
  u32 r = 0;
  switch (x & FBSD_CSIZE) {
    case FBSD_CS5: r |= CS5_LINUX; break;
    case FBSD_CS6: r |= CS6_LINUX; break;
    case FBSD_CS7: r |= CS7_LINUX; break;
    default:       r |= CS8_LINUX; break;
  }
  if (x & FBSD_CSTOPB) r |= CSTOPB_LINUX;
  if (x & FBSD_CREAD)  r |= CREAD_LINUX;
  if (x & FBSD_PARENB) r |= PARENB_LINUX;
  if (x & FBSD_PARODD) r |= PARODD_LINUX;
  if (x & FBSD_HUPCL)  r |= HUPCL_LINUX;
  if (x & FBSD_CLOCAL) r |= CLOCAL_LINUX;
  return r;
}

static u32 LinuxToFreeBSDLflag(u32 x) {
  u32 r = 0;
  if (x & ISIG_LINUX)    r |= FBSD_ISIG;
  if (x & ICANON_LINUX)  r |= FBSD_ICANON;
  if (x & ECHO_LINUX)    r |= FBSD_ECHO;
  if (x & ECHOE_LINUX)   r |= FBSD_ECHOE;
  if (x & ECHOK_LINUX)   r |= FBSD_ECHOK;
  if (x & ECHONL_LINUX)  r |= FBSD_ECHONL;
  if (x & NOFLSH_LINUX)  r |= FBSD_NOFLSH;
  if (x & TOSTOP_LINUX)  r |= FBSD_TOSTOP;
  if (x & IEXTEN_LINUX)  r |= FBSD_IEXTEN;
  if (x & ECHOCTL_LINUX) r |= FBSD_ECHOCTL;
  if (x & ECHOPRT_LINUX) r |= FBSD_ECHOPRT;
  if (x & ECHOKE_LINUX)  r |= FBSD_ECHOKE;
  if (x & FLUSHO_LINUX)  r |= FBSD_FLUSHO;
  if (x & PENDIN_LINUX)  r |= FBSD_PENDIN;
  return r;
}

static u32 FreeBSDToLinuxLflag(u32 x) {
  u32 r = 0;
  if (x & FBSD_ISIG)    r |= ISIG_LINUX;
  if (x & FBSD_ICANON)  r |= ICANON_LINUX;
  if (x & FBSD_ECHO)    r |= ECHO_LINUX;
  if (x & FBSD_ECHOE)   r |= ECHOE_LINUX;
  if (x & FBSD_ECHOK)   r |= ECHOK_LINUX;
  if (x & FBSD_ECHONL)  r |= ECHONL_LINUX;
  if (x & FBSD_NOFLSH)  r |= NOFLSH_LINUX;
  if (x & FBSD_TOSTOP)  r |= TOSTOP_LINUX;
  if (x & FBSD_IEXTEN)  r |= IEXTEN_LINUX;
  if (x & FBSD_ECHOCTL) r |= ECHOCTL_LINUX;
  if (x & FBSD_ECHOPRT) r |= ECHOPRT_LINUX;
  if (x & FBSD_ECHOKE)  r |= ECHOKE_LINUX;
  if (x & FBSD_FLUSHO)  r |= FLUSHO_LINUX;
  if (x & FBSD_PENDIN)  r |= PENDIN_LINUX;
  return r;
}

// TIOCGETA: get terminal attrs and write FreeBSD-format termios to guest
static int IoctlTiocgeta(struct Machine *m, int fildes, i64 addr,
                         int fn(int, struct termios *)) {
  int rc;
  struct termios tio;
  struct termios_linux ltio;
  struct fbsd_termios ftio;
  if ((rc = fn(fildes, &tio)) != -1) {
    XlatTermiosToLinux(&ltio, &tio);
    memset(&ftio, 0, sizeof(ftio));
    ftio.c_iflag  = LinuxToFreeBSDIflag(Read32(ltio.iflag));
    ftio.c_oflag  = LinuxToFreeBSDOflag(Read32(ltio.oflag));
    ftio.c_cflag  = LinuxToFreeBSDCflag(Read32(ltio.cflag));
    ftio.c_lflag  = LinuxToFreeBSDLflag(Read32(ltio.lflag));
    ftio.c_ispeed = cfgetispeed(&tio);
    ftio.c_ospeed = cfgetospeed(&tio);
    ftio.c_cc[FBSD_VINTR]    = ltio.cc[VINTR_LINUX];
    ftio.c_cc[FBSD_VQUIT]    = ltio.cc[VQUIT_LINUX];
    ftio.c_cc[FBSD_VERASE]   = ltio.cc[VERASE_LINUX];
    ftio.c_cc[FBSD_VKILL]    = ltio.cc[VKILL_LINUX];
    ftio.c_cc[FBSD_VEOF]     = ltio.cc[VEOF_LINUX];
    ftio.c_cc[FBSD_VTIME]    = ltio.cc[VTIME_LINUX];
    ftio.c_cc[FBSD_VMIN]     = ltio.cc[VMIN_LINUX];
    ftio.c_cc[FBSD_VSTATUS]  = ltio.cc[VSWTC_LINUX];
    ftio.c_cc[FBSD_VSTART]   = ltio.cc[VSTART_LINUX];
    ftio.c_cc[FBSD_VSTOP]    = ltio.cc[VSTOP_LINUX];
    ftio.c_cc[FBSD_VSUSP]    = ltio.cc[VSUSP_LINUX];
    ftio.c_cc[FBSD_VEOL]     = ltio.cc[VEOL_LINUX];
    ftio.c_cc[FBSD_VREPRINT] = ltio.cc[VREPRINT_LINUX];
    ftio.c_cc[FBSD_VDISCARD] = ltio.cc[VDISCARD_LINUX];
    ftio.c_cc[FBSD_VWERASE]  = ltio.cc[VWERASE_LINUX];
    ftio.c_cc[FBSD_VLNEXT]   = ltio.cc[VLNEXT_LINUX];
    ftio.c_cc[FBSD_VEOL2]    = ltio.cc[VEOL2_LINUX];
    if (CopyToUserWrite(m, addr, &ftio, sizeof(ftio)) == -1) rc = -1;
  }
  return rc;
}

// TIOCSETA/TIOCSETAW/TIOCSETAF: read FreeBSD-format termios from guest and apply
static int IoctlTiocseta(struct Machine *m, int fildes, int how, i64 addr,
                         int fn(int, int, const struct termios *)) {
  struct termios tio;
  struct termios_linux ltio;
  struct fbsd_termios ftio;
  if (CopyFromUserRead(m, &ftio, addr, sizeof(ftio)) == -1) return -1;
  memset(&ltio, 0, sizeof(ltio));
  Write32(ltio.iflag, FreeBSDToLinuxIflag(ftio.c_iflag));
  Write32(ltio.oflag, FreeBSDToLinuxOflag(ftio.c_oflag));
  Write32(ltio.cflag, FreeBSDToLinuxCflag(ftio.c_cflag));
  Write32(ltio.lflag, FreeBSDToLinuxLflag(ftio.c_lflag));
  ltio.cc[VINTR_LINUX]    = ftio.c_cc[FBSD_VINTR];
  ltio.cc[VQUIT_LINUX]    = ftio.c_cc[FBSD_VQUIT];
  ltio.cc[VERASE_LINUX]   = ftio.c_cc[FBSD_VERASE];
  ltio.cc[VKILL_LINUX]    = ftio.c_cc[FBSD_VKILL];
  ltio.cc[VEOF_LINUX]     = ftio.c_cc[FBSD_VEOF];
  ltio.cc[VTIME_LINUX]    = ftio.c_cc[FBSD_VTIME];
  ltio.cc[VMIN_LINUX]     = ftio.c_cc[FBSD_VMIN];
  ltio.cc[VSWTC_LINUX]    = ftio.c_cc[FBSD_VSTATUS];
  ltio.cc[VSTART_LINUX]   = ftio.c_cc[FBSD_VSTART];
  ltio.cc[VSTOP_LINUX]    = ftio.c_cc[FBSD_VSTOP];
  ltio.cc[VSUSP_LINUX]    = ftio.c_cc[FBSD_VSUSP];
  ltio.cc[VEOL_LINUX]     = ftio.c_cc[FBSD_VEOL];
  ltio.cc[VREPRINT_LINUX] = ftio.c_cc[FBSD_VREPRINT];
  ltio.cc[VDISCARD_LINUX] = ftio.c_cc[FBSD_VDISCARD];
  ltio.cc[VWERASE_LINUX]  = ftio.c_cc[FBSD_VWERASE];
  ltio.cc[VLNEXT_LINUX]   = ftio.c_cc[FBSD_VLNEXT];
  ltio.cc[VEOL2_LINUX]    = ftio.c_cc[FBSD_VEOL2];
  XlatLinuxToTermios(&tio, &ltio);
  if (ftio.c_ospeed) cfsetospeed(&tio, ftio.c_ospeed);
  if (ftio.c_ispeed) cfsetispeed(&tio, ftio.c_ispeed);
  return fn(fildes, how, &tio);
}

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
#ifndef DISABLE_NONPOSIX
    case FIONBIO_LINUX:
      return IoctlFionbio(m, fildes);
    case FIOCLEX_LINUX:
      return IoctlFioclex(m, fildes);
    case FIONCLEX_LINUX:
      return IoctlFionclex(m, fildes);
#endif
    case TCSBRK_LINUX:
      return IoctlTcsbrk(m, fildes, addr);
    case TCXONC_LINUX:
      return IoctlTcxonc(m, fildes, addr);
    case TIOCGSID_LINUX:
      return IoctlTiocgsid(m, fildes, addr);
    case TCFLSH_LINUX:
      return IoctlTcflsh(m, fildes, addr);
#ifdef HAVE_SOCKATMARK
#ifndef DISABLE_SOCKETS
    case SIOCATMARK_LINUX:
      return IoctlSiocatmark(m, fildes, addr);
#endif
#endif
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
#ifndef DISABLE_SOCKETS
#ifndef DISABLE_NONPOSIX
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
#endif /* DISABLE_NONPOSIX */
#endif /* DISABLE_SOCKETS */
#endif /* HAVE_SIOCGIFCONF */
    case FBSD_TIOCGETA:
      return IoctlTiocgeta(m, fildes, addr, tcgetattr_impl);
    case FBSD_TIOCSETA:
      return IoctlTiocseta(m, fildes, TCSANOW, addr, tcsetattr_impl);
    case FBSD_TIOCSETAW:
      return IoctlTiocseta(m, fildes, TCSADRAIN, addr, tcsetattr_impl);
    case FBSD_TIOCSETAF:
      return IoctlTiocseta(m, fildes, TCSAFLUSH, addr, tcsetattr_impl);
    case FBSD_TIOCGWINSZ:
      return IoctlTiocgwinsz(m, fildes, addr, tcgetwinsize_impl);
    case FBSD_TIOCSWINSZ:
      return IoctlTiocswinsz(m, fildes, addr, tcsetwinsize_impl);
    case FBSD_TIOCGPGRP:
      return IoctlTiocgpgrp(m, fildes, addr);
    case FBSD_TIOCSPGRP:
      return IoctlTiocspgrp(m, fildes, addr);
    case FBSD_TIOCGETD:  // line discipline number (TTYDISC/N_TTY=0 for a tty)
      return IoctlGetInt32(m, fildes, TIOCGETD, addr);
#ifdef TIOCSETD
    case FBSD_TIOCSETD:
      return IoctlSetInt32(m, fildes, TIOCSETD, addr);
#endif
    // NB: TIOCSCTTY is deliberately NOT implemented. Making it succeed lets a
    // shell believe it has job control, but blink can't deliver the rest of the
    // job-control machinery (process-group SIGSTOP/SIGCONT + waitpid(WUNTRACED)
    // across guest processes), so programs that then drive job control deadlock
    // -- e.g. mc's concurrent subshell hangs forever once its sync succeeds.
    // Leaving TIOCSCTTY to fail (default einval) keeps shells in the honest
    // "no job control" mode, where mc's subshell works (no command-line cwd
    // tracking, but no hang). Revisit if real job control is implemented.
    case FBSD_TIOCDRAIN:
      return VfsIoctl(fildes, TCSBRK, (void *)1L);
    case FBSD_TIOCFLUSH:
      return 0;
    case 0x8004667eu: {  // FreeBSD FIONBIO
      const u8 *p;
      int val = 1;
      if (addr && (p = (const u8 *)SchlepR(m, addr, 4))) {
        val = Read32(p);
      }
      if (val) {
        return IoctlFionbio(m, fildes);
      } else {
        // Clear non-blocking
        int oflags;
        if ((oflags = GetOflags(m, fildes)) == -1) return -1;
        return VfsFcntl(fildes, F_SETFL, (oflags & SETFL_FLAGS) & ~O_NDELAY);
      }
    }
    case 0x8004667du: {  // FreeBSD FIOASYNC: enable/disable SIGIO via O_ASYNC.
      // nginx sets this on its master<->worker channel socket while spawning
      // workers and treats failure as fatal, so it must succeed.
      const u8 *p;
      int val = 0;
      int oflags;
      if (addr && (p = (const u8 *)SchlepR(m, addr, 4))) val = Read32(p);
      if ((oflags = GetOflags(m, fildes)) == -1) return -1;
      if (val) {
        return VfsFcntl(fildes, F_SETFL, (oflags & SETFL_FLAGS) | O_ASYNC_SETFL);
      } else {
        return VfsFcntl(fildes, F_SETFL,
                        (oflags & SETFL_FLAGS) & ~O_ASYNC_SETFL);
      }
    }
    case 0x20006601u:  // FreeBSD FIOCLEX
      return IoctlFioclex(m, fildes);
    case 0x20006602u:  // FreeBSD FIONCLEX
      return IoctlFionclex(m, fildes);
    case 0x4004667fu:  // FreeBSD FIONREAD
      return IoctlGetInt32(m, fildes, FIONREAD, addr);
    case 0x2000741cu:  // FreeBSD TIOCPTMASTER — grantpt(3) probes this.
      // Returns 0 on master end; ENOTTY otherwise. We know any fd we handed
      // back from posix_openpt is a master; trust it and succeed.
      (void)m; (void)addr;
      return 0;
    case 0x80106678u: {  // FreeBSD FIODGNAME — powers ptsname(3) on FreeBSD
      // struct fiodgname_arg { int len; void *buf; }  (16 bytes total: 4 len,
      // 4 pad, 8 buf-ptr on x86-64).
      const u8 *gp;
      i32 buflen;
      i64 bufaddr;
      char hname[256];
      size_t hlen;
      if (!(gp = (const u8 *)SchlepR(m, addr, 16))) return -1;
      buflen = Read32(gp);
      bufaddr = Read64(gp + 8);
      if (buflen <= 0 || buflen > (i32)sizeof(hname)) return einval();
#ifdef TIOCGPTN
      {
        int ptn;
        // FreeBSD's FIODGNAME returns just the device name (e.g. "pts/0"),
        // without the /dev/ prefix — libc prepends "/dev/" itself.
        if (VfsIoctl(fildes, TIOCGPTN, &ptn) == -1) return -1;
        snprintf(hname, sizeof(hname), "pts/%d", ptn);
      }
#else
      {
        char *p = ptsname(fildes);
        if (!p) return -1;
        if (!strncmp(p, "/dev/", 5)) p += 5;
        snprintf(hname, sizeof(hname), "%s", p);
      }
#endif
      hlen = strlen(hname) + 1;
      if ((size_t)buflen < hlen) {
        errno = EINVAL;
        return -1;
      }
      if (CopyToUserWrite(m, bufaddr, hname, hlen) == -1) return -1;
      return 0;
    }
    default:
      LOGF("missing ioctl %#" PRIx64, request);
      return einval();
  }
}
