#ifndef BLINK_SYSCALL_H_
#define BLINK_SYSCALL_H_
#include "blink/fds.h"
#include "blink/machine.h"
#include "blink/types.h"

extern char *g_blink_path;

void OpSyscall(P);

int SysClose(struct System *, i32);
int SysCloseRange(struct System *, u32, u32, u32);
int SysDup(struct Machine *, i32, i32, i32, i32);
int SysOpenat(struct Machine *, i32, i64, i32, i32);
int SysPipe(struct Machine *, i64, i32);

void AddStdFd(struct Fds *, int);
int GetAfd(struct Machine *, int, struct Fd **);
void DropFd(struct Machine *, struct Fd *);
int GetFildes(struct Machine *, int);
struct Fd *GetAndLockFd(struct Machine *, int);

#endif /* BLINK_SYSCALL_H_ */
