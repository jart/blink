/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
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
#include "blink/syscall.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <inttypes.h>
#include <limits.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/ipc.h>
#include <sys/mman.h>
#include <sys/param.h>
#include <sys/shm.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "blink/ancillary.h"
#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/bitscan.h"
#include "blink/bus.h"
#include "blink/case.h"
#include "blink/checked.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/flag.h"
#include "blink/flags.h"
#include "blink/iovs.h"
#include "blink/limits.h"
#include "blink/linux.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/ndelay.h"
#include "blink/overlays.h"
#include "blink/pml4t.h"
#include "blink/preadv.h"
#include "blink/random.h"
#include "blink/signal.h"
#include "blink/stats.h"
#include "blink/strace.h"
#include "blink/swap.h"
#include "blink/thread.h"
#include "blink/timespec.h"
#include "blink/util.h"
#include "blink/vfs.h"
#include "blink/xlat.h"

#ifdef __linux
#include <sys/prctl.h>
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __HAIKU__
#include <OS.h>
#include <sys/sockio.h>
#endif

#ifdef HAVE_SCHED_H
#include <sched.h>
#endif

#ifdef HAVE_EPOLL_PWAIT1
#include <sys/epoll.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#ifdef __linux__
#include <sys/inotify.h>
#include <sys/signalfd.h>
#include <sys/syscall.h>
#include <sys/timerfd.h>
#include <sys/xattr.h>
#endif

#ifdef SO_LINGER_SEC
#define SO_LINGER_ SO_LINGER_SEC
#else
#define SO_LINGER_ SO_LINGER
#endif

#define SYSARGS0
#define SYSARGS1 , di
#define SYSARGS2 , di, si
#define SYSARGS3 , di, si, dx
#define SYSARGS4 , di, si, dx, r0
#define SYSARGS5 , di, si, dx, r0, r8
#define SYSARGS6 , di, si, dx, r0, r8, r9

#define SYSCALL(arity, ordinal, name, func, signature)            \
  case ordinal:                                                   \
    if (STRACE && FLAG_strace >= (signature)[0]) {                \
      Strace(m, name, true, &(signature)[1] SYSARGS##arity);      \
    }                                                             \
    ax = func(m SYSARGS##arity);                                  \
    if (STRACE && FLAG_strace) {                                  \
      Strace(m, name, false, &(signature)[1], ax SYSARGS##arity); \
    }                                                             \
    break

char *g_blink_path;
bool FLAG_statistics;

// delegate to work around function pointer errors, b/c
// old musl toolchains using `int ioctl(int, int, ...)`
static int SystemIoctl(int fd, unsigned long request, ...) {
  va_list va;
  uintptr_t arg;
  va_start(va, request);
  arg = va_arg(va, uintptr_t);
  va_end(va);
  return VfsIoctl(fd, request, (void *)arg);
}

#ifdef __EMSCRIPTEN__
// If this runs on the main thread, the browser is blocked until we return
// back to the main loop. Yield regularly when the process waits for some
// user input.

int em_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  int ret = VfsPoll(fds, nfds, timeout);
  if (ret == 0) emscripten_sleep(50);
  return ret;
}

ssize_t em_readv(int fd, const struct iovec *iov, int iovcnt) {
  // Handle blocking reads by waiting for POLLIN
  if ((VfsFcntl(fd, F_GETFL, 0) & O_NONBLOCK) == 0) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    while (em_poll(&pfd, 1, 50) == 0) {
    }
  }
  size_t ret = VfsReadv(fd, iov, iovcnt);
  if (ret == -1 && errno == EAGAIN) emscripten_sleep(50);
  return ret;
}
#endif

static int my_tcgetwinsize(int fd, struct winsize *ws) {
  return VfsIoctl(fd, TIOCGWINSZ, (void *)ws);
}

static int my_tcsetwinsize(int fd, const struct winsize *ws) {
  return VfsIoctl(fd, TIOCSWINSZ, (void *)ws);
}

const struct FdCb kFdCbHost = {
    .close = VfsClose,
#ifdef __EMSCRIPTEN__
    .readv = em_readv,
#else
    .readv = VfsReadv,
#endif
    .writev = VfsWritev,
#ifdef __EMSCRIPTEN__
    .poll = em_poll,
#else
    .poll = VfsPoll,
#endif
    .tcgetattr = VfsTcgetattr,
    .tcsetattr = VfsTcsetattr,
    .tcgetwinsize = my_tcgetwinsize,
    .tcsetwinsize = my_tcsetwinsize,
};

struct Fd *GetAndLockFd(struct Machine *m, int fildes) {
  struct Fd *fd;
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) LockFd(fd);
  UNLOCK(&m->system->fds.lock);
  return fd;
}

int GetDirFildes(int fildes) {
  if (fildes == AT_FDCWD_LINUX) return AT_FDCWD;
  return fildes;
}

const char *GetDirFildesPath(struct System *s, int fildes) {
  struct Fd *fd;
  if (fildes == AT_FDCWD_LINUX) return ".";
  if ((fd = GetFd(&s->fds, fildes))) return fd->path;
  return 0;
}

void SignalActor(struct Machine *m) {
  for (;;) {
    STATISTIC(++interps);
    // FreeBSD signal trampoline intercept — mirrors ExecuteInstruction.
    // SignalActor is used for *recursive* signal delivery (called from
    // CheckInterrupt when a host syscall returns EINTR). Without this
    // check, when the FreeBSD handler returns to 0x7fff0000 the dispatcher
    // tries to fetch an instruction there and segfaults.
    if (m->system->sigtramp && m->ip == m->system->sigtramp) {
      SigRestore(m);
      if (m->restored) break;
      continue;
    }
    JitlessDispatch(DISPATCH_NOTHING);
    if (atomic_load_explicit(&m->attention, memory_order_acquire)) {
      if (m->restored) break;
      CheckForSignals(m);
    }
  }
}

bool DeliverSignalRecursively(struct Machine *m, int sig) {
  bool issigsuspend;
  sigset_t unblock, oldmask;
  SYS_LOGF("recursing %s", DescribeSignal(sig));
  if (m->sigdepth >= kMaxSigDepth) {
    LOGF("exceeded max signal depth");
    return false;
  }
  // we're officially calling a signal handler
  // run the signal handler code inside the i/o routine
  // it's very important that no locks are currently held
  // garbage may exist on the freelist for calls like sendmsg
  ++m->sigdepth;
  if ((issigsuspend = m->issigsuspend)) {
    m->issigsuspend = false;
    unassert(!sigemptyset(&unblock));
    unassert(!pthread_sigmask(SIG_BLOCK, &unblock, &oldmask));
  }
  m->restored = false;
  m->insyscall = false;
  SignalActor(m);
  m->insyscall = true;
  m->restored = false;
  if (issigsuspend) {
    unassert(!pthread_sigmask(SIG_SETMASK, &oldmask, 0));
    m->issigsuspend = true;
  }
  --m->sigdepth;
  return true;
}

bool CheckInterrupt(struct Machine *m, bool restartable) {
  bool res, restart;
  int sig, delivered;
  // an actual i/o call just received EINTR from the kernel
  // if we're being killed, exit the thread immediately rather than
  // returning EINTR to guest code which may corrupt its state
  if (atomic_load_explicit(&m->killed, memory_order_acquire)) {
    SysExit(m, 0);
  }
HandleSomeMoreInterrupts:
  // determine if there's any signals pending for our guest
  Put64(m->ax, -EINTR_LINUX);
  if ((sig = ConsumeSignal(m, &delivered, &restart))) {
    TerminateSignal(m, sig, 0);
  }
  if (delivered) {
    if (DeliverSignalRecursively(m, delivered)) {
      if (restart && restartable) {
        // try to consume some more signals while we're here
        goto HandleSomeMoreInterrupts;
      } else {
        // let the i/o routine return eintr
        errno = EINTR;
        res = true;
      }
    } else {
      errno = EINTR;
      res = true;
    }
  } else {
    // no signal is being delivered
    res = false;
  }
  m->interrupted = res;
  return res;
}

static struct Futex *FindFutex(struct Machine *m, i64 addr) {
  struct Dll *e;
  for (e = dll_first(g_bus->futexes.active); e;
       e = dll_next(g_bus->futexes.active, e)) {
    if (FUTEX_CONTAINER(e)->addr == addr) {
      return FUTEX_CONTAINER(e);
    }
  }
  return 0;
}

static int SysFutexWake(struct Machine *m, i64 uaddr, u32 count) {
  int rc;
  struct Futex *f;
  if (!count) return 0;
  LOCK(&g_bus->futexes.lock);
  if ((f = FindFutex(m, uaddr))) {
    LOCK(&f->lock);
  }
  UNLOCK(&g_bus->futexes.lock);
  if (f && f->waiters) {
    THR_LOGF("pid=%d tid=%d is waking %d waiters at address %#" PRIx64,
             m->system->pid, m->tid, f->waiters, uaddr);
    if (count == 1) {
      unassert(!pthread_cond_signal(&f->cond));
      rc = 1;
    } else {
      unassert(!pthread_cond_broadcast(&f->cond));
      rc = f->waiters;
    }
    UNLOCK(&f->lock);
  } else {
    if (f) UNLOCK(&f->lock);
    THR_LOGF("pid=%d tid=%d is waking no one at address %#" PRIx64,
             m->system->pid, m->tid, uaddr);
    rc = 0;
  }
  return rc;
}

void WakeAllFutexes(void) {
  struct Dll *e;
  LOCK(&g_bus->futexes.lock);
  for (e = dll_first(g_bus->futexes.active); e;
       e = dll_next(g_bus->futexes.active, e)) {
    struct Futex *f = FUTEX_CONTAINER(e);
    LOCK(&f->lock);
    if (f->waiters) {
      pthread_cond_broadcast(&f->cond);
    }
    UNLOCK(&f->lock);
  }
  UNLOCK(&g_bus->futexes.lock);
}

static void ClearChildTid(struct Machine *m) {
#if defined(HAVE_FORK) || defined(HAVE_THREADS)
  _Atomic(int) *ctid;
  if (m->ctid) {
    THR_LOGF("ClearChildTid(%#" PRIx64 ")", m->ctid);
    if ((ctid = (_Atomic(int) *)LookupAddress(m, m->ctid))) {
      atomic_store_explicit(ctid, 0, memory_order_seq_cst);
    } else {
      THR_LOGF("invalid clear child tid address %#" PRIx64, m->ctid);
    }
  }
  SysFutexWake(m, m->ctid, INT_MAX);
#endif
}

_Noreturn void SysExitGroup(struct Machine *m, int rc) {
  /* fprintf(stderr, "exit_group(%d) pid=%d\n", rc, m->system->pid); */
  THR_LOGF("pid=%d tid=%d SysExitGroup", m->system->pid, m->tid);
  ClearChildTid(m);
  if (m->system->vfork_done_fd) {
    close(m->system->vfork_done_fd);
    m->system->vfork_done_fd = 0;
  }
  if (m->system->isfork) {
#ifndef NDEBUG
    if (FLAG_statistics) {
      PrintStats();
    }
#endif
    THR_LOGF("calling _Exit(%d)", rc);
    _Exit(rc);
  } else {
    THR_LOGF("calling exit(%d)", rc);
    KillOtherThreads(m->system);
#ifdef HAVE_JIT
    DisableJit(&m->system->jit);  // unmapping exec pages is slow
#endif
    if (m->system->trapexit && !m->system->exited) {
      m->system->exited = true;
      m->system->exitcode = rc;
      HaltMachine(m, kMachineExitTrap);
    }
    FreeMachine(m);
#ifdef HAVE_JIT
    ShutdownJit();
#endif
#ifndef NDEBUG
    if (FLAG_statistics) {
      PrintStats();
    }
#endif
    exit(rc);
  }
}

_Noreturn void SysExit(struct Machine *m, int rc) {
#ifdef HAVE_THREADS
  THR_LOGF("pid=%d tid=%d SysExit", m->system->pid, m->tid);
  if (IsOrphan(m)) {
    SysExitGroup(m, rc);
  } else {
    ClearChildTid(m);
    FreeMachine(m);
    WakeAllFutexes();
    pthread_exit(EXIT_SUCCESS);
  }
#else
  SysExitGroup(m, rc);
#endif
}

static int Fork(struct Machine *m, u64 flags, u64 stack, u64 ctid) {
  int pid, newpid = 0;
  _Atomic(int) *ctid_ptr;
  unassert(!m->path.jb);
  // NOTES ON THE LOCKING TOPOLOGY
  // exec_lock must come before sig_lock (see dup3)
  // exec_lock must come before fds.lock (see dup3)
  // exec_lock must come before fds.lock (see execve)
  // mmap_lock must come before fds.lock (see GetOflags)
  // mmap_lock must come before pagelocks_lock (see FreePage)
  if (m->threaded) {
    LOCK(&m->system->exec_lock);
    LOCK(&m->system->sig_lock);
    LOCK(&m->system->mmap_lock);
    LOCK(&m->system->pagelocks_lock);
    LOCK(&m->system->fds.lock);
    LOCK(&m->system->machines_lock);
#ifndef HAVE_PTHREAD_PROCESS_SHARED
    LOCK(&g_bus->futexes.lock);
#endif
#ifdef HAVE_JIT
    LOCK(&m->system->jit.lock);
#endif
  }
  pid = fork();
#ifdef __HAIKU__
  // haiku wipes tls after fork() in child
  // https://dev.haiku-os.org/ticket/17896
  if (!pid) g_machine = m;
#endif
  if (m->threaded) {
#ifdef HAVE_JIT
    UNLOCK(&m->system->jit.lock);
#endif
#ifndef HAVE_PTHREAD_PROCESS_SHARED
    UNLOCK(&g_bus->futexes.lock);
#endif
    UNLOCK(&m->system->machines_lock);
    UNLOCK(&m->system->fds.lock);
    UNLOCK(&m->system->pagelocks_lock);
    UNLOCK(&m->system->mmap_lock);
    UNLOCK(&m->system->sig_lock);
    UNLOCK(&m->system->exec_lock);
  }
  if (!pid) {
    newpid = getpid();
    if (stack) {
      Put64(m->sp, stack);
    }
#ifndef HAVE_PTHREAD_PROCESS_SHARED
    InitBus();
#endif
    THR_LOGF("pid=%d tid=%d SysFork -> pid=%d tid=%d",  //
             m->system->pid, m->tid, newpid, newpid);
    m->tid = m->system->pid = newpid;
    m->system->isfork = true;
    RemoveOtherThreads(m->system);
#ifdef __CYGWIN__
    // Cygwin doesn't seem to properly set the PROT_EXEC
    // protection for JIT blocks after forking.
    FixJitProtection(&m->system->jit);
#endif
    if ((flags & (CLONE_CHILD_SETTID_LINUX | CLONE_CHILD_CLEARTID_LINUX)) &&
        !(ctid & (sizeof(i32) - 1)) &&
        (ctid_ptr = (_Atomic(i32) *)LookupAddress(m, ctid))) {
      if (flags & CLONE_CHILD_SETTID_LINUX) {
        atomic_store_explicit(ctid_ptr, Little32(newpid), memory_order_release);
      }
      if (flags & CLONE_CHILD_CLEARTID_LINUX) {
        m->ctid = ctid;
      }
    }
  }
  return pid;
}

static int SysFork(struct Machine *m) {
  return Fork(m, 0, 0, 0);
}

static int SysFreeBSDpdfork(struct Machine* m, i64 fdpaddr, i32 flags) {
  int rc;
  int sv[2];
  struct Fd* fd;
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == -1) return -1;
  if ((rc = Fork(m, 0, 0, 0)) > 0) {
    close(sv[1]);
    LOCK(&m->system->fds.lock);
    if ((fd = AddFd(&m->system->fds, sv[0], O_CLOEXEC))) {
      int guestfd = fd->fildes;
      fd->pdpid = rc;  // remember the child pid for pdgetpid(2)
      UNLOCK(&m->system->fds.lock);
      if (CopyToUserWrite(m, fdpaddr, &guestfd, 4) == -1) {
        return -1;
      }
    } else {
      UNLOCK(&m->system->fds.lock);
      close(sv[0]);
      return -1;
    }
  } else if (!rc) {
    // Child: keep sv[1] open as a sentinel at a high fd that the emulated
    // program will never accidentally close (it's not in Blink's fd table).
    // When this child process exits, the sentinel closes and the parent's
    // sv[0] (the process descriptor) sees EOF — signaling child death.
    // Close sv[0] in the child; the child doesn't use the parent's pd end.
    int sentinel = fcntl(sv[1], F_DUPFD, 1000);
    close(sv[0]);
    close(sv[1]);
    if (sentinel == -1) _exit(127);
    (void)sentinel;
  } else {
    close(sv[0]);
    close(sv[1]);
  }
  return rc;
}

static int SysVfork(struct Machine *m) {
  int pid;
  int pipefd[2];
  char buf[1];
  if (pipe2(pipefd, O_CLOEXEC) == -1) {
    return SysFork(m);
  }
  pid = Fork(m, 0, 0, 0);
  if (pid > 0) {
    // Parent: block until child exec's or exits.
    close(pipefd[1]);
    (void)read(pipefd[0], buf, 1);
    close(pipefd[0]);
  } else if (pid == 0) {
    // Child: save write end; it signals the parent when closed.
    close(pipefd[0]);
    m->system->vfork_done_fd = pipefd[1];
  } else {
    close(pipefd[0]);
    close(pipefd[1]);
  }
  return pid;
}

static void *OnSpawn(void *arg) {
  int rc;
  struct Machine *m = (struct Machine *)arg;
  SYS_LOGF("pid=%d tid=%d OnSpawn ip=%#" PRIx64, m->system->pid, m->tid, (u64)m->ip);
  m->thread = pthread_self();
  if (!(rc = sigsetjmp(m->onhalt, 1))) {
    m->canhalt = true;
    unassert(!pthread_sigmask(SIG_SETMASK, &m->spawn_sigmask, 0));
  } else if (rc == kMachineFatalSystemSignal) {
    HandleFatalSystemSignal(m, &g_siginfo);
  }
  Blink(m);
}

#ifdef HAVE_THREADS
static int SysSpawn(struct Machine *m, u64 flags, u64 stack, u64 ptid, u64 ctid,
                    u64 tls, u64 func) {
  int tid;
  int err;
  int ignored;
  pthread_t thread;
  unsigned supported;
  unsigned mandatory;
  sigset_t ss, oldss;
  pthread_attr_t attr;
  _Atomic(int) *ptid_ptr;
  _Atomic(int) *ctid_ptr;
  struct Machine *m2 = 0;
  THR_LOGF("pid=%d tid=%d SysSpawn", m->system->pid, m->tid);
  if ((flags & 255) != 0 && (flags & 255) != SIGCHLD_LINUX) {
    LOGF("unsupported clone() signal: %" PRId64, flags & 255);
    return einval();
  }
  flags &= ~255;
  supported = CLONE_THREAD_LINUX | CLONE_VM_LINUX | CLONE_FS_LINUX |
              CLONE_FILES_LINUX | CLONE_SIGHAND_LINUX | CLONE_SETTLS_LINUX |
              CLONE_PARENT_SETTID_LINUX | CLONE_CHILD_CLEARTID_LINUX |
              CLONE_CHILD_SETTID_LINUX | CLONE_SYSVSEM_LINUX;
  mandatory = CLONE_THREAD_LINUX | CLONE_VM_LINUX | CLONE_FS_LINUX |
              CLONE_FILES_LINUX | CLONE_SIGHAND_LINUX;
  ignored = CLONE_DETACHED_LINUX | CLONE_IO_LINUX;
  flags &= ~ignored;
  if (flags & ~supported) {
    LOGF("unsupported clone() flags: %#" PRIx64, flags & ~supported);
    return einval();
  }
  if ((flags & mandatory) != mandatory) {
    LOGF("missing mandatory clone() thread flags: %#" PRIx64
         " out of %#" PRIx64,
         (flags & mandatory) ^ mandatory, flags);
    return einval();
  }
  if (((flags & CLONE_PARENT_SETTID_LINUX) &&
       ((ptid & (sizeof(int) - 1)) ||
        !IsValidMemory(m, ptid, 4, PROT_READ | PROT_WRITE) ||
        !(ptid_ptr = (_Atomic(int) *)LookupAddress(m, ptid)))) ||
      ((flags & CLONE_CHILD_SETTID_LINUX) &&
       ((ctid & (sizeof(int) - 1)) ||
        !IsValidMemory(m, ctid, 4, PROT_READ | PROT_WRITE) ||
        !(ctid_ptr = (_Atomic(int) *)LookupAddress(m, ctid))))) {
    LOGF("bad clone() ptid / ctid pointers: %#" PRIx64, flags);
    return efault();
  }
  m->threaded = true;
  m->system->jit.threaded = true;
  if (!(m2 = NewMachine(m->system, m))) {
    return eagain();
  }
  sigfillset(&ss);
  unassert(!pthread_sigmask(SIG_SETMASK, &ss, &oldss));
  tid = m2->tid;
  if (flags & CLONE_SETTLS_LINUX) {
    m2->fs.base = tls;
  }
  if (flags & CLONE_CHILD_CLEARTID_LINUX) {
    m2->ctid = ctid;
  }
  if (flags & CLONE_CHILD_SETTID_LINUX) {
    atomic_store_explicit(ctid_ptr, Little32(tid), memory_order_release);
  }
  Put64(m2->ax, 0);
  Put64(m2->sp, stack);
  m2->spawn_sigmask = oldss;
  unassert(!pthread_attr_init(&attr));
  unassert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  err = pthread_create(&thread, &attr, OnSpawn, m2);
  unassert(!pthread_attr_destroy(&attr));
  if (err) {
    FreeMachine(m2);
    unassert(!pthread_sigmask(SIG_SETMASK, &oldss, 0));
    return eagain();
  }
  if (flags & CLONE_PARENT_SETTID_LINUX) {
    atomic_store_explicit(ptid_ptr, Little32(tid), memory_order_release);
  }
  unassert(!pthread_sigmask(SIG_SETMASK, &oldss, 0));
  return tid;
}
#endif

static bool IsForkOrVfork(u64 flags) {
  u64 supported = CLONE_CHILD_SETTID_LINUX | CLONE_CHILD_CLEARTID_LINUX;
  return (flags & ~supported) == SIGCHLD_LINUX ||
         (flags & ~supported) ==
             (CLONE_VM_LINUX | CLONE_VFORK_LINUX | SIGCHLD_LINUX);
}

static int SysClone(struct Machine *m, u64 flags, u64 stack, u64 ptid, u64 ctid,
                    u64 tls, u64 func) {
  if (IsForkOrVfork(flags)) {
#ifdef HAVE_FORK
    return Fork(m, flags, stack, ctid);
#else
    LOGF("forking support disabled");
    return enosys();
#endif
  }
#ifdef HAVE_THREADS
  return SysSpawn(m, flags, stack, ptid, ctid, tls, func);
#else
  LOGF("threading support disabled");
  return enosys();
#endif
}

static struct Futex *NewFutex(i64 addr) {
  struct Dll *e;
  struct Futex *f;
  if (!(e = dll_first(g_bus->futexes.free))) {
    LOG_ONCE(LOGF("ran out of futexes"));
    enomem();
    return 0;
  }
  dll_remove(&g_bus->futexes.free, e);
  f = FUTEX_CONTAINER(e);
  f->waiters = 1;
  f->addr = addr;
  return f;
}

static void FreeFutex(struct Futex *f) {
  dll_make_first(&g_bus->futexes.free, &f->elem);
}

static int LoadTimespec(struct Machine *m, i64 addr, struct timespec *ts,
                        u64 mask, u64 need) {
  const struct timespec_linux *gt;
  if ((gt = (const struct timespec_linux *)Schlep(m, addr, sizeof(*gt), mask,
                                                  need))) {
    ts->tv_sec = Read64(gt->sec);
    ts->tv_nsec = Read64(gt->nsec);
    if (0 <= ts->tv_sec && (0 <= ts->tv_nsec && ts->tv_nsec < 1000000000)) {
      return 0;
    } else {
      return einval();
    }
  } else {
    return -1;
  }
}

static int LoadTimespecR(struct Machine *m, i64 addr, struct timespec *ts) {
  return LoadTimespec(m, addr, ts, PAGE_U, PAGE_U);
}

static int LoadTimespecRW(struct Machine *m, i64 addr, struct timespec *ts) {
  return LoadTimespec(m, addr, ts, PAGE_U | PAGE_RW, PAGE_U | PAGE_RW);
}

static int SysFutexWait(struct Machine *m,  //
                        i64 uaddr,          //
                        i32 op,             //
                        u32 expect,         //
                        i64 timeout_addr) {
  int rc;
  u8 *mem;
  struct Futex *f;
  const struct timespec_linux *gtimeout;
  struct timespec now, tick, timeout, deadline;
  now = tick = GetTime();
  if (timeout_addr) {
    if (!(gtimeout = (const struct timespec_linux *)SchlepR(
              m, timeout_addr, sizeof(*gtimeout)))) {
      return -1;
    }
    timeout.tv_sec = Read64(gtimeout->sec);
    timeout.tv_nsec = Read64(gtimeout->nsec);
    if (!(0 <= timeout.tv_nsec && timeout.tv_nsec < 1000000000)) {
      return einval();
    }
    deadline = AddTime(now, timeout);
  } else {
    deadline = GetMaxTime();
  }
  if (!(mem = LookupAddress(m, uaddr))) return -1;
  LOCK(&g_bus->futexes.lock);
  if (Load32(mem) != expect) {
    UNLOCK(&g_bus->futexes.lock);
    return eagain();
  }
  if ((f = FindFutex(m, uaddr))) {
    LOCK(&f->lock);
    ++f->waiters;
    UNLOCK(&f->lock);
  }
  if (!f) {
    if ((f = NewFutex(uaddr))) {
      dll_make_first(&g_bus->futexes.active, &f->elem);
    } else {
      UNLOCK(&g_bus->futexes.lock);
      return -1;
    }
  }
  UNLOCK(&g_bus->futexes.lock);
  THR_LOGF("pid=%d tid=%d is waiting at address %#" PRIx64, m->system->pid,
           m->tid, uaddr);
  do {
    if (m->killed) {
      rc = EAGAIN;
      break;
    }
    if (CheckInterrupt(m, true)) {
      rc = EINTR;
      break;
    }
    if (!(mem = LookupAddress(m, uaddr))) {
      rc = errno;
      break;
    }
    LOCK(&f->lock);
    if (Load32(mem) != expect) {
      rc = 0;
    } else {
      tick = AddTime(tick, FromMilliseconds(kPollingMs));
      if (CompareTime(tick, deadline) > 0) tick = deadline;
      rc = pthread_cond_timedwait(&f->cond, &f->lock, &tick);
      if (rc == ETIMEDOUT) {
        THR_LOGF("futex wait timed out");
      } else {
        THR_LOGF("futex wait returned %s", DescribeHostErrno(rc));
      }
    }
    UNLOCK(&f->lock);
  } while (rc == ETIMEDOUT && CompareTime(tick, deadline) < 0);
  LOCK(&g_bus->futexes.lock);
  LOCK(&f->lock);
  if (!--f->waiters) {
    dll_remove(&g_bus->futexes.active, &f->elem);
    UNLOCK(&f->lock);
    FreeFutex(f);
    UNLOCK(&g_bus->futexes.lock);
  } else {
    UNLOCK(&f->lock);
    UNLOCK(&g_bus->futexes.lock);
  }
  if (rc) {
    errno = rc;
    rc = -1;
  }
  return rc;
}

static int SysFutex(struct Machine *m,  //
                    i64 uaddr,          //
                    i32 op,             //
                    u32 val,            //
                    i64 timeout_addr,   //
                    i64 uaddr2,         //
                    u32 val3) {
  if (uaddr & 3) return efault();
  op &= ~FUTEX_PRIVATE_FLAG_LINUX;
  switch (op) {
    case FUTEX_WAIT_LINUX:
      return SysFutexWait(m, uaddr, op, val, timeout_addr);
    case FUTEX_WAKE_LINUX:
      return SysFutexWake(m, uaddr, val);
    case FUTEX_WAIT_BITSET_LINUX:
    case FUTEX_WAIT_BITSET_LINUX | FUTEX_CLOCK_REALTIME_LINUX:
      // will be supported soon
      // avoid logging when cosmo feature checks this
      if (!m->system->iscosmo) goto DefaultCase;
      return einval();
    default:
    DefaultCase:
      LOGF("unsupported %s op %#x", "futex", op);
      return einval();
  }
}

static void UnlockRobustFutex(struct Machine *m, u64 futex_addr,
                              bool ispending) {
  int owner;
  u32 value, replace;
  _Atomic(u32) *futex;
  if (futex_addr & 3) {
    LOGF("robust futex isn't aligned");
    return;
  }
  if (!(futex = (_Atomic(u32) *)SchlepR(m, futex_addr, 4))) {
    LOGF("encountered efault in robust futex list");
    return;
  }
  for (value = atomic_load_explicit(futex, memory_order_acquire);;) {
    owner = value & FUTEX_TID_MASK_LINUX;
    if (ispending && !owner) {
      THR_LOGF("unlocking pending ownerless futex");
      SysFutexWake(m, futex_addr, 1);
      return;
    }
    if (owner && owner != m->tid) {
      THR_LOGF("robust futex 0x%08" PRIx32
               " was owned by %d but we're tid=%d pid=%d",
               value, owner, m->tid, m->system->pid);
      return;
    }
    replace = FUTEX_OWNER_DIED_LINUX | (value & FUTEX_WAITERS_LINUX);
    if (atomic_compare_exchange_weak_explicit(futex, &value, replace,
                                              memory_order_release,
                                              memory_order_acquire)) {
      THR_LOGF("successfully unlocked robust futex");
      if (value & FUTEX_WAITERS_LINUX) {
        THR_LOGF("waking robust futex waiters");
        SysFutexWake(m, futex_addr, 1);
      }
      return;
    } else {
      THR_LOGF("robust futex cas failed");
    }
  }
}

void UnlockRobustFutexes(struct Machine *m) {
  if (1) return;  // TODO: Figure out how these work.
  int limit = 1000;
  bool once = false;
  u64 list, item, pending;
  struct robust_list_linux *data;
  if (!(item = list = m->robust_list)) return;
  m->robust_list = 0;
  pending = 0;
  do {
    if (!(data = (struct robust_list_linux *)SchlepR(m, item, sizeof(*data)))) {
      LOGF("encountered efault in robust futex list");
      break;
    }
    THR_LOGF("unlocking robust futex %#" PRIx64 " {.next=%#" PRIx64
             " .offset=%" PRId64 " .pending=%#" PRIx64 "}",
             item, Read64(data->next), Read64(data->offset),
             Read64(data->pending));
    if (!once) {
      pending = Read64(data->pending);
      once = true;
    }
    if (!--limit) {
      LOGF("encountered cycle or limit in robust futex list");
      break;
    }
    if (item != pending) {
      UnlockRobustFutex(m, item + Read64(data->offset), false);
    }
    item = Read64(data->next);
  } while (item != list);
  if (!pending) return;
  if (!(data =
            (struct robust_list_linux *)SchlepR(m, pending, sizeof(*data)))) {
    LOGF("encountered efault in robust futex list");
    return;
  }
  THR_LOGF("unlocking pending robust futex %#" PRIx64 " {.next=%#" PRIx64
           " .offset=%" PRId64 " .pending=%#" PRIx64 "}",
           item, Read64(data->next), Read64(data->offset),
           Read64(data->pending));
  UnlockRobustFutex(m, pending + Read64(data->offset), true);
}

static i32 ReturnRobustList(struct Machine *m, i64 head_ptr_addr,
                            i64 len_ptr_addr) {
  u8 buf[8];
  Write64(buf, m->robust_list);
  CopyToUserWrite(m, head_ptr_addr, buf, sizeof(buf));
  Write64(buf, sizeof(struct robust_list_linux));
  CopyToUserWrite(m, len_ptr_addr, buf, sizeof(buf));
  return 0;
}

static i32 SysGetRobustList(struct Machine *m, int pid, i64 head_ptr_addr,
                            i64 len_ptr_addr) {
  int rc;
  struct Dll *e;
  struct Machine *m2;
  if (!pid || pid == m->tid) {
    rc = ReturnRobustList(m, head_ptr_addr, len_ptr_addr);
  } else {
    rc = -1;
    errno = ESRCH;
    LOCK(&m->system->machines_lock);
    for (e = dll_first(m->system->machines); e;
         e = dll_next(m->system->machines, e)) {
      m2 = MACHINE_CONTAINER(e);
      if (m2->tid == pid) {
        rc = ReturnRobustList(m2, head_ptr_addr, len_ptr_addr);
        break;
      }
    }
    UNLOCK(&m->system->machines_lock);
  }
  return rc;
}

static i32 SysSetRobustList(struct Machine *m, i64 head_addr, u64 len) {
  if (len != sizeof(struct robust_list_linux)) return einval();
  if (!IsValidMemory(m, head_addr, len, PROT_READ | PROT_WRITE)) return -1;
  m->robust_list = head_addr;
  return 0;
}

static int ValidateAffinityPid(struct Machine *m, int pid) {
  if (pid < 0) return esrch();
  if (pid && pid != m->tid && pid != m->system->pid) return eperm();
  return 0;
}

static int SysSchedSetaffinity(struct Machine *m,  //
                               i32 pid,            //
                               u64 cpusetsize,     //
                               i64 maskaddr) {
  if (ValidateAffinityPid(m, pid) == -1) return -1;
#ifdef HAVE_SCHED_GETAFFINITY
  u8 *mask;
  size_t i, n;
  cpu_set_t sysmask;
  GetCpuCount();  // call for effect
  n = MIN(cpusetsize, CPU_SETSIZE / 8) * 8;
  if (!(mask = (u8 *)AddToFreeList(m, malloc(n / 8)))) return -1;
  if (CopyFromUserRead(m, mask, maskaddr, n / 8) == -1) return -1;
  CPU_ZERO(&sysmask);
  for (i = 0; i < n; ++i) {
    if (mask[i / 8] & (1 << (i % 8))) {
      CPU_SET(i, &sysmask);
    }
  }
  return sched_setaffinity(pid, sizeof(sysmask), &sysmask);
#else
  return 0;  // do nothing
#endif
}

static int SysSchedGetaffinity(struct Machine *m,  //
                               i32 pid,            //
                               u64 cpusetsize,     //
                               i64 maskaddr) {
  if (ValidateAffinityPid(m, pid) == -1) return -1;
#ifdef HAVE_SCHED_GETAFFINITY
  int rc;
  u8 *mask;
  size_t i, n;
  cpu_set_t sysmask;
  n = MIN(cpusetsize, CPU_SETSIZE / 8) * 8;
  if (!(mask = (u8 *)AddToFreeList(m, malloc(n / 8)))) return -1;
  rc = sched_getaffinity(pid, sizeof(sysmask), &sysmask);
  unassert(rc == 0 || rc == -1);
  if (!rc) {
    rc = n / 8;
    memset(mask, 0, n / 8);
    for (i = 0; i < n; ++i) {
      if (CPU_ISSET(i, &sysmask)) {
        mask[i / 8] |= 1 << (i % 8);
      }
    }
    if (CopyToUserWrite(m, maskaddr, mask, n / 8) == -1) rc = -1;
  }
  return rc;
#else
  u8 *mask;
  unsigned i, rc, count;
  count = GetCpuCount();
  rc = ROUNDUP(count, 64) / 8;
  if (cpusetsize < rc) return einval();
  if (!(mask = (u8 *)AddToFreeList(m, calloc(1, rc)))) return -1;
  count = MIN(count, rc * 8);
  for (i = 0; i < count; ++i) {
    mask[i / 8] |= 1 << (i % 8);
  }
  if (CopyToUserWrite(m, maskaddr, mask, rc) == -1) rc = -1;
  return rc;
#endif
}

static int SysPrctlGetTsc(struct Machine *m, i64 arg2) {
  u8 word[4];
  Write32(word, m->traprdtsc ? PR_TSC_SIGSEGV_LINUX : PR_TSC_ENABLE_LINUX);
  return CopyToUserWrite(m, arg2, word, sizeof(word));
}

static int SysPrctlSetTsc(struct Machine *m, i64 arg2) {
  switch (arg2) {
    case PR_TSC_ENABLE_LINUX:
      m->traprdtsc = false;
      return 0;
    case PR_TSC_SIGSEGV_LINUX:
      m->traprdtsc = true;
      return 0;
    default:
      return einval();
  }
}

static int SysPrctl(struct Machine *m, int op, i64 arg2, i64 arg3, i64 arg4,
                    i64 arg5) {
  switch (op) {
    case PR_GET_TSC_LINUX:
      return SysPrctlGetTsc(m, arg2);
    case PR_SET_TSC_LINUX:
      return SysPrctlSetTsc(m, arg2);
#ifdef PR_CAPBSET_DROP
    case PR_CAPBSET_DROP_LINUX:
      return prctl(PR_CAPBSET_DROP, arg2, arg3, arg4, arg5);
#else
    case PR_CAPBSET_DROP_LINUX:
      return einval();
#endif
#ifdef PR_SET_NO_NEW_PRIVS
    case PR_SET_NO_NEW_PRIVS_LINUX:
      return prctl(PR_SET_NO_NEW_PRIVS, arg2, arg3, arg4, arg5);
#else
    case PR_SET_NO_NEW_PRIVS_LINUX:
      return einval();
#endif
    case PR_GET_SECCOMP_LINUX:
    case PR_SET_SECCOMP_LINUX:
      // avoid noisy feature check warnings in cosmopolitan
      if (!m->system->iscosmo) goto DefaultCase;
      return einval();
    default:
    DefaultCase:
      LOGF("unsupported %s op %#x", "prctl", op);
      return einval();
  }
}

static int SysArchPrctl(struct Machine *m, int op, i64 addr) {
#ifndef DISABLE_NONPOSIX
  u8 buf[8];
#endif
  switch (op) {
    case ARCH_SET_FS_LINUX:
      m->fs.base = addr;
      return 0;
#ifndef DISABLE_NONPOSIX
    case ARCH_SET_GS_LINUX:
      m->gs.base = addr;
      return 0;
    case ARCH_GET_FS_LINUX:
      Write64(buf, m->fs.base);
      return CopyToUserWrite(m, addr, buf, 8);
    case ARCH_GET_GS_LINUX:
      Write64(buf, m->gs.base);
      return CopyToUserWrite(m, addr, buf, 8);
    case ARCH_GET_CPUID_LINUX:
      return !m->trapcpuid;
    case ARCH_SET_CPUID_LINUX:
      m->trapcpuid = !addr;
      return 0;
#endif
    default:
      LOGF("unsupported %s op %#x", "arch_prctl", op);
      return einval();
  }
}

static u64 Prot2Page(int prot) {
  u64 key = 0;
  if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) return einval();
  if (prot & PROT_READ) key |= PAGE_U;
  if (prot & PROT_WRITE) key |= PAGE_RW | PAGE_U;
  if (~prot & PROT_EXEC) key |= PAGE_XD;
  return key;
}

static int SysMprotect(struct Machine *m, i64 addr, u64 size, int prot) {
  _Static_assert(PROT_READ == 1, "");
  _Static_assert(PROT_WRITE == 2, "");
  _Static_assert(PROT_EXEC == 4, "");
  int rc;
  int unsupported;
  if (prot & PROT_EXEC) {
  }
  if (size > NUMERIC_MAX(size_t)) return eoverflow();
  if (!IsValidAddrSize(addr, size)) return einval();
  if ((unsupported = prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC))) {
    LOGF("unsupported mprotect() protection: %#x", unsupported);
    return einval();
  }
  BEGIN_NO_PAGE_FAULTS;
  LOCK(&m->system->mmap_lock);
  rc = ProtectVirtual(m->system, addr, size, prot, false);
  unassert(CheckMemoryInvariants(m->system));
  UNLOCK(&m->system->mmap_lock);
  END_NO_PAGE_FAULTS;
  return rc;
}

static int SysMadvise(struct Machine *m, i64 addr, u64 len, int advice) {
  return 0;
}

static i64 SysShmget(struct Machine *m, i64 key, u64 size, i32 shmflg) {
  return enosys(); /* SHM disabled: guest can't use host addresses */
}

static i64 SysShmat(struct Machine *m, i32 shmid, i64 shmaddr, i32 shmflg) {
  void *addr;
  void *hint = shmaddr ? (void *)(uintptr_t)shmaddr : NULL;
  addr = shmat(shmid, hint, shmflg);
  if (addr == (void *)-1) return -1;
  return (i64)(uintptr_t)addr;
}

static i64 SysShmdt(struct Machine *m, i64 shmaddr) {
  return shmdt((void *)(uintptr_t)shmaddr);
}

static i64 SysShmctl(struct Machine *m, i32 shmid, i32 cmd, i64 bufaddr) {
  // Only support IPC_RMID (0) and IPC_STAT (2) commands
  if (cmd == IPC_RMID) {
    return shmctl(shmid, IPC_RMID, NULL);
  }
  if (cmd == IPC_STAT || cmd == IPC_SET) {
    struct shmid_ds buf;
    if (cmd == IPC_STAT) {
      if (shmctl(shmid, IPC_STAT, &buf) == -1) return -1;
      if (bufaddr) {
        CopyToUserWrite(m, bufaddr, &buf, sizeof(buf));
      }
      return 0;
    }
    // IPC_SET
    if (bufaddr) {
      CopyFromUserRead(m, &buf, bufaddr, sizeof(buf));
    }
    return shmctl(shmid, IPC_SET, &buf);
  }
  return einval();
}

static i64 SysBrk(struct Machine *m, i64 addr) {
  i64 rc, size;
  long pagesize;
  BEGIN_NO_PAGE_FAULTS;
  LOCK(&m->system->mmap_lock);
  MEM_LOGF("brk(%#" PRIx64 ") currently %#" PRIx64, addr, m->system->brk);
  pagesize = FLAG_pagesize;
  addr = ROUNDUP(addr, pagesize);
  if (addr >= kNullSize) {
    if (addr > m->system->brk) {
      size = addr - m->system->brk;
      CleanseMemory(m->system, size);
      if (m->system->rss < GetMaxRss(m->system)) {
        if (size / 4096 + m->system->vss < GetMaxVss(m->system)) {
          if (ReserveVirtual(m->system, m->system->brk, addr - m->system->brk,
                             PAGE_FILE | PAGE_RW | PAGE_U | PAGE_XD, -1, 0, 0,
                             0) != -1) {
            if (!m->system->brkchanged) {
              unassert(AddFileMap(m->system, m->system->brk,
                                  addr - m->system->brk, "[heap]", -1));
              m->system->brkchanged = true;
            }
            MEM_LOGF("increased break %" PRIx64 " -> %" PRIx64, m->system->brk,
                     addr);
            m->system->brk = addr;
          }
        } else {
          LOGF("not enough virtual memory (%lx / %#lx pages) to map size "
               "%#" PRIx64,
               m->system->vss, GetMaxVss(m->system), size);
        }
      } else {
        LOGF("ran out of resident memory (%#lx / %#lx pages)", m->system->rss,
             GetMaxRss(m->system));
      }
    } else if (addr < m->system->brk) {
      if (FreeVirtual(m->system, addr, m->system->brk - addr) != -1) {
        m->system->brk = addr;
      }
    }
  }
  rc = m->system->brk;
  unassert(CheckMemoryInvariants(m->system));
  UNLOCK(&m->system->mmap_lock);
  END_NO_PAGE_FAULTS;
  return rc;
}

static int SysMunmap(struct Machine *m, i64 virt, u64 size) {
  int rc;
  BEGIN_NO_PAGE_FAULTS;
  LOCK(&m->system->mmap_lock);
  rc = FreeVirtual(m->system, virt, size);
  unassert(CheckMemoryInvariants(m->system));
  UNLOCK(&m->system->mmap_lock);
  END_NO_PAGE_FAULTS;
  return rc;
}

int GetOflags(struct Machine *m, int fildes) {
  int oflags;
  struct Fd *fd;
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    oflags = fd->oflags;
  } else {
    oflags = -1;
  }
  UNLOCK(&m->system->fds.lock);
  return oflags;
}

static i64 SysMmapImpl(struct Machine *m, i64 virt, i64 size, int prot,
                       int flags, int fildes, i64 offset) {
  u64 key;
  int oflags;
  bool fixedmap;
  i64 newautomap;
  if (!IsValidAddrSize(virt, size)) return einval();
  if (flags & MAP_GROWSDOWN_LINUX) return enotsup();
  if ((key = Prot2Page(prot)) == (u64)-1) return einval();
  CleanseMemory(m->system, size);
  if (m->system->rss >= GetMaxRss(m->system)) {
    LOGF("ran out of resident memory (%lx / %lx pages)", m->system->rss,
         GetMaxRss(m->system));
    return enomem();
  }
  if (size / 4096 + m->system->vss > GetMaxVss(m->system)) {
    LOGF("not enough virtual memory (%lx / %lx pages) to map size %" PRIx64,
         m->system->vss, GetMaxVss(m->system), size);
    return enomem();
  }
  if (flags & MAP_ANONYMOUS_LINUX) {
    fildes = -1;
    if ((flags & MAP_TYPE_LINUX) == MAP_FILE_LINUX) {
      return einval();
    }
  } else if (offset < 0) {
    return einval();
  } else if (offset > NUMERIC_MAX(off_t)) {
    return eoverflow();
  }
  if (fildes != -1) {
    if ((oflags = GetOflags(m, fildes)) == -1) return -1;
    if (!(m->system->isfreebsd && (oflags & O_PATH_LINUX)) &&
        ((oflags & O_ACCMODE) == O_WRONLY ||   //
         ((prot & PROT_WRITE) &&               //
          (oflags & O_APPEND)) ||              //
         ((prot & PROT_WRITE) &&               //
          (flags & MAP_SHARED_LINUX) &&        //
          (oflags & O_ACCMODE) != O_RDWR))) {  //
      errno = EACCES;
      return -1;
    }
  }
  newautomap = -1;
  fixedmap = false;
  if (flags & MAP_FIXED_LINUX) {
    fixedmap = true;
    goto CreateTheMap;
  }
  if (flags & MAP_FIXED_NOREPLACE_LINUX) {
    if (IsFullyUnmapped(m->system, virt, size)) {
      goto CreateTheMap;
    } else {
      MEM_LOGF("memory already exists on the interval"
               " [%" PRIx64 ",%" PRIx64 ")\n"
               "%s",
               virt, virt + size, FormatPml4t(g_machine));
      errno = EEXIST;
      virt = -1;
      goto Finished;
    }
  }
  if (HasLinearMapping() && FLAG_vabits <= 47 && !kSkew && !virt) {
    goto CreateTheMap;
  }
  if ((!virt || !IsFullyUnmapped(m->system, virt, size))) {
    if ((virt = FindVirtual(m->system, m->system->automap, size)) == -1) {
      goto Finished;
    }
    newautomap = ROUNDUP(virt + size, FLAG_pagesize);
    if (newautomap >= FLAG_automapend) {
      newautomap = FLAG_automapstart;
    }
  }
CreateTheMap:
  virt = ReserveVirtual(m->system, virt, size, key, fildes, offset,
                        !!(flags & MAP_SHARED_LINUX), fixedmap);
  if (virt != -1 && newautomap != -1) {
    m->system->automap = newautomap;
  }
Finished:
  return virt;
}

static int XlatFreeBSDMmapFlags(int flags) {
  // FreeBSD mmap flags differ from Linux
  int out = flags & (MAP_SHARED_LINUX | MAP_PRIVATE_LINUX | MAP_FIXED_LINUX |
                     MAP_FIXED_NOREPLACE_LINUX);
  // FreeBSD MAP_STACK (0x400) implies MAP_ANON|MAP_PRIVATE in the kernel
  if (flags & 0x0400) {
    out |= MAP_ANONYMOUS_LINUX | MAP_PRIVATE_LINUX | MAP_STACK_LINUX;
    out &= ~MAP_SHARED_LINUX;
  }
  // FreeBSD MAP_ANON (0x1000) → Linux MAP_ANONYMOUS (0x0020)
  if (flags & 0x1000) {
    out |= MAP_ANONYMOUS_LINUX;
  }
  // FreeBSD defaults to MAP_PRIVATE when neither SHARED nor PRIVATE given
  if (!(out & (MAP_SHARED_LINUX | MAP_PRIVATE_LINUX))) {
    out |= MAP_PRIVATE_LINUX;
  }
  // FreeBSD MAP_PREFAULT_READ (0x40000) → Linux MAP_POPULATE (0x8000)
  if (flags & 0x40000) {
    out |= MAP_POPULATE_LINUX;
  }
  // FreeBSD MAP_NOSYNC (0x800), MAP_NOCORE (0x20000) → stripped
  return out;
}

static i64 SysMmap(struct Machine *m, i64 virt, u64 size, int prot, int flags,
                   int fildes, i64 offset) {
  i64 res;
  if (m->system->isfreebsd) {
    int bsd_flags = flags;
    flags = XlatFreeBSDMmapFlags(flags);
    // FreeBSD libthr creates RWX stacks for compat; strip PROT_EXEC to
    // avoid pathological SMC (self-modifying code) detection thrashing
    if ((bsd_flags & 0x0400) && (prot & PROT_EXEC)) {
      prot &= ~PROT_EXEC;
    }
    (void)bsd_flags;
  }
  BEGIN_NO_PAGE_FAULTS;
  LOCK(&m->system->mmap_lock);
  res = SysMmapImpl(m, virt, size, prot, flags, fildes, offset);
  unassert(CheckMemoryInvariants(m->system));
  UNLOCK(&m->system->mmap_lock);
  END_NO_PAGE_FAULTS;
  return res;
}

static i64 SysMremap(struct Machine *m, i64 old_address, u64 old_size,
                     u64 new_size, int flags, i64 new_address) {
  // would be nice to have
  // avoid being noisy in the logs
  // hope program has fallback for failure
  LOG_ONCE(MEM_LOGF("mremap() not supported yet"));
  return enomem();
}

static int XlatMsyncFlags(int flags) {
  int sysflags;
  if (flags & ~(MS_ASYNC_LINUX | MS_SYNC_LINUX | MS_INVALIDATE_LINUX)) {
    LOGF("unsupported msync() flags %#x", flags);
    return einval();
  }
  // According to POSIX, either MS_SYNC or MS_ASYNC must be specified
  // in flags, and indeed failure to include one of these flags will
  // cause msync() to fail on some systems. However, Linux permits a
  // call to msync() that specifies neither of these flags, with
  // semantics that are (currently) equivalent to specifying MS_ASYNC.
  // ──Quoth msync(2) of Linux Programmer's Manual
  if (flags & MS_ASYNC_LINUX) {
    sysflags = MS_ASYNC;
  } else if (flags & MS_SYNC_LINUX) {
    sysflags = MS_SYNC;
  } else {
    sysflags = MS_ASYNC;
  }
  if (flags & MS_INVALIDATE_LINUX) {
    sysflags |= MS_INVALIDATE;
  }
#ifdef __FreeBSD__
  // FreeBSD's manual says "The flags argument was both MS_ASYNC and
  // MS_INVALIDATE. Only one of these flags is allowed." which makes
  // following the POSIX recommendation somewhat difficult.
  if (sysflags == (MS_ASYNC | MS_INVALIDATE)) {
    sysflags = MS_INVALIDATE;
  }
#endif
  return sysflags;
}

static int SysMsync(struct Machine *m, i64 virt, u64 size, int flags) {
  if (size > NUMERIC_MAX(size_t)) return eoverflow();
  if ((flags = XlatMsyncFlags(flags)) == -1) return -1;
  return SyncVirtual(m->system, virt, size, flags);
}

static int SysDup1(struct Machine *m, i32 fildes) {
  int lim;
  int oflags;
  int newfildes;
  struct Fd *fd;
  if (fildes < 0) return ebadf();
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  if ((newfildes = VfsDup(fildes)) != -1) {
    if (newfildes >= lim) {
      VfsClose(newfildes);
      return emfile();
    }
    LOCK(&m->system->fds.lock);
    unassert(fd = GetFd(&m->system->fds, fildes));
    oflags = fd->oflags & ~O_CLOEXEC;
    unassert(ForkFd(&m->system->fds, fd, newfildes, oflags));
    UNLOCK(&m->system->fds.lock);
  }
  return newfildes;
}

static int Dup2(struct Machine *m, int fildes, int newfildes) {
  int rc;
  // POSIX.1-2007 lists dup2() as raising EINTR which seems impossible
  // so it'd be wonderful to learn what kernel(s) actually return this
  // noting Linux reproduces that in both its dup(2) and dup(3) manual
  RESTARTABLE(rc = VfsDup2(fildes, newfildes));
  return rc;
}

#ifdef HAVE_DUP3
static int Dup3(struct Machine *m, int fildes, int newfildes, int flags) {
  int rc;
  // The Linux Programmer's Manual also lists this as interruptible.
  RESTARTABLE(rc = VfsDup3(fildes, newfildes, flags));
  return rc;
}
#endif

static int SysDup2(struct Machine *m, i32 fildes, i32 newfildes) {
  int rc, oflags;
  struct Fd *fd;
  if (newfildes < 0) {
    LOGF("dup2() ebadf");
    return ebadf();
  }
  if (fildes == newfildes) {
    // no-op system call, but still must validate
    LOCK(&m->system->fds.lock);
    if (GetFd(&m->system->fds, fildes)) {
      rc = fildes;
    } else {
      rc = -1;
    }
    UNLOCK(&m->system->fds.lock);
  } else if (newfildes >= GetFileDescriptorLimit(m->system)) {
    return ebadf();
  } else if ((rc = Dup2(m, fildes, newfildes)) != -1) {
    LOCK(&m->system->fds.lock);
    if ((fd = GetFd(&m->system->fds, newfildes))) {
      dll_remove(&m->system->fds.list, &fd->elem);
      FreeFd(fd);
    }
    unassert(fd = GetFd(&m->system->fds, fildes));
    oflags = fd->oflags & ~O_CLOEXEC;
    unassert(ForkFd(&m->system->fds, fd, newfildes, oflags));
    UNLOCK(&m->system->fds.lock);
  }
  return rc;
}

static int SysDup3(struct Machine *m, i32 fildes, i32 newfildes, i32 flags) {
  int rc;
  int oflags;
  struct Fd *fd;
  if (newfildes < 0) return ebadf();
  if (fildes == newfildes) return einval();
  if (flags & ~O_CLOEXEC_LINUX) return einval();
  if (newfildes >= GetFileDescriptorLimit(m->system)) return ebadf();
#ifdef HAVE_DUP3
  if ((rc = Dup3(m, fildes, newfildes, XlatOpenFlags(flags))) != -1) {
#else
  if ((rc = Dup2(m, fildes, newfildes)) != -1) {
    if (flags & O_CLOEXEC_LINUX) {
      unassert(!VfsFcntl(newfildes, F_SETFD, FD_CLOEXEC));
    }
#endif
    LOCK(&m->system->fds.lock);
    if ((fd = GetFd(&m->system->fds, newfildes))) {
      dll_remove(&m->system->fds.list, &fd->elem);
      FreeFd(fd);
    }
    unassert(fd = GetFd(&m->system->fds, fildes));
    oflags = fd->oflags & ~O_CLOEXEC;
    if (flags & O_CLOEXEC_LINUX) {
      oflags |= O_CLOEXEC;
    }
    unassert(ForkFd(&m->system->fds, fd, newfildes, oflags));
    UNLOCK(&m->system->fds.lock);
  }
  return rc;
}

static int SysDupf(struct Machine *m, i32 fildes, i32 minfildes, int cmd) {
  struct Fd *fd;
  int lim, oflags, newfildes;
  if (minfildes >= (lim = GetFileDescriptorLimit(m->system))) return emfile();
  if ((newfildes = VfsFcntl(fildes, cmd, minfildes)) != -1) {
    if (newfildes >= lim) {
      VfsClose(newfildes);
      return emfile();
    }
    LOCK(&m->system->fds.lock);
    unassert(fd = GetFd(&m->system->fds, fildes));
    oflags = fd->oflags & ~O_CLOEXEC;
    if (cmd == F_DUPFD_CLOEXEC) {
      oflags |= O_CLOEXEC;
    }
    unassert(ForkFd(&m->system->fds, fd, newfildes, oflags));
    UNLOCK(&m->system->fds.lock);
  }
  return newfildes;
}

static void FixupSock(int fd, int flags) {
  if (flags & SOCK_CLOEXEC_LINUX) {
    unassert(!VfsFcntl(fd, F_SETFD, FD_CLOEXEC));
  }
  if (flags & SOCK_NONBLOCK_LINUX) {
    unassert(!VfsFcntl(fd, F_SETFL, O_NDELAY));
  }
}

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP __TIMESTAMP__
#endif
#ifndef BLINK_VERSION
#define BLINK_VERSION "BLINK_VERSION_UNKNOWN"
#warning "-DBLINK_VERSION=... should be passed to blink/syscall.c"
#endif
#ifndef LINUX_VERSION
#define LINUX_VERSION "LINUX_VERSION_UNKNOWN"
#warning "-DLINUX_VERSION=... should be passed to blink/syscall.c"
#endif
#ifndef BLINK_COMMITS
#define BLINK_COMMITS "BLINK_COMMITS_UNKNOWN"
#warning "-DBLINK_COMMITS=... should be passed to blink/syscall.c"
#endif
#ifndef BLINK_UNAME_V
#define BLINK_UNAME_V "BLINK_UNAME_V_UNKNOWN"
#warning "-DBLINK_UNAME_V=... should be passed to blink/syscall.c"
#endif

static int SysUname(struct Machine *m, i64 utsaddr) {
  // glibc binaries won't run unless we report blink as a
  // modern linux kernel on top of genuine intel hardware
  struct utsname_linux uts;
  union {
    char host[sizeof(uts.nodename)];
    char domain[sizeof(uts.domainname)];
  } u;
  memset(&uts, 0, sizeof(uts));
  strcpy(uts.machine, "x86_64");
  strcpy(uts.sysname, "Linux");
  strcpy(uts.release, LINUX_VERSION "-blink-" BLINK_VERSION);
  strcpy(uts.version, "#" BLINK_COMMITS " " BLINK_UNAME_V " " BUILD_TIMESTAMP);
  memset(u.host, 0, sizeof(u.host));
  gethostname(u.host, sizeof(u.host) - 1);
  strcpy(uts.nodename, u.host);
#ifdef HAVE_GETDOMAINNAME
  memset(u.domain, 0, sizeof(u.domain));
  if (getdomainname(u.domain, sizeof(u.domain) - 1) != 0 || !*u.domain)
#endif
  {
    strcpy(u.domain, "(none)");
  }
  strcpy(uts.domainname, u.domain);
  return CopyToUser(m, utsaddr, &uts, sizeof(uts));
}

#define SOCK_CLOEXEC_FREEBSD  0x10000000
#define SOCK_NONBLOCK_FREEBSD 0x20000000
#define AF_INET6_FREEBSD      28

static int SysSocket(struct Machine* m, i32 family, i32 type, i32 protocol) {
  struct Fd* fd;
  int lim, flags, fildes;
  if (m->system->isfreebsd) {
    flags = 0;
    if (type & SOCK_CLOEXEC_FREEBSD) flags |= SOCK_CLOEXEC_LINUX;
    if (type & SOCK_NONBLOCK_FREEBSD) flags |= SOCK_NONBLOCK_LINUX;
    type &= ~(SOCK_CLOEXEC_FREEBSD | SOCK_NONBLOCK_FREEBSD);
    // FreeBSD AF_INET6=28 differs from Linux AF_INET6=10
    if (family == AF_INET6_FREEBSD) family = AF_INET6_LINUX;
  } else {
    flags = type & (SOCK_NONBLOCK_LINUX | SOCK_CLOEXEC_LINUX);
    type &= ~(SOCK_NONBLOCK_LINUX | SOCK_CLOEXEC_LINUX);
  }
  if ((type = XlatSocketType(type)) == -1) return -1;
  if ((family = XlatSocketFamily(family)) == -1) return -1;
  if ((protocol = XlatSocketProtocol(protocol)) == -1) return -1;
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  if (flags) LOCK(&m->system->exec_lock);
  if ((fildes = VfsSocket(family, type, protocol)) != -1) {
    if (fildes >= lim) {
      VfsClose(fildes);
      fildes = emfile();
    } else {
      FixupSock(fildes, flags);
      LOCK(&m->system->fds.lock);
      fd = AddFd(&m->system->fds, fildes,
                 O_RDWR | (flags & SOCK_CLOEXEC_LINUX ? O_CLOEXEC : 0) |
                     (flags & SOCK_NONBLOCK_LINUX ? O_NDELAY : 0));
      fd->socktype = type;
      UNLOCK(&m->system->fds.lock);
    }
  }
  if (flags) UNLOCK(&m->system->exec_lock);
  return fildes;
}

static int SysSocketpair(struct Machine *m, i32 family, i32 type, i32 protocol,
                         i64 pipefds_addr) {
  struct Fd *fd;
  u8 fds_linux[2][4];
  int rc, lim, flags, sysflags, fds[2];
  if (m->system->isfreebsd) {
    flags = 0;
    if (type & SOCK_CLOEXEC_FREEBSD) flags |= SOCK_CLOEXEC_LINUX;
    if (type & SOCK_NONBLOCK_FREEBSD) flags |= SOCK_NONBLOCK_LINUX;
    type &= ~(SOCK_CLOEXEC_FREEBSD | SOCK_NONBLOCK_FREEBSD);
    if (family == AF_INET6_FREEBSD) family = AF_INET6_LINUX;
  } else {
    flags = type & (SOCK_NONBLOCK_LINUX | SOCK_CLOEXEC_LINUX);
    type &= ~(SOCK_NONBLOCK_LINUX | SOCK_CLOEXEC_LINUX);
  }
  if ((type = XlatSocketType(type)) == -1) return -1;
  if ((family = XlatSocketFamily(family)) == -1) return -1;
  if ((protocol = XlatSocketProtocol(protocol)) == -1) return -1;
  if (!IsValidMemory(m, pipefds_addr, sizeof(fds_linux), PROT_WRITE)) return -1;
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  if (flags) LOCK(&m->system->exec_lock);
  if ((rc = VfsSocketpair(family, type, protocol, fds)) != -1) {
    if (fds[0] >= lim || fds[1] >= lim) {
      VfsClose(fds[0]);
      VfsClose(fds[1]);
      rc = emfile();
    } else {
      FixupSock(fds[0], flags);
      FixupSock(fds[1], flags);
      LOCK(&m->system->fds.lock);
      sysflags = O_RDWR;
      if (flags & SOCK_CLOEXEC_LINUX) sysflags |= O_CLOEXEC;
      if (flags & SOCK_NONBLOCK_LINUX) sysflags |= O_NDELAY;
      unassert(fd = AddFd(&m->system->fds, fds[0], sysflags));
      fd->socktype = type;
      unassert(fd = AddFd(&m->system->fds, fds[1], sysflags));
      fd->socktype = type;
      UNLOCK(&m->system->fds.lock);
      Write32(fds_linux[0], fds[0]);
      Write32(fds_linux[1], fds[1]);
      unassert(!CopyToUserWrite(m, pipefds_addr, fds_linux, sizeof(fds_linux)));
    }
  }
  if (flags) UNLOCK(&m->system->exec_lock);
  return rc;
}

static u32 LoadAddrSize(struct Machine *m, i64 asa) {
  const u8 *p;
  if (asa && (p = (const u8 *)SchlepR(m, asa, 4))) {
    return Read32(p);
  } else {
    return 0;
  }
}

static int StoreAddrSize(struct Machine *m, i64 asa, socklen_t len) {
  u8 buf[4];
  if (!asa) return 0;
  Write32(buf, len);
  return CopyToUserWrite(m, asa, buf, sizeof(buf));
}

static int LoadSockaddr(struct Machine *m, i64 sockaddr_addr, u32 sockaddr_size,
                        struct sockaddr_storage *out_sockaddr) {
  const struct sockaddr_linux *sockaddr_linux;
  if ((sockaddr_linux = (const struct sockaddr_linux *)SchlepR(
           m, sockaddr_addr, sockaddr_size))) {
    return XlatSockaddrToHost(out_sockaddr, sockaddr_linux, sockaddr_size,
                              m->system->isfreebsd);
  } else {
    return -1;
  }
}

static int CheckSockaddr(struct Machine *m, i64 sockaddr_addr,
                         i64 sockaddr_size_addr) {
  const u8 *size;
  if (!sockaddr_size_addr) return 0;
  if (!(size = (const u8 *)SchlepRW(m, sockaddr_size_addr, 4))) return -1;
  if ((i32)Read32(size) < 0) return einval();
  return 0;
}

static int StoreSockaddr(struct Machine *m, i64 sockaddr_addr,
                         i64 sockaddr_size_addr, const struct sockaddr *sa,
                         socklen_t salen) {
  int got;
  u32 avail;
  struct sockaddr_storage_linux ss;
  if (!sockaddr_addr) return 0;
  if (!sockaddr_size_addr) return 0;
  avail = LoadAddrSize(m, sockaddr_size_addr);
  if ((got = XlatSockaddrToLinux(&ss, sa, salen, m->system->isfreebsd)) == -1)
    return -1;
  if (StoreAddrSize(m, sockaddr_size_addr, got) == -1) return -1;
  return CopyToUserWrite(m, sockaddr_addr, &ss, MIN(got, avail));
}

static int SysSocketName(struct Machine *m, i32 fildes, i64 sockaddr_addr,
                         i64 sockaddr_size_addr,
                         int SocketName(int, struct sockaddr *, socklen_t *)) {
  int rc;
  socklen_t addrlen;
  struct sockaddr_storage addr;
  addrlen = sizeof(addr);
  rc = SocketName(fildes, (struct sockaddr *)&addr, &addrlen);
  if (rc != -1) {
    if (StoreSockaddr(m, sockaddr_addr, sockaddr_size_addr,
                      (struct sockaddr *)&addr, addrlen) == -1) {
      rc = -1;
    }
  }
  return rc;
}

static int SysGetsockname(struct Machine *m, int fd, i64 aa, i64 asa) {
  return SysSocketName(m, fd, aa, asa, VfsGetsockname);
}

static int SysGetpeername(struct Machine *m, int fd, i64 aa, i64 asa) {
  return SysSocketName(m, fd, aa, asa, VfsGetpeername);
}

static int GetNoRestart(struct Machine *m, int fildes, bool *norestart) {
  struct Fd *fd;
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    *norestart = fd->norestart;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return ebadf();
  return 0;
}

static int SysAccept4(struct Machine *m, i32 fildes, i64 sockaddr_addr,
                      i64 sockaddr_size_addr, i32 flags) {
  struct Fd *fd;
  socklen_t addrlen;
  bool restartable = false;
  int lim, newfd, socktype;
  struct sockaddr_storage addr;
  if (flags & ~(SOCK_CLOEXEC_LINUX | SOCK_NONBLOCK_LINUX)) return einval();
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    socktype = fd->socktype;
    restartable = !fd->norestart;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) {
    return -1;
  }
  if (!socktype) {
    errno = ENOTSOCK;
    return -1;
  }
  if (socktype != SOCK_STREAM) {
    // POSIX.1 and Linux require EOPNOTSUPP when called on a file
    // descriptor that doesn't support accepting, i.e. SOCK_STREAM,
    // but FreeBSD incorrectly returns EINVAL.
    return eopnotsupp();
  }
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  addrlen = sizeof(addr);
  INTERRUPTIBLE(restartable,
                newfd = VfsAccept(fildes, (struct sockaddr *)&addr, &addrlen));
  if (newfd != -1) {
    if (newfd >= lim) {
      VfsClose(newfd);
      newfd = emfile();
    } else {
      FixupSock(newfd, flags);
      LOCK(&m->system->fds.lock);
      if (!(fd = GetFd(&m->system->fds, fildes)) ||
          !ForkFd(&m->system->fds, fd, newfd,
                  O_RDWR | (flags & SOCK_CLOEXEC_LINUX ? O_CLOEXEC : 0) |
                      (flags & SOCK_NONBLOCK_LINUX ? O_NDELAY : 0))) {
        VfsClose(newfd);
        newfd = -1;
      }
      UNLOCK(&m->system->fds.lock);
      if (newfd != -1) {
        StoreSockaddr(m, sockaddr_addr, sockaddr_size_addr,
                      (struct sockaddr *)&addr, addrlen);
      }
    }
  }
  return newfd;
}

static int XlatSendFlags(int flags, int socktype) {
  int supported, hostflags;
  supported = MSG_OOB_LINUX |        //
              MSG_DONTROUTE_LINUX |  //
              MSG_NOSIGNAL_LINUX |   //
#ifdef MSG_EOR
              MSG_EOR_LINUX |
#endif
              MSG_DONTWAIT_LINUX;
  if (flags & ~supported) {
    LOGF("unsupported %s flags %#x", "send", flags & ~supported);
    return einval();
  }
  hostflags = 0;
  if (flags & MSG_OOB_LINUX) {
    if (socktype != SOCK_STREAM) {
      return eopnotsupp();
    }
    hostflags |= MSG_OOB;
  }
  if (flags & MSG_DONTROUTE_LINUX) hostflags |= MSG_DONTROUTE;
  if (flags & MSG_DONTWAIT_LINUX) hostflags |= MSG_DONTWAIT;
#ifdef MSG_EOR
  if (flags & MSG_EOR_LINUX) hostflags |= MSG_EOR;
#endif
  return hostflags;
}

// FreeBSD MSG send flags differ from Linux MSG flags for many values.
// FreeBSD: OOB=0x1 DONTROUTE=0x4 EOR=0x8 DONTWAIT=0x80 EOF=0x100
//          NOSIGNAL=0x20000
// Linux:   OOB=0x1 DONTROUTE=0x4 EOR=0x80 DONTWAIT=0x40
//          NOSIGNAL=0x4000
static int XlatFreeBSDSendFlags(int flags) {
  int out = 0;
  if (flags & 0x001) out |= MSG_OOB_LINUX;
  if (flags & 0x004) out |= MSG_DONTROUTE_LINUX;
  if (flags & 0x008) out |= MSG_EOR_LINUX;
  if (flags & 0x080) out |= MSG_DONTWAIT_LINUX;
  if (flags & 0x20000) out |= MSG_NOSIGNAL_LINUX;
  return out;
}

// FreeBSD MSG recv flags differ from Linux MSG flags for many values.
// FreeBSD: OOB=0x1 PEEK=0x2 TRUNC=0x10 WAITALL=0x40 DONTWAIT=0x80
//          CMSG_CLOEXEC=0x40000
// Linux:   OOB=0x1 PEEK=0x2 TRUNC=0x20 WAITALL=0x100 DONTWAIT=0x40
//          CMSG_CLOEXEC=0x40000000
static int XlatFreeBSDRecvFlags(int flags) {
  int out = 0;
  if (flags & 0x001) out |= MSG_OOB_LINUX;
  if (flags & 0x002) out |= MSG_PEEK_LINUX;
  if (flags & 0x010) out |= MSG_TRUNC_LINUX;
  if (flags & 0x040) out |= MSG_WAITALL_LINUX;
  if (flags & 0x080) out |= MSG_DONTWAIT_LINUX;
  if (flags & 0x40000) out |= MSG_CMSG_CLOEXEC_LINUX;
  return out;
}

static int XlatRecvFlags(int flags) {
  int supported, hostflags;
  supported = MSG_OOB_LINUX |    //
              MSG_PEEK_LINUX |   //
              MSG_TRUNC_LINUX |  //
#ifndef DISABLE_NONPOSIX
              MSG_DONTWAIT_LINUX |      //
              MSG_CMSG_CLOEXEC_LINUX |  //
#endif
              MSG_WAITALL_LINUX;
  if (flags & ~supported) {
    LOGF("unsupported %s flags %#x", "recv", flags & ~supported);
    return einval();
  }
  hostflags = 0;
  if (flags & MSG_OOB_LINUX) hostflags |= MSG_OOB;
  if (flags & MSG_PEEK_LINUX) hostflags |= MSG_PEEK;
  if (flags & MSG_TRUNC_LINUX) hostflags |= MSG_TRUNC;
  if (flags & MSG_WAITALL_LINUX) hostflags |= MSG_WAITALL;
  if (flags & MSG_DONTWAIT_LINUX) hostflags |= MSG_DONTWAIT;
#ifdef MSG_CMSG_CLOEXEC
  if (flags & MSG_CMSG_CLOEXEC_LINUX) hostflags |= MSG_CMSG_CLOEXEC;
#endif
  return hostflags;
}

static int UnXlatMsgFlags(int flags) {
  int guestflags = 0;
  if (flags & MSG_OOB) {
    guestflags |= MSG_OOB_LINUX;
    flags &= ~MSG_OOB;
  }
  if (flags & MSG_PEEK) {
    guestflags |= MSG_PEEK_LINUX;
    flags &= ~MSG_PEEK;
  }
  if (flags & MSG_TRUNC) {
    guestflags |= MSG_TRUNC_LINUX;
    flags &= ~MSG_TRUNC;
  }
  if (flags & MSG_CTRUNC) {
    guestflags |= MSG_CTRUNC_LINUX;
    flags &= ~MSG_CTRUNC;
  }
  if (flags & MSG_WAITALL) {
    guestflags |= MSG_WAITALL_LINUX;
    flags &= ~MSG_WAITALL;
  }
  if (flags & MSG_DONTROUTE) {
    guestflags |= MSG_DONTROUTE_LINUX;
    flags &= ~MSG_DONTROUTE;
  }
#ifdef MSG_EOR
  if (flags & MSG_EOR) {
    guestflags |= MSG_EOR_LINUX;
    flags &= ~MSG_EOR;
  }
#endif
#ifdef MSG_CMSG_CLOEXEC
#ifndef DISABLE_NONPOSIX
  if (flags & MSG_CMSG_CLOEXEC) {
    guestflags |= MSG_CMSG_CLOEXEC_LINUX;
    flags &= ~MSG_CMSG_CLOEXEC;
  }
#endif
#endif
  if (flags) {
    LOGF("unsupported %s flags %#x", "msg", flags);
  }
  return guestflags;
}

// we need to handle any shutdown via pipeline explicitly
// because blink always puts SIGPIPE in the SIG_IGN state
static i64 HandleSigpipe(struct Machine *m, i64 rc, int flags) {
#ifndef __linux
  // TODO(jart): Should we just intercept shutdown() state instead?
  if (rc == -1 && errno == ENOTCONN) {
    errno = EPIPE;
  }
#endif
  if (rc == -1 && errno == EPIPE &&
      !((flags & MSG_NOSIGNAL_LINUX) ||
        (m->sigmask & ((u64)1 << (SIGPIPE_LINUX - 1))))) {
    LOCK(&m->system->sig_lock);
    switch (Read64(m->system->hands[SIGPIPE_LINUX - 1].handler)) {
      case SIG_IGN_LINUX:
        break;
      case SIG_DFL_LINUX:
        TerminateSignal(m, SIGPIPE_LINUX, 0);
        errno = EPIPE;
        break;
      default:
        m->interrupted = true;
        Put64(m->ax, -EPIPE_LINUX);
        DeliverSignal(m, SIGPIPE_LINUX, SI_KERNEL_LINUX);
        break;
    }
    UNLOCK(&m->system->sig_lock);
  }
  return rc;
}

static bool IsSockAddrEmpty(const struct sockaddr *sa) {
  return (sa->sa_family == AF_INET &&
          !((const struct sockaddr_in *)sa)->sin_addr.s_addr) ||
         (sa->sa_family == AF_INET6 &&
          !Read64(((u8 *)&((struct sockaddr_in6 *)sa)->sin6_addr) + 0) &&
          !Read64(((u8 *)&((struct sockaddr_in6 *)sa)->sin6_addr) + 8));
}

// Operating systems like Linux let you sendmsg(buf, dest=0.0.0.0) where
// the empty destination address implies the source address. Other OSes,
// e.g. OpenBSD, do not implement this behavior. See also this Stack
// Exchange question: https://unix.stackexchange.com/a/419881/451352
static void EnsureSockAddrHasDestination(struct Machine *m, int fildes,
                                         struct sockaddr_storage *ss) {
#ifndef HAVE_SENDTO_ZERO
  struct Fd *fd;
  if (!IsSockAddrEmpty((const struct sockaddr *)ss)) return;
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    if (ss->ss_family == fd->saddr.sa.sa_family) {
      if (ss->ss_family == AF_INET) {
        memcpy(&((struct sockaddr_in *)ss)->sin_addr, &fd->saddr.sin.sin_addr,
               sizeof(fd->saddr.sin.sin_addr));
      } else if (ss->ss_family == AF_INET6) {
        memcpy(&((struct sockaddr_in6 *)ss)->sin6_addr,
               &fd->saddr.sin6.sin6_addr, sizeof(fd->saddr.sin6.sin6_addr));
      } else {
        unassert(!"inconsistent code");
        __builtin_unreachable();
      }
    } else if (ss->ss_family == AF_INET) {
      // if we do socket() followed by connect(0.0.0.0) assume 127.0.0.1
      ((struct sockaddr_in *)ss)->sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    }
  }
  UNLOCK(&m->system->fds.lock);
#endif
}

static i64 SysSendto(struct Machine *m,  //
                     i32 fildes,         //
                     i64 bufaddr,        //
                     u64 buflen,         //
                     i32 flags,          //
                     i64 sockaddr_addr,  //
                     i32 sockaddr_size) {
  ssize_t rc;
  int socktype;
  struct Fd *fd;
  struct Iovs iv;
  bool norestart;
  struct msghdr msg;
  int len, hostflags;
  struct sockaddr_storage ss;
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    socktype = fd->socktype;
    norestart = fd->norestart;
  } else {
    socktype = 0;
    norestart = false;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return ebadf();
  if (sockaddr_size < 0) return einval();
  if (m->system->isfreebsd) flags = XlatFreeBSDSendFlags(flags);
  if ((hostflags = XlatSendFlags(flags, socktype)) == -1) return -1;
  memset(&msg, 0, sizeof(msg));
  // "If sendto() is used on a connection-mode socket, the arguments
  // dest_addr and addrlen are ignored." ──Quoth the Linux Programmer's
  // Manual § send(2). However FreeBSD behaves differently.
  if (socktype != SOCK_STREAM && sockaddr_size) {
    if ((len = LoadSockaddr(m, sockaddr_addr, sockaddr_size, &ss)) != -1) {
      msg.msg_namelen = len;
      EnsureSockAddrHasDestination(m, fildes, &ss);
    } else {
      return -1;
    }
    msg.msg_name = &ss;
  }
  InitIovs(&iv);
  if ((rc = AppendIovsReal(m, &iv, bufaddr, buflen, PROT_READ)) != -1) {
    msg.msg_iov = iv.p;
    msg.msg_iovlen = iv.i;
    INTERRUPTIBLE(!norestart, rc = VfsSendmsg(fildes, &msg, hostflags));
  }
  FreeIovs(&iv);
  return HandleSigpipe(m, rc, flags);
}

static i64 SysRecvfrom(struct Machine *m,  //
                       i32 fildes,         //
                       i64 bufaddr,        //
                       u64 buflen,         //
                       i32 flags,          //
                       i64 sockaddr_addr,  //
                       i64 sockaddr_size_addr) {
  ssize_t rc;
  int hostflags;
  struct Iovs iv;
  struct msghdr msg;
  bool norestart = false;
  struct sockaddr_storage addr;
  if (m->system->isfreebsd) flags = XlatFreeBSDRecvFlags(flags);
  if ((hostflags = XlatRecvFlags(flags)) == -1) return -1;
  if (GetNoRestart(m, fildes, &norestart) == -1) return -1;
  if (CheckSockaddr(m, sockaddr_addr, sockaddr_size_addr) == -1) return -1;
  memset(&msg, 0, sizeof(msg));
  if (sockaddr_addr && sockaddr_size_addr) {
    msg.msg_name = &addr;
    msg.msg_namelen = sizeof(addr);
  }
  InitIovs(&iv);
  if ((rc = AppendIovsReal(m, &iv, bufaddr, buflen, PROT_WRITE)) != -1) {
    msg.msg_iov = iv.p;
    msg.msg_iovlen = iv.i;
    INTERRUPTIBLE(!norestart, rc = VfsRecvmsg(fildes, &msg, hostflags));
    if (rc != -1) {
      StoreSockaddr(m, sockaddr_addr, sockaddr_size_addr,
                    (struct sockaddr *)msg.msg_name, msg.msg_namelen);
    }
  }
  FreeIovs(&iv);
  return rc;
}

static void ConvertFreeBSDMsghdr(struct msghdr_linux *dst,
                                 const struct msghdr_freebsd *src) {
  memset(dst, 0, sizeof(*dst));
  memcpy(dst->name, src->name, 8);
  memcpy(dst->namelen, src->namelen, 4);
  memcpy(dst->iov, src->iov, 8);
  Write64(dst->iovlen, Read32(src->iovlen));
  memcpy(dst->control, src->control, 8);
  Write64(dst->controllen, Read32(src->controllen));
  memcpy(dst->flags, src->flags, 4);
}

static void WriteBackFreeBSDMsghdr(struct Machine *m, i64 msgaddr,
                                   const struct msghdr_linux *src) {
  struct msghdr_freebsd fb;
  memcpy(fb.name, src->name, 8);
  memcpy(fb.namelen, src->namelen, 4);
  memset(fb.pad1_, 0, 4);
  memcpy(fb.iov, src->iov, 8);
  Write32(fb.iovlen, (u32)Read64(src->iovlen));
  memset(fb.pad2_, 0, 4);
  memcpy(fb.control, src->control, 8);
  Write32(fb.controllen, (u32)Read64(src->controllen));
  memcpy(fb.flags, src->flags, 4);
  CopyToUserWrite(m, msgaddr, &fb, sizeof(fb));
}

static i64 SysSendmsg(struct Machine *m, i32 fildes, i64 msgaddr, i32 flags) {
  i32 len;
  u64 iovlen;
  ssize_t rc;
  i64 iovaddr;
  int socktype;
  struct Fd *fd;
  struct Iovs iv;
  bool norestart;
  struct msghdr msg;
  struct sockaddr_storage ss;
  struct msghdr_linux gm_storage;
  const struct msghdr_linux *gm;
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    socktype = fd->socktype;
    norestart = fd->norestart;
  } else {
    socktype = 0;
    norestart = false;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return ebadf();
  if (m->system->isfreebsd) flags = XlatFreeBSDSendFlags(flags);
  if ((flags = XlatSendFlags(flags, socktype)) == -1) return -1;
  if (m->system->isfreebsd) {
    const struct msghdr_freebsd *fb;
    if (!(fb = (const struct msghdr_freebsd *)SchlepR(
              m, msgaddr, sizeof(*fb)))) {
      return -1;
    }
    ConvertFreeBSDMsghdr(&gm_storage, fb);
    gm = &gm_storage;
  } else {
    if (!(gm = (const struct msghdr_linux *)SchlepR(
              m, msgaddr, sizeof(*gm)))) {
      return -1;
    }
  }
  memset(&msg, 0, sizeof(msg));
  if (socktype != SOCK_STREAM && (len = Read32(gm->namelen)) > 0) {
    if ((len = LoadSockaddr(m, Read64(gm->name), len, &ss)) == -1) {
      return -1;
    }
    EnsureSockAddrHasDestination(m, fildes, &ss);
    msg.msg_name = &ss;
    msg.msg_namelen = len;
  }
#ifndef DISABLE_ANCILLARY
  if (SendAncillary(m, &msg, gm) == -1) {
    return -1;
  }
#else
  if (Read64(gm->controllen)) {
    LOGF("ancillary support disabled");
    return einval();
  }
#endif
  iovaddr = Read64(gm->iov);
  iovlen = Read64(gm->iovlen);
  if (!iovlen || iovlen > IOV_MAX_LINUX) {
    errno = EMSGSIZE;
    return -1;
  }
  InitIovs(&iv);
  if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen, PROT_READ)) != -1) {
    msg.msg_iov = iv.p;
    msg.msg_iovlen = iv.i;
    INTERRUPTIBLE(!norestart, rc = VfsSendmsg(fildes, &msg, flags));
  }
  FreeIovs(&iv);
  return HandleSigpipe(m, rc, flags);
}

static i64 SysRecvmsg(struct Machine *m, i32 fildes, i64 msgaddr, i32 flags) {
  ssize_t rc;
  u64 iovlen;
  i64 iovaddr;
  struct Iovs iv;
  struct msghdr msg;
  bool norestart = false;
  struct msghdr_linux gm;
  struct sockaddr_storage addr;
  if (m->system->isfreebsd) flags = XlatFreeBSDRecvFlags(flags);
  if ((flags = XlatRecvFlags(flags)) == -1) return -1;
  if (GetNoRestart(m, fildes, &norestart) == -1) return -1;
  if (m->system->isfreebsd) {
    struct msghdr_freebsd fb;
    if (CopyFromUserRead(m, &fb, msgaddr, sizeof(fb)) == -1) return -1;
    ConvertFreeBSDMsghdr(&gm, &fb);
  } else {
    if (CopyFromUserRead(m, &gm, msgaddr, sizeof(gm)) == -1) return -1;
  }
  memset(&msg, 0, sizeof(msg));
  iovaddr = Read64(gm.iov);
  iovlen = Read64(gm.iovlen);
  if (!iovlen || iovlen > IOV_MAX_LINUX) {
    errno = EMSGSIZE;
    return -1;
  }
  if (Read64(gm.controllen)) {
#ifndef DISABLE_ANCILLARY
    msg.msg_control = AddToFreeList(m, calloc(1, kMaxAncillary));
    msg.msg_controllen = kMaxAncillary;
#else
    LOGF("ancillary support disabled");
    return einval();
#endif
  }
  InitIovs(&iv);
  if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen, PROT_WRITE)) != -1) {
    msg.msg_iov = iv.p;
    msg.msg_iovlen = iv.i;
    if (Read64(gm.name)) {
      memset(&addr, 0, sizeof(addr));
      msg.msg_name = &addr;
      msg.msg_namelen = sizeof(addr);
    }
    INTERRUPTIBLE(!norestart, rc = VfsRecvmsg(fildes, &msg, flags));
    if (rc != -1) {
      Write32(gm.flags, UnXlatMsgFlags(msg.msg_flags));
#ifndef DISABLE_ANCILLARY
      if (ReceiveAncillary(m, &gm, &msg, flags) == -1) {
        return -1;
      }
#endif
      // Write back the full msghdr to guest in the correct layout.
      if (m->system->isfreebsd) {
        WriteBackFreeBSDMsghdr(m, msgaddr, &gm);
      } else {
        unassert(CopyToUserWrite(m, msgaddr, &gm, sizeof(gm)) != -1);
        unassert(CopyToUserWrite(
                     m, msgaddr + offsetof(struct msghdr_linux, controllen),
                     gm.controllen, 8) != -1);
      }
      if (Read64(gm.name)) {
        i64 namelen_off = m->system->isfreebsd
            ? offsetof(struct msghdr_freebsd, namelen)
            : offsetof(struct msghdr_linux, namelen);
        StoreSockaddr(m, Read64(gm.name),
                      msgaddr + namelen_off,
                      (struct sockaddr *)&addr, msg.msg_namelen);
      }
    }
  }
  FreeIovs(&iv);
  return rc;
}

static i64 SysSendmmsg(struct Machine *m, i32 fildes, i64 msgsaddr, u32 msgcnt,
                       i32 flags) {
  u32 i;
  i64 rc;
  u8 word[4];
  const struct mmsghdr_linux *msgs;
  if (!(msgs = (const struct mmsghdr_linux *)SchlepRW(
            m, msgsaddr, msgcnt * sizeof(*msgs)))) {
    return -1;
  }
  for (i = 0; i < msgcnt; ++i) {
    if ((rc = SysSendmsg(m, fildes, msgsaddr + i * sizeof(*msgs), flags)) !=
        -1) {
      Write32(word, rc);
      unassert(CopyToUserWrite(m,
                               msgsaddr + i * sizeof(*msgs) +
                                   offsetof(struct mmsghdr_linux, len),
                               word, 4) != -1);
    } else {
      if (!i) return -1;
      if (errno == EINTR || errno == EWOULDBLOCK) break;
      LOGF("%s raised %s after doing work", "sendmmsg",
           DescribeHostErrno(errno));
      break;
    }
  }
  return i;
}

static i64 SysRecvmmsg(struct Machine *m, i32 fildes, i64 msgsaddr, u32 msgcnt,
                       i32 flags, i64 timeoutaddr) {
  u32 i;
  i64 rc;
  u8 word[4];
  i32 flags2;
  struct timespec_linux gt;
  const struct mmsghdr_linux *msgs;
  struct timespec ts, now, remain, deadline = {0};
  if (!(msgs = (const struct mmsghdr_linux *)SchlepRW(
            m, msgsaddr, msgcnt * sizeof(*msgs)))) {
    return -1;
  }
  if (timeoutaddr) {
    if (LoadTimespecR(m, timeoutaddr, &ts) == -1) return -1;
    deadline = AddTime(GetTime(), ts);
  }
  for (i = 0; i < msgcnt; ++i) {
    flags2 = flags & ~MSG_WAITFORONE_LINUX;
    if ((flags & MSG_WAITFORONE_LINUX) && i) {
      flags2 |= MSG_DONTWAIT_LINUX;
    }
    if ((rc = SysRecvmsg(m, fildes, msgsaddr + i * sizeof(*msgs), flags2)) !=
        -1) {
      Write32(word, rc);
      unassert(CopyToUserWrite(m,
                               msgsaddr + i * sizeof(*msgs) +
                                   offsetof(struct mmsghdr_linux, len),
                               word, 4) != -1);
    } else {
      if (!i) return -1;
      if (errno == EAGAIN || errno == EWOULDBLOCK) break;
      // If an error occurs after at least one message has been
      // received, the call succeeds, and returns the number of messages
      // received. The error code is expected to be returned on a
      // subsequent call to recvmmsg(). In the current implementation,
      // however, the error code can be overwritten in the meantime by
      // an unrelated network event on a socket, for example an incoming
      // ICMP packet. ──Quoth the Linux Programmer's Manual § recvmmsg()
      LOGF("%s raised %s after doing work", "recvmmsg",
           DescribeHostErrno(errno));
      break;
    }
    // The timeout argument does not work as intended. The timeout is
    // checked only after the receipt of each datagram, so that if up to
    // vlen-1 datagrams are received before the timeout expires, but
    // then no further datagrams are received, the call will block
    // forever. ──Quoth the Linux Programmer's Manual § recvmmsg()
    if (timeoutaddr && CompareTime(GetTime(), deadline) >= 0) {
      break;
    }
  }
  if (timeoutaddr) {
    now = GetTime();
    if (CompareTime(now, deadline) >= 0) {
      remain = GetZeroTime();
    } else {
      remain = SubtractTime(deadline, now);
    }
    Write64(gt.sec, remain.tv_sec);
    Write64(gt.nsec, remain.tv_nsec);
    CopyToUserWrite(m, timeoutaddr, &gt, sizeof(gt));
  }
  return i;
}

static int SysConnectBind(struct Machine *m, i32 fildes, i64 sockaddr_addr,
                          u32 sockaddr_size,
                          int impl(int, const struct sockaddr *, socklen_t)) {
  int rc, len;
  struct Fd *fd;
  socklen_t addrlen;
  bool norestart = false;
  struct sockaddr_storage addr;
  if (GetNoRestart(m, fildes, &norestart) == -1) return -1;
  if ((len = LoadSockaddr(m, sockaddr_addr, sockaddr_size, &addr)) != -1) {
    addrlen = len;
    if (impl == VfsConnect) {
      EnsureSockAddrHasDestination(m, fildes, &addr);
    }
  } else {
    return -1;
  }
  INTERRUPTIBLE(!norestart,
                (rc = impl(fildes, (const struct sockaddr *)&addr, addrlen)));
  if (rc != -1 && impl == VfsBind) {
    LOCK(&m->system->fds.lock);
    if ((fd = GetFd(&m->system->fds, fildes))) {
      memcpy(&fd->saddr, &addr, sizeof(fd->saddr));
    }
    UNLOCK(&m->system->fds.lock);
  }
  return rc;
}

static int SysBind(struct Machine *m, int fd, i64 aa, u32 as) {
  return SysConnectBind(m, fd, aa, as, VfsBind);
}

static int SysConnect(struct Machine *m, int fd, i64 aa, u32 as) {
  return SysConnectBind(m, fd, aa, as, VfsConnect);
}

static int UnXlatSocketType(int x) {
  if (x == SOCK_STREAM) return SOCK_STREAM_LINUX;
  if (x == SOCK_DGRAM) return SOCK_DGRAM_LINUX;
  LOGF("%s %d not supported yet", "socket type", x);
  return einval();
}

static int GetsockoptInt32(struct Machine *m, i32 fd, int level, int optname,
                           i64 optvaladdr, i64 optvalsizeaddr, int xlat(int)) {
  u8 *psize;
  u8 gval[4];
  int rc, val;
  socklen_t optvalsize;
  u32 optvalsize_linux;
  if (!(psize = (u8 *)SchlepRW(m, optvalsizeaddr, 4))) return -1;
  optvalsize_linux = Read32(psize);
  if (!IsValidMemory(m, optvaladdr, optvalsize_linux, PROT_WRITE)) return -1;
  optvalsize = sizeof(val);
  if ((rc = VfsGetsockopt(fd, level, optname, &val, &optvalsize)) != -1) {
    if ((val = xlat(val)) == -1) return -1;
    Write32(gval, val);
    CopyToUserWrite(m, optvaladdr, &gval, MIN(sizeof(gval), optvalsize_linux));
    Write32(psize, sizeof(gval));
  }
  return rc;
}

static int SetsockoptLinger(struct Machine *m, i32 fildes, i64 optvaladdr,
                            u32 optvalsize) {
  struct linger hl;
  const struct linger_linux *gl;
  if (optvalsize < sizeof(*gl)) return einval();
  if (!(gl =
            (const struct linger_linux *)SchlepR(m, optvaladdr, sizeof(*gl)))) {
    return -1;
  }
  hl.l_onoff = (i32)Read32(gl->onoff);
  hl.l_linger = (i32)Read32(gl->linger);
  return VfsSetsockopt(fildes, SOL_SOCKET, SO_LINGER_, &hl, sizeof(hl));
}

static int GetsockoptLinger(struct Machine *m, i32 fd, i64 optvaladdr,
                            i64 optvalsizeaddr) {
  int rc;
  u8 *psize;
  struct linger hl;
  socklen_t optvalsize;
  u32 optvalsize_linux;
  struct linger_linux gl;
  if (!(psize = (u8 *)SchlepRW(m, optvalsizeaddr, 4))) return -1;
  optvalsize_linux = Read32(psize);
  if (!IsValidMemory(m, optvaladdr, optvalsize_linux, PROT_WRITE)) return -1;
  optvalsize = sizeof(hl);
  if ((rc = VfsGetsockopt(fd, SOL_SOCKET, SO_LINGER_, &hl, &optvalsize)) !=
      -1) {
    Write32(gl.onoff, hl.l_onoff);
    Write32(gl.linger, hl.l_linger);
    CopyToUserWrite(m, optvaladdr, &gl, MIN(sizeof(gl), optvalsize_linux));
    Write32(psize, sizeof(gl));
  }
  return rc;
}

static int SysSetsockopt(struct Machine *m, i32 fildes, i32 level, i32 optname,
                         i64 optvaladdr, u32 optvalsize) {
  int rc;
  void *optval;
  struct Fd *fd;
  int syslevel, sysoptname;
  if (m->system->isfreebsd) {
    level = XlatFreeBSDSocketLevel(level);
    if (level == SOL_SOCKET_LINUX) {
      // FreeBSD SO_NOSIGPIPE (0x800) has no Linux equivalent; Linux uses
      // MSG_NOSIGNAL per-send instead, which we already translate. Silently
      // succeed so SSL libraries (which set SO_NOSIGPIPE) don't bail out.
      if (optname == 0x800u) return 0;
      optname = XlatFreeBSDSocketOptname(optname);
    }
  }
  switch (level) {
    case SOL_SOCKET_LINUX:
      switch (optname) {
        case SO_LINGER_LINUX:
          return SetsockoptLinger(m, fildes, optvaladdr, optvalsize);
        default:
          break;
      }
      break;
    default:
      break;
  }
  if (optvalsize > 256) return einval();
  if (XlatSocketLevel(level, &syslevel) == -1) return -1;
  if ((sysoptname = XlatSocketOptname(level, optname)) == -1) return -1;
  if (!(optval = SchlepR(m, optvaladdr, optvalsize))) return -1;
  rc = VfsSetsockopt(fildes, syslevel, sysoptname, optval, optvalsize);
  if (rc != -1 &&                      //
      level == SOL_SOCKET_LINUX &&     //
      optname == SO_RCVTIMEO_LINUX &&  //
      optvalsize >= 4) {
    LOCK(&m->system->fds.lock);
    if ((fd = GetFd(&m->system->fds, fildes))) {
      fd->norestart = !!Read32((u8 *)optval);
    }
    UNLOCK(&m->system->fds.lock);
  }
  return rc;
}

static int SysGetsockopt(struct Machine *m, i32 fildes, i32 level, i32 optname,
                         i64 optvaladdr, i64 optvalsizeaddr) {
  int rc;
  void *optval;
  socklen_t optvalsize;
  u8 optvalsize_linux[4];
  int syslevel, sysoptname;
  if (m->system->isfreebsd) {
    level = XlatFreeBSDSocketLevel(level);
    if (level == SOL_SOCKET_LINUX) {
      if (optname == 0x800u) {
        // SO_NOSIGPIPE: report as enabled (we translate MSG_NOSIGNAL)
        u8 *psize, gval[4];
        if (!(psize = (u8 *)SchlepRW(m, optvalsizeaddr, 4))) return -1;
        u32 gsz = Read32(psize);
        if (!IsValidMemory(m, optvaladdr, gsz, PROT_WRITE)) return -1;
        Write32(gval, 1);
        CopyToUserWrite(m, optvaladdr, gval, MIN(4u, gsz));
        Write32(psize, 4);
        return 0;
      }
      optname = XlatFreeBSDSocketOptname(optname);
    }
  }
  switch (level) {
    case SOL_SOCKET_LINUX:
      switch (optname) {
        case SO_TYPE_LINUX:
          return GetsockoptInt32(m, fildes, SOL_SOCKET, SO_TYPE, optvaladdr,
                                 optvalsizeaddr, UnXlatSocketType);
        case SO_ERROR_LINUX:
          return GetsockoptInt32(m, fildes, SOL_SOCKET, SO_ERROR, optvaladdr,
                                 optvalsizeaddr, XlatErrno);
        case SO_LINGER_LINUX:
          return GetsockoptLinger(m, fildes, optvaladdr, optvalsizeaddr);
        default:
          break;
      }
      break;
    default:
      break;
  }
  if (XlatSocketLevel(level, &syslevel) == -1) return -1;
  if ((sysoptname = XlatSocketOptname(level, optname)) == -1) return -1;
  if (CopyFromUserRead(m, optvalsize_linux, optvalsizeaddr,
                       sizeof(optvalsize_linux)) == -1) {
    return -1;
  }
  optvalsize = Read32(optvalsize_linux);
  if (optvalsize > 256) return einval();
  if (!(optval = AddToFreeList(m, calloc(1, optvalsize)))) return -1;
  rc = VfsGetsockopt(fildes, syslevel, sysoptname, optval, &optvalsize);
  Write32(optvalsize_linux, optvalsize);
  CopyToUserWrite(m, optvaladdr, optval, optvalsize);
  CopyToUserWrite(m, optvalsizeaddr, optvalsize_linux,
                  sizeof(optvalsize_linux));
  return rc;
}

// Debug aid: when env BLINK_KEYLOG=<path> is set, every read() that returns
// bytes from a tty is appended to that file as one hex line. Used to capture
// exactly what bytes a TUI (e.g. mc) receives per keypress. Off by default; the
// log fd is moved to the blink-reserved high range so it can't collide with a
// guest fd.
static _Atomic(int) g_keylog_fd = -2;  // -2=uninit, -1=disabled, else host fd
static void KeyLog(int fildes, const struct Iovs *iv, i64 rc) {
  int kf = atomic_load_explicit(&g_keylog_fd, memory_order_relaxed);
  if (kf == -2) {
    const char *p = getenv("BLINK_KEYLOG");
    int nf = -1;
    if (p && *p) {
      int t = open(p, O_WRONLY | O_CREAT | O_APPEND, 0644);
      if (t != -1) {
        nf = fcntl(t, F_DUPFD_CLOEXEC, kMinBlinkFd);
        close(t);
      }
    }
    atomic_store_explicit(&g_keylog_fd, nf, memory_order_relaxed);
    kf = nf;
  }
  if (kf < 0 || rc <= 0 || iv->i == 0) return;
  if (!isatty(fildes)) return;
  char line[160];
  int o = snprintf(line, sizeof(line), "fd%d read %ld:", fildes, (long)rc);
  const unsigned char *b = (const unsigned char *)iv->p[0].iov_base;
  i64 n = rc < (i64)iv->p[0].iov_len ? rc : (i64)iv->p[0].iov_len;
  if (n > 48) n = 48;
  for (i64 i = 0; i < n && o < (int)sizeof(line) - 4; ++i) {
    o += snprintf(line + o, sizeof(line) - o, " %02x", b[i]);
  }
  if (o < (int)sizeof(line) - 1) line[o++] = '\n';
  (void)!write(kf, line, o);
}

static i64 SysRead(struct Machine *m, i32 fildes, i64 addr, u64 size) {
  i64 rc;
  int oflags;
  struct Fd *fd;
  struct Iovs iv;
  ssize_t (*readv_impl)(int, const struct iovec *, int);
  if (size > NUMERIC_MAX(size_t)) return eoverflow();
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(readv_impl = fd->cb->readv);
    oflags = fd->oflags;
  } else {
    readv_impl = 0;
    oflags = 0;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return -1;
  if ((oflags & O_ACCMODE) == O_WRONLY) return ebadf();
  if (size) {
    InitIovs(&iv);
    if ((rc = AppendIovsReal(m, &iv, addr, size, PROT_WRITE)) != -1) {
      RESTARTABLE(rc = readv_impl(fildes, iv.p, iv.i));
      if (rc != -1) SetWriteAddr(m, addr, rc);
      KeyLog(fildes, &iv, rc);
    }
    FreeIovs(&iv);
  } else {
    rc = 0;
  }
  return rc;
}

static i64 SysWrite(struct Machine *m, i32 fildes, i64 addr, u64 size) {
  i64 rc;
  int oflags;
  struct Fd *fd;
  struct Iovs iv;
  if (m->system->isfork && (fildes == 1 || fildes == 2) && size > 0 && size < 4096) {
    u8 *p = LookupAddress(m, addr);
    if (p) fprintf(stderr, "[pid=%d fd=%d] %.*s", m->system->pid, fildes, (int)size, (char*)p);
  }
  ssize_t (*writev_impl)(int, const struct iovec *, int);
  if (size > NUMERIC_MAX(size_t)) return eoverflow();
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(writev_impl = fd->cb->writev);
    oflags = fd->oflags;
  } else {
    writev_impl = 0;
    oflags = 0;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return -1;
  if ((oflags & O_ACCMODE) == O_RDONLY) return ebadf();
  if (size) {
    InitIovs(&iv);
    if ((rc = AppendIovsReal(m, &iv, addr, size, PROT_READ)) != -1) {
      RESTARTABLE(rc = writev_impl(fildes, iv.p, iv.i));
      if (rc != -1) SetReadAddr(m, addr, rc);
    }
    FreeIovs(&iv);
  } else {
    rc = 0;
  }
  return HandleSigpipe(m, rc, 0);
}

// FreeBSD doesn't do access mode check on read/write to pipes.
// Cygwin generally doesn't do access checks or is inconsistent.
static long CheckFdAccess(struct Machine *m, i32 fildes, bool writable,
                          int errno_if_check_fails) {
  int oflags;
  struct Fd *fd;
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    oflags = fd->oflags;
  } else {
    oflags = 0;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return -1;
  if ((writable && ((oflags & O_ACCMODE) == O_RDONLY)) ||
      (!writable && ((oflags & O_ACCMODE) == O_WRONLY))) {
    errno = errno_if_check_fails;
    return -1;
  }
  return 0;
}

static i64 SysPread(struct Machine *m, i32 fildes, i64 addr, u64 size,
                    u64 offset) {
  ssize_t rc;
  struct Iovs iv;
  if (size > NUMERIC_MAX(size_t)) return eoverflow();
  if (CheckFdAccess(m, fildes, false, EBADF) == -1) return -1;
  if (size) {
    InitIovs(&iv);
    if ((rc = AppendIovsReal(m, &iv, addr, size, PROT_WRITE)) != -1) {
      RESTARTABLE(rc = VfsPreadv(fildes, iv.p, iv.i, offset));
      if (rc != -1) SetWriteAddr(m, addr, rc);
    }
    FreeIovs(&iv);
  } else {
    rc = 0;
  }
  return rc;
}

static i64 SysPwrite(struct Machine *m, i32 fildes, i64 addr, u64 size,
                     u64 offset) {
  ssize_t rc;
  struct Iovs iv;
  if (size > NUMERIC_MAX(size_t)) return eoverflow();
  if (CheckFdAccess(m, fildes, true, EBADF) == -1) return -1;
  if (size) {
    InitIovs(&iv);
    if ((rc = AppendIovsReal(m, &iv, addr, size, PROT_READ)) != -1) {
      RESTARTABLE(rc = VfsPwritev(fildes, iv.p, iv.i, offset));
      if (rc != -1) SetReadAddr(m, addr, rc);
    }
    FreeIovs(&iv);
  } else {
    rc = 0;
  }
  return rc;
}

static i64 SysPreadv2(struct Machine *m, i32 fildes, i64 iovaddr, u32 iovlen,
                      i64 offset, i32 flags) {
  i64 rc;
  int oflags;
  struct Fd *fd;
  struct Iovs iv;
  ssize_t (*readv_impl)(int, const struct iovec *, int);
  if (flags) {
    LOGF("%s flags not supported yet: %#" PRIx32, "preadv2", flags);
    return einval();
  }
  if (iovlen > IOV_MAX_LINUX) return einval();
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(readv_impl = fd->cb->readv);
    oflags = fd->oflags;
  } else {
    readv_impl = 0;
    oflags = 0;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return -1;
  if ((oflags & O_ACCMODE) == O_WRONLY) return ebadf();
  if (iovlen) {
    InitIovs(&iv);
    if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen, PROT_WRITE)) != -1) {
      if (iv.i) {
        if (offset == -1) {
          RESTARTABLE(rc = readv_impl(fildes, iv.p, iv.i));
        } else if (offset < 0) {
          return einval();
        } else if (offset > NUMERIC_MAX(off_t)) {
          return eoverflow();
        } else {
          RESTARTABLE(rc = VfsPreadv(fildes, iv.p, iv.i, offset));
        }
      } else {
        rc = 0;
      }
    }
    FreeIovs(&iv);
  } else {
    rc = 0;
  }
  return rc;
}

static i64 SysPwritev2(struct Machine *m, i32 fildes, i64 iovaddr, u32 iovlen,
                       i64 offset, i32 flags) {
  i64 rc;
  int oflags;
  struct Fd *fd;
  struct Iovs iv;
  ssize_t (*writev_impl)(int, const struct iovec *, int);
  if (flags) {
    LOGF("%s flags not supported yet: %#" PRIx32, "pwritev2", flags);
    return einval();
  }
  if (iovlen > IOV_MAX_LINUX) return einval();
  LOCK(&m->system->fds.lock);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(writev_impl = fd->cb->writev);
    oflags = fd->oflags;
  } else {
    writev_impl = 0;
    oflags = 0;
  }
  UNLOCK(&m->system->fds.lock);
  if (!fd) return -1;
  if ((oflags & O_ACCMODE) == O_RDONLY) return ebadf();
  if (iovlen) {
    InitIovs(&iv);
    if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen, PROT_READ)) != -1) {
      if (iv.i) {
        if (offset == -1) {
          RESTARTABLE(rc = writev_impl(fildes, iv.p, iv.i));
          rc = HandleSigpipe(m, rc, 0);
        } else if (offset < 0) {
          return einval();
        } else if (offset > NUMERIC_MAX(off_t)) {
          return eoverflow();
        } else {
          RESTARTABLE(rc = VfsPwritev(fildes, iv.p, iv.i, offset));
        }
      } else {
        rc = 0;
      }
    }
    FreeIovs(&iv);
  } else {
    rc = 0;
  }
  return rc;
}

static i64 SysReadv(struct Machine *m, i32 fildes, i64 iovaddr, u32 iovlen) {
  return SysPreadv2(m, fildes, iovaddr, iovlen, -1, 0);
}

static i64 SysWritev(struct Machine *m, i32 fildes, i64 iovaddr, u32 iovlen) {
  return SysPwritev2(m, fildes, iovaddr, iovlen, -1, 0);
}

static i64 SysPreadv(struct Machine *m, i32 fildes, i64 iovaddr, u32 iovlen,
                     i64 offset) {
  if (offset < 0) return einval();
  return SysPreadv2(m, fildes, iovaddr, iovlen, offset, 0);
}

static i64 SysPwritev(struct Machine *m, i32 fildes, i64 iovaddr, u32 iovlen,
                      i64 offset) {
  if (offset < 0) return einval();
  return SysPwritev2(m, fildes, iovaddr, iovlen, offset, 0);
}

static i64 SysSendfile(struct Machine *m, i32 out_fd, i32 in_fd, i64 offsetaddr,
                       u64 count) {
  u64 toto, offset;
  ssize_t got, wrote;
  u8 *buf, *offsetp = 0;
  size_t chunk, maxchunk = 16384;
  if (CheckFdAccess(m, out_fd, true, EBADF) == -1) return -1;
  if (CheckFdAccess(m, in_fd, false, EBADF) == -1) return -1;
  if (offsetaddr && !(offsetp = (u8 *)SchlepRW(m, offsetaddr, 8))) return -1;
  if (!(buf = (u8 *)AddToFreeList(m, malloc(maxchunk)))) return -1;
  if (offsetp) {
    offset = Read64(offsetp);
    if ((i64)offset < 0) return einval();
    if (Read64(offsetp) + count < count ||
        Read64(offsetp) + count > NUMERIC_MAX(off_t)) {
      return eoverflow();
    }
  }
  for (toto = 0; toto < count;) {
    chunk = MIN(count - toto, maxchunk);
    if (offsetp) {
      got = VfsPread(in_fd, buf, chunk, offset + toto);
    } else {
      got = VfsRead(in_fd, buf, chunk);
    }
    if (got == -1) goto OnFailure;
    if (offsetp) Write64(offsetp, offset + toto + got);
    if (got == 0) break;
    while (got > 0) {
      if ((wrote = VfsWrite(out_fd, buf, got)) == -1) goto OnFailure;
      toto += wrote;
      got -= wrote;
    }
  }
  return toto;
OnFailure:
  if (toto) {
    LOGF("sendfile() partial failure: %s", DescribeHostErrno(errno));
    return toto;
  } else {
    return -1;
  }
}

// Write a guest iovec array to socket s, accumulating bytes into *total.
// Returns 0 on success, -1 on error (errno set; *total reflects bytes that
// did make it out, so callers can report partial progress on EAGAIN).
static int FreeBSDSendIovecs(struct Machine *m, int s, u64 iov_addr,
                             int iovcnt, i64 *total) {
  for (int i = 0; i < iovcnt; i++) {
    u8 iovbuf[16];
    if (CopyFromUserRead(m, iovbuf, (i64)iov_addr + (i64)i * 16, 16) == -1) {
      return -1;
    }
    u64 base = Read64(iovbuf);
    u64 len = Read64(iovbuf + 8);
    u64 off = 0;
    while (off < len) {
      size_t chunk = (size_t)MIN(len - off, (u64)65536);
      const u8 *p = (const u8 *)SchlepR(m, (i64)(base + off), chunk);
      if (!p) return -1;
      ssize_t w = VfsWrite(s, p, chunk);
      if (w == -1) return -1;
      off += (u64)w;
      *total += w;
    }
  }
  return 0;
}

// FreeBSD sendfile(fd, s, offset, nbytes, hdtr, sbytes, flags):
//   fd     = source file, s = destination socket (note: opposite of Linux)
//   nbytes = file bytes to send (0 = to EOF); headers/trailers sent in addition
//   hdtr   = struct sf_hdtr {iovec *headers; int hdr_cnt; iovec *trailers;
//            int trl_cnt;} (32 bytes), or NULL
//   sbytes = out: total bytes sent (headers + file + trailers), or NULL
//   flags  = SF_* hints (ignored)
// Returns 0 on success, -1 on error. On a non-blocking socket a short write
// yields -1/EAGAIN with *sbytes set to what was sent — nginx waits for
// EVFILT_WRITE and resumes from there, so partial progress must be reported.
static int SysFreeBSDSendfile(struct Machine *m, i32 fd, i32 s, i64 offset,
                              u64 nbytes, i64 hdtr_addr, i64 sbytes_addr) {
  i64 total = 0;
  int ret = 0;
  int saved_errno = 0;
  u64 headers = 0, trailers = 0;
  i32 hdr_cnt = 0, trl_cnt = 0;
  if (CheckFdAccess(m, s, true, EBADF) == -1) return -1;
  if (CheckFdAccess(m, fd, false, EBADF) == -1) return -1;
  if (offset < 0) return einval();
  if (hdtr_addr) {
    u8 hd[32];
    if (CopyFromUserRead(m, hd, hdtr_addr, 32) == -1) return -1;
    headers = Read64(hd + 0);
    hdr_cnt = (i32)Read32(hd + 8);
    trailers = Read64(hd + 16);
    trl_cnt = (i32)Read32(hd + 24);
  }
  if (headers && hdr_cnt > 0) {
    if (FreeBSDSendIovecs(m, s, headers, hdr_cnt, &total) == -1) {
      saved_errno = errno;
      ret = -1;
      goto done;
    }
  }
  {
    u64 sent = 0;
    bool until_eof = (nbytes == 0);
    u8 *buf;
    size_t maxchunk = 65536;
    if (!(buf = (u8 *)AddToFreeList(m, malloc(maxchunk)))) return -1;
    while (until_eof || sent < nbytes) {
      size_t chunk =
          until_eof ? maxchunk : (size_t)MIN(nbytes - sent, (u64)maxchunk);
      ssize_t got = VfsPread(fd, buf, chunk, offset + (i64)sent);
      if (got == -1) {
        saved_errno = errno;
        ret = -1;
        goto done;
      }
      if (got == 0) break;  // EOF
      ssize_t off2 = 0;
      while (off2 < got) {
        ssize_t w = VfsWrite(s, buf + off2, (size_t)(got - off2));
        if (w == -1) {
          saved_errno = errno;
          ret = -1;
          goto done;
        }
        off2 += w;
        total += w;
      }
      sent += (u64)got;
    }
  }
  if (trailers && trl_cnt > 0) {
    if (FreeBSDSendIovecs(m, s, trailers, trl_cnt, &total) == -1) {
      saved_errno = errno;
      ret = -1;
      goto done;
    }
  }
done:
  if (sbytes_addr) {
    u8 sb[8];
    Write64(sb, (u64)total);
    if (CopyToUserWrite(m, sbytes_addr, sb, 8) == -1) return -1;
  }
  if (ret == -1) {
    errno = saved_errno;
    return -1;
  }
  return 0;
}

static int UnXlatDt(int x) {
#ifndef DT_UNKNOWN
  return DT_UNKNOWN_LINUX;
#else
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
#endif
}

static i64 Getdents(struct Machine *m, i32 fildes, i64 addr, i64 size,
                    struct Fd *fd) {
  i64 i;
  int type;
  off_t off;
  int reclen;
  size_t len;
  struct stat st;
  struct dirent *ent;
  struct dirent_linux rec;
  if (size < sizeof(rec) - sizeof(rec.name)) return einval();
  if (!IsValidMemory(m, addr, size, PROT_WRITE)) return -1;
  if (VfsFstat(fildes, &st) || !st.st_nlink) return enoent();
  // O_DIRECTORY is only an open-time hint; a directory may be opened O_RDONLY
  // and read (FreeBSD libc's physical getcwd() opens ".." O_RDONLY then
  // getdirentries). Gate on the fd's ACTUAL type, not the open flag.
  if (!S_ISDIR(st.st_mode)) return enotdir();
  if (!fd->dirstream && !(fd->dirstream = VfsOpendir(fd->fildes))) {
    return -1;
  }
  for (i = 0; i + sizeof(rec) <= size; i += reclen) {
    // telldir() can actually return negative on ARM/MIPS/i386
#ifdef HAVE_SEEKDIR
    long tell;
    errno = 0;
    tell = VfsTelldir(fd->dirstream);
    unassert(tell != -1 || errno == 0);
    off = tell;
#else
    off = -1;
#endif
    if (!(ent = VfsReaddir(fd->dirstream))) break;
    len = strlen(ent->d_name);
    if (len + 1 > sizeof(rec.name)) {
      LOGF("ignoring %zu byte d_name: %s", len, ent->d_name);
      reclen = 0;
      continue;
    }
#ifdef DT_UNKNOWN
    type = UnXlatDt(ent->d_type);
#else
    struct stat st;
    if (fstatat(fd->fildes, ent->d_name, &st, AT_SYMLINK_NOFOLLOW) == -1) {
      LOGF("getdents() fstatat(%d, %s) failed: %s", fd->fildes, ent->d_name,
           DescribeHostErrno(errno));
      type = DT_UNKNOWN_LINUX;
    } else {
      if (S_ISDIR(st.st_mode)) {
        type = DT_DIR_LINUX;
      } else if (S_ISCHR(st.st_mode)) {
        type = DT_CHR_LINUX;
      } else if (S_ISBLK(st.st_mode)) {
        type = DT_BLK_LINUX;
      } else if (S_ISFIFO(st.st_mode)) {
        type = DT_FIFO_LINUX;
      } else if (S_ISLNK(st.st_mode)) {
        type = DT_LNK_LINUX;
      } else if (S_ISSOCK(st.st_mode)) {
        type = DT_SOCK_LINUX;
      } else if (S_ISREG(st.st_mode)) {
        type = DT_REG_LINUX;
      } else {
        LOGF("unknown st_mode %d", st.st_mode);
        type = DT_UNKNOWN_LINUX;
      }
    }
#endif
    reclen = ROUNDUP(8 + 8 + 2 + 1 + len + 1, 8);
    memset(&rec, 0, sizeof(rec));
    Write64(rec.ino, ent->d_ino);
    Write64(rec.off, off);
    Write16(rec.reclen, reclen);
    Write8(rec.type, type);
    strcpy(rec.name, ent->d_name);
    CopyToUserWrite(m, addr + i, &rec, reclen);
  }
  return i;
}

static i64 GetFreeBSDdents(struct Machine* m, i32 fildes, i64 addr, i64 size,
                           struct Fd* fd) {
  i64 i;
  int reclen;
  size_t len;
  struct stat st;
  struct dirent* ent;
  u8 rec[512];
  if (size < 24) return einval();
  if (!IsValidMemory(m, addr, size, PROT_WRITE)) return -1;
  if (VfsFstat(fildes, &st) || !st.st_nlink) return enoent();
  // O_DIRECTORY is only an open-time hint; gate on the fd's ACTUAL type so a
  // directory opened O_RDONLY (e.g. getcwd walking "..") can be read.
  if (!S_ISDIR(st.st_mode)) return enotdir();
  if (!fd->dirstream && !(fd->dirstream = VfsOpendir(fd->fildes))) return -1;
  for (i = 0; i + 24 <= size; i += reclen) {
    long tell;
    errno = 0;
    tell = VfsTelldir(fd->dirstream);
    unassert(tell != -1 || errno == 0);
    if (!(ent = VfsReaddir(fd->dirstream))) break;
    len = strlen(ent->d_name);
    reclen = ROUNDUP(8 + 8 + 2 + 1 + 1 + 4 + len + 1, 8);
    if (i + reclen > size) {
      VfsSeekdir(fd->dirstream, tell);
      break;
    }
    memset(rec, 0, reclen);
    Write64(rec + 0, ent->d_ino);             // d_fileno
    Write64(rec + 8, tell);                   // d_off
    Write16(rec + 16, reclen);                // d_reclen
    Write8(rec + 18, UnXlatDt(ent->d_type));  // d_type
    // rec[19] = d_pad0 (already 0)
    Write16(rec + 20, len);  // d_namlen (uint16_t)
    // rec[22-23] = d_pad1 (already 0)
    strcpy((char*)rec + 24, ent->d_name);
    if (CopyToUserWrite(m, addr + i, rec, reclen) == -1) return -1;
  }
  return i;
}

static i64 SysFreeBSDGetdents(struct Machine* m, i32 fildes, i64 addr,
                              i64 size) {
  i64 rc;
  struct Fd *fd;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  rc = GetFreeBSDdents(m, fildes, addr, size, fd);
  UnlockFd(fd);
  return rc;
}

static int SysFreeBSDUname(struct Machine* m, i64 addr) {
  u8 b[5 * 256];
  memset(b, 0, sizeof(b));
  strcpy((char*)b + 0 * 256, "FreeBSD");
  strcpy((char*)b + 1 * 256, "blink");
  strcpy((char*)b + 2 * 256, "16.0-RELEASE");
  strcpy((char*)b + 3 * 256, "FreeBSD 16.0-RELEASE");
  strcpy((char*)b + 4 * 256, "amd64");
  return CopyToUserWrite(m, addr, b, sizeof(b));
}

static i64 SysGetdents(struct Machine* m, i32 fildes, i64 addr, i64 size) {
  i64 rc;
  struct Fd* fd;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  rc = Getdents(m, fildes, addr, size, fd);
  UnlockFd(fd);
  return rc;
}

static int SysSetTidAddress(struct Machine *m, i64 ctid) {
  m->ctid = ctid;
  return m->tid;
}

static int SysFadvise(struct Machine *m, u32 fd, u64 offset, u64 len,
                      i32 advice) {
#ifdef __linux__
  // POSIX_FADV_* constants 0..5 are identical on Linux and FreeBSD.
  // posix_fadvise(3) returns the errno as a positive value rather than
  // setting errno; convert to syscall convention.
  int rc = posix_fadvise((int)fd, (off_t)offset, (off_t)len, advice);
  if (rc != 0) {
    errno = rc;
    return -1;
  }
  return 0;
#else
  (void)m; (void)fd; (void)offset; (void)len; (void)advice;
  return 0;
#endif
}

static int SysFallocate(struct Machine *m, i32 fd, i32 mode, i64 offset,
                        i64 len) {
#ifdef __linux__
  int rc;
  (void)m;
  RESTARTABLE(rc = fallocate(fd, mode, (off_t)offset, (off_t)len));
  return rc;
#else
  (void)m; (void)fd; (void)mode; (void)offset; (void)len;
  return enosys();
#endif
}

static i64 SysLseek(struct Machine *m, i32 fildes, i64 offset, int whence) {
  i64 rc;
  struct Fd *fd;
  if (offset > NUMERIC_MAX(off_t)) return eoverflow();
  if (offset < -NUMERIC_MAX(off_t) - 1) return eoverflow();
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  if (!fd->dirstream) {
    rc = VfsSeek(fd->fildes, offset, XlatWhence(whence));
  } else if (whence == SEEK_SET_LINUX) {
    if (!offset) {
      VfsRewinddir(fd->dirstream);
      rc = 0;
    } else {
#ifdef HAVE_SEEKDIR
      VfsSeekdir(fd->dirstream, offset);
      rc = 0;
#else
      LOGF("host platform doesn't support seekdir()");
      rc = enotsup();
#endif
    }
  } else {
    rc = einval();
  }
  UnlockFd(fd);
  return rc;
}

static i64 SysFtruncate(struct Machine *m, i32 fildes, i64 length) {
  i64 rc;
  if (length < 0) return einval();
  if (length > NUMERIC_MAX(off_t)) return eoverflow();
  if (CheckFdAccess(m, fildes, true, EINVAL) == -1) return -1;
  RESTARTABLE(rc = VfsFtruncate(fildes, length));
  return rc;
}

// FreeBSD AT flags differ from Linux:
//   AT_EACCESS=0x100 (FreeBSD) vs AT_SYMLINK_NOFOLLOW=0x100 (Linux)
//   AT_SYMLINK_NOFOLLOW=0x200 (FreeBSD) vs AT_EACCESS=0x200 (Linux)
//   AT_REMOVEDIR=0x800 (FreeBSD) vs AT_REMOVEDIR=0x200 (Linux)
// Translate FreeBSD AT flags to Linux equivalents.
static i32 XlatFreeBSDAtFlags(i32 x) {
  i32 r = x & ~0xb00;  // mask out FreeBSD-specific bits (0x100|0x200|0x800)
  if (x & 0x200) r |= AT_SYMLINK_NOFOLLOW_LINUX;  // FreeBSD nofollow → Linux
  if (x & 0x100) r |= AT_EACCESS_LINUX;           // FreeBSD AT_EACCESS → Linux
  if (x & 0x800) r |= AT_REMOVEDIR_LINUX;         // FreeBSD AT_REMOVEDIR → Linux
  return r;
}

static int XlatFaccessatFlags(int x) {
  int res = 0;
  if (x & AT_EACCESS_LINUX) {
#if !defined(__EMSCRIPTEN__) && !defined(__CYGWIN__)
    res |= AT_EACCESS;
#endif
    x &= ~AT_EACCESS_LINUX;
  }
  if (x & AT_SYMLINK_FOLLOW_LINUX) {
    x &= ~AT_SYMLINK_FOLLOW_LINUX;  // default behavior
  }
  if (x & AT_SYMLINK_NOFOLLOW_LINUX) {
    res |= AT_SYMLINK_NOFOLLOW;
    x &= ~AT_SYMLINK_NOFOLLOW_LINUX;
  }
  if (x) {
    LOGF("%s() flags %d not supported", "faccessat", x);
    return -1;
  }
  return res;
}

static int SysFaccessat2(struct Machine *m, i32 dirfd, i64 path, i32 mode,
                         i32 flags) {
  if (m->system->isfreebsd) flags = XlatFreeBSDAtFlags(flags);
  const char *p = LoadStr(m, path);
  int xmode = XlatAccess(mode);
  int xflags = XlatFaccessatFlags(flags);
  int rc = VfsAccess(GetDirFildes(dirfd), p, xmode, xflags);
  return rc;
}

static int SysFaccessat(struct Machine *m, i32 dirfd, i64 path, i32 mode) {
  return SysFaccessat2(m, dirfd, path, mode, 0);
}

static int SysFstat(struct Machine *m, i32 fd, i64 staddr) {
  int rc;
  struct stat st;
  struct stat_linux gst;
  if ((rc = VfsFstat(fd, &st)) != -1) {
    XlatStatToLinux(&gst, &st);
    if (CopyToUserWrite(m, staddr, &gst, sizeof(gst)) == -1) rc = -1;
  }
  return rc;
}

static int XlatFstatatFlags(int x) {
  int res = 0;
  if (x & AT_SYMLINK_FOLLOW_LINUX) {
    x &= ~AT_SYMLINK_FOLLOW_LINUX;  // default behavior
  }
  if (x & AT_SYMLINK_NOFOLLOW_LINUX) {
    res |= AT_SYMLINK_NOFOLLOW;
    x &= ~AT_SYMLINK_NOFOLLOW_LINUX;
  }
#ifndef DISABLE_NONPOSIX
  if (x & AT_NO_AUTOMOUNT_LINUX) {
#if defined(AT_NO_AUTOMOUNT) && defined(DISABLE_VFS)
    res |= AT_NO_AUTOMOUNT;
#endif
    x &= ~AT_NO_AUTOMOUNT_LINUX;
  }
#endif
  if (x) {
    LOGF("%s() flags %d not supported", "fstatat", x);
    return -1;
  }
  return res;
}

static int SysFstatat(struct Machine *m, i32 dirfd, i64 pathaddr, i64 staddr,
                      i32 flags) {
  int rc;
  struct stat st;
  const char *path;
  struct stat_linux gst;
  if (m->system->isfreebsd) flags = XlatFreeBSDAtFlags(flags);
  if (!(path = LoadStr(m, pathaddr))) return -1;
#ifndef DISABLE_NONPOSIX
  if (flags & AT_EMPTY_PATH_LINUX) {
    flags &= ~AT_EMPTY_PATH_LINUX;
    if (!*path) {
      if (flags) {
        LOGF("%s() flags %d not supported", "fstatat(AT_EMPTY_PATH)", flags);
        return -1;
      }
      return SysFstat(m, dirfd, staddr);
    }
  }
#endif
  if ((rc = VfsStat(GetDirFildes(dirfd), path, &st, XlatFstatatFlags(flags))) !=
      -1) {
    XlatStatToLinux(&gst, &st);
    if (CopyToUserWrite(m, staddr, &gst, sizeof(gst)) == -1) rc = -1;
  }
  return rc;
}

static int XlatFchownatFlags(int x) {
  int res = 0;
  if (x & AT_SYMLINK_FOLLOW_LINUX) {
    x &= ~AT_SYMLINK_FOLLOW_LINUX;  // default behavior
  }
  if (x & AT_SYMLINK_NOFOLLOW_LINUX) {
    res |= AT_SYMLINK_NOFOLLOW;
    x &= ~AT_SYMLINK_NOFOLLOW_LINUX;
  }
#ifdef AT_EMPTY_PATH
#ifndef DISABLE_NONPOSIX
  if (x & AT_EMPTY_PATH_LINUX) {
    res |= AT_EMPTY_PATH;
    x &= ~AT_EMPTY_PATH_LINUX;
  }
#endif
#endif
  if (x) {
    LOGF("%s() flags %#x not supported", "fchownat", x);
    return -1;
  }
  return res;
}

static int SysFchown(struct Machine *m, i32 fildes, u32 uid, u32 gid) {
  if (m->system->emulate_root) return 0;
  return VfsFchown(fildes, uid, gid);
}

static int SysFchownat(struct Machine *m, i32 dirfd, i64 pathaddr, u32 uid,
                       u32 gid, i32 flags) {
  const char *path;
  if (m->system->emulate_root) return 0;
  if (m->system->isfreebsd) flags = XlatFreeBSDAtFlags(flags);
  if (!(path = LoadStr(m, pathaddr))) return -1;
#ifndef DISABLE_NONPOSIX
  if (flags & AT_EMPTY_PATH_LINUX) {
    flags &= AT_EMPTY_PATH_LINUX;
    if (!*path) {
      if (flags) {
        LOGF("%s() flags %d not supported", "fchownat(AT_EMPTY_PATH)", flags);
        return -1;
      }
      return SysFchown(m, dirfd, uid, gid);
    }
  }
#endif
  return VfsChown(GetDirFildes(dirfd), path, uid, gid,
                  XlatFchownatFlags(flags));
}

static int SysChown(struct Machine *m, i64 pathaddr, u32 uid, u32 gid) {
  return SysFchownat(m, AT_FDCWD_LINUX, pathaddr, uid, gid, 0);
}

static int SysLchown(struct Machine *m, i64 pathaddr, u32 uid, u32 gid) {
  return SysFchownat(m, AT_FDCWD_LINUX, pathaddr, uid, gid,
                     AT_SYMLINK_NOFOLLOW_LINUX);
}

#if !defined(DISABLE_OVERLAYS)
static int SysChroot(struct Machine *m, i64 path) {
  return SetOverlays(LoadStr(m, path), false);
}
#elif !defined(DISABLE_VFS)
static int SysChroot(struct Machine *m, i64 path) {
  return VfsChroot(LoadStr(m, path));
}
#endif

#ifndef DISABLE_VFS
static int SysMount(struct Machine *m, i64 source, i64 target, i64 fstype,
                    i64 mountflags, i64 data) {
  // No xlat, the VFS system will handle raw Linux options.
  return VfsMount(LoadStr(m, source), LoadStr(m, target), LoadStr(m, fstype),
                  mountflags, (void *)(uintptr_t)data);
}
#endif

static int SysSync(struct Machine *m) {
#ifdef HAVE_SYNC
  sync();
  return 0;
#else
  // sync() is an xsi extension to posix. not having sync() puts us in a
  // difficult position because the libc wrapper for sync() can't report
  // errors. the best we can do is sync what we know and report an error
  struct Dll *e;
  int *p2, *p = 0;
  size_t i, n = 0;
  LOGF("host platform doesn't support sync()");
  LOCK(&m->system->fds.lock);
  for (e = dll_first(m->system->fds.list); e;
       e = dll_next(m->system->fds.list, e)) {
    if ((p2 = (int *)realloc(p, (n + 1) * sizeof(*p)))) {
      (p = p2)[n++] = FD_CONTAINER(e)->fildes;
    } else {
      break;
    }
  }
  UNLOCK(&m->system->fds.lock);
  for (i = 0; i < n; ++i) {
    VfsFsync(p[i]);
  }
  free(p);
  return enosys();
#endif
}

static int CheckSyncable(int fildes) {
#ifndef __linux
  // FreeBSD doesn't return EINVAL like Linux does when trying to
  // synchronize character devices, e.g. /dev/null. An unresolved
  // question though is if FreeBSD actually does something here.
  struct stat st;
  if (!VfsFstat(fildes, &st) &&  //
      (S_ISCHR(st.st_mode) ||    //
       S_ISFIFO(st.st_mode) ||   //
       S_ISLNK(st.st_mode) ||    //
       S_ISSOCK(st.st_mode))) {
    return einval();
  }
#endif
  return 0;
}

static int SysFsync(struct Machine *m, i32 fildes) {
  if (CheckSyncable(fildes) == -1) return -1;
#ifdef F_FULLSYNC
  int rc;
  // MacOS fsync() provides weaker guarantees than Linux fsync()
  // https://mjtsai.com/blog/2022/02/17/apple-ssd-benchmarks-and-f_fullsync/
  if ((rc = VfsFcntl(fildes, F_FULLFSYNC, 0))) {
    // If the FULLFSYNC failed, fall back to attempting an fsync(). It
    // shouldn't be possible for fullfsync to fail on the local file
    // system (on OSX), so failure indicates that FULLFSYNC isn't
    // supported for this file system. So, attempt an fsync and (for
    // now) ignore the overhead of a superfluous fcntl call. It'd be
    // better to detect fullfsync support once and avoid the fcntl call
    // every time sync is called. ──Quoth SQLite (os_unix.c) It's also
    // possible for F_FULLFSYNC to fail on Cosmopolitan Libc when our
    // binary isn't running on MacOS.
    rc = VfsFsync(fildes);
  }
  return rc;
#else
  return VfsFsync(fildes);
#endif
}

static int SysFdatasync(struct Machine *m, i32 fildes) {
  if (CheckSyncable(fildes) == -1) return -1;
#ifdef F_FULLSYNC
  int rc;
  if ((rc = VfsFcntl(fildes, F_FULLFSYNC, 0))) {
    rc = VfsFsync(fildes);
  }
  return rc;
#elif defined(__APPLE__)
  // fdatasync() on HFS+ doesn't yet flush the file size if it changed
  // correctly so currently we default to the macro that redefines
  // fdatasync() to fsync(). ──Quoth SQLite (os_unix.c)
  return VfsFsync(fildes);
#elif defined(__HAIKU__)
  // Haiku doesn't have fdatasync() yet
  return VfsFsync(fildes);
#else
  return VfsFdatasync(fildes);
#endif
}

static int SysChdir(struct Machine *m, i64 path) {
  return VfsChdir(LoadStr(m, path));
}

static int SysFchdir(struct Machine *m, i32 fildes) {
  return VfsFchdir(fildes);
}

static int XlatLock(int x) {
  switch (x) {
    case LOCK_UN_LINUX:
      return LOCK_UN;
    case LOCK_SH_LINUX:
      return LOCK_SH;
    case LOCK_SH_LINUX | LOCK_NB_LINUX:
      return LOCK_SH | LOCK_NB;
    case LOCK_EX_LINUX:
      return LOCK_EX;
    case LOCK_EX_LINUX | LOCK_NB_LINUX:
      return LOCK_EX | LOCK_NB;
    default:
      LOGF("bad flock() type: %#x", x);
      return einval();
  }
}

static int SysFlock(struct Machine *m, i32 fd, i32 lock) {
  if ((lock = XlatLock(lock)) == -1) return -1;
  return VfsFlock(fd, lock);
}

static int SysShutdown(struct Machine *m, i32 fd, i32 how) {
  return VfsShutdown(fd, XlatShutdown(how));
}

static int SysListen(struct Machine *m, i32 fd, i32 backlog) {
  return VfsListen(fd, backlog);
}

static int SysMkdirat(struct Machine *m, i32 dirfd, i64 path, i32 mode) {
  return VfsMkdir(GetDirFildes(dirfd), LoadStr(m, path), mode);
}

static int SysMkdir(struct Machine *m, i64 path, i32 mode) {
  return SysMkdirat(m, AT_FDCWD_LINUX, path, mode);
}

static int SysFchmod(struct Machine *m, i32 fd, u32 mode) {
  return VfsFchmod(fd, mode);
}

static int SysFchmodatImpl(struct Machine *m, i32 dirfd, i64 path, u32 mode,
                           i32 flags) {
  const char *p = LoadStr(m, path);
  if (!p) return -1;
  // On Linux, chmod on a symlink with AT_SYMLINK_NOFOLLOW is not supported.
  // If it's a symlink and caller wants nofollow, just return success.
  if (flags & AT_SYMLINK_NOFOLLOW_LINUX) {
    struct stat st;
    if (fstatat(GetDirFildes(dirfd), p, &st, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISLNK(st.st_mode)) {
      return 0;
    }
  }
  return VfsChmod(GetDirFildes(dirfd), p, mode, flags);
}

static int SysFchmodat(struct Machine *m, i32 dirfd, i64 path, u32 mode) {
  int flags = 0;
  if (m->system->isfreebsd) {
    flags = XlatFreeBSDAtFlags((i32)Get64(m->r10));
  }
  return SysFchmodatImpl(m, dirfd, path, mode, flags);
}

// FreeBSD struct flock layout: start(8) len(8) pid(4) type(2) whence(2) sysid(4)
struct flock_freebsd {
  u8 start[8];
  u8 len[8];
  u8 pid[4];
  u8 type[2];
  u8 whence[2];
  u8 sysid[4];
};

static int SysFcntlLock(struct Machine *m, int systemfd, int cmd, i64 arg) {
  int rc;
  int whence;
  int syscmd;
  struct flock flock;
  struct flock_linux flock_linux;
  if (m->system->isfreebsd) {
    // FreeBSD flock layout and type values differ from Linux:
    //   FreeBSD: F_RDLCK=1 F_UNLCK=2 F_WRLCK=3
    //   Linux:   F_RDLCK=0 F_WRLCK=1 F_UNLCK=2
    struct flock_freebsd fbsd;
    u16 fbsd_type;
    if (CopyFromUserRead(m, &fbsd, arg, sizeof(fbsd))) return -1;
    fbsd_type = Read16(fbsd.type);
    if (fbsd_type == 1) {
      Write16(flock_linux.type, F_RDLCK_LINUX);
    } else if (fbsd_type == 2) {
      Write16(flock_linux.type, F_UNLCK_LINUX);
    } else if (fbsd_type == 3) {
      Write16(flock_linux.type, F_WRLCK_LINUX);
    } else {
      return einval();
    }
    Write16(flock_linux.whence, Read16(fbsd.whence));
    Write64(flock_linux.start, Read64(fbsd.start));
    Write64(flock_linux.len, Read64(fbsd.len));
    Write32(flock_linux.pid, Read32(fbsd.pid));
  } else if (CopyFromUserRead(m, &flock_linux, arg, sizeof(flock_linux))) {
    return -1;
  }
  if (cmd == F_SETLK_LINUX) {
    syscmd = F_SETLK;
  } else if (cmd == F_SETLKW_LINUX) {
    syscmd = F_SETLKW;
  } else {
    syscmd = F_GETLK;
  }
  memset(&flock, 0, sizeof(flock));
  if (Read16(flock_linux.type) == F_RDLCK_LINUX) {
    flock.l_type = F_RDLCK;
  } else if (Read16(flock_linux.type) == F_WRLCK_LINUX) {
    flock.l_type = F_WRLCK;
  } else if (Read16(flock_linux.type) == F_UNLCK_LINUX) {
    flock.l_type = F_UNLCK;
  } else {
    return einval();
  }
  if ((whence = XlatWhence(Read16(flock_linux.whence))) == -1) return -1;
  flock.l_whence = whence;
  flock.l_start = Read64(flock_linux.start);
  flock.l_len = Read64(flock_linux.len);
  RESTARTABLE(rc = VfsFcntl(systemfd, syscmd, &flock));
  if (rc != -1 && syscmd == F_GETLK) {
    u16 gtype, gwhence;
    if (flock.l_type == F_RDLCK) {
      gtype = F_RDLCK_LINUX;
    } else if (flock.l_type == F_WRLCK) {
      gtype = F_WRLCK_LINUX;
    } else {
      gtype = F_UNLCK_LINUX;
    }
    if (flock.l_whence == SEEK_END) {
      gwhence = SEEK_END_LINUX;
    } else if (flock.l_whence == SEEK_CUR) {
      gwhence = SEEK_CUR_LINUX;
    } else {
      gwhence = SEEK_SET_LINUX;
    }
    if (m->system->isfreebsd) {
      struct flock_freebsd fbsd;
      u16 fbsd_type;
      // Convert Linux type back to FreeBSD type
      if (gtype == F_RDLCK_LINUX) fbsd_type = 1;
      else if (gtype == F_WRLCK_LINUX) fbsd_type = 3;
      else fbsd_type = 2;  // F_UNLCK
      memset(&fbsd, 0, sizeof(fbsd));
      Write16(fbsd.type, fbsd_type);
      Write16(fbsd.whence, gwhence);
      Write64(fbsd.start, flock.l_start);
      Write64(fbsd.len, flock.l_len);
      Write32(fbsd.pid, flock.l_pid);
      CopyToUserWrite(m, arg, &fbsd, sizeof(fbsd));
    } else {
      Write16(flock_linux.type, gtype);
      Write16(flock_linux.whence, gwhence);
      Write64(flock_linux.start, flock.l_start);
      Write64(flock_linux.len, flock.l_len);
      Write32(flock_linux.pid, flock.l_pid);
      CopyToUserWrite(m, arg, &flock_linux, sizeof(flock_linux));
    }
  }
  return rc;
}

#ifdef HAVE_F_GETOWN_EX
static int UnxlatFownerType(int type) {
  if (type == F_OWNER_TID) return F_OWNER_TID_LINUX;
  if (type == F_OWNER_PID) return F_OWNER_PID_LINUX;
  if (type == F_OWNER_PGRP) return F_OWNER_PGRP_LINUX;
  LOGF("unknown f_owner_ex::type %d", type);
  return einval();
}
#endif

#ifdef F_SETOWN
static int SysFcntlSetownEx(struct Machine *m, i32 fildes, i64 addr) {
  const struct f_owner_ex_linux *gowner;
  if (!(gowner = (const struct f_owner_ex_linux *)SchlepR(m, addr,
                                                          sizeof(*gowner)))) {
    return -1;
  }
#ifdef HAVE_F_GETOWN_EX
  struct f_owner_ex howner;
  switch (Read32(gowner->type)) {
    case F_OWNER_TID_LINUX:
      howner.type = F_OWNER_TID;
      howner.pid = (i32)Read32(gowner->pid);
      break;
    case F_OWNER_PID_LINUX:
      howner.type = F_OWNER_PID;
      howner.pid = (i32)Read32(gowner->pid);
      break;
    case F_OWNER_PGRP_LINUX:
      howner.type = F_OWNER_PGRP;
      howner.pid = (i32)Read32(gowner->pid);
      break;
    default:
      LOGF("unknown f_owner_ex::type %" PRId32, Read32(gowner->type));
      return einval();
  }
  return VfsFcntl(fildes, F_SETOWN_EX, &howner);
#else
  pid_t pid;
  switch (Read32(gowner->type)) {
    case F_OWNER_PID_LINUX:
      pid = (i32)Read32(gowner->pid);
      break;
    case F_OWNER_PGRP_LINUX:
      pid = (i32)-Read32(gowner->pid);
      break;
    case F_OWNER_TID_LINUX:
      // POSIX doesn't specify fcntl() as applying per-thread
      if (!(pid = (i32)Read32(gowner->pid)) || pid == m->system->pid) {
        break;
      }
      // fallthrough
    default:
      LOGF("unknown f_owner_ex::type %" PRId32, Read32(gowner->type));
      return einval();
  }
  return VfsFcntl(fildes, F_SETOWN, pid);
#endif
}
#endif

#ifdef F_GETOWN
static int SysFcntlGetownEx(struct Machine *m, i32 fildes, i64 addr) {
  int rc;
  struct f_owner_ex_linux gowner;
  if (!IsValidMemory(m, addr, sizeof(gowner), PROT_WRITE)) return -1;
#ifdef HAVE_F_GETOWN_EX
  int type;
  struct f_owner_ex howner;
  if ((rc = VfsFcntl(fildes, F_GETOWN_EX, &howner)) != -1) {
    if ((type = UnxlatFownerType(howner.type)) != -1) {
      Write32(gowner.type, type);
      Write32(gowner.pid, howner.pid);
      CopyToUserWrite(m, addr, &gowner, sizeof(gowner));
    } else {
      rc = -1;
    }
  }
#else
  if ((rc = VfsFcntl(fildes, F_GETOWN)) != -1) {
    if (rc >= 0) {
      Write32(gowner.type, F_OWNER_PID_LINUX);
      Write32(gowner.pid, rc);
    } else {
      Write32(gowner.type, F_OWNER_PGRP_LINUX);
      Write32(gowner.pid, -rc);
    }
    CopyToUserWrite(m, addr, &gowner, sizeof(gowner));
  }
#endif
  return rc;
}
#endif

// FreeBSD fcntl command numbers that differ from Linux
#define FBSD_F_GETOWN          5
#define FBSD_F_SETOWN          6
#define FBSD_F_GETLK          11
#define FBSD_F_SETLK          12
#define FBSD_F_SETLKW         13
#define FBSD_F_DUPFD_CLOEXEC  17
#define FBSD_F_DUP2FD         10
#define FBSD_F_DUP2FD_CLOEXEC 18
#define FBSD_F_ISUNIONSTACK    21

static int SysFcntl(struct Machine *m, i32 fildes, i32 cmd, i64 arg) {
  int rc, fl;
  struct Fd *fd;
  if (m->system->isfreebsd) {
    switch (cmd) {
      case FBSD_F_GETOWN:         cmd = F_GETOWN_LINUX; break;
      case FBSD_F_SETOWN:         cmd = F_SETOWN_LINUX; break;
      case FBSD_F_GETLK:          cmd = F_GETLK_LINUX; break;
      case FBSD_F_SETLK:          cmd = F_SETLK_LINUX; break;
      case FBSD_F_SETLKW:         cmd = F_SETLKW_LINUX; break;
      case FBSD_F_DUPFD_CLOEXEC:  cmd = F_DUPFD_CLOEXEC_LINUX; break;
      case FBSD_F_DUP2FD:
        return SysDup3(m, fildes, (i32)arg, 0);
      case FBSD_F_DUP2FD_CLOEXEC:
        return SysDup3(m, fildes, (i32)arg, O_CLOEXEC_LINUX);
      case FBSD_F_ISUNIONSTACK:
        return 0;  // not a union stack
      default: break;
    }
  }
  if (cmd == F_DUPFD_LINUX) {
    return SysDupf(m, fildes, arg, F_DUPFD);
  } else if (cmd == F_DUPFD_CLOEXEC_LINUX) {
    return SysDupf(m, fildes, arg, F_DUPFD_CLOEXEC);
  }
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  if (cmd == F_GETFD_LINUX) {
    rc = (fd->oflags & O_CLOEXEC) ? FD_CLOEXEC_LINUX : 0;
  } else if (cmd == F_GETFL_LINUX) {
    rc = UnXlatOpenFlags(fd->oflags);
    if (m->system->isfreebsd) {
      // Translate Linux open flags back to FreeBSD values
      int bsd = 0;
      if (rc & O_WRONLY_LINUX) bsd |= 1;
      if (rc & O_RDWR_LINUX) bsd |= 2;
      if (rc & O_NDELAY_LINUX) bsd |= 0x0004;
      if (rc & O_APPEND_LINUX) bsd |= 0x0008;
      if (rc & O_ASYNC_LINUX) bsd |= 0x0040;
      if (rc & O_SYNC_LINUX) bsd |= 0x0080;
      if (rc & O_NOFOLLOW_LINUX) bsd |= 0x0100;
      if (rc & O_CREAT_LINUX) bsd |= 0x0200;
      if (rc & O_NOCTTY_LINUX) bsd |= 0x8000;
      if (rc & O_DIRECTORY_LINUX) bsd |= 0x20000;
      if (rc & O_CLOEXEC_LINUX) bsd |= 0x100000;
      if (rc & O_DIRECT_LINUX) bsd |= 0x10000;
      rc = bsd;
    }
  } else if (cmd == F_SETFD_LINUX) {
    if (!(arg & ~FD_CLOEXEC_LINUX)) {
      if (VfsFcntl(fd->fildes, F_SETFD, arg ? FD_CLOEXEC : 0) != -1) {
        fd->oflags &= ~O_CLOEXEC;
        if (arg) fd->oflags |= O_CLOEXEC;
        rc = 0;
      } else {
        rc = -1;
      }
    } else {
      rc = einval();
    }
  } else if (cmd == F_SETFL_LINUX) {
    if (m->system->isfreebsd) {
      // Translate FreeBSD open flags to Linux before XlatOpenFlags
      int bsd = arg;
      arg = 0;
      if (bsd & 0x0004) arg |= O_NDELAY_LINUX;   // O_NONBLOCK
      if (bsd & 0x0008) arg |= O_APPEND_LINUX;
      if (bsd & 0x0040) arg |= O_ASYNC_LINUX;
      if (bsd & 0x10000) arg |= O_DIRECT_LINUX;
    }
    fl = XlatOpenFlags(arg & (O_APPEND_LINUX | O_ASYNC_LINUX | O_DIRECT_LINUX |
                              O_NOATIME_LINUX | O_NDELAY_LINUX));
    if (VfsFcntl(fd->fildes, F_SETFL, fl) != -1) {
      fd->oflags &= ~SETFL_FLAGS;
      fd->oflags |= fl;
      rc = 0;
    } else {
      rc = -1;
    }
  } else if (cmd == F_SETLK_LINUX ||   //
             cmd == F_SETLKW_LINUX ||  //
             cmd == F_GETLK_LINUX) {
    UnlockFd(fd);
    return SysFcntlLock(m, fildes, cmd, arg);
#ifdef F_SETOWN
  } else if (cmd == F_SETOWN_LINUX) {
    rc = VfsFcntl(fd->fildes, F_SETOWN, arg);
#endif
#ifdef F_GETOWN
  } else if (cmd == F_GETOWN_LINUX) {
    rc = VfsFcntl(fd->fildes, F_GETOWN);
#endif
#ifndef DISABLE_NONPOSIX
#ifdef HAVE_F_GETOWN_EX
  } else if (cmd == F_SETOWN_EX_LINUX) {
    rc = SysFcntlSetownEx(m, fd->fildes, arg);
#endif
#ifdef HAVE_F_GETOWN_EX
  } else if (cmd == F_GETOWN_EX_LINUX) {
    rc = SysFcntlGetownEx(m, fd->fildes, arg);
#endif
#endif
  } else {
    LOGF("missing fcntl() command %" PRId32, cmd);
    rc = einval();
  }
  UnlockFd(fd);
  return rc;
}

static ssize_t SysReadlinkat(struct Machine *m, int dirfd, i64 path,
                             i64 bufaddr, i64 bufsiz) {
  char *buf;
  ssize_t rc;
  // This system call raises EINVAL when "bufsiz is not positive."
  // ──Quoth the Linux Programmer's Manual § readlink(2). Some libc
  // implementations (e.g. Musl) consider it to be posixly incorrect.
  if (bufsiz <= 0) return einval();
  if (!(buf = (char *)AddToFreeList(m, malloc(bufsiz)))) return -1;
  if ((rc = VfsReadlink(GetDirFildes(dirfd), LoadStr(m, path), buf, bufsiz)) !=
      -1) {
    if (CopyToUserWrite(m, bufaddr, buf, rc) == -1) rc = -1;
  }
  return rc;
}

static int SysChmod(struct Machine *m, i64 path, u32 mode) {
  return SysFchmodatImpl(m, AT_FDCWD_LINUX, path, mode, 0);
}

static int SysTruncate(struct Machine *m, i64 pathaddr, i64 length) {
  int rc, fd;
  const char *path;
  if (length < 0) return einval();
  if (length > NUMERIC_MAX(off_t)) return eoverflow();
  if (!(path = LoadStr(m, pathaddr))) return -1;
  RESTARTABLE(fd = VfsOpen(AT_FDCWD, path, O_RDWR | O_CLOEXEC, 0));
  if (fd == -1) return -1;
  rc = VfsFtruncate(fd, length);
  VfsClose(fd);
  return rc;
}

static int SysSymlinkat(struct Machine *m, i64 targetpath, i32 newdirfd,
                        i64 linkpath) {
  return VfsSymlink(LoadStr(m, targetpath), GetDirFildes(newdirfd),
                    LoadStr(m, linkpath));
}

static int SysSymlink(struct Machine *m, i64 targetpath, i64 linkpath) {
  return SysSymlinkat(m, targetpath, AT_FDCWD_LINUX, linkpath);
}

static int SysReadlink(struct Machine *m, i64 path, i64 bufaddr, u64 size) {
  return SysReadlinkat(m, AT_FDCWD_LINUX, path, bufaddr, size);
}

static int SysMknodat(struct Machine *m, i32 dirfd, i64 path, i32 mode,
                      u64 dev) {
  _Static_assert(S_IFIFO == 0010000, "");   // pipe
  _Static_assert(S_IFCHR == 0020000, "");   // character device
  _Static_assert(S_IFDIR == 0040000, "");   // directory
  _Static_assert(S_IFBLK == 0060000, "");   // block device
  _Static_assert(S_IFREG == 0100000, "");   // regular file
  _Static_assert(S_IFLNK == 0120000, "");   // symbolic link
  _Static_assert(S_IFSOCK == 0140000, "");  // socket
  _Static_assert(S_IFMT == 0170000, "");    // mask of file types above
  if ((mode & S_IFMT) == S_IFIFO) {
    return VfsMkfifo(GetDirFildes(dirfd), LoadStr(m, path), mode & ~S_IFMT);
  } else {
    LOGF("mknod mode %#o not supported yet", mode);
    return enosys();
  }
}

static int SysMknod(struct Machine *m, i64 path, i32 mode, u64 dev) {
  return SysMknodat(m, AT_FDCWD_LINUX, path, mode, dev);
}

static int XlatPrio(int x) {
  switch (x) {
    XLAT(0, PRIO_PROCESS);
    XLAT(1, PRIO_PGRP);
    XLAT(2, PRIO_USER);
    default:
      return -1;
  }
}

static int SysGetpriority(struct Machine *m, i32 which, u32 who) {
  int rc;
  errno = 0;
  rc = getpriority(XlatPrio(which), who);
  if (rc == -1 && errno) return -1;
  return MAX(-20, MIN(19, rc)) + 20;
}

static int SysSetpriority(struct Machine *m, i32 which, u32 who, int prio) {
  return setpriority(XlatPrio(which), who, prio);
}

static int XlatUnlinkatFlags(int x) {
  int res = 0;
  if (x & AT_REMOVEDIR_LINUX) {
    res |= AT_REMOVEDIR;
    x &= ~AT_REMOVEDIR_LINUX;
  }
  if (x) {
    LOGF("%s() flags %#x not supported", "unlinkat", x);
    return einval();
  }
  return res;
}

static int SysUnlinkat(struct Machine *m, i32 dirfd, i64 pathaddr, i32 flags) {
  int rc;
  const char *path;
  dirfd = GetDirFildes(dirfd);
  if (m->system->isfreebsd) flags = XlatFreeBSDAtFlags(flags);
  if ((flags = XlatUnlinkatFlags(flags)) == -1) return -1;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  rc = VfsUnlink(dirfd, path, flags);
#ifndef __linux
  // POSIX.1 says unlink(directory) raises EPERM but on Linux
  // it always raises EISDIR, which is so much less ambiguous
  if (rc == -1 && !flags && errno == EPERM) {
    struct stat st;
    if (!fstatat(dirfd, path, &st, 0) && S_ISDIR(st.st_mode)) {
      errno = EISDIR;
    } else {
      errno = EPERM;
    }
  }
#endif
  return rc;
}

static int SysUnlink(struct Machine *m, i64 pathaddr) {
  return SysUnlinkat(m, AT_FDCWD_LINUX, pathaddr, 0);
}

static int SysRmdir(struct Machine *m, i64 path) {
  return SysUnlinkat(m, AT_FDCWD_LINUX, path, AT_REMOVEDIR_LINUX);
}

static int SysRenameat2(struct Machine *m, int srcdirfd, i64 srcpath,
                        int dstdirfd, i64 dstpathaddr, i32 flags) {
  struct stat st;
  i32 unsupported;
  const char *dstpath;
  i32 supported = RENAME_NOREPLACE_LINUX;
  if ((unsupported = flags & ~supported)) {
    LOGF("%s flags not supported yet: %#" PRIx32, "renameat2", unsupported);
    return einval();
  }
  if (!(dstpath = LoadStr(m, dstpathaddr))) return -1;
  // TODO: check for renameat2 in configure script
  if ((flags & RENAME_NOREPLACE_LINUX) &&
      !VfsStat(GetDirFildes(dstdirfd), dstpath, &st, AT_SYMLINK_NOFOLLOW)) {
    errno = EEXIST;
    return -1;
  }
  return VfsRename(GetDirFildes(srcdirfd), LoadStr(m, srcpath),
                   GetDirFildes(dstdirfd), dstpath);
}

static int SysRenameat(struct Machine *m, int srcdirfd, i64 srcpath,
                       int dstdirfd, i64 dstpath) {
  return SysRenameat2(m, srcdirfd, srcpath, dstdirfd, dstpath, 0);
}

static int SysRename(struct Machine *m, i64 src, i64 dst) {
  return SysRenameat(m, AT_FDCWD_LINUX, src, AT_FDCWD_LINUX, dst);
}

static int XlatLinkatFlags(int x) {
  int res = 0;
  if (x & AT_SYMLINK_NOFOLLOW_LINUX) {
    x &= ~AT_SYMLINK_NOFOLLOW_LINUX;  // default behavior
  }
  if (x & AT_SYMLINK_FOLLOW_LINUX) {
    res |= AT_SYMLINK_FOLLOW;
    x &= ~AT_SYMLINK_FOLLOW_LINUX;
  }
  if (x) {
    LOGF("%s() flags %d not supported", "linkat", x);
    return -1;
  }
  return res;
}

static i32 SysLinkat(struct Machine *m,  //
                     i32 olddirfd,       //
                     i64 oldpath,        //
                     i32 newdirfd,       //
                     i64 newpath,        //
                     i32 flags) {
  return VfsLink(GetDirFildes(olddirfd), LoadStr(m, oldpath),
                 GetDirFildes(newdirfd), LoadStr(m, newpath),
                 XlatLinkatFlags(flags));
}

static int SysLink(struct Machine *m, i64 existingpath, i64 newpath) {
  return SysLinkat(m, AT_FDCWD_LINUX, existingpath, AT_FDCWD_LINUX, newpath, 0);
}

static bool IsBlinkSig(struct System *s, int sig) {
  unassert(1 <= sig && sig <= 64);
  return !!(s->blinksigs & ((u64)1 << (sig - 1)));
}

static void ResetTimerDispositions(struct System *s) {
  struct itimerval it;
  memset(&it, 0, sizeof(it));
  setitimer(ITIMER_REAL, &it, 0);
}

static void ResetSignalDispositions(struct System *s) {
  int sig;
  int syssig;
  LOCK(&s->sig_lock);
  for (sig = 1; sig <= 64; ++sig) {
    if (!IsBlinkSig(s, sig) &&
        Read64(s->hands[sig - 1].handler) != SIG_IGN_LINUX &&
        Read64(s->hands[sig - 1].handler) != SIG_DFL_LINUX &&
        (syssig = XlatSignal(sig)) != -1) {
      Write64(s->hands[sig - 1].handler, SIG_DFL_LINUX);
      signal(syssig, SIG_DFL);
    }
  }
  UNLOCK(&s->sig_lock);
}

static void ExecveBlink(struct Machine *m, char *prog, char **argv,
                        char **envp) {
  char *execfn;
  sigset_t block;
  if (m->system->exec) {
    // it's worth blocking signals on the outside of the if statement
    // since open() during the executable check, might possibly EINTR
    // and the same could apply to calling close() on our cloexec fds
    execfn = prog;
    sigfillset(&block);
    unassert(!pthread_sigmask(SIG_BLOCK, &block, &m->system->exec_sigmask));
    if (CanEmulateExecutable(m, &prog, &argv)) {
      // point of no return
      // prog/argv/envp are copied onto the freelist
      m->sysdepth = 0;
      CollectPageLocks(m);
      // TODO(jart): Prevent possibility of stack overflow.
      SYS_LOGF("m->system->exec(%s)", prog);
      SysCloseExec(m->system);
      ResetTimerDispositions(m->system);
      ResetSignalDispositions(m->system);
      if (m->system->vfork_done_fd) {
        close(m->system->vfork_done_fd);
        m->system->vfork_done_fd = 0;
      }
      _Exit(m->system->exec(execfn, prog, argv, envp));
    }
    unassert(!pthread_sigmask(SIG_SETMASK, &m->system->exec_sigmask, 0));
  }
}

static int SysExecve(struct Machine *m, i64 pa, i64 aa, i64 ea) {
  char *prog, **argv, **envp;
  if (!(prog = CopyStr(m, pa))) return -1;
  if (!(argv = CopyStrList(m, aa))) return -1;
  if (!(envp = CopyStrList(m, ea))) return -1;
  LOCK(&m->system->exec_lock);
  ExecveBlink(m, prog, argv, envp);
  SYS_LOGF("execve(%s)", prog);
  VfsExecve(prog, argv, envp);
  UNLOCK(&m->system->exec_lock);
  return -1;
}

static int SysWait4(struct Machine *m, int pid, i64 opt_out_wstatus_addr,
                    int options, i64 opt_out_rusage_addr) {
  int rc;
  int wstatus;
  i32 gwstatus;
  u8 gwstatusb[4];
  struct rusage hrusage;
  struct rusage_linux grusage;
  if (m->system->isfreebsd) {
    // FreeBSD wait4 flags differ from Linux:
    // FreeBSD WCONTINUED=4, Linux WCONTINUED=8
    // FreeBSD WNOWAIT=8, Linux WNOWAIT=0x01000000
    int fbsd = options;
    options = 0;
    if (fbsd & 1) options |= WNOHANG_LINUX;
    if (fbsd & 2) options |= WUNTRACED_LINUX;
    if (fbsd & 4) options |= WCONTINUED_LINUX;  // FreeBSD WCONTINUED=4 → Linux 8
    if (fbsd & 8) options |= WNOWAIT_LINUX;     // FreeBSD WNOWAIT=8 → Linux 0x01000000
  }
  if ((options = XlatWait(options)) == -1) return -1;
  if ((opt_out_wstatus_addr && !IsValidMemory(m, opt_out_wstatus_addr,
                                              sizeof(gwstatusb), PROT_WRITE)) ||
      (opt_out_rusage_addr &&
       !IsValidMemory(m, opt_out_rusage_addr, sizeof(grusage), PROT_WRITE))) {
    return -1;
  }
#ifdef HAVE_WAIT4
  RESTARTABLE(rc = wait4(pid, &wstatus, options, &hrusage));
#else
  memset(&hrusage, 0, sizeof(hrusage));
  if (opt_out_rusage_addr && (!pid || pid == -1)) {
    LOGF("wait4(rusage) with indeterminate pid not possible on this platform");
  } else {
    getrusage(pid < 0 ? -pid : pid, &hrusage);
  }
  RESTARTABLE(rc = waitpid(pid, &wstatus, options));
#endif
  if (rc != -1 && rc != 0) {
    if (opt_out_wstatus_addr) {
#ifdef WIFCONTINUED
      if (WIFCONTINUED(wstatus)) {
        gwstatus = 0xffff;
        SYS_LOGF("pid %d continued", rc);
      } else
#endif
          if (WIFEXITED(wstatus)) {
        int exitcode;
        exitcode = WEXITSTATUS(wstatus) & 255;
        gwstatus = exitcode << 8;
        SYS_LOGF("pid %d exited %d", rc, exitcode);
      } else if (WIFSTOPPED(wstatus)) {
        int stopsig;
        stopsig = UnXlatSignal(WSTOPSIG(wstatus));
        gwstatus = stopsig << 8 | 127;
        SYS_LOGF("pid %d stopped %s", rc, DescribeSignal(stopsig));
      } else {
        int termsig;
        unassert(WIFSIGNALED(wstatus));
        termsig = UnXlatSignal(WTERMSIG(wstatus));
        SYS_LOGF("pid %d terminated %s", rc, DescribeSignal(termsig));
        gwstatus = termsig & 127;
      }
      Write32(gwstatusb, gwstatus);
      CopyToUserWrite(m, opt_out_wstatus_addr, gwstatusb, sizeof(gwstatusb));
    }
    if (opt_out_rusage_addr) {
      XlatRusageToLinux(&grusage, &hrusage);
      CopyToUserWrite(m, opt_out_rusage_addr, &grusage, sizeof(grusage));
    }
  }
  return rc;
}

static int SysGetrusage(struct Machine *m, i32 resource, i64 rusageaddr) {
  int rc;
  struct rusage hrusage;
  struct rusage_linux grusage;
  if ((rc = getrusage(XlatRusage(resource), &hrusage)) != -1) {
    XlatRusageToLinux(&grusage, &hrusage);
    if (CopyToUserWrite(m, rusageaddr, &grusage, sizeof(grusage)) == -1) {
      rc = -1;
    }
  }
  return rc;
}

// FreeBSD RLIMIT constants 6-10 differ from Linux:
//   FreeBSD: MEMLOCK=6 NPROC=7 NOFILE=8 SBSIZE=9 AS=10
//   Linux:   NPROC=6   NOFILE=7 MEMLOCK=8 AS=9
static int XlatFreeBSDResource(int r) {
  switch (r) {
    case 6: return RLIMIT_MEMLOCK_LINUX;  // MEMLOCK
    case 7: return RLIMIT_NPROC_LINUX;    // NPROC
    case 8: return RLIMIT_NOFILE_LINUX;   // NOFILE
    case 10: return RLIMIT_AS_LINUX;      // AS/VMEM
    default: return r;  // 0-5 are same
  }
}

static bool IsSupportedResourceLimit(int resource) {
  return resource == RLIMIT_AS_LINUX ||    //
         resource == RLIMIT_DATA_LINUX ||  //
         resource == RLIMIT_NOFILE_LINUX;
}

static void GetResourceLimit_(struct Machine *m, int resource,
                              struct rlimit_linux *lux) {
  LOCK(&m->system->mmap_lock);
  memcpy(lux, m->system->rlim + resource, sizeof(*lux));
  UNLOCK(&m->system->mmap_lock);
}

static int SetResourceLimit(struct Machine *m, int resource,
                            const struct rlimit_linux *lux) {
  int rc;
  LOCK(&m->system->mmap_lock);
  if (Read64(lux->cur) <= Read64(m->system->rlim[resource].max) &&
      Read64(lux->max) <= Read64(m->system->rlim[resource].max)) {
    memcpy(m->system->rlim + resource, lux, sizeof(*lux));
    rc = 0;
  } else {
    rc = eperm();
  }
  UNLOCK(&m->system->mmap_lock);
  return rc;
}

static int SysGetrlimit(struct Machine *m, i32 resource, i64 rlimitaddr) {
  int rc;
  struct rlimit rlim;
  struct rlimit_linux lux;
  if (m->system->isfreebsd) resource = XlatFreeBSDResource(resource);
  if (IsSupportedResourceLimit(resource)) {
    GetResourceLimit_(m, resource, &lux);
    return CopyToUserWrite(m, rlimitaddr, &lux, sizeof(lux));
  }
  if ((rc = getrlimit(XlatResource(resource), &rlim)) != -1) {
    XlatRlimitToLinux(&lux, &rlim);
    if (CopyToUserWrite(m, rlimitaddr, &lux, sizeof(lux)) == -1) rc = -1;
  }
  return rc;
}

static int SysSetrlimit(struct Machine *m, i32 resource, i64 rlimitaddr) {
  int sysresource;
  struct rlimit rlim;
  const struct rlimit_linux *lux;
  if (m->system->isfreebsd) resource = XlatFreeBSDResource(resource);
  if (!(lux = (const struct rlimit_linux *)SchlepR(m, rlimitaddr,
                                                   sizeof(*lux)))) {
    return -1;
  }
  if (IsSupportedResourceLimit(resource)) {
    return SetResourceLimit(m, resource, lux);
  }
  if ((sysresource = XlatResource(resource)) == -1) return -1;
  XlatLinuxToRlimit(sysresource, &rlim, lux);
  return setrlimit(sysresource, &rlim);
}

static int SysPrlimit(struct Machine *m, i32 pid, i32 resource,
                      i64 new_rlimit_addr, i64 old_rlimit_addr) {
  if (pid && pid != m->system->pid) {
    return eperm();
  }
#ifndef TINY
  if ((old_rlimit_addr &&
       !IsValidMemory(m, old_rlimit_addr, sizeof(struct rlimit_linux),
                      PROT_WRITE)) &&
      (new_rlimit_addr &&
       !IsValidMemory(m, new_rlimit_addr, sizeof(struct rlimit_linux),
                      PROT_READ))) {
    return -1;
  }
#endif
  if ((old_rlimit_addr && SysGetrlimit(m, resource, old_rlimit_addr) == -1) ||
      (new_rlimit_addr && SysSetrlimit(m, resource, new_rlimit_addr) == -1)) {
    return -1;
  }
  return 0;
}

static int SysSysinfo(struct Machine *m, i64 siaddr) {
  struct sysinfo_linux si;
  if (sysinfo_linux(&si) == -1) return -1;
  CopyToUserWrite(m, siaddr, &si, sizeof(si));
  return 0;
}

static i64 SysGetcwd(struct Machine *m, i64 bufaddr, i64 size) {
  i64 res;
  size_t n;
  char buf[PATH_MAX + 1];
  if (size < 0) return enomem();
  if (VfsGetcwd(buf, sizeof(buf))) {
    n = strlen(buf) + 1;
    if (size < n) {
      res = erange();
    } else if (CopyToUserWrite(m, bufaddr, buf, n) != -1) {
      res = n;
    } else {
      res = -1;
    }
  } else {
    res = -1;
  }
  return res;
}

static ssize_t SysGetrandom(struct Machine *m, i64 a, size_t n, int f) {
  char *p;
  ssize_t rc;
  int besteffort, unsupported;
  besteffort = GRND_NONBLOCK_LINUX | GRND_RANDOM_LINUX;
  if ((unsupported = f & ~besteffort)) {
    LOGF("%s() flags %d not supported", "getrandom", unsupported);
    return einval();
  }
  if (n) {
    if (!(p = (char *)AddToFreeList(m, malloc(n)))) return -1;
    RESTARTABLE(rc = GetRandom(p, n, f));
    if (rc != -1) {
      if (CopyToUserWrite(m, a, p, rc) == -1) {
        rc = -1;
      }
    }
  } else {
    rc = 0;
  }
  return rc;
}

void OnSignal(int sig, siginfo_t *si, void *uc) {
  SIG_LOGF("OnSignal(%s)", DescribeSignal(UnXlatSignal(sig)));
  EnqueueSignal(g_machine, UnXlatSignal(sig));
}

static int SysSigaction(struct Machine *m, int sig, i64 act, i64 old,
                        u64 sigsetsize) {
  int syssig;
  u64 flags = 0;
  i64 handler = 0;
  bool isignored = false;
  struct sigaction syshand;
  struct sigaction_linux hand;
  u32 supported = SA_SIGINFO_LINUX |    //
                  SA_RESTART_LINUX |    //
                  SA_ONSTACK_LINUX |    //
                  SA_NODEFER_LINUX |    //
                  SA_RESTORER_LINUX |   //
                  SA_RESETHAND_LINUX |  //
#ifdef SA_NOCLDWAIT
                  SA_NOCLDWAIT_LINUX |  //
#endif
                  SA_NOCLDSTOP_LINUX;
  if (sigsetsize != 8) return einval();
  if (!(1 <= sig && sig <= 64)) return einval();
  if (sig == SIGKILL_LINUX || sig == SIGSTOP_LINUX) return einval();
  if (old && !IsValidMemory(m, old, sizeof(hand), PROT_WRITE)) return -1;
  memset(&hand, 0, sizeof(hand));
  if (act) {
    if (CopyFromUserRead(m, &hand, act, sizeof(hand)) == -1) return -1;
    flags = Read64(hand.flags);
    handler = Read64(hand.handler);
    if (handler == SIG_IGN_LINUX) {
      flags &= ~SA_NOCLDWAIT_LINUX;
    }
    if (flags & ~supported) {
      LOGF("unrecognized sigaction() flags: %#" PRIx64, flags & ~supported);
      return einval();
    }
    switch (handler) {
      case SIG_DFL_LINUX:
        isignored = IsSignalIgnoredByDefault(sig);
        break;
      case SIG_IGN_LINUX:
        isignored = true;
        break;
      default:
        if (!(flags & SA_RESTORER_LINUX)) {
          LOGF("sigaction() flags missing SA_RESTORER");
          return einval();
        }
        if (!IsValidMemory(m, Read64(hand.restorer), 1, PROT_EXEC)) {
          LOGF("sigaction() SA_RESTORER at %" PRIx64 " isn't executable",
               Read64(hand.restorer));
          return -1;
        }
        break;
    }
  }
  LOCK(&m->system->sig_lock);
  if (old) {
    CopyToUserWrite(m, old, &m->system->hands[sig - 1], sizeof(hand));
  }
  if (act) {
    m->system->hands[sig - 1] = hand;
    if (isignored) {
      m->signals &= ~((u64)1 << (sig - 1));
    }
    if ((syssig = XlatSignal(sig)) != -1 && !IsBlinkSig(m->system, sig)) {
      sigfillset(&syshand.sa_mask);
      syshand.sa_flags = SA_SIGINFO;
      if (flags & SA_NOCLDSTOP_LINUX) syshand.sa_flags |= SA_NOCLDSTOP;
#ifdef SA_NOCLDWAIT
      if (flags & SA_NOCLDWAIT_LINUX) syshand.sa_flags |= SA_NOCLDWAIT;
#endif
      switch (handler) {
        case SIG_DFL_LINUX:
          syshand.sa_handler = SIG_DFL;
          break;
        case SIG_IGN_LINUX:
          syshand.sa_handler = SIG_IGN;
          break;
        default:
          syshand.sa_sigaction = OnSignal;
          break;
      }
      if (sigaction(syssig, &syshand, 0)) {
        LOGF("system sigaction(%s) returned %s", DescribeSignal(sig),
             DescribeHostErrno(errno));
      }
    }
  }
  UNLOCK(&m->system->sig_lock);
  return 0;
}

static int SysGetitimer(struct Machine *m, int which, i64 curvaladdr) {
  int rc;
  struct itimerval it;
  struct itimerval_linux git;
  if ((rc = getitimer(UnXlatItimer(which), &it)) != -1) {
    XlatItimervalToLinux(&git, &it);
    CopyToUserWrite(m, curvaladdr, &git, sizeof(git));
  }
  return rc;
}

static int SysSetitimer(struct Machine *m, int which, i64 neuaddr,
                        i64 oldaddr) {
  int rc;
  struct itimerval_linux gold;
  struct itimerval neu, *neup, old;
  const struct itimerval_linux *git = 0;
  if ((neuaddr && !(git = (const struct itimerval_linux *)SchlepR(
                        m, neuaddr, sizeof(*git)))) ||
      (oldaddr && !IsValidMemory(m, oldaddr, sizeof(gold), PROT_WRITE))) {
    return -1;
  }
  if (git) {
    XlatLinuxToItimerval(&neu, git);
    neup = &neu;
  } else {
    neup = 0;
  }
  if ((rc = setitimer(UnXlatItimer(which), neup, &old)) != -1) {
    if (oldaddr) {
      XlatItimervalToLinux(&gold, &old);
      CopyToUserWrite(m, oldaddr, &gold, sizeof(gold));
    }
  }
  return rc;
}

static int SysNanosleep(struct Machine *m, i64 req, i64 rem) {
  struct timespec_linux gt;
  const struct timespec_linux *gtp;
  struct timespec ts, now, deadline;
  now = GetTime();
  if ((rem && !IsValidMemory(m, rem, sizeof(gtp), PROT_WRITE)) ||
      !(gtp = (const struct timespec_linux *)SchlepR(m, req, sizeof(*gtp)))) {
    return -1;
  }
  ts.tv_sec = Read64(gtp->sec);
  ts.tv_nsec = Read64(gtp->nsec);
  if (ts.tv_sec < 0) return einval();
  if (!(0 <= ts.tv_nsec && ts.tv_nsec < 1000000000)) return einval();
  deadline = AddTime(now, ts);
  for (;;) {
    if (CompareTime(now, deadline) >= 0) return 0;
    ts = SubtractTime(deadline, now);
    if (nanosleep(&ts, 0)) {
      unassert(errno == EINTR);
      // this may run a guest signal handler before returning
      if (CheckInterrupt(m, false)) {
        // a signal was delivered or is about to be delivered
        if (rem) {
          // rem is only updated when -1 w/ eintr is returned
          now = GetTime();
          if (CompareTime(now, deadline) < 0) {
            ts = SubtractTime(deadline, now);
          } else {
            ts = GetZeroTime();
          }
          Write64(gt.sec, ts.tv_sec);
          Write64(gt.nsec, ts.tv_nsec);
          CopyToUserWrite(m, rem, &gt, sizeof(gt));
        }
        return -1;
      }
    }
    // sleep apis aren't nearly as fast and reliable as time apis
    // even if nanosleep() claims it slept the full time we check
    now = GetTime();
  }
}

static int SysClockNanosleep(struct Machine *m, int clock, int flags,
                             i64 reqaddr, i64 remaddr) {
  int rc;
  clock_t sysclock;
  struct timespec req, rem;
  struct timespec_linux gtimespec;
  if (XlatClock(clock, &sysclock) == -1) return -1;
  if (flags & ~TIMER_ABSTIME_LINUX) return einval();
  if (CopyFromUserRead(m, &gtimespec, reqaddr, sizeof(gtimespec)) == -1) {
    return -1;
  }
  req.tv_sec = Read64(gtimespec.sec);
  req.tv_nsec = Read64(gtimespec.nsec);
TryAgain:
#if defined(TIMER_ABSTIME) && !defined(__OpenBSD__)
  flags = flags & TIMER_ABSTIME_LINUX ? TIMER_ABSTIME : 0;
  if ((rc = clock_nanosleep(sysclock, flags, &req, &rem))) {
    errno = rc;
    rc = -1;
  }
#else
  if (!flags) {
    if (sysclock == CLOCK_REALTIME) {
      rc = nanosleep(&req, &rem);
    } else {
      rc = einval();
    }
  } else {
    struct timespec now;
    if (!(rc = clock_gettime(sysclock, &now))) {
      if (CompareTime(now, req) < 0) {
        req = SubtractTime(req, now);
        rc = nanosleep(&req, &rem);
      } else {
        rc = 0;
      }
    }
  }
#endif
  if (rc == -1 && errno == EINTR) {
    if (CheckInterrupt(m, false)) {
      if (!flags && remaddr) {
        Write64(gtimespec.sec, rem.tv_sec);
        Write64(gtimespec.nsec, rem.tv_nsec);
        CopyToUserWrite(m, remaddr, &gtimespec, sizeof(gtimespec));
      }
    } else {
      if (!flags) {
        req = rem;
      }
      goto TryAgain;
    }
  }
  return rc;
}

static int SigsuspendActual(struct Machine *m, u64 mask) {
  int rc;
  u64 oldmask;
  sigset_t block_host, oldmask_host;
  oldmask = m->sigmask;
  unassert(!sigfillset(&block_host));
  unassert(!pthread_sigmask(SIG_BLOCK, &block_host, &oldmask_host));
  m->sigmask = mask;
  SIG_LOGF("sigmask push %" PRIx64, m->sigmask);
  m->issigsuspend = true;
  NORESTART(rc, sigsuspend(&oldmask_host));
  m->issigsuspend = false;
  unassert(!pthread_sigmask(SIG_SETMASK, &oldmask_host, 0));
  m->sigmask = oldmask;
  SIG_LOGF("sigmask pop %" PRIx64, m->sigmask);
  return rc;
}

static int SigsuspendPolyfill(struct Machine *m, u64 mask) {
  long nanos;
  u64 oldmask;
  struct timespec ts;
  oldmask = m->sigmask;
  m->sigmask = mask;
  nanos = 1;
  while (!CheckInterrupt(m, false)) {
    if (nanos > 256) {
      if (nanos < 10 * 1000) {
#ifdef HAVE_SCHED_YIELD
        sched_yield();
#endif
      } else {
        ts = FromNanoseconds(nanos);
        if (nanosleep(&ts, 0)) {
          unassert(errno == EINTR);
          continue;
        }
      }
    }
    if (nanos < 100 * 1000 * 1000) {
      nanos <<= 1;
    }
  }
  m->sigmask = oldmask;
  return -1;
}

static int SysSigsuspend(struct Machine *m, i64 maskaddr, i64 sigsetsize) {
  u8 word[8];
  if (sigsetsize != 8) return einval();
  if (CopyFromUserRead(m, word, maskaddr, 8) == -1) return -1;
#ifdef __EMSCRIPTEN__
  return SigsuspendPolyfill(m, Read64(word));
#else
  return SigsuspendActual(m, Read64(word));
#endif
}

static int SysSigaltstack(struct Machine *m, i64 newaddr, i64 oldaddr) {
  bool isonstack;
  int supported, unsupported;
  const struct sigaltstack_linux *ss = 0;
  struct sigaltstack_linux converted;
  supported = SS_ONSTACK_LINUX | SS_DISABLE_LINUX | SS_AUTODISARM_LINUX;
  isonstack =
      (~Read32(m->sigaltstack.flags) & SS_DISABLE_LINUX) &&
      Read64(m->sp) >= Read64(m->sigaltstack.sp) &&
      Read64(m->sp) <= Read64(m->sigaltstack.sp) + Read64(m->sigaltstack.size);
  if (newaddr) {
    if (isonstack) {
      LOGF("can't change sigaltstack whilst on sigaltstack");
      return eperm();
    }
    if (!(ss = (const struct sigaltstack_linux *)SchlepR(m, newaddr,
                                                         sizeof(*ss)))) {
      LOGF("couldn't schlep new sigaltstack: %#" PRIx64, newaddr);
      return -1;
    }
    if (m->system->isfreebsd) {
      // FreeBSD sigaltstack: {ss_sp[8], ss_size[8], ss_flags[4], pad[4]}
      // Linux sigaltstack:   {ss_sp[8], ss_flags[4], pad[4], ss_size[8]}
      // Convert FreeBSD layout AND flag values to Linux's.
      // FreeBSD: SS_ONSTACK=0x1, SS_DISABLE=0x4
      // Linux:   SS_ONSTACK=0x1, SS_DISABLE=0x2
      memcpy(converted.sp, ss->sp, 8);           // sp is at same offset
      u64 fbsd_size = Read64(ss->flags);          // FreeBSD size at offset 8
      u32 fbsd_flags = Read32(ss->size);          // FreeBSD flags at offset 16
      u32 lin_flags = 0;
      if (fbsd_flags & 0x1) lin_flags |= SS_ONSTACK_LINUX;
      if (fbsd_flags & 0x4) lin_flags |= SS_DISABLE_LINUX;
      Write32(converted.flags, lin_flags);
      memset(converted.pad1_, 0, 4);
      Write64(converted.size, fbsd_size);
      ss = &converted;
    }
    if ((unsupported = Read32(ss->flags) & ~supported)) {
      LOGF("unsupported %s flags: %#x", "sigaltstack", unsupported);
      return einval();
    }
    if (~Read32(ss->flags) & SS_DISABLE_LINUX) {
      if (Read64(ss->size) < MINSIGSTKSZ_LINUX) {
        LOGF("sigaltstack ss_size=%#" PRIx64 " must be at least %#x",
             Read64(ss->size), MINSIGSTKSZ_LINUX);
        return enomem();
      }
      if (!IsValidMemory(m, Read64(ss->sp), Read64(ss->size),
                         PROT_READ | PROT_WRITE)) {
        LOGF("sigaltstack ss_sp=%#" PRIx64 " ss_size=%#" PRIx64
             " didn't exist with read+write permission",
             Read64(ss->sp), Read64(ss->size));
        return einval();
      }
    }
  }
  if (oldaddr) {
    Write32(m->sigaltstack.flags,
            Read32(m->sigaltstack.flags) & ~SS_ONSTACK_LINUX);
    if (isonstack) {
      Write32(m->sigaltstack.flags,
              Read32(m->sigaltstack.flags) | SS_ONSTACK_LINUX);
    }
    if (m->system->isfreebsd) {
      // Convert Linux layout AND flag values back to FreeBSD's for the guest.
      u8 fbsd_ss[24];
      u32 lin_flags = Read32(m->sigaltstack.flags);
      u32 fbsd_flags = 0;
      if (lin_flags & SS_ONSTACK_LINUX) fbsd_flags |= 0x1;
      if (lin_flags & SS_DISABLE_LINUX) fbsd_flags |= 0x4;
      memcpy(fbsd_ss, m->sigaltstack.sp, 8);              // sp at offset 0
      Write64(fbsd_ss + 8, Read64(m->sigaltstack.size));   // size at offset 8
      Write32(fbsd_ss + 16, fbsd_flags);                   // flags at offset 16
      Write32(fbsd_ss + 20, 0);                            // padding
      CopyToUserWrite(m, oldaddr, fbsd_ss, 24);
    } else {
      CopyToUserWrite(m, oldaddr, &m->sigaltstack, sizeof(m->sigaltstack));
    }
  }
  if (ss) {
    memcpy(&m->sigaltstack, ss, sizeof(*ss));
  }
  return 0;
}

static int SysClockGettime(struct Machine *m, int clock, i64 ts) {
  int rc;
  clock_t sysclock;
  struct timespec htimespec;
  struct timespec_linux gtimespec;
  if (clock == CLOCK_REALTIME_LINUX) {
    sysclock = CLOCK_REALTIME;
  } else if (XlatClock(clock, &sysclock) == -1) {
    return -1;
  }
  if ((rc = clock_gettime(sysclock, &htimespec)) != -1) {
    if (ts) {
      Write64(gtimespec.sec, htimespec.tv_sec);
      Write64(gtimespec.nsec, htimespec.tv_nsec);
      CopyToUserWrite(m, ts, &gtimespec, sizeof(gtimespec));
    }
  }
  return rc;
}

#ifdef HAVE_CLOCK_SETTIME
static int SysClockSettime(struct Machine *m, int clock, i64 ts) {
  clock_t sysclock;
  struct timespec ht;
  const struct timespec_linux *gt;
  if (XlatClock(clock, &sysclock) == -1) return -1;
  if ((gt = (const struct timespec_linux *)SchlepR(m, ts, sizeof(*gt)))) {
    ht.tv_sec = Read64(gt->sec);
    ht.tv_nsec = Read64(gt->nsec);
  }
  return clock_settime(sysclock, &ht);
}
#endif

static int SysClockGetres(struct Machine *m, int clock, i64 ts) {
  int rc;
  clock_t sysclock;
  struct timespec htimespec;
  struct timespec_linux gtimespec;
  if (XlatClock(clock, &sysclock) == -1) return -1;
  if ((rc = clock_getres(sysclock, &htimespec)) != -1) {
    if (ts) {
      Write64(gtimespec.sec, htimespec.tv_sec);
      Write64(gtimespec.nsec, htimespec.tv_nsec);
      CopyToUserWrite(m, ts, &gtimespec, sizeof(gtimespec));
    }
  }
  return rc;
}

static int SysGettimeofday(struct Machine *m, i64 tv, i64 tz) {
  int rc;
  void *htimezonep;
  struct timeval htimeval;
  struct timeval_linux gtimeval;
#ifdef HAVE_STRUCT_TIMEZONE
  struct timezone htimezone;
  struct timezone_linux gtimezone;
  memset(&htimezone, 0, sizeof(htimezone));
  htimezonep = tz ? &htimezone : 0;
#else
  htimezonep = 0;
#endif
  if ((rc = gettimeofday(&htimeval, htimezonep)) != -1) {
    Write64(gtimeval.sec, htimeval.tv_sec);
    Write64(gtimeval.usec, htimeval.tv_usec);
    if (CopyToUserWrite(m, tv, &gtimeval, sizeof(gtimeval)) == -1) {
      return -1;
    }
    // "If tzp is not a null pointer, the behavior is unspecified."
    // ──Quoth the POSIX.1 IEEE Std 1003.1-2017 for gettimeofday().
    if (tz) {
#ifdef HAVE_STRUCT_TIMEZONE
      Write32(gtimezone.minuteswest, htimezone.tz_minuteswest);
      Write32(gtimezone.dsttime, htimezone.tz_dsttime);
      if (CopyToUserWrite(m, tz, &gtimezone, sizeof(gtimezone)) == -1) {
        return -1;
      }
#else
      rc = enotsup();
#endif
    }
  }
  return rc;
}

static i64 SysTime(struct Machine *m, i64 addr) {
  u8 buf[8];
  time_t secs;
  if ((secs = time(0)) == (time_t)-1) return -1;
  if (addr) {
    Write64(buf, secs);
    if (CopyToUserWrite(m, addr, buf, sizeof(buf)) == -1) return -1;
  }
  return secs;
}

static i64 SysTimes(struct Machine *m, i64 bufaddr) {
  // no conversion needed thanks to getauxval(AT_CLKTCK)
  clock_t res;
  struct tms tms;
  struct tms_linux gtms;
  if ((res = times(&tms)) == (clock_t)-1) return -1;
  Write64(gtms.utime, tms.tms_utime);
  Write64(gtms.stime, tms.tms_stime);
  Write64(gtms.cutime, tms.tms_cutime);
  Write64(gtms.cstime, tms.tms_cstime);
  if (CopyToUserWrite(m, bufaddr, &gtms, sizeof(gtms)) == -1) return -1;
  return res;
}

static struct timespec ConvertUtimeTimespec(const struct timespec_linux *tv) {
  struct timespec ts;
  switch (Read64(tv->nsec)) {
    case UTIME_NOW_LINUX:
      ts.tv_sec = 0;
      ts.tv_nsec = UTIME_NOW;
      return ts;
    case UTIME_OMIT_LINUX:
      ts.tv_sec = 0;
      ts.tv_nsec = UTIME_OMIT;
      return ts;
    default:
      ts.tv_sec = Read64(tv->sec);
      ts.tv_nsec = Read64(tv->nsec);
      return ts;
  }
}

static struct timespec ConvertUtimeTimeval(const struct timeval_linux *tv) {
  i64 x;
  struct timespec ts;
  switch ((x = Read64(tv->usec))) {
    case UTIME_NOW_LINUX:
      ts.tv_sec = 0;
      ts.tv_nsec = UTIME_NOW;
      return ts;
    case UTIME_OMIT_LINUX:
      ts.tv_sec = 0;
      ts.tv_nsec = UTIME_OMIT;
      return ts;
    default:
      ts.tv_sec = Read64(tv->sec);
      if (0 <= x && x < 1000000) {
        ts.tv_nsec = x * 1000;
      } else {
        // make sure system call will einval
        // should not overlap with magnums above
        ts.tv_nsec = 1000000666;
      }
      return ts;
  }
}

static void ConvertUtimeTimespecs(struct timespec *ts,
                                  const struct timespec_linux *tv) {
  ts[0] = ConvertUtimeTimespec(tv + 0);
  ts[1] = ConvertUtimeTimespec(tv + 1);
}

static void ConvertUtimeTimevals(struct timespec *ts,
                                 const struct timeval_linux *tv) {
  ts[0] = ConvertUtimeTimeval(tv + 0);
  ts[1] = ConvertUtimeTimeval(tv + 1);
}

static int XlatUtimensatFlags(int x) {
  int res = 0;
  if (x & AT_SYMLINK_FOLLOW_LINUX) {
    x &= ~AT_SYMLINK_FOLLOW_LINUX;  // default behavior
  }
  if (x & AT_SYMLINK_NOFOLLOW_LINUX) {
    res |= AT_SYMLINK_NOFOLLOW;
    x &= ~AT_SYMLINK_NOFOLLOW_LINUX;
  }
  if (x) {
    LOGF("%s() flags %d not supported", "utimensat", x);
    return -1;
  }
  return res;
}

static int SysUtime(struct Machine *m, i64 pathaddr, i64 timesaddr) {
  const char *path;
  struct timespec ts[2];
  const struct utimbuf_linux *t;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  if (!timesaddr) return VfsUtime(AT_FDCWD, path, 0, 0);
  if ((t = (const struct utimbuf_linux *)SchlepR(m, timesaddr, sizeof(*t)))) {
    ts[0].tv_sec = Read64(t->actime);
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = Read64(t->modtime);
    ts[1].tv_nsec = 0;
    return VfsUtime(AT_FDCWD, path, ts, 0);
  } else {
    return -1;
  }
}

static int SysUtimes(struct Machine *m, i64 pathaddr, i64 tvsaddr) {
  const char *path;
  struct timespec ts[2];
  const struct timeval_linux *tv;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  if (!tvsaddr) return VfsUtime(AT_FDCWD, path, 0, 0);
  if ((tv = (const struct timeval_linux *)SchlepR(
           m, tvsaddr, sizeof(struct timeval_linux) * 2))) {
    ConvertUtimeTimevals(ts, tv);
    return VfsUtime(AT_FDCWD, path, ts, 0);
  } else {
    return -1;
  }
}

static int SysFutimesat(struct Machine *m, i32 dirfd, i64 pathaddr,
                        i64 tvsaddr) {
  const char *path;
  struct timespec ts[2];
  const struct timeval_linux *tv;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  if (!tvsaddr) return VfsUtime(GetDirFildes(dirfd), path, 0, 0);
  if ((tv = (const struct timeval_linux *)SchlepR(
           m, tvsaddr, sizeof(struct timeval_linux) * 2))) {
    ConvertUtimeTimevals(ts, tv);
    return VfsUtime(GetDirFildes(dirfd), path, ts, 0);
  } else {
    return -1;
  }
}

static int SysUtimensat(struct Machine *m, i32 fd, i64 pathaddr, i64 tvsaddr,
                        i32 flags) {
  const char *path;
  struct timespec ts[2], *tsp;
  const struct timespec_linux *tv;
  if (m->system->isfreebsd) flags = XlatFreeBSDAtFlags(flags);
  if (!pathaddr) {
    path = 0;
  } else if (!(path = LoadStr(m, pathaddr))) {
    return -1;
  }
  if (tvsaddr) {
    if ((tv = (const struct timespec_linux *)SchlepR(
             m, tvsaddr, sizeof(struct timespec_linux) * 2))) {
      ConvertUtimeTimespecs(ts, tv);
      tsp = ts;
    } else {
      return -1;
    }
  } else {
    tsp = 0;
  }
  if ((flags = XlatUtimensatFlags(flags)) == -1) return -1;
  if (path) {
    return VfsUtime(GetDirFildes(fd), path, tsp, flags);
  } else {
    if (flags) {
      LOGF("%s() flags %d not supported", "utimensat(path=null)", flags);
      return einval();
    }
    return VfsFutime(fd, tsp);
  }
}

static int LoadFdSet(struct Machine *m, int nfds, fd_set *fds, i64 addr) {
  u64 w;
  int fd;
  unsigned o;
  const u64 *p;
  if ((p = (const u64 *)SchlepRW(m, addr, FD_SETSIZE_LINUX / 8))) {
    FD_ZERO(fds);
    for (fd = 0; fd < nfds; fd += 64) {
      w = p[fd >> 6];
      while (w) {
        o = bsr(w);
        w &= ~((u64)1 << o);
        if (fd + o < nfds) {
          FD_SET(fd + o, fds);
        }
      }
    }
    return 0;
  } else {
    return -1;
  }
}

static int SaveFdSet(struct Machine *m, int nfds, const fd_set *fds, i64 addr) {
  int fd;
  u8 p[FD_SETSIZE_LINUX / 8] = {0};
  for (fd = 0; fd < nfds; ++fd) {
    if (FD_ISSET(fd, fds)) {
      p[fd >> 3] |= 1 << (fd & 7);
    }
  }
  return CopyToUserWrite(m, addr, p, FD_SETSIZE_LINUX / 8);
}

static i32 Select(struct Machine *m,          //
                  i32 nfds,                   //
                  i64 readfds_addr,           //
                  i64 writefds_addr,          //
                  i64 exceptfds_addr,         //
                  struct timespec *timeoutp,  //
                  const u64 *sigmaskp_guest) {
  int fildes, rc;
  i32 setsize;
  u64 oldmask_guest = 0;
  fd_set readfds, writefds, exceptfds, readyreadfds, readywritefds,
      readyexceptfds;
  struct pollfd hfds[1];
  struct timespec now, wait, remain, deadline = {0};
  struct Fd *fd;
  int (*poll_impl)(struct pollfd *, nfds_t, int);
  if (timeoutp) {
    deadline = AddTime(GetTime(), *timeoutp);
  }
  setsize = MIN(FD_SETSIZE, FD_SETSIZE_LINUX);
  if (nfds < 0 || nfds > setsize) {
    LOGF("select() nfds=%d can't exceed %d on this platform", nfds, setsize);
    return einval();
  }
  if (readfds_addr) {
    if (LoadFdSet(m, nfds, &readfds, readfds_addr) == -1) {
      return -1;
    }
  } else {
    FD_ZERO(&readfds);
  }
  if (writefds_addr) {
    if (LoadFdSet(m, nfds, &writefds, writefds_addr) == -1) {
      return -1;
    }
  } else {
    FD_ZERO(&writefds);
  }
  if (exceptfds_addr) {
    if (LoadFdSet(m, nfds, &exceptfds, exceptfds_addr) == -1) {
      return -1;
    }
  } else {
    FD_ZERO(&exceptfds);
  }
  FD_ZERO(&readyreadfds);
  FD_ZERO(&readywritefds);
  FD_ZERO(&readyexceptfds);
  if (sigmaskp_guest) {
    oldmask_guest = m->sigmask;
    m->sigmask = *sigmaskp_guest;
    SIG_LOGF("sigmask push %" PRIx64, m->sigmask);
  }
  for (;;) {
    if (CheckInterrupt(m, false)) {
      rc = eintr();
      break;
    }
    rc = 0;
    for (fildes = 0; fildes < nfds; ++fildes) {
      if (!FD_ISSET(fildes, &readfds) && !FD_ISSET(fildes, &writefds) &&
          !FD_ISSET(fildes, &exceptfds)) {
        continue;
      }
    TryAgain:
      if (CheckInterrupt(m, false)) {
        rc = eintr();
        break;
      }
      LOCK(&m->system->fds.lock);
      if ((fd = GetFd(&m->system->fds, fildes))) {
        unassert(fd->cb);
        unassert(poll_impl = fd->cb->poll);
      } else {
        poll_impl = 0;
      }
      UNLOCK(&m->system->fds.lock);
      if (fd) {
        hfds[0].fd = fildes;
        hfds[0].events = ((FD_ISSET(fildes, &readfds) ? POLLIN : 0) |
                          (FD_ISSET(fildes, &writefds) ? POLLOUT : 0) |
                          (FD_ISSET(fildes, &exceptfds) ? POLLPRI : 0));
        switch (poll_impl(hfds, 1, 0)) {
          case 0:
            break;
          case 1:
            if (FD_ISSET(fildes, &readfds) && (hfds[0].revents & POLLIN)) {
              ++rc;
              FD_SET(fildes, &readyreadfds);
              FD_CLR(fildes, &readfds);
            }
            if (FD_ISSET(fildes, &writefds) && (hfds[0].revents & POLLOUT)) {
              ++rc;
              FD_SET(fildes, &readywritefds);
              FD_CLR(fildes, &writefds);
            }
            if (FD_ISSET(fildes, &exceptfds) && (hfds[0].revents & POLLPRI)) {
              ++rc;
              FD_SET(fildes, &readyexceptfds);
              FD_CLR(fildes, &exceptfds);
            }
            break;
          case -1:
            if (errno == EINTR) {
              goto TryAgain;
            }
            rc = -1;
            goto BreakLoop;
        }
      } else {
        rc = ebadf();
        break;
      }
    }
  BreakLoop:
    if (rc || (timeoutp && CompareTime(now = GetTime(), deadline) >= 0)) {
      break;
    }
    if (timeoutp) {
      wait = FromMilliseconds(kPollingMs);
      remain = SubtractTime(deadline, now);
      if (CompareTime(remain, wait) < 0) {
        wait = remain;
      }
    } else {
      wait = FromMilliseconds(kPollingMs);
    }
    nanosleep(&wait, 0);
  }
  if (sigmaskp_guest) {
    m->sigmask = oldmask_guest;
    SIG_LOGF("sigmask pop %" PRIx64, m->sigmask);
  }
  if (rc != -1) {
    if ((readfds_addr &&
         SaveFdSet(m, nfds, &readyreadfds, readfds_addr) == -1) ||
        (writefds_addr &&
         SaveFdSet(m, nfds, &readywritefds, writefds_addr) == -1) ||
        (exceptfds_addr &&
         SaveFdSet(m, nfds, &readyexceptfds, exceptfds_addr) == -1)) {
      return -1;
    }
  }
#ifndef DISABLE_NONPOSIX
  if (timeoutp) {
    now = GetTime();
    if (CompareTime(now, deadline) < 0) {
      *timeoutp = SubtractTime(deadline, now);
    } else {
      *timeoutp = GetZeroTime();
    }
  }
#endif
  return rc;
}

static i32 SysSelect(struct Machine *m, i32 nfds, i64 readfds_addr,
                     i64 writefds_addr, i64 exceptfds_addr, i64 timeout_addr) {
  i32 rc;
  struct timespec timeout, *timeoutp;
#ifndef DISABLE_NONPOSIX
  struct timeval_linux timeout_linux;
#endif
  const struct timeval_linux *timeoutp_linux;
  if (timeout_addr) {
    if ((timeoutp_linux = (const struct timeval_linux *)SchlepRW(
             m, timeout_addr, sizeof(*timeoutp_linux)))) {
      timeout.tv_sec = Read64(timeoutp_linux->sec);
      timeout.tv_nsec = Read64(timeoutp_linux->usec);
      if (0 <= timeout.tv_sec &&
          (0 <= timeout.tv_nsec && timeout.tv_nsec < 1000000)) {
        timeout.tv_nsec *= 1000;
        timeoutp = &timeout;
      } else {
        return einval();
      }
    } else {
      return -1;
    }
  } else {
    timeoutp = 0;
    memset(&timeout, 0, sizeof(timeout));
  }
  rc =
      Select(m, nfds, readfds_addr, writefds_addr, exceptfds_addr, timeoutp, 0);
#ifndef DISABLE_NONPOSIX
  if (timeout_addr) {
    Write64(timeout_linux.sec, timeout.tv_sec);
    Write64(timeout_linux.usec, (timeout.tv_nsec + 999) / 1000);
    CopyToUserWrite(m, timeout_addr, &timeout_linux, sizeof(timeout_linux));
  }
#endif
  return rc;
}

static i32 SysPselect(struct Machine *m, i32 nfds, i64 readfds_addr,
                      i64 writefds_addr, i64 exceptfds_addr, i64 timeout_addr,
                      i64 pselect6_addr) {
  i32 rc;
  u64 sigmask, *sigmaskp;
  const struct sigset_linux *sm;
  const struct pselect6_linux *ps;
  struct timespec timeout, *timeoutp;
#ifndef DISABLE_NONPOSIX
  struct timespec_linux timeout_linux;
#endif
  if (timeout_addr) {
    if (LoadTimespecRW(m, timeout_addr, &timeout) == -1) return -1;
    timeoutp = &timeout;
  } else {
    timeoutp = 0;
    memset(&timeout, 0, sizeof(timeout));
  }
  if (pselect6_addr) {
    if ((ps = (const struct pselect6_linux *)SchlepR(m, pselect6_addr,
                                                     sizeof(*ps)))) {
      if (Read64(ps->sigmaskaddr)) {
        if (Read64(ps->sigmasksize) == 8) {
          if ((sm = (const struct sigset_linux *)SchlepR(
                   m, Read64(ps->sigmaskaddr), sizeof(*sm)))) {
            sigmask = Read64(sm->sigmask);
            sigmaskp = &sigmask;
          } else {
            return -1;
          }
        } else {
          return einval();
        }
      } else {
        sigmaskp = 0;
      }
    } else {
      return -1;
    }
  } else {
    sigmaskp = 0;
  }
  rc = Select(m, nfds, readfds_addr, writefds_addr, exceptfds_addr, timeoutp,
              sigmaskp);
#ifndef DISABLE_NONPOSIX
  if (timeout_addr) {
    Write64(timeout_linux.sec, timeout.tv_sec);
    Write64(timeout_linux.nsec, timeout.tv_nsec);
    CopyToUserWrite(m, timeout_addr, &timeout_linux, sizeof(timeout_linux));
  }
#endif
  return rc;
}

static int Poll(struct Machine *m, i64 fdsaddr, u64 nfds,
                struct timespec deadline) {
  long i;
  u64 gfdssize;
  struct Fd *fd;
  int fildes, rc, ev;
  struct pollfd hfds[1];
  struct pollfd_linux *gfds;
  struct timespec now, wait, remain;
  int (*poll_impl)(struct pollfd *, nfds_t, int);
  if (!ckd_mul(&gfdssize, nfds, sizeof(struct pollfd_linux)) &&
      gfdssize <= 0x7ffff000) {
    if ((gfds = (struct pollfd_linux *)AddToFreeList(m, malloc(gfdssize)))) {
      rc = 0;
      CopyFromUserRead(m, gfds, fdsaddr, gfdssize);
      for (;;) {
        for (i = 0; i < nfds; ++i) {
        TryAgain:
          if (CheckInterrupt(m, false)) {
            rc = eintr();
            break;
          }
          fildes = Read32(gfds[i].fd);
          LOCK(&m->system->fds.lock);
          if ((fd = GetFd(&m->system->fds, fildes))) {
            unassert(fd->cb);
            unassert(poll_impl = fd->cb->poll);
          } else {
            poll_impl = 0;
          }
          UNLOCK(&m->system->fds.lock);
          if (fd) {
            hfds[0].fd = fildes;
            ev = Read16(gfds[i].events);
            hfds[0].events = (((ev & POLLIN_LINUX) ? POLLIN : 0) |
                              ((ev & POLLOUT_LINUX) ? POLLOUT : 0) |
                              ((ev & POLLPRI_LINUX) ? POLLPRI : 0) |
                              ((ev & 0x0040) ? POLLRDNORM : 0) |
                              ((ev & 0x0080) ? POLLRDBAND : 0));
            switch (poll_impl(hfds, 1, 0)) {
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
                if (hfds[0].revents & POLLRDNORM) ev |= 0x0040;
                if (hfds[0].revents & POLLRDBAND) ev |= 0x0080;
                if (!ev) ev |= POLLERR_LINUX;
                Write16(gfds[i].revents, ev);
                break;
              case -1:
                if (errno == EINTR) {
                  goto TryAgain;
                }
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
        if (rc || CompareTime((now = GetTime()), deadline) >= 0) {
          break;
        }
        wait = FromMilliseconds(kPollingMs);
        remain = SubtractTime(deadline, now);
        if (CompareTime(remain, wait) < 0) {
          wait = remain;
        }
        nanosleep(&wait, 0);
      }
      if (rc != -1) {
        CopyToUserWrite(m, fdsaddr, gfds, nfds * sizeof(*gfds));
      }
    } else {
      rc = enomem();
    }
    return rc;
  } else {
    return einval();
  }
}

static int SysPoll(struct Machine *m, i64 fdsaddr, u64 nfds, i32 timeout_ms) {
  struct timespec deadline;
  if (timeout_ms < 0) {
    deadline = GetMaxTime();
  } else {
    deadline = AddTime(GetTime(), FromMilliseconds(timeout_ms));
  }
  return Poll(m, fdsaddr, nfds, deadline);
}

static int SysPpoll(struct Machine *m, i64 fdsaddr, u64 nfds, i64 timeoutaddr,
                    i64 sigmaskaddr, u64 sigsetsize) {
  int rc;
  u64 oldmask = 0;
  const struct sigset_linux *sm;
  struct timespec_linux timeout_linux;
  struct timespec now, timeout, remain, deadline;
  if (sigmaskaddr) {
    if (sigsetsize != 8) return einval();
    if ((sm = (const struct sigset_linux *)SchlepR(m, sigmaskaddr,
                                                   sizeof(*sm)))) {
      oldmask = m->sigmask;
      m->sigmask = Read64(sm->sigmask);
      SIG_LOGF("sigmask push %" PRIx64, m->sigmask);
    } else {
      return -1;
    }
  }
  if (!CheckInterrupt(m, false)) {
    if (timeoutaddr) {
      if (LoadTimespecRW(m, timeoutaddr, &timeout) == -1) return -1;
      deadline = AddTime(GetTime(), timeout);
      rc = Poll(m, fdsaddr, nfds, deadline);
      now = GetTime();
      if (CompareTime(now, deadline) >= 0) {
        remain = FromMilliseconds(0);
      } else {
        remain = SubtractTime(deadline, now);
      }
      Write64(timeout_linux.sec, remain.tv_sec);
      Write64(timeout_linux.nsec, remain.tv_nsec);
      CopyToUserWrite(m, timeoutaddr, &timeout_linux, sizeof(timeout_linux));
    } else {
      rc = Poll(m, fdsaddr, nfds, GetMaxTime());
    }
  } else {
    rc = -1;
  }
  if (sigmaskaddr) {
    m->sigmask = oldmask;
    SIG_LOGF("sigmask pop %" PRIx64, m->sigmask);
  }
  return rc;
}

static int SysSigprocmask(struct Machine *m, int how, i64 setaddr,
                          i64 oldsetaddr, u64 sigsetsize) {
  u64 set;
  u8 word[8];
  sigset_t ss;
  const u8 *neu;
  int sig, delivered;
  if (sigsetsize != 8) {
    return einval();
  }
  if (how != SIG_BLOCK_LINUX &&    //
      how != SIG_UNBLOCK_LINUX &&  //
      how != SIG_SETMASK_LINUX) {
    return einval();
  }
  if (setaddr) {
    if (!(neu = (const u8 *)SchlepR(m, setaddr, 8))) {
      return -1;
    }
  } else {
    neu = 0;
  }
  if (oldsetaddr) {
    SIG_LOGF("sigmask read %" PRIx64, m->sigmask);
    Write64(word, m->sigmask);
    if (CopyToUserWrite(m, oldsetaddr, word, 8) == -1) {
      return -1;
    }
  }
  if (setaddr) {
    set = Read64(neu);
    if (how == SIG_BLOCK_LINUX) {
      m->sigmask |= set;
    } else if (how == SIG_UNBLOCK_LINUX) {
      m->sigmask &= ~set;
    } else if (how == SIG_SETMASK_LINUX) {
      m->sigmask = set;
    } else {
      __builtin_unreachable();
    }
    XlatLinuxToSigset(&ss, m->sigmask & ((u64)1 << (SIGTSTP_LINUX - 1) |
                                         (u64)1 << (SIGTTIN_LINUX - 1) |
                                         (u64)1 << (SIGTTOU_LINUX - 1)));
    sigprocmask(SIG_BLOCK, &ss, 0);
  }
  Put64(m->ax, 0);
  do {
    if ((sig = ConsumeSignal(m, &delivered, 0))) {
      TerminateSignal(m, sig, 0);
    }
  } while (delivered && DeliverSignalRecursively(m, delivered));
  return 0;
}

static int SysSigpending(struct Machine *m, i64 setaddr) {
  u8 word[8];
  Write64(word, m->signals);
  return CopyToUserWrite(m, setaddr, word, 8);
}

static int SysKill(struct Machine *m, int pid, int sig) {
  return kill(pid, sig ? XlatSignal(sig) : 0);
}

static bool IsValidThreadId(struct System *s, int tid) {
  return tid == s->pid ||
         (kMinThreadId <= tid && tid < kMinThreadId + kMaxThreadIds);
}

static int SysTkill(struct Machine *m, int tid, int sig) {
#if defined(HAVE_FORK) || defined(HAVE_THREADS)
  bool found;
  int rc, err;
  if (tid < 0) return einval();
  if (!(0 <= sig && sig <= 64)) {
    LOGF("tkill(%d, %d) failed due to bogus signal", tid, sig);
    return einval();
  }
  // trigger signal immediately if possible
  if (tid == m->tid) {
    if (sig == SIGSTOP_LINUX || sig == SIGKILL_LINUX) {
      return raise(XlatSignal(sig));
    } else if (~m->sigmask & ((u64)1 << (sig - 1))) {
      LOCK(&m->system->sig_lock);
      switch (Read64(m->system->hands[sig - 1].handler)) {
        case SIG_DFL_LINUX:
          if (!IsSignalIgnoredByDefault(sig)) {
            UNLOCK(&m->system->sig_lock);
            // If a FreeBSD child thread sends itself SIGABRT (from
            // libthr's abort() during thread exit cleanup), just exit
            // the thread instead of killing the entire process.
            if (m->nojit && sig == SIGABRT_LINUX) {
              WakeAllFutexes();
              SysExit(m, 0);
            }
            TerminateSignal(m, sig, 0);
            return 0;
          }
          // fallthrough
        case SIG_IGN_LINUX:
          rc = 0;
          break;
        default:
          Put64(m->ax, 0);
          m->interrupted = true;
          DeliverSignal(m, sig, SI_TKILL_LINUX);
          rc = -1;
          break;
      }
      UNLOCK(&m->system->sig_lock);
      return rc;
    } else {
      m->signals |= (u64)1 << (sig - 1);
      return 0;
    }
  }
  if (!IsValidThreadId(m->system, tid)) {
    LOGF("tkill(%d, %d) failed due to bogus thread id", tid, sig);
    return esrch();
  }
  err = 0;
  found = 0;
#ifndef DISABLE_THREADS
  {
    struct Dll *e;
    LOCK(&m->system->machines_lock);
    for (e = dll_first(m->system->machines); e;
         e = dll_next(m->system->machines, e)) {
      struct Machine *m2;
      m2 = MACHINE_CONTAINER(e);
      if (m2->tid == tid) {
        if (sig) {
          EnqueueSignal(m2, sig);
          err = pthread_kill(m2->thread, SIGSYS);
        } else {
          err = pthread_kill(m2->thread, 0);
        }
        found = true;
        break;
      }
    }
    UNLOCK(&m->system->machines_lock);
  }
#endif
  if (!found) {
    return SysKill(m, tid, sig);
  }
  if (!err) {
    return 0;
  } else {
    errno = err;
    return -1;
  }
#else
  return SysKill(m, tid, sig);
#endif /* HAVE_THREADS */
}

static int SysTgkill(struct Machine *m, int pid, int tid, int sig) {
  if (pid < 1 || tid < 1) return einval();
  if (pid != m->system->pid) return eperm();
#ifdef HAVE_THREADS
  return SysTkill(m, tid, sig);
#else
  if (tid != pid) return esrch();
  return SysKill(m, tid, sig);
#endif
}

static int SysPause(struct Machine *m) {
  int rc;
  NORESTART(rc, pause());
  return rc;
}

static int SysSetsid(struct Machine *m) {
  return setsid();
}

static i32 SysGetsid(struct Machine *m, i32 pid) {
  return getsid(pid);
}

static int SysGetpid(struct Machine *m) {
  return m->system->pid;
}

static int SysGettid(struct Machine *m) {
  return m->tid;
}

static int SysGetppid(struct Machine *m) {
  return getppid();
}

static int SysGetuid(struct Machine *m) {
  if (m->system->emulate_root) return 0;
  return getuid();
}

static int SysGetgid(struct Machine *m) {
  if (m->system->emulate_root) return 0;
  return getgid();
}

static int SysGeteuid(struct Machine *m) {
  if (m->system->emulate_root) return 0;
  return geteuid();
}

static int SysGetegid(struct Machine *m) {
  if (m->system->emulate_root) return 0;
  return getegid();
}

static i32 SysGetgroups(struct Machine *m, i32 size, i64 addr) {
  gid_t *group;
  u8 i32buf[4];
  int i, ngroups;
  long ngroups_max;
  if (!size) {
    return getgroups(0, 0);
  } else {
    // POSIX.1 recommends adding 1 to ngroups_max but the Linux manual
    // says this can result in EINVAL. Apple M1 says NGROUPS_MAX is 16
    // even though it usually returns 18 groups or more...
#ifdef __APPLE__
    ngroups_max = size;
#else
    ngroups_max = sysconf(_SC_NGROUPS_MAX);
#endif
    size = MIN(size, ngroups_max);
    if (!IsValidMemory(m, addr, (size_t)size * 4, PROT_WRITE)) return -1;
    if (!(group = (gid_t *)AddToFreeList(m, malloc(size * sizeof(gid_t))))) {
      return -1;
    }
    if ((ngroups = getgroups(size, group)) != -1) {
      for (i = 0; i < ngroups; ++i) {
        Write32(i32buf, group[i]);
        CopyToUserWrite(m, addr + (size_t)i * 4, i32buf, 4);
      }
    }
    return ngroups;
  }
}

static i32 SysSetgroups(struct Machine *m, i32 size, i64 addr) {
  if (m->system->emulate_root) return 0;
#ifdef HAVE_SETGROUPS
  int i;
  gid_t *group;
  const u8 *group_linux;
  if (!(group_linux = (const u8 *)SchlepR(m, addr, (size_t)size * 4)) ||
      !(group = (gid_t *)AddToFreeList(m, malloc(size * sizeof(gid_t))))) {
    return -1;
  }
  for (i = 0; i < size; ++i) {
    group[i] = Read32(group_linux + (size_t)i * 4);
  }
  return setgroups(size, group);
#else
  return enosys();
#endif
}

static i32 SysSetresuid(struct Machine *m,  //
                        u32 real,           //
                        u32 effective,      //
                        u32 saved) {
  if (m->system->emulate_root) return 0;
#ifdef HAVE_SETRESUID
  return setresuid(real, effective, saved);
#elif defined(HAVE_SETREUID)
  // we're going to assume "saved uids" don't exist if the platform
  // doesn't provide the api for changing them. this lets us ignore
  // complexity regarding how setruid() vs. setresuid() impact uids
  return setreuid(real, effective);
#else
  if (real == -1 && effective == -1) return 0;
  if (real == -1) return seteuid(effective);
  if (real != effective) return enosys();
  return setuid(real);
#endif
}

static i32 SysSetresgid(struct Machine *m,  //
                        u32 real,           //
                        u32 effective,      //
                        u32 saved) {
  if (m->system->emulate_root) return 0;
#ifdef HAVE_SETRESGID
  return setresgid(real, effective, saved);
#elif defined(HAVE_SETREGID)
  // we're going to assume "saved gids" don't exist if the platform
  // doesn't provide the api for changing them. this lets us ignore
  // complexity regarding how setrgid() vs. setresgid() impact gids
  return setregid(real, effective);
#else
  if (real == -1 && effective == -1) return 0;
  if (real == -1) return setegid(effective);
  if (real != effective) return enosys();
  return setgid(real);
#endif
}

static int SysSetreuid(struct Machine *m, u32 real, u32 effective) {
#ifdef HAVE_SETRESUID
  // If the real user ID is set (i.e., ruid is not -1) or the effective
  // user ID is set to a value not equal to the previous real user ID,
  // the saved set-user-ID will be set to the new effective user ID.
  // ──Quoth the Linux Programmer's Manual § setreuid()
  if (real != -1 || (effective != -1 && effective != getuid())) {
    if (effective == -1) effective = geteuid();
    return setresuid(real, effective, effective);
  } else {
    return setresuid(real, effective, -1);
  }
#else
  return SysSetresuid(m, real, effective, -1);
#endif
}

static int SysSetregid(struct Machine *m, u32 real, u32 effective) {
#ifdef HAVE_SETRESUID
  if (real != -1 || (effective != -1 && effective != getgid())) {
    if (effective == -1) effective = getegid();
    return setresgid(real, effective, effective);
  } else {
    return setresgid(real, effective, -1);
  }
#else
  return SysSetresgid(m, real, effective, -1);
#endif
}

static i32 SysGetresuid(struct Machine *m,  //
                        i64 realaddr,       //
                        i64 effectiveaddr,  //
                        i64 savedaddr) {
  u8 *real = 0;
  u8 *saved = 0;
  u8 *effective = 0;
  uid_t uid, euid, suid;
  if ((realaddr && !(real = (u8 *)SchlepW(m, realaddr, 4))) ||
      (savedaddr && !(saved = (u8 *)SchlepW(m, savedaddr, 4))) ||
      (effectiveaddr && !(effective = (u8 *)SchlepW(m, effectiveaddr, 4)))) {
    return -1;
  }
#ifdef HAVE_SETRESUID
  if (getresuid(&uid, &euid, &suid) == -1) return -1;
#else
  uid = getuid();
  euid = geteuid();
  suid = euid;
#endif
  if (real) Write32(real, uid);
  if (saved) Write32(saved, suid);
  if (effective) Write32(effective, euid);
  return 0;
}

static i32 SysGetresgid(struct Machine *m,  //
                        i64 realaddr,       //
                        i64 effectiveaddr,  //
                        i64 savedaddr) {
  u8 *real = 0;
  u8 *saved = 0;
  u8 *effective = 0;
  gid_t gid, egid, sgid;
  if ((realaddr && !(real = (u8 *)SchlepW(m, realaddr, 4))) ||
      (savedaddr && !(saved = (u8 *)SchlepW(m, savedaddr, 4))) ||
      (effectiveaddr && !(effective = (u8 *)SchlepW(m, effectiveaddr, 4)))) {
    return -1;
  }
#ifdef HAVE_SETRESUID
  if (getresgid(&gid, &egid, &sgid) == -1) return -1;
#else
  gid = getgid();
  egid = getegid();
  sgid = egid;
#endif
  if (real) Write32(real, gid);
  if (saved) Write32(saved, sgid);
  if (effective) Write32(effective, egid);
  return 0;
}

static int SysSchedYield(struct Machine *m) {
#ifdef HAVE_SCHED_YIELD
  return sched_yield();
#else
  return 0;
#endif
}

static int SysUmask(struct Machine *m, int mask) {
  return umask(mask);
}

static int SysSetuid(struct Machine *m, int uid) {
  if (m->system->emulate_root) return 0;
  return setuid(uid);
}

static int SysSetgid(struct Machine *m, int gid) {
  if (m->system->emulate_root) return 0;
  return setgid(gid);
}

static int SysGetpgid(struct Machine *m, int pid) {
  return getpgid(pid);
}

static int SysGetpgrp(struct Machine *m) {
  return getpgid(0);
}

static int SysAlarm(struct Machine *m, unsigned seconds) {
  return alarm(seconds);
}

static int SysSetpgid(struct Machine *m, int pid, int gid) {
  return setpgid(pid, gid);
}

static int SysCreat(struct Machine *m, i64 path, int mode) {
  return SysOpenat(m, AT_FDCWD_LINUX, path,
                   O_WRONLY_LINUX | O_CREAT_LINUX | O_TRUNC_LINUX, mode);
}

static int SysAccess(struct Machine *m, i64 path, int mode) {
  return SysFaccessat(m, AT_FDCWD_LINUX, path, mode);
}

static int SysFreeBSDEaccess(struct Machine *m, i64 path, int mode) {
  return SysFaccessat2(m, AT_FDCWD_LINUX, path, mode, AT_EACCESS_LINUX);
}

static int SysStat(struct Machine *m, i64 path, i64 st) {
  return SysFstatat(m, AT_FDCWD_LINUX, path, st, 0);
}

static int SysLstat(struct Machine *m, i64 path, i64 st) {
  return SysFstatat(m, AT_FDCWD_LINUX, path, st, AT_SYMLINK_NOFOLLOW_LINUX);
}

static int SysOpen(struct Machine *m, i64 path, int flags, int mode) {
  return SysOpenat(m, AT_FDCWD_LINUX, path, flags, mode);
}

static int SysAccept(struct Machine *m, int fd, i64 sa, i64 sas) {
  return SysAccept4(m, fd, sa, sas, 0);
}

static int SysSchedSetparam(struct Machine *m, int pid, i64 paramaddr) {
  if (pid < 0 || !paramaddr) return einval();
  return 0;
}

static int SysSchedGetparam(struct Machine *m, int pid, i64 paramaddr) {
  u8 param[8];
  if (pid < 0 || !paramaddr) return einval();
  Write32(param, 0);
  CopyToUserWrite(m, paramaddr, param, 8);
  return 0;
}

static int SysSchedSetscheduler(struct Machine *m, int pid, int policy,
                                i64 paramaddr) {
  if (pid < 0 || !paramaddr) return einval();
  return 0;
}

static int SysSchedGetscheduler(struct Machine *m, int pid) {
  if (pid < 0) return einval();
  return SCHED_OTHER_LINUX;
}

static int SysSchedGetPriorityMax(struct Machine *m, int policy) {
  return 0;
}

static int SysSchedGetPriorityMin(struct Machine *m, int policy) {
  return 0;
}

static int SysPipe(struct Machine *m, i64 pipefds_addr) {
  return SysPipe2(m, pipefds_addr, 0);
}

static i32 SysEventfd2(struct Machine *m, u32 initval, i32 flags) {
  int lim, fildes, oflags, sysflags;
  oflags = O_RDWR;
  sysflags = 0;
  if (flags & EFD_SEMAPHORE) {
    sysflags |= EFD_SEMAPHORE;
    flags &= ~1;  // EFD_SEMAPHORE_LINUX = 1
  }
  if (flags & O_NDELAY_LINUX) {
    oflags |= O_CLOEXEC;  // track nonblock
    sysflags |= EFD_NONBLOCK;
    flags &= ~O_NDELAY_LINUX;
  }
  if (flags & O_CLOEXEC_LINUX) {
    oflags |= O_CLOEXEC;
    sysflags |= EFD_CLOEXEC;
    flags &= ~O_CLOEXEC_LINUX;
  }
  if (flags) {
    LOGF("unsupported %s flags: %#x", "eventfd2", flags);
    return einval();
  }
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  if ((fildes = eventfd(initval, sysflags)) != -1) {
    if (fildes >= lim) {
      close(fildes);
      fildes = emfile();
    } else {
      LOCK(&m->system->fds.lock);
      unassert(AddFd(&m->system->fds, fildes, oflags));
      UNLOCK(&m->system->fds.lock);
    }
  }
  return fildes;
}

static i32 SysMemfdCreate(struct Machine *m, i64 nameaddr, u32 flags) {
#ifdef __linux__
  int lim, fildes, oflags;
  const char *name;
  if (!(name = LoadStr(m, nameaddr))) return -1;
  oflags = O_RDWR;
  if (flags & 0x0001) oflags |= O_CLOEXEC;  // MFD_CLOEXEC_LINUX
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  if ((fildes = memfd_create(name, flags)) != -1) {
    if (fildes >= lim) {
      close(fildes);
      fildes = emfile();
    } else {
      LOCK(&m->system->fds.lock);
      unassert(AddFd(&m->system->fds, fildes, oflags));
      UNLOCK(&m->system->fds.lock);
    }
  }
  return fildes;
#else
  return enosys();
#endif
}

static int SysStatxImpl(struct Machine *m, i32 dirfd, const char *path,
                        i32 flags, u32 mask, i64 staddr) {
  int rc, sysflags;
  struct stat st;
  struct statx_linux gst;
  (void)mask;
  sysflags = 0;
  if (flags & AT_SYMLINK_NOFOLLOW_LINUX) {
    sysflags |= AT_SYMLINK_NOFOLLOW;
    flags &= ~AT_SYMLINK_NOFOLLOW_LINUX;
  }
#ifndef DISABLE_NONPOSIX
  if (flags & AT_EMPTY_PATH_LINUX) {
    flags &= ~AT_EMPTY_PATH_LINUX;
    if (path && !*path) {
      if ((rc = VfsFstat(dirfd, &st)) == -1) return -1;
      goto have_stat;
    }
  }
#endif
  if (flags & AT_NO_AUTOMOUNT_LINUX) flags &= ~AT_NO_AUTOMOUNT_LINUX;
  flags &= ~0x6000;  // AT_STATX_SYNC_TYPE bits — we ignore sync hints
  if (flags) {
    LOGF("%s() flags %d not supported", "statx", flags);
    return einval();
  }
  if ((rc = VfsStat(GetDirFildes(dirfd), path, &st, sysflags)) == -1) return -1;
have_stat:
  memset(&gst, 0, sizeof(gst));
  Write32(gst.mask, STATX_BASIC_STATS_LINUX);
  Write32(gst.blksize, st.st_blksize);
  Write32(gst.nlink, st.st_nlink);
  Write32(gst.uid, st.st_uid);
  Write32(gst.gid, st.st_gid);
  Write16(gst.mode, st.st_mode);
  Write64(gst.ino, st.st_ino);
  Write64(gst.size, st.st_size);
  Write64(gst.blocks, st.st_blocks);
  Write64(gst.atime.sec, st.st_atim.tv_sec);
  Write32(gst.atime.nsec, st.st_atim.tv_nsec);
  Write64(gst.mtime.sec, st.st_mtim.tv_sec);
  Write32(gst.mtime.nsec, st.st_mtim.tv_nsec);
  Write64(gst.ctime.sec, st.st_ctim.tv_sec);
  Write32(gst.ctime.nsec, st.st_ctim.tv_nsec);
  Write32(gst.rdev_major, (u32)((st.st_rdev >> 8) & 0xfff) |
                              ((u32)(st.st_rdev >> 32) & ~0xfffu));
  Write32(gst.rdev_minor, (u32)(st.st_rdev & 0xff) |
                              ((u32)(st.st_rdev >> 12) & ~0xffu));
  Write32(gst.dev_major, (u32)((st.st_dev >> 8) & 0xfff) |
                             ((u32)(st.st_dev >> 32) & ~0xfffu));
  Write32(gst.dev_minor, (u32)(st.st_dev & 0xff) |
                             ((u32)(st.st_dev >> 12) & ~0xffu));
  if (CopyToUserWrite(m, staddr, &gst, sizeof(gst)) == -1) return -1;
  return 0;
}

static int SysStatx(struct Machine *m, i32 dirfd, i64 pathaddr, i32 flags,
                    u32 mask, i64 staddr) {
  const char *path;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  return SysStatxImpl(m, dirfd, path, flags, mask, staddr);
}

#ifdef __linux__
static int FinishHostFd(struct Machine *m, int hostfd, int oflags) {
  int lim;
  if (hostfd == -1) return -1;
  if (!(lim = GetFileDescriptorLimit(m->system))) {
    close(hostfd);
    return emfile();
  }
  if (hostfd >= lim) {
    close(hostfd);
    return emfile();
  }
  LOCK(&m->system->fds.lock);
  unassert(AddFd(&m->system->fds, hostfd, oflags));
  UNLOCK(&m->system->fds.lock);
  return hostfd;
}
#endif

static i32 SysSignalfd4(struct Machine *m, i32 fd, i64 maskaddr, i64 sizemask,
                        i32 flags) {
#ifdef __linux__
  sigset_t mask;
  u8 buf[8];
  int sysflags = 0, oflags = O_RDWR;
  if ((size_t)sizemask > sizeof(buf)) sizemask = sizeof(buf);
  if (CopyFromUserRead(m, buf, maskaddr, sizemask) == -1) return -1;
  sigemptyset(&mask);
  {
    u64 m64 = 0;
    for (int i = 0; i < (int)sizemask && i < 8; i++) m64 |= ((u64)buf[i]) << (i * 8);
    for (int sig = 1; sig < 64; sig++) {
      if (m64 & ((u64)1 << (sig - 1))) sigaddset(&mask, sig);
    }
  }
  if (flags & O_NDELAY_LINUX) {
    sysflags |= SFD_NONBLOCK;
    oflags |= O_CLOEXEC;
    flags &= ~O_NDELAY_LINUX;
  }
  if (flags & O_CLOEXEC_LINUX) {
    sysflags |= SFD_CLOEXEC;
    oflags |= O_CLOEXEC;
    flags &= ~O_CLOEXEC_LINUX;
  }
  if (flags) {
    LOGF("unsupported %s flags: %#x", "signalfd4", flags);
    return einval();
  }
  if (fd != -1) {
    int hfd = GetDirFildes(fd);  // resolves guest fd → host fd
    return signalfd(hfd, &mask, sysflags);
  }
  return FinishHostFd(m, signalfd(-1, &mask, sysflags), oflags);
#else
  return enosys();
#endif
}

static int XlatLinuxClockToHost(int clk) {
  switch (clk) {
    case 0: return CLOCK_REALTIME;
    case 1: return CLOCK_MONOTONIC;
#ifdef CLOCK_BOOTTIME
    case 7: return CLOCK_BOOTTIME;
#endif
#ifdef CLOCK_REALTIME_ALARM
    case 8: return CLOCK_REALTIME_ALARM;
#endif
#ifdef CLOCK_BOOTTIME_ALARM
    case 9: return CLOCK_BOOTTIME_ALARM;
#endif
    default: return -1;
  }
}

static i32 SysTimerfdCreate(struct Machine *m, i32 clockid, i32 flags) {
#ifdef __linux__
  int sysflags = 0, oflags = O_RDWR, hclk;
  if ((hclk = XlatLinuxClockToHost(clockid)) == -1) return einval();
  if (flags & O_NDELAY_LINUX) {
    sysflags |= TFD_NONBLOCK;
    oflags |= O_CLOEXEC;
    flags &= ~O_NDELAY_LINUX;
  }
  if (flags & O_CLOEXEC_LINUX) {
    sysflags |= TFD_CLOEXEC;
    oflags |= O_CLOEXEC;
    flags &= ~O_CLOEXEC_LINUX;
  }
  if (flags) {
    LOGF("unsupported %s flags: %#x", "timerfd_create", flags);
    return einval();
  }
  return FinishHostFd(m, timerfd_create(hclk, sysflags), oflags);
#else
  return enosys();
#endif
}

#ifdef __linux__
static void XlatItimerspecFromLinux(struct itimerspec *dst,
                                    const struct itimerspec_linux *src) {
  dst->it_interval.tv_sec = Read64(src->interval.sec);
  dst->it_interval.tv_nsec = Read64(src->interval.nsec);
  dst->it_value.tv_sec = Read64(src->value.sec);
  dst->it_value.tv_nsec = Read64(src->value.nsec);
}

static void XlatItimerspecToLinux(struct itimerspec_linux *dst,
                                  const struct itimerspec *src) {
  Write64(dst->interval.sec, src->it_interval.tv_sec);
  Write64(dst->interval.nsec, src->it_interval.tv_nsec);
  Write64(dst->value.sec, src->it_value.tv_sec);
  Write64(dst->value.nsec, src->it_value.tv_nsec);
}
#endif

static i32 SysTimerfdSettime(struct Machine *m, i32 fd, i32 flags,
                             i64 newaddr, i64 oldaddr) {
#ifdef __linux__
  int sysflags = 0, rc;
  struct itimerspec_linux gnew, gold;
  struct itimerspec snew, sold;
  if (flags & 1) sysflags |= TFD_TIMER_ABSTIME;
  flags &= ~1;
  if (flags) return einval();
  if (CopyFromUserRead(m, &gnew, newaddr, sizeof(gnew)) == -1) return -1;
  XlatItimerspecFromLinux(&snew, &gnew);
  if ((rc = timerfd_settime(fd, sysflags, &snew, oldaddr ? &sold : 0)) == -1) {
    return -1;
  }
  if (oldaddr) {
    XlatItimerspecToLinux(&gold, &sold);
    if (CopyToUserWrite(m, oldaddr, &gold, sizeof(gold)) == -1) return -1;
  }
  return rc;
#else
  return enosys();
#endif
}

static i32 SysTimerfdGettime(struct Machine *m, i32 fd, i64 curraddr) {
#ifdef __linux__
  int rc;
  struct itimerspec_linux gcur;
  struct itimerspec scur;
  if ((rc = timerfd_gettime(fd, &scur)) == -1) return -1;
  XlatItimerspecToLinux(&gcur, &scur);
  if (CopyToUserWrite(m, curraddr, &gcur, sizeof(gcur)) == -1) return -1;
  return rc;
#else
  return enosys();
#endif
}

static i32 SysInotifyInit1(struct Machine *m, i32 flags) {
#ifdef __linux__
  int sysflags = 0, oflags = O_RDWR;
  if (flags & O_NDELAY_LINUX) {
    sysflags |= IN_NONBLOCK;
    oflags |= O_CLOEXEC;
    flags &= ~O_NDELAY_LINUX;
  }
  if (flags & O_CLOEXEC_LINUX) {
    sysflags |= IN_CLOEXEC;
    oflags |= O_CLOEXEC;
    flags &= ~O_CLOEXEC_LINUX;
  }
  if (flags) {
    LOGF("unsupported %s flags: %#x", "inotify_init1", flags);
    return einval();
  }
  return FinishHostFd(m, inotify_init1(sysflags), oflags);
#else
  return enosys();
#endif
}

static i32 SysInotifyInit(struct Machine *m) {
  return SysInotifyInit1(m, 0);
}

static i32 SysInotifyAddWatch(struct Machine *m, i32 fd, i64 pathaddr,
                              u32 mask) {
#ifdef __linux__
  const char *path;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  return inotify_add_watch(fd, path, mask);
#else
  return enosys();
#endif
}

static i32 SysInotifyRmWatch(struct Machine *m, i32 fd, i32 wd) {
#ifdef __linux__
  return inotify_rm_watch(fd, wd);
#else
  return enosys();
#endif
}

static i32 SysEventfd(struct Machine *m, u32 initval) {
  return SysEventfd2(m, initval, 0);
}

static i32 SysSignalfd(struct Machine *m, i32 fd, i64 maskaddr, i64 sizemask) {
  return SysSignalfd4(m, fd, maskaddr, sizemask, 0);
}

// Linux waitid idtype constants
#define P_ALL_LINUX  0
#define P_PID_LINUX  1
#define P_PGID_LINUX 2
// Linux WEXITED/WSTOPPED/WCONTINUED already defined; waitid needs them as
// the "which states to report" mask (in addition to WNOHANG/WNOWAIT).
#define WSTOPPED_LINUX WUNTRACED_LINUX

static int XlatWaitidOptions(int x, int *out) {
  int r = 0;
  if (x & WNOHANG_LINUX) {
    r |= WNOHANG;
    x &= ~WNOHANG_LINUX;
  }
  if (x & WUNTRACED_LINUX) {  // == WSTOPPED
    r |= WUNTRACED;
    x &= ~WUNTRACED_LINUX;
  }
  if (x & WEXITED_LINUX) {
    x &= ~WEXITED_LINUX;  // always reported by host waitid
  }
#ifdef WCONTINUED
  if (x & WCONTINUED_LINUX) {
    r |= WCONTINUED;
    x &= ~WCONTINUED_LINUX;
  }
#endif
#ifdef WNOWAIT
  if (x & WNOWAIT_LINUX) {
    r |= WNOWAIT;
    x &= ~WNOWAIT_LINUX;
  }
#endif
  if (x) {
    LOGF("%s %d not supported yet", "waitid", x);
    return einval();
  }
  *out = r;
  return 0;
}

static int XlatWaitidType(int idtype, idtype_t *out) {
  switch (idtype) {
    case P_ALL_LINUX:  *out = P_ALL;  return 0;
    case P_PID_LINUX:  *out = P_PID;  return 0;
    case P_PGID_LINUX: *out = P_PGID; return 0;
    default:
      LOGF("waitid idtype %d not supported", idtype);
      return einval();
  }
}

static int SysWaitid(struct Machine *m, i32 idtype, i32 id, i64 infoaddr,
                     i32 options, i64 rusageaddr) {
  int rc, sysopts;
  idtype_t sysidtype;
  siginfo_t info;
  struct siginfo_linux gsi;
  struct rusage hrusage;
  struct rusage_linux grusage;
  if (XlatWaitidType(idtype, &sysidtype) == -1) return -1;
  if (XlatWaitidOptions(options, &sysopts) == -1) return -1;
  memset(&info, 0, sizeof(info));
  if (rusageaddr) memset(&hrusage, 0, sizeof(hrusage));
#ifdef __linux__
  // Linux's waitid() exposes a 5-arg variant taking rusage*; libc doesn't.
  RESTARTABLE(rc = syscall(SYS_waitid, sysidtype, id, &info, sysopts,
                           rusageaddr ? &hrusage : NULL));
#else
  RESTARTABLE(rc = waitid(sysidtype, id, &info, sysopts));
  if (rc != -1 && rusageaddr && info.si_pid > 0) {
    getrusage(info.si_pid, &hrusage);
  }
#endif
  if (rc == -1) return -1;
  memset(&gsi, 0, sizeof(gsi));
  // info.si_signo is 0 if WNOHANG and no child changed state
  if (info.si_signo) {
    Write32(gsi.signo, UnXlatSignal(info.si_signo));
    Write32(gsi.errno_, 0);
    {
      int code;
      switch (info.si_code) {
        case CLD_EXITED:    code = CLD_EXITED_LINUX; break;
        case CLD_KILLED:    code = CLD_KILLED_LINUX; break;
        case CLD_DUMPED:    code = CLD_DUMPED_LINUX; break;
        case CLD_TRAPPED:   code = CLD_TRAPPED_LINUX; break;
        case CLD_STOPPED:   code = CLD_STOPPED_LINUX; break;
        case CLD_CONTINUED: code = CLD_CONTINUED_LINUX; break;
        default:            code = info.si_code; break;
      }
      Write32(gsi.code, code);
    }
    Write32(gsi.pid, info.si_pid);
    Write32(gsi.uid, info.si_uid);
    // For CLD_EXITED, status is the exit code; otherwise it's the signal.
    if (info.si_code == CLD_EXITED) {
      Write32(gsi.status, info.si_status);
    } else {
      Write32(gsi.status, UnXlatSignal(info.si_status));
    }
  }
  if (CopyToUserWrite(m, infoaddr, &gsi, sizeof(gsi)) == -1) return -1;
  if (rusageaddr) {
    XlatRusageToLinux(&grusage, &hrusage);
    if (CopyToUserWrite(m, rusageaddr, &grusage, sizeof(grusage)) == -1) {
      return -1;
    }
  }
  return 0;
}

static i32 SysMlock(struct Machine *m, i64 addr, u64 len) {
  (void)m; (void)addr; (void)len;
  return 0;
}

static i32 SysMlock2(struct Machine *m, i64 addr, u64 len, u32 flags) {
  (void)m; (void)addr; (void)len; (void)flags;
  return 0;
}

static i32 SysMunlock(struct Machine *m, i64 addr, u64 len) {
  (void)m; (void)addr; (void)len;
  return 0;
}

static i32 SysMlockall(struct Machine *m, i32 flags) {
  (void)m; (void)flags;
  return 0;
}

static i32 SysMunlockall(struct Machine *m) {
  (void)m;
  return 0;
}

// struct open_how (Linux uapi). Total 24 bytes.
struct open_how_linux {
  u8 flags[8];
  u8 mode[8];
  u8 resolve[8];
};

static i32 SysOpenat2(struct Machine *m, i32 dirfd, i64 pathaddr, i64 howaddr,
                      u64 usize) {
  struct open_how_linux how;
  u64 flags, mode, resolve;
  // Linux: usize too small → EINVAL; too big → E2BIG (we use EINVAL too).
  if (usize < sizeof(how)) return einval();
  if (usize > 4096) return einval();
  memset(&how, 0, sizeof(how));
  if (CopyFromUserRead(m, &how, howaddr, sizeof(how)) == -1) return -1;
  // Linux: trailing bytes beyond sizeof(how) must be zero — we don't validate
  // them (assumes caller is well-behaved; the basic-stats path covers all
  // common modern callers that probe with usize=24).
  flags = Read64(how.flags);
  mode = Read64(how.mode);
  resolve = Read64(how.resolve);
  // mode must be zero unless O_CREAT/O_TMPFILE is in flags.
  if (mode && !(flags & (O_CREAT_LINUX | __O_TMPFILE_LINUX))) return einval();
  // RESOLVE_* — we don't currently honor any of these. Most callers probe
  // with resolve=0, so refuse any non-zero value to make them fall back to
  // openat. (RESOLVE_NO_SYMLINKS could be approximated with O_NOFOLLOW, but
  // its semantics differ on intermediate components — be conservative.)
  if (resolve) {
    LOGF("openat2 resolve=%#" PRIx64 " not supported", resolve);
    return einval();
  }
  return SysOpenat(m, dirfd, pathaddr, (i32)flags, (i32)mode);
}

// struct clone_args (Linux uapi). The three documented sizes are
// CLONE_ARGS_SIZE_VER0=64, VER1=80, VER2=88. We honor up to VER2 and refuse
// non-zero fields beyond what we can implement.
struct clone_args_linux {
  u8 flags[8];        // off 0
  u8 pidfd[8];        // off 8
  u8 child_tid[8];    // off 16
  u8 parent_tid[8];   // off 24
  u8 exit_signal[8];  // off 32
  u8 stack[8];        // off 40
  u8 stack_size[8];   // off 48
  u8 tls[8];          // off 56  -- end of VER0
  u8 set_tid[8];      // off 64
  u8 set_tid_size[8]; // off 72  -- end of VER1
  u8 cgroup[8];       // off 80  -- end of VER2
};

static i32 SysClone3(struct Machine *m, i64 argsaddr, u64 size) {
  struct clone_args_linux args;
  u64 flags, stack, stack_size, parent_tid, child_tid, tls, exit_signal;
  u64 pidfd_ptr, set_tid, set_tid_size, cgroup;
  if (size < 64) return einval();
  if (size > sizeof(args)) {
    // Unknown trailing fields — refuse rather than silently ignore.
    LOGF("clone3 size=%" PRIu64 " exceeds supported", size);
    return einval();
  }
  memset(&args, 0, sizeof(args));
  if (CopyFromUserRead(m, &args, argsaddr, size) == -1) return -1;
  flags = Read64(args.flags);
  pidfd_ptr = Read64(args.pidfd);
  child_tid = Read64(args.child_tid);
  parent_tid = Read64(args.parent_tid);
  exit_signal = Read64(args.exit_signal);
  stack = Read64(args.stack);
  stack_size = Read64(args.stack_size);
  tls = Read64(args.tls);
  set_tid = (size >= 72) ? Read64(args.set_tid) : 0;
  set_tid_size = (size >= 80) ? Read64(args.set_tid_size) : 0;
  cgroup = (size >= 88) ? Read64(args.cgroup) : 0;
  if (set_tid || set_tid_size) {
    LOGF("clone3 set_tid not supported");
    return einval();
  }
  if (cgroup) {
    LOGF("clone3 CLONE_INTO_CGROUP not supported");
    return einval();
  }
  if (pidfd_ptr && !(flags & 0x00001000)) {  // CLONE_PIDFD = 0x1000
    // pidfd ptr without CLONE_PIDFD is invalid per kernel.
    return einval();
  }
  if (pidfd_ptr) {
    LOGF("clone3 CLONE_PIDFD not supported");
    return einval();
  }
  // Compose into the legacy clone() ABI:
  //   flags = clone_flags | (exit_signal & 0xff)
  //   stack = top of stack (start of region + size, since the kernel grows down)
  // For fork-style (stack == 0), pass 0 through unchanged.
  flags |= (exit_signal & 0xff);
  if (stack) stack += stack_size;
  return SysClone(m, flags, stack, parent_tid, child_tid, tls, 0);
}

// Linux SPLICE_F_* flag values are stable across architectures and match the
// host on Linux, so no translation needed. We pass them through and let the
// host kernel reject anything unknown. On non-Linux hosts we return ENOSYS.
#define SPLICE_F_ALL_LINUX 0x0f  // MOVE|NONBLOCK|MORE|GIFT

static i64 SysSplice(struct Machine *m, i32 fd_in, i64 off_in_addr, i32 fd_out,
                     i64 off_out_addr, u64 len, u32 flags) {
#ifdef __linux__
  i64 rc;
  u8 *off_in_p = 0, *off_out_p = 0;
  loff_t off_in_v, off_out_v;
  loff_t *off_in = 0, *off_out = 0;
  if (flags & ~SPLICE_F_ALL_LINUX) {
    LOGF("splice flags %#x not supported", flags);
    return einval();
  }
  if (off_in_addr) {
    if (!(off_in_p = (u8 *)SchlepRW(m, off_in_addr, 8))) return -1;
    off_in_v = (loff_t)Read64(off_in_p);
    off_in = &off_in_v;
  }
  if (off_out_addr) {
    if (!(off_out_p = (u8 *)SchlepRW(m, off_out_addr, 8))) return -1;
    off_out_v = (loff_t)Read64(off_out_p);
    off_out = &off_out_v;
  }
  RESTARTABLE(rc = splice(fd_in, off_in, fd_out, off_out, (size_t)len, flags));
  if (rc != -1) {
    if (off_in_p) Write64(off_in_p, (u64)off_in_v);
    if (off_out_p) Write64(off_out_p, (u64)off_out_v);
  }
  return rc;
#else
  (void)m; (void)fd_in; (void)off_in_addr; (void)fd_out; (void)off_out_addr;
  (void)len; (void)flags;
  return enosys();
#endif
}

static i64 SysTee(struct Machine *m, i32 fd_in, i32 fd_out, u64 len,
                  u32 flags) {
#ifdef __linux__
  i64 rc;
  (void)m;
  if (flags & ~SPLICE_F_ALL_LINUX) {
    LOGF("tee flags %#x not supported", flags);
    return einval();
  }
  RESTARTABLE(rc = tee(fd_in, fd_out, (size_t)len, flags));
  return rc;
#else
  (void)m; (void)fd_in; (void)fd_out; (void)len; (void)flags;
  return enosys();
#endif
}

static i64 SysVmsplice(struct Machine *m, i32 fildes, i64 iovaddr, u64 iovlen,
                       u32 flags) {
#ifdef __linux__
  i64 rc;
  struct Iovs iv;
  // The direction (read vs write) depends on the pipe end. We don't know it
  // here without checking the fd, so we use PROT_READ which lets the host
  // kernel handle both directions — for the rare "splice FROM pipe INTO
  // userspace" case (SPLICE_F_GIFT with read end), the guest memory will
  // need to be writable; in that case AppendIovsGuest will catch it via the
  // page permissions and the host call will EFAULT.
  int prot = PROT_READ;
  if (flags & ~SPLICE_F_ALL_LINUX) {
    LOGF("vmsplice flags %#x not supported", flags);
    return einval();
  }
  if (iovlen > IOV_MAX_LINUX) return einval();
  if (!iovlen) return 0;
  InitIovs(&iv);
  if ((rc = AppendIovsGuest(m, &iv, iovaddr, (int)iovlen, prot)) != -1) {
    if (iv.i) {
      RESTARTABLE(rc = vmsplice(fildes, iv.p, iv.i, flags));
    } else {
      rc = 0;
    }
  }
  FreeIovs(&iv);
  return rc;
#else
  (void)m; (void)fildes; (void)iovaddr; (void)iovlen; (void)flags;
  return enosys();
#endif
}

static i32 SysPidfdOpen(struct Machine *m, i32 pid, u32 flags) {
#ifdef __linux__
  int oflags = O_RDWR;
  // PIDFD_NONBLOCK = O_NONBLOCK = 0x800 in Linux; pass through to host.
  if (flags & O_NDELAY_LINUX) {
    oflags |= O_CLOEXEC;  // blink tracks nonblock via cloexec slot
  }
  // Unknown bits → let the host kernel reject (it'll return EINVAL).
  int hfd = (int)syscall(SYS_pidfd_open, (pid_t)pid, (unsigned)flags);
  return FinishHostFd(m, hfd, oflags);
#else
  (void)m; (void)pid; (void)flags;
  return enosys();
#endif
}

static i32 SysPidfdSendSignal(struct Machine *m, i32 pidfd, i32 sig,
                              i64 infoaddr, u32 flags) {
#ifdef __linux__
  int syssig;
  (void)m;
  if (flags) {
    LOGF("pidfd_send_signal flags %#x not supported", flags);
    return einval();
  }
  if (infoaddr) {
    // Non-NULL siginfo would require translating guest siginfo_linux to host
    // siginfo_t — field layouts diverge in the union tail. Real callers
    // (systemd, supervisors) pass NULL in the common path; refuse the rest
    // so glibc's wrapper falls back rather than misdelivering.
    LOGF("pidfd_send_signal with non-NULL info not supported");
    return einval();
  }
  if (sig == 0) {
    // Existence check only — host pidfd_send_signal(pidfd, 0, ...) works.
    return (i32)syscall(SYS_pidfd_send_signal, pidfd, 0, (void *)0, 0u);
  }
  if ((syssig = XlatSignal(sig)) == -1) return einval();
  return (i32)syscall(SYS_pidfd_send_signal, pidfd, syssig, (void *)0, 0u);
#else
  (void)m; (void)pidfd; (void)sig; (void)infoaddr; (void)flags;
  return enosys();
#endif
}

static i32 SysPidfdGetfd(struct Machine *m, i32 pidfd, i32 targetfd,
                         u32 flags) {
#ifdef __linux__
  if (flags) {
    LOGF("pidfd_getfd flags %#x not supported", flags);
    return einval();
  }
  int hfd = (int)syscall(SYS_pidfd_getfd, pidfd, targetfd, flags);
  return FinishHostFd(m, hfd, O_RDWR);
#else
  (void)m; (void)pidfd; (void)targetfd; (void)flags;
  return enosys();
#endif
}

// FreeBSD procctl command numbers (sys/sys/procctl.h).
#define PROC_REAP_ACQUIRE_FBSD       2
#define PROC_REAP_RELEASE_FBSD       3
#define PROC_PDEATHSIG_CTL_FBSD     11
#define PROC_PDEATHSIG_STATUS_FBSD  12
#define PROC_NO_NEW_PRIVS_CTL_FBSD  19
#define PROC_NO_NEW_PRIVS_STATUS_FBSD 20

static int SysFreeBSDProcctl(struct Machine *m, i32 idtype, i64 id, i32 cmd,
                             i64 dataaddr) {
#ifdef __linux__
  // We only support P_PID (1) — the common case is "this process" with id=0.
  // P_PGID (2) and a few others exist but are rarely used by libc/apps.
  if (idtype != 1 && idtype != 0) {
    LOGF("procctl idtype %d not supported", idtype);
    return einval();
  }
  if (id != 0 && id != m->system->pid) {
    LOGF("procctl id=%" PRId64 " (only self supported)", id);
    return einval();
  }
  switch (cmd) {
    case PROC_PDEATHSIG_CTL_FBSD: {
#ifdef PR_SET_PDEATHSIG
      u8 buf[4];
      int sig, hsig;
      if (CopyFromUserRead(m, buf, dataaddr, sizeof(buf)) == -1) return -1;
      sig = (i32)Read32(buf);
      if (sig == 0) {
        hsig = 0;  // clear pdeathsig
      } else if ((hsig = XlatSignal(sig)) == -1) {
        return einval();
      }
      return prctl(PR_SET_PDEATHSIG, hsig, 0, 0, 0);
#else
      return enosys();
#endif
    }
    case PROC_PDEATHSIG_STATUS_FBSD: {
#ifdef PR_GET_PDEATHSIG
      int hsig = 0, sig;
      u8 buf[4];
      if (prctl(PR_GET_PDEATHSIG, (unsigned long)&hsig, 0, 0, 0) == -1) {
        return -1;
      }
      sig = hsig ? UnXlatSignal(hsig) : 0;
      Write32(buf, sig);
      if (CopyToUserWrite(m, dataaddr, buf, sizeof(buf)) == -1) return -1;
      return 0;
#else
      return enosys();
#endif
    }
    case PROC_REAP_ACQUIRE_FBSD:
#ifdef PR_SET_CHILD_SUBREAPER
      return prctl(PR_SET_CHILD_SUBREAPER, 1, 0, 0, 0);
#else
      return enosys();
#endif
    case PROC_REAP_RELEASE_FBSD:
#ifdef PR_SET_CHILD_SUBREAPER
      return prctl(PR_SET_CHILD_SUBREAPER, 0, 0, 0, 0);
#else
      return enosys();
#endif
    case PROC_NO_NEW_PRIVS_CTL_FBSD: {
#ifdef PR_SET_NO_NEW_PRIVS
      u8 buf[4];
      int val;
      if (CopyFromUserRead(m, buf, dataaddr, sizeof(buf)) == -1) return -1;
      val = (i32)Read32(buf);
      // FreeBSD allows enable (1) or disable (0); Linux only supports the
      // one-way latch (enable). Refuse disable.
      if (val == 0) return einval();
      if (val != 1) return einval();
      return prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0);
#else
      return enosys();
#endif
    }
    case PROC_NO_NEW_PRIVS_STATUS_FBSD: {
#ifdef PR_GET_NO_NEW_PRIVS
      int rc = prctl(PR_GET_NO_NEW_PRIVS, 0, 0, 0, 0);
      u8 buf[4];
      if (rc == -1) return -1;
      Write32(buf, rc);
      if (CopyToUserWrite(m, dataaddr, buf, sizeof(buf)) == -1) return -1;
      return 0;
#else
      return enosys();
#endif
    }
    default:
      LOGF("procctl cmd %d not supported", cmd);
      return einval();
  }
#else
  (void)m; (void)idtype; (void)id; (void)cmd; (void)dataaddr;
  return enosys();
#endif
}

static i32 SysFreeBSDPselect(struct Machine *m, i32 nfds, i64 readfds_addr,
                             i64 writefds_addr, i64 exceptfds_addr,
                             i64 timeout_addr, i64 sigmask_addr) {
  u64 sigmask, *sigmaskp = 0;
  const u8 *sm;
  struct timespec timeout, *timeoutp;
  if (timeout_addr) {
    if (LoadTimespecRW(m, timeout_addr, &timeout) == -1) return -1;
    timeoutp = &timeout;
  } else {
    timeoutp = 0;
  }
  if (sigmask_addr) {
    // FreeBSD sigset_t is 16 bytes (4 u32). The lowest 8 bytes cover signals
    // 1..64 in little-endian layout, which is what Select() wants.
    if (!(sm = (const u8 *)SchlepR(m, sigmask_addr, 8))) return -1;
    sigmask = Read64(sm);
    sigmaskp = &sigmask;
  }
  return Select(m, nfds, readfds_addr, writefds_addr, exceptfds_addr, timeoutp,
                sigmaskp);
}

static int SysFreeBSDChflags(struct Machine *m, u32 flags) {
  // FreeBSD UF_/SF_ flags don't map cleanly to Linux FS_*FL_ via FS_IOC_*FLAGS,
  // and most common callers (rm, tar -p, pkg) only want to *clear* flags before
  // removing/updating a file. Accept flags==0 silently; refuse anything else so
  // callers can detect and skip.
  (void)m;
  if (flags == 0) return 0;
  LOGF("chflags(flags=%#x) not supported", flags);
  errno = EOPNOTSUPP;
  return -1;
}

static int SysFreeBSDPdgetpid(struct Machine *m, i32 fildes, i64 pidaddr) {
  struct Fd *fd;
  i32 pid;
  LOCK(&m->system->fds.lock);
  fd = GetFd(&m->system->fds, fildes);
  pid = (fd && fd->pdpid) ? fd->pdpid : 0;
  UNLOCK(&m->system->fds.lock);
  if (!fd) return ebadf();
  if (!pid) return einval();  // fd isn't a process descriptor
  {
    u8 buf[4];
    Write32(buf, pid);
    if (CopyToUserWrite(m, pidaddr, buf, 4) == -1) return -1;
  }
  return 0;
}

static int SysFreeBSDGetlogin(struct Machine *m, i64 nameaddr, u32 namelen) {
  // Real lookups would need /etc/passwd. For chroots that may lack it, return
  // a stable placeholder so callers don't fall over. Most apps treat this as
  // a hint, not an authentication credential.
  const char *name;
#ifdef __linux__
  if (!(name = getlogin())) name = "root";
#else
  name = "root";
#endif
  size_t nlen = strlen(name) + 1;
  if ((size_t)namelen < nlen) {
    errno = ERANGE;
    return -1;
  }
  if (CopyToUserWrite(m, nameaddr, name, nlen) == -1) return -1;
  return 0;
}

static i32 SysFreeBSDPosixOpenpt(struct Machine *m, i32 fbsd_flags) {
  // Translate FreeBSD oflags → host oflags. posix_openpt only honors a small
  // subset (O_RDWR, O_NOCTTY, O_NONBLOCK, O_CLOEXEC).
  // FreeBSD: O_RDWR=2, O_NONBLOCK=4, O_NOCTTY=0x8000, O_CLOEXEC=0x100000
  int hflags = 0;
  int oflags = O_RDWR;
  if ((fbsd_flags & 3) == 2) hflags |= O_RDWR;
  if ((fbsd_flags & 3) == 1) hflags |= O_WRONLY;
  if (fbsd_flags & 0x4) hflags |= O_NONBLOCK;
  if (fbsd_flags & 0x8000) hflags |= O_NOCTTY;
  if (fbsd_flags & 0x100000) {
    hflags |= O_CLOEXEC;
    oflags |= O_CLOEXEC;
  }
  int lim, fildes;
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  // Deliberately bypass VFS/chroot — the chroot rarely contains a working
  // /dev/ptmx, but the host's pty allocator is what we actually want to
  // use. The returned slave (/dev/pts/N) still has to be reachable by the
  // guest; the user is expected to bind-mount or symlink /dev/pts into the
  // chroot if they want the slave open to succeed.
  RESTARTABLE(fildes = open("/dev/ptmx", hflags));
  if (fildes == -1) return -1;
  if (fildes >= lim) {
    close(fildes);
    return emfile();
  }
#ifdef TIOCSPTLCK
  // Linux ptmx hands the slave back locked; userspace must call
  // ioctl(TIOCSPTLCK, &0) before the slave is openable. FreeBSD's unlockpt
  // is a no-op, so the guest will never make this call itself — do it now.
  {
    int unlock = 0;
    ioctl(fildes, TIOCSPTLCK, &unlock);
  }
#endif
  LOCK(&m->system->fds.lock);
  unassert(AddFd(&m->system->fds, fildes, oflags));
  UNLOCK(&m->system->fds.lock);
  return fildes;
}

static int SysFreeBSDThrSetName(struct Machine *m, long id, i64 nameaddr) {
  const char *name;
  char buf[16];
  if (!(name = LoadStr(m, nameaddr))) return -1;
  // Kernel comm field is 15 bytes + nul.
  snprintf(buf, sizeof(buf), "%s", name);
#ifdef __linux__
  if (id == -1 || id == 0 || id == m->tid) {
#ifdef PR_SET_NAME
    return prctl(PR_SET_NAME, (unsigned long)buf, 0, 0, 0);
#else
    return 0;
#endif
  }
  {
    char path[64];
    int fd, rc;
    snprintf(path, sizeof(path), "/proc/self/task/%ld/comm", id);
    if ((fd = open(path, O_WRONLY | O_CLOEXEC)) == -1) return -1;
    rc = write(fd, buf, strlen(buf));
    close(fd);
    return rc == -1 ? -1 : 0;
  }
#else
  (void)m; (void)id;
  return 0;
#endif
}

// ----- Linux xattr family ---------------------------------------------------
// All thin wrappers around the host xattr syscalls. Names/values pass through
// unchanged. On non-Linux hosts we return ENOSYS so callers can detect-and-skip.

#ifdef __linux__
static i64 XattrGet(struct Machine *m, const char *path, int fd,
                    i64 nameaddr, i64 valueaddr, u64 size, int kind) {
  const char *name;
  void *buf = 0;
  ssize_t rc;
  if (!(name = LoadStr(m, nameaddr))) return -1;
  if (size && valueaddr) {
    if (!(buf = AddToFreeList(m, malloc(size)))) return enomem();
  }
  // kind: 0=getxattr 1=lgetxattr 2=fgetxattr
  if (kind == 0)      rc = getxattr(path, name, buf, size);
  else if (kind == 1) rc = lgetxattr(path, name, buf, size);
  else                rc = fgetxattr(fd, name, buf, size);
  if (rc > 0 && valueaddr) {
    if (CopyToUserWrite(m, valueaddr, buf, rc) == -1) return -1;
  }
  return rc;
}

static i64 XattrSet(struct Machine *m, const char *path, int fd,
                    i64 nameaddr, i64 valueaddr, u64 size, i32 flags, int kind) {
  const char *name;
  void *buf = 0;
  if (!(name = LoadStr(m, nameaddr))) return -1;
  if (size) {
    if (!(buf = AddToFreeList(m, malloc(size)))) return enomem();
    if (CopyFromUserRead(m, buf, valueaddr, size) == -1) return -1;
  }
  if (kind == 0)      return setxattr(path, name, buf, size, flags);
  else if (kind == 1) return lsetxattr(path, name, buf, size, flags);
  else                return fsetxattr(fd, name, buf, size, flags);
}

static i64 XattrList(struct Machine *m, const char *path, int fd, i64 listaddr,
                     u64 size, int kind) {
  void *buf = 0;
  ssize_t rc;
  if (size && listaddr) {
    if (!(buf = AddToFreeList(m, malloc(size)))) return enomem();
  }
  if (kind == 0)      rc = listxattr(path, buf, size);
  else if (kind == 1) rc = llistxattr(path, buf, size);
  else                rc = flistxattr(fd, buf, size);
  if (rc > 0 && listaddr) {
    if (CopyToUserWrite(m, listaddr, buf, rc) == -1) return -1;
  }
  return rc;
}

static i64 XattrRemove(struct Machine *m, const char *path, int fd,
                       i64 nameaddr, int kind) {
  const char *name;
  if (!(name = LoadStr(m, nameaddr))) return -1;
  if (kind == 0)      return removexattr(path, name);
  else if (kind == 1) return lremovexattr(path, name);
  else                return fremovexattr(fd, name);
}
#endif

static i64 SysSetxattr(struct Machine *m, i64 pa, i64 na, i64 va, u64 sz, i32 fl) {
#ifdef __linux__
  const char *p = LoadStr(m, pa);
  if (!p) return -1;
  return XattrSet(m, p, -1, na, va, sz, fl, 0);
#else
  (void)m; (void)pa; (void)na; (void)va; (void)sz; (void)fl;
  return enosys();
#endif
}
static i64 SysLsetxattr(struct Machine *m, i64 pa, i64 na, i64 va, u64 sz, i32 fl) {
#ifdef __linux__
  const char *p = LoadStr(m, pa);
  if (!p) return -1;
  return XattrSet(m, p, -1, na, va, sz, fl, 1);
#else
  (void)m; (void)pa; (void)na; (void)va; (void)sz; (void)fl;
  return enosys();
#endif
}
static i64 SysFsetxattr(struct Machine *m, i32 fd, i64 na, i64 va, u64 sz, i32 fl) {
#ifdef __linux__
  return XattrSet(m, 0, fd, na, va, sz, fl, 2);
#else
  (void)m; (void)fd; (void)na; (void)va; (void)sz; (void)fl;
  return enosys();
#endif
}
static i64 SysGetxattr(struct Machine *m, i64 pa, i64 na, i64 va, u64 sz) {
#ifdef __linux__
  const char *p = LoadStr(m, pa);
  if (!p) return -1;
  return XattrGet(m, p, -1, na, va, sz, 0);
#else
  (void)m; (void)pa; (void)na; (void)va; (void)sz;
  return enosys();
#endif
}
static i64 SysLgetxattr(struct Machine *m, i64 pa, i64 na, i64 va, u64 sz) {
#ifdef __linux__
  const char *p = LoadStr(m, pa);
  if (!p) return -1;
  return XattrGet(m, p, -1, na, va, sz, 1);
#else
  (void)m; (void)pa; (void)na; (void)va; (void)sz;
  return enosys();
#endif
}
static i64 SysFgetxattr(struct Machine *m, i32 fd, i64 na, i64 va, u64 sz) {
#ifdef __linux__
  return XattrGet(m, 0, fd, na, va, sz, 2);
#else
  (void)m; (void)fd; (void)na; (void)va; (void)sz;
  return enosys();
#endif
}
static i64 SysListxattr(struct Machine *m, i64 pa, i64 la, u64 sz) {
#ifdef __linux__
  const char *p = LoadStr(m, pa);
  if (!p) return -1;
  return XattrList(m, p, -1, la, sz, 0);
#else
  (void)m; (void)pa; (void)la; (void)sz;
  return enosys();
#endif
}
static i64 SysLlistxattr(struct Machine *m, i64 pa, i64 la, u64 sz) {
#ifdef __linux__
  const char *p = LoadStr(m, pa);
  if (!p) return -1;
  return XattrList(m, p, -1, la, sz, 1);
#else
  (void)m; (void)pa; (void)la; (void)sz;
  return enosys();
#endif
}
static i64 SysFlistxattr(struct Machine *m, i32 fd, i64 la, u64 sz) {
#ifdef __linux__
  return XattrList(m, 0, fd, la, sz, 2);
#else
  (void)m; (void)fd; (void)la; (void)sz;
  return enosys();
#endif
}
static i64 SysRemovexattr(struct Machine *m, i64 pa, i64 na) {
#ifdef __linux__
  const char *p = LoadStr(m, pa);
  if (!p) return -1;
  return XattrRemove(m, p, -1, na, 0);
#else
  (void)m; (void)pa; (void)na;
  return enosys();
#endif
}
static i64 SysLremovexattr(struct Machine *m, i64 pa, i64 na) {
#ifdef __linux__
  const char *p = LoadStr(m, pa);
  if (!p) return -1;
  return XattrRemove(m, p, -1, na, 1);
#else
  (void)m; (void)pa; (void)na;
  return enosys();
#endif
}
static i64 SysFremovexattr(struct Machine *m, i32 fd, i64 na) {
#ifdef __linux__
  return XattrRemove(m, 0, fd, na, 2);
#else
  (void)m; (void)fd; (void)na;
  return enosys();
#endif
}

// ----- Linux misc easy stubs -----------------------------------------------

static int SysPersonality(struct Machine *m, u64 pers) {
  // PER_LINUX = 0. Most callers pass 0xffffffff to query the current value.
  (void)m; (void)pers;
  return 0;
}

static int SysMincore(struct Machine *m, i64 addr, u64 length, i64 vecaddr) {
#ifdef __linux__
  // Best-effort: report all pages as resident. Allocators that probe this
  // (jemalloc) only use it as a hint; lying optimistically is fine.
  (void)m; (void)addr;
  unsigned char *buf;
  size_t pages = (length + 4095) / 4096;
  if (!pages) return 0;
  if (!(buf = (unsigned char *)AddToFreeList(m, malloc(pages)))) return enomem();
  memset(buf, 1, pages);  // bit 0 = resident
  if (CopyToUserWrite(m, vecaddr, buf, pages) == -1) return -1;
  return 0;
#else
  (void)m; (void)addr; (void)length; (void)vecaddr;
  return enosys();
#endif
}

static int SysSyncfs(struct Machine *m, i32 fd) {
#ifdef __linux__
  (void)m;
  return syncfs(fd);
#else
  (void)m; (void)fd;
  return enosys();
#endif
}

static int SysSyncFileRange(struct Machine *m, i32 fd, i64 offset, i64 nbytes,
                            u32 flags) {
#ifdef __linux__
  (void)m;
  return sync_file_range(fd, offset, nbytes, flags);
#else
  (void)m; (void)fd; (void)offset; (void)nbytes; (void)flags;
  return enosys();
#endif
}

static int SysGetcpu(struct Machine *m, i64 cpuaddr, i64 nodeaddr, i64 tcache) {
#ifdef __linux__
  unsigned cpu = 0, node = 0;
  (void)tcache;
  syscall(SYS_getcpu, &cpu, &node, NULL);
  if (cpuaddr) {
    u8 buf[4]; Write32(buf, cpu);
    if (CopyToUserWrite(m, cpuaddr, buf, 4) == -1) return -1;
  }
  if (nodeaddr) {
    u8 buf[4]; Write32(buf, node);
    if (CopyToUserWrite(m, nodeaddr, buf, 4) == -1) return -1;
  }
  return 0;
#else
  (void)m; (void)cpuaddr; (void)nodeaddr; (void)tcache;
  return enosys();
#endif
}

static int SysKcmp(struct Machine *m, i32 pid1, i32 pid2, i32 type, u64 idx1,
                   u64 idx2) {
#ifdef __linux__
  (void)m;
  return (int)syscall(SYS_kcmp, pid1, pid2, type, idx1, idx2);
#else
  (void)m; (void)pid1; (void)pid2; (void)type; (void)idx1; (void)idx2;
  return enosys();
#endif
}

static int SysSetfsuid(struct Machine *m, u32 uid) {
#ifdef __linux__
  (void)m;
  return (int)syscall(SYS_setfsuid, uid);
#else
  (void)m; (void)uid;
  return 0;  // pretend success on non-Linux hosts
#endif
}

static int SysSetfsgid(struct Machine *m, u32 gid) {
#ifdef __linux__
  (void)m;
  return (int)syscall(SYS_setfsgid, gid);
#else
  (void)m; (void)gid;
  return 0;
#endif
}

static int SysIoprioGet(struct Machine *m, i32 which, i32 who) {
#ifdef __linux__
  (void)m;
  return (int)syscall(SYS_ioprio_get, which, who);
#else
  (void)m; (void)which; (void)who;
  return 0;  // IOPRIO_PRIO_VALUE(IOPRIO_CLASS_NONE, 0) — host default
#endif
}

static int SysIoprioSet(struct Machine *m, i32 which, i32 who, i32 ioprio) {
#ifdef __linux__
  (void)m;
  return (int)syscall(SYS_ioprio_set, which, who, ioprio);
#else
  (void)m; (void)which; (void)who; (void)ioprio;
  return 0;
#endif
}

// ----- FreeBSD-only handlers -----------------------------------------------

static int SysFreeBSDPdkill(struct Machine *m, i32 fildes, i32 sig) {
  struct Fd *fd;
  i32 pid;
  int hsig;
  LOCK(&m->system->fds.lock);
  fd = GetFd(&m->system->fds, fildes);
  pid = (fd && fd->pdpid) ? fd->pdpid : 0;
  UNLOCK(&m->system->fds.lock);
  if (!fd) return ebadf();
  if (!pid) return einval();  // not a process descriptor
  if (sig == 0) return kill(pid, 0);
  if ((hsig = XlatSignal(sig)) == -1) return einval();
  return kill(pid, hsig);
}

static int SysFreeBSDKenv(struct Machine *m, i32 action, i64 nameaddr,
                          i64 valueaddr, i32 len) {
  // FreeBSD kenv(2) — action: 0=GET, 1=SET, 2=UNSET, 3=DUMP. We support GET
  // by reading the host environment; everything else fails politely so init
  // scripts can detect and skip.
  if (action != 0) {
    errno = EPERM;
    return -1;
  }
  const char *name = LoadStr(m, nameaddr);
  if (!name) return -1;
  const char *val = getenv(name);
  if (!val) {
    errno = ENOENT;
    return -1;
  }
  size_t vl = strlen(val) + 1;
  if ((i32)vl > len) {
    errno = ENOMEM;
    return -1;
  }
  if (CopyToUserWrite(m, valueaddr, val, vl) == -1) return -1;
  return (int)vl - 1;
}

static int SysFreeBSDSetlogin(struct Machine *m, i64 nameaddr) {
  // Accept and ignore — chrooted code rarely has a real login session.
  (void)m;
  const char *n = LoadStr(m, nameaddr);
  if (!n) return -1;
  return 0;
}

static int SysFreeBSDGetloginclass(struct Machine *m, i64 nameaddr, u64 len) {
  const char *def = "default";
  size_t dl = strlen(def) + 1;
  if (len < dl) {
    errno = ERANGE;
    return -1;
  }
  if (CopyToUserWrite(m, nameaddr, def, dl) == -1) return -1;
  return 0;
}

static int SysFreeBSDSetloginclass(struct Machine *m, i64 nameaddr) {
  (void)m;
  const char *n = LoadStr(m, nameaddr);
  if (!n) return -1;
  return 0;
}

static int SysFreeBSDSetfib(struct Machine *m, i32 fib) {
  (void)m;
  if (fib == 0) return 0;
  errno = EINVAL;  // we only know about the default FIB
  return -1;
}

static int SysSysarch(struct Machine* m, int op, i64 parms) {
  i64 addr;
  const u8* p;
  if (op == 129 || op == 136) {  // AMD64_SET_FSBASE or AMD64_SET_TLSBASE
    if (!(p = (const u8*)SchlepR(m, parms, 8))) return -1;
    addr = Read64(p);
    m->fs.base = addr;
    return 0;
  } else if (op == 128) {  // AMD64_GET_FSBASE
    if (!(p = (const u8*)SchlepR(m, parms, 8))) return -1;
    u8 buf[8];
    Write64(buf, m->fs.base);
    if (CopyToUserWrite(m, parms, buf, 8) == -1) return -1;
    return 0;
  } else if (op == 130) {  // AMD64_GET_GSBASE
    if (!(p = (const u8*)SchlepR(m, parms, 8))) return -1;
    u8 buf[8];
    Write64(buf, m->gs.base);
    if (CopyToUserWrite(m, parms, buf, 8) == -1) return -1;
    return 0;
  } else if (op == 131) {  // AMD64_SET_GSBASE
    if (!(p = (const u8*)SchlepR(m, parms, 8))) return -1;
    addr = Read64(p);
    m->gs.base = addr;
    return 0;
  }
  return 0;
}

static int SysFreeBSDIssetugid(struct Machine* m) {
  return 0;
}

static int SysFreeBSDSigprocmask(struct Machine* m, int how, i64 set,
                                 i64 oset) {
  return SysSigprocmask(m, how - 1, set, oset, 8);
}

// FreeBSD sigpending(sigset_t *set): report signals pending delivery. FreeBSD's
// sigset_t is 16 bytes (4 x u32); blink tracks pending signals in m->signals,
// whose low 8 bytes cover signals 1-64 (FreeBSD only uses 1-32 in practice).
// Mirror SysSigpending() but emit the full 16-byte set with the upper words
// zeroed. Mask bits are treated as bit-compatible with Linux, consistent with
// SysFreeBSDSigprocmask above.
static int SysFreeBSDSigpending(struct Machine *m, i64 setaddr) {
  u8 set[16];
  Write64(set, m->signals);
  Write64(set + 8, 0);
  return CopyToUserWrite(m, setaddr, set, sizeof(set));
}

// FreeBSD sigsuspend takes a 4-byte sigset_t* (not 8-byte like Linux).
// Read 4 bytes and zero-extend to 8 for compatibility.
static int SysFreeBSDSigsuspend(struct Machine* m, i64 maskaddr) {
  u8 word[8];
  if (CopyFromUserRead(m, word, maskaddr, 4) == -1) return -1;
  Write32(word + 4, 0);
  return SigsuspendActual(m, Read64(word));
}

// FreeBSD __realpathat(int fd, const char *path, char *buf, size_t size)
// Resolves path to canonical form. We handle absolute paths directly;
// for relative paths prepend cwd. No symlink resolution (simplified).
static int SysFreeBSDRealpathat(struct Machine *m, i32 dirfd, i64 pathaddr,
                                i64 bufaddr, i64 size) {
  struct stat st;
  const char *path;
  char resolved[PATH_MAX];
  if (!(path = LoadStr(m, pathaddr))) return -1;
  if (VfsStat(GetDirFildes(dirfd), path, &st, 0) == -1) return -1;
  if (path[0] == '/') {
    if ((size_t)snprintf(resolved, sizeof(resolved), "%s", path) >=
        sizeof(resolved)) {
      errno = ENAMETOOLONG;
      return -1;
    }
  } else {
    char cwd[PATH_MAX];
    if (!VfsGetcwd(cwd, sizeof(cwd))) return -1;
    if ((size_t)snprintf(resolved, sizeof(resolved), "%s/%s", cwd, path) >=
        sizeof(resolved)) {
      errno = ENAMETOOLONG;
      return -1;
    }
  }
  size_t len = strlen(resolved);
  if ((u64)len >= (u64)size) {
    errno = ERANGE;
    return -1;
  }
  if (CopyToUserWrite(m, bufaddr, resolved, len + 1) == -1) return -1;
  return 0;
}

// FreeBSD _umtx_op: userspace mutex/condvar/rwlock operations (futex-like).
// In blink's single-threaded emulation, we stub most operations.
static int SysFreeBSD_umtx_op(struct Machine *m, i64 obj, int op, u64 val,
                               i64 uaddr1, i64 uaddr2) {
  if (m->killed) return eintr();
  // Op numbers from FreeBSD sys/umtx.h:
  //  0=LOCK  1=UNLOCK  2=WAIT  3=WAKE  4=MUTEX_TRYLOCK  5=MUTEX_LOCK
  //  6=MUTEX_UNLOCK  7=SET_CEILING  8=CV_WAIT  9=CV_SIGNAL  10=CV_BROADCAST
  // 11=WAIT_UINT  12=RW_RDLOCK  13=RW_WRLOCK  14=RW_UNLOCK
  // 15=WAIT_UINT_PRIVATE  16=WAKE_PRIVATE  17=MUTEX_WAIT  18=MUTEX_WAKE
  // 19=SEM_WAIT  20=SEM_WAKE  21=NWAKE_PRIVATE  22=MUTEX_WAKE2
  // 23=SEM2_WAIT  24=SEM2_WAKE  25=SHM  26=ROBUST_LISTS
  switch (op) {
    case 2:   // UMTX_OP_WAIT
    case 11:  // UMTX_OP_WAIT_UINT
    case 15: { // UMTX_OP_WAIT_UINT_PRIVATE
      u8 *mem = LookupAddress(m, obj);
      if (!mem) return efault();
      u32 curval = Load32(mem);
      // FreeBSD: when *addr != val the kernel does NOT sleep and returns 0
      // (success), unlike Linux futex which returns EAGAIN. Go's
      // futexsleep1 deliberately crashes on any unexpected error, so we
      // must return 0 here rather than EAGAIN.
      if (curval != (u32)val) return 0;
      if (IsOrphan(m)) { Store32(mem, 0); return 0; }
      int rc = SysFutexWait(m, obj, FUTEX_WAIT_LINUX, val, 0);
      // Same rationale for the TOCTOU re-check inside SysFutexWait.
      if (rc == -1 && errno == EAGAIN) return 0;
      return rc;
    }
    case 3:   // UMTX_OP_WAKE
    case 16:  // UMTX_OP_WAKE_PRIVATE
      return SysFutexWake(m, obj, val ? val : 1);
    case 4: { // UMTX_OP_MUTEX_TRYLOCK
      u8 *mem = LookupAddress(m, obj);
      if (!mem) return efault();
      if ((Load32(mem) & 0x7fffffffu) == 0) return 0;
      return -(EBUSY_LINUX);
    }
    case 5:   // UMTX_OP_MUTEX_LOCK
    case 17: { // UMTX_OP_MUTEX_WAIT
      // FreeBSD mutex word: low 30 bits = owner TID, bit 31 = CONTESTED
      u8 *mem = LookupAddress(m, obj);
      if (!mem) return efault();
      u32 curval = Load32(mem);
      if ((curval & 0x7fffffffu) == 0) return 0;  // unowned
      if (IsOrphan(m)) { Store32(mem, 0); return 0; }
      // Set CONTESTED bit so owner calls MUTEX_WAKE2 on unlock
      u32 contested = curval | 0x80000000u;
      if (contested != curval) {
        _Atomic(u32) *atom = (_Atomic(u32) *)mem;
        atomic_compare_exchange_strong_explicit(atom, &curval, contested,
                                                memory_order_acq_rel,
                                                memory_order_acquire);
        if ((curval & 0x7fffffffu) == 0) return 0;  // became free during CAS
        contested = curval | 0x80000000u;
      }
      // Sleep until the mutex word changes (owner unlocks)
      return SysFutexWait(m, obj,
                          FUTEX_WAIT_LINUX | FUTEX_PRIVATE_FLAG_LINUX,
                          contested, uaddr2);
    }
    case 1:   // UMTX_OP_UNLOCK (deprecated)
    case 6:   // UMTX_OP_MUTEX_UNLOCK
      // Userspace already cleared m_owner via CAS; just wake a waiter
      return SysFutexWake(m, obj, 1);
    case 7:   // UMTX_OP_SET_CEILING
      return 0;
    case 8: { // UMTX_OP_CV_WAIT
      // obj = ucond*, val = flags/clock_id, uaddr1 = umutex*, uaddr2 = timespec*
      // libthr unlocks the mutex in userspace before calling this; we just
      // need to sleep on the ucond until CV_SIGNAL/CV_BROADCAST wakes us.
      //
      // The FreeBSD kernel ALWAYS sets ucond->c_has_waiters = 1 and sleeps;
      // we must do the same. An earlier version returned 0 immediately when
      // it observed c_has_waiters == 0, but that's a spurious wake from
      // libthr's POV: libthr left the thread on its userspace sleepq, the
      // caller loops back into pthread_cond_wait, and the second sleepq_add
      // panics with "thread %p was already on queue" (libthr thr_cond.c).
      // LibreOffice's Qt thread pool tripped this within ~20s of startup.
      u8 *cv_mem = LookupAddress(m, obj);
      if (!cv_mem) return efault();
      _Atomic(u32) *atom = (_Atomic(u32) *)cv_mem;
      atomic_store_explicit(atom, 1, memory_order_release);
      if (IsOrphan(m)) return 0;
      return SysFutexWait(m, obj, FUTEX_WAIT_LINUX, 1, uaddr2);
    }
    case 9:   // UMTX_OP_CV_SIGNAL
      return SysFutexWake(m, obj, 1);
    case 10:  // UMTX_OP_CV_BROADCAST
      return SysFutexWake(m, obj, INT_MAX);
    case 12: { // UMTX_OP_RW_RDLOCK
      // FreeBSD kernel loops internally: try CAS to acquire, sleep if busy
      u8 *mem = LookupAddress(m, obj);
      if (!mem) return efault();
      _Atomic(u32) *atom = (_Atomic(u32) *)mem;
      for (;;) {
        u32 state = atomic_load_explicit(atom, memory_order_acquire);
        // Try to acquire read lock if no write owner and no write waiters
        while (!(state & 0x80000000u) &&
               !(state & 0x40000000u)) {
          u32 nstate = state + 1;  // increment reader count
          if (atomic_compare_exchange_weak_explicit(
                  atom, &state, nstate,
                  memory_order_acq_rel, memory_order_acquire)) {
            return 0;  // acquired!
          }
        }
        if (IsOrphan(m)) return 0;
        int rc = SysFutexWait(m, obj,
                              FUTEX_WAIT_LINUX | FUTEX_PRIVATE_FLAG_LINUX,
                              state, uaddr2);
        if (rc == -1 && errno == EINTR) return eintr();
      }
    }
    case 13: { // UMTX_OP_RW_WRLOCK
      // FreeBSD kernel loops internally: try CAS to acquire, sleep if busy
      u8 *mem = LookupAddress(m, obj);
      if (!mem) return efault();
      _Atomic(u32) *atom = (_Atomic(u32) *)mem;
      for (;;) {
        u32 state = atomic_load_explicit(atom, memory_order_acquire);
        // Try to acquire write lock if no readers and no write owner
        while (!(state & 0x80000000u) &&
               (state & 0x1fffffffu) == 0) {
          u32 nstate = state | 0x80000000u;  // set WRITE_OWNER
          if (atomic_compare_exchange_weak_explicit(
                  atom, &state, nstate,
                  memory_order_acq_rel, memory_order_acquire)) {
            return 0;  // acquired!
          }
          // CAS failed, state updated → retry inner loop
        }
        if (IsOrphan(m)) return 0;
        // Couldn't acquire; wait for state to change
        int rc = SysFutexWait(m, obj,
                              FUTEX_WAIT_LINUX | FUTEX_PRIVATE_FLAG_LINUX,
                              state, uaddr2);
        if (rc == -1 && errno == EINTR) return eintr();
      }
    }
    case 14: { // UMTX_OP_RW_UNLOCK
      // Wake all waiters (readers and writers)
      SysFutexWake(m, obj, INT_MAX);
      return 0;
    }
    case 18:  // UMTX_OP_MUTEX_WAKE (deprecated)
    case 22:  // UMTX_OP_MUTEX_WAKE2
      return SysFutexWake(m, obj, val ? val : 1);
    case 19:  // UMTX_OP_SEM_WAIT (deprecated)
    case 23:  // UMTX_OP_SEM2_WAIT
      return SysFutexWait(m, obj, FUTEX_WAIT_LINUX, 0, 0);
    case 20:  // UMTX_OP_SEM_WAKE (deprecated)
    case 24:  // UMTX_OP_SEM2_WAKE
      return SysFutexWake(m, obj, val ? val : INT_MAX);
    case 21: { // UMTX_OP_NWAKE_PRIVATE
      // obj = pointer to array of addresses, val = count
      // Wake one waiter at each address in the array
      int count = val ? (int)val : 0;
      for (int i = 0; i < count; i++) {
        u8 *p = LookupAddress(m, obj + i * 8);
        if (!p) continue;
        i64 addr = Load64(p);
        if (addr) SysFutexWake(m, addr, INT_MAX);
      }
      return 0;
    }
    case 25:  // UMTX_OP_SHM
    case 26:  // UMTX_OP_ROBUST_LISTS
      return 0;
    default:
      return 0;
  }
}

// FreeBSD thr_new(struct thr_param *param, int param_size)
// Creates a new thread, similar to Linux clone() with CLONE_THREAD.
// struct thr_param layout (104 bytes):
//   0: void (*start_func)(void*)  8 bytes
//   8: void *arg                  8 bytes
//  16: char *stack_base           8 bytes
//  24: size_t stack_size          8 bytes
//  32: char *tls_base             8 bytes
//  40: size_t tls_size            8 bytes
//  48: long *child_tid            8 bytes
//  56: long *parent_tid           8 bytes
//  64: int flags                  4 bytes
//  68: struct rtprio *rtp         8 bytes (with padding)
// Allocate a page in guest memory containing a thr_exit(0) thunk.
// This is used as the return address for threads created by thr_new,
// so that when start_func returns, the thread cleanly calls thr_exit(0)
// instead of jumping to address 0.
static i64 GetFreeBSDThrExitThunk(struct Machine *m) {
  i64 thunk;
  u8 *p;
  if (m->system->thr_exit_thunk) return m->system->thr_exit_thunk;
  // x86-64 code: xor %edi,%edi; mov $431,%eax; syscall; ud2
  // 31 ff  b8 af 01 00 00  0f 05  0f 0b
  static const u8 code[] = {
      0x31, 0xff,                    // xor %edi, %edi
      0xb8, 0xaf, 0x01, 0x00, 0x00,  // mov $431, %eax  (FreeBSD thr_exit)
      0x0f, 0x05,                    // syscall
      0x0f, 0x0b,                    // ud2 (should never reach here)
  };
  thunk = ReserveVirtual(m->system, 0, 0x1000,
                         PAGE_U, -1, 0, false, false);
  if (thunk == -1) return 0;
  p = (u8 *)LookupAddress(m, thunk);
  if (!p) return 0;
  memcpy(p, code, sizeof(code));
  m->system->thr_exit_thunk = thunk;
  return thunk;
}

static int SysFreeBSDThrNew(struct Machine *m) {
#ifdef HAVE_THREADS
  i64 param_addr = Get64(m->di);
  u8 *param;
  int err, tid;
  pthread_t thread;
  pthread_attr_t attr;
  sigset_t ss, oldss;
  struct Machine *m2;
  if (!(param = (u8 *)SchlepR(m, param_addr, 76)))
    return efault();
  u64 start_func = Read64(param + 0);
  u64 arg        = Read64(param + 8);
  u64 stack_base = Read64(param + 16);
  u64 stack_size = Read64(param + 24);
  u64 tls_base   = Read64(param + 32);
  u64 child_tid  = Read64(param + 48);
  u64 parent_tid = Read64(param + 56);
  SYS_LOGF("thr_new: start=%#" PRIx64 " arg=%#" PRIx64
           " stack=%#" PRIx64 "+%#" PRIx64 " tls=%#" PRIx64,
           start_func, arg, stack_base, stack_size, tls_base);
  m->threaded = true;
  m->system->jit.threaded = true;
  if (!(m2 = NewMachine(m->system, m)))
    return eagain();
  sigfillset(&ss);
  unassert(!pthread_sigmask(SIG_SETMASK, &ss, &oldss));
  tid = m2->tid;
  // Set up the new thread's registers
  m2->ip = start_func;      // start executing at start_func
  Put64(m2->di, arg);        // first argument
  Put64(m2->ax, 0);
  // Lay out the top of the child's g0 stack. Everything we write MUST stay
  // within [stack_base, stack_base+stack_size): the thr_exit thunk and the
  // return-address slot live just below the (16-aligned) stack top, NOT at it.
  // An earlier version put the 11-byte thunk at `top` itself (thunk_addr =
  // sp + 8 where sp = top - 8), writing up to ~11 bytes PAST stack.hi into
  // whatever guest memory happened to be adjacent (often another M's data or
  // a sudog) — silently corrupting the Go runtime on EVERY thread creation.
  // That was the long-standing "schedule: holding locks" / "sudog with non-nil
  // next" SMP bug. Linux clone() writes no such thunk, which is why only
  // FreeBSD Go programs hit it. See memory: go_freebsd_support.md.
  //
  //   top        = (stack_base+stack_size) & ~15   (16-aligned, == stack.hi)
  //   thunk_addr = top - 16   -> thunk occupies [top-16, top-6)  (in-bounds)
  //   sp         = top - 24   -> return slot [top-24, top-16); sp%16 == 8
  // When start_func returns it pops thunk_addr and jumps to the thunk, which
  // cleanly calls thr_exit(0) instead of returning to address 0.
  u64 top = (stack_base + stack_size) & ~(u64)15;
  u64 thunk_addr = top - 16;
  u64 sp = top - 24;  // ABI: (RSP%16)==8 at the callee's first instruction
  Put64(m2->sp, sp);
  {
    // x86-64 code: xor %edi,%edi; mov $431,%eax; syscall; ud2
    static const u8 code[] = {
        0x31, 0xff,                    // xor %edi, %edi
        0xb8, 0xaf, 0x01, 0x00, 0x00,  // mov $431, %eax (FreeBSD thr_exit)
        0x0f, 0x05,                    // syscall
        0x0f, 0x0b,                    // ud2
    };
    u8 *thunk_mem = (u8 *)LookupAddress(m, thunk_addr);
    u8 *ret_mem = (u8 *)LookupAddress(m, sp);
    if (thunk_mem && ret_mem) {
      memcpy(thunk_mem, code, sizeof(code));
      Write64(ret_mem, thunk_addr);
    }
  }
  m2->fs.base = tls_base;   // TLS
  m2->nojit = true;         // disable JIT for FreeBSD child threads
  if (child_tid) {
    // Write child TID (FreeBSD uses long = 8 bytes)
    u8 *ctid_ptr;
    if ((ctid_ptr = (u8 *)LookupAddress(m, child_tid))) {
      Write64(ctid_ptr, tid);
    }
    m2->ctid = child_tid;  // for CHILD_CLEARTID on exit
  }
  if (parent_tid) {
    u8 *ptid_ptr;
    if ((ptid_ptr = (u8 *)LookupAddress(m, parent_tid))) {
      Write64(ptid_ptr, tid);
    }
  }
  m2->spawn_sigmask = oldss;
  unassert(!pthread_attr_init(&attr));
  unassert(!pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED));
  err = pthread_create(&thread, &attr, OnSpawn, m2);
  unassert(!pthread_attr_destroy(&attr));
  if (err) {
    FreeMachine(m2);
    unassert(!pthread_sigmask(SIG_SETMASK, &oldss, 0));
    return eagain();
  }
  unassert(!pthread_sigmask(SIG_SETMASK, &oldss, 0));
  return 0;
#else
  return enosys();
#endif
}

// FreeBSD thr_exit(long *state): exit thread, signaling TID_TERMINATED.
// The FreeBSD kernel stores TID_TERMINATED (1) at *state via suword_lwpid
// (a 32-bit write), then wakes futex waiters.  _pthread_join spins until
// thread->tid == TID_TERMINATED (1).
static _Noreturn void SysFreeBSDThrExit(struct Machine *m) {
  i64 state_addr = Get64(m->di);
  if (state_addr) {
    u8 *state = LookupAddress(m, state_addr);
    if (state) {
      Store32(state, 1);  // TID_TERMINATED = 1 (suword_lwpid writes 32 bits)
    }
    SysFutexWake(m, state_addr, INT_MAX);
  }
  // Clear ctid so ClearChildTid() in SysExit doesn't overwrite
  // the TID_TERMINATED value we just stored.
  m->ctid = 0;
  SysExit(m, 0);
}

// FreeBSD thr_wake(long id): wake a thread. Stub — return 0.
static int SysFreeBSDThrWake(struct Machine *m, long id) {
  return 0;
}

// FreeBSD getcontext/rtprio_thread: stub
static int SysFreeBSDStub0(struct Machine *m) {
  return 0;
}

// getdtablesize() — max number of open file descriptors. Real FreeBSD reads
// kern.maxfilesperproc; we mirror Linux's getrlimit(RLIMIT_NOFILE) here.
static int SysFreeBSDGetdtablesize(struct Machine *m) {
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) == 0) return (int)rl.rlim_cur;
  return 1024;
}

// FreeBSD audit syscalls (auditon, getauid, etc.) — return 0/success quietly.
// Most callers (su, login) treat audit absence as benign.
static int SysFreeBSDAuditStub(struct Machine *m) {
  return 0;
}

// FreeBSD getpriority(which, who) — return 0 (normal priority). Mapping to
// Linux getpriority(2) would work but Linux returns `20 - nice` and FreeBSD
// returns `nice` directly, so semantics differ. Stub to 0 is safe.
static int SysFreeBSDGetpriority(struct Machine *m, int which, int who) {
  return 0;
}

// FreeBSD setpriority(which, who, prio) — accept silently.
static int SysFreeBSDSetpriority(struct Machine *m, int which, int who,
                                 int prio) {
  return 0;
}

// FreeBSD futimens(fd, times) → utimensat(fd, NULL, times, 0)
static int SysFreeBSDFutimens(struct Machine *m, i32 fd, i64 tvsaddr) {
  return SysUtimensat(m, fd, 0, tvsaddr, 0);
}

// FreeBSD cpuset_getaffinity: report a single CPU available.
// blink's multi-threaded SMP emulation intermittently corrupts guest runtime
// state once a program (e.g. Go) schedules across more than one CPU, so we
// pin guests to one CPU until that cross-thread coherence bug is fixed.
static int SysFreeBSDCpusetGetaffinity(struct Machine *m) {
  i64 setsize = Get64(m->r10);
  i64 mask_addr = Get64(m->r8);
  u8 *mask;
  if (setsize <= 0 || !mask_addr) return einval();
  if (!(mask = (u8 *)SchlepW(m, mask_addr, setsize))) return -1;
  memset(mask, 0, setsize);
  {
    int ncpus = GetCpuCount();
    if (ncpus < 1) ncpus = 1;
    for (int i = 0; i < ncpus && i < (int)(setsize * 8); ++i) {
      mask[i / 8] |= 1 << (i % 8);
    }
  }
  return 0;
}

static int SysFreeBSDSysarch(struct Machine* m, int op, i64 parms) {
  SYS_LOGF("sysarch(%d, %#" PRIx64 ")", op, parms);
  return SysSysarch(m, op, parms);
}

#ifdef HAVE_EPOLL_PWAIT1

static i32 SysEpollCreate1(struct Machine *m, i32 flags) {
  int lim, fildes, oflags, sysflags;
  oflags = 0;
  sysflags = 0;
  if (flags & EPOLL_CLOEXEC_LINUX) {
    oflags |= O_CLOEXEC;
    sysflags |= EPOLL_CLOEXEC;
    flags &= ~EPOLL_CLOEXEC_LINUX;
  }
  if (flags) {
    LOGF("unsupported %s flags: %#x", "epoll_create1", flags);
    return einval();
  }
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  if ((fildes = epoll_create1(sysflags)) != -1) {
    if (fildes >= lim) {
      close(fildes);
      fildes = emfile();
    } else {
      LOCK(&m->system->fds.lock);
      unassert(AddFd(&m->system->fds, fildes, oflags));
      UNLOCK(&m->system->fds.lock);
    }
  }
  return fildes;
}

static i32 SysEpollCreate(struct Machine *m, i32 size) {
  if (size <= 0) return einval();
  return SysEpollCreate1(m, 0);
}

static i32 SysEpollCtl(struct Machine *m, i32 epfd, i32 op, i32 fd,
                       i64 eventaddr) {
  struct epoll_event epe, *pepe;
  const struct epoll_event_linux *gepe;
  switch (op) {
    case EPOLL_CTL_DEL_LINUX:
      pepe = 0;
      break;
    case EPOLL_CTL_ADD_LINUX:
    case EPOLL_CTL_MOD_LINUX:
      if (!(gepe = (const struct epoll_event_linux *)SchlepR(m, eventaddr,
                                                             sizeof(*gepe)))) {
        return -1;
      }
      epe.events = Read32(gepe->events);
      epe.data.u64 = Read64(gepe->data);
      pepe = &epe;
      break;
    default:
      return einval();
  }
  return epoll_ctl(epfd, op, fd, pepe);
}

static i32 EpollPwait(struct Machine *m, i32 epfd, i64 eventsaddr,
                      i32 maxevents, struct timespec deadline, i64 sigmaskaddr,
                      u64 sigsetsize) {
  i32 i, rc;
  u64 oldmask_guest = 0;
  sigset_t block, oldmask;
  struct epoll_event *events;
  struct timespec now, waitfor;
  struct epoll_event_linux *gevents;
  const struct sigset_linux *sigmaskp_guest = 0;
  if (maxevents <= 0) return einval();
  if (sigmaskaddr) {
    if (sigsetsize != 8) return einval();
    if (!(sigmaskp_guest = (const struct sigset_linux *)SchlepR(
              m, sigmaskaddr, sizeof(*sigmaskp_guest)))) {
      return -1;
    }
  }
  if (!IsValidMemory(m, eventsaddr,
                     maxevents * sizeof(struct epoll_event_linux),
                     PROT_WRITE) ||
      !(events = (struct epoll_event *)AddToFreeList(
            m, calloc(maxevents, sizeof(struct epoll_event)))) ||
      !(gevents = (struct epoll_event_linux *)AddToFreeList(
            m, calloc(maxevents, sizeof(struct epoll_event_linux))))) {
    return -1;
  }
  unassert(!sigfillset(&block));
  unassert(!pthread_sigmask(SIG_BLOCK, &block, &oldmask));
  if (sigmaskp_guest) {
    oldmask_guest = m->sigmask;
    m->sigmask = Read64(sigmaskp_guest->sigmask);
    SIG_LOGF("sigmask push %" PRIx64, m->sigmask);
  }
  if (!CheckInterrupt(m, false)) {
    do {
      now = GetTime();
      if (CompareTime(now, deadline) < 0) {
        waitfor = SubtractTime(deadline, now);
      } else {
        waitfor = GetZeroTime();
      }
#if defined(HAVE_EPOLL_PWAIT2) && !defined(MUSL_CROSS_MAKE)
      rc = epoll_pwait2(epfd, events, maxevents, &waitfor, &oldmask);
#else
      rc = epoll_pwait(epfd, events, maxevents,
                       ConvertTimeToInt(ToMilliseconds(waitfor)), &oldmask);
#endif
      if (rc == -1 && errno == EINTR) {
        if (CheckInterrupt(m, false)) {
          break;
        }
      } else {
        break;
      }
    } while (1);
  } else {
    rc = -1;
  }
  if (sigmaskp_guest) {
    m->sigmask = oldmask_guest;
    SIG_LOGF("sigmask pop %" PRIx64, m->sigmask);
  }
  unassert(!pthread_sigmask(SIG_SETMASK, &oldmask, 0));
  if (rc != -1) {
    for (i = 0; i < rc; ++i) {
      Write32(gevents[i].events, events[i].events);
      Write64(gevents[i].data, events[i].data.u64);
    }
    unassert(!CopyToUserWrite(m, eventsaddr, gevents,
                              rc * sizeof(struct epoll_event_linux)));
  }
  return rc;
}

static i32 SysEpollPwait(struct Machine *m, i32 epfd, i64 eventsaddr,
                         i32 maxevents, i32 timeout, i64 sigmaskaddr,
                         u64 sigsetsize) {
  struct timespec deadline;
  if (timeout >= 0) {
    deadline = AddTime(GetTime(), FromMilliseconds(timeout));
  } else {
    deadline = GetMaxTime();
  }
  return EpollPwait(m, epfd, eventsaddr, maxevents, deadline, sigmaskaddr,
                    sigsetsize);
}

static i32 SysEpollPwait2(struct Machine *m, i32 epfd, i64 eventsaddr,
                          i32 maxevents, i64 timeoutaddr, i64 sigmaskaddr,
                          u64 sigsetsize) {
  struct timespec ts, deadline;
  if (timeoutaddr) {
    if (LoadTimespecR(m, timeoutaddr, &ts) == -1) return -1;
    deadline = AddTime(GetTime(), ts);
  } else {
    deadline = GetMaxTime();
  }
  return EpollPwait(m, epfd, eventsaddr, maxevents, deadline, sigmaskaddr,
                    sigsetsize);
}

static int SysEpollWait(struct Machine *m, i32 epfd, i64 eventsaddr,
                        i32 maxevents, i32 timeout) {
  return SysEpollPwait(m, epfd, eventsaddr, maxevents, timeout, 0, 8);
}

#endif /* HAVE_EPOLL_PWAIT1 */

#ifdef HAVE_EPOLL_PWAIT1
////////////////////////////////////////////////////////////////////////////////
// FreeBSD kqueue/kevent → Linux epoll translation

#define KQ_MAX_INSTANCES 32
#define KQ_MAX_WATCHES   1024

// FreeBSD kevent constants
#define EVFILT_READ_    (-1)
#define EVFILT_WRITE_   (-2)
#define EVFILT_TIMER_   (-7)
#define EVFILT_USER_    (-11)
#define EV_ADD_         0x0001
#define EV_DELETE_      0x0002
#define EV_ENABLE_      0x0004
#define EV_DISABLE_     0x0008
#define EV_ONESHOT_     0x0010
#define EV_CLEAR_       0x0020
#define EV_RECEIPT_     0x0040
#define EV_DISPATCH_    0x0080
#define EV_EOF_         0x8000
#define EV_ERROR_       0x4000

struct KqWatch {
  u64 ident;
  i16 filter;
  u64 udata;
  bool active;
  bool clear;  // EV_CLEAR requested → edge-triggered (EPOLLET)
};

struct KqState {
  int epfd;
  pthread_mutex_t_ lock;
  int nwatches;
  struct KqWatch watches[KQ_MAX_WATCHES];
};

static struct KqState *g_kqueues[KQ_MAX_INSTANCES];
static pthread_mutex_t_ g_kqueues_lock = PTHREAD_MUTEX_INITIALIZER;

static struct KqState *GetKqState(int epfd) {
  struct KqState *kq = NULL;
  LOCK(&g_kqueues_lock);
  for (int i = 0; i < KQ_MAX_INSTANCES; i++) {
    if (g_kqueues[i] && g_kqueues[i]->epfd == epfd) {
      kq = g_kqueues[i];
      break;
    }
  }
  UNLOCK(&g_kqueues_lock);
  return kq;
}

static struct KqWatch *FindWatch(struct KqState *kq, u64 ident, i16 filter) {
  for (int i = 0; i < kq->nwatches; i++) {
    if (kq->watches[i].active && kq->watches[i].ident == ident &&
        kq->watches[i].filter == filter) {
      return &kq->watches[i];
    }
  }
  return NULL;
}

static u64 FindUdata(struct KqState *kq, u64 ident, i16 filter) {
  struct KqWatch *w = FindWatch(kq, ident, filter);
  return w ? w->udata : 0;
}

// Get combined epoll events for an fd (READ + WRITE). EV_CLEAR on any active
// watch maps to EPOLLET so the fd is edge-triggered like kqueue — otherwise a
// level-triggered epoll re-reports readiness forever and the guest busy-loops.
static u32 GetCombinedEpollEvents(struct KqState *kq, u64 ident) {
  u32 events = 0;
  for (int i = 0; i < kq->nwatches; i++) {
    if (kq->watches[i].active && kq->watches[i].ident == ident) {
      if (kq->watches[i].filter == EVFILT_READ_) events |= EPOLLIN;
      if (kq->watches[i].filter == EVFILT_WRITE_) events |= EPOLLOUT;
      if (kq->watches[i].clear) events |= EPOLLET;
    }
  }
  return events;
}

static i32 SysFreeBSDKqueue(struct Machine *m) {
  int lim, epfd;
  if (!(lim = GetFileDescriptorLimit(m->system))) return emfile();
  if ((epfd = epoll_create1(EPOLL_CLOEXEC)) == -1) return -1;
  if (epfd >= lim) {
    close(epfd);
    return emfile();
  }
  struct KqState *kq = (struct KqState *)calloc(1, sizeof(struct KqState));
  if (!kq) {
    close(epfd);
    return enomem();
  }
  kq->epfd = epfd;
  pthread_mutex_init(&kq->lock, NULL);
  LOCK(&g_kqueues_lock);
  int slot = -1;
  for (int i = 0; i < KQ_MAX_INSTANCES; i++) {
    if (!g_kqueues[i]) {
      slot = i;
      break;
    }
  }
  if (slot >= 0) {
    g_kqueues[slot] = kq;
  }
  UNLOCK(&g_kqueues_lock);
  if (slot < 0) {
    close(epfd);
    free(kq);
    return enomem();
  }
  LOCK(&m->system->fds.lock);
  unassert(AddFd(&m->system->fds, epfd, O_CLOEXEC));
  UNLOCK(&m->system->fds.lock);
  return epfd;
}

static int KqProcessChange(struct KqState *kq, u64 ident, i16 filter,
                           u16 flags, u32 fflags, u64 udata) {
  if (filter != EVFILT_READ_ && filter != EVFILT_WRITE_ &&
      filter != EVFILT_USER_ && filter != EVFILT_TIMER_) {
    // Silently ignore unsupported filters
    return 0;
  }
  // EVFILT_USER and EVFILT_TIMER: stub (return success, don't register)
  if (filter == EVFILT_USER_ || filter == EVFILT_TIMER_) {
    if (flags & EV_ADD_) {
      // find or create watch slot for udata tracking
      LOCK(&kq->lock);
      struct KqWatch *w = FindWatch(kq, ident, filter);
      if (!w && kq->nwatches < KQ_MAX_WATCHES) {
        w = &kq->watches[kq->nwatches++];
        w->ident = ident;
        w->filter = filter;
        w->active = true;
      }
      if (w) w->udata = udata;
      UNLOCK(&kq->lock);
    }
    return 0;
  }
  LOCK(&kq->lock);
  struct KqWatch *w = FindWatch(kq, ident, filter);
  if (flags & EV_ADD_) {
    if (!w) {
      if (kq->nwatches >= KQ_MAX_WATCHES) {
        UNLOCK(&kq->lock);
        return enomem();
      }
      w = &kq->watches[kq->nwatches++];
      w->ident = ident;
      w->filter = filter;
      w->active = true;
    }
    w->udata = udata;
    w->clear = (flags & EV_CLEAR_) != 0;
    // Update epoll registration with combined events for this fd
    u32 events = GetCombinedEpollEvents(kq, ident);
    struct epoll_event ev = {0};
    ev.events = events;
    ev.data.u64 = ident;
    // Try MOD first, then ADD
    if (epoll_ctl(kq->epfd, EPOLL_CTL_MOD, (int)ident, &ev) == -1) {
      if (errno == ENOENT) {
        epoll_ctl(kq->epfd, EPOLL_CTL_ADD, (int)ident, &ev);
      }
    }
  } else if (flags & EV_DELETE_) {
    if (w) {
      w->active = false;
      // Update epoll — may need MOD if other filter still active, or DEL
      u32 remaining = GetCombinedEpollEvents(kq, ident);
      if (remaining) {
        struct epoll_event ev = {0};
        ev.events = remaining;
        ev.data.u64 = ident;
        epoll_ctl(kq->epfd, EPOLL_CTL_MOD, (int)ident, &ev);
      } else {
        epoll_ctl(kq->epfd, EPOLL_CTL_DEL, (int)ident, NULL);
      }
    }
  }
  UNLOCK(&kq->lock);
  return 0;
}

// FreeBSD kevent(kq, changelist, nchanges, eventlist, nevents, timeout)
// struct kevent (modern, 64 bytes): [ident:8][filter:2][flags:2][fflags:4][data:8][udata:8][ext:32]
// struct kevent11 (old, 32 bytes): [ident:8][filter:2][flags:2][fflags:4][data:8][udata:8]
static i32 SysFreeBSDKevent(struct Machine *m, i32 kq_fd, i64 changelist_addr,
                            i32 nchanges, i64 eventlist_addr, i32 nevents,
                            i64 timeout_addr, int kev_size) {
  struct KqState *kq = GetKqState(kq_fd);
  if (!kq) return ebadf();
  // Process changelist
  for (int i = 0; i < nchanges; i++) {
    u8 kev[64];
    if (CopyFromUserRead(m, kev, changelist_addr + (i64)i * kev_size,
                         MIN(kev_size, 32)) == -1)
      return -1;
    u64 ident = Read64(kev + 0);
    i16 filter = (i16)Read16(kev + 8);
    u16 flags = Read16(kev + 10);
    u32 fflags = Read32(kev + 12);
    u64 udata = Read64(kev + 24);
      int rc = KqProcessChange(kq, ident, filter, flags, fflags, udata);
    // EV_RECEIPT: return result in eventlist
    if ((flags & EV_RECEIPT_) && eventlist_addr && nevents > 0) {
      u8 out[64] = {0};
      Write64(out + 0, ident);
      Write16(out + 8, (u16)filter);
      Write16(out + 10, EV_ERROR_);
      Write32(out + 12, rc < 0 ? errno : 0);
      Write64(out + 16, 0);
      Write64(out + 24, udata);
      CopyToUserWrite(m, eventlist_addr, out, kev_size);
      eventlist_addr += kev_size;
      nevents--;
    } else if (rc < 0) {
      return -1;
    }
  }
  // Wait for events
  if (nevents <= 0 || !eventlist_addr) return 0;
  int timeout_ms;
  if (timeout_addr) {
    u8 ts[16];
    if (CopyFromUserRead(m, ts, timeout_addr, 16) == -1) return -1;
    i64 sec = (i64)Read64(ts + 0);
    i64 nsec = (i64)Read64(ts + 8);
    timeout_ms = (int)(sec * 1000 + nsec / 1000000);
    if (sec == 0 && nsec == 0) timeout_ms = 0;
  } else {
    timeout_ms = -1;  // block forever
  }
  int maxep = MIN(nevents, 64);
  struct epoll_event epevents[64];
  int n = epoll_wait(kq->epfd, epevents, maxep, timeout_ms);
  if (n <= 0) return n;
  // Convert epoll events to kevents — may produce multiple kevents per epoll event
  int nout = 0;
  for (int i = 0; i < n && nout < nevents; i++) {
    u64 fd = epevents[i].data.u64;
    u32 ev = epevents[i].events;
    LOCK(&kq->lock);
    if ((ev & (EPOLLIN | EPOLLHUP | EPOLLERR)) && nout < nevents) {
      struct KqWatch *w = FindWatch(kq, fd, EVFILT_READ_);
      if (w) {
        u8 kev[64] = {0};
        Write64(kev + 0, fd);
        Write16(kev + 8, (u16)(i16)EVFILT_READ_);
        if (ev & EPOLLHUP) Write16(kev + 10, EV_EOF_);
        // kev.data = bytes available to read. nginx (and others) use this
        // under kqueue to size reads and track ready state; a missing value
        // makes them stop reading. FIONREAD works for sockets and pipes.
        int navail = 0;
        if (ioctl((int)fd, FIONREAD, &navail) == -1) navail = 0;
        Write64(kev + 16, (u64)(i64)navail);
        Write64(kev + 24, w->udata);
        CopyToUserWrite(m, eventlist_addr + (i64)nout * kev_size, kev, kev_size);
        nout++;
      }
    }
    if ((ev & (EPOLLOUT | EPOLLHUP | EPOLLERR)) && nout < nevents) {
      struct KqWatch *w = FindWatch(kq, fd, EVFILT_WRITE_);
      if (w) {
        u8 kev[64] = {0};
        Write64(kev + 0, fd);
        Write16(kev + 8, (u16)(i16)EVFILT_WRITE_);
        if (ev & EPOLLHUP) Write16(kev + 10, EV_EOF_);
        // kev.data = free space in the send buffer for write events.
        int sndbuf = 0;
        socklen_t optlen = sizeof(sndbuf);
        if (getsockopt((int)fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, &optlen) == -1) {
          sndbuf = 0;
        }
        Write64(kev + 16, (u64)(i64)sndbuf);
        Write64(kev + 24, w->udata);
        CopyToUserWrite(m, eventlist_addr + (i64)nout * kev_size, kev, kev_size);
        nout++;
      }
    }
    UNLOCK(&kq->lock);
  }
  return nout;
}

////////////////////////////////////////////////////////////////////////////////
#endif /* HAVE_EPOLL_PWAIT1 (kqueue) */

static void WriteFreeBSDStat(struct Machine* m, i64 addr, struct stat* st) {
  u8 b[144];
  memset(b, 0, sizeof(b));
  Write64(b + 0, st->st_dev);
  Write64(b + 8, st->st_ino);
  Write64(b + 16, st->st_nlink);
  Write16(b + 24, st->st_mode);
  Write32(b + 28, m->system->emulate_root ? 0 : st->st_uid);
  Write32(b + 32, m->system->emulate_root ? 0 : st->st_gid);
  Write64(b + 40, st->st_rdev);
  Write64(b + 48, st->st_atime);
#ifdef __linux__
  Write64(b + 56, st->st_atim.tv_nsec);
  Write64(b + 64, st->st_mtime);
  Write64(b + 72, st->st_mtim.tv_nsec);
  Write64(b + 80, st->st_ctime);
  Write64(b + 88, st->st_ctim.tv_nsec);
  Write64(b + 96, st->st_ctime);
  Write64(b + 104, st->st_ctim.tv_nsec);
#else
  Write64(b + 64, st->st_mtime);
  Write64(b + 80, st->st_ctime);
  Write64(b + 96, st->st_ctime);
#endif
  Write64(b + 112, st->st_size);
  Write64(b + 120, st->st_blocks);
  Write32(b + 128, (u32)st->st_blksize);
  CopyToUserWrite(m, addr, b, 144);
}

static int SysFreeBSDFstat(struct Machine* m, i32 fd, i64 addr) {
  int rc;
  struct stat st;
  if ((rc = VfsFstat(fd, &st)) != -1) WriteFreeBSDStat(m, addr, &st);
  return rc;
}

static int SysFreeBSDStat(struct Machine* m, i64 pathaddr, i64 addr) {
  int rc;
  struct stat st;
  const char* path;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  if ((rc = VfsStat(AT_FDCWD, path, &st, 0)) != -1)
    WriteFreeBSDStat(m, addr, &st);
  return rc;
}

static int SysFreeBSDLstat(struct Machine* m, i64 pathaddr, i64 addr) {
  int rc;
  struct stat st;
  const char* path;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  if ((rc = VfsStat(AT_FDCWD, path, &st, AT_SYMLINK_NOFOLLOW)) != -1) {
    WriteFreeBSDStat(m, addr, &st);
  }
  return rc;
}

static int SysFreeBSDFstatat(struct Machine* m, i32 dirfd, i64 pathaddr,
                             i64 addr, i32 flags) {
  int rc;
  struct stat st;
  const char* path;
  // FreeBSD AT_SYMLINK_NOFOLLOW=0x200 differs from Linux AT_SYMLINK_NOFOLLOW=0x100
  flags = XlatFreeBSDAtFlags(flags);
  if (!(path = LoadStr(m, pathaddr))) return -1;
  if ((rc = VfsStat(GetDirFildes(dirfd), path, &st,
                    XlatFstatatFlags(flags))) != -1) {
    WriteFreeBSDStat(m, addr, &st);
  }
  return rc;
}

static int SysFreeBSDThrSelf(struct Machine* m, i64 idaddr) {
  long tid = m->tid;
  if (CopyToUserWrite(m, idaddr, &tid, 8) == -1) return -1;
  return 0;
}

static int SysFreeBSDThrKill(struct Machine* m, long id, int sig) {
  if (sig == 0) return 0;  // existence check only
  // In single-threaded blink, TID matches the machine's tid
  if (id == m->tid) {
    return kill(getpid(), sig);
  }
  return 0;  // ignore signals to other threads
}

static int SysFreeBSDShmOpen2(struct Machine* m, i64 path, int flags, int mode,
                              i64 shmflags, i64 name) {
  int sysflags = O_CLOEXEC;
  if ((flags & 3) == 0) sysflags |= O_ACCMODE;  // O_RDONLY is 0
  if (flags & 0x0001) sysflags |= O_WRONLY;
  if (flags & 0x0002) sysflags |= O_RDWR;
  if (flags & 0x0200) sysflags |= O_CREAT;
  if (flags & 0x0400) sysflags |= O_TRUNC;
  if (flags & 0x0800) sysflags |= O_EXCL;

  if (path == 1) {  // SHM_ANON
#ifdef __linux__
    int fd = memfd_create("blink", (flags & 0x00100000) ? MFD_CLOEXEC : 0);
    if (fd != -1) {
      struct Fd* f;
      LOCK(&m->system->fds.lock);
      if ((f = AddFd(&m->system->fds, fd, O_RDWR | (sysflags & O_CLOEXEC)))) {
        int guestfd = f->fildes;
        UNLOCK(&m->system->fds.lock);
        return guestfd;
      }
      UNLOCK(&m->system->fds.lock);
      close(fd);
      return -1;
    }
    return -1;
#else
    return eopnotsupp();
#endif
  } else {
    const char* pathname = LoadStr(m, path);
    if (pathname) {
      char fixed_path[1024];
      const char* p = pathname;
      if (p[0] != '/') {
        snprintf(fixed_path, sizeof(fixed_path), "/%s", pathname);
        p = fixed_path;
      }
      int fd = shm_open(p, sysflags, mode);
      if (fd != -1) {
        struct Fd* f;
        LOCK(&m->system->fds.lock);
        if ((f = AddFd(&m->system->fds, fd, sysflags))) {
          int guestfd = f->fildes;
          UNLOCK(&m->system->fds.lock);
          return guestfd;
        }
        UNLOCK(&m->system->fds.lock);
        close(fd);
        return -1;
      }
    }
    return -1;
  }
}

static int SysFreeBSDShmUnlink(struct Machine* m, i64 path) {
  const char* pathname = LoadStr(m, path);
  if (pathname) {
    char fixed_path[1024];
    const char* p = pathname;
    if (p[0] != '/') {
      snprintf(fixed_path, sizeof(fixed_path), "/%s", pathname);
      p = fixed_path;
    }
    return shm_unlink(p);
  }
  return -1;
}
static int SysFreeBSDUuidgen(struct Machine* m, i64 store, int count) {
  int i;
  u8 uuid[16];
  if (count < 0) return -EINVAL;
  for (i = 0; i < count; ++i) {
    if (GetRandom(uuid, 16, 0) != 16) return -EAGAIN;
    uuid[6] = (uuid[6] & 0x0f) | 0x40; /* version 4 */
    uuid[8] = (uuid[8] & 0x3f) | 0x80; /* variant 1 */
    if (CopyToUserWrite(m, store + i * 16, uuid, 16) == -1) return -1;
  }
  return 0;
}

static int SysFreeBSDCapGetMode(struct Machine* m, i64 addr) {
  u32 mode = 0;
  if (CopyToUserWrite(m, addr, &mode, 4) == -1) return -1;
  return 0;
}

static int SysFreeBSDCapRightsGet(struct Machine* m, i32 fd, i64 addr) {
  u64 rights[2] = {-1ULL, -1ULL};
  if (CopyToUserWrite(m, addr, rights, 16) == -1) return -1;
  return 0;
}

// Translate FreeBSD sa_flags to Linux sa_flags
// FreeBSD: SA_ONSTACK=0x1 SA_RESTART=0x2 SA_RESETHAND=0x4 SA_NOCLDSTOP=0x8
//          SA_NODEFER=0x10 SA_NOCLDWAIT=0x20 SA_SIGINFO=0x40
// Linux:   SA_NOCLDSTOP=1 SA_NOCLDWAIT=2 SA_SIGINFO=4 SA_RESTORER=0x4000000
//          SA_ONSTACK=0x8000000 SA_RESTART=0x10000000 SA_NODEFER=0x40000000
//          SA_RESETHAND=0x80000000
static u64 XlatFreeBSDSaFlags(u64 fbsd) {
  u64 lnx = 0;
  if (fbsd & 0x0001) lnx |= SA_ONSTACK_LINUX;
  if (fbsd & 0x0002) lnx |= SA_RESTART_LINUX;
  if (fbsd & 0x0004) lnx |= SA_RESETHAND_LINUX;
  if (fbsd & 0x0008) lnx |= SA_NOCLDSTOP_LINUX;
  if (fbsd & 0x0010) lnx |= SA_NODEFER_LINUX;
  if (fbsd & 0x0020) lnx |= SA_NOCLDWAIT_LINUX;
  if (fbsd & 0x0040) lnx |= SA_SIGINFO_LINUX;
  return lnx;
}

static u64 XlatLinuxToFreeBSDSaFlags(u64 lnx) {
  u64 fbsd = 0;
  if (lnx & SA_ONSTACK_LINUX)    fbsd |= 0x0001;
  if (lnx & SA_RESTART_LINUX)    fbsd |= 0x0002;
  if (lnx & SA_RESETHAND_LINUX)  fbsd |= 0x0004;
  if (lnx & SA_NOCLDSTOP_LINUX)  fbsd |= 0x0008;
  if (lnx & SA_NODEFER_LINUX)    fbsd |= 0x0010;
  if (lnx & SA_NOCLDWAIT_LINUX)  fbsd |= 0x0020;
  if (lnx & SA_SIGINFO_LINUX)    fbsd |= 0x0040;
  return fbsd;
}

static int SysFreeBSDSigaction(struct Machine* m, int sig, i64 actaddr,
                               i64 oldaddr) {
  int syssig;
  int lsig;  // Linux signal number
  struct sigaction_linux hand;
  u8 fbsd_hand[32];
  // Translate FreeBSD signal number to Linux signal number for hands[] indexing
  lsig = XlatFreeBSDSignal(sig);
  if (lsig < 1 || lsig > 64) return einval();
  if (oldaddr) {
    LOCK(&m->system->sig_lock);
    hand = m->system->hands[lsig - 1];
    UNLOCK(&m->system->sig_lock);
    memset(fbsd_hand, 0, 32);
    Write64(fbsd_hand + 0, Read64(hand.handler));
    // Convert Linux flags back to FreeBSD flags for the guest
    Write64(fbsd_hand + 8, XlatLinuxToFreeBSDSaFlags(Read64(hand.flags)));
    Write64(fbsd_hand + 16, Read64(hand.mask));
    if (CopyToUserWrite(m, oldaddr, fbsd_hand, 24) == -1) return -1;
  }
  if (actaddr) {
    if (CopyFromUserRead(m, fbsd_hand, actaddr, 24) == -1) return -1;
    u64 fbsd_flags = Read64(fbsd_hand + 8);
    u64 linux_flags = XlatFreeBSDSaFlags(fbsd_flags);
    memset(&hand, 0, sizeof(hand));
    Write64(hand.handler, Read64(fbsd_hand + 0));
    Write64(hand.flags, linux_flags);
    Write64(hand.restorer, m->system->sigtramp);
    Write64(hand.mask, Read64(fbsd_hand + 16));
    LOCK(&m->system->sig_lock);
    m->system->hands[lsig - 1] = hand;
    if ((syssig = XlatSignal(lsig)) != -1 && !IsBlinkSig(m->system, lsig)) {
      struct sigaction syshand;
      sigfillset(&syshand.sa_mask);
      syshand.sa_flags = SA_SIGINFO;
      if (linux_flags & SA_NOCLDSTOP_LINUX)
        syshand.sa_flags |= SA_NOCLDSTOP;
#ifdef SA_NOCLDWAIT
      if (linux_flags & SA_NOCLDWAIT_LINUX)
        syshand.sa_flags |= SA_NOCLDWAIT;
#endif
      u64 handler = Read64(hand.handler);
      if (handler == SIG_DFL_LINUX) {
        syshand.sa_handler = SIG_DFL;
      } else if (handler == SIG_IGN_LINUX) {
        syshand.sa_handler = SIG_IGN;
      } else {
        syshand.sa_sigaction = OnSignal;
      }
      sigaction(syssig, &syshand, 0);
    }
    UNLOCK(&m->system->sig_lock);
  }
  return 0;
}

static int SysGetdomainname(struct Machine* m, i64 addr, u64 size) {
  if (CopyToUserWrite(m, addr, "blink.local", 12) == -1) return -1;
  return 0;
}

static int SysSetdomainname(struct Machine* m, i64 addr, u64 size) {
  return 0;
}

static int SysFreeBSDSysctl(struct Machine* m, i64 nameaddr, u32 namelen,
                            i64 oldaddr, i64 oldlenaddr, i64 newaddr,
                            u64 newlen) {
  int rc = 0;
  u32 name[6];
  if (namelen < 2) return einval();
  u32 readlen = namelen < 6 ? namelen : 6;
  memset(name, 0, sizeof(name));
  if (CopyFromUserRead(m, name, nameaddr, readlen * 4) == -1) return -1;
  if (name[0] == 8 /* CTL_USER */) {
    if (name[1] == 1 /* USER_CS_PATH */) {
      // confstr(_CS_PATH): the default utility search path. whereis(1) and
      // other tools read this; include /usr/local so pkg-installed binaries
      // (e.g. mc) are findable. Callers size the buffer via a first oldaddr=0
      // query, matching the other string sysctls here.
      static const char kCsPath[] =
          "/usr/bin:/bin:/usr/sbin:/sbin:/usr/local/bin:/usr/local/sbin";
      u64 len = sizeof(kCsPath);  // includes the NUL terminator
      if (oldaddr && CopyToUserWrite(m, oldaddr, kCsPath, len) == -1) return -1;
      if (oldlenaddr && CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      return 0;
    }
  }
  if (name[0] == 4 /* CTL_NET */) {
    if (name[1] == 17 /* PF_ROUTE */) {
      // NET_RT_IFLISTL / routing table from getifaddrs()
      // Return a minimal fake interface list with one eth0 + IPv4 address
      // so getaddrinfo() knows IPv4 is available.
      //
      // if_msghdrl(176) + sockaddr_dl[RTA_IFP](16) = 192 byte msg
      // ifa_msghdrl(176) + netmask[RTA_NETMASK](16) + sockaddr_in[RTA_IFA](16) = 208 byte msg
      // Total = 400 bytes
      u8 buf[400];
      u64 total = sizeof(buf);
      memset(buf, 0, total);
      // -- if_msghdrl for interface index 1 --
      // ifm_msglen (u16 @ 0) = 192 (176 header + 16 sockaddr_dl)
      buf[0] = 192; buf[1] = 0;
      // ifm_version (u8 @ 2) = RTM_VERSION = 5
      buf[2] = 5;
      // ifm_type (u8 @ 3) = RTM_IFINFO = 0xe
      buf[3] = 0xe;
      // ifm_addrs (i32 @ 4) = RTA_IFP(0x10)
      buf[4] = 0x10;
      // ifm_flags (i32 @ 8) = IFF_UP|IFF_RUNNING|IFF_BROADCAST = 0x43
      buf[8] = 0x43;
      // ifm_index (u16 @ 12) = 1
      buf[12] = 1;
      // ifm_len (u16 @ 16) = 176
      buf[16] = 176; buf[17] = 0;
      // ifm_data_off (u16 @ 18) = 24
      buf[18] = 24;
      // ifm_data.ifi_type (u8 @ 24) = IFT_ETHER = 6
      buf[24] = 6;
      // ifm_data.ifi_datalen (u16 @ 30) = 152
      buf[30] = 152; buf[31] = 0;
      // ifm_data.ifi_mtu (u32 @ 32) = 1500
      buf[32] = 0xdc; buf[33] = 0x05;
      // -- sockaddr_dl for RTA_IFP (at offset 176) --
      // struct sockaddr_dl: sdl_len, sdl_family, sdl_index(2), sdl_type,
      //   sdl_nlen, sdl_alen, sdl_slen, sdl_data[name+addr]
      u8 *sdl = buf + 176;
      sdl[0] = 12;   // sdl_len = 8 + 4 (name "em0")  but rounded to 16
      sdl[1] = 18;   // AF_LINK
      sdl[2] = 1;    // sdl_index low
      sdl[3] = 0;    // sdl_index high
      sdl[4] = 6;    // sdl_type = IFT_ETHER
      sdl[5] = 3;    // sdl_nlen = 3 ("em0")
      sdl[6] = 0;    // sdl_alen = 0 (no MAC)
      sdl[7] = 0;    // sdl_slen = 0
      sdl[8] = 'e'; sdl[9] = 'm'; sdl[10] = '0'; // name
      // -- ifa_msghdrl for IPv4 address --
      u8 *ifa = buf + 192;
      // ifam_msglen (u16 @ 0) = 208 (176 + 16 netmask + 16 ifa)
      ifa[0] = 208; ifa[1] = 0;
      // ifam_version (u8 @ 2) = 5
      ifa[2] = 5;
      // ifam_type (u8 @ 3) = RTM_NEWADDR = 0xc
      ifa[3] = 0xc;
      // ifam_addrs (i32 @ 4) = RTA_NETMASK(0x4) | RTA_IFA(0x20) = 0x24
      ifa[4] = 0x24;
      // ifam_index (u16 @ 12) = 1
      ifa[12] = 1;
      // ifam_len (u16 @ 16) = 176
      ifa[16] = 176; ifa[17] = 0;
      // ifam_data_off (u16 @ 18) = 24
      ifa[18] = 24;
      // ifam_data.ifi_datalen (u16 @ 30) = 152
      ifa[30] = 152; ifa[31] = 0;
      // -- sockaddr_in for RTA_NETMASK (at offset 192+176=368) --
      u8 *sa_mask = buf + 368;
      sa_mask[0] = 16;  // sa_len
      sa_mask[1] = 2;   // AF_INET
      // sin_addr = 255.255.255.0
      sa_mask[4] = 255; sa_mask[5] = 255; sa_mask[6] = 255; sa_mask[7] = 0;
      // -- sockaddr_in for RTA_IFA (at offset 368+16=384) --
      u8 *sa_ifa = buf + 384;
      sa_ifa[0] = 16;  // sa_len
      sa_ifa[1] = 2;   // AF_INET
      // sin_addr = 10.0.2.15
      sa_ifa[4] = 10; sa_ifa[5] = 0; sa_ifa[6] = 2; sa_ifa[7] = 15;
      if (oldlenaddr) {
        if (CopyToUserWrite(m, oldlenaddr, &total, 8) == -1) return -1;
      }
      if (oldaddr) {
        if (CopyToUserWrite(m, oldaddr, buf, total) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 28 /* PF_INET6 */) {
      // pkg probes net.inet6 to check IPv6 availability.
      // Return empty data (0 length) to indicate no IPv6 info.
      if (oldlenaddr) {
        u64 len = 0;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    fprintf(stderr, "missing freebsd sysctl %d.%d\n", name[0], name[1]);
    return enosys();
  }
  if (name[0] == 0 /* CTL_SYSCTL */) {
    if (name[1] == 1 /* CTL_SYSCTL_NAME */) {
      // OID-to-name: name[2..namelen-1] is the OID, return dotted name string
      int oid0 = namelen > 2 ? name[2] : -1;
      int oid1 = namelen > 3 ? name[3] : -1;
      const char *sname = NULL;
      if (oid0 == 1) {
        switch (oid1) {
          case 1:  sname = "kern.ostype"; break;
          case 2:  sname = "kern.osrelease"; break;
          case 4:  sname = "kern.version"; break;
          case 8:  sname = "kern.argmax"; break;
          case 10: sname = "kern.hostname"; break;
          case 22: sname = "kern.domainname"; break;
          case 24: sname = "kern.osreldate"; break;
          case 33: sname = "kern.usrstack"; break;
          case 37: sname = "kern.arnd"; break;
          case 200: sname = "kern.boottrace.enabled"; break;
          case 201: sname = "kern.smp.maxcpus"; break;
          case 202: sname = "kern.conftxt"; break;
          case 203: sname = "kern.ipc.soacceptqueue"; break;
        }
      } else if (oid0 == 6) {
        switch (oid1) {
          case 1:  sname = "hw.machine"; break;
          case 2:  sname = "hw.model"; break;
          case 3:  sname = "hw.ncpu"; break;
          case 12: sname = "hw.machine_arch"; break;
          case 100: sname = "hw.pagesizes"; break;
        }
      } else if (oid0 == 8 && oid1 == 21) {
        sname = "user.localbase";
      }
      if (!sname) {
        fprintf(stderr, "missing freebsd sysctl name for %d.%d\n", oid0, oid1);
        return enoent();
      }
      u64 slen = strlen(sname) + 1;
      if (oldaddr && CopyToUserWrite(m, oldaddr, sname, slen) == -1) return -1;
      if (oldlenaddr) {
        if (CopyToUserWrite(m, oldlenaddr, &slen, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 3 /* CTL_SYSCTL_NAME2OID */) {
      char buf[64];
      if (newlen >= sizeof(buf)) return einval();
      if (CopyFromUserRead(m, buf, newaddr, newlen) == -1) return -1;
      buf[newlen] = 0;
      int oid[2];
      if (!strcmp(buf, "kern.ostype")) {
        oid[0] = 1;
        oid[1] = 1;
      } else if (!strcmp(buf, "kern.osrelease")) {
        oid[0] = 1;
        oid[1] = 2;
      } else if (!strcmp(buf, "kern.version")) {
        oid[0] = 1;
        oid[1] = 4;
      } else if (!strcmp(buf, "kern.hostname")) {
        oid[0] = 1;
        oid[1] = 10;
      } else if (!strcmp(buf, "kern.osreldate")) {
        oid[0] = 1;
        oid[1] = 24;
      } else if (!strcmp(buf, "kern.arnd")) {
        oid[0] = 1;
        oid[1] = 37;
      } else if (!strcmp(buf, "kern.domainname")) {
        oid[0] = 1;
        oid[1] = 22;
      } else if (!strcmp(buf, "kern.argmax")) {
        oid[0] = 1;
        oid[1] = 8;
      } else if (!strcmp(buf, "user.localbase")) {
        oid[0] = 8;
        oid[1] = 21;
      } else if (!strcmp(buf, "hw.machine_arch")) {
        oid[0] = 6;
        oid[1] = 12;
      } else if (!strcmp(buf, "hw.machine")) {
        oid[0] = 6;
        oid[1] = 1;
      } else if (!strcmp(buf, "hw.pagesizes")) {
        oid[0] = 6;
        oid[1] = 100;
      } else if (!strcmp(buf, "kern.boottrace.enabled")) {
        oid[0] = 1;
        oid[1] = 200;  // synthetic OID for boottrace.enabled
      } else if (!strcmp(buf, "kern.smp.maxcpus")) {
        oid[0] = 1;
        oid[1] = 201;  // synthetic OID for smp.maxcpus
      } else if (!strcmp(buf, "kern.conftxt")) {
        oid[0] = 1;
        oid[1] = 202;  // synthetic OID for conftxt
      } else if (!strcmp(buf, "kern.ipc.soacceptqueue") ||
                 !strcmp(buf, "kern.ipc.somaxconn")) {
        oid[0] = 1;
        oid[1] = 203;  // synthetic OID for soacceptqueue (listen backlog max)
      } else {
        fprintf(stderr, "missing freebsd sysctl name2oid: %s\n", buf);
        return enoent();
      }
      if (oldaddr) {
        if (CopyToUserWrite(m, oldaddr, oid, sizeof(oid)) == -1) return -1;
        if (oldlenaddr) {
          u64 len = sizeof(oid);
          if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
        }
      }
      return 0;
    }
    if (name[1] == 4 /* CTL_SYSCTL_OIDFMT */) {
      // Returns: u32 kind + format string for the OID in name[2..namelen-1]
      // FreeBSD CTLTYPE: INT=2, STRING=3, UINT=6, LONG=7, ULONG=8, OPAQUE=5
      // CTLFLAG_RD=0x80000000, CTLFLAG_WR=0x40000000, CTLFLAG_RW=both
      u32 kind;
      const char *fmt;
      int oid0 = namelen > 2 ? name[2] : -1;
      int oid1 = namelen > 3 ? name[3] : -1;
      if (oid0 == 1 /* CTL_KERN */) {
        switch (oid1) {
          case 1:  /* KERN_OSTYPE */
          case 2:  /* KERN_OSRELEASE */
          case 4:  /* KERN_VERSION */
          case 10: /* KERN_HOSTNAME */
          case 22: /* KERN_DOMAINNAME */
          case 202: /* kern.conftxt (synthetic) */
            kind = 0x80000003; /* CTLFLAG_RD | CTLTYPE_STRING */
            fmt = "A";
            break;
          case 8:  /* KERN_ARGMAX */
          case 24: /* KERN_OSRELDATE */
          case 200: /* kern.boottrace.enabled (synthetic) */
            kind = 0xC0000002; /* CTLFLAG_RW | CTLTYPE_INT */
            fmt = "I";
            break;
          case 33: /* KERN_USRSTACK */
            kind = 0x80000008; /* CTLFLAG_RD | CTLTYPE_ULONG */
            fmt = "LU";
            break;
          case 37: /* KERN_ARND */
            kind = 0x80000005; /* CTLFLAG_RD | CTLTYPE_OPAQUE */
            fmt = "";
            break;
          default:
            kind = 0x80000002; /* CTLFLAG_RD | CTLTYPE_INT */
            fmt = "I";
            break;
        }
      } else if (oid0 == 6 /* CTL_HW */) {
        switch (oid1) {
          case 1:  /* HW_MACHINE */
          case 2:  /* HW_MODEL */
          case 12: /* HW_MACHINE_ARCH */
            kind = 0x80000003; /* CTLFLAG_RD | CTLTYPE_STRING */
            fmt = "A";
            break;
          case 3:  /* HW_NCPU */
            kind = 0x80000002; /* CTLFLAG_RD | CTLTYPE_INT */
            fmt = "I";
            break;
          case 100: /* HW_PAGESIZES */
            kind = 0x80000005; /* CTLFLAG_RD | CTLTYPE_OPAQUE */
            fmt = "LU";
            break;
          default:
            kind = 0x80000002; /* CTLFLAG_RD | CTLTYPE_INT */
            fmt = "I";
            break;
        }
      } else if (oid0 == 8 /* CTL_USER */) {
        kind = 0x80000003; /* CTLFLAG_RD | CTLTYPE_STRING */
        fmt = "A";
      } else {
        // Unknown OID - return generic int format
        kind = 0x80000002; /* CTLFLAG_RD | CTLTYPE_INT */
        fmt = "I";
      }
      if (oldaddr) {
        u8 buf2[68];
        u32 fmtlen = strlen(fmt) + 1;
        if (4 + fmtlen > sizeof(buf2)) return einval();
        memcpy(buf2, &kind, 4);
        memcpy(buf2 + 4, fmt, fmtlen);
        u64 total = 4 + fmtlen;
        if (CopyToUserWrite(m, oldaddr, buf2, total) == -1) return -1;
        if (oldlenaddr) {
          if (CopyToUserWrite(m, oldlenaddr, &total, 8) == -1) return -1;
        }
      } else if (oldlenaddr) {
        u64 total = 4 + strlen(fmt) + 1;
        if (CopyToUserWrite(m, oldlenaddr, &total, 8) == -1) return -1;
      }
      return 0;
    }
  } else if (name[0] == 1 /* CTL_KERN */) {
    if (name[1] == 1 /* KERN_OSTYPE */) {
      if (oldaddr && CopyToUserWrite(m, oldaddr, "FreeBSD", 8) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 8;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 2 /* KERN_OSRELEASE */) {
      if (oldaddr && CopyToUserWrite(m, oldaddr, "16.0-RELEASE", 13) == -1)
        return -1;
      if (oldlenaddr) {
        u64 len = 13;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 4 /* KERN_VERSION */) {
      if (oldaddr &&
          CopyToUserWrite(
              m, oldaddr,
              "FreeBSD 16.0-RELEASE 6666666: Mon Jan 01 00:00:00 UTC 2024     "
              "root@blink.local:/usr/obj/usr/src/amd64.amd64/sys/GENERIC",
              120) == -1)
        return -1;
      if (oldlenaddr) {
        u64 len = 120;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 10 /* KERN_HOSTNAME */) {
      if (oldaddr && CopyToUserWrite(m, oldaddr, "blink.local", 12) == -1)
        return -1;
      if (oldlenaddr) {
        u64 len = 12;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 22 /* KERN_DOMAINNAME */) {
      if (oldaddr && CopyToUserWrite(m, oldaddr, "blink.local", 12) == -1)
        return -1;
      if (oldlenaddr) {
        u64 len = 12;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 24 /* KERN_OSRELDATE */) {
      u32 rel = 1600012;
      if (oldaddr && CopyToUserWrite(m, oldaddr, &rel, 4) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 4;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 37 /* KERN_ARND */) {
      if (oldaddr) {
        u64 size;
        if (oldlenaddr) {
          u8 lenbuf[8];
          if (CopyFromUserRead(m, lenbuf, oldlenaddr, 8) == -1) return -1;
          size = Read64(lenbuf);
          if (size > 256) size = 256;
        } else {
          size = 32;
        }
        void* buf = malloc(size);
        if (!buf) return -1;
        GetRandom(buf, size, GRND_NONBLOCK_LINUX);
        rc = CopyToUserWrite(m, oldaddr, buf, size);
        free(buf);
        if (rc == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 8 /* KERN_ARGMAX */) {
      u32 argmax = 262144; /* 256KB, FreeBSD default */
      if (oldaddr && CopyToUserWrite(m, oldaddr, &argmax, 4) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 4;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 18 /* KERN_NGROUPS */) {
      u32 ngroups = 1023;
      if (oldaddr && CopyToUserWrite(m, oldaddr, &ngroups, 4) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 4;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 14 /* KERN_PROC */) {
      // KERN_PROC: process information queries
      // name[2] = what (KERN_PROC_PID=0, KERN_PROC_PATHNAME=12, ...)
      // name[3] = pid or arg
      if (namelen >= 3 && name[2] == 12 /* KERN_PROC_PATHNAME */) {
        // Return the path of the executable for the given pid (-1 = self)
        const char *path = m->system->elf.prog;
        if (!path) path = "/unknown";
        u64 len = strlen(path) + 1;
        if (oldlenaddr) {
          if (!oldaddr) {
            // Size query only
            if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
            return 0;
          }
        }
        if (oldaddr) {
          if (CopyToUserWrite(m, oldaddr, (void *)path, len) == -1) return -1;
        }
        if (oldlenaddr) {
          if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
        }
        return 0;
      }
      // For other KERN_PROC queries (process list, etc.), return empty
      if (oldlenaddr) {
        u64 len = 0;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 33 /* KERN_USRSTACK */) {
      if (oldaddr) {
        u64 usrstack = 0x800000000000;
        if (CopyToUserWrite(m, oldaddr, &usrstack, 8) == -1) return -1;
      }
      if (oldlenaddr) {
        u64 len = 8;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 21 /* KERN_BOOTTIME */) {
      // struct timeval { time_t tv_sec; suseconds_t tv_usec; }
      // Return a fixed boot time (current time - 3600 seconds)
      struct timespec now;
      clock_gettime(CLOCK_REALTIME, &now);
      u8 tv[16];
      i64 bootsec = now.tv_sec - 3600;
      Write64(tv, bootsec);
      Write64(tv + 8, 0);
      if (oldaddr && CopyToUserWrite(m, oldaddr, tv, 16) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 16;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 26 /* KERN_BOOTFILE */) {
      const char *bootfile = "/boot/kernel/kernel";
      u64 len = strlen(bootfile) + 1;
      if (oldaddr && CopyToUserWrite(m, oldaddr, bootfile, len) == -1)
        return -1;
      if (oldlenaddr) {
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 200 /* synthetic: kern.boottrace.enabled */) {
      u32 val = 0;  // boottrace not enabled under emulation
      if (oldaddr && CopyToUserWrite(m, oldaddr, &val, 4) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 4;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 201 /* synthetic: kern.smp.maxcpus */) {
      // Report the host CPU count. Guests were formerly pinned to 1 CPU to
      // dodge SMP corruption that turned out to be three -m/thread bugs (an
      // out-of-bounds thr_exit-thunk write in thr_new, an unlocked g_hostpages
      // table, and a bogus free on the page-fault CAS-loss path) — all fixed.
      // Go's getCPUCount() reads this to size its cpuset affinity buffer, then
      // counts the bits cpuset_getaffinity returns.
      u32 val = (u32)GetCpuCount();
      if (val < 1) val = 1;
      if (oldaddr && CopyToUserWrite(m, oldaddr, &val, 4) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 4;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 202 /* synthetic: kern.conftxt */) {
      // Go's routebsd reads the "machine <arch>" line to pick the routing
      // message ABI alignment. Provide a minimal config naming the arch.
      static const char conftxt[] = "machine amd64\nident GENERIC\n";
      u64 slen = sizeof(conftxt);  // includes NUL
      if (oldaddr && CopyToUserWrite(m, oldaddr, conftxt, slen) == -1) return -1;
      if (oldlenaddr) {
        if (CopyToUserWrite(m, oldlenaddr, &slen, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 203 /* synthetic: kern.ipc.soacceptqueue */) {
      u32 val = 128;  // SOMAXCONN: max listen backlog
      if (oldaddr && CopyToUserWrite(m, oldaddr, &val, 4) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 4;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
  } else if (name[0] == 6 /* CTL_HW */) {
    if (name[1] == 1 /* HW_MACHINE */) {
      if (oldaddr && CopyToUserWrite(m, oldaddr, "amd64", 6) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 6;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 2 /* HW_MODEL */) {
      if (oldaddr &&
          CopyToUserWrite(m, oldaddr,
                          "Intel(R) Core(TM) i9-9900K CPU @ 3.60GHz", 37) == -1)
        return -1;
      if (oldlenaddr) {
        u64 len = 37;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 3 /* HW_NCPU */) {
      u32 ncpu = (u32)GetCpuCount();  // see kern.smp.maxcpus note above
      if (ncpu < 1) ncpu = 1;
      if (oldaddr && CopyToUserWrite(m, oldaddr, &ncpu, 4) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 4;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 12 /* HW_MACHINE_ARCH */) {
      if (oldaddr && CopyToUserWrite(m, oldaddr, "amd64", 6) == -1) return -1;
      if (oldlenaddr) {
        u64 len = 6;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (name[1] == 100 /* HW_PAGESIZES */) {
      if (oldaddr) {
        u64 pagesizes[2] = {4096, 0};
        if (CopyToUserWrite(m, oldaddr, pagesizes, sizeof(pagesizes)) == -1)
          return -1;
      }
      if (oldlenaddr) {
        u64 len = 16;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
  } else if (name[0] == 2 /* CTL_VM */) {
    if (name[1] == 2 /* VM_LOADAVG */) {
      // FreeBSD struct loadavg { fixpt_t ldavg[3]; long fscale; }
      // fixpt_t = u32, fscale typically 2048
      // Return host load averages if available
      double loadavg[3] = {0.0, 0.0, 0.0};
      getloadavg(loadavg, 3);
      u64 fscale = 2048;
      u8 buf[32];  // 3 * u32 + u64 = 20 bytes on 64-bit
      // On FreeBSD amd64: fixpt_t is u32, long is 8 bytes
      Write32(buf + 0, (u32)(loadavg[0] * fscale));
      Write32(buf + 4, (u32)(loadavg[1] * fscale));
      Write32(buf + 8, (u32)(loadavg[2] * fscale));
      // Padding for alignment (4 bytes before long)
      Write32(buf + 12, 0);
      Write64(buf + 16, fscale);
      u64 total = 24;
      if (oldaddr && CopyToUserWrite(m, oldaddr, buf, total) == -1) return -1;
      if (oldlenaddr) {
        if (CopyToUserWrite(m, oldlenaddr, &total, 8) == -1) return -1;
      }
      return 0;
    }
  } else if (name[0] == 8 /* CTL_USER */) {
    if (name[1] == 21 /* USER_LOCALBASE */) {
      if (oldaddr && CopyToUserWrite(m, oldaddr, "/usr/local", 11) == -1)
        return -1;
      if (oldlenaddr) {
        u64 len = 11;
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
  }
  fprintf(stderr, "missing freebsd sysctl %d.%d.%d.%d (namelen=%d)\n",
          name[0], name[1], name[2], name[3], namelen);
  return enosys();
}

// FreeBSD syscall 570: __sysctlbyname(name, namelen, old, oldlenp, new, newlen)
static int SysFreeBSDSysctlbyname(struct Machine* m, i64 nameaddr,
                                   u64 namelen, i64 oldaddr, i64 oldlenaddr,
                                   i64 newaddr, u64 newlen) {
  char buf[128];
  if (namelen >= sizeof(buf)) return einval();
  if (CopyFromUserRead(m, buf, nameaddr, namelen) == -1) return -1;
  buf[namelen] = 0;
  // Map name to MIB and delegate to SysFreeBSDSysctl
  u32 mib[2];
  u32 miblen = 2;
  if (!strcmp(buf, "kern.ostype")) {
    mib[0] = 1; mib[1] = 1;
  } else if (!strcmp(buf, "kern.osrelease")) {
    mib[0] = 1; mib[1] = 2;
  } else if (!strcmp(buf, "kern.version")) {
    mib[0] = 1; mib[1] = 4;
  } else if (!strcmp(buf, "kern.hostname")) {
    mib[0] = 1; mib[1] = 10;
  } else if (!strcmp(buf, "kern.osreldate")) {
    mib[0] = 1; mib[1] = 24;
  } else if (!strcmp(buf, "kern.arnd")) {
    mib[0] = 1; mib[1] = 37;
  } else if (!strcmp(buf, "kern.domainname")) {
    mib[0] = 1; mib[1] = 22;
  } else if (!strcmp(buf, "kern.argmax")) {
    mib[0] = 1; mib[1] = 8;
  } else if (!strcmp(buf, "kern.usrstack")) {
    mib[0] = 1; mib[1] = 33;
  } else if (!strcmp(buf, "hw.machine")) {
    mib[0] = 6; mib[1] = 1;
  } else if (!strcmp(buf, "hw.model")) {
    mib[0] = 6; mib[1] = 2;
  } else if (!strcmp(buf, "hw.ncpu")) {
    mib[0] = 6; mib[1] = 3;
  } else if (!strcmp(buf, "hw.machine_arch")) {
    mib[0] = 6; mib[1] = 12;
  } else if (!strcmp(buf, "user.localbase")) {
    mib[0] = 8; mib[1] = 21;
  } else if (!strcmp(buf, "hw.pagesizes")) {
    // Return array of page sizes (just 4096, 0 terminator)
    if (oldaddr) {
      u64 pagesizes[2] = {4096, 0};
      if (CopyToUserWrite(m, oldaddr, pagesizes, sizeof(pagesizes)) == -1)
        return -1;
    }
    if (oldlenaddr) {
      u64 len = 16;
      if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
    }
    return 0;
  } else if (!strcmp(buf, "kern.boottime")) {
    mib[0] = 1; mib[1] = 21;
  } else if (!strcmp(buf, "kern.bootfile")) {
    mib[0] = 1; mib[1] = 26;
  } else if (!strcmp(buf, "kern.boottrace.enabled")) {
    mib[0] = 1; mib[1] = 200;
  } else if (!strcmp(buf, "kern.smp.maxcpus")) {
    mib[0] = 1; mib[1] = 201;
  } else if (!strcmp(buf, "kern.conftxt")) {
    mib[0] = 1; mib[1] = 202;
  } else if (!strcmp(buf, "kern.ipc.soacceptqueue") ||
             !strcmp(buf, "kern.ipc.somaxconn")) {
    mib[0] = 1; mib[1] = 203;
  } else if (!strcmp(buf, "security.jail.jailed")) {
    // Return 0 (not jailed)
    u32 val = 0;
    if (oldaddr && CopyToUserWrite(m, oldaddr, &val, 4) == -1) return -1;
    if (oldlenaddr) {
      u64 len = 4;
      if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
    }
    return 0;
  } else if (!strcmp(buf, "hw.availpages")) {
    // Return ~4GB worth of pages (1048576 * 4096 = 4GB)
    u64 val = 1048576;
    if (oldaddr && CopyToUserWrite(m, oldaddr, &val, sizeof(val)) == -1)
      return -1;
    if (oldlenaddr) {
      u64 len = sizeof(val);
      if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
    }
    return 0;
  } else {
    if (!strcmp(buf, "kern.ps_strings")) {
      return enoent();  // not applicable under emulation
    }
    if (!strcmp(buf, "vfs.usermount")) {
      // Return 0 (user mounts not allowed)
      u32 val = 0;
      if (oldaddr && CopyToUserWrite(m, oldaddr, &val, sizeof(val)) == -1)
        return -1;
      if (oldlenaddr) {
        u64 len = sizeof(val);
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (!strcmp(buf, "kern.securelevel")) {
      // -1 = "no securelevel", i.e. permissive. Matches a freshly booted
      // FreeBSD system before init raises it. Many tools (pkg, su, login)
      // check this to gate behaviour.
      i32 val = -1;
      if (oldaddr && CopyToUserWrite(m, oldaddr, &val, sizeof(val)) == -1)
        return -1;
      if (oldlenaddr) {
        u64 len = sizeof(val);
        if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
      }
      return 0;
    }
    if (!strcmp(buf, "kern.console")) {
      // FreeBSD format: "available,,active,". Empty active list is fine —
      // su/login query this just to log; they don't care about contents.
      const char *s = ",,,";
      u64 slen = strlen(s) + 1;
      if (oldaddr && CopyToUserWrite(m, oldaddr, s, slen) == -1) return -1;
      if (oldlenaddr) {
        if (CopyToUserWrite(m, oldlenaddr, &slen, 8) == -1) return -1;
      }
      return 0;
    }
    // Common knobs servers probe at startup. Return plausible defaults so
    // the guest's sizing/heuristics get a sane value rather than ENOENT.
    {
      u32 val32 = 0;
      bool handled = true;
      if (!strcmp(buf, "machdep.hlt_logical_cpus")) {
        val32 = 0;  // we don't expose hlt-halts-logical-cpu semantics
      } else if (!strcmp(buf, "net.inet.tcp.sendspace")) {
        val32 = 32768;  // FreeBSD default
      } else if (!strcmp(buf, "net.inet.tcp.recvspace")) {
        val32 = 65536;  // FreeBSD default
      } else if (!strcmp(buf, "kern.ipc.somaxconn")) {
        val32 = 128;  // FreeBSD default listen queue
      } else if (!strcmp(buf, "kern.ipc.maxsockbuf")) {
        val32 = 2097152;  // ~2 MiB, FreeBSD default
      } else if (!strcmp(buf, "machdep.smp_active")) {
        val32 = 0;  // single-threaded view
      } else if (!strcmp(buf, "kern.smp.cpus")) {
        val32 = 1;
      } else {
        handled = false;
      }
      if (handled) {
        if (oldaddr && CopyToUserWrite(m, oldaddr, &val32, sizeof(val32)) == -1)
          return -1;
        if (oldlenaddr) {
          u64 len = sizeof(val32);
          if (CopyToUserWrite(m, oldlenaddr, &len, 8) == -1) return -1;
        }
        return 0;
      }
    }
    fprintf(stderr, "missing freebsd sysctlbyname: %s\n", buf);
    return enoent();
  }
  // Construct a fake MIB address on guest stack and call SysFreeBSDSysctl
  // Instead, just inline the sysctl handling directly
  i64 fakemib = Read64(m->sp) - 16;
  if (CopyToUserWrite(m, fakemib, mib, 8) == -1) return -1;
  return SysFreeBSDSysctl(m, fakemib, miblen, oldaddr, oldlenaddr, newaddr,
                          newlen);
}

static i64 SysFreeBSDGetdirentries(struct Machine* m, i32 fd, i64 buf,
                                   u32 count, i64 basepaddr) {
  i64 rc = SysFreeBSDGetdents(m, fd, buf, count);
  if (rc >= 0 && basepaddr) {
    struct Fd* f;
    if ((f = GetAndLockFd(m, fd))) {
      long tell = 0;
      if (f->dirstream) tell = VfsTelldir(f->dirstream);
      u8 b[8];
      Write64(b, tell);
      CopyToUserWrite(m, basepaddr, b, 8);
      UnlockFd(f);
    }
  }
  return rc;
}

static i64 SysCopyFileRange(struct Machine* m, i32 fd_in, i64 off_in,
                            i32 fd_out, i64 off_out, u64 len, u32 flags) {
  u8* buf;
  ssize_t rc;
  u64 toto = 0;
  i64 offIn = 0;
  i64 offOut = 0;
  size_t chunk, maxchunk = 16384;
  u8 *offInP = 0, *offOutP = 0;
  if (flags) return einval();
  if (CheckFdAccess(m, fd_in, false, EBADF) == -1) return -1;
  if (CheckFdAccess(m, fd_out, true, EBADF) == -1) return -1;
  if (off_in && !(offInP = (u8*)SchlepRW(m, off_in, 8))) return -1;
  if (off_out && !(offOutP = (u8*)SchlepRW(m, off_out, 8))) return -1;
  if (!(buf = (u8*)AddToFreeList(m, malloc(maxchunk)))) return -1;
  if (offInP) {
    offIn = Read64(offInP);
    if (offIn < 0) return einval();
    if (offIn + len < len || offIn + len > NUMERIC_MAX(off_t)) {
      return eoverflow();
    }
  }
  if (offOutP) {
    offOut = Read64(offOutP);
    if (offOut < 0) return einval();
    if (offOut + len < len || offOut + len > NUMERIC_MAX(off_t)) {
      return eoverflow();
    }
  }
  while (toto < len) {
    chunk = MIN(len - toto, maxchunk);
    if (offInP) {
      rc = VfsPread(fd_in, buf, chunk, offIn + toto);
    } else {
      rc = VfsRead(fd_in, buf, chunk);
    }
    if (rc == -1) {
      if (!toto) return -1;
      break;
    }
    if (rc == 0) break;
    if (offOutP) {
      rc = VfsPwrite(fd_out, buf, rc, offOut + toto);
    } else {
      rc = VfsWrite(fd_out, buf, rc);
    }
    if (rc == -1) {
      if (!toto) return -1;
      break;
    }
    toto += rc;
  }
  if (offInP) Write64(offInP, offIn + toto);
  if (offOutP) Write64(offOutP, offOut + toto);
  return toto;
}

void OpSyscall(P) {
  size_t mark;
  u64 ax, di, si, dx, r0, r8, r9;
  unassert(!m->nofault);
  ax = Get64(m->ax);
  if (ax == 0xE4 || (m->system->isfreebsd && ax == 232)) {
    // clock_gettime() is
    //   1) called frequently,
    //   2) latency sensitive, and
    //   3) usually implemented as a VDSO.
    // Therefore we exempt it from system call tracing.
    di = Get64(m->di);
    if (m->system->isfreebsd) {
      // Translate FreeBSD clock IDs to Linux equivalents
      switch (di) {
        case 0:  di = CLOCK_REALTIME_LINUX; break;  // CLOCK_REALTIME
        case 1:  di = CLOCK_MONOTONIC_LINUX; break;  // CLOCK_VIRTUAL→MONOTONIC
        case 4:  di = CLOCK_MONOTONIC_LINUX; break;  // CLOCK_MONOTONIC
        case 5:  di = CLOCK_BOOTTIME_LINUX; break;   // CLOCK_UPTIME
        case 7:  di = CLOCK_BOOTTIME_LINUX; break;   // CLOCK_UPTIME_PRECISE
        case 8:  di = CLOCK_BOOTTIME_LINUX; break;   // CLOCK_UPTIME_FAST
        case 9:  di = CLOCK_REALTIME_LINUX; break;  // CLOCK_REALTIME_PRECISE
        case 10: di = CLOCK_REALTIME_COARSE_LINUX; break;  // CLOCK_REALTIME_FAST
        case 11: di = CLOCK_MONOTONIC_LINUX; break;  // CLOCK_MONOTONIC_PRECISE
        case 12: di = CLOCK_MONOTONIC_COARSE_LINUX; break;  // CLOCK_MONOTONIC_FAST
        case 13: di = CLOCK_REALTIME_LINUX; break;  // CLOCK_SECOND→REALTIME
        default: break;
      }
    }
    ax = SysClockGettime(m, di, Get64(m->si));
    if (ax == -1) {
      if (m->system->isfreebsd) {
        Put64(m->ax, XlatErrnoToFreeBSD(errno));
        m->flags |= 1u << FLAGS_CF;
      } else {
        Put64(m->ax, -(XlatErrno(errno) & 0xfff));
      }
    } else {
      Put64(m->ax, ax);
      if (m->system->isfreebsd) {
        m->flags &= ~(1u << FLAGS_CF);
      }
    }
    return;
  }
  STATISTIC(++syscalls);
  // make sure blinkenlights display is up to date before performing any
  // potentially blocking operations which would otherwise freeze things
  if (m->system->redraw && m->tid == m->system->pid) {
    m->system->redraw(true);
  }
  // unlike pure opcodes where we'll confidently longjmp out of segfault
  // handlers, system calls are too complex to do that safely, and it is
  // therefore of the highest importance that they never crash under any
  // circumstances. in order to do ensure that we need to lock any pages
  // the system call accesses, so the user can't munmap() them away from
  // some other thread. since we don't want to slow down instructions by
  // adding locking logic to the tranlation lookaside buffer, we need to
  // ensure any memory references the system call performs will tlb miss
  m->insyscall = true;
  if (!m->sysdepth++) {
    atomic_store_explicit(&m->invalidated, true, memory_order_relaxed);
  }
  // to make system calls simpler and safer, any temporary memory that's
  // allocated will be added to a free list to be collected later. since
  // OpSyscall() is potentially recursive when SA_RESTART signals happen
  // we need to save the current mark, so we don't collect parent's data
  mark = m->freelist.n;
  m->interrupted = false;
  ax = Get64(m->ax);
  u64 fbsd_syscall = 0;
  // FreeBSD syscall number translation
  if (m->system->isfreebsd) {
    fbsd_syscall = ax;
    if (ax != 232 && ax != 340) {
      SYS_LOGF("FBSDRAX %" PRIu64 " di=%#" PRIx64 " si=%#" PRIx64
               " dx=%#" PRIx64 " r10=%#" PRIx64 " r8=%#" PRIx64 " r9=%#" PRIx64
               " ip=%#" PRIx64,
               ax, Get64(m->di), Get64(m->si), Get64(m->dx), Get64(m->r10),
               Get64(m->r8), Get64(m->r9), m->ip);
    }
    freebsd_translate:
    switch (ax) {
      case 0: {
        // Indirect syscall: di=real_syscall, si=arg1, dx=arg2, ...
        ax = Get64(m->di);
        Put64(m->di, Get64(m->si));
        Put64(m->si, Get64(m->dx));
        Put64(m->dx, Get64(m->r10));
        Put64(m->r10, Get64(m->r8));
        Put64(m->r8, Get64(m->r9));
        fbsd_syscall = ax;
        goto freebsd_translate;
      }
      case 1:
        // FreeBSD exit(2) terminates the ENTIRE process (all threads), like
        // Linux exit_group(2). Per-thread exit on FreeBSD is thr_exit(431).
        // Mapping to Linux exit (0x3c) would only pthread_exit the calling
        // thread, hanging the process when other runtime threads are alive
        // (e.g. Go's sysmon/GC). Map to exit_group instead.
        ax = 0xe7;
        break;  // exit → exit_group (whole-process termination)
      case 2:
        ax = 0x39;
        break;  // fork
      case 66:
        ax = 0x3a;
        break;  // vfork
      case 42:
        ax = 0x16;
        break;  // pipe
      case 542: {
        // pipe2: translate FreeBSD flags to Linux flags
        // FreeBSD O_CLOEXEC=0x100000 O_NONBLOCK=4
        u64 bsd_flags = Get64(m->si);
        u64 linux_flags = 0;
        if (bsd_flags & 0x100000) linux_flags |= O_CLOEXEC_LINUX;   // O_CLOEXEC
        if (bsd_flags & 4) linux_flags |= O_NDELAY_LINUX;            // O_NONBLOCK
        Put64(m->si, linux_flags);
        ax = 0x125;
        break;  // pipe2
      }
      case 3:
        ax = 0x00;
        break;  // read
      case 4:
        ax = 0x01;
        break;  // write
      case 14:
        ax = 0x85;
        break;  // mknod
      case 15:
        ax = 0x5a;
        break;  // chmod
      case 124:
        ax = 0x5b;
        break;  // fchmod
      case 16:
        ax = 0x5c;
        break;  // chown
      case 17:
        ax = 0x0c;
        break;  // break -> brk
      case 18:
        ax = 0x301;
        break;  // getfsstat -> stub?
      case 19:
        ax = 0x08;
        break;  // lseek
      case 5:
        ax = 0x02;
        break;  // open
      case 120:
        ax = 0x13;
        break;  // readv
      case 121:
        ax = 0x14;
        break;  // writev
      case 499:
        ax = 0x101;
        break;  // openat
      case 490:
        ax = 0x10C;
        break;  // fchmodat
      case 491:
        ax = 0x104;
        break;  // fchownat
      case 326:
        ax = 0x4F;
        break;  // getcwd (__getcwd)
      case 569:
        ax = 0x146;
        break;  // copy_file_range
      case 515:
        ax = 0x23c;
        break;  // cap_rights_get
      case 516:
      case 533:
      case 534:
      case 536:
        ax = 0x18;
        break;  // cap_enter, cap_rights_limit, cap_ioctls_limit,
                // cap_fcntls_limit
      case 517:
        ax = 0x23b;
        break;  // cap_getmode
      case 518:
        ax = 0x206;
        break;  // pdfork
      case 6:
        ax = 0x03;
        break;  // close
      case 41:
        ax = 0x20;
        break;  // dup
      case 90:
        ax = 0x21;
        break;  // dup2
      case 330:
        ax = 0x124;
        break;  // dup3
      case 7:
        ax = 0x3d;
        break;  // wait4
      case 9:
        ax = 0x56;
        break;  // link
      case 135:
        ax = 0x35;
        break;  // socketpair
      case 128:
        ax = 0x52;
        break;  // rename
      case 10:
        ax = 0x57;
        break;  // unlink
      case 12:
        ax = 0x50;
        break;  // chdir
      case 13:
        ax = 0x51;
        break;  // fchdir
      case 20:
        ax = 0x27;
        break;  // getpid
      case 360:
        ax = 0x76;
        break;  // getresuid
      case 361:
        ax = 0x77;
        break;  // getresgid
      case 482:
        ax = 0x70;
        break;  // shm_open
      case 483:
        ax = 0x71;
        break;  // shm_unlink
      case 23:
        ax = 0x069;
        break;  // setuid
      case 24:
        ax = 0x66;
        break;  // getuid
      case 25:
        ax = 0x6b;
        break;  // geteuid
      case 33:
        ax = 0x15;
        break;  // access
      case 37: {
        // kill(pid, sig): translate the FreeBSD signal NUMBER to Linux. Without
        // this, signals that differ between the OSes (e.g. FreeBSD SIGSTOP=17,
        // SIGCONT=19, SIGCHLD=20 vs Linux 19/18/17) are sent wrong -- notably
        // `kill -STOP` became "send SIGCHLD", so a shell's `kill -STOP $$`
        // never stopped it and mc's subshell job-control sync hung forever.
        i64 fsig = Get64(m->si);
        if (fsig > 0) Put64(m->si, XlatFreeBSDSignal(fsig));
        ax = 0x3e;
        break;  // kill
      }
      case 39:
        ax = 0x6e;
        break;  // getppid
      case 43:
        ax = 0x6c;
        break;  // getegid
      case 47:
        ax = 0x68;
        break;  // getgid
      case 53:
        ax = 0x83;
        break;  // sigaltstack
      case 59:
        ax = 0x3B;
        break;  // execve
      case 81:
        ax = 0x6f;
        break;  // getpgrp
      case 82:
        ax = 0x6d;
        break;  // setpgid
      case 83:
        ax = 0x26;
        break;  // setitimer
      case 99:
        ax = 0x200;
        break;  // sigsuspend (FreeBSD 4-byte sigset variant)
      case 147:
        ax = 0x70;
        break;  // setsid
      case 207:
        ax = 0x79;
        break;  // getpgid
      case 532:
        ax = 0x3d;
        break;  // wait6 -> wait4 (approximate)
      case 136:
        ax = 0x53;
        break;  // mkdir
      case 137:
        ax = 0x54;
        break;  // rmdir
      case 54:
        ax = 0x10;
        break;  // ioctl
      case 58:
        ax = 0x59;
        break;  // readlink
      case 92:
        ax = 0x48;
        break;  // fcntl
      case 93:
        ax = 0x17;
        break;  // select
      case 209:
        ax = 0x07;
        break;  // poll
      case 123:
        ax = 0x5d;
        break;  // fchown
      case 131:
        ax = 0x49;
        break;  // flock
      case 164:
        ax = 0x1FE;
        break;  // uname
      case 162:
        ax = 0xAC;
        break;  // getdomainname
      case 163:
        ax = 0xAB;
        break;  // setdomainname
      case 165:
        ax = 0xFE9;
        break;  // sysarch (custom dispatch — 0xFE collides with Linux inotify_add_watch)
      case 181:
        ax = 0x06A;
        break;  // setgid
      case 188:
      case 555:
        ax = 0x1F8;
        break;  // stat
      case 189:
      case 551:
        ax = 0x1F7;
        break;  // fstat
      case 190:
      case 553:
        ax = 0x1F9;
        break;  // lstat
      case 74:
        ax = 0x0a;
        break;  // mprotect
      case 75:
        ax = 0x01C;
        break;  // madvise
      case 487:
        ax = 0x24c;
        break;  // cpuset_getaffinity
      case 194:
        ax = 0x61;
        break;  // getrlimit
      case 195:
        ax = 0x0A0;
        break;  // setrlimit
      case 202:
        ax = 0x1FA;
        break;  // sysctl
      case 221:
      case 554:
        ax = 0x1F6;
        break;  // getdirentries
      case 228:
        ax = 0x1E;
        break;  // shmat
      case 230:
        ax = 0x43;
        break;  // shmdt
      case 231:
        ax = 0x1D;
        break;  // shmget
      case 232:
        ax = 0xE4;
        break;  // clock_gettime
      case 233:  // clock_settime - translate FreeBSD clock ID
      case 234: {  // clock_getres - translate FreeBSD clock ID
        u64 fbsd_clk2 = Get64(m->di);
        u64 linux_clk2;
        switch (fbsd_clk2) {
          case 0:  linux_clk2 = CLOCK_REALTIME_LINUX; break;
          case 4:  linux_clk2 = CLOCK_MONOTONIC_LINUX; break;
          case 5:  linux_clk2 = CLOCK_BOOTTIME_LINUX; break;
          case 7:  linux_clk2 = CLOCK_BOOTTIME_LINUX; break;
          case 8:  linux_clk2 = CLOCK_BOOTTIME_LINUX; break;
          case 9:  linux_clk2 = CLOCK_REALTIME_LINUX; break;
          case 11: linux_clk2 = CLOCK_MONOTONIC_LINUX; break;
          case 12: linux_clk2 = 6; break;
          case 14: linux_clk2 = 3; break;
          case 15: linux_clk2 = 2; break;
          default: linux_clk2 = fbsd_clk2; break;
        }
        Put64(m->di, linux_clk2);
        ax = (fbsd_syscall == 233) ? 0xE3 : 0xE5;
        break;
      }
      case 240:
        ax = 0x23;
        break;  // nanosleep
      case 244: {
        // clock_nanosleep - translate FreeBSD clock IDs to Linux
        u64 fbsd_clk = Get64(m->di);
        u64 linux_clk;
        switch (fbsd_clk) {
          case 0:  linux_clk = CLOCK_REALTIME_LINUX; break;
          case 4:  linux_clk = CLOCK_MONOTONIC_LINUX; break;
          case 5:  linux_clk = CLOCK_BOOTTIME_LINUX; break;   // CLOCK_UPTIME
          case 7:  linux_clk = CLOCK_BOOTTIME_LINUX; break;   // CLOCK_UPTIME_PRECISE
          case 8:  linux_clk = CLOCK_BOOTTIME_LINUX; break;   // CLOCK_UPTIME_FAST
          case 9:  linux_clk = CLOCK_REALTIME_LINUX; break;   // CLOCK_REALTIME_PRECISE
          case 11: linux_clk = CLOCK_MONOTONIC_LINUX; break;  // CLOCK_MONOTONIC_PRECISE
          case 12: linux_clk = 6; break;  // CLOCK_MONOTONIC_FAST→COARSE
          case 14: linux_clk = 3; break;  // CLOCK_THREAD_CPUTIME_ID
          case 15: linux_clk = 2; break;  // CLOCK_PROCESS_CPUTIME_ID
          default: linux_clk = fbsd_clk; break;
        }
        Put64(m->di, linux_clk);
        ax = 0xE6;
        break;  // clock_nanosleep
      }
      case 253:
        ax = 0xFEA;
        break;  // issetugid (custom dispatch — 0xFD collides with Linux inotify_init)
      case 272:
        ax = 0x1FD;
        break;  // getdents
      case 340:
        ax = 0x0ED;
        break;  // sigprocmask
      case 341: ax = 0x200; break;  // sigsuspend
      case 343: ax = 0x251; break;  // sigpending
      case 342:
        ax = 0x1FB;
        break;  // __sys_sigaction
      case 432:
        ax = 0x1B0;
        break;  // thr_self
      case 433:
        // thr_kill(tid, sig) → tkill(tid, sig)
        // Translate FreeBSD signal number to Linux
        if (m->system->isfreebsd) {
          i64 fbsd_sig = Get64(m->si);
          if (fbsd_sig > 0) Put64(m->si, XlatFreeBSDSignal(fbsd_sig));
        }
        ax = 0x0C8;
        break;  // thr_kill → tkill
      case 563:
        ax = 0x13e;
        break;  // getrandom
      case 570:
        ax = 0x24b;
        break;  // __sysctlbyname
      case 571:
        ax = 0x23a;
        break;  // shm_open2
      case 416:
        ax = 0x1FB;
        break;  // sigaction
      case 417:
        ax = 0x0F;
        break;  // sigreturn
      case 477:
        ax = 0x09;
        break;  // mmap
      case 73:
        ax = 0x0b;
        break;  // munmap
      case 493:
      case 552:
      case 556:
        ax = 0x1FC;
        break;  // fstatat
      // Socket-related syscalls
      case 97:
        ax = 0x29;
        break;  // socket
      case 98:
        ax = 0x2A;
        break;  // connect
      case 30:
        ax = 0x2B;
        break;  // accept
      case 541: {
        // accept4(s, addr, addrlen, flags): translate FreeBSD socket flags
        // (SOCK_CLOEXEC=0x10000000, SOCK_NONBLOCK=0x20000000) in the flags
        // arg (r10) to Linux (SOCK_CLOEXEC_LINUX, SOCK_NONBLOCK_LINUX);
        // SysAccept4 rejects any unknown flag bits.
        u64 bsd_flags = Get64(m->r10);
        u64 linux_flags = 0;
        if (bsd_flags & 0x10000000) linux_flags |= SOCK_CLOEXEC_LINUX;
        if (bsd_flags & 0x20000000) linux_flags |= SOCK_NONBLOCK_LINUX;
        Put64(m->r10, linux_flags);
        ax = 0x120;  // Linux accept4
        break;
      }
      case 104:
        ax = 0x31;
        break;  // bind
      case 105:
        ax = 0x36;
        break;  // setsockopt
      case 106:
        ax = 0x32;
        break;  // listen
      case 116:
        ax = 0x60;
        break;  // gettimeofday
      case 117:
        ax = 0x62;
        break;  // getrusage
      case 118:
        ax = 0x37;
        break;  // getsockopt
      case 31:
        ax = 0x34;
        break;  // getpeername
      case 32:
        ax = 0x33;
        break;  // getsockname
      case 27:
        ax = 0x2F;
        break;  // recvmsg
      case 28:
        ax = 0x2E;
        break;  // sendmsg
      case 29:
        ax = 0x2D;
        break;  // recvfrom
      case 133:
        ax = 0x2C;
        break;  // sendto
      case 134:
        ax = 0x30;
        break;  // shutdown
      case 509:
        // closefrom(int fd): close all fds >= fd
        // Implement as close_range(fd, UINT_MAX, 0)
        Put64(m->si, ~(u32)0);  // last = UINT_MAX
        Put64(m->dx, 0);         // flags = 0
        ax = 0x1B4;              // Linux close_range
        break;
      case 60:
        ax = 0x5F;
        break;  // umask
      case 95:
        ax = 0x4A;
        break;  // fsync
      case 550:
        ax = 0x4B;
        break;  // fdatasync
      case 251:
        ax = 0x3a;
        break;  // rfork → vfork (approximate)
      case 475:
        ax = 0x11;
        break;  // pread
      case 476:
        ax = 0x12;
        break;  // pwrite
      case 478:
        ax = 0x08;
        break;  // lseek (new-ABI variant, same as FreeBSD 19)
      case 480:
        ax = 0x4D;
        break;  // ftruncate
      case 250:
        ax = 0x24e;
        break;  // minherit (stub)
      case 421:
        ax = 0x249;
        break;  // getcontext (stub)
      case 431:
        ax = 0x24f;
        break;  // thr_exit (with state pointer clearing)
      case 443:
        ax = 0x248;
        break;  // thr_wake
      case 454:
        ax = 0x247;
        break;  // _umtx_op
      case 455:
        ax = 0x24d;
        break;  // thr_new
      case 466:
        ax = 0x24a;
        break;  // rtprio_thread (stub)
      case 574:
        ax = 0x246;
        break;  // __realpathat
      case 489:
        ax = 0x1b7;
        break;  // faccessat (4-arg, use faccessat2 which handles FreeBSD AT flags)
      case 500:
        ax = 0x10B;
        break;  // readlinkat
      case 496:
        ax = 0x102;
        break;  // mkdirat
      case 501:
        ax = 0x108;
        break;  // renameat
      case 57:
        ax = 0x58;
        break;  // symlink
      case 495:
        ax = 0x109;
        break;  // linkat
      case 502:
        ax = 0x10A;
        break;  // symlinkat
      case 503:
        ax = 0x107;
        break;  // unlinkat
      case 546:
        ax = 0x250;
        break;  // futimens
      case 547:
        ax = 0x118;
        break;  // utimensat
      case 595:
        ax = 0x073;
        break;  // getgroups (renumbered in FreeBSD 15+)
      case 596:
        ax = 0x074;
        break;  // setgroups
      case 50:
        ax = 0xFF4;
        break;  // setlogin
      case 175:
        ax = 0xFF7;
        break;  // setfib
      case 390:
        ax = 0xFF3;
        break;  // kenv
      case 519:
        ax = 0xFF2;
        break;  // pdkill
      case 523:
        ax = 0xFF5;
        break;  // getloginclass
      case 524:
        ax = 0xFF6;
        break;  // setloginclass
      // FreeBSD-specific syscalls we don't yet implement: return ENOSYS
      // cleanly so callers can fall back rather than panic.
      case 297:   // fhstatfs (old)
      case 298:   // fhopen
      case 299:   // fhstat (old)
      case 355:   // extattrctl
      case 356:   // extattr_set_file
      case 357:   // extattr_get_file
      case 358:   // extattr_delete_file
      case 371:   // extattr_set_fd
      case 372:   // extattr_get_fd
      case 373:   // extattr_delete_fd
      case 412:   // extattr_set_link
      case 413:   // extattr_get_link
      case 414:   // extattr_delete_link
      case 437:   // extattr_list_fd
      case 438:   // extattr_list_file
      case 439:   // extattr_list_link
        ax = 0xFFF;
        break;
      case 376:
        ax = 0xFE0;
        break;  // eaccess
      case 512:
        ax = 0x1F;
        break;  // shmctl
      case 191:  // pathconf(path, name)
      case 192:  // fpathconf(fd, name)
      case 513:  // lpathconf(link, name)
        // All three answer (something, name) — the handler only reads
        // `name` from %rsi and returns sensible defaults, so the same
        // stub serves all three.
        ax = 0xFE1;
        break;
      case 310:        // getsid(pid)
        ax = 0x07C;    // Linux getsid
        break;
      case 311:        // setresuid(ruid, euid, suid)
        ax = 0x075;    // Linux setresuid
        break;
      case 312:        // setresgid(rgid, egid, sgid)
        ax = 0x077;    // Linux setresgid
        break;
      case 89:         // getdtablesize()
        ax = 0xFF8;
        break;
      case 96:         // setpriority(which, who, prio)
        ax = 0xFF9;
        break;
      case 100:        // getpriority(which, who)
        ax = 0xFFA;
        break;
      case 182:        // setegid(egid) → setresgid(-1, egid, -1)
        Put64(m->si, Get64(m->di));
        Put64(m->di, -1);
        Put64(m->dx, -1);
        ax = 0x077;    // Linux setresgid
        break;
      case 183:        // seteuid(euid) → setresuid(-1, euid, -1)
        Put64(m->si, Get64(m->di));
        Put64(m->di, -1);
        Put64(m->dx, -1);
        ax = 0x075;    // Linux setresuid
        break;
      case 445:        // audit(record, len)
      case 446:        // auditon(cmd, data, len)
      case 447:        // getauid(auid)
      case 448:        // setauid(auid)
      case 449:        // getaudit(ai)
      case 450:        // setaudit(ai)
      case 451:        // getaudit_addr
      case 452:        // setaudit_addr
      case 453:        // auditctl(path)
        ax = 0xFFB;    // audit stub — return 0
        break;
      case 331:
        ax = 0x018;
        break;  // sched_yield
      case 393:
        ax = 0xFFC;
        break;  // sendfile (FreeBSD signature, custom handler)
      case 362:
        ax = 0xFE2;
        break;  // kqueue
      case 363:
        ax = 0xFE3;
        break;  // freebsd11_kevent (32-byte struct)
      case 560:
        ax = 0xFE4;
        break;  // kevent (64-byte struct)
      case 464:
        ax = 0xFEE;
        break;  // thr_set_name
      case 530: {
        // FreeBSD: posix_fallocate(fd, offset, len)
        // Linux:   fallocate(fd, mode=0, offset, len)
        // Reshape rdi (fd) unchanged; rsi=offset → rdx; rdx=len → r10; rsi=0.
        u64 fbsd_off = Get64(m->si);
        u64 fbsd_len = Get64(m->dx);
        Put64(m->si, 0);          // mode = 0
        Put64(m->dx, fbsd_off);   // offset
        Put64(m->r10, fbsd_len);  // len
        ax = 0x11D;  // fallocate
        break;
      }
      case 531:
        ax = 0xDD;
        break;  // posix_fadvise → Linux fadvise64
      case 522:
        ax = 0xFEC;
        break;  // pselect (FreeBSD-shape, plain sigmask*)
      case 544:
        ax = 0xFEB;
        break;  // procctl
      case 548: {
        // FreeBSD: funlinkat(dfd, path, fd, flag) — fd is a verification arg.
        // Linux:   unlinkat(dfd, path, flag). Drop the fd argument.
        Put64(m->dx, Get64(m->r10));
        ax = 0x107;  // unlinkat
        break;
      }
      case 504:
        ax = 0xFEF;
        break;  // posix_openpt
      case 520:
        ax = 0xFF0;
        break;  // pdgetpid
      case 49:
        ax = 0xFF1;
        break;  // getlogin
      case 34:   // chflags(path, flags)
      case 35:   // fchflags(fd, flags)
      case 391: {  // lchflags(path, flags)
        // Only `flags` matters for our reject-non-zero policy.
        Put64(m->di, Get64(m->si));  // flags → rdi for SysFreeBSDChflags(flags)
        ax = 0xFED;
        break;
      }
      // mac_* family: not supported. Returning ENOSYS lets MAC-aware code
      // detect absence and fall back.
      case 382:  // __mac_get_proc
      case 383:  // __mac_set_proc
      case 384:  // __mac_get_fd
      case 385:  // __mac_get_file
      case 386:  // __mac_set_fd
      case 387:  // __mac_set_file
      case 388:  // extattrctl (often grouped with mac_*)
      case 389:  // extattr_set_file
      case 409:  // __mac_get_link
      case 410:  // __mac_set_link
        ax = 0xFFF;  // → DefaultCase → ENOSYS
        break;
      case 545:
        ax = 0x10F;
        break;  // ppoll
      case 557:
        ax = 0xFE5;
        break;  // getfsstat
      case 593:
        ax = 0xFE6;
        break;  // inotify_add_watch_at
      case 594:
        ax = 0xFE7;
        break;  // inotify_rm_watch
      case 577: {
        // __specialfd(type, req, len)
        // SPECIALFD_EVENTFD=1: req is struct { unsigned int initval; int flags; }
        int sfd_type = (int)Get64(m->di);
        if (sfd_type == 1) {  // SPECIALFD_EVENTFD
          u8 *req = LookupAddress(m, Get64(m->si));
          if (req) {
            u32 initval = Load32(req);
            i32 bsd_flags = (i32)Load32(req + 4);
            // Translate FreeBSD eventfd flags to Linux
            // FreeBSD EFD_SEMAPHORE=1, EFD_NONBLOCK=4, EFD_CLOEXEC=0x100000
            // Linux EFD_SEMAPHORE=1, EFD_NONBLOCK=0x800, EFD_CLOEXEC=0x80000
            i32 linux_flags = 0;
            if (bsd_flags & 1) linux_flags |= 1;          // EFD_SEMAPHORE
            if (bsd_flags & 4) linux_flags |= 0x800;      // EFD_NONBLOCK
            if (bsd_flags & 0x100000) linux_flags |= 0x80000;  // EFD_CLOEXEC
            Put64(m->di, initval);
            Put64(m->si, linux_flags);
            ax = 0x122;  // eventfd2
          } else {
            ax = 0x122;  // let it fail naturally
          }
        } else if (sfd_type == 2) {  // SPECIALFD_INOTIFY
          ax = 0xFE8;  // custom handler
        } else {
          ax = 0x18;  // stub unsupported specialfd types
        }
        break;  // __specialfd
      }
      default:
        fprintf(stderr, "UNMAPPED FreeBSD syscall %" PRIu64 " pid=%d\n", ax, m->system->pid);
        SYS_LOGF("unmapped FreeBSD syscall %" PRIu64, ax);
        ax = 0xFFF;  // will hit DefaultCase → ENOSYS
        break;
    }
  }
  di = Get64(m->di);
  si = Get64(m->si);
  dx = Get64(m->dx);
  r0 = Get64(m->r10);
  r8 = Get64(m->r8);
  r9 = Get64(m->r9);
  switch (ax & 0xfff) {
    SYSCALL(3, 0x000, "read", SysRead, STRACE_READ);
    SYSCALL(3, 0x001, "write", SysWrite, STRACE_WRITE);
    SYSCALL(3, 0x002, "open", SysOpen, STRACE_OPEN);
    SYSCALL(1, 0x003, "close", SysClose, STRACE_CLOSE);
    SYSCALL(2, 0x004, "stat", SysStat, STRACE_STAT);
    SYSCALL(2, 0x005, "fstat", SysFstat, STRACE_FSTAT);
    SYSCALL(2, 0x006, "lstat", SysLstat, STRACE_LSTAT);
    SYSCALL(3, 0x007, "poll", SysPoll, STRACE_3);
    SYSCALL(3, 0x008, "lseek", SysLseek, STRACE_LSEEK);
    SYSCALL(6, 0x009, "mmap", SysMmap, STRACE_MMAP);
    SYSCALL(4, 0x011, "pread", SysPread, STRACE_PREAD);
    SYSCALL(4, 0x012, "pwrite", SysPwrite, STRACE_PWRITE);
    SYSCALL(5, 0x017, "select", SysSelect, STRACE_SELECT);
    SYSCALL(5, 0x019, "mremap", SysMremap, STRACE_5);
    SYSCALL(6, 0x10E, "pselect6", SysPselect, STRACE_6);
    SYSCALL(3, 0x01A, "msync", SysMsync, STRACE_3);
    SYSCALL(3, 0x00A, "mprotect", SysMprotect, STRACE_MPROTECT);
    SYSCALL(2, 0x00B, "munmap", SysMunmap, STRACE_MUNMAP);
    SYSCALL(4, 0x00D, "rt_sigaction", SysSigaction, STRACE_SIGACTION);
    SYSCALL(4, 0x00E, "rt_sigprocmask", SysSigprocmask, STRACE_SIGPROCMASK);
    SYSCALL(3, 0x010, "ioctl", SysIoctl, STRACE_3);
    SYSCALL(3, 0x013, "readv", SysReadv, STRACE_READV);
    SYSCALL(3, 0x014, "writev", SysWritev, STRACE_WRITEV);
    SYSCALL(2, 0x015, "access", SysAccess, STRACE_ACCESS);
    SYSCALL(3, 0x10D, "faccessat", SysFaccessat, STRACE_FACCESSAT);
    SYSCALL(4, 0x1b7, "faccessat2", SysFaccessat2, STRACE_FACCESSAT2);
    SYSCALL(0, 0x018, "sched_yield", SysSchedYield, STRACE_0);
    SYSCALL(3, 0x01C, "madvise", SysMadvise, STRACE_3);
    SYSCALL(3, 0x01D, "shmget", SysShmget, STRACE_3);
    SYSCALL(3, 0x01E, "shmat", SysShmat, STRACE_3);
    SYSCALL(3, 0x01F, "shmctl", SysShmctl, STRACE_3);
    SYSCALL(1, 0x020, "dup", SysDup1, STRACE_DUP);
    SYSCALL(2, 0x021, "dup2", SysDup2, STRACE_DUP2);
    SYSCALL(0, 0x022, "pause", SysPause, STRACE_PAUSE);
    SYSCALL(2, 0x023, "nanosleep", SysNanosleep, STRACE_NANOSLEEP);
    SYSCALL(2, 0x024, "getitimer", SysGetitimer, STRACE_2);
    SYSCALL(1, 0x025, "alarm", SysAlarm, STRACE_ALARM);
    SYSCALL(3, 0x026, "setitimer", SysSetitimer, STRACE_3);
    SYSCALL(0, 0x027, "getpid", SysGetpid, STRACE_GETPID);
    SYSCALL(0, 0x0BA, "gettid", SysGettid, STRACE_GETTID);
    SYSCALL(1, 0x03F, "uname", SysUname, STRACE_1);
    SYSCALL(1, 0x043, "shmdt", SysShmdt, STRACE_1);
    SYSCALL(3, 0x048, "fcntl", SysFcntl, STRACE_FCNTL);
    SYSCALL(2, 0x049, "flock", SysFlock, STRACE_2);
    SYSCALL(1, 0x04A, "fsync", SysFsync, STRACE_FSYNC);
    SYSCALL(1, 0x04B, "fdatasync", SysFdatasync, STRACE_FDATASYNC);
    SYSCALL(2, 0x04C, "truncate", SysTruncate, STRACE_TRUNCATE);
    SYSCALL(2, 0x04D, "ftruncate", SysFtruncate, STRACE_FTRUNCATE);
    SYSCALL(2, 0x04F, "getcwd", SysGetcwd, STRACE_GETCWD);
    SYSCALL(1, 0x050, "chdir", SysChdir, STRACE_CHDIR);
    SYSCALL(1, 0x051, "fchdir", SysFchdir, STRACE_FCHOWN);
    SYSCALL(2, 0x052, "rename", SysRename, STRACE_RENAME);
    SYSCALL(2, 0x053, "mkdir", SysMkdir, STRACE_MKDIR);
    SYSCALL(1, 0x054, "rmdir", SysRmdir, STRACE_RMDIR);
    SYSCALL(2, 0x055, "creat", SysCreat, STRACE_CREAT);
    SYSCALL(2, 0x056, "link", SysLink, STRACE_LINK);
    SYSCALL(1, 0x057, "unlink", SysUnlink, STRACE_UNLINK);
    SYSCALL(2, 0x058, "symlink", SysSymlink, STRACE_SYMLINK);
    SYSCALL(3, 0x059, "readlink", SysReadlink, STRACE_READLINK);
    SYSCALL(2, 0x05A, "chmod", SysChmod, STRACE_CHMOD);
    SYSCALL(2, 0x05B, "fchmod", SysFchmod, STRACE_FCHOWN);
    SYSCALL(3, 0x05C, "chown", SysChown, STRACE_CHOWN);
    SYSCALL(3, 0x05D, "fchown", SysFchown, STRACE_FCHOWN);
    SYSCALL(3, 0x05E, "lchown", SysLchown, STRACE_LCHOWN);
    SYSCALL(5, 0x104, "fchownat", SysFchownat, STRACE_CHOWNAT);
    SYSCALL(1, 0x05F, "umask", SysUmask, STRACE_UMASK);
    SYSCALL(2, 0x060, "gettimeofday", SysGettimeofday, STRACE_2);
    SYSCALL(2, 0x061, "getrlimit", SysGetrlimit, STRACE_GETRLIMIT);
    SYSCALL(2, 0x062, "getrusage", SysGetrusage, STRACE_2);
    SYSCALL(1, 0x064, "times", SysTimes, STRACE_1);
    SYSCALL(0, 0x06F, "getpgrp", SysGetpgrp, STRACE_GETPGRP);
    SYSCALL(0, 0x070, "setsid", SysSetsid, STRACE_SETSID);
    SYSCALL(2, 0x073, "getgroups", SysGetgroups, STRACE_2);
    SYSCALL(1, 0x079, "getpgid", SysGetpgid, STRACE_GETPGID);
    SYSCALL(1, 0x07C, "getsid", SysGetsid, STRACE_1);
    SYSCALL(1, 0x07F, "rt_sigpending", SysSigpending, STRACE_1);
    SYSCALL(2, 0x089, "statfs", SysStatfs, STRACE_2);
    SYSCALL(2, 0x08A, "fstatfs", SysFstatfs, STRACE_2);
    SYSCALL(2, 0x06D, "setpgid", SysSetpgid, STRACE_2);
    SYSCALL(0, 0x066, "getuid", SysGetuid, STRACE_GETUID);
    SYSCALL(0, 0x068, "getgid", SysGetgid, STRACE_GETGID);
    SYSCALL(1, 0x069, "setuid", SysSetuid, STRACE_SETUID);
    SYSCALL(1, 0x06A, "setgid", SysSetgid, STRACE_SETGID);
    SYSCALL(0, 0x06B, "geteuid", SysGeteuid, STRACE_GETEUID);
    SYSCALL(0, 0x06C, "getegid", SysGetegid, STRACE_GETEGID);
    SYSCALL(0, 0x06E, "getppid", SysGetppid, STRACE_GETPPID);
    SYSCALL(2, 0x071, "setreuid", SysSetreuid, STRACE_SETREUID);
    SYSCALL(2, 0x072, "setregid", SysSetregid, STRACE_SETREGID);
    SYSCALL(2, 0x082, "rt_sigsuspend", SysSigsuspend, STRACE_SIGSUSPEND);
    SYSCALL(2, 0x083, "sigaltstack", SysSigaltstack, STRACE_2);
    SYSCALL(3, 0x085, "mknod", SysMknod, STRACE_3);
    SYSCALL(2, 0x09E, "arch_prctl", SysArchPrctl, STRACE_2);
    SYSCALL(2, 0x0A0, "setrlimit", SysSetrlimit, STRACE_SETRLIMIT);
    SYSCALL(0, 0x0A2, "sync", SysSync, STRACE_SYNC);
    SYSCALL(2, 0x0AB, "setdomainname", SysSetdomainname, STRACE_2);
    SYSCALL(2, 0x0AC, "getdomainname", SysGetdomainname, STRACE_2);
    SYSCALL(3, 0x0D9, "getdents", SysGetdents, STRACE_3);
    SYSCALL(1, 0x0DA, "set_tid_address", SysSetTidAddress, STRACE_1);
    SYSCALL(4, 0x0DD, "fadvise", SysFadvise, STRACE_4);
#ifdef HAVE_CLOCK_SETTIME
    SYSCALL(2, 0x0E3, "clock_settime", SysClockSettime, STRACE_2);
#endif
    SYSCALL(2, 0x0E5, "clock_getres", SysClockGetres, STRACE_2);
    SYSCALL(4, 0x0E6, "clock_nanosleep", SysClockNanosleep, STRACE_CLOCK_SLEEP);
    SYSCALL(2, 0x084, "utime", SysUtime, STRACE_2);
    SYSCALL(2, 0x0EB, "utimes", SysUtimes, STRACE_2);
    SYSCALL(3, 0x105, "futimesat", SysFutimesat, STRACE_3);
    SYSCALL(4, 0x118, "utimensat", SysUtimensat, STRACE_UTIMENSAT);
    SYSCALL(4, 0x101, "openat", SysOpenat, STRACE_OPENAT);
    SYSCALL(3, 0x102, "mkdirat", SysMkdirat, STRACE_MKDIRAT);
    SYSCALL(4, 0x106, "fstatat", SysFstatat, STRACE_FSTATAT);
    SYSCALL(3, 0x107, "unlinkat", SysUnlinkat, STRACE_UNLINKAT);
    SYSCALL(4, 0x108, "renameat", SysRenameat, STRACE_RENAMEAT);
    SYSCALL(5, 0x109, "linkat", SysLinkat, STRACE_LINKAT);
    SYSCALL(3, 0x10A, "symlinkat", SysSymlinkat, STRACE_SYMLINKAT);
    SYSCALL(4, 0x10B, "readlinkat", SysReadlinkat, STRACE_READLINKAT);
    SYSCALL(3, 0x10C, "fchmodat", SysFchmodat, STRACE_FCHMODAT);
#ifndef DISABLE_SOCKETS
    SYSCALL(3, 0x029, "socket", SysSocket, STRACE_SOCKET);
    SYSCALL(3, 0x02A, "connect", SysConnect, STRACE_CONNECT);
    SYSCALL(3, 0x02B, "accept", SysAccept, STRACE_ACCEPT);
    SYSCALL(4, 0x120, "accept4", SysAccept4, STRACE_ACCEPT4);
    SYSCALL(6, 0x02C, "sendto", SysSendto, STRACE_SENDTO);
    SYSCALL(6, 0x02D, "recvfrom", SysRecvfrom, STRACE_RECVFROM);
    SYSCALL(3, 0x02E, "sendmsg", SysSendmsg, STRACE_3);
    SYSCALL(3, 0x02F, "recvmsg", SysRecvmsg, STRACE_3);
#ifndef DISABLE_NONPOSIX
    SYSCALL(4, 0x133, "sendmmsg", SysSendmmsg, STRACE_4);
    SYSCALL(5, 0x12B, "recvmmsg", SysRecvmmsg, STRACE_5);
#endif
    SYSCALL(2, 0x030, "shutdown", SysShutdown, STRACE_2);
    SYSCALL(3, 0x031, "bind", SysBind, STRACE_BIND);
    SYSCALL(2, 0x032, "listen", SysListen, STRACE_LISTEN);
    SYSCALL(3, 0x033, "getsockname", SysGetsockname, STRACE_GETSOCKNAME);
    SYSCALL(3, 0x034, "getpeername", SysGetpeername, STRACE_GETPEERNAME);
    SYSCALL(5, 0x036, "setsockopt", SysSetsockopt, STRACE_5);
    SYSCALL(5, 0x037, "getsockopt", SysGetsockopt, STRACE_5);
#endif /* DISABLE_SOCKETS */
#ifdef HAVE_FORK
    SYSCALL(0, 0x039, "fork", SysFork, STRACE_FORK);
#ifndef DISABLE_NONPOSIX
    SYSCALL(0, 0x03A, "vfork", SysVfork, STRACE_VFORK);
#endif
    SYSCALL(4, 0x03D, "wait4", SysWait4, STRACE_WAIT4);
    SYSCALL(2, 0x03E, "kill", SysKill, STRACE_KILL);
#endif /* HAVE_FORK */
#ifdef HAVE_THREADS
    SYSCALL(6, 0x0CA, "futex", SysFutex, STRACE_FUTEX);
#endif
#if defined(HAVE_FORK) || defined(HAVE_THREADS)
    SYSCALL(1, 0x016, "pipe", SysPipe, STRACE_PIPE);
#ifndef DISABLE_NONPOSIX
    SYSCALL(2, 0x125, "pipe2", SysPipe2, STRACE_PIPE2);
    SYSCALL(2, 0x122, "eventfd2", SysEventfd2, STRACE_2);
    SYSCALL(2, 0x13F, "memfd_create", SysMemfdCreate, STRACE_2);
    SYSCALL(5, 0x14C, "statx", SysStatx, STRACE_5);
    SYSCALL(3, 0x11A, "signalfd", SysSignalfd, STRACE_3);
    SYSCALL(4, 0x121, "signalfd4", SysSignalfd4, STRACE_4);
    SYSCALL(1, 0x11C, "eventfd", SysEventfd, STRACE_1);
    SYSCALL(2, 0x11B, "timerfd_create", SysTimerfdCreate, STRACE_2);
    SYSCALL(4, 0x11E, "timerfd_settime", SysTimerfdSettime, STRACE_4);
    SYSCALL(2, 0x11F, "timerfd_gettime", SysTimerfdGettime, STRACE_2);
    SYSCALL(0, 0x0FD, "inotify_init", SysInotifyInit, STRACE_0);
    SYSCALL(1, 0x126, "inotify_init1", SysInotifyInit1, STRACE_1);
    SYSCALL(3, 0x0FE, "inotify_add_watch", SysInotifyAddWatch, STRACE_3);
    SYSCALL(2, 0x0FF, "inotify_rm_watch", SysInotifyRmWatch, STRACE_2);
    SYSCALL(5, 0x0F7, "waitid", SysWaitid, STRACE_5);
    SYSCALL(4, 0x1B5, "openat2", SysOpenat2, STRACE_4);
    SYSCALL(2, 0x1B3, "clone3", SysClone3, STRACE_2);
    SYSCALL(2, 0x095, "mlock", SysMlock, STRACE_2);
    SYSCALL(2, 0x096, "munlock", SysMunlock, STRACE_2);
    SYSCALL(1, 0x097, "mlockall", SysMlockall, STRACE_1);
    SYSCALL(0, 0x098, "munlockall", SysMunlockall, STRACE_0);
    SYSCALL(3, 0x145, "mlock2", SysMlock2, STRACE_3);
    SYSCALL(6, 0x113, "splice", SysSplice, STRACE_6);
    SYSCALL(4, 0x114, "tee", SysTee, STRACE_4);
    SYSCALL(4, 0x116, "vmsplice", SysVmsplice, STRACE_4);
    SYSCALL(2, 0x1B2, "pidfd_open", SysPidfdOpen, STRACE_2);
    SYSCALL(4, 0x1A8, "pidfd_send_signal", SysPidfdSendSignal, STRACE_4);
    SYSCALL(3, 0x1B6, "pidfd_getfd", SysPidfdGetfd, STRACE_3);
#endif
    SYSCALL(6, 0x038, "clone", SysClone, STRACE_CLONE);
    SYSCALL(2, 0x0C8, "tkill", SysTkill, STRACE_TKILL);
    SYSCALL(3, 0x0EA, "tgkill", SysTgkill, STRACE_3);
    SYSCALL(3, 0x03B, "execve", SysExecve, STRACE_3);
    SYSCALL(4, 0x035, "socketpair", SysSocketpair, STRACE_SOCKETPAIR);
    SYSCALL(2, 0x111, "set_robust_list", SysSetRobustList, STRACE_2);
    SYSCALL(3, 0x112, "get_robust_list", SysGetRobustList, STRACE_3);
    SYSCALL(2, 0x08C, "getpriority", SysGetpriority, STRACE_2);
    SYSCALL(3, 0x08D, "setpriority", SysSetpriority, STRACE_3);
    SYSCALL(2, 0x08E, "sched_set_param", SysSchedSetparam, STRACE_2);
    SYSCALL(2, 0x08F, "sched_get_param", SysSchedGetparam, STRACE_2);
    SYSCALL(3, 0x090, "sched_set_scheduler", SysSchedSetscheduler, STRACE_3);
    SYSCALL(1, 0x091, "sched_get_scheduler", SysSchedGetscheduler, STRACE_1);
    SYSCALL(1, 0x092, "sched_get_priority_max", SysSchedGetPriorityMax,
            STRACE_1);
    SYSCALL(1, 0x093, "sched_get_priority_min", SysSchedGetPriorityMin,
            STRACE_1);
#ifndef DISABLE_NONPOSIX
    SYSCALL(3, 0x0CB, "sched_set_affinity", SysSchedSetaffinity, STRACE_3);
#endif
#endif /* defined(HAVE_FORK) || defined(HAVE_THREADS) */
#ifndef DISABLE_NONPOSIX
    SYSCALL(4, 0x028, "sendfile", SysSendfile, STRACE_4);
    SYSCALL(3, 0x0CC, "sched_get_affinity", SysSchedGetaffinity, STRACE_3);
    SYSCALL(1, 0x00C, "brk", SysBrk, STRACE_1);
    SYSCALL(1, 0x063, "sysinfo", SysSysinfo, STRACE_1);
    SYSCALL(2, 0x074, "setgroups", SysSetgroups, STRACE_2);
    SYSCALL(3, 0x075, "setresuid", SysSetresuid, STRACE_SETRESUID);
    SYSCALL(3, 0x076, "getresuid", SysGetresuid, STRACE_3);
    SYSCALL(3, 0x077, "setresgid", SysSetresgid, STRACE_SETRESGID);
    SYSCALL(3, 0x078, "getresgid", SysGetresgid, STRACE_3);
    SYSCALL(5, 0x09D, "prctl", SysPrctl, STRACE_5);
#if !defined(DISABLE_OVERLAYS) || !defined(DISABLE_VFS)
    SYSCALL(1, 0x0A1, "chroot", SysChroot, STRACE_CHROOT);
#endif
#ifndef DISABLE_VFS
    SYSCALL(5, 0x0A5, "mount", SysMount, STRACE_MOUNT);
#endif
    SYSCALL(3, 0x124, "dup3", SysDup3, STRACE_DUP3);
    SYSCALL(4, 0x103, "mknodat", SysMknodat, STRACE_4);
    SYSCALL(4, 0x127, "preadv", SysPreadv, STRACE_PREADV);
    SYSCALL(4, 0x128, "pwritev", SysPwritev, STRACE_PWRITEV);
    SYSCALL(4, 0x12E, "prlimit", SysPrlimit, STRACE_PRLIMIT);
    SYSCALL(5, 0x10F, "ppoll", SysPpoll, STRACE_5);
    SYSCALL(5, 0x13C, "renameat2", SysRenameat2, STRACE_RENAMEAT2);
    SYSCALL(3, 0x13E, "getrandom", SysGetrandom, STRACE_GETRANDOM);
    SYSCALL(6, 0x146, "copy_file_range", SysCopyFileRange, STRACE_6);
    SYSCALL(5, 0x147, "preadv2", SysPreadv2, STRACE_PREADV2);
    SYSCALL(5, 0x148, "pwritev2", SysPwritev2, STRACE_PWRITEV2);
    SYSCALL(3, 0x1B4, "close_range", SysCloseRange, STRACE_3);
    SYSCALL(2, 0xFE9, "sysarch", SysSysarch, STRACE_2);
    SYSCALL(0, 0xFEA, "issetugid", SysFreeBSDIssetugid, STRACE_0);
    SYSCALL(4, 0x11D, "fallocate", SysFallocate, STRACE_4);
    SYSCALL(4, 0xFEB, "fbsd_procctl", SysFreeBSDProcctl, STRACE_4);
    SYSCALL(6, 0xFEC, "fbsd_pselect", SysFreeBSDPselect, STRACE_6);
    SYSCALL(1, 0xFED, "fbsd_chflags", SysFreeBSDChflags, STRACE_1);
    SYSCALL(2, 0xFEE, "fbsd_thr_set_name", SysFreeBSDThrSetName, STRACE_2);
    SYSCALL(1, 0xFEF, "fbsd_posix_openpt", SysFreeBSDPosixOpenpt, STRACE_1);
    SYSCALL(2, 0xFF0, "fbsd_pdgetpid", SysFreeBSDPdgetpid, STRACE_2);
    SYSCALL(2, 0xFF1, "fbsd_getlogin", SysFreeBSDGetlogin, STRACE_2);
    SYSCALL(2, 0xFF2, "fbsd_pdkill", SysFreeBSDPdkill, STRACE_2);
    SYSCALL(4, 0xFF3, "fbsd_kenv", SysFreeBSDKenv, STRACE_4);
    SYSCALL(1, 0xFF4, "fbsd_setlogin", SysFreeBSDSetlogin, STRACE_1);
    SYSCALL(2, 0xFF5, "fbsd_getloginclass", SysFreeBSDGetloginclass, STRACE_2);
    SYSCALL(1, 0xFF6, "fbsd_setloginclass", SysFreeBSDSetloginclass, STRACE_1);
    SYSCALL(1, 0xFF7, "fbsd_setfib", SysFreeBSDSetfib, STRACE_1);
    SYSCALL(0, 0xFF8, "fbsd_getdtablesize", SysFreeBSDGetdtablesize, STRACE_0);
    SYSCALL(3, 0xFF9, "fbsd_setpriority", SysFreeBSDSetpriority, STRACE_3);
    SYSCALL(2, 0xFFA, "fbsd_getpriority", SysFreeBSDGetpriority, STRACE_2);
    SYSCALL(0, 0xFFB, "fbsd_audit_stub", SysFreeBSDAuditStub, STRACE_0);
    // Linux xattr family (0xbc..0xc7)
    SYSCALL(5, 0x0BC, "setxattr", SysSetxattr, STRACE_5);
    SYSCALL(5, 0x0BD, "lsetxattr", SysLsetxattr, STRACE_5);
    SYSCALL(5, 0x0BE, "fsetxattr", SysFsetxattr, STRACE_5);
    SYSCALL(4, 0x0BF, "getxattr", SysGetxattr, STRACE_4);
    SYSCALL(4, 0x0C0, "lgetxattr", SysLgetxattr, STRACE_4);
    SYSCALL(4, 0x0C1, "fgetxattr", SysFgetxattr, STRACE_4);
    SYSCALL(3, 0x0C2, "listxattr", SysListxattr, STRACE_3);
    SYSCALL(3, 0x0C3, "llistxattr", SysLlistxattr, STRACE_3);
    SYSCALL(3, 0x0C4, "flistxattr", SysFlistxattr, STRACE_3);
    SYSCALL(2, 0x0C5, "removexattr", SysRemovexattr, STRACE_2);
    SYSCALL(2, 0x0C6, "lremovexattr", SysLremovexattr, STRACE_2);
    SYSCALL(2, 0x0C7, "fremovexattr", SysFremovexattr, STRACE_2);
    // Linux misc easy stubs
    SYSCALL(1, 0x087, "personality", SysPersonality, STRACE_1);
    SYSCALL(3, 0x01B, "mincore", SysMincore, STRACE_3);
    SYSCALL(1, 0x132, "syncfs", SysSyncfs, STRACE_1);
    SYSCALL(4, 0x115, "sync_file_range", SysSyncFileRange, STRACE_4);
    SYSCALL(3, 0x135, "getcpu", SysGetcpu, STRACE_3);
    SYSCALL(5, 0x138, "kcmp", SysKcmp, STRACE_5);
    SYSCALL(1, 0x07A, "setfsuid", SysSetfsuid, STRACE_1);
    SYSCALL(1, 0x07B, "setfsgid", SysSetfsgid, STRACE_1);
    SYSCALL(2, 0x0FB, "ioprio_get", SysIoprioGet, STRACE_2);
    SYSCALL(3, 0x0FC, "ioprio_set", SysIoprioSet, STRACE_3);
    SYSCALL(3, 0x0ED, "sigprocmask", SysFreeBSDSigprocmask, STRACE_3);
    SYSCALL(4, 0x1F6, "getdirentries", SysFreeBSDGetdirentries, STRACE_4);
    SYSCALL(3, 0x1FD, "getdents", SysFreeBSDGetdents, STRACE_3);
    SYSCALL(1, 0x1FE, "uname", SysFreeBSDUname, STRACE_1);
    SYSCALL(3, 0x1FB, "sigaction", SysFreeBSDSigaction, STRACE_3);
    SYSCALL(4, 0x1FC, "fstatat", SysFreeBSDFstatat, STRACE_4);
    SYSCALL(6, 0x1FA, "sysctl", SysFreeBSDSysctl, STRACE_6);
    SYSCALL(2, 0x1F8, "stat", SysFreeBSDStat, STRACE_2);
    SYSCALL(2, 0x1F7, "fstat", SysFreeBSDFstat, STRACE_2);
    SYSCALL(2, 0x1F9, "lstat", SysFreeBSDLstat, STRACE_2);
    SYSCALL(1, 0x1B0, "thr_self", SysFreeBSDThrSelf, STRACE_1);
    SYSCALL(2, 0x1B1, "thr_kill", SysFreeBSDThrKill, STRACE_2);
    SYSCALL(5, 0x23a, "shm_open2", SysFreeBSDShmOpen2, STRACE_5);
    SYSCALL(1, 0x23d, "shm_unlink", SysFreeBSDShmUnlink, STRACE_1);
    SYSCALL(2, 392, "uuidgen", SysFreeBSDUuidgen, STRACE_2);
    SYSCALL(1, 0x200, "sigsuspend", SysFreeBSDSigsuspend, STRACE_1);
    SYSCALL(1, 0x251, "sigpending", SysFreeBSDSigpending, STRACE_1);
    SYSCALL(4, 0x246, "__realpathat", SysFreeBSDRealpathat, STRACE_4);
    SYSCALL(5, 0x247, "_umtx_op", SysFreeBSD_umtx_op, STRACE_5);
    SYSCALL(1, 0x248, "thr_wake", SysFreeBSDThrWake, STRACE_1);
    SYSCALL(0, 0x249, "getcontext", SysFreeBSDStub0, STRACE_0);
    SYSCALL(0, 0x24a, "rtprio_thread", SysFreeBSDStub0, STRACE_0);
    SYSCALL(6, 0x24b, "sysctlbyname", SysFreeBSDSysctlbyname, STRACE_6);
    SYSCALL(0, 0x24c, "cpuset_getaffinity", SysFreeBSDCpusetGetaffinity, STRACE_0);
    SYSCALL(0, 0x24d, "thr_new", SysFreeBSDThrNew, STRACE_0);
    SYSCALL(0, 0x24e, "minherit", SysFreeBSDStub0, STRACE_0);
    SYSCALL(2, 0x250, "futimens", SysFreeBSDFutimens, STRACE_2);
#ifdef HAVE_EPOLL_PWAIT1
    SYSCALL(1, 0x0D5, "epoll_create", SysEpollCreate, STRACE_1);
    SYSCALL(1, 0x123, "epoll_create1", SysEpollCreate1, STRACE_1);
    SYSCALL(4, 0x0E9, "epoll_ctl", SysEpollCtl, STRACE_4);
    SYSCALL(4, 0x0E8, "epoll_wait", SysEpollWait, STRACE_4);
    SYSCALL(6, 0x119, "epoll_pwait", SysEpollPwait, STRACE_6);
    SYSCALL(6, 0x1B9, "epoll_pwait2", SysEpollPwait2, STRACE_6);
#endif /* HAVE_EPOLL_PWAIT1 */
#endif /* DISABLE_NONPOSIX */
    case 0x3C:
      /* fprintf(stderr, "exit(%d) pid=%d tid=%d\n", (int)di, m->system->pid, m->tid); */
      SYS_LOGF("%s(%#" PRIx64 ")", "exit", di);
      SysExit(m, di);
    case 0x24f:
      SYS_LOGF("%s(%#" PRIx64 ")", "thr_exit", di);
      SysFreeBSDThrExit(m);
    case 0xE7:
      SYS_LOGF("%s(%#" PRIx64 ")", "exit_group", di);
      SysExitGroup(m, di);
    case 0x00F:
      SigRestore(m);
      m->interrupted = true;  // preevnt ax clobber
      break;
    //case 0x146:
      // avoid noisy copy_file_range() feature check in cosmo
    case 0x1BC:
      // avoid noisy landlock_create_ruleset() feature check in cosmo
    case 0x500:
      // Cosmopolitan uses this number to trigger ENOSYS for testing.
      if (!m->system->iscosmo) goto DefaultCase;
      ax = enosys();
      break;
    case 0x0C9:
      // time() is also noisy in some environments.
      ax = SysTime(m, di);
      break;
    case 0x0FA:
      ax = -38;  // ENOSYS (sysctl stub)
      break;
    case 0x206:
      ax = SysFreeBSDpdfork(m, di, si);
      break;
    case 0xFE0:
      ax = SysFreeBSDEaccess(m, di, si);
      break;
#ifdef HAVE_EPOLL_PWAIT1
    case 0xFE2:
      ax = SysFreeBSDKqueue(m);
      break;
    case 0xFE3:
      // freebsd11_kevent(kq, changelist, nchanges, eventlist, nevents, timeout)
      ax = SysFreeBSDKevent(m, di, si, dx, r0, r8, r9, 32);
      break;
    case 0xFE4:
      // kevent(kq, changelist, nchanges, eventlist, nevents, timeout)
      ax = SysFreeBSDKevent(m, di, si, dx, r0, r8, r9, 64);
      break;
#endif
    case 0xFFC:
      // sendfile(fd, s, offset, nbytes, hdtr, sbytes [, flags])
      ax = SysFreeBSDSendfile(m, di, si, dx, r0, r8, r9);
      break;
    case 0xFE5: {
      // getfsstat(buf, bufsize, mode)
      // FreeBSD struct statfs is 2344 bytes on 64-bit
      // Layout: f_fstypename@280(16), f_mntfromname@296(1024), f_mntonname@1320(1024)
      if (!di) {
        ax = 1;  // report 1 filesystem (root)
      } else {
        u8 *buf = LookupAddress(m, di);
        if (buf && si >= 2344) {
          memset(buf, 0, 2344);
          // f_version = 0x20190901 (current FreeBSD statfs version)
          buf[0] = 0x01; buf[1] = 0x09; buf[2] = 0x19; buf[3] = 0x20;
          // f_bsize = 4096
          buf[16] = 0x00; buf[17] = 0x10;
          // f_iosize = 32768
          buf[24] = 0x00; buf[25] = 0x80;
          // f_blocks = 10000000
          u64 blocks = 10000000; memcpy(buf + 32, &blocks, 8);
          // f_bfree = 5000000
          u64 bfree = 5000000; memcpy(buf + 40, &bfree, 8);
          // f_bavail = 5000000
          i64 bavail = 5000000; memcpy(buf + 48, &bavail, 8);
          // f_namemax = 255
          buf[184] = 255;
          // f_fstypename at offset 280 (16 bytes)
          memcpy(buf + 280, "ufs", 4);
          // f_mntfromname at offset 296 (1024 bytes)
          memcpy(buf + 296, "/dev/da0s1a", 12);
          // f_mntonname at offset 1320 (1024 bytes)
          memcpy(buf + 1320, "/", 2);
          ax = 1;
        } else {
          ax = efault();
        }
      }
      break;
    }
    case 0xFE6: {
      // FreeBSD inotify_add_watch_at(fd, dfd, pathname, mask)
      // di=fd, si=dfd, dx=pathname, r0=mask. We don't honor dfd because
      // host inotify_add_watch doesn't take a dirfd; resolve relative paths
      // via the FreeBSD chroot the same way LoadStr already does for di.
#ifdef __linux__
      const char *path = LoadStr(m, dx);
      if (path) {
        ax = inotify_add_watch((int)di, path, (u32)r0);
      } else {
        ax = -1;
      }
#else
      ax = enosys();
#endif
      break;
    }
    case 0xFE7:
#ifdef __linux__
      ax = inotify_rm_watch((int)di, (int)si);
#else
      ax = 0;
#endif
      break;
    case 0xFE8: {
      // FreeBSD __specialfd(SPECIALFD_INOTIFY, req, len). The flags live in
      // a struct at si: { int flags; } with FreeBSD IN_NONBLOCK=4,
      // IN_CLOEXEC=0x100000. Translate to Linux inotify_init1 flags.
#ifdef __linux__
      int linux_flags = 0;
      u8 *req = LookupAddress(m, si);
      if (req) {
        i32 bsd_flags = (i32)Load32(req);
        if (bsd_flags & 0x4) linux_flags |= O_NDELAY_LINUX;
        if (bsd_flags & 0x100000) linux_flags |= O_CLOEXEC_LINUX;
      }
      ax = SysInotifyInit1(m, linux_flags);
#else
      ax = enosys();
#endif
      break;
    }
    case 0xFE1: {
      // lpathconf(path, name) — return sensible defaults
      // Common queries: _PC_NAME_MAX=4, _PC_PATH_MAX=5, _PC_ACL_NFS4=64
      switch ((int)si) {
        case 1: ax = 32767; break;       // _PC_LINK_MAX
        case 4: ax = 255; break;         // _PC_NAME_MAX
        case 5: ax = 1024; break;        // _PC_PATH_MAX
        case 18: ax = 1024; break;       // _PC_SYMLINK_MAX
        default: ax = 0; break;          // ACL/capability queries → unsupported
      }
      break;
    }
    default:
    DefaultCase:
      fprintf(stderr, "MISSING Linux syscall 0x%03" PRIx64 " pid=%d\n", ax, m->system->pid);
      LOGF("missing syscall 0x%03" PRIx64, ax);
      ax = enosys();
      break;
  }
  if (!m->interrupted) {
    if (m->system->isfreebsd) {
      if (ax != (u64)-1) {
        // FreeBSD fork/vfork/rfork/pdfork kernel ABI: rdx=0 parent, rdx=1 child
        if (fbsd_syscall == 2 || fbsd_syscall == 66 ||
            fbsd_syscall == 251 || fbsd_syscall == 518) {
          Put64(m->dx, ax == 0 ? 1 : 0);
        }
        Put64(m->ax, ax);
        m->flags &= ~(1u << FLAGS_CF);
      } else {
        Put64(m->ax, XlatErrnoToFreeBSD(errno));
        m->flags |= 1u << FLAGS_CF;
      }
    } else {
      if (ax == (u64)-1) {
        ax = -(XlatErrno(errno) & 0xfff);
      }
      Put64(m->ax, ax);
    }
  }
  unassert(--m->sysdepth >= 0);
  CollectPageLocks(m);
  unassert(!m->pagelocks.i || m->sysdepth);
  CollectGarbage(m, mark);
  m->insyscall = false;
}
