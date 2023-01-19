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
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
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
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#ifdef __HAIKU__
#include <sys/sockio.h>
#endif

#include "blink/assert.h"
#include "blink/errno.h"
#include "blink/loader.h"
#include "blink/macros.h"
#include "blink/mop.h"
#include "blink/syscall.h"
#include "blink/timespec.h"
#include "blink/util.h"

#ifdef __linux
#include <sched.h>
#endif

#include "blink/assert.h"
#include "blink/case.h"
#include "blink/debug.h"
#include "blink/endian.h"
#include "blink/errno.h"
#include "blink/iovs.h"
#include "blink/linux.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/map.h"
#include "blink/mop.h"
#include "blink/pml4t.h"
#include "blink/random.h"
#include "blink/signal.h"
#include "blink/stats.h"
#include "blink/swap.h"
#include "blink/syscall.h"
#include "blink/timespec.h"
#include "blink/timeval.h"
#include "blink/util.h"
#include "blink/xlat.h"

#ifdef O_ASYNC
#define O_ASYNC_SETFL O_ASYNC
#else
#define O_ASYNC_SETFL 0
#endif
#ifdef O_DIRECT
#define O_DIRECT_SETFL O_DIRECT
#else
#define O_DIRECT_SETFL 0
#endif
#ifdef O_NOATIME
#define O_NOATIME_SETFL O_NOATIME
#else
#define O_NOATIME_SETFL 0
#endif

#define SETFL_FLAGS \
  (O_APPEND | O_NDELAY | O_ASYNC_SETFL | O_DIRECT_SETFL | O_NOATIME_SETFL)

#define ASSIGN(D, S) memcpy(&D, &S, MIN(sizeof(S), sizeof(D)))
#define SYSCALL(x, name, args)                                              \
  CASE(x, SYS_LOGF("%s(%#" PRIx64 ", %#" PRIx64 ", %#" PRIx64 ", %#" PRIx64 \
                   ", %#" PRIx64 ", %#" PRIx64 ")",                         \
                   #name, di, si, dx, r0, r8, r9);                          \
       ax = name args)

char *g_blink_path;
bool FLAG_statistics;

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

#ifdef __EMSCRIPTEN__
// If this runs on the main thread, the browser is blocked until we return
// back to the main loop. Yield regularly when the process waits for some
// user input.

int em_poll(struct pollfd *fds, nfds_t nfds, int timeout) {
  int ret = poll(fds, nfds, timeout);
  if (ret == 0) emscripten_sleep(50);
  return ret;
}

ssize_t em_readv(int fd, const struct iovec *iov, int iovcnt) {
  // Handle blocking reads by waiting for POLLIN
  if ((fcntl(fd, F_GETFL, 0) & O_NONBLOCK) == 0) {
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    while (em_poll(&pfd, 1, 50) == 0) {
    }
  }
  size_t ret = readv(fd, iov, iovcnt);
  if (ret == -1 && errno == EAGAIN) emscripten_sleep(50);
  return ret;
}
#endif

const struct FdCb kFdCbHost = {
    .close = close,
#ifdef __EMSCRIPTEN__
    .readv = em_readv,
#else
    .readv = readv,
#endif
    .writev = writev,
    .ioctl = SystemIoctl,
#ifdef __EMSCRIPTEN__
    .poll = em_poll,
#else
    .poll = poll,
#endif
    .tcgetattr = tcgetattr,
    .tcsetattr = tcsetattr,
};

void AddStdFd(struct Fds *fds, int fildes) {
  int flags;
  if ((flags = fcntl(fildes, F_GETFL, 0)) >= 0) {
    unassert(AddFd(fds, fildes, flags));
  }
}

struct Fd *GetAndLockFd(struct Machine *m, int fildes) {
  struct Fd *fd;
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) LockFd(fd);
  UnlockFds(&m->system->fds);
  return fd;
}

int GetDirFildes(int fildes) {
  if (fildes == AT_FDCWD_LINUX) return AT_FDCWD;
  return fildes;
}

bool CheckInterrupt(struct Machine *m) {
  int sig;
  u64 signals;
  Put64(m->ax, -EINTR_LINUX);
  signals = m->signals;
  if ((sig = ConsumeSignal(m))) {
    TerminateSignal(m, sig);
  }
  return (m->interrupted = signals != m->signals);
}

static struct Futex *FindFutex(struct Machine *m, i64 addr) {
  struct Dll *e;
  for (e = dll_first(m->system->futexes); e;
       e = dll_next(m->system->futexes, e)) {
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
  LOCK(&m->system->futex_lock);
  if ((f = FindFutex(m, uaddr))) {
    LOCK(&f->lock);
  }
  UNLOCK(&m->system->futex_lock);
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

static void ClearChildTid(struct Machine *m) {
  atomic_int *ctid;
  if (m->ctid) {
    THR_LOGF("ClearChildTid(%#" PRIx64 ")", m->ctid);
    if ((ctid = (atomic_int *)LookupAddress(m, m->ctid))) {
      atomic_store_explicit(ctid, 0, memory_order_seq_cst);
    } else {
      THR_LOGF("invalid clear child tid address %#" PRIx64, m->ctid);
    }
  }
  SysFutexWake(m, m->ctid, INT_MAX);
}

_Noreturn void SysExitGroup(struct Machine *m, int rc) {
  THR_LOGF("pid=%d tid=%d SysExitGroup", m->system->pid, m->tid);
#ifndef NDEBUG
  if (FLAG_statistics) {
    PrintStats();
  }
#endif
  if (m->system->isfork) {
    THR_LOGF("calling _Exit(%d)", rc);
    _Exit(rc);
  } else {
    THR_LOGF("calling exit(%d)", rc);
    KillOtherThreads(m->system);
    FreeMachine(m);
    ShutdownJit();
    exit(rc);
  }
}

_Noreturn void SysExit(struct Machine *m, int rc) {
  THR_LOGF("pid=%d tid=%d SysExit", m->system->pid, m->tid);
  if (IsOrphan(m)) {
    SysExitGroup(m, rc);
  } else {
    ClearChildTid(m);
    FreeMachine(m);
    pthread_exit(0);
  }
}

static int SysFork(struct Machine *m) {
  int pid, newpid = 0;
  unassert(!m->path.jb);
  // NOTES ON LOCKING HIERARCHY
  // exec_lock must come before sig_lock (see dup3)
  // exec_lock must come before fds.lock (see dup3)
  // exec_lock must come before fds.lock (see execve)
  LOCK(&m->system->exec_lock);
  LockFds(&m->system->fds);
  LOCK(&m->system->sig_lock);
  LOCK(&m->system->mmap_lock);
  LOCK(&m->system->futex_lock);
  LOCK(&m->system->machines_lock);
  pid = fork();
  UNLOCK(&m->system->machines_lock);
  UNLOCK(&m->system->futex_lock);
  UNLOCK(&m->system->mmap_lock);
  UNLOCK(&m->system->sig_lock);
  UnlockFds(&m->system->fds);
  UNLOCK(&m->system->exec_lock);
  if (!pid) {
    newpid = getpid();
    InitBus();  // TODO(jart): use shared memory for g_bus
    THR_LOGF("pid=%d tid=%d SysFork -> pid=%d tid=%d",  //
             m->system->pid, m->tid, newpid, newpid);
    m->tid = m->system->pid = newpid;
    m->system->isfork = true;
    RemoveOtherThreads(m->system);
  }
  return pid;
}

static int SysVfork(struct Machine *m) {
  // TODO(jart): Parent should be stopped while child is running.
  return SysFork(m);
}

static void *OnSpawn(void *arg) {
  int rc;
  struct Machine *m = (struct Machine *)arg;
  THR_LOGF("pid=%d tid=%d OnSpawn", m->system->pid, m->tid);
  g_machine = m;
  if (!(rc = sigsetjmp(m->onhalt, 1))) {
    unassert(!pthread_sigmask(SIG_SETMASK, &m->spawn_sigmask, 0));
    Actor(m);
  } else {
    LOGF("halting machine from thread: %d", rc);
    SysExitGroup(m, rc);
  }
}

static int SysSpawn(struct Machine *m, u64 flags, u64 stack, u64 ptid, u64 ctid,
                    u64 tls, u64 func) {
  int tid;
  int err;
  int ignored;
  int supported;
  int mandatory;
  sigset_t ss, oldss;
  pthread_attr_t attr;
  atomic_int *ptid_ptr;
  atomic_int *ctid_ptr;
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
  ignored = CLONE_DETACHED_LINUX;
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
        !(ptid_ptr = (atomic_int *)LookupAddress(m, ptid)))) ||
      ((flags & CLONE_CHILD_SETTID_LINUX) &&
       ((ctid & (sizeof(int) - 1)) ||
        !(ctid_ptr = (atomic_int *)LookupAddress(m, ctid))))) {
    LOGF("bad clone() ptid / ctid pointers: %#" PRIx64, flags);
    return efault();
  }
  if (!(m2 = NewMachine(m->system, m))) {
    return eagain();
  }
  sigfillset(&ss);
  unassert(!pthread_sigmask(SIG_SETMASK, &ss, &oldss));
  tid = m2->tid;
  if (flags & CLONE_SETTLS_LINUX) {
    m2->fs = tls;
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
  err = pthread_create(&m2->thread, &attr, OnSpawn, m2);
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

static int SysClone(struct Machine *m, u64 flags, u64 stack, u64 ptid, u64 ctid,
                    u64 tls, u64 func) {
  if (flags == SIGCHLD_LINUX) {
    if (stack) return einval();
    return SysFork(m);
  } else if (flags == (CLONE_VM_LINUX | CLONE_VFORK_LINUX | SIGCHLD_LINUX)) {
    if (stack) return einval();
    return SysVfork(m);
  } else {
    return SysSpawn(m, flags, stack, ptid, ctid, tls, func);
  }
}

static struct Futex *NewFutex(i64 addr) {
  struct Futex *f;
  if ((f = (struct Futex *)calloc(1, sizeof(struct Futex)))) {
    pthread_cond_init(&f->cond, 0);
    pthread_mutex_init(&f->lock, 0);
    dll_init(&f->elem);
    f->waiters = 1;
    f->addr = addr;
  }
  return f;
}

static void FreeFutex(struct Futex *f) {
  unassert(!pthread_mutex_destroy(&f->lock));
  unassert(!pthread_cond_destroy(&f->cond));
  free(f);
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
    if (!(gtimeout = (const struct timespec_linux *)Schlep(
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
  if (!(mem = GetAddress(m, uaddr))) return efault();
  LOCK(&m->system->futex_lock);
  if (Load32(mem) != expect) {
    UNLOCK(&m->system->futex_lock);
    return eagain();
  }
  if ((f = FindFutex(m, uaddr))) {
    LOCK(&f->lock);
    ++f->waiters;
    UNLOCK(&f->lock);
  }
  if (!f) {
    if ((f = NewFutex(uaddr))) {
      dll_make_first(&m->system->futexes, &f->elem);
    } else {
      UNLOCK(&m->system->futex_lock);
      return -1;
    }
  }
  UNLOCK(&m->system->futex_lock);
  THR_LOGF("pid=%d tid=%d is waiting at address %#" PRIx64, m->system->pid,
           m->tid, uaddr);
  do {
    if (m->killed) {
      rc = EAGAIN;
      break;
    }
    if (CheckInterrupt(m)) {
      rc = EINTR;
      break;
    }
    if (!(mem = GetAddress(m, uaddr))) {
      rc = EFAULT;
      break;
    }
    LOCK(&f->lock);
    if (Load32(mem) != expect) {
      rc = 0;
    } else {
      tick = AddTime(tick, FromMilliseconds(kPollingMs));
      if (CompareTime(tick, deadline) > 0) tick = deadline;
      rc = pthread_cond_timedwait(&f->cond, &f->lock, &tick);
    }
    UNLOCK(&f->lock);
  } while (rc == ETIMEDOUT && CompareTime(tick, deadline) < 0);
  LOCK(&m->system->futex_lock);
  LOCK(&f->lock);
  if (!--f->waiters) {
    dll_remove(&m->system->futexes, &f->elem);
    UNLOCK(&f->lock);
    UNLOCK(&m->system->futex_lock);
    FreeFutex(f);
  } else {
    UNLOCK(&f->lock);
    UNLOCK(&m->system->futex_lock);
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
      return einval();
    default:
      LOGF("unsupported futex op %#x", op);
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
  if (!(futex = (_Atomic(u32) *)Schlep(m, futex_addr, 4))) {
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
    if (!(data = (struct robust_list_linux *)Schlep(m, item, sizeof(*data)))) {
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
  if (!(data = (struct robust_list_linux *)Schlep(m, pending, sizeof(*data)))) {
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
  if (len != sizeof(struct robust_list_linux)) {
    return einval();
  }
  if (!IsValidMemory(m, head_addr, len, PROT_READ | PROT_WRITE)) {
    return efault();
  }
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
#ifdef __linux
  int rc;
  u8 *mask;
  size_t i, n;
  cpu_set_t sysmask;
  GetCpuCount();  // call for effect
  n = MIN(cpusetsize, CPU_SETSIZE / 8) * 8;
  if (!(mask = (u8 *)malloc(n / 8))) return -1;
  CopyFromUserRead(m, mask, maskaddr, n / 8);
  CPU_ZERO(&sysmask);
  for (i = 0; i < n; ++i) {
    if (mask[i / 8] & (1 << (i % 8))) {
      CPU_SET(i, &sysmask);
    }
  }
  rc = sched_setaffinity(pid, sizeof(sysmask), &sysmask);
  free(mask);
  return rc;
#else
  return 0;  // do nothing
#endif
}

static int SysSchedGetaffinity(struct Machine *m,  //
                               i32 pid,            //
                               u64 cpusetsize,     //
                               i64 maskaddr) {
  if (ValidateAffinityPid(m, pid) == -1) return -1;
#ifdef __linux
  int rc;
  u8 *mask;
  size_t i, n;
  cpu_set_t sysmask;
  n = MIN(cpusetsize, CPU_SETSIZE / 8) * 8;
  if (!(mask = (u8 *)malloc(n / 8))) return -1;
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
    CopyToUserWrite(m, maskaddr, mask, n / 8);
  }
  free(mask);
  return rc;
#else
  u8 *mask;
  unsigned i, rc, count;
  count = GetCpuCount();
  rc = ROUNDUP(count, 64) / 8;
  if (cpusetsize < rc) return einval();
  if (!(mask = (u8 *)calloc(1, rc))) return -1;
  count = MIN(count, rc * 8);
  for (i = 0; i < count; ++i) {
    mask[i / 8] |= 1 << (i % 8);
  }
  CopyToUserWrite(m, maskaddr, mask, rc);
  free(mask);
  return rc;
#endif
}

static int SysPrctl(struct Machine *m, int op, i64 a, i64 b, i64 c, i64 d) {
  return einval();
}

static int SysArchPrctl(struct Machine *m, int code, i64 addr) {
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
      CopyToUserWrite(m, addr, buf, 8);
      return 0;
    case ARCH_GET_FS_LINUX:
      Write64(buf, m->fs);
      CopyToUserWrite(m, addr, buf, 8);
      return 0;
    default:
      return einval();
  }
}

static u64 Prot2Page(int prot) {
  u64 key = 0;
  if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) return einval();
  if (prot & PROT_READ) key |= PAGE_U;
  if (prot & PROT_WRITE) key |= PAGE_RW;
  if (~prot & PROT_EXEC) key |= PAGE_XD;
  return key;
}

static int SysMprotect(struct Machine *m, i64 addr, u64 size, int prot) {
  _Static_assert(PROT_READ == 1, "");
  _Static_assert(PROT_WRITE == 2, "");
  _Static_assert(PROT_EXEC == 4, "");
  int rc;
  i64 i, beg, end;
  long gotsome = 0;
  if (!IsValidAddrSize(addr, size)) return einval();
  if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) return einval();
  LOCK(&m->system->mmap_lock);
  rc = ProtectVirtual(m->system, addr, size, prot);
  if (rc != -1 && (prot & PROT_EXEC)) {
    beg = MAX(addr, m->codestart);
    end = MIN(addr + size, m->codestart + m->codesize);
    if (beg < end) {
      for (i = m->codestart; i < m->codestart + m->codesize; ++i) {
        if (GetHook(m, i) != GeneralDispatch) {
          SetHook(m, i, 0);
          ++gotsome;
        }
      }
    }
  }
  UNLOCK(&m->system->mmap_lock);
  if (gotsome) {
    STATISTIC(++smc_resets);
    JIT_LOGF("mprotect(PROT_EXEC) reset %ld JIT hooks", gotsome);
    InvalidateSystem(m->system, false, true);
  }
  return rc;
}

static int SysMadvise(struct Machine *m, i64 addr, size_t len, int advice) {
  return 0;
}

static i64 SysBrk(struct Machine *m, i64 addr) {
  i64 rc;
  long pagesize;
  LOCK(&m->system->mmap_lock);
  MEM_LOGF("brk(%#" PRIx64 ") currently %#" PRIx64, addr, m->system->brk);
  pagesize = GetSystemPageSize();
  addr = ROUNDUP(addr, pagesize);
  if (addr >= kMinBrk) {
    if (addr > m->system->brk) {
      if (ReserveVirtual(m->system, m->system->brk, addr - m->system->brk,
                         PAGE_RW | PAGE_U, -1, 0, false) != -1) {
        m->system->brk = addr;
      }
    } else if (addr < m->system->brk) {
      if (FreeVirtual(m->system, addr, m->system->brk - addr) != -1) {
        m->system->brk = addr;
      }
    }
  }
  rc = m->system->brk;
  UNLOCK(&m->system->mmap_lock);
  return rc;
}

static int SysMunmap(struct Machine *m, i64 virt, u64 size) {
  int rc;
  LOCK(&m->system->mmap_lock);
  rc = FreeVirtual(m->system, virt, size);
  UNLOCK(&m->system->mmap_lock);
  return rc;
}

static int GetOflags(struct Machine *m, int fildes) {
  int oflags;
  struct Fd *fd;
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    oflags = fd->oflags;
  } else {
    oflags = -1;
  }
  UnlockFds(&m->system->fds);
  return oflags;
}

static i64 SysMmap(struct Machine *m, i64 virt, size_t size, int prot,
                   int flags, int fildes, i64 offset) {
  u64 key;
  int oflags;
  ssize_t rc;
  long pagesize;
  if (!IsValidAddrSize(virt, size)) return einval();
  if (flags & MAP_GROWSDOWN_LINUX) return enotsup();
  if ((key = Prot2Page(prot)) == -1) return einval();
  if (flags & MAP_FIXED_NOREPLACE_LINUX) return enotsup();
  if (flags & MAP_ANONYMOUS_LINUX) {
    fildes = -1;
    if ((flags & MAP_TYPE_LINUX) == MAP_FILE_LINUX) {
      return einval();
    }
  }
  if (fildes != -1) {
    if ((oflags = GetOflags(m, fildes)) == -1) return -1;
    if ((oflags & O_ACCMODE) == O_WRONLY ||  //
        ((prot & PROT_WRITE) &&              //
         (oflags & O_APPEND)) ||             //
        ((prot & PROT_WRITE) &&              //
         (flags & MAP_SHARED_LINUX) &&       //
         (oflags & O_ACCMODE) != O_RDWR)) {  //
      errno = EACCES;
      return -1;
    }
  }
  LOCK(&m->system->mmap_lock);
  if (!(flags & MAP_FIXED_LINUX)) {
    if (!virt) {
      if ((virt = FindVirtual(m->system, m->system->automap, size)) == -1) {
        UNLOCK(&m->system->mmap_lock);
        goto Finished;
      }
      pagesize = GetSystemPageSize();
      m->system->automap = ROUNDUP(virt + size, pagesize);
    } else {
      if ((virt = FindVirtual(m->system, virt, size)) == -1) {
        UNLOCK(&m->system->mmap_lock);
        goto Finished;
      }
    }
  }
  rc = ReserveVirtual(m->system, virt, size, key, fildes, offset,
                      !!(flags & MAP_SHARED_LINUX));
  UNLOCK(&m->system->mmap_lock);
  if (rc == -1) virt = -1;
Finished:
  return virt;
}

static int SysMsync(struct Machine *m, i64 virt, size_t size, int flags) {
  // TODO(jart): Is this safe on all platforms?
  return 0;
}

static int SysDup1(struct Machine *m, i32 fildes) {
  int oflags;
  int newfildes;
  struct Fd *fd;
  if (fildes < 0) {
    LOGF("dup() ebadf");
    return ebadf();
  }
  if ((newfildes = dup(fildes)) != -1) {
    LockFds(&m->system->fds);
    if ((fd = GetFd(&m->system->fds, fildes))) {
      oflags = fd->oflags & ~O_CLOEXEC;
    } else {
      oflags = 0;
    }
    unassert(AddFd(&m->system->fds, newfildes, oflags));
    UnlockFds(&m->system->fds);
  } else {
    LOGF("dup() failed %s", strerror(errno));
  }
  return newfildes;
}

static int Dup2(struct Machine *m, int fildes, int newfildes) {
  int rc;
  // POSIX.1-2007 lists dup2() as raising EINTR which seems impossible
  // so it'd be wonderful to learn what kernel(s) actually return this
  // noting Linux reproduces that in both its dup(2) and dup(3) manual
  INTERRUPTIBLE(rc = dup2(fildes, newfildes));
  return rc;
}

static int SysDup2(struct Machine *m, i32 fildes, i32 newfildes) {
  int rc;
  int oflags;
  struct Fd *fd;
  if (newfildes < 0) {
    LOGF("dup2() ebadf");
    return ebadf();
  }
  if (fildes == newfildes) {
    // no-op system call, but still must validate
    LockFds(&m->system->fds);
    if (GetFd(&m->system->fds, fildes)) {
      rc = fildes;
    } else {
      rc = -1;
    }
    UnlockFds(&m->system->fds);
  } else if ((rc = Dup2(m, fildes, newfildes)) != -1) {
    LockFds(&m->system->fds);
    if ((fd = GetFd(&m->system->fds, newfildes))) {
      FreeFd(&m->system->fds, fd);
    }
    if ((fd = GetFd(&m->system->fds, fildes))) {
      oflags = fd->oflags & ~O_CLOEXEC;
    } else {
      oflags = 0;
    }
    unassert(AddFd(&m->system->fds, newfildes, oflags));
    UnlockFds(&m->system->fds);
  } else {
    LOGF("dup2() failed %s", strerror(errno));
  }
  return rc;
}

static int SysDup3(struct Machine *m, i32 fildes, i32 newfildes, i32 flags) {
  int rc;
  int oflags;
  struct Fd *fd;
  if (newfildes < 0) {
    LOGF("dup3() ebadf");
    return ebadf();
  }
  if (fildes == newfildes) {
    LOGF("dup3() args equal");
    return einval();
  }
  if (flags & ~O_CLOEXEC_LINUX) {
    LOGF("dup3() unsupported flags");
    return einval();
  }
  if (flags) LOCK(&m->system->exec_lock);
  if ((rc = Dup2(m, fildes, newfildes)) != -1) {
    if (flags & O_CLOEXEC_LINUX) {
      unassert(!fcntl(newfildes, F_SETFD, FD_CLOEXEC));
    }
    LockFds(&m->system->fds);
    if ((fd = GetFd(&m->system->fds, newfildes))) {
      FreeFd(&m->system->fds, fd);
    }
    if ((fd = GetFd(&m->system->fds, fildes))) {
      oflags = fd->oflags & ~O_CLOEXEC;
    } else {
      oflags = 0;
    }
    if (flags & O_CLOEXEC_LINUX) {
      oflags |= O_CLOEXEC;
    }
    unassert(AddFd(&m->system->fds, newfildes, oflags));
    UnlockFds(&m->system->fds);
  } else {
    LOGF("dup2() failed %s", strerror(errno));
  }
  if (flags) UNLOCK(&m->system->exec_lock);
  return rc;
}

static int SysDupf(struct Machine *m, i32 fildes, i32 minfildes, int cmd) {
  int oflags;
  int newfildes;
  struct Fd *fd;
  if ((newfildes = fcntl(fildes, cmd, minfildes)) != -1) {
    LockFds(&m->system->fds);
    if ((fd = GetFd(&m->system->fds, fildes))) {
      oflags = fd->oflags & ~O_CLOEXEC;
    } else {
      oflags = 0;
    }
    if (cmd == F_DUPFD_CLOEXEC) {
      oflags |= O_CLOEXEC;
    }
    unassert(AddFd(&m->system->fds, newfildes, oflags));
    UnlockFds(&m->system->fds);
  } else {
    LOGF("dupf() failed %s", strerror(errno));
  }
  return newfildes;
}

static void FixupSock(int fd, int flags) {
  if (flags & SOCK_CLOEXEC_LINUX) {
    unassert(!fcntl(fd, F_SETFD, FD_CLOEXEC));
  }
  if (flags & SOCK_NONBLOCK_LINUX) {
    unassert(!fcntl(fd, F_SETFL, O_NDELAY));
  }
}

static int SysUname(struct Machine *m, i64 utsaddr) {
  // glibc binaries won't run unless we report blink as a
  // modern linux kernel on top of genuine intel hardware
  struct utsname_linux uts = {
      .sysname = "blink",
      .release = "4.0",
      .version = "blink 4.0",
      .machine = "x86_64",
  };
  union {
    char host[sizeof(uts.nodename)];
    char domain[sizeof(uts.domainname)];
  } u;
  memset(u.host, 0, sizeof(u.host));
  gethostname(u.host, sizeof(u.host) - 1);
  strcpy(uts.nodename, u.host);
  memset(u.domain, 0, sizeof(u.domain));
#ifndef __HAIKU__
  getdomainname(u.domain, sizeof(u.domain) - 1);
#endif
  strcpy(uts.domainname, u.domain);
  CopyToUser(m, utsaddr, &uts, sizeof(uts));
  return 0;
}

static int SysSocket(struct Machine *m, i32 family, i32 type, i32 protocol) {
  int flags, fildes;
  flags = type & (SOCK_NONBLOCK_LINUX | SOCK_CLOEXEC_LINUX);
  type &= ~(SOCK_NONBLOCK_LINUX | SOCK_CLOEXEC_LINUX);
  if ((type = XlatSocketType(type)) == -1) return -1;
  if ((family = XlatSocketFamily(family)) == -1) return -1;
  if ((protocol = XlatSocketProtocol(protocol)) == -1) return -1;
  if (flags) LOCK(&m->system->exec_lock);
  if ((fildes = socket(family, type, protocol)) != -1) {
    FixupSock(fildes, flags);
    LockFds(&m->system->fds);
    unassert(AddFd(&m->system->fds, fildes,
                   O_RDWR | (flags & SOCK_CLOEXEC_LINUX ? O_CLOEXEC : 0) |
                       (flags & SOCK_NONBLOCK_LINUX ? O_NDELAY : 0)));
    UnlockFds(&m->system->fds);
  }
  if (flags) UNLOCK(&m->system->exec_lock);
  return fildes;
}

static u32 LoadAddrSize(struct Machine *m, i64 asa) {
  const u8 *p;
  if (asa && (p = (const u8 *)Schlep(m, asa, 4))) {
    return Read32(p);
  } else {
    return 0;
  }
}

static void StoreAddrSize(struct Machine *m, i64 asa, socklen_t len) {
  u8 buf[4];
  if (!asa) return;
  Write32(buf, len);
  CopyToUserWrite(m, asa, buf, sizeof(buf));
}

static int LoadSockaddr(struct Machine *m, i64 sockaddr_addr, u32 sockaddr_size,
                        struct sockaddr_storage *out_sockaddr) {
  const struct sockaddr_linux *sockaddr_linux;
  if ((sockaddr_linux = (const struct sockaddr_linux *)Schlep(m, sockaddr_addr,
                                                              sockaddr_size))) {
    return XlatSockaddrToHost(out_sockaddr, sockaddr_linux, sockaddr_size);
  } else {
    return -1;
  }
}

static void StoreSockaddr(struct Machine *m, i64 sockaddr_addr,
                          i64 sockaddr_size_addr, const struct sockaddr *sa,
                          socklen_t salen) {
  int got;
  u32 avail;
  struct sockaddr_storage_linux ss;
  if (!sockaddr_addr) return;
  if (!sockaddr_size_addr) return;
  avail = LoadAddrSize(m, sockaddr_size_addr);
  if ((got = XlatSockaddrToLinux(&ss, sa, salen)) == -1) return;
  StoreAddrSize(m, sockaddr_size_addr, got);
  CopyToUserWrite(m, sockaddr_addr, &ss, MIN(got, avail));
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
    StoreSockaddr(m, sockaddr_addr, sockaddr_size_addr,
                  (struct sockaddr *)&addr, addrlen);
  }
  return rc;
}

static int SysGetsockname(struct Machine *m, int fd, i64 aa, i64 asa) {
  return SysSocketName(m, fd, aa, asa, getsockname);
}

static int SysGetpeername(struct Machine *m, int fd, i64 aa, i64 asa) {
  return SysSocketName(m, fd, aa, asa, getpeername);
}

static int SysAccept4(struct Machine *m, i32 fildes, i64 sockaddr_addr,
                      i64 sockaddr_size_addr, i32 flags) {
  int newfd;
  socklen_t addrlen;
  struct sockaddr_storage addr;
  if (flags & ~(SOCK_CLOEXEC_LINUX | SOCK_NONBLOCK_LINUX)) return einval();
  addrlen = sizeof(addr);
  if (flags) LOCK(&m->system->exec_lock);
  INTERRUPTIBLE(newfd = accept(fildes, (struct sockaddr *)&addr, &addrlen));
  if (newfd != -1) {
    FixupSock(newfd, flags);
    LockFds(&m->system->fds);
    unassert(AddFd(&m->system->fds, newfd,
                   O_RDWR | (flags & SOCK_CLOEXEC_LINUX ? O_CLOEXEC : 0) |
                       (flags & SOCK_NONBLOCK_LINUX ? O_NDELAY : 0)));
    UnlockFds(&m->system->fds);
    StoreSockaddr(m, sockaddr_addr, sockaddr_size_addr,
                  (struct sockaddr *)&addr, addrlen);
  }
  if (flags) UNLOCK(&m->system->exec_lock);
  return newfd;
}

static int XlatSendFlags(int flags) {
  int supported, hostflags;
  supported = MSG_OOB_LINUX |        //
              MSG_DONTROUTE_LINUX |  //
#ifdef MSG_NOSIGNAL
              MSG_NOSIGNAL_LINUX |  //
#endif
#ifdef MSG_EOR
              MSG_EOR_LINUX |
#endif
              MSG_DONTWAIT_LINUX;
  if (flags & ~supported) {
    LOGF("unsupported %s flags %#x", "send", flags & ~supported);
    return einval();
  }
  hostflags = 0;
  if (flags & MSG_OOB_LINUX) hostflags |= MSG_OOB;
  if (flags & MSG_DONTROUTE_LINUX) hostflags |= MSG_DONTROUTE;
  if (flags & MSG_DONTWAIT_LINUX) hostflags |= MSG_DONTWAIT;
#ifdef MSG_NOSIGNAL
  if (flags & MSG_NOSIGNAL_LINUX) hostflags |= MSG_NOSIGNAL;
#endif
#ifdef MSG_EOR
  if (flags & MSG_EOR_LINUX) hostflags |= MSG_EOR;
#endif
  return hostflags;
}

static int XlatRecvFlags(int flags) {
  int supported, hostflags;
  supported = MSG_OOB_LINUX |      //
              MSG_PEEK_LINUX |     //
              MSG_TRUNC_LINUX |    //
              MSG_WAITALL_LINUX |  //
              MSG_DONTWAIT_LINUX;
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
  return hostflags;
}

static i64 SysSendto(struct Machine *m,  //
                     i32 fildes,         //
                     i64 bufaddr,        //
                     u64 buflen,         //
                     i32 flags,          //
                     i64 sockaddr_addr,  //
                     u32 sockaddr_size) {
  i64 rc;
  int len;
  int hostflags;
  const void *mem;
  socklen_t addrlen;
  struct sockaddr *addrp;
  struct sockaddr_storage ss;
  if ((hostflags = XlatSendFlags(flags)) == -1) return -1;
  if (sockaddr_size) {
    if ((len = LoadSockaddr(m, sockaddr_addr, sockaddr_size, &ss)) != -1) {
      addrlen = len;
    } else {
      return -1;
    }
    addrp = (struct sockaddr *)&ss;
  } else {
    addrlen = 0;
    addrp = 0;
  }
  mem = Schlep(m, bufaddr, buflen);
  INTERRUPTIBLE(rc = sendto(fildes, mem, buflen, hostflags, addrp, addrlen));
  return rc;
}

static i64 SysRecvfrom(struct Machine *m,  //
                       i32 fildes,         //
                       i64 bufaddr,        //
                       u64 buflen,         //
                       i32 flags,          //
                       i64 sockaddr_addr,  //
                       i64 sockaddr_size_addr) {
  i64 rc;
  void *buf;
  int hostflags;
  socklen_t addrlen;
  struct sockaddr_storage addr;
  if ((hostflags = XlatRecvFlags(flags)) == -1) return -1;
  if (!(buf = malloc(buflen))) return -1;
  addrlen = sizeof(addr);
  rc = recvfrom(fildes, buf, buflen, hostflags, (struct sockaddr *)&addr,
                &addrlen);
  if (rc != -1) {
    CopyToUserWrite(m, bufaddr, buf, rc);
    StoreSockaddr(m, sockaddr_addr, sockaddr_size_addr,
                  (struct sockaddr *)&addr, addrlen);
  }
  free(buf);
  return rc;
}

static int SysConnectBind(struct Machine *m, i32 fildes, i64 sockaddr_addr,
                          u32 sockaddr_size,
                          int impl(int, const struct sockaddr *, socklen_t)) {
  int rc, len;
  socklen_t addrlen;
  struct sockaddr_storage addr;
  if ((len = LoadSockaddr(m, sockaddr_addr, sockaddr_size, &addr)) != -1) {
    addrlen = len;
  } else {
    return -1;
  }
  rc = impl(fildes, (const struct sockaddr *)&addr, addrlen);
  return rc;
}

static int SysBind(struct Machine *m, int fd, i64 aa, u32 as) {
  return SysConnectBind(m, fd, aa, as, bind);
}

static int SysConnect(struct Machine *m, int fd, i64 aa, u32 as) {
  return SysConnectBind(m, fd, aa, as, connect);
}

static int SysSetsockopt(struct Machine *m, i32 fildes, i32 level, i32 optname,
                         i64 optvaladdr, u32 optvalsize) {
  int rc;
  void *optval;
  int syslevel, sysoptname;
  if (optvalsize > 256) return einval();
  if ((syslevel = XlatSocketLevel(level)) == -1) return -1;
  if ((sysoptname = XlatSocketOptname(level, optname)) == -1) return -1;
  if (!(optval = calloc(1, optvalsize))) return -1;
  CopyFromUserRead(m, optval, optvaladdr, optvalsize);
  rc = setsockopt(fildes, syslevel, sysoptname, optval, optvalsize);
  free(optval);
  return rc;
}

static int SysGetsockopt(struct Machine *m, i32 fildes, i32 level, i32 optname,
                         i64 optvaladdr, i64 optvalsizeaddr) {
  int rc;
  void *optval;
  socklen_t optvalsize;
  u8 optvalsize_linux[4];
  int syslevel, sysoptname;
  if ((syslevel = XlatSocketLevel(level)) == -1) return -1;
  if ((sysoptname = XlatSocketOptname(level, optname)) == -1) return -1;
  CopyFromUserRead(m, optvalsize_linux, optvalsizeaddr,
                   sizeof(optvalsize_linux));
  optvalsize = Read32(optvalsize_linux);
  if (optvalsize > 256) return einval();
  if (!(optval = calloc(1, optvalsize))) return -1;
  rc = getsockopt(fildes, syslevel, sysoptname, optval, &optvalsize);
  Write32(optvalsize_linux, optvalsize);
  CopyToUserWrite(m, optvaladdr, optval, optvalsize);
  CopyToUserWrite(m, optvalsizeaddr, optvalsize_linux,
                  sizeof(optvalsize_linux));
  free(optval);
  return rc;
}

static i64 SysRead(struct Machine *m, i32 fildes, i64 addr, u64 size) {
  i64 rc;
  int oflags;
  struct Fd *fd;
  struct Iovs iv;
  ssize_t (*readv_impl)(int, const struct iovec *, int);
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(readv_impl = fd->cb->readv);
    oflags = fd->oflags;
  } else {
    readv_impl = 0;
    oflags = 0;
  }
  UnlockFds(&m->system->fds);
  if (!fd) return -1;
  if ((oflags & O_ACCMODE) == O_WRONLY) return ebadf();
  if (size) {
    InitIovs(&iv);
    if ((rc = AppendIovsReal(m, &iv, addr, size)) != -1) {
      INTERRUPTIBLE(rc = readv_impl(fildes, iv.p, iv.i));
      if (rc != -1) SetWriteAddr(m, addr, rc);
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
  ssize_t (*writev_impl)(int, const struct iovec *, int);
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(writev_impl = fd->cb->writev);
    oflags = fd->oflags;
  } else {
    writev_impl = 0;
    oflags = 0;
  }
  UnlockFds(&m->system->fds);
  if (!fd) return -1;
  if ((oflags & O_ACCMODE) == O_RDONLY) return ebadf();
  if (size) {
    InitIovs(&iv);
    if ((rc = AppendIovsReal(m, &iv, addr, size)) != -1) {
      INTERRUPTIBLE(rc = writev_impl(fildes, iv.p, iv.i));
      if (rc != -1) SetReadAddr(m, addr, rc);
    }
    FreeIovs(&iv);
  } else {
    rc = 0;
  }
  return rc;
}

// FreeBSD doesn't do access mode check on read/write to pipes.
// Cygwin generally doesn't do access checks or is inconsistent.
static long CheckFdAccess(struct Machine *m, i32 fildes, bool writable,
                          int errno_if_check_fails) {
  int oflags;
  struct Fd *fd;
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    oflags = fd->oflags;
  } else {
    oflags = 0;
  }
  UnlockFds(&m->system->fds);
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
  void *buf;
  ssize_t rc;
  if (CheckFdAccess(m, fildes, false, EBADF) == -1) return -1;
  if (size) {
    if ((buf = malloc(size))) {
      INTERRUPTIBLE(rc = pread(fildes, buf, size, offset));
      if (rc != -1) CopyToUserWrite(m, addr, buf, rc);
      free(buf);
    } else {
      rc = -1;
    }
  } else {
    rc = pread(fildes, 0, 0, offset);
  }
  return rc;
}

static i64 SysPwrite(struct Machine *m, i32 fildes, i64 addr, u64 size,
                     u64 offset) {
  i64 rc;
  if (CheckFdAccess(m, fildes, true, EBADF) == -1) return -1;
  INTERRUPTIBLE(rc = pwrite(fildes, Schlep(m, addr, size), size, offset));
  return rc;
}

static i64 SysReadv(struct Machine *m, i32 fildes, i64 iovaddr, i32 iovlen) {
  i64 rc;
  int oflags;
  struct Fd *fd;
  struct Iovs iv;
  ssize_t (*readv_impl)(int, const struct iovec *, int);
  if (iovlen <= 0 || iovlen > IOV_MAX) return einval();
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(readv_impl = fd->cb->readv);
    oflags = fd->oflags;
  } else {
    readv_impl = 0;
    oflags = 0;
  }
  UnlockFds(&m->system->fds);
  if (!fd) return -1;
  if ((oflags & O_ACCMODE) == O_WRONLY) return ebadf();
  InitIovs(&iv);
  if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen)) != -1) {
    if (iv.i) {
      INTERRUPTIBLE(rc = readv_impl(fildes, iv.p, iv.i));
    } else {
      rc = 0;
    }
  }
  FreeIovs(&iv);
  return rc;
}

static i64 SysWritev(struct Machine *m, i32 fildes, i64 iovaddr, u32 iovlen) {
  i64 rc;
  int oflags;
  struct Fd *fd;
  struct Iovs iv;
  ssize_t (*writev_impl)(int, const struct iovec *, int);
  if (iovlen <= 0 || iovlen > IOV_MAX) return einval();
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(writev_impl = fd->cb->writev);
    oflags = fd->oflags;
  } else {
    writev_impl = 0;
    oflags = 0;
  }
  UnlockFds(&m->system->fds);
  if (!fd) return -1;
  if ((oflags & O_ACCMODE) == O_RDONLY) return ebadf();
  InitIovs(&iv);
  if ((rc = AppendIovsGuest(m, &iv, iovaddr, iovlen)) != -1) {
    if (iv.i) {
      INTERRUPTIBLE(rc = writev_impl(fildes, iv.p, iv.i));
    } else {
      rc = 0;
    }
  }
  FreeIovs(&iv);
  return rc;
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

static i64 SysGetdents(struct Machine *m, i32 fildes, i64 addr, i64 size) {
  int type;
  off_t off;
  long tell;
  int reclen;
  i64 i, dlz;
  size_t len;
  struct Fd *fd;
  struct dirent *ent;
  struct dirent_linux rec;
  dlz = sizeof(struct dirent_linux);
  if (size < dlz) return einval();
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  if (!fd->dirstream && !(fd->dirstream = fdopendir(fd->fildes))) {
    UnlockFd(fd);
    return -1;
  }
  for (i = 0; i + dlz <= size; i += reclen) {
    // telldir() can actually return negative on ARM/MIPS/i386
    errno = 0;
    tell = telldir(fd->dirstream);
    unassert(tell != -1 || errno == 0);
    off = tell;
    if (!(ent = readdir(fd->dirstream))) break;
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
      LOGF("fstatat(%d, %s) failed: %s", fd->fildes, ent->d_name,
           strerror(errno));
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
  UnlockFd(fd);
  return i;
}

static int SysSetTidAddress(struct Machine *m, i64 ctid) {
  m->ctid = ctid;
  return m->tid;
}

static int SysFadvise(struct Machine *m, u32 fd, u64 offset, u64 len,
                      i32 advice) {
  return 0;
}

static int IoctlTiocgwinsz(struct Machine *m, int fd, i64 addr,
                           int fn(int, unsigned long, ...)) {
  int rc;
  struct winsize ws;
  struct winsize_linux gws;
  if ((rc = fn(fd, TIOCGWINSZ, &ws)) != -1) {
    XlatWinsizeToLinux(&gws, &ws);
    CopyToUserWrite(m, addr, &gws, sizeof(gws));
  }
  return rc;
}

static int IoctlTcgets(struct Machine *m, int fd, i64 addr,
                       int fn(int, struct termios *)) {
  int rc;
  struct termios tio;
  struct termios_linux gtio;
  if ((rc = fn(fd, &tio)) != -1) {
    XlatTermiosToLinux(&gtio, &tio);
    CopyToUserWrite(m, addr, &gtio, sizeof(gtio));
  }
  return rc;
}

static int IoctlTcsets(struct Machine *m, int fd, int request, i64 addr,
                       int fn(int, int, const struct termios *)) {
  struct termios tio;
  struct termios_linux gtio;
  CopyFromUserRead(m, &gtio, addr, sizeof(gtio));
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

  if (!(pgrp = (u8 *)Schlep(m, addr, 4))) return -1;
  if ((rc = tcgetpgrp(fd)) == -1) return -1;
  Write32(pgrp, rc);
  return 0;
}

static int IoctlTiocspgrp(struct Machine *m, int fd, i64 addr) {
  u8 *pgrp;
  if (!(pgrp = (u8 *)Schlep(m, addr, 4))) return -1;
  return tcsetpgrp(fd, Read32(pgrp));
}

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
  memset(&ifreq_linux, 0, sizeof(ifreq_linux));
  CopyFromUserRead(m, &ifconf_linux, ifconf_addr, sizeof(ifconf_linux));
  bufsize = MIN(16384, Read64(ifconf_linux.len));
  if (!(buf = (char *)malloc(bufsize))) return -1;
  if (!(buf_linux = (char *)malloc(bufsize))) {
    free(buf);
    return -1;
  }
  ifconf.ifc_len = bufsize;
  ifconf.ifc_buf = buf;
  if (ioctl(systemfd, SIOCGIFCONF, &ifconf)) {
    free(buf_linux);
    free(buf);
    return -1;
  }
  len_linux = 0;
  ifreq = ifconf.ifc_req;
  for (i = 0; i < ifconf.ifc_len;) {
    if (len_linux + sizeof(ifreq_linux) > bufsize) break;
#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || \
    defined(__NetBSD__)
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
  Write64(ifconf_linux.len, len_linux);
  CopyToUserWrite(m, ifconf_addr, &ifconf_linux, sizeof(ifconf_linux));
  CopyToUserWrite(m, Read64(ifconf_linux.buf), buf_linux, len_linux);
  free(buf_linux);
  free(buf);
  return 0;
}

static int IoctlSiocgifaddr(struct Machine *m, int systemfd, i64 ifreq_addr,
                            unsigned long kind) {
  struct ifreq ifreq;
  struct ifreq_linux ifreq_linux;
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

static int SysIoctl(struct Machine *m, int fildes, u64 request, i64 addr) {
  struct Fd *fd;
  int (*ioctl_impl)(int, unsigned long, ...);
  int (*tcgetattr_impl)(int, struct termios *);
  int (*tcsetattr_impl)(int, int, const struct termios *);
  LockFds(&m->system->fds);
  if ((fd = GetFd(&m->system->fds, fildes))) {
    unassert(fd->cb);
    unassert(ioctl_impl = fd->cb->ioctl);
    unassert(tcgetattr_impl = fd->cb->tcgetattr);
    unassert(tcsetattr_impl = fd->cb->tcsetattr);
  } else {
    ioctl_impl = 0;
    tcsetattr_impl = 0;
    tcgetattr_impl = 0;
  }
  UnlockFds(&m->system->fds);
  if (!fd) return -1;
  switch (request) {
    case TIOCGWINSZ_LINUX:
      return IoctlTiocgwinsz(m, fildes, addr, ioctl_impl);
    case TCGETS_LINUX:
      return IoctlTcgets(m, fildes, addr, tcgetattr_impl);
    case TCSETS_LINUX:
      return IoctlTcsets(m, fildes, TCSANOW, addr, tcsetattr_impl);
    case TCSETSW_LINUX:
      return IoctlTcsets(m, fildes, TCSADRAIN, addr, tcsetattr_impl);
    case TCSETSF_LINUX:
      return IoctlTcsets(m, fildes, TCSAFLUSH, addr, tcsetattr_impl);
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
    default:
      LOGF("missing ioctl %#" PRIx64, request);
      return einval();
  }
}

static i64 SysLseek(struct Machine *m, i32 fildes, i64 offset, int whence) {
  i64 rc;
  struct Fd *fd;
  if (!(fd = GetAndLockFd(m, fildes))) return -1;
  if (!fd->dirstream) {
    rc = lseek(fd->fildes, offset, XlatWhence(whence));
  } else if (whence == SEEK_SET_LINUX) {
    if (!offset) {
      rewinddir(fd->dirstream);
    } else {
      seekdir(fd->dirstream, offset);
    }
    rc = 0;
  } else {
    rc = einval();
  }
  UnlockFd(fd);
  return rc;
}

static i64 SysFtruncate(struct Machine *m, i32 fildes, i64 size) {
  i64 rc;
  if (CheckFdAccess(m, fildes, true, EINVAL) == -1) return -1;
  INTERRUPTIBLE(rc = ftruncate(fildes, size));
  return rc;
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

static int SysFaccessat(struct Machine *m, i32 dirfd, i64 path, i32 mode) {
  return faccessat(GetDirFildes(dirfd), LoadStr(m, path), XlatAccess(mode), 0);
}

static int SysFaccessat2(struct Machine *m, i32 dirfd, i64 path, i32 mode,
                         i32 flags) {
  return faccessat(GetDirFildes(dirfd), LoadStr(m, path), XlatAccess(mode),
                   XlatFaccessatFlags(flags));
}

static int SysFstat(struct Machine *m, i32 fd, i64 staddr) {
  int rc;
  struct stat st;
  struct stat_linux gst;
  if ((rc = fstat(fd, &st)) != -1) {
    XlatStatToLinux(&gst, &st);
    CopyToUserWrite(m, staddr, &gst, sizeof(gst));
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
#ifdef AT_EMPTY_PATH
  if (x & AT_EMPTY_PATH_LINUX) {
    res |= AT_EMPTY_PATH;
    x &= ~AT_EMPTY_PATH_LINUX;
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
  if (!(path = LoadStr(m, pathaddr))) return -1;
#ifndef AT_EMPTY_PATH
  if (flags & AT_EMPTY_PATH_LINUX) {
    flags &= AT_EMPTY_PATH_LINUX;
    if (!*path) {
      if (flags) {
        LOGF("%s() flags %d not supported", "fstatat(AT_EMPTY_PATH)", flags);
        return -1;
      }
      return SysFstat(m, dirfd, staddr);
    }
  }
#endif
  if ((rc = fstatat(GetDirFildes(dirfd), path, &st, XlatFstatatFlags(flags))) !=
      -1) {
    XlatStatToLinux(&gst, &st);
    CopyToUserWrite(m, staddr, &gst, sizeof(gst));
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
  if (x & AT_EMPTY_PATH_LINUX) {
    res |= AT_EMPTY_PATH;
    x &= ~AT_EMPTY_PATH_LINUX;
  }
#endif
  if (x) {
    LOGF("%s() flags %d not supported", "fchownat", x);
    return -1;
  }
  return res;
}

static int SysFchown(struct Machine *m, i32 fildes, u32 uid, u32 gid) {
  return fchown(fildes, uid, gid);
}

static int SysFchownat(struct Machine *m, i32 dirfd, i64 pathaddr, u32 uid,
                       u32 gid, i32 flags) {
  const char *path;
  if (!(path = LoadStr(m, pathaddr))) return -1;
#ifndef AT_EMPTY_PATH
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
  return fchownat(GetDirFildes(dirfd), path, uid, gid,
                  XlatFchownatFlags(flags));
}

static int SysChown(struct Machine *m, i64 pathaddr, u32 uid, u32 gid) {
  return SysFchownat(m, AT_FDCWD_LINUX, pathaddr, uid, gid, 0);
}

static int SysLchown(struct Machine *m, i64 pathaddr, u32 uid, u32 gid) {
  return SysFchownat(m, AT_FDCWD_LINUX, pathaddr, uid, gid,
                     AT_SYMLINK_NOFOLLOW_LINUX);
}

static int SysSync(struct Machine *m) {
  sync();
  return 0;
}

static int SysFsync(struct Machine *m, i32 fildes) {
#ifdef F_FULLSYNC
  int rc;
  // MacOS fsync() provides weaker guarantees than Linux fsync()
  // https://mjtsai.com/blog/2022/02/17/apple-ssd-benchmarks-and-f_fullsync/
  if ((rc = fcntl(fildes, F_FULLFSYNC, 0))) {
    // If the FULLFSYNC failed, fall back to attempting an fsync(). It
    // shouldn't be possible for fullfsync to fail on the local file
    // system (on OSX), so failure indicates that FULLFSYNC isn't
    // supported for this file system. So, attempt an fsync and (for
    // now) ignore the overhead of a superfluous fcntl call. It'd be
    // better to detect fullfsync support once and avoid the fcntl call
    // every time sync is called. ──Quoth SQLite (os_unix.c) It's also
    // possible for F_FULLFSYNC to fail on Cosmopolitan Libc when our
    // binary isn't running on MacOS.
    rc = fsync(fildes);
  }
  return rc;
#else
  return fsync(fildes);
#endif
}

static int SysFdatasync(struct Machine *m, i32 fildes) {
#ifdef F_FULLSYNC
  int rc;
  if ((rc = fcntl(fildes, F_FULLFSYNC, 0))) {
    rc = fsync(fildes);
  }
  return rc;
#elif defined(__APPLE__)
  // fdatasync() on HFS+ doesn't yet flush the file size if it changed
  // correctly so currently we default to the macro that redefines
  // fdatasync() to fsync(). ──Quoth SQLite (os_unix.c)
  return fsync(fildes);
#elif defined(__HAIKU__)
  // Haiku doesn't have fdatasync() yet
  return fsync(fildes);
#else
  return fdatasync(fildes);
#endif
}

static int SysChdir(struct Machine *m, i64 path) {
  return chdir(LoadStr(m, path));
}

static int SysFchdir(struct Machine *m, i32 fildes) {
  return fchdir(fildes);
}

static int SysFlock(struct Machine *m, i32 fd, i32 lock) {
  return flock(fd, XlatLock(lock));
}

static int SysShutdown(struct Machine *m, i32 fd, i32 how) {
  return shutdown(fd, XlatShutdown(how));
}

static int SysListen(struct Machine *m, i32 fd, i32 backlog) {
  return listen(fd, backlog);
}

static int SysMkdirat(struct Machine *m, i32 dirfd, i64 path, i32 mode) {
  return mkdirat(GetDirFildes(dirfd), LoadStr(m, path), mode);
}

static int SysMkdir(struct Machine *m, i64 path, i32 mode) {
  return SysMkdirat(m, AT_FDCWD_LINUX, path, mode);
}

static int SysFchmod(struct Machine *m, i32 fd, u32 mode) {
  return fchmod(fd, mode);
}

static int SysFchmodat(struct Machine *m, i32 dirfd, i64 path, u32 mode) {
  return fchmodat(GetDirFildes(dirfd), LoadStr(m, path), mode, 0);
}

static int SysFcntlLock(struct Machine *m, int systemfd, int cmd, i64 arg) {
  int rc;
  int whence;
  int syscmd;
  struct flock flock;
  struct flock_linux flock_linux;
  CopyFromUserRead(m, &flock_linux, arg, sizeof(flock_linux));
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
  INTERRUPTIBLE(rc = fcntl(systemfd, syscmd, &flock));
  if (rc != -1 && syscmd == F_GETLK) {
    if (flock.l_type == F_RDLCK) {
      Write16(flock_linux.type, F_RDLCK_LINUX);
    } else if (flock.l_type == F_WRLCK) {
      Write16(flock_linux.type, F_WRLCK_LINUX);
    } else {
      Write16(flock_linux.type, F_UNLCK_LINUX);
    }
    if (flock.l_whence == SEEK_END) {
      Write16(flock_linux.whence, SEEK_END_LINUX);
    } else if (flock.l_whence == SEEK_CUR) {
      Write16(flock_linux.whence, SEEK_CUR_LINUX);
    } else {
      Write16(flock_linux.whence, SEEK_SET_LINUX);
    }
    Write64(flock_linux.start, flock.l_start);
    Write64(flock_linux.len, flock.l_len);
    Write32(flock_linux.pid, flock.l_pid);
    CopyToUserWrite(m, arg, &flock_linux, sizeof(flock_linux));
  }
  return rc;
}

static int SysFcntl(struct Machine *m, i32 fildes, i32 cmd, i64 arg) {
  int rc, fl;
  struct Fd *fd;
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
  } else if (cmd == F_SETFD_LINUX) {
    if (!(arg & ~FD_CLOEXEC_LINUX)) {
      if (fcntl(fd->fildes, F_SETFD, arg ? FD_CLOEXEC : 0) != -1) {
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
    fl = XlatOpenFlags(arg & (O_APPEND_LINUX | O_ASYNC_LINUX | O_DIRECT_LINUX |
                              O_NOATIME_LINUX | O_NDELAY_LINUX));
    if (fcntl(fd->fildes, F_SETFL, fl) != -1) {
      fd->oflags &= ~SETFL_FLAGS;
      fd->oflags |= fl;
      rc = 0;
    } else {
      rc = -1;
    }
  } else if (cmd == F_SETLK_LINUX ||   //
             cmd == F_SETLKW_LINUX ||  //
             cmd == F_GETLK_LINUX) {
    rc = SysFcntlLock(m, fd->fildes, cmd, arg);
  } else {
    LOGF("missing fcntl() command %" PRId32, cmd);
    rc = einval();
  }
  UnlockFd(fd);
  return rc;
}

static ssize_t SysReadlinkat(struct Machine *m, int dirfd, i64 path,
                             i64 bufaddr, i64 size) {
  char *buf;
  ssize_t rc;
  if (size < 0) return einval();
  if (!(buf = (char *)malloc(size))) return -1;
  if ((rc = readlinkat(GetDirFildes(dirfd), LoadStr(m, path), buf, size)) !=
      -1) {
    CopyToUserWrite(m, bufaddr, buf, rc);
  }
  free(buf);
  return rc;
}

static int SysRmdir(struct Machine *m, i64 path) {
  return rmdir(LoadStr(m, path));
}

static int SysUnlink(struct Machine *m, i64 path) {
  return unlink(LoadStr(m, path));
}

static int SysRename(struct Machine *m, i64 srcpath, i64 dstpath) {
  return rename(LoadStr(m, srcpath), LoadStr(m, dstpath));
}

static int SysChmod(struct Machine *m, i64 path, u32 mode) {
  return SysFchmodat(m, AT_FDCWD_LINUX, path, mode);
}

static int SysTruncate(struct Machine *m, i64 path, u64 length) {
  int rc;
  INTERRUPTIBLE(rc = truncate(LoadStr(m, path), length));
  return rc;
}

static int SysSymlink(struct Machine *m, i64 targetpath, i64 linkpath) {
  return symlink(LoadStr(m, targetpath), LoadStr(m, linkpath));
}

static int SysSymlinkat(struct Machine *m, i64 targetpath, i32 newdirfd,
                        i64 linkpath) {
  return symlinkat(LoadStr(m, targetpath), GetDirFildes(newdirfd),
                   LoadStr(m, linkpath));
}

static int SysReadlink(struct Machine *m, i64 path, i64 bufaddr, u64 size) {
  return SysReadlinkat(m, AT_FDCWD_LINUX, path, bufaddr, size);
}

static int SysLink(struct Machine *m, i64 existingpath, i64 newpath) {
  return link(LoadStr(m, existingpath), LoadStr(m, newpath));
}

static int SysMknod(struct Machine *m, i64 path, i32 mode, u64 dev) {
  // TODO(jart): we can probably only emulate a subset of this call
  return mknod(LoadStr(m, path), mode, dev);
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
    LOGF("%s() flags %d not supported", "unlinkat", x);
    return -1;
  }
  return res;
}

static int SysUnlinkat(struct Machine *m, i32 dirfd, i64 path, i32 flags) {
  return unlinkat(GetDirFildes(dirfd), LoadStr(m, path),
                  XlatUnlinkatFlags(flags));
}

static int SysRenameat(struct Machine *m, int srcdirfd, i64 srcpath,
                       int dstdirfd, i64 dstpath) {
  return renameat(GetDirFildes(srcdirfd), LoadStr(m, srcpath),
                  GetDirFildes(dstdirfd), LoadStr(m, dstpath));
}

static bool IsFileExecutable(const char *path) {
  return !access(path, X_OK);
}

static bool CanEmulateExecutable(const char *prog) {
  int fd;
  bool res;
  ssize_t rc;
  char hdr[64];
  if (!IsFileExecutable(prog)) return false;
  if ((fd = open(prog, O_RDONLY | O_CLOEXEC)) == -1) return false;
  if ((rc = pread(fd, hdr, 64, 0)) == 64) {
    res = IsSupportedExecutable(prog, hdr);
  } else {
    res = false;
  }
  close(fd);
  return res;
}

static int SysExecve(struct Machine *m, i64 pa, i64 aa, i64 ea) {
  char *prog, **argv, **envp;
  prog = CopyStr(m, pa);
  argv = CopyStrList(m, aa);
  envp = CopyStrList(m, ea);
  LOCK(&m->system->exec_lock);
  SYS_LOGF("execve(%s)", prog);
  execve(prog, argv, envp);
  if (errno != ENOEXEC) return -1;
  if (m->system->exec && CanEmulateExecutable(prog)) {
    // TODO(jart): Prevent possibility of stack overflow.
    SYS_LOGF("m->system->exec(%s)", prog);
    SysCloseExec(m->system);
    _Exit(m->system->exec(prog, argv, envp));
  }
  UNLOCK(&m->system->exec_lock);
  errno = ENOEXEC;
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
  if ((options = XlatWait(options)) == -1) return -1;
#if !defined(__EMSCRIPTEN__)
  INTERRUPTIBLE(rc = wait4(pid, &wstatus, options, &hrusage));
#else
  memset(&hrusage, 0, sizeof(hrusage));
  INTERRUPTIBLE(rc = waitpid(pid, &wstatus, options));
#endif
  if (rc != -1) {
    if (opt_out_wstatus_addr) {
      if (WIFEXITED(wstatus)) {
        gwstatus = (WEXITSTATUS(wstatus) & 255) << 8;
      } else {
        unassert(WIFSIGNALED(wstatus));
        gwstatus = UnXlatSignal(WTERMSIG(wstatus)) & 127;
#ifdef WCOREDUMP
        if (WCOREDUMP(wstatus)) {
          gwstatus |= 128;
        }
#endif
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
    CopyToUserWrite(m, rusageaddr, &grusage, sizeof(grusage));
  }
  return rc;
}

static int SysGetrlimit(struct Machine *m, i32 resource, i64 rlimitaddr) {
  int rc;
  struct rlimit rlim;
  struct rlimit_linux lux;
  if ((rc = getrlimit(XlatResource(resource), &rlim)) != -1) {
    XlatRlimitToLinux(&lux, &rlim);
    CopyToUserWrite(m, rlimitaddr, &lux, sizeof(lux));
  }
  return rc;
}

static int SysSetrlimit(struct Machine *m, i32 resource, i64 rlimitaddr) {
  int sysresource;
  struct rlimit rlim;
  struct rlimit_linux lux;
  if ((sysresource = XlatResource(resource)) == -1) return -1;
  CopyFromUserRead(m, &lux, rlimitaddr, sizeof(lux));
  XlatLinuxToRlimit(sysresource, &rlim, &lux);
  return setrlimit(sysresource, &rlim);
}

static int SysPrlimit(struct Machine *m, i32 pid, i32 resource,
                      i64 new_rlimit_addr, i64 old_rlimit_addr) {
  int rc;
  struct rlimit old;
  struct rlimit_linux lux;
  if (pid && pid != m->system->pid) return eperm();
  if ((rc = getrlimit(XlatResource(resource), &old)) != -1) {
    if (new_rlimit_addr) {
      rc = SysSetrlimit(m, XlatResource(resource), new_rlimit_addr);
    }
    if (rc != -1 && old_rlimit_addr) {
      XlatRlimitToLinux(&lux, &old);
      CopyToUserWrite(m, old_rlimit_addr, &lux, sizeof(lux));
    }
  }
  return rc;
}

static int SysSysinfo(struct Machine *m, i64 siaddr) {
  struct sysinfo_linux si;
  if (sysinfo_linux(&si) == -1) return -1;
  CopyToUserWrite(m, siaddr, &si, sizeof(si));
  return 0;
}

static i64 SysGetcwd(struct Machine *m, i64 bufaddr, size_t size) {
  size_t n;
  char *buf;
  i64 res;
  if (!(buf = (char *)malloc(size))) return -1;
  if ((getcwd)(buf, size)) {
    n = strlen(buf) + 1;
    CopyToUserWrite(m, bufaddr, buf, n);
    res = bufaddr;
  } else {
    res = -1;
  }
  free(buf);
  return res;
}

static ssize_t SysGetrandom(struct Machine *m, i64 a, size_t n, int f) {
  char *p;
  ssize_t rc;
  int ignored;
  ignored = GRND_NONBLOCK_LINUX | GRND_RANDOM_LINUX;
  if ((f &= ~ignored)) {
    LOGF("%s() flags %d not supported", "getrandom", f);
    return einval();
  }
  if (n) {
    if (!(p = (char *)malloc(n))) return -1;
    INTERRUPTIBLE(rc = GetRandom(p, n));
    if (rc != -1) CopyToUserWrite(m, a, p, rc);
    free(p);
  } else {
    rc = 0;
  }
  return rc;
}

void OnSignal(int sig, siginfo_t *si, void *uc) {
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
  if (act) {
    CopyFromUserRead(m, &hand, act, sizeof(hand));
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
      m->signals &= ~(1ull << (sig - 1));
    }
    if ((syssig = XlatSignal(sig)) != -1 &&
        (~m->system->blinksigs & (1ull << (sig - 1)))) {
      unassert(syssig != SIGSEGV);
      sigfillset(&syshand.sa_mask);
      syshand.sa_flags = SA_SIGINFO;
      if (flags & SA_RESTART_LINUX) syshand.sa_flags |= SA_RESTART;
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
      unassert(!sigaction(syssig, &syshand, 0));
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
  struct itimerval neu, old;
  struct itimerval_linux git;
  CopyFromUserRead(m, &git, neuaddr, sizeof(git));
  XlatLinuxToItimerval(&neu, &git);
  if ((rc = setitimer(UnXlatItimer(which), &neu, &old)) != -1) {
    if (oldaddr) {
      XlatItimervalToLinux(&git, &old);
      CopyToUserWrite(m, oldaddr, &git, sizeof(git));
    }
  }
  return rc;
}

static int SysNanosleep(struct Machine *m, i64 req, i64 rem) {
  int rc;
  struct timespec hreq, hrem;
  struct timespec_linux gtimespec;
  CopyFromUserRead(m, &gtimespec, req, sizeof(gtimespec));
  hreq.tv_sec = Read64(gtimespec.sec);
  hreq.tv_nsec = Read64(gtimespec.nsec);
TryAgain:
  rc = nanosleep(&hreq, &hrem);
  if (rc == -1 && errno == EINTR) {
    if (CheckInterrupt(m)) {
      if (rem) {
        Write64(gtimespec.sec, hrem.tv_sec);
        Write64(gtimespec.nsec, hrem.tv_nsec);
        CopyToUserWrite(m, rem, &gtimespec, sizeof(gtimespec));
      }
    } else {
      hreq = hrem;
      goto TryAgain;
    }
  }
  return rc;
}

static int SysClockNanosleep(struct Machine *m, int clock, int flags,
                             i64 reqaddr, i64 remaddr) {
  int rc;
  struct timespec req, rem;
  struct timespec_linux gtimespec;
  if ((clock = XlatClock(clock)) == -1) return -1;
  if (flags & ~TIMER_ABSTIME_LINUX) return einval();
  CopyFromUserRead(m, &gtimespec, reqaddr, sizeof(gtimespec));
  req.tv_sec = Read64(gtimespec.sec);
  req.tv_nsec = Read64(gtimespec.nsec);
TryAgain:
#if defined(TIMER_ABSTIME) && !defined(__OpenBSD__)
  flags = flags & TIMER_ABSTIME_LINUX ? TIMER_ABSTIME : 0;
  if ((rc = clock_nanosleep(clock, flags, &req, &rem))) {
    errno = rc;
    rc = -1;
  }
#else
  if (!flags) {
    if (clock == CLOCK_REALTIME) {
      rc = nanosleep(&req, &rem);
    } else {
      rc = einval();
    }
  } else {
    struct timespec now;
    if (!(rc = clock_gettime(clock, &now))) {
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
    if (CheckInterrupt(m)) {
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

static int SysSigsuspend(struct Machine *m, i64 maskaddr, i64 sigsetsize) {
  u8 word[8];
  long nanos;
  u64 oldmask;
  struct timespec ts;
  SIG_LOGF("SysSigsuspend");
  if (sigsetsize != 8) return einval();
  CopyFromUserRead(m, word, maskaddr, 8);
  oldmask = m->sigmask;
  m->sigmask = Read64(word);
  nanos = 1;
  while (!CheckInterrupt(m)) {
    // TODO(jart): do better than expo backoff
    if (nanos > 256) {
      if (nanos < 10 * 1000) {
        sched_yield();
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

static int SysSigaltstack(struct Machine *m, i64 newaddr, i64 oldaddr) {
  bool isonstack;
  int supported, unsupported;
  const struct sigaltstack_linux *ss = 0;
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
    if (!(ss = (const struct sigaltstack_linux *)Schlep(m, newaddr,
                                                        sizeof(*ss)))) {
      LOGF("couldn't schlep new sigaltstack: %#" PRIx64, newaddr);
      return -1;
    }
    if ((unsupported = Read32(ss->flags) & ~supported)) {
      LOGF("unsupported sigaltstack flags: %#x", unsupported);
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
        return efault();
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
    CopyToUserWrite(m, oldaddr, &m->sigaltstack, sizeof(m->sigaltstack));
  }
  if (ss) {
    memcpy(&m->sigaltstack, ss, sizeof(*ss));
  }
  return 0;
}

static int SysClockGettime(struct Machine *m, int clock, i64 ts) {
  int rc;
  struct timespec htimespec;
  struct timespec_linux gtimespec;
  if ((rc = clock_gettime(XlatClock(clock), &htimespec)) != -1) {
    if (ts) {
      Write64(gtimespec.sec, htimespec.tv_sec);
      Write64(gtimespec.nsec, htimespec.tv_nsec);
      CopyToUserWrite(m, ts, &gtimespec, sizeof(gtimespec));
    }
  }
  return rc;
}

static int SysClockGetres(struct Machine *m, int clock, i64 ts) {
  int rc;
  struct timespec htimespec;
  struct timespec_linux gtimespec;
  if ((rc = clock_getres(XlatClock(clock), &htimespec)) != -1) {
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
  struct timeval htimeval;
  struct timezone htimezone;
  struct timeval_linux gtimeval;
  struct timezone_linux gtimezone;
  if ((rc = gettimeofday(&htimeval, tz ? &htimezone : 0)) != -1) {
    Write64(gtimeval.sec, htimeval.tv_sec);
    Write64(gtimeval.usec, htimeval.tv_usec);
    CopyToUserWrite(m, tv, &gtimeval, sizeof(gtimeval));
    if (tz) {
      Write32(gtimezone.minuteswest, htimezone.tz_minuteswest);
      Write32(gtimezone.dsttime, htimezone.tz_dsttime);
      CopyToUserWrite(m, tz, &gtimezone, sizeof(gtimezone));
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
    CopyToUserWrite(m, addr, buf, sizeof(buf));
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
  CopyToUserWrite(m, bufaddr, &gtms, sizeof(gtms));
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
  if (!timesaddr) return utimensat(AT_FDCWD, path, 0, 0);
  if ((t = (const struct utimbuf_linux *)Schlep(m, timesaddr, sizeof(*t)))) {
    ts[0].tv_sec = Read64(t->actime);
    ts[0].tv_nsec = 0;
    ts[1].tv_sec = Read64(t->modtime);
    ts[1].tv_nsec = 0;
    return utimensat(AT_FDCWD, path, ts, 0);
  } else {
    return -1;
  }
}

static int SysUtimes(struct Machine *m, i64 pathaddr, i64 tvsaddr) {
  const char *path;
  struct timespec ts[2];
  const struct timeval_linux *tv;
  if (!(path = LoadStr(m, pathaddr))) return -1;
  if (!tvsaddr) return utimensat(AT_FDCWD, path, 0, 0);
  if ((tv = (const struct timeval_linux *)Schlep(
           m, tvsaddr, sizeof(struct timeval_linux) * 2))) {
    ConvertUtimeTimevals(ts, tv);
    return utimensat(AT_FDCWD, path, ts, 0);
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
  if (!tvsaddr) return utimensat(GetDirFildes(dirfd), path, 0, 0);
  if ((tv = (const struct timeval_linux *)Schlep(
           m, tvsaddr, sizeof(struct timeval_linux) * 2))) {
    ConvertUtimeTimevals(ts, tv);
    return utimensat(GetDirFildes(dirfd), path, ts, 0);
  } else {
    return -1;
  }
}

static int SysUtimensat(struct Machine *m, i32 fd, i64 pathaddr, i64 tvsaddr,
                        i32 flags) {
  const char *path;
  struct timespec ts[2], *tsp;
  const struct timespec_linux *tv;
  if (!pathaddr) {
    path = 0;
  } else if (!(path = LoadStr(m, pathaddr))) {
    return -1;
  }
  if (tvsaddr) {
    if ((tv = (const struct timespec_linux *)Schlep(
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
    return utimensat(GetDirFildes(fd), path, tsp, flags);
  } else {
    if (flags) {
      LOGF("%s() flags %d not supported", "utimensat(path=null)", flags);
      return einval();
    }
    return futimens(fd, tsp);
  }
}

static int LoadFdSet(struct Machine *m, int nfds, fd_set *fds, i64 addr) {
  int fd;
  const u8 *p;
  if ((p = (const u8 *)Schlep(m, addr, ROUNDUP(nfds, 8) / 8))) {
    FD_ZERO(fds);
    for (fd = 0; fd < nfds; ++fd) {
      if (p[fd >> 3] & (1 << (fd & 7))) {
        FD_SET(fd, fds);
      }
    }
    return 0;
  } else {
    return -1;
  }
}

static int SaveFdSet(struct Machine *m, int nfds, const fd_set *fds, i64 addr) {
  u8 *p;
  int n, fd;
  n = ROUNDUP(nfds, 8) / 8;
  if (!(p = (u8 *)calloc(1, n))) return -1;
  for (fd = 0; fd < nfds; ++fd) {
    if (FD_ISSET(fd, fds)) {
      p[fd >> 3] |= 1 << (fd & 7);
    }
  }
  CopyToUserWrite(m, addr, p, n);
  free(p);
  return 0;
}

static i32 Select(struct Machine *m,          //
                  i32 nfds,                   //
                  i64 readfds_addr,           //
                  i64 writefds_addr,          //
                  i64 exceptfds_addr,         //
                  struct timespec *timeoutp,  //
                  const u64 *sigmaskp_guest) {
  int rc;
  i32 setsize;
  u64 oldmask_guest = 0;
  sigset_t block, oldmask;
  fd_set readfds, writefds, exceptfds;
  fd_set *readfdsp, *writefdsp, *exceptfdsp;
  struct timespec timeout, started, elapsed;
  if (timeoutp) {
    started = GetTime();
    timeout = *timeoutp;
  } else {
    memset(&timeout, 0, sizeof(timeout));
  }
  setsize = MIN(FD_SETSIZE, FD_SETSIZE_LINUX);
  if (nfds < 0 || nfds > setsize) {
    LOGF("select() nfds=%d can't exceed %d on this platform", nfds, setsize);
    return einval();
  }
  if (readfds_addr) {
    if (LoadFdSet(m, nfds, &readfds, readfds_addr) != -1) {
      readfdsp = &readfds;
    } else {
      LOGF("select() bad %s memory", "readfds");
      return -1;
    }
  } else {
    readfdsp = 0;
  }
  if (writefds_addr) {
    if (LoadFdSet(m, nfds, &writefds, writefds_addr) != -1) {
      writefdsp = &writefds;
    } else {
      LOGF("select() bad %s memory", "writefds");
      return -1;
    }
  } else {
    writefdsp = 0;
  }
  if (exceptfds_addr) {
    if (LoadFdSet(m, nfds, &exceptfds, exceptfds_addr) != -1) {
      exceptfdsp = &exceptfds;
    } else {
      LOGF("select() bad %s memory", "exceptfds");
      return -1;
    }
  } else {
    exceptfdsp = 0;
  }
  unassert(!sigfillset(&block));
  unassert(!pthread_sigmask(SIG_BLOCK, &block, &oldmask));
  if (sigmaskp_guest) {
    oldmask_guest = m->sigmask;
    m->sigmask = *sigmaskp_guest;
  }
  if (!CheckInterrupt(m)) {
    INTERRUPTIBLE(rc = pselect(nfds, readfdsp, writefdsp, exceptfdsp, timeoutp,
                               &oldmask));
  } else {
    rc = -1;
  }
  if (sigmaskp_guest) {
    m->sigmask = oldmask_guest;
  }
  unassert(!pthread_sigmask(SIG_SETMASK, &oldmask, 0));
  if (rc != -1) {
    if ((readfds_addr && SaveFdSet(m, nfds, &readfds, readfds_addr) == -1) ||
        (writefds_addr && SaveFdSet(m, nfds, &writefds, writefds_addr) == -1) ||
        (exceptfds_addr &&
         SaveFdSet(m, nfds, &exceptfds, exceptfds_addr) == -1)) {
      return -1;
    }
  }
  if (timeoutp) {
    elapsed = SubtractTime(GetTime(), started);
    if (CompareTime(elapsed, timeout) < 0) {
      *timeoutp = SubtractTime(timeout, elapsed);
    } else {
      memset(timeoutp, 0, sizeof(*timeoutp));
    }
  }
  return rc;
}

static i32 SysSelect(struct Machine *m, i32 nfds, i64 readfds_addr,
                     i64 writefds_addr, i64 exceptfds_addr, i64 timeout_addr) {
  i32 rc;
  struct timespec timeout, *timeoutp;
  struct timeval_linux timeout_linux;
  const struct timeval_linux *timeoutp_linux;
  if (timeout_addr) {
    if ((timeoutp_linux = (const struct timeval_linux *)Schlep(
             m, timeout_addr, sizeof(*timeoutp_linux)))) {
      timeout.tv_sec = Read64(timeoutp_linux->sec);
      timeout.tv_nsec = Read64(timeoutp_linux->usec);
      if (0 <= timeout.tv_nsec && timeout.tv_nsec < 1000000) {
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
  if (timeout_addr && (rc != -1 || errno == EINTR)) {
    Write64(timeout_linux.sec, timeout.tv_sec);
    Write64(timeout_linux.usec, (timeout.tv_nsec + 999) / 1000);
    CopyToUserWrite(m, timeout_addr, &timeout_linux, sizeof(timeout_linux));
  }
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
  struct timespec_linux timeout_linux;
  const struct timespec_linux *timeoutp_linux;
  if (timeout_addr) {
    if ((timeoutp_linux = (const struct timespec_linux *)Schlep(
             m, timeout_addr, sizeof(*timeoutp_linux)))) {
      timeout.tv_sec = Read64(timeoutp_linux->sec);
      timeout.tv_nsec = Read64(timeoutp_linux->nsec);
      timeoutp = &timeout;
    } else {
      return -1;
    }
  } else {
    timeoutp = 0;
    memset(&timeout, 0, sizeof(timeout));
  }
  if (pselect6_addr) {
    if ((ps = (const struct pselect6_linux *)Schlep(m, pselect6_addr,
                                                    sizeof(*ps)))) {
      if (Read64(ps->sigmaskaddr)) {
        if (Read64(ps->sigmasksize) == 8) {
          if ((sm = (const struct sigset_linux *)Schlep(
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
  if (timeout_addr && (rc != -1 || errno == EINTR)) {
    Write64(timeout_linux.sec, timeout.tv_sec);
    Write64(timeout_linux.nsec, timeout.tv_nsec);
    CopyToUserWrite(m, timeout_addr, &timeout_linux, sizeof(timeout_linux));
  }
  return rc;
}

static int SysPoll(struct Machine *m, i64 fdsaddr, u64 nfds, i32 timeout_ms) {
  long i;
  u64 gfdssize;
  struct Fd *fd;
  int fildes, rc, ev;
  struct pollfd hfds[1];
  struct timeval ts1, ts2;
  struct pollfd_linux *gfds;
  long wait, elapsed, timeout;
  int (*poll_impl)(struct pollfd *, nfds_t, int);
  timeout = timeout_ms * 1000L;
  if (!mulo(nfds, sizeof(struct pollfd_linux), &gfdssize) &&
      gfdssize <= 0x7ffff000) {
    if ((gfds = (struct pollfd_linux *)malloc(gfdssize))) {
      rc = 0;
      CopyFromUserRead(m, gfds, fdsaddr, gfdssize);
      unassert(!gettimeofday(&ts1, 0));
      for (;;) {
        for (i = 0; i < nfds; ++i) {
        TryAgain:
          if (CheckInterrupt(m)) {
            rc = eintr();
            break;
          }
          fildes = Read32(gfds[i].fd);
          LockFds(&m->system->fds);
          if ((fd = GetFd(&m->system->fds, fildes))) {
            unassert(fd->cb);
            unassert(poll_impl = fd->cb->poll);
          } else {
            poll_impl = 0;
          }
          UnlockFds(&m->system->fds);
          if (fd) {
            hfds[0].fd = fildes;
            ev = Read16(gfds[i].events);
            hfds[0].events = (((ev & POLLIN_LINUX) ? POLLIN : 0) |
                              ((ev & POLLOUT_LINUX) ? POLLOUT : 0) |
                              ((ev & POLLPRI_LINUX) ? POLLPRI : 0));
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
        if (rc || !timeout) break;
        wait = kPollingMs * 1000;
        if (timeout < 0) {
          usleep(wait);
        } else {
          unassert(!gettimeofday(&ts2, 0));
          elapsed = TimevalTomicros(TimevalSub(ts2, ts1));
          if (elapsed >= timeout) {
            break;
          }
          if (timeout - elapsed < wait) {
            wait = timeout - elapsed;
          }
          usleep(wait);
        }
      }
      if (rc != -1) {
        CopyToUserWrite(m, fdsaddr, gfds, nfds * sizeof(*gfds));
      }
    } else {
      rc = enomem();
    }
    free(gfds);
    return rc;
  } else {
    return einval();
  }
}

static int SysSigprocmask(struct Machine *m, int how, i64 setaddr,
                          i64 oldsetaddr, u64 sigsetsize) {
  u64 set;
  u8 word[8];
  if (sigsetsize != 8) return einval();
  if (oldsetaddr) {
    Write64(word, m->sigmask);
    CopyToUserWrite(m, oldsetaddr, word, 8);
  }
  if (setaddr) {
    CopyFromUserRead(m, word, setaddr, 8);
    set = Read64(word);
    if (how == SIG_BLOCK_LINUX) {
      m->sigmask |= set;
    } else if (how == SIG_UNBLOCK_LINUX) {
      m->sigmask &= ~set;
    } else if (how == SIG_SETMASK_LINUX) {
      m->sigmask = set;
    } else {
      return einval();
    }
  }
  return 0;
}

static int SysKill(struct Machine *m, int pid, int sig) {
  return kill(pid, sig ? XlatSignal(sig) : 0);
}

static bool IsValidThreadId(struct System *s, int tid) {
  return tid == s->pid ||
         (kMinThreadId <= tid && tid < kMinThreadId + kMaxThreadIds);
}

static int SysTkill(struct Machine *m, int tid, int sig) {
  int err;
  bool found;
  struct Dll *e;
  struct Machine *m2;
  if (!(0 <= sig && sig <= 64)) {
    LOGF("tkill(%d, %d) failed due to bogus signal", tid, sig);
    return einval();
  }
  if (!IsValidThreadId(m->system, tid)) {
    LOGF("tkill(%d, %d) failed due to bogus thread id", tid, sig);
    return esrch();
  }
  err = 0;
  LOCK(&m->system->machines_lock);
  for (e = dll_first(m->system->machines); e;
       e = dll_next(m->system->machines, e)) {
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
  if (!sig && !found) err = ESRCH;
  if (!err) {
    return 0;
  } else {
    LOGF("tkill(%d, %d) failed: %s", tid, sig, strerror(err));
    errno = err;
    return -1;
  }
}

static int SysTgkill(struct Machine *m, int pid, int tid, int sig) {
  if (pid < 1 || tid < 1) return einval();
  if (pid != m->system->pid) return eperm();
  return SysTkill(m, tid, sig);
}

static int SysPause(struct Machine *m) {
  int rc;
  INTERRUPTIBLE(rc = pause());
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
  return getuid();
}

static int SysGetgid(struct Machine *m) {
  return getgid();
}

static int SysGeteuid(struct Machine *m) {
  return geteuid();
}

static int SysGetegid(struct Machine *m) {
  return getegid();
}

static int SysSchedYield(struct Machine *m) {
  return sched_yield();
}

static int SysUmask(struct Machine *m, int mask) {
  return umask(mask);
}

static int SysSetuid(struct Machine *m, int uid) {
  return setuid(uid);
}

static int SysSetgid(struct Machine *m, int gid) {
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

static int SysStat(struct Machine *m, i64 path, i64 st) {
  return SysFstatat(m, AT_FDCWD_LINUX, path, st, 0);
}

static int SysLstat(struct Machine *m, i64 path, i64 st) {
  return SysFstatat(m, AT_FDCWD_LINUX, path, st, 0x0400);
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

void OpSyscall(P) {
  u64 ax, di, si, dx, r0, r8, r9;
  STATISTIC(++syscalls);
  if (m->system->redraw) {
    m->system->redraw(true);
  }
  ax = Get64(m->ax);
  di = Get64(m->di);
  si = Get64(m->si);
  dx = Get64(m->dx);
  r0 = Get64(m->r10);
  r8 = Get64(m->r8);
  r9 = Get64(m->r9);
  m->interrupted = false;
  switch (ax & 0xfff) {
    SYSCALL(0x000, SysRead, (m, di, si, dx));
    SYSCALL(0x001, SysWrite, (m, di, si, dx));
    SYSCALL(0x002, SysOpen, (m, di, si, dx));
    SYSCALL(0x003, SysClose, (m->system, di));
    SYSCALL(0x004, SysStat, (m, di, si));
    SYSCALL(0x005, SysFstat, (m, di, si));
    SYSCALL(0x006, SysLstat, (m, di, si));
    SYSCALL(0x007, SysPoll, (m, di, si, dx));
    SYSCALL(0x008, SysLseek, (m, di, si, dx));
    SYSCALL(0x009, SysMmap, (m, di, si, dx, r0, r8, r9));
    SYSCALL(0x011, SysPread, (m, di, si, dx, r0));
    SYSCALL(0x012, SysPwrite, (m, di, si, dx, r0));
    SYSCALL(0x017, SysSelect, (m, di, si, dx, r0, r8));
    SYSCALL(0x10E, SysPselect, (m, di, si, dx, r0, r8, r9));
    SYSCALL(0x01A, SysMsync, (m, di, si, dx));
    SYSCALL(0x00A, SysMprotect, (m, di, si, dx));
    SYSCALL(0x00B, SysMunmap, (m, di, si));
    SYSCALL(0x00C, SysBrk, (m, di));
    SYSCALL(0x00D, SysSigaction, (m, di, si, dx, r0));
    SYSCALL(0x00E, SysSigprocmask, (m, di, si, dx, r0));
    SYSCALL(0x010, SysIoctl, (m, di, si, dx));
    SYSCALL(0x013, SysReadv, (m, di, si, dx));
    SYSCALL(0x014, SysWritev, (m, di, si, dx));
    SYSCALL(0x015, SysAccess, (m, di, si));
    SYSCALL(0x016, SysPipe, (m, di, 0));
    SYSCALL(0x125, SysPipe, (m, di, si));
    SYSCALL(0x018, SysSchedYield, (m));
    SYSCALL(0x01C, SysMadvise, (m, di, si, dx));
    SYSCALL(0x020, SysDup1, (m, di));
    SYSCALL(0x021, SysDup2, (m, di, si));
    SYSCALL(0x124, SysDup3, (m, di, si, dx));
    SYSCALL(0x022, SysPause, (m));
    SYSCALL(0x023, SysNanosleep, (m, di, si));
    SYSCALL(0x024, SysGetitimer, (m, di, si));
    SYSCALL(0x025, SysAlarm, (m, di));
    SYSCALL(0x026, SysSetitimer, (m, di, si, dx));
    SYSCALL(0x027, SysGetpid, (m));
    SYSCALL(0x0BA, SysGettid, (m));
    SYSCALL(0x029, SysSocket, (m, di, si, dx));
    SYSCALL(0x02A, SysConnect, (m, di, si, dx));
    SYSCALL(0x02B, SysAccept, (m, di, si, dx));
    SYSCALL(0x02C, SysSendto, (m, di, si, dx, r0, r8, r9));
    SYSCALL(0x02D, SysRecvfrom, (m, di, si, dx, r0, r8, r9));
    SYSCALL(0x030, SysShutdown, (m, di, si));
    SYSCALL(0x031, SysBind, (m, di, si, dx));
    SYSCALL(0x032, SysListen, (m, di, si));
    SYSCALL(0x033, SysGetsockname, (m, di, si, dx));
    SYSCALL(0x034, SysGetpeername, (m, di, si, dx));
    SYSCALL(0x036, SysSetsockopt, (m, di, si, dx, r0, r8));
    SYSCALL(0x037, SysGetsockopt, (m, di, si, dx, r0, r8));
    SYSCALL(0x038, SysClone, (m, di, si, dx, r0, r8, r9));
    SYSCALL(0x039, SysFork, (m));
    SYSCALL(0x03A, SysVfork, (m));
    SYSCALL(0x03B, SysExecve, (m, di, si, dx));
    SYSCALL(0x03D, SysWait4, (m, di, si, dx, r0));
    SYSCALL(0x03E, SysKill, (m, di, si));
    SYSCALL(0x03F, SysUname, (m, di));
    SYSCALL(0x048, SysFcntl, (m, di, si, dx));
    SYSCALL(0x049, SysFlock, (m, di, si));
    SYSCALL(0x04A, SysFsync, (m, di));
    SYSCALL(0x04B, SysFdatasync, (m, di));
    SYSCALL(0x04C, SysTruncate, (m, di, si));
    SYSCALL(0x04D, SysFtruncate, (m, di, si));
    SYSCALL(0x04F, SysGetcwd, (m, di, si));
    SYSCALL(0x050, SysChdir, (m, di));
    SYSCALL(0x051, SysFchdir, (m, di));
    SYSCALL(0x052, SysRename, (m, di, si));
    SYSCALL(0x053, SysMkdir, (m, di, si));
    SYSCALL(0x054, SysRmdir, (m, di));
    SYSCALL(0x055, SysCreat, (m, di, si));
    SYSCALL(0x056, SysLink, (m, di, si));
    SYSCALL(0x057, SysUnlink, (m, di));
    SYSCALL(0x058, SysSymlink, (m, di, si));
    SYSCALL(0x059, SysReadlink, (m, di, si, dx));
    SYSCALL(0x05A, SysChmod, (m, di, si));
    SYSCALL(0x05B, SysFchmod, (m, di, si));
    SYSCALL(0x05C, SysChown, (m, di, si, dx));
    SYSCALL(0x05D, SysFchown, (m, di, si, dx));
    SYSCALL(0x05E, SysLchown, (m, di, si, dx));
    SYSCALL(0x104, SysFchownat, (m, di, si, dx, r0, r8));
    SYSCALL(0x05F, SysUmask, (m, di));
    SYSCALL(0x060, SysGettimeofday, (m, di, si));
    SYSCALL(0x061, SysGetrlimit, (m, di, si));
    SYSCALL(0x062, SysGetrusage, (m, di, si));
    SYSCALL(0x063, SysSysinfo, (m, di));
    SYSCALL(0x064, SysTimes, (m, di));
    SYSCALL(0x06F, SysGetpgrp, (m));
    SYSCALL(0x070, SysSetsid, (m));
    SYSCALL(0x07C, SysGetsid, (m, di));
    SYSCALL(0x079, SysGetpgid, (m, di));
    SYSCALL(0x06D, SysSetpgid, (m, di, si));
    SYSCALL(0x066, SysGetuid, (m));
    SYSCALL(0x068, SysGetgid, (m));
    SYSCALL(0x069, SysSetuid, (m, di));
    SYSCALL(0x06A, SysSetgid, (m, di));
    SYSCALL(0x06B, SysGeteuid, (m));
    SYSCALL(0x06C, SysGetegid, (m));
    SYSCALL(0x06E, SysGetppid, (m));
    SYSCALL(0x082, SysSigsuspend, (m, di, si));
    SYSCALL(0x083, SysSigaltstack, (m, di, si));
    SYSCALL(0x085, SysMknod, (m, di, si, dx));
    SYSCALL(0x08C, SysGetpriority, (m, di, si));
    SYSCALL(0x08D, SysSetpriority, (m, di, si, dx));
    SYSCALL(0x08E, SysSchedSetparam, (m, di, si));
    SYSCALL(0x08F, SysSchedGetparam, (m, di, si));
    SYSCALL(0x090, SysSchedSetscheduler, (m, di, si, dx));
    SYSCALL(0x091, SysSchedGetscheduler, (m, di));
    SYSCALL(0x092, SysSchedGetPriorityMax, (m, di));
    SYSCALL(0x093, SysSchedGetPriorityMin, (m, di));
    SYSCALL(0x09D, SysPrctl, (m, di, si, dx, r0, r8));
    SYSCALL(0x09E, SysArchPrctl, (m, di, si));
    SYSCALL(0x0A0, SysSetrlimit, (m, di, si));
    SYSCALL(0x0A2, SysSync, (m));
    SYSCALL(0x0C8, SysTkill, (m, di, si));
    SYSCALL(0x0C9, SysTime, (m, di));
    SYSCALL(0x0CA, SysFutex, (m, di, si, dx, r0, r8, r9));
    SYSCALL(0x0CB, SysSchedSetaffinity, (m, di, si, dx));
    SYSCALL(0x0CC, SysSchedGetaffinity, (m, di, si, dx));
    SYSCALL(0x0D9, SysGetdents, (m, di, si, dx));
    SYSCALL(0x0DA, SysSetTidAddress, (m, di));
    SYSCALL(0x0DD, SysFadvise, (m, di, si, dx, r0));
    SYSCALL(0x0E4, SysClockGettime, (m, di, si));
    SYSCALL(0x0E5, SysClockGetres, (m, di, si));
    SYSCALL(0x0E6, SysClockNanosleep, (m, di, si, dx, r0));
    SYSCALL(0x0EA, SysTgkill, (m, di, si, dx));
    SYSCALL(0x084, SysUtime, (m, di, si));
    SYSCALL(0x0EB, SysUtimes, (m, di, si));
    SYSCALL(0x105, SysFutimesat, (m, di, si, dx));
    SYSCALL(0x118, SysUtimensat, (m, di, si, dx, r0));
    SYSCALL(0x101, SysOpenat, (m, di, si, dx, r0));
    SYSCALL(0x102, SysMkdirat, (m, di, si, dx));
    SYSCALL(0x106, SysFstatat, (m, di, si, dx, r0));
    SYSCALL(0x107, SysUnlinkat, (m, di, si, dx));
    SYSCALL(0x108, SysRenameat, (m, di, si, dx, r0));
    SYSCALL(0x10A, SysSymlinkat, (m, di, si, dx));
    SYSCALL(0x10B, SysReadlinkat, (m, di, si, dx, r0));
    SYSCALL(0x10C, SysFchmodat, (m, di, si, dx));
    SYSCALL(0x10D, SysFaccessat, (m, di, si, dx));
    SYSCALL(0x1b7, SysFaccessat2, (m, di, si, dx, r0));
    SYSCALL(0x120, SysAccept4, (m, di, si, dx, r0));
    SYSCALL(0x111, SysSetRobustList, (m, di, si));
    SYSCALL(0x112, SysGetRobustList, (m, di, si, dx));
    SYSCALL(0x12E, SysPrlimit, (m, di, si, dx, r0));
    SYSCALL(0x13E, SysGetrandom, (m, di, si, dx));
    SYSCALL(0x1B4, SysCloseRange, (m->system, di, si, dx));
    case 0x3C:
      SysExit(m, di);
    case 0xE7:
      SysExitGroup(m, di);
    case 0x00F:
      SigRestore(m);
      return;
    case 0x500:
      ax = enosys();
      break;
    default:
      LOGF("missing syscall 0x%03" PRIx64, ax);
      ax = enosys();
      break;
  }
  if (!m->interrupted) {
    SYS_LOGF("system call returned %" PRId64 " %s", ax,
             ax == -1 ? strerror(errno) : "");
    Put64(m->ax, ax != -1 ? ax : -(XlatErrno(errno) & 0xfff));
  } else {
    SYS_LOGF("system call interrupted");
  }
  CollectGarbage(m);
}
