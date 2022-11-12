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
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/case.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/iovs.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/memory.h"
#include "blink/pml4t.h"
#include "blink/random.h"
#include "blink/signal.h"
#include "blink/swap.h"
#include "blink/timespec.h"
#include "blink/timeval.h"
#include "blink/util.h"
#include "blink/xlat.h"

#define POLLING_INTERVAL_MS 50

#define TIOCGWINSZ_LINUX    0x5413
#define TCGETS_LINUX        0x5401
#define TCSETS_LINUX        0x5402
#define TCSETSW_LINUX       0x5403
#define TCSETSF_LINUX       0x5404
#define ARCH_SET_GS_LINUX   0x1001
#define ARCH_SET_FS_LINUX   0x1002
#define ARCH_GET_FS_LINUX   0x1003
#define ARCH_GET_GS_LINUX   0x1004
#define MAP_GROWSDOWN_LINUX 0x0100
#define SOCK_CLOEXEC_LINUX  0x080000
#define O_CLOEXEC_LINUX     0x080000
#define POLLIN_LINUX        0x01
#define POLLPRI_LINUX       0x02
#define POLLOUT_LINUX       0x04
#define POLLERR_LINUX       0x08
#define POLLHUP_LINUX       0x10
#define POLLNVAL_LINUX      0x20
#define TIMER_ABSTIME_LINUX 0x01

#define CLONE_VM_             0x00000100
#define CLONE_THREAD_         0x00010000
#define CLONE_FS_             0x00000200
#define CLONE_FILES_          0x00000400
#define CLONE_SIGHAND_        0x00000800
#define CLONE_SETTLS_         0x00080000
#define CLONE_PARENT_SETTID_  0x00100000
#define CLONE_CHILD_CLEARTID_ 0x00200000
#define CLONE_CHILD_SETTID_   0x01000000

#define SYSCALL(x, y) CASE(x, /* LOGF("%s", #y);  */ ax = y)
#define ASSIGN(D, S)  memcpy(&D, &S, MIN(sizeof(S), sizeof(D)))

// delegate to work around function pointer errors, b/c
// old musl toolchains using `int ioctl(int, int, ...)`
static int SystemIoctl(int fd, unsigned long request, ...) {
  va_list va;
  intptr_t arg;
  va_start(va, request);
  arg = va_arg(va, intptr_t);
  va_end(va);
  return ioctl(fd, request, arg);
}

const struct MachineFdCb kMachineFdCbHost = {
    .close = close,
    .readv = readv,
    .writev = writev,
    .ioctl = SystemIoctl,
    .poll = poll,
};

static int GetFd(struct Machine *m, int fd) {
  if (!(0 <= fd && fd < m->system->fds.i)) return ebadf();
  if (!m->system->fds.p[fd].cb) return ebadf();
  return m->system->fds.p[fd].fd;
}

static int GetAfd(struct Machine *m, int fd) {
  if (fd == -100) return AT_FDCWD;
  return GetFd(m, fd);
}

static int AppendIovsReal(struct Machine *m, struct Iovs *ib, i64 addr,
                          u64 size) {
  void *real;
  unsigned got;
  u64 have;
  while (size) {
    if (!(real = FindReal(m, addr))) return efault();
    have = 0x1000 - (addr & 0xfff);
    got = MIN(size, have);
    if (AppendIovs(ib, real, got) == -1) return -1;
    addr += got;
    size -= got;
  }
  return 0;
}

static int AppendIovsGuest(struct Machine *m, struct Iovs *iv, i64 iovaddr,
                           i64 iovlen) {
  int rc;
  size_t i;
  u64 iovsize;
  struct iovec_linux *guestiovs;
  if (!mulo(iovlen, sizeof(struct iovec_linux), &iovsize) &&
      (0 <= iovsize && iovsize <= 0x7ffff000)) {
    if ((guestiovs = (struct iovec_linux *)malloc(iovsize))) {
      VirtualSendRead(m, guestiovs, iovaddr, iovsize);
      for (rc = i = 0; i < iovlen; ++i) {
        if (AppendIovsReal(m, iv, Read64(guestiovs[i].iov_base),
                           Read64(guestiovs[i].iov_len)) == -1) {
          rc = -1;
          break;
        }
      }
      free(guestiovs);
      return rc;
    } else {
      return enomem();
    }
  } else {
    return eoverflow();
  }
}

_Noreturn static void OpExit(struct Machine *m, int rc) {
  atomic_store_explicit((atomic_int *)ResolveAddress(m, m->ctid), 0,
                        memory_order_release);
  FreeMachine(m);
  pthread_exit(0);
}

_Noreturn static void OpExitGroup(struct Machine *m, int rc) {
  if (m->system->isfork) {
    _Exit(rc);
  } else {
    HaltMachine(m, rc | 0x100);
  }
}

static int OpFork(struct Machine *m) {
  int pid;
  pid = fork();
  if (!pid) m->system->isfork = true;
  return pid;
}

static void *Actor(void *arg) {
  int rc;
  sigset_t ss;
  struct Machine *m = (struct Machine *)arg;
  sigfillset(&ss);
  unassert(!sigprocmask(SIG_SETMASK, &ss, 0));
  if (!(rc = setjmp(m->onhalt))) {
    atomic_store_explicit((atomic_int *)ResolveAddress(m, m->ctid),
                          SWAP32LE(m->tid), memory_order_release);
    for (;;) {
      LoadInstruction(m);
      ExecuteInstruction(m);
    }
  } else {
    LOGF("halting machine from thread: %d", rc);
    _Exit(rc);
  }
}

static int OpSpawn(struct Machine *m, u64 flags, u64 stack, u64 ptid, u64 ctid,
                   u64 tls, u64 func) {
  int tid, err;
  pthread_t th;
  pthread_attr_t attr;
  struct Machine *m2 = 0;
  if (flags != (CLONE_VM_ | CLONE_THREAD_ | CLONE_FS_ | CLONE_FILES_ |
                CLONE_SIGHAND_ | CLONE_SETTLS_ | CLONE_PARENT_SETTID_ |
                CLONE_CHILD_CLEARTID_ | CLONE_CHILD_SETTID_)) {
    LOGF("bad clone() flags: %#x", flags);
    return einval();
  }
  if (!(m2 = NewMachine(m->system, m))) {
    return eagain();
  }
  tid = m2->tid;
  m2->ctid = ctid;
  Write64(m2->ax, 0);
  Write64(m2->fs, tls);
  Write64(m2->sp, stack);
  unassert(!pthread_attr_init(&attr));
  unassert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  err = pthread_create(&th, &attr, Actor, m2);
  unassert(!pthread_attr_destroy(&attr));
  if (err) {
    FreeMachine(m2);
    return eagain();
  }
  atomic_store_explicit((atomic_int *)ResolveAddress(m, ptid), SWAP32LE(tid),
                        memory_order_release);
  return tid;
}

static int OpClone(struct Machine *m, u64 flags, u64 stack, u64 ptid, u64 ctid,
                   u64 tls, u64 func) {
  if (flags == 17 && !stack) {
    return OpFork(m);
  } else {
    return OpSpawn(m, flags, stack, ptid, ctid, tls, func);
  }
}

static int OpPrctl(struct Machine *m, int op, i64 a, i64 b, i64 c, i64 d) {
  return einval();
}

static int OpArchPrctl(struct Machine *m, int code, i64 addr) {
  switch (code) {
    case ARCH_SET_GS_LINUX:
      Write64(m->gs, addr);
      return 0;
    case ARCH_SET_FS_LINUX:
      Write64(m->fs, addr);
      return 0;
    case ARCH_GET_GS_LINUX:
      VirtualRecvWrite(m, addr, m->gs, 8);
      return 0;
    case ARCH_GET_FS_LINUX:
      VirtualRecvWrite(m, addr, m->fs, 8);
      return 0;
    default:
      return einval();
  }
}

static int OpMprotect(struct Machine *m, i64 addr, u64 len, int prot) {
  return 0;
}

static int OpMadvise(struct Machine *m, i64 addr, size_t len, int advice) {
  return enosys();
}

static i64 OpBrk(struct Machine *m, i64 addr) {
  addr = ROUNDUP(addr, 4096);
  if (addr > m->system->brk) {
    if (ReserveVirtual(m->system, m->system->brk, addr - m->system->brk,
                       PAGE_V | PAGE_RW | PAGE_U | PAGE_RSRV) != -1) {
      m->system->brk = addr;
    }
  } else if (addr < m->system->brk) {
    if (FreeVirtual(m->system, addr, m->system->brk - addr) != -1) {
      m->system->brk = addr;
    }
  }
  return m->system->brk;
}

static int OpMunmap(struct Machine *m, i64 virt, u64 size) {
  return FreeVirtual(m->system, virt, size);
}

static i64 OpMmap(struct Machine *m, i64 virt, size_t size, int prot, int flags,
                  int fd, i64 offset) {
  int e;
  void *tmp;
  ssize_t rc;
  u64 key;
  if (flags & MAP_GROWSDOWN_LINUX) {
    errno = ENOTSUP;
    return -1;
  }
  if (prot & PROT_READ) {
    key = PAGE_RSRV | PAGE_U | PAGE_V;
    if (prot & PROT_WRITE) key |= PAGE_RW;
    if (!(prot & PROT_EXEC)) key |= PAGE_XD;
    flags = XlatMapFlags(flags);
    if (fd != -1 && (fd = GetFd(m, fd)) == -1) return -1;
    if (!(flags & MAP_FIXED)) {
      if (!virt) {
        if ((virt = FindVirtual(m->system, m->system->brk, size)) == -1) {
          return -1;
        }
        m->system->brk = virt + size;
      } else {
        if ((virt = FindVirtual(m->system, virt, size)) == -1) {
          return -1;
        }
      }
    }
    if (ReserveVirtual(m->system, virt, size, key) != -1) {
      if (fd != -1 && !(flags & MAP_ANONYMOUS)) {
        /* TODO: lazy file mappings */
        tmp = malloc(size);
        for (e = errno;;) {
          rc = pread(fd, tmp, size, offset);
          if (rc != -1) break;
          unassert(EINTR == errno);
          errno = e;
        }
        unassert(size == rc);
        VirtualRecvWrite(m, virt, tmp, size);
        free(tmp);
      }
    } else {
      FreeVirtual(m->system, virt, size);
      return -1;
    }
    return virt;
  } else {
    return FreeVirtual(m->system, virt, size);
  }
}

static int OpMsync(struct Machine *m, i64 virt, size_t size, int flags) {
  return enosys();
}

static int OpClose(struct Machine *m, int fd) {
  int rc;
  if (0 <= fd && fd < m->system->fds.i) {
    if (!m->system->fds.p[fd].cb) return ebadf();
    rc = m->system->fds.p[fd].cb->close(m->system->fds.p[fd].fd);
    MachineFdRemove(&m->system->fds, fd);
    return rc;
  } else {
    return ebadf();
  }
}

static ssize_t OpCloseRange(struct Machine *m, u32 first, u32 last, u32 flags) {
  int fd;
  if (flags || last > INT_MAX || first > INT_MAX || first > last) {
    return einval();
  }
  if (m->system->fds.i > 0) {
    for (fd = first; fd <= MIN(last, m->system->fds.i - 1); ++fd) {
      MachineFdRemove(&m->system->fds, fd);
    }
  }
  return 0;
}

static int OpOpenat(struct Machine *m, int dirfd, i64 pathaddr, int flags,
                    int mode) {
  int fd;
  long i;
  const char *path;
  flags = XlatOpenFlags(flags);
  if ((dirfd = GetAfd(m, dirfd)) == -1) return -1;
  if ((i = MachineFdAdd(&m->system->fds)) == -1) return -1;
  path = LoadStr(m, pathaddr);
  if ((fd = openat(dirfd, path, flags, mode)) != -1) {
    m->system->fds.p[i].cb = &kMachineFdCbHost;
    m->system->fds.p[i].fd = fd;
    fd = i;
  } else {
    MachineFdRemove(&m->system->fds, i);
  }
  return fd;
}

static int OpPipe(struct Machine *m, i64 pipefds_addr) {
  long i, j;
  int hpipefds[2];
  u8 gpipefds[2][4];
  if ((i = MachineFdAdd(&m->system->fds)) != -1) {
    if ((j = MachineFdAdd(&m->system->fds)) != -1) {
      if (pipe(hpipefds) != -1) {
        m->system->fds.p[i].cb = &kMachineFdCbHost;
        m->system->fds.p[i].fd = hpipefds[0];
        m->system->fds.p[j].cb = &kMachineFdCbHost;
        m->system->fds.p[j].fd = hpipefds[1];
        Write32(gpipefds[0], i);
        Write32(gpipefds[1], j);
        VirtualRecvWrite(m, pipefds_addr, gpipefds, sizeof(gpipefds));
        return 0;
      }
      MachineFdRemove(&m->system->fds, j);
    }
    MachineFdRemove(&m->system->fds, i);
  }
  return -1;
}

static int OpDup(struct Machine *m, int fd) {
  long i;
  if ((fd = GetFd(m, fd)) != -1) {
    if ((i = MachineFdAdd(&m->system->fds)) != -1) {
      if ((fd = dup(fd)) != -1) {
        m->system->fds.p[i].cb = &kMachineFdCbHost;
        m->system->fds.p[i].fd = fd;
        return i;
      }
      MachineFdRemove(&m->system->fds, i);
    }
  }
  return -1;
}

static int OpDup3(struct Machine *m, int fd, int newfd, int flags) {
  int rc;
  long i;
  if (!(flags & ~O_CLOEXEC_LINUX)) {
    if ((fd = GetFd(m, fd)) == -1) return -1;
    if ((0 <= newfd && newfd < m->system->fds.i)) {
      if ((rc = dup2(fd, m->system->fds.p[newfd].fd)) != -1) {
        if (flags & O_CLOEXEC_LINUX) {
          fcntl(rc, F_SETFD, FD_CLOEXEC);
        }
        m->system->fds.p[newfd].cb = &kMachineFdCbHost;
        m->system->fds.p[newfd].fd = rc;
        rc = newfd;
      }
    } else if ((i = MachineFdAdd(&m->system->fds)) != -1) {
      if ((rc = dup(fd)) != -1) {
        m->system->fds.p[i].cb = &kMachineFdCbHost;
        m->system->fds.p[i].fd = rc;
        rc = i;
      }
    } else {
      rc = -1;
    }
    return rc;
  } else {
    return einval();
  }
}

static int OpDup2(struct Machine *m, int fd, int newfd) {
  int rc;
  long i;
  if ((fd = GetFd(m, fd)) == -1) return -1;
  if ((0 <= newfd && newfd < m->system->fds.i)) {
    if ((rc = dup2(fd, m->system->fds.p[newfd].fd)) != -1) {
      m->system->fds.p[newfd].cb = &kMachineFdCbHost;
      m->system->fds.p[newfd].fd = rc;
      rc = newfd;
    }
  } else if ((i = MachineFdAdd(&m->system->fds)) != -1) {
    if ((rc = dup(fd)) != -1) {
      m->system->fds.p[i].cb = &kMachineFdCbHost;
      m->system->fds.p[i].fd = rc;
      rc = i;
    }
  } else {
    rc = -1;
  }
  return rc;
}

static int OpSocket(struct Machine *m, int family, int type, int protocol) {
  int fd;
  long i;
  if ((family = XlatSocketFamily(family)) == -1) return -1;
  if ((type = XlatSocketType(type)) == -1) return -1;
  if ((protocol = XlatSocketProtocol(protocol)) == -1) return -1;
  if ((i = MachineFdAdd(&m->system->fds)) == -1) return -1;
  if ((fd = socket(family, type, protocol)) != -1) {
    m->system->fds.p[i].cb = &kMachineFdCbHost;
    m->system->fds.p[i].fd = fd;
    fd = i;
  } else {
    MachineFdRemove(&m->system->fds, i);
  }
  return fd;
}

static int OpSocketName(struct Machine *m, int fd, i64 aa, i64 asa,
                        int SocketName(int, struct sockaddr *, socklen_t *)) {
  int rc;
  u32 addrsize;
  u8 gaddrsize[4];
  struct sockaddr_in addr;
  struct sockaddr_in_linux gaddr;
  if ((fd = GetFd(m, fd)) == -1) return -1;
  VirtualSendRead(m, gaddrsize, asa, sizeof(gaddrsize));
  if (Read32(gaddrsize) < sizeof(gaddr)) return einval();
  addrsize = sizeof(addr);
  if ((rc = SocketName(fd, (struct sockaddr *)&addr, &addrsize)) != -1) {
    Write32(gaddrsize, sizeof(gaddr));
    XlatSockaddrToLinux(&gaddr, &addr);
    VirtualRecv(m, asa, gaddrsize, sizeof(gaddrsize));
    VirtualRecvWrite(m, aa, &gaddr, sizeof(gaddr));
  }
  return rc;
}

static int OpUname(struct Machine *m, i64 utsaddr) {
  struct utsname_linux uts = {
      .sysname = "blink",
      .nodename = "blink.local",
      .release = "1.0",
      .version = "blink 1.0",
      .machine = "x86_64",
  };
  strcpy(uts.sysname, "unknown");
  strcpy(uts.sysname, "unknown");
  VirtualRecv(m, utsaddr, &uts, sizeof(uts));
  return 0;
}

static int OpGetsockname(struct Machine *m, int fd, i64 aa, i64 asa) {
  return OpSocketName(m, fd, aa, asa, getsockname);
}

static int OpGetpeername(struct Machine *m, int fd, i64 aa, i64 asa) {
  return OpSocketName(m, fd, aa, asa, getpeername);
}

static int OpAccept4(struct Machine *m, int fd, i64 aa, i64 asa, int flags) {
  int rc;
  long i;
  u32 addrsize;
  u8 gaddrsize[4];
  struct sockaddr_in addr;
  struct sockaddr_in_linux gaddr;
  if (m->system->redraw) m->system->redraw();
  if (!(flags & ~SOCK_CLOEXEC_LINUX)) {
    if ((fd = GetFd(m, fd)) == -1) return -1;
    if (aa) {
      VirtualSendRead(m, gaddrsize, asa, sizeof(gaddrsize));
      if (Read32(gaddrsize) < sizeof(gaddr)) return einval();
    }
    if ((i = rc = MachineFdAdd(&m->system->fds)) != -1) {
      addrsize = sizeof(addr);
      if ((rc = accept(fd, (struct sockaddr *)&addr, &addrsize)) != -1) {
        if (flags & SOCK_CLOEXEC_LINUX) {
          fcntl(fd, F_SETFD, FD_CLOEXEC);
        }
        if (aa) {
          Write32(gaddrsize, sizeof(gaddr));
          XlatSockaddrToLinux(&gaddr, &addr);
          VirtualRecv(m, asa, gaddrsize, sizeof(gaddrsize));
          VirtualRecvWrite(m, aa, &gaddr, sizeof(gaddr));
        }
        m->system->fds.p[i].cb = &kMachineFdCbHost;
        m->system->fds.p[i].fd = rc;
        rc = i;
      } else {
        MachineFdRemove(&m->system->fds, i);
      }
    }
    return rc;
  } else {
    return einval();
  }
}

static int OpConnectBind(struct Machine *m, int fd, i64 aa, u32 as,
                         int impl(int, const struct sockaddr *, u32)) {
  struct sockaddr_in addr;
  struct sockaddr_in_linux gaddr;
  if (as != sizeof(gaddr)) return einval();
  if ((fd = GetFd(m, fd)) == -1) return -1;
  VirtualSendRead(m, &gaddr, aa, sizeof(gaddr));
  XlatSockaddrToHost(&addr, &gaddr);
  return impl(fd, (const struct sockaddr *)&addr, sizeof(addr));
}

static int OpBind(struct Machine *m, int fd, i64 aa, u32 as) {
  return OpConnectBind(m, fd, aa, as, bind);
}

static int OpConnect(struct Machine *m, int fd, i64 aa, u32 as) {
  return OpConnectBind(m, fd, aa, as, connect);
}

static int OpSetsockopt(struct Machine *m, int fd, int level, int optname,
                        i64 optvaladdr, u32 optvalsize) {
  int rc;
  void *optval;
  if ((level = XlatSocketLevel(level)) == -1) return -1;
  if ((optname = XlatSocketOptname(level, optname)) == -1) return -1;
  if ((fd = GetFd(m, fd)) == -1) return -1;
  if (!(optval = malloc(optvalsize))) return -1;
  VirtualSendRead(m, optval, optvaladdr, optvalsize);
  rc = setsockopt(fd, level, optname, optval, optvalsize);
  free(optval);
  return rc;
}

static i64 OpRead(struct Machine *m, int fd, i64 addr, u64 size) {
  i64 rc;
  struct Iovs iv;
  if ((0 <= fd && fd < m->system->fds.i) && m->system->fds.p[fd].cb) {
    InitIovs(&iv);
    if ((rc = AppendIovsReal(m, &iv, addr, size)) != -1) {
      if ((rc = m->system->fds.p[fd].cb->readv(m->system->fds.p[fd].fd, iv.p,
                                               iv.i)) != -1) {
        SetWriteAddr(m, addr, rc);
      }
    }
    FreeIovs(&iv);
    return rc;
  } else {
    return ebadf();
  }
}

static int OpGetdents(struct Machine *m, int fd, i64 addr, u32 size) {
  int rc;
  DIR *dir;
  struct dirent *ent;
  if (size < sizeof(struct dirent)) errno = EINVAL;
  return -1;
  if (0 <= fd && fd < m->system->fds.i) {
    if ((dir = fdopendir(m->system->fds.p[fd].fd))) {
      rc = 0;
      while (rc + sizeof(struct dirent) <= size) {
        if (!(ent = readdir(dir))) break;
        VirtualRecvWrite(m, addr + rc, ent, ent->d_reclen);
        rc += ent->d_reclen;
      }
      free(dir);
    } else {
      rc = -1;
    }
    return rc;
  } else {
    return ebadf();
  }
}

static long OpSetTidAddress(struct Machine *m, i64 tidptr) {
  return getpid();
}

static i64 OpWrite(struct Machine *m, int fd, i64 addr, u64 size) {
  i64 rc;
  struct Iovs iv;
  if ((0 <= fd && fd < m->system->fds.i) && m->system->fds.p[fd].cb) {
    InitIovs(&iv);
    if ((rc = AppendIovsReal(m, &iv, addr, size)) != -1) {
      if ((rc = m->system->fds.p[fd].cb->writev(m->system->fds.p[fd].fd, iv.p,
                                                iv.i)) != -1) {
        SetReadAddr(m, addr, rc);
      }
    }
    FreeIovs(&iv);
    return rc;
  } else {
    return ebadf();
  }
}

static int IoctlTiocgwinsz(struct Machine *m, int fd, i64 addr,
                           int fn(int, unsigned long, ...)) {
  int rc;
  struct winsize ws;
  struct winsize_linux gws;
  if ((rc = fn(fd, TIOCGWINSZ, &ws)) != -1) {
    XlatWinsizeToLinux(&gws, &ws);
    VirtualRecvWrite(m, addr, &gws, sizeof(gws));
  }
  return rc;
}

static int IoctlTcgets(struct Machine *m, int fd, i64 addr,
                       int fn(int, unsigned long, ...)) {
  int rc;
  struct termios tio;
  struct termios_linux gtio;
  if ((rc = fn(fd, TCGETS, &tio)) != -1) {
    XlatTermiosToLinux(&gtio, &tio);
    VirtualRecvWrite(m, addr, &gtio, sizeof(gtio));
  }
  return rc;
}

static int IoctlTcsets(struct Machine *m, int fd, int request, i64 addr,
                       int fn(int, unsigned long, ...)) {
  struct termios tio;
  struct termios_linux gtio;
  VirtualSendRead(m, &gtio, addr, sizeof(gtio));
  XlatLinuxToTermios(&tio, &gtio);
  return fn(fd, request, &tio);
}

static int OpIoctl(struct Machine *m, int fd, u64 request, i64 addr) {
  int (*fn)(int, unsigned long, ...);
  if (!(0 <= fd && fd < m->system->fds.i) || !m->system->fds.p[fd].cb)
    return ebadf();
  fn = m->system->fds.p[fd].cb->ioctl;
  fd = m->system->fds.p[fd].fd;
  switch (request) {
    case TIOCGWINSZ_LINUX:
      return IoctlTiocgwinsz(m, fd, addr, fn);
    case TCGETS_LINUX:
      return IoctlTcgets(m, fd, addr, fn);
    case TCSETS_LINUX:
      return IoctlTcsets(m, fd, TCSETS, addr, fn);
    case TCSETSW_LINUX:
      return IoctlTcsets(m, fd, TCSETSW, addr, fn);
    case TCSETSF_LINUX:
      return IoctlTcsets(m, fd, TCSETSF, addr, fn);
    default:
      return einval();
  }
}

static i64 OpReadv(struct Machine *m, i32 fd, i64 iovaddr, i32 iovlen) {
  i64 rc;
  struct Iovs iv;
  if ((0 <= fd && fd < m->system->fds.i) && m->system->fds.p[fd].cb) {
    InitIovs(&iv);
    if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen)) != -1) {
      rc = m->system->fds.p[fd].cb->readv(m->system->fds.p[fd].fd, iv.p, iv.i);
    }
    FreeIovs(&iv);
    return rc;
  } else {
    return ebadf();
  }
}

static i64 OpWritev(struct Machine *m, i32 fd, i64 iovaddr, i32 iovlen) {
  i64 rc;
  struct Iovs iv;
  if ((0 <= fd && fd < m->system->fds.i) && m->system->fds.p[fd].cb) {
    InitIovs(&iv);
    if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen)) != -1) {
      rc = m->system->fds.p[fd].cb->writev(m->system->fds.p[fd].fd, iv.p, iv.i);
    }
    FreeIovs(&iv);
    return rc;
  } else {
    return ebadf();
  }
}

static i64 OpLseek(struct Machine *m, int fd, i64 offset, int whence) {
  if ((fd = GetFd(m, fd)) == -1) return -1;
  return lseek(fd, offset, whence);
}

static i64 OpFtruncate(struct Machine *m, int fd, i64 size) {
  if ((fd = GetFd(m, fd)) == -1) return -1;
  return ftruncate(fd, size);
}

static int OpFaccessat(struct Machine *m, int dirfd, i64 path, int mode,
                       int flags) {
  flags = XlatAtf(flags);
  mode = XlatAccess(mode);
  if ((dirfd = GetAfd(m, dirfd)) == -1) return -1;
  return faccessat(dirfd, LoadStr(m, path), mode, flags);
}

static int OpFstatat(struct Machine *m, int dirfd, i64 path, i64 staddr,
                     int flags) {
  int rc;
  struct stat st;
  struct stat_linux gst;
  flags = XlatAtf(flags);
  if ((dirfd = GetAfd(m, dirfd)) == -1) return -1;
  if ((rc = fstatat(dirfd, LoadStr(m, path), &st, flags)) != -1) {
    XlatStatToLinux(&gst, &st);
    VirtualRecvWrite(m, staddr, &gst, sizeof(gst));
  }
  return rc;
}

static int OpFstat(struct Machine *m, int fd, i64 staddr) {
  int rc;
  struct stat st;
  struct stat_linux gst;
  if ((fd = GetFd(m, fd)) == -1) return -1;
  if ((rc = fstat(fd, &st)) != -1) {
    XlatStatToLinux(&gst, &st);
    VirtualRecvWrite(m, staddr, &gst, sizeof(gst));
  }
  return rc;
}

static int OpFsync(struct Machine *m, int fd) {
  if ((fd = GetFd(m, fd)) == -1) return -1;
  return fsync(fd);
}

static int OpFdatasync(struct Machine *m, int fd) {
  if ((fd = GetFd(m, fd)) == -1) return -1;
  return fdatasync(fd);
}

static int OpChdir(struct Machine *m, i64 path) {
  return chdir(LoadStr(m, path));
}

static int OpFlock(struct Machine *m, int fd, int lock) {
  if ((fd = GetFd(m, fd)) == -1) return -1;
  if ((lock = XlatLock(lock)) == -1) return -1;
  return flock(fd, lock);
}

static int OpShutdown(struct Machine *m, int fd, int how) {
  if ((fd = GetFd(m, fd)) == -1) return -1;
  return shutdown(fd, how);
}

static int OpListen(struct Machine *m, int fd, int backlog) {
  if ((fd = GetFd(m, fd)) == -1) return -1;
  return listen(fd, backlog);
}

static int OpMkdir(struct Machine *m, i64 path, int mode) {
  return mkdir(LoadStr(m, path), mode);
}

static int OpFchmod(struct Machine *m, int fd, u32 mode) {
  if ((fd = GetFd(m, fd)) == -1) return -1;
  return fchmod(fd, mode);
}

static int OpFcntl(struct Machine *m, int fd, int cmd, int arg) {
  if ((cmd = XlatFcntlCmd(cmd)) == -1) return -1;
  if ((arg = XlatFcntlArg(arg)) == -1) return -1;
  if ((fd = GetFd(m, fd)) == -1) return -1;
  return fcntl(fd, cmd, arg);
}

static int OpRmdir(struct Machine *m, i64 path) {
  return rmdir(LoadStr(m, path));
}

static int OpUnlink(struct Machine *m, i64 path) {
  return unlink(LoadStr(m, path));
}

static int OpRename(struct Machine *m, i64 src, i64 dst) {
  return rename(LoadStr(m, src), LoadStr(m, dst));
}

static int OpChmod(struct Machine *m, i64 path, u32 mode) {
  return chmod(LoadStr(m, path), mode);
}

static int OpTruncate(struct Machine *m, i64 path, u64 length) {
  return truncate(LoadStr(m, path), length);
}

static int OpSymlink(struct Machine *m, i64 target, i64 linkpath) {
  return symlink(LoadStr(m, target), LoadStr(m, linkpath));
}

static int OpMkdirat(struct Machine *m, int dirfd, i64 path, int mode) {
  return mkdirat(GetAfd(m, dirfd), LoadStr(m, path), mode);
}

static int OpLink(struct Machine *m, i64 existingpath, i64 newpath) {
  return link(LoadStr(m, existingpath), LoadStr(m, newpath));
}

static int OpMknod(struct Machine *m, i64 path, int mode, u64 dev) {
  return mknod(LoadStr(m, path), mode, dev);
}

static int OpUnlinkat(struct Machine *m, int dirfd, i64 path, int flags) {
  return unlinkat(GetAfd(m, dirfd), LoadStr(m, path), XlatAtf(flags));
}

static int OpRenameat(struct Machine *m, int srcdirfd, i64 src, int dstdirfd,
                      i64 dst) {
  return renameat(GetAfd(m, srcdirfd), LoadStr(m, src), GetAfd(m, dstdirfd),
                  LoadStr(m, dst));
}

static int OpExecve(struct Machine *m, i64 pa, i64 aa, i64 ea) {
  if (m->system->exec) {
    _exit(m->system->exec(LoadStr(m, pa), LoadStrList(m, aa),
                          LoadStrList(m, ea)));
  } else {
    return enosys();
  }
}

static int OpWait4(struct Machine *m, int pid, i64 opt_out_wstatus_addr,
                   int options, i64 opt_out_rusage_addr) {
  int rc;
  i32 wstatus;
  struct rusage hrusage;
  struct rusage_linux grusage;
  if ((options = XlatWait(options)) == -1) return -1;
  if ((rc = wait4(pid, &wstatus, options, &hrusage)) != -1) {
    if (opt_out_wstatus_addr) {
      VirtualRecvWrite(m, opt_out_wstatus_addr, &wstatus, sizeof(wstatus));
    }
    if (opt_out_rusage_addr) {
      XlatRusageToLinux(&grusage, &hrusage);
      VirtualRecvWrite(m, opt_out_rusage_addr, &grusage, sizeof(grusage));
    }
  }
  return rc;
}

static int OpGetrusage(struct Machine *m, int resource, i64 rusageaddr) {
  int rc;
  struct rusage hrusage;
  struct rusage_linux grusage;
  if ((resource = XlatRusage(resource)) == -1) return -1;
  if ((rc = getrusage(resource, &hrusage)) != -1) {
    XlatRusageToLinux(&grusage, &hrusage);
    VirtualRecvWrite(m, rusageaddr, &grusage, sizeof(grusage));
  }
  return rc;
}

static int OpGetrlimit(struct Machine *m, int resource, i64 rlimitaddr) {
  int rc;
  struct rlimit rlim;
  struct rlimit_linux linux_;
  if ((resource = XlatResource(resource)) == -1) return -1;
  if ((rc = getrlimit(resource, &rlim)) != -1) {
    XlatRlimitToLinux(&linux_, &rlim);
    VirtualRecvWrite(m, rlimitaddr, &linux_, sizeof(linux_));
  }
  return rc;
}

static int OpSetrlimit(struct Machine *m, int resource, i64 rlimitaddr) {
  struct rlimit rlim;
  struct rlimit_linux linux_;
  if ((resource = XlatResource(resource)) == -1) return -1;
  VirtualSendRead(m, &linux_, rlimitaddr, sizeof(linux_));
  XlatRlimitToLinux(&linux_, &rlim);
  return setrlimit(resource, &rlim);
}

static ssize_t OpReadlinkat(struct Machine *m, int dirfd, i64 pathaddr,
                            i64 bufaddr, size_t size) {
  char *buf;
  ssize_t rc;
  const char *path;
  if ((dirfd = GetAfd(m, dirfd)) == -1) return -1;
  path = LoadStr(m, pathaddr);
  if (!(buf = (char *)malloc(size))) return enomem();
  if ((rc = readlinkat(dirfd, path, buf, size)) != -1) {
    VirtualRecvWrite(m, bufaddr, buf, rc);
  }
  free(buf);
  return rc;
}

static i64 OpGetcwd(struct Machine *m, i64 bufaddr, size_t size) {
  size_t n;
  char *buf;
  i64 res;
  if ((buf = (char *)malloc(size))) {
    if ((getcwd)(buf, size)) {
      n = strlen(buf) + 1;
      VirtualRecvWrite(m, bufaddr, buf, n);
      res = bufaddr;
    } else {
      res = -1;
    }
    free(buf);
    return res;
  } else {
    return enomem();
  }
}

static ssize_t OpGetrandom(struct Machine *m, i64 a, size_t n, int f) {
  char *p;
  ssize_t rc;
  if ((p = (char *)malloc(n))) {
    if ((rc = GetRandom(p, n)) != -1) {
      VirtualRecvWrite(m, a, p, rc);
    }
    free(p);
  } else {
    rc = enomem();
  }
  return rc;
}

static int OpSigaction(struct Machine *m, int sig, i64 act, i64 old,
                       u64 sigsetsize) {
  if ((sig = XlatSignal(sig) - 1) != -1 &&
      (0 <= sig && sig < ARRAYLEN(m->system->hands)) && sigsetsize == 8) {
    if (old)
      VirtualRecvWrite(m, old, &m->system->hands[sig],
                       sizeof(m->system->hands[0]));
    if (act)
      VirtualSendRead(m, &m->system->hands[sig], act,
                      sizeof(m->system->hands[0]));
    return 0;
  } else {
    return einval();
  }
}

static int OpGetitimer(struct Machine *m, int which, i64 curvaladdr) {
  int rc;
  struct itimerval it;
  struct itimerval_linux git;
  if ((rc = getitimer(which, &it)) != -1) {
    XlatItimervalToLinux(&git, &it);
    VirtualRecvWrite(m, curvaladdr, &git, sizeof(git));
  }
  return rc;
}

static int OpSetitimer(struct Machine *m, int which, i64 neuaddr, i64 oldaddr) {
  int rc;
  struct itimerval neu, old;
  struct itimerval_linux git;
  VirtualSendRead(m, &git, neuaddr, sizeof(git));
  XlatLinuxToItimerval(&neu, &git);
  if ((rc = setitimer(which, &neu, &old)) != -1) {
    if (oldaddr) {
      XlatItimervalToLinux(&git, &old);
      VirtualRecvWrite(m, oldaddr, &git, sizeof(git));
    }
  }
  return rc;
}

static int OpNanosleep(struct Machine *m, i64 req, i64 rem) {
  int rc;
  struct timespec hreq, hrem;
  struct timespec_linux gtimespec;
  if (req) {
    VirtualSendRead(m, &gtimespec, req, sizeof(gtimespec));
    hreq.tv_sec = Read64(gtimespec.tv_sec);
    hreq.tv_nsec = Read64(gtimespec.tv_nsec);
  }
  rc = nanosleep(req ? &hreq : 0, rem ? &hrem : 0);
  if (rc == -1 && errno == EINTR && rem) {
    Write64(gtimespec.tv_sec, hrem.tv_sec);
    Write64(gtimespec.tv_nsec, hrem.tv_nsec);
    VirtualRecvWrite(m, rem, &gtimespec, sizeof(gtimespec));
  }
  return rc;
}

static int OpClockNanosleep(struct Machine *m, int clock, int flags,
                            i64 reqaddr, i64 remaddr) {
  int rc;
  struct timespec req, rem;
  struct timespec_linux gtimespec;
  if ((clock = XlatClock(clock)) == -1) return -1;
  if (flags & ~TIMER_ABSTIME_LINUX) return einval();
  VirtualSendRead(m, &gtimespec, reqaddr, sizeof(gtimespec));
  req.tv_sec = Read64(gtimespec.tv_sec);
  req.tv_nsec = Read64(gtimespec.tv_nsec);
#ifdef TIMER_ABSTIME
  flags = flags & TIMER_ABSTIME_LINUX ? TIMER_ABSTIME : 0;
  if ((rc = clock_nanosleep(clock, flags, &req, &rem))) {
    errno = rc;
    rc = -1;
  }
#else
  struct timespec now;
  if (!flags) {
    if (clock == CLOCK_REALTIME) {
      rc = nanosleep(&req, &rem);
    } else {
      rc = einval();
    }
  } else if (!(rc = clock_gettime(clock, &now))) {
    if (timespec_cmp(now, req) < 0) {
      req = timespec_sub(req, now);
      rc = nanosleep(&req, &rem);
    } else {
      rc = 0;
    }
  }
#endif
  if (!flags && remaddr && rc == -1 && errno == EINTR) {
    Write64(gtimespec.tv_sec, rem.tv_sec);
    Write64(gtimespec.tv_nsec, rem.tv_nsec);
    VirtualRecvWrite(m, remaddr, &gtimespec, sizeof(gtimespec));
  }
  return rc;
}

static int OpSigsuspend(struct Machine *m, i64 maskaddr) {
  sigset_t mask;
  u8 gmask[8];
  VirtualSendRead(m, &gmask, maskaddr, 8);
  XlatLinuxToSigset(&mask, gmask);
  return sigsuspend(&mask);
}

static int OpSigaltstack(struct Machine *m, i64 newaddr, i64 oldaddr) {
  return 0;
}

static int OpClockGettime(struct Machine *m, int clock, i64 ts) {
  int rc;
  struct timespec htimespec;
  struct timespec_linux gtimespec;
  if ((clock = XlatClock(clock)) == -1) return -1;
  if (!ts) return 0;
  if ((rc = clock_gettime(clock, &htimespec)) != -1) {
    if (ts) {
      Write64(gtimespec.tv_sec, htimespec.tv_sec);
      Write64(gtimespec.tv_nsec, htimespec.tv_nsec);
      VirtualRecvWrite(m, ts, &gtimespec, sizeof(gtimespec));
    }
  }
  return rc;
}

static int OpClockGetres(struct Machine *m, int clock, i64 ts) {
  int rc;
  struct timespec htimespec;
  struct timespec_linux gtimespec;
  if ((clock = XlatClock(clock)) == -1) return -1;
  if (!ts) return 0;
  if ((rc = clock_getres(clock, &htimespec)) != -1) {
    if (ts) {
      Write64(gtimespec.tv_sec, htimespec.tv_sec);
      Write64(gtimespec.tv_nsec, htimespec.tv_nsec);
      VirtualRecvWrite(m, ts, &gtimespec, sizeof(gtimespec));
    }
  }
  return rc;
}

static int OpGettimeofday(struct Machine *m, i64 tv, i64 tz) {
  int rc;
  struct timeval htimeval;
  struct timezone htimezone;
  struct timeval_linux gtimeval;
  struct timezone_linux gtimezone;
  if ((rc = gettimeofday(&htimeval, tz ? &htimezone : 0)) != -1) {
    Write64(gtimeval.tv_sec, htimeval.tv_sec);
    Write64(gtimeval.tv_usec, htimeval.tv_usec);
    VirtualRecvWrite(m, tv, &gtimeval, sizeof(gtimeval));
    if (tz) {
      Write32(gtimezone.tz_minuteswest, htimezone.tz_minuteswest);
      Write32(gtimezone.tz_dsttime, htimezone.tz_dsttime);
      VirtualRecvWrite(m, tz, &gtimezone, sizeof(gtimezone));
    }
  }
  return rc;
}

static int OpUtimes(struct Machine *m, i64 pathaddr, i64 tvsaddr) {
  const char *path;
  struct timeval tvs[2];
  struct timeval_linux gtvs[2];
  path = LoadStr(m, pathaddr);
  if (tvsaddr) {
    VirtualSendRead(m, gtvs, tvsaddr, sizeof(gtvs));
    tvs[0].tv_sec = Read64(gtvs[0].tv_sec);
    tvs[0].tv_usec = Read64(gtvs[0].tv_usec);
    tvs[1].tv_sec = Read64(gtvs[1].tv_sec);
    tvs[1].tv_usec = Read64(gtvs[1].tv_usec);
    return utimes(path, tvs);
  } else {
    return utimes(path, 0);
  }
}

static int OpPoll(struct Machine *m, i64 fdsaddr, u64 nfds, i32 timeout_ms) {
  long i;
  int fd, rc, ev;
  u64 gfdssize;
  struct pollfd hfds[1];
  struct timeval ts1, ts2;
  struct pollfd_linux *gfds;
  long wait, elapsed, timeout;
  timeout = timeout_ms * 1000L;
  if (!mulo(nfds, sizeof(struct pollfd_linux), &gfdssize) &&
      gfdssize <= 0x7ffff000) {
    if ((gfds = (struct pollfd_linux *)malloc(gfdssize))) {
      rc = 0;
      VirtualSendRead(m, gfds, fdsaddr, gfdssize);
      gettimeofday(&ts1, 0);
      for (;;) {
        for (i = 0; i < nfds; ++i) {
          fd = Read32(gfds[i].fd);
          if ((0 <= fd && fd < m->system->fds.i) && m->system->fds.p[fd].cb) {
            hfds[0].fd = m->system->fds.p[fd].fd;
            ev = Read16(gfds[i].events);
            hfds[0].events = (((ev & POLLIN_LINUX) ? POLLIN : 0) |
                              ((ev & POLLOUT_LINUX) ? POLLOUT : 0) |
                              ((ev & POLLPRI_LINUX) ? POLLPRI : 0));
            switch (m->system->fds.p[fd].cb->poll(hfds, 1, 0)) {
              case 0:
                Write16(gfds[i].revents, 0);
                break;
              case 1:
                ++rc;
                ev = 0;
                if (hfds[0].revents & POLLIN) ev |= POLLIN_LINUX;
                if (hfds[0].revents & POLLPRI) ev |= POLLPRI_LINUX;
                if (hfds[0].revents & POLLOUT) ev |= POLLOUT_LINUX;
                if (hfds[0].revents & POLLERR) ev |= POLLERR_LINUX;
                if (hfds[0].revents & POLLHUP) ev |= POLLHUP_LINUX;
                if (hfds[0].revents & POLLNVAL) ev |= POLLERR_LINUX;
                if (!ev) ev |= POLLERR_LINUX;
                Write16(gfds[i].revents, ev);
                break;
              case -1:
                ++rc;
                Write16(gfds[i].revents, POLLERR_LINUX);
                break;
              default:
                break;
            }
          } else {
            Write16(gfds[i].revents, POLLNVAL_LINUX);
          }
        }
        if (rc || !timeout) break;
        wait = POLLING_INTERVAL_MS * 1000;
        if (timeout < 0) {
          if (usleep(wait)) {
            rc = eintr();
            goto Finished;
          }
        } else {
          gettimeofday(&ts2, 0);
          elapsed = timeval_tomicros(timeval_sub(ts2, ts1));
          if (elapsed >= timeout) {
            break;
          }
          if (timeout - elapsed < wait) {
            wait = timeout - elapsed;
          }
          if (usleep(wait)) {
            rc = eintr();
            goto Finished;
          }
        }
      }
      VirtualRecvWrite(m, fdsaddr, gfds, nfds * sizeof(*gfds));
    } else {
      rc = enomem();
    }
  Finished:
    free(gfds);
    return rc;
  } else {
    return einval();
  }
}

static int OpSigprocmask(struct Machine *m, int how, i64 setaddr,
                         i64 oldsetaddr, u64 sigsetsize) {
  u8 set[8];
  if ((how = XlatSig(how)) != -1 && sigsetsize == sizeof(set)) {
    if (oldsetaddr) {
      VirtualRecvWrite(m, oldsetaddr, m->sigmask, sizeof(set));
    }
    if (setaddr) {
      VirtualSendRead(m, set, setaddr, sizeof(set));
      if (how == SIG_BLOCK) {
        Write64(m->sigmask, Read64(m->sigmask) | Read64(set));
      } else if (how == SIG_UNBLOCK) {
        Write64(m->sigmask, Read64(m->sigmask) & ~Read64(set));
      } else {
        Write64(m->sigmask, Read64(set));
      }
    }
    return 0;
  } else {
    return einval();
  }
}

static int OpKill(struct Machine *m, int pid, int sig) {
  if (pid == getpid()) {
    ThrowProtectionFault(m);
  } else {
    return kill(pid, sig);
  }
}

static int OpPause(struct Machine *m) {
  return pause();
}

static int OpSetsid(struct Machine *m) {
  return setsid();
}

static int OpGetpid(struct Machine *m) {
  return getpid();
}

static int OpGettid(struct Machine *m) {
  return m->tid;
}

static int OpGetppid(struct Machine *m) {
  return getppid();
}

static int OpGetuid(struct Machine *m) {
  return getuid();
}

static int OpGetgid(struct Machine *m) {
  return getgid();
}

static int OpGeteuid(struct Machine *m) {
  return geteuid();
}

static int OpGetegid(struct Machine *m) {
  return getegid();
}

static int OpSchedYield(struct Machine *m) {
  return sched_yield();
}

static int OpUmask(struct Machine *m, int mask) {
  return umask(mask);
}

static int OpSetuid(struct Machine *m, int uid) {
  return setuid(uid);
}

static int OpSetgid(struct Machine *m, int gid) {
  return setgid(gid);
}

static int OpGetpgid(struct Machine *m, int pid) {
  return getpgid(pid);
}

static int OpAlarm(struct Machine *m, unsigned seconds) {
  return alarm(seconds);
}

static int OpSetpgid(struct Machine *m, int pid, int gid) {
  return setpgid(pid, gid);
}

static int OpCreat(struct Machine *m, i64 path, int mode) {
  return OpOpenat(m, -100, path, 0x241, mode);
}

static int OpAccess(struct Machine *m, i64 path, int mode) {
  return OpFaccessat(m, -100, path, mode, 0);
}

static int OpStat(struct Machine *m, i64 path, i64 st) {
  return OpFstatat(m, -100, path, st, 0);
}

static int OpLstat(struct Machine *m, i64 path, i64 st) {
  return OpFstatat(m, -100, path, st, 0x0400);
}

static int OpOpen(struct Machine *m, i64 path, int flags, int mode) {
  return OpOpenat(m, -100, path, flags, mode);
}

static int OpAccept(struct Machine *m, int fd, i64 sa, i64 sas) {
  return OpAccept4(m, fd, sa, sas, 0);
}

void OpSyscall(struct Machine *m, u32 rde) {
  u64 ax, di, si, dx, r0, r8, r9;
  ax = Read64(m->ax);
  di = Read64(m->di);
  si = Read64(m->si);
  dx = Read64(m->dx);
  r0 = Read64(m->r10);
  r8 = Read64(m->r8);
  r9 = Read64(m->r9);
  switch (ax & 0x1ff) {
    SYSCALL(0x000, OpRead(m, di, si, dx));
    SYSCALL(0x001, OpWrite(m, di, si, dx));
    SYSCALL(0x002, OpOpen(m, di, si, dx));
    SYSCALL(0x003, OpClose(m, di));
    SYSCALL(0x004, OpStat(m, di, si));
    SYSCALL(0x005, OpFstat(m, di, si));
    SYSCALL(0x006, OpLstat(m, di, si));
    SYSCALL(0x007, OpPoll(m, di, si, dx));
    SYSCALL(0x008, OpLseek(m, di, si, dx));
    SYSCALL(0x009, OpMmap(m, di, si, dx, r0, r8, r9));
    SYSCALL(0x01A, OpMsync(m, di, si, dx));
    SYSCALL(0x00A, OpMprotect(m, di, si, dx));
    SYSCALL(0x00B, OpMunmap(m, di, si));
    SYSCALL(0x00C, OpBrk(m, di));
    SYSCALL(0x00D, OpSigaction(m, di, si, dx, r0));
    SYSCALL(0x00E, OpSigprocmask(m, di, si, dx, r0));
    SYSCALL(0x010, OpIoctl(m, di, si, dx));
    SYSCALL(0x013, OpReadv(m, di, si, dx));
    SYSCALL(0x014, OpWritev(m, di, si, dx));
    SYSCALL(0x015, OpAccess(m, di, si));
    SYSCALL(0x016, OpPipe(m, di));
    SYSCALL(0x018, OpSchedYield(m));
    SYSCALL(0x01C, OpMadvise(m, di, si, dx));
    SYSCALL(0x020, OpDup(m, di));
    SYSCALL(0x021, OpDup2(m, di, si));
    SYSCALL(0x124, OpDup3(m, di, si, dx));
    SYSCALL(0x022, OpPause(m));
    SYSCALL(0x023, OpNanosleep(m, di, si));
    SYSCALL(0x024, OpGetitimer(m, di, si));
    SYSCALL(0x025, OpAlarm(m, di));
    SYSCALL(0x026, OpSetitimer(m, di, si, dx));
    SYSCALL(0x027, OpGetpid(m));
    SYSCALL(0x0BA, OpGettid(m));
    SYSCALL(0x029, OpSocket(m, di, si, dx));
    SYSCALL(0x02A, OpConnect(m, di, si, dx));
    SYSCALL(0x02B, OpAccept(m, di, di, dx));
    SYSCALL(0x030, OpShutdown(m, di, si));
    SYSCALL(0x031, OpBind(m, di, si, dx));
    SYSCALL(0x032, OpListen(m, di, si));
    SYSCALL(0x033, OpGetsockname(m, di, si, dx));
    SYSCALL(0x034, OpGetpeername(m, di, si, dx));
    SYSCALL(0x036, OpSetsockopt(m, di, si, dx, r0, r8));
    SYSCALL(0x038, OpClone(m, di, si, dx, r0, r8, r9));
    SYSCALL(0x039, OpFork(m));
    SYSCALL(0x03B, OpExecve(m, di, si, dx));
    SYSCALL(0x03D, OpWait4(m, di, si, dx, r0));
    SYSCALL(0x03E, OpKill(m, di, si));
    SYSCALL(0x03F, OpUname(m, di));
    SYSCALL(0x048, OpFcntl(m, di, si, dx));
    SYSCALL(0x049, OpFlock(m, di, si));
    SYSCALL(0x04A, OpFsync(m, di));
    SYSCALL(0x04B, OpFdatasync(m, di));
    SYSCALL(0x04C, OpTruncate(m, di, si));
    SYSCALL(0x04D, OpFtruncate(m, di, si));
    SYSCALL(0x04F, OpGetcwd(m, di, si));
    SYSCALL(0x050, OpChdir(m, di));
    SYSCALL(0x052, OpRename(m, di, si));
    SYSCALL(0x053, OpMkdir(m, di, si));
    SYSCALL(0x054, OpRmdir(m, di));
    SYSCALL(0x055, OpCreat(m, di, si));
    SYSCALL(0x056, OpLink(m, di, si));
    SYSCALL(0x057, OpUnlink(m, di));
    SYSCALL(0x058, OpSymlink(m, di, si));
    SYSCALL(0x05A, OpChmod(m, di, si));
    SYSCALL(0x05B, OpFchmod(m, di, si));
    SYSCALL(0x05F, OpUmask(m, di));
    SYSCALL(0x060, OpGettimeofday(m, di, si));
    SYSCALL(0x061, OpGetrlimit(m, di, si));
    SYSCALL(0x062, OpGetrusage(m, di, si));
    SYSCALL(0x070, OpSetsid(m));
    SYSCALL(0x079, OpGetpgid(m, di));
    SYSCALL(0x06D, OpSetpgid(m, di, si));
    SYSCALL(0x066, OpGetuid(m));
    SYSCALL(0x068, OpGetgid(m));
    SYSCALL(0x069, OpSetuid(m, di));
    SYSCALL(0x06A, OpSetgid(m, di));
    SYSCALL(0x06B, OpGeteuid(m));
    SYSCALL(0x06C, OpGetegid(m));
    SYSCALL(0x06E, OpGetppid(m));
    SYSCALL(0x082, OpSigsuspend(m, di));
    SYSCALL(0x083, OpSigaltstack(m, di, si));
    SYSCALL(0x085, OpMknod(m, di, si, dx));
    SYSCALL(0x09D, OpPrctl(m, di, si, dx, r0, r8));
    SYSCALL(0x09E, OpArchPrctl(m, di, si));
    SYSCALL(0x0A0, OpSetrlimit(m, di, si));
    SYSCALL(0x0D9, OpGetdents(m, di, si, dx));
    SYSCALL(0x0DA, OpSetTidAddress(m, di));
    SYSCALL(0x0E4, OpClockGettime(m, di, si));
    SYSCALL(0x0E5, OpClockGetres(m, di, si));
    SYSCALL(0x0E6, OpClockNanosleep(m, di, si, dx, r0));
    SYSCALL(0x0EB, OpUtimes(m, di, si));
    SYSCALL(0x101, OpOpenat(m, di, si, dx, r0));
    SYSCALL(0x102, OpMkdirat(m, di, si, dx));
    SYSCALL(0x106, OpFstatat(m, di, si, dx, r0));
    SYSCALL(0x107, OpUnlinkat(m, di, si, dx));
    SYSCALL(0x108, OpRenameat(m, di, si, dx, r0));
    SYSCALL(0x10B, OpReadlinkat(m, di, si, dx, r0));
    SYSCALL(0x10D, OpFaccessat(m, di, si, dx, r0));
    SYSCALL(0x120, OpAccept4(m, di, si, dx, r0));
    SYSCALL(0x13e, OpGetrandom(m, di, si, dx));
    SYSCALL(0x1b4, OpCloseRange(m, di, si, dx));
    case 0x3C:
      OpExit(m, di);
    case 0xE7:
      OpExitGroup(m, di);
    case 0x00F:
      OpRestore(m);
      return;
    default:
      LOGF("missing syscall 0x%03" PRIx64, ax);
      ax = enosys();
      break;
  }
  Write64(m->ax, ax != -1 ? ax : -(XlatErrno(errno) & 0xfff));
  CollectGarbage(m);
}
