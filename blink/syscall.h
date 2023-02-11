#ifndef BLINK_SYSCALL_H_
#define BLINK_SYSCALL_H_
#include <fcntl.h>

#include "blink/fds.h"
#include "blink/machine.h"
#include "blink/types.h"

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

#define INTERRUPTIBLE(restartable, x)       \
  do {                                      \
    int rc_;                                \
    rc_ = (x);                              \
    if (rc_ == -1 && errno == EINTR) {      \
      if (CheckInterrupt(m, restartable)) { \
        break;                              \
      }                                     \
    } else {                                \
      break;                                \
    }                                       \
  } while (1)

#define RESTARTABLE(x) INTERRUPTIBLE(true, x)

#define NORESTART(rc, x)             \
  do {                               \
    if (!CheckInterrupt(m, false)) { \
      INTERRUPTIBLE(false, rc = x);  \
    } else {                         \
      rc = -1;                       \
    }                                \
  } while (0)

extern char *g_blink_path;

void OpSyscall(P);

void SysCloseExec(struct System *);
int SysClose(struct Machine *, i32);
int SysCloseRange(struct Machine *, u32, u32, u32);
int SysDup(struct Machine *, i32, i32, i32, i32);
int SysOpenat(struct Machine *, i32, i64, i32, i32);
int SysPipe2(struct Machine *, i64, i32);
int SysIoctl(struct Machine *, int, u64, i64);
_Noreturn void SysExitGroup(struct Machine *, int);
_Noreturn void SysExit(struct Machine *, int);

int GetDirFildes(int);
void AddStdFd(struct Fds *, int);
int GetOflags(struct Machine *, int);
int GetFildes(struct Machine *, int);
struct Fd *GetAndLockFd(struct Machine *, int);
bool CheckInterrupt(struct Machine *, bool);
int SysStatfs(struct Machine *, i64, i64);
int SysFstatfs(struct Machine *, i32, i64);

#endif /* BLINK_SYSCALL_H_ */
