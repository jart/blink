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
#include "blink/mop.h"
#include "blink/pml4t.h"
#include "blink/random.h"
#include "blink/signal.h"
#include "blink/swap.h"
#include "blink/syscall.h"
#include "blink/timespec.h"
#include "blink/timeval.h"
#include "blink/util.h"
#include "blink/xlat.h"

#define POLLING_INTERVAL_MS 50

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

const struct FdCb kFdCbHost = {
    .close = close,
    .readv = readv,
    .writev = writev,
    .ioctl = SystemIoctl,
    .poll = poll,
};

void AddStdFd(struct Fds *fds, int fildes) {
  int flags;
  struct Fd *fd;
  if ((flags = fcntl(fildes, F_GETFL)) >= 0) {
    unassert((fd = AllocateFd(fds, fildes, flags)));
    unassert(fd->fildes == fildes);
    atomic_store_explicit(&fd->systemfd, fildes, memory_order_release);
  }
}

struct Fd *GetAndLockFd(struct Machine *m, int fildes) {
  struct Fd *fd;
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) LockFd(fd);
  UnlockFds(&m->system->fds);
  return fd;
}

int GetFildes(struct Machine *m, int fildes) {
  int systemfd;
  struct Fd *fd;
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    systemfd = atomic_load_explicit(&fd->systemfd, memory_order_relaxed);
  } else {
    systemfd = -1;
  }
  UnlockFds(&m->system->fds);
  return systemfd;
}

static int GetDirFildes(struct Machine *m, int fildes) {
  if (fildes == AT_FDCWD_LINUX) return AT_FDCWD;
  return GetFildes(m, fildes);
}

int GetAfd(struct Machine *m, int fildes, struct Fd **out_fd) {
  struct Fd *fd;
  if (fildes == AT_FDCWD_LINUX) {
    *out_fd = 0;
    return 0;
  } else if ((fd = GetFd(&m->system->fds, fildes))) {
    *out_fd = fd;
    return 0;
  } else {
    return -1;
  }
}

static int AppendIovsReal(struct Machine *m, struct Iovs *ib, i64 addr,
                          u64 size) {
  void *real;
  unsigned got;
  u64 have;
  while (size) {
    if (!(real = FindReal(m, addr))) return efault();
    have = 4096 - (addr & 4095);
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
  atomic_int *ctid;
  if (m->ctid && (ctid = (atomic_int *)FindReal(m, m->ctid))) {
    atomic_store_explicit(ctid, 0, memory_order_release);
  }
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

static void *OnSpawn(void *arg) {
  int rc;
  u8 *ctid;
  sigset_t ss;
  struct Machine *m = (struct Machine *)arg;
  sigfillset(&ss);
  unassert(!sigprocmask(SIG_SETMASK, &ss, 0));
  if (!(rc = setjmp(m->onhalt))) {
    if (m->ctid && (ctid = FindReal(m, m->ctid))) {
      Store32(ctid, m->tid);
    }
    Actor(m);
  } else {
    LOGF("halting machine from thread: %d", rc);
    exit(rc);
  }
}

static int OpSpawn(struct Machine *m, u64 flags, u64 stack, u64 ptid, u64 ctid,
                   u64 tls, u64 func) {
  int tid, err;
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
  Put64(m2->ax, 0);
  m2->fs = tls;
  Put64(m2->sp, stack);
  unassert(!pthread_attr_init(&attr));
  unassert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  err = pthread_create(&m2->thread, &attr, OnSpawn, m2);
  unassert(!pthread_attr_destroy(&attr));
  if (err) {
    FreeMachine(m2);
    return eagain();
  }
  Store32(ResolveAddress(m, ptid), tid);
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
  u8 buf[8];
  switch (code) {
    case ARCH_SET_GS_LINUX:
      m->gs = addr;
      return 0;
    case ARCH_SET_FS_LINUX:
      m->fs = addr;
      return 0;
    case ARCH_GET_GS_LINUX:
      Write64(buf, m->gs);
      VirtualRecvWrite(m, addr, buf, 8);
      return 0;
    case ARCH_GET_FS_LINUX:
      Write64(buf, m->fs);
      VirtualRecvWrite(m, addr, buf, 8);
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
                  int fildes, i64 offset) {
  u64 key;
  void *tmp;
  ssize_t rc;
  struct Fd *fd;
  if (flags & MAP_GROWSDOWN_LINUX) {
    errno = ENOTSUP;
    return -1;
  }
  if (prot & PROT_READ) {
    key = PAGE_RSRV | PAGE_U | PAGE_V;
    if (prot & PROT_WRITE) key |= PAGE_RW;
    if (!(prot & PROT_EXEC)) key |= PAGE_XD;
    flags = XlatMapFlags(flags);
    if (fildes != -1) {
      if (flags & MAP_ANONYMOUS) {
        return einval();
      }
      if (!(fd = GetAndLockFd(m, fildes))) {
        return -1;
      }
    } else {
      fd = 0;
    }
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
      if (fd) {
        // TODO(jart): Raise SIGBUS on i/o error.
        // TODO(jart): Support lazy file mappings.
        unassert((tmp = malloc(size)));
        for (;;) {
          rc = pread(fd->systemfd, tmp, size, offset);
          if (rc != -1) break;
          if (errno != EINTR) {
            LOGF("failed to read %zu bytes at offset %" PRId64
                 " from fd %d into memory map: %s",
                 size, offset, fd->systemfd, strerror(errno));
            abort();
          }
        }
        UnlockFd(fd);
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

static int OpDup1(struct Machine *m, i32 fildes) {
  return OpDup(m, fildes, -1, 0, 0);
}

static int OpDup2(struct Machine *m, i32 fildes, i32 newfildes) {
  if (newfildes < 0) return ebadf();
  return OpDup(m, fildes, newfildes, 0, 0);
}

static int OpDup3(struct Machine *m, i32 fildes, i32 newfildes, i32 flags) {
  if (newfildes < 0) return ebadf();
  if (fildes == newfildes) return einval();
  return OpDup(m, fildes, -1, 0, 0);
}

static void FixupSock(int systemfd, int flags) {
  if (flags & SOCK_CLOEXEC_LINUX) {
    unassert(!fcntl(systemfd, F_SETFD, FD_CLOEXEC));
  }
  if (flags & SOCK_NONBLOCK_LINUX) {
    unassert(!fcntl(systemfd, F_SETFL, O_NDELAY));
  }
}

void DropFd(struct Machine *m, struct Fd *fd) {
  LockFds(&m->system->fds);
  FreeFd(&m->system->fds, fd);
  UnlockFds(&m->system->fds);
}

static int OpSocket(struct Machine *m, i32 family, i32 type, i32 protocol) {
  struct Fd *fd;
  int flags, fildes, systemfd;
  flags = type & (SOCK_NONBLOCK_LINUX | SOCK_CLOEXEC_LINUX);
  type &= ~(SOCK_NONBLOCK_LINUX | SOCK_CLOEXEC_LINUX);
  if ((family = XlatSocketFamily(family)) == -1) return -1;
  if ((type = XlatSocketType(type)) == -1) return -1;
  if ((protocol = XlatSocketProtocol(protocol)) == -1) return -1;
  LockFds(&m->system->fds);
  fd = AllocateFd(&m->system->fds, 0,
                  O_RDWR | (flags & SOCK_CLOEXEC_LINUX ? O_CLOEXEC : 0) |
                      (flags & SOCK_NONBLOCK_LINUX ? O_NDELAY : 0));
  UnlockFds(&m->system->fds);
  if (fd) {
    if ((systemfd = socket(family, type, protocol)) != -1) {
      FixupSock(systemfd, flags);
      fildes = fd->fildes;
      atomic_store_explicit(&fd->systemfd, systemfd, memory_order_release);
    } else {
      fildes = -1;
    }
  } else {
    fildes = -1;
  }
  if (fildes == -1 && fd) DropFd(m, fd);
  return fildes;
}

static int OpSocketName(struct Machine *m, i32 fildes, i64 aa, i64 asa,
                        int SocketName(int, struct sockaddr *, socklen_t *)) {
  int rc;
  u32 addrsize;
  struct Fd *fd;
  u8 gaddrsize[4];
  struct sockaddr_in addr;
  struct sockaddr_in_linux gaddr;
  VirtualSendRead(m, gaddrsize, asa, sizeof(gaddrsize));
  if (Read32(gaddrsize) < sizeof(gaddr)) return einval();
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  addrsize = sizeof(addr);
  rc = SocketName(fd->systemfd, (struct sockaddr *)&addr, &addrsize);
  if (rc != -1) {
    Write32(gaddrsize, sizeof(gaddr));
    XlatSockaddrToLinux(&gaddr, &addr);
    VirtualRecv(m, asa, gaddrsize, sizeof(gaddrsize));
    VirtualRecvWrite(m, aa, &gaddr, sizeof(gaddr));
  }
  UnlockFd(fd);
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

static int OpAccept4(struct Machine *m, i32 fildes, i64 aa, i64 asa,
                     i32 flags) {
  u32 addrsize;
  int systemfd;
  u8 gaddrsize[4];
  struct Fd *fd1, *fd2;
  struct sockaddr_in addr;
  struct sockaddr_in_linux gaddr;
  if (m->system->redraw) m->system->redraw();
  if (flags & ~(SOCK_CLOEXEC_LINUX | SOCK_NONBLOCK_LINUX)) return einval();
  if (aa) {
    VirtualSendRead(m, gaddrsize, asa, sizeof(gaddrsize));
    if (Read32(gaddrsize) < sizeof(gaddr)) return einval();
  }
  LockFds(&m->system->fds);
  if ((fd1 = GetFd(&m->system->fds, fildes))) {
    LockFd(fd1);
    fd2 = AllocateFd(&m->system->fds, 0,
                     O_RDWR | (flags & SOCK_CLOEXEC_LINUX ? O_CLOEXEC : 0) |
                         (flags & SOCK_NONBLOCK_LINUX ? O_NDELAY : 0));
  } else {
    fd2 = 0;
  }
  UnlockFds(&m->system->fds);
  if (fd1 && fd2) {
    addrsize = sizeof(addr);
    systemfd = atomic_load_explicit(&fd1->systemfd, memory_order_relaxed);
    systemfd = accept(systemfd, (struct sockaddr *)&addr, &addrsize);
    if (systemfd != -1) {
      FixupSock(systemfd, flags);
      if (aa) {
        Write32(gaddrsize, sizeof(gaddr));
        XlatSockaddrToLinux(&gaddr, &addr);
        VirtualRecv(m, asa, gaddrsize, sizeof(gaddrsize));
        VirtualRecvWrite(m, aa, &gaddr, sizeof(gaddr));
      }
      fildes = fd2->fildes;
      atomic_store_explicit(&fd2->systemfd, systemfd, memory_order_release);
    } else {
      fildes = -1;
    }
  } else {
    fildes = -1;
  }
  if (fd1) UnlockFd(fd1);
  if (fd2 && fildes == -1) DropFd(m, fd2);
  return fildes;
}

static int OpConnectBind(struct Machine *m, i32 fildes, i64 aa, u32 as,
                         int impl(int, const struct sockaddr *, u32)) {
  int rc;
  struct Fd *fd;
  struct sockaddr_in addr;
  struct sockaddr_in_linux gaddr;
  if (as != sizeof(gaddr)) return einval();
  VirtualSendRead(m, &gaddr, aa, sizeof(gaddr));
  if (XlatSockaddrToHost(&addr, &gaddr) == -1) return -1;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  rc = impl(fd->systemfd, (const struct sockaddr *)&addr, sizeof(addr));
  UnlockFd(fd);
  return rc;
}

static int OpBind(struct Machine *m, int fd, i64 aa, u32 as) {
  return OpConnectBind(m, fd, aa, as, bind);
}

static int OpConnect(struct Machine *m, int fd, i64 aa, u32 as) {
  return OpConnectBind(m, fd, aa, as, connect);
}

static int OpSetsockopt(struct Machine *m, i32 fildes, i32 level, i32 optname,
                        i64 optvaladdr, u32 optvalsize) {
  int rc;
  void *optval;
  struct Fd *fd;
  if (optvalsize > 256) return einval();
  if ((level = XlatSocketLevel(level)) == -1) return -1;
  if ((optname = XlatSocketOptname(level, optname)) == -1) return -1;
  if (!(optval = calloc(1, optvalsize))) return -1;
  if (!(fd = GetAndLockFd(m, fildes))) {
    free(optval);
    return -1;
  }
  VirtualSendRead(m, optval, optvaladdr, optvalsize);
  rc = setsockopt(fd->systemfd, level, optname, optval, optvalsize);
  UnlockFd(fd);
  free(optval);
  return rc;
}

static i64 OpRead(struct Machine *m, i32 fildes, i64 addr, u64 size) {
  i64 rc;
  struct Fd *fd;
  struct Iovs iv;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  unassert(fd->cb);
  InitIovs(&iv);
  if ((rc = AppendIovsReal(m, &iv, addr, size)) != -1 &&
      (rc = fd->cb->readv(fd->systemfd, iv.p, iv.i)) != -1) {
    SetWriteAddr(m, addr, rc);
  }
  UnlockFd(fd);
  FreeIovs(&iv);
  return rc;
}

static i64 OpWrite(struct Machine *m, i32 fildes, i64 addr, u64 size) {
  i64 rc;
  struct Fd *fd;
  struct Iovs iv;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  unassert(fd->cb);
  InitIovs(&iv);
  if ((rc = AppendIovsReal(m, &iv, addr, size)) != -1 &&
      (rc = fd->cb->writev(fd->systemfd, iv.p, iv.i)) != -1) {
    SetReadAddr(m, addr, rc);
  }
  UnlockFd(fd);
  FreeIovs(&iv);
  return rc;
}

static i64 OpReadv(struct Machine *m, i32 fildes, i64 iovaddr, i32 iovlen) {
  i64 rc;
  struct Fd *fd;
  struct Iovs iv;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  unassert(fd->cb);
  InitIovs(&iv);
  if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen)) != -1) {
    rc = fd->cb->readv(fd->systemfd, iv.p, iv.i);
  }
  FreeIovs(&iv);
  return rc;
}

static i64 OpWritev(struct Machine *m, i32 fildes, i64 iovaddr, i32 iovlen) {
  i64 rc;
  struct Fd *fd;
  struct Iovs iv;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  unassert(fd->cb);
  InitIovs(&iv);
  if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen)) != -1) {
    rc = fd->cb->writev(fd->systemfd, iv.p, iv.i);
  }
  FreeIovs(&iv);
  return rc;
}

static int UnXlatDt(int x) {
  switch (x) {
    XLAT(DT_UNKNOWN, DT_UNKNOWN_LINUX);
    XLAT(DT_FIFO, DT_FIFO_LINUX);
    XLAT(DT_CHR, DT_CHR_LINUX);
    XLAT(DT_DIR, DT_DIR_LINUX);
    XLAT(DT_BLK, DT_BLK_LINUX);
    XLAT(DT_REG, DT_REG_LINUX);
    XLAT(DT_LNK, DT_LNK_LINUX);
    XLAT(DT_SOCK, DT_SOCK_LINUX);
    default:
      __builtin_unreachable();
  }
}

static i64 OpGetdents(struct Machine *m, i32 fildes, i64 addr, i64 size) {
  int type;
  long off;
  int reclen;
  i64 i, dlz;
  size_t len;
  struct Fd *fd;
  struct dirent *ent;
  struct dirent_linux rec;
  dlz = sizeof(struct dirent_linux);
  if (size < dlz) return einval();
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  if (!fd->dirstream && !(fd->dirstream = fdopendir(fd->systemfd))) {
    UnlockFd(fd);
    return -1;
  }
  for (i = 0; i + dlz <= size; i += reclen) {
    unassert((off = telldir(fd->dirstream)) >= 0);
    if (!(ent = readdir(fd->dirstream))) break;
    len = strlen(ent->d_name);
    if (len + 1 > sizeof(rec.d_name)) {
      LOGF("ignoring %ld byte d_name: %s", len, ent->d_name);
      reclen = 0;
      continue;
    }
    type = UnXlatDt(ent->d_type);
    reclen = 8 + 8 + 2 + 1 + len + 1;
    Write64(rec.d_ino, 0);
    Write64(rec.d_off, off);
    Write16(rec.d_reclen, reclen);
    Write8(rec.d_type, type);
    strcpy(rec.d_name, ent->d_name);
    VirtualRecvWrite(m, addr + i, &rec, reclen);
  }
  UnlockFd(fd);
  return i;
}

static int OpSetTidAddress(struct Machine *m, i64 ctid) {
  m->ctid = ctid;
  return m->tid;
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

static int OpIoctl(struct Machine *m, int fildes, u64 request, i64 addr) {
  struct Fd *fd;
  int rc, systemfd;
  int (*func)(int, unsigned long, ...);
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  unassert(fd->cb);
  func = fd->cb->ioctl;
  systemfd = atomic_load_explicit(&fd->systemfd, memory_order_relaxed);
  switch (request) {
    case TIOCGWINSZ_LINUX:
      rc = IoctlTiocgwinsz(m, systemfd, addr, func);
      break;
    case TCGETS_LINUX:
      rc = IoctlTcgets(m, systemfd, addr, func);
      break;
    case TCSETS_LINUX:
      rc = IoctlTcsets(m, systemfd, TCSETS, addr, func);
      break;
    case TCSETSW_LINUX:
      rc = IoctlTcsets(m, systemfd, TCSETSW, addr, func);
      break;
    case TCSETSF_LINUX:
      rc = IoctlTcsets(m, systemfd, TCSETSF, addr, func);
      break;
    default:
      rc = einval();
      break;
  }
  UnlockFd(fd);
  return rc;
}

static i64 OpLseek(struct Machine *m, i32 fildes, i64 offset, int whence) {
  i64 rc;
  struct Fd *fd;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  if (!fd->dirstream) {
    rc = lseek(fd->systemfd, offset, XlatWhence(whence));
  } else if (whence == SEEK_SET_LINUX) {
    seekdir(fd->dirstream, offset);
    rc = 0;
  } else {
    rc = einval();
  }
  UnlockFd(fd);
  return rc;
}

static i64 OpFtruncate(struct Machine *m, i32 fd, i64 size) {
  return ftruncate(GetFildes(m, fd), size);
}

static int OpFaccessat(struct Machine *m, i32 dirfd, i64 path, i32 mode,
                       i32 flags) {
  return faccessat(GetDirFildes(m, dirfd), LoadStr(m, path), XlatAccess(mode),
                   XlatAtf(flags));
}

static int OpFstatat(struct Machine *m, i32 dirfd, i64 path, i64 staddr,
                     i32 flags) {
  int rc;
  struct stat st;
  struct stat_linux gst;
  if ((rc = fstatat(GetDirFildes(m, dirfd), LoadStr(m, path), &st,
                    XlatAtf(flags))) != -1) {
    XlatStatToLinux(&gst, &st);
    VirtualRecvWrite(m, staddr, &gst, sizeof(gst));
  }
  return rc;
}

static int OpFstat(struct Machine *m, i32 fd, i64 staddr) {
  int rc;
  struct stat st;
  struct stat_linux gst;
  if ((rc = fstat(GetFildes(m, fd), &st)) != -1) {
    XlatStatToLinux(&gst, &st);
    VirtualRecvWrite(m, staddr, &gst, sizeof(gst));
  }
  return rc;
}

static int OpFsync(struct Machine *m, i32 fd) {
  return fsync(GetFildes(m, fd));
}

static int OpFdatasync(struct Machine *m, i32 fd) {
  return fdatasync(GetFildes(m, fd));
}

static int OpChdir(struct Machine *m, i64 path) {
  return chdir(LoadStr(m, path));
}

static int OpFlock(struct Machine *m, i32 fd, i32 lock) {
  return flock(GetFildes(m, fd), XlatLock(lock));
}

static int OpShutdown(struct Machine *m, i32 fd, i32 how) {
  return shutdown(GetFildes(m, fd), XlatShutdown(how));
}

static int OpListen(struct Machine *m, i32 fd, i32 backlog) {
  return listen(GetFildes(m, fd), backlog);
}

static int OpMkdir(struct Machine *m, i64 path, i32 mode) {
  return mkdir(LoadStr(m, path), mode);
}

static int OpFchmod(struct Machine *m, i32 fd, u32 mode) {
  return fchmod(GetFildes(m, fd), mode);
}

static int OpFcntl(struct Machine *m, i32 fildes, i32 cmd, i64 arg) {
  int rc, fl;
  struct Fd *fd;
  if (cmd == F_DUPFD_LINUX) {
    return OpDup(m, fildes, -1, 0, arg);
  }
  if (cmd == F_DUPFD_CLOEXEC_LINUX) {
    return OpDup(m, fildes, -1, O_CLOEXEC_LINUX, arg);
  }
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  if (cmd == F_GETFD_LINUX) {
    rc = fd->cloexec ? FD_CLOEXEC_LINUX : 0;
  } else if (cmd == F_GETFL_LINUX) {
    rc = UnXlatOpenFlags(fd->oflags);
  } else if (cmd == F_SETFD_LINUX) {
    if (!(arg & ~FD_CLOEXEC_LINUX)) {
      if (fcntl(fd->systemfd, F_SETFD, arg ? FD_CLOEXEC : 0) != -1) {
        fd->cloexec = !!arg;
        rc = 0;
      } else {
        rc = -1;
      }
    } else {
      rc = einval();
    }
  } else if (cmd == F_SETFL_LINUX) {
    fl = XlatOpenFlags(arg & (O_APPEND_LINUX | O_ASYNC_LINUX | O_DIRECT_LINUX |
                              O_NOATIME_LINUX | O_NDELAY_LINUX));
    if (fcntl(fd->systemfd, F_SETFL, fl) != -1) {
      fd->oflags &= ~(O_APPEND | O_ASYNC |
#ifdef O_DIRECT
                      O_DIRECT |
#endif
#ifdef O_NOATIME
                      O_NOATIME |
#endif
                      O_NDELAY);
      fd->oflags |= fl;
      rc = 0;
    } else {
      rc = -1;
    }
  } else {
    LOGF("missing fcntl() command %" PRId32, cmd);
    rc = einval();
  }
  UnlockFd(fd);
  return rc;
}

static int OpRmdir(struct Machine *m, i64 path) {
  return rmdir(LoadStr(m, path));
}

static int OpUnlink(struct Machine *m, i64 path) {
  return unlink(LoadStr(m, path));
}

static int OpRename(struct Machine *m, i64 srcpath, i64 dstpath) {
  return rename(LoadStr(m, srcpath), LoadStr(m, dstpath));
}

static int OpChmod(struct Machine *m, i64 path, u32 mode) {
  return chmod(LoadStr(m, path), mode);
}

static int OpTruncate(struct Machine *m, i64 path, u64 length) {
  return truncate(LoadStr(m, path), length);
}

static int OpSymlink(struct Machine *m, i64 targetpath, i64 linkpath) {
  return symlink(LoadStr(m, targetpath), LoadStr(m, linkpath));
}

static int OpMkdirat(struct Machine *m, i32 dirfd, i64 path, i32 mode) {
  return mkdirat(GetDirFildes(m, dirfd), LoadStr(m, path), mode);
}

static int OpLink(struct Machine *m, i64 existingpath, i64 newpath) {
  return link(LoadStr(m, existingpath), LoadStr(m, newpath));
}

static int OpMknod(struct Machine *m, i64 path, i32 mode, u64 dev) {
  return mknod(LoadStr(m, path), mode, dev);
}

static int OpUnlinkat(struct Machine *m, i32 dirfd, i64 path, i32 flags) {
  return unlinkat(GetDirFildes(m, dirfd), LoadStr(m, path), XlatAtf(flags));
}

static int OpRenameat(struct Machine *m, int srcdirfd, i64 srcpath,
                      int dstdirfd, i64 dstpath) {
  return renameat(GetDirFildes(m, srcdirfd), LoadStr(m, srcpath),
                  GetDirFildes(m, dstdirfd), LoadStr(m, dstpath));
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

static int OpGetrusage(struct Machine *m, i32 resource, i64 rusageaddr) {
  int rc;
  struct rusage hrusage;
  struct rusage_linux grusage;
  if ((rc = getrusage(XlatRusage(resource), &hrusage)) != -1) {
    XlatRusageToLinux(&grusage, &hrusage);
    VirtualRecvWrite(m, rusageaddr, &grusage, sizeof(grusage));
  }
  return rc;
}

static int OpGetrlimit(struct Machine *m, i32 resource, i64 rlimitaddr) {
  int rc;
  struct rlimit rlim;
  struct rlimit_linux lux;
  if ((rc = getrlimit(XlatResource(resource), &rlim)) != -1) {
    XlatRlimitToLinux(&lux, &rlim);
    VirtualRecvWrite(m, rlimitaddr, &lux, sizeof(lux));
  }
  return rc;
}

static int OpSetrlimit(struct Machine *m, i32 resource, i64 rlimitaddr) {
  struct rlimit rlim;
  struct rlimit_linux lux;
  VirtualSendRead(m, &lux, rlimitaddr, sizeof(lux));
  XlatRlimitToLinux(&lux, &rlim);
  return setrlimit(XlatResource(resource), &rlim);
}

static ssize_t OpReadlinkat(struct Machine *m, int dirfd, i64 path, i64 bufaddr,
                            i64 size) {
  char *buf;
  ssize_t rc;
  if (size < 0) return einval();
  if (size > PATH_MAX) return enomem();
  if (!(buf = (char *)malloc(size))) return -1;
  if ((rc = readlinkat(GetDirFildes(m, dirfd), LoadStr(m, path), buf, size)) !=
      -1) {
    VirtualRecvWrite(m, bufaddr, buf, rc);
  }
  free(buf);
  return rc;
}

static i64 OpGetcwd(struct Machine *m, i64 bufaddr, size_t size) {
  size_t n;
  char *buf;
  i64 res;
  if (!(buf = (char *)malloc(size))) return -1;
  if ((getcwd)(buf, size)) {
    n = strlen(buf) + 1;
    VirtualRecvWrite(m, bufaddr, buf, n);
    res = bufaddr;
  } else {
    res = -1;
  }
  free(buf);
  return res;
}

static ssize_t OpGetrandom(struct Machine *m, i64 a, size_t n, int f) {
  char *p;
  ssize_t rc;
  if (!(p = (char *)malloc(n))) return -1;
  if ((rc = GetRandom(p, n)) != -1) {
    VirtualRecvWrite(m, a, p, rc);
  }
  free(p);
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
  u8 gmask[8];
  sigset_t mask;
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
  if ((rc = clock_gettime(XlatClock(clock), &htimespec)) != -1) {
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
  if ((rc = clock_getres(XlatClock(clock), &htimespec)) != -1) {
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
  u64 gfdssize;
  struct Fd *fd;
  int fildes, rc, ev;
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
      unassert(!gettimeofday(&ts1, 0));
      for (;;) {
        for (i = 0; i < nfds; ++i) {
          fildes = Read32(gfds[i].fd);
          if ((fd = GetFd(&m->system->fds, fildes))) {
            hfds[0].fd =
                atomic_load_explicit(&fd->systemfd, memory_order_relaxed);
            ev = Read16(gfds[i].events);
            hfds[0].events = (((ev & POLLIN_LINUX) ? POLLIN : 0) |
                              ((ev & POLLOUT_LINUX) ? POLLOUT : 0) |
                              ((ev & POLLPRI_LINUX) ? POLLPRI : 0));
            switch (fd->cb->poll(hfds, 1, 0)) {
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
          unassert(!gettimeofday(&ts2, 0));
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
  u8 set[8], mask[8];
  if ((how = XlatSig(how)) != -1 && sigsetsize == sizeof(set)) {
    if (oldsetaddr) {
      Write64(mask, m->sigmask);
      VirtualRecvWrite(m, oldsetaddr, mask, sizeof(mask));
    }
    if (setaddr) {
      VirtualSendRead(m, set, setaddr, sizeof(set));
      if (how == SIG_BLOCK) {
        m->sigmask = m->sigmask | Read64(set);
      } else if (how == SIG_UNBLOCK) {
        m->sigmask = m->sigmask & ~Read64(set);
      } else {
        m->sigmask = Read64(set);
      }
    }
    return 0;
  } else {
    return einval();
  }
}

static int OpKill(struct Machine *m, int pid, int sig) {
  if (pid == getpid()) {
    // TODO(jart): Enqueue signal instead.
    ThrowProtectionFault(m);
  } else {
    return kill(pid, sig);
  }
}

static int OpTkill(struct Machine *m, int tid, int sig) {
  dll_element *e;
  bool gotsome = false;
  if (!(1 <= sig && sig <= 64)) return einval();
  unassert(!pthread_mutex_lock(&m->system->machines_lock));
  for (e = dll_first(m->system->machines); e;
       e = dll_next(m->system->machines, e)) {
    if (MACHINE_CONTAINER(e)->tid == tid) {
      MACHINE_CONTAINER(e)->signals |= 1ull << (sig - 1);
      gotsome = true;
      break;
    }
  }
  unassert(!pthread_mutex_unlock(&m->system->machines_lock));
  if (!gotsome) return esrch();
  return 0;
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
  return OpOpenat(m, AT_FDCWD_LINUX, path,
                  O_WRONLY_LINUX | O_CREAT_LINUX | O_TRUNC_LINUX, mode);
}

static int OpAccess(struct Machine *m, i64 path, int mode) {
  return OpFaccessat(m, AT_FDCWD_LINUX, path, mode, 0);
}

static int OpStat(struct Machine *m, i64 path, i64 st) {
  return OpFstatat(m, AT_FDCWD_LINUX, path, st, 0);
}

static int OpLstat(struct Machine *m, i64 path, i64 st) {
  return OpFstatat(m, AT_FDCWD_LINUX, path, st, 0x0400);
}

static int OpOpen(struct Machine *m, i64 path, int flags, int mode) {
  return OpOpenat(m, AT_FDCWD_LINUX, path, flags, mode);
}

static int OpAccept(struct Machine *m, int fd, i64 sa, i64 sas) {
  return OpAccept4(m, fd, sa, sas, 0);
}

void OpSyscall(struct Machine *m, u64 rde) {
  u64 ax, di, si, dx, r0, r8, r9;
  ax = Get64(m->ax);
  di = Get64(m->di);
  si = Get64(m->si);
  dx = Get64(m->dx);
  r0 = Get64(m->r10);
  r8 = Get64(m->r8);
  r9 = Get64(m->r9);
  switch (ax & 0x1ff) {
    SYSCALL(0x000, OpRead(m, di, si, dx));
    SYSCALL(0x001, OpWrite(m, di, si, dx));
    SYSCALL(0x002, OpOpen(m, di, si, dx));
    SYSCALL(0x003, OpClose(m->system, di));
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
    SYSCALL(0x016, OpPipe(m, di, 0));
    SYSCALL(0x125, OpPipe(m, di, si));
    SYSCALL(0x018, OpSchedYield(m));
    SYSCALL(0x01C, OpMadvise(m, di, si, dx));
    SYSCALL(0x020, OpDup1(m, di));
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
    SYSCALL(0x0c8, OpTkill(m, di, si));
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
    SYSCALL(0x1b4, OpCloseRange(m->system, di, si, dx));
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
  Put64(m->ax, ax != -1 ? ax : -(XlatErrno(errno) & 0xfff));
  CollectGarbage(m);
}
