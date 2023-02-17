#ifndef BLINK_SYSCALL_MACRO_H_
#define BLINK_SYSCALL_MACRO_H_
#include "blink/flag.h"
#include "blink/strace.h"

#define SYSCALL0(magnum, name, func, signature) \
  case magnum:                                  \
    if (STRACE && FLAG_strace) {                \
      EnterStrace(m, name, signature);          \
    }                                           \
    ax = func(m);                               \
    if (STRACE && FLAG_strace) {                \
      LeaveStrace(m, name, signature, ax);      \
    }                                           \
    break

#define SYSCALL1(magnum, name, func, signature) \
  case magnum:                                  \
    if (STRACE && FLAG_strace) {                \
      EnterStrace(m, name, signature, di);      \
    }                                           \
    ax = func(m, di);                           \
    if (STRACE && FLAG_strace) {                \
      LeaveStrace(m, name, signature, ax, di);  \
    }                                           \
    break

#define SYSCALL2(magnum, name, func, signature)    \
  case magnum:                                     \
    if (STRACE && FLAG_strace) {                   \
      EnterStrace(m, name, signature, di, si);     \
    }                                              \
    ax = func(m, di, si);                          \
    if (STRACE && FLAG_strace) {                   \
      LeaveStrace(m, name, signature, ax, di, si); \
    }                                              \
    break

#define SYSCALL3(magnum, name, func, signature)        \
  case magnum:                                         \
    if (STRACE && FLAG_strace) {                       \
      EnterStrace(m, name, signature, di, si, dx);     \
    }                                                  \
    ax = func(m, di, si, dx);                          \
    if (STRACE && FLAG_strace) {                       \
      LeaveStrace(m, name, signature, ax, di, si, dx); \
    }                                                  \
    break

#define SYSCALL4(magnum, name, func, signature)            \
  case magnum:                                             \
    if (STRACE && FLAG_strace) {                           \
      EnterStrace(m, name, signature, di, si, dx, r0);     \
    }                                                      \
    ax = func(m, di, si, dx, r0);                          \
    if (STRACE && FLAG_strace) {                           \
      LeaveStrace(m, name, signature, ax, di, si, dx, r0); \
    }                                                      \
    break

#define SYSCALL5(magnum, name, func, signature)                \
  case magnum:                                                 \
    if (STRACE && FLAG_strace) {                               \
      EnterStrace(m, name, signature, di, si, dx, r0, r8);     \
    }                                                          \
    ax = func(m, di, si, dx, r0, r8);                          \
    if (STRACE && FLAG_strace) {                               \
      LeaveStrace(m, name, signature, ax, di, si, dx, r0, r8); \
    }                                                          \
    break

#define SYSCALL6(magnum, name, func, signature)                    \
  case magnum:                                                     \
    if (STRACE && FLAG_strace) {                                   \
      EnterStrace(m, name, signature, di, si, dx, r0, r8, r9);     \
    }                                                              \
    ax = func(m, di, si, dx, r0, r8, r9);                          \
    if (STRACE && FLAG_strace) {                                   \
      LeaveStrace(m, name, signature, ax, di, si, dx, r0, r8, r9); \
    }                                                              \
    break

#endif /* BLINK_SYSCALL_MACRO_H_ */
