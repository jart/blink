#ifndef BLINK_STRACE_H_
#define BLINK_STRACE_H_
#include "blink/builtin.h"
#include "blink/machine.h"
#include "blink/types.h"

#if !defined(DISABLE_STRACE) && !defined(TINY)
#define STRACE 1
#else
#define STRACE 0
#endif

#define IS_INT(c)    (0000 == (0200 & (c)))
#define IS_I32(c)    (0000 == (0300 & (c)))
#define IS_I64(c)    (0100 == (0300 & (c)))
#define IS_MEM(c)    (0200 == (0200 & (c)))
#define IS_MEM_I(c)  (0200 == (0340 & (c)))
#define IS_MEM_O(c)  (0300 == (0340 & (c)))
#define IS_MEM_IO(c) (0340 == (0340 & (c)))
#define IS_BUF(c)    (0200 == (0237 & (c)))
#define IS_TIME(c)   (0202 == (0237 & (c)))
#define IS_HAND(c)   (0203 == (0237 & (c)))
#define IS_ADDR(c)   (0210 == (0237 & (c)))
#define IS_SIGSET(c) (0204 == (0237 & (c)))

#define ST_HAS(s, f) \
  (f(s[1]) || f(s[2]) || f(s[3]) || f(s[4]) || f(s[5]) || f(s[6]))

#define UN             "\000"
#define I32            "\001"
#define I32_FD         "\002"
#define I32_DIRFD      "\003"
#define I32_OCTAL      "\004"
#define I32_OFLAGS     "\005"
#define I32_WHENCE     "\006"
#define I32_ATFLAGS    "\007"
#define I32_PROT       "\010"
#define I32_MAPFLAGS   "\011"
#define I32_MSFLAGS    "\012"
#define I32_SIG        "\013"
#define I32_RENFLAGS   "\014"
#define I32_CLONEFLAGS "\015"
#define I32_CLOCK      "\016"
#define I32_WAITFLAGS  "\017"
#define I32_FAMILY     "\020"
#define I32_SOCKTYPE   "\021"
#define I32_SOCKFLAGS  "\022"
#define I64            "\100"
#define I64_HEX        "\101"
#define I_BUF          "\200"
#define I_STR          "\201"
#define I_TIME         "\202"
#define I_HAND         "\203"
#define I_SIGSET       "\204"
#define I_ADDR         "\210"
#define O_BUF          "\300"
#define O_STAT         "\301"
#define O_TIME         "\302"
#define O_HAND         "\303"
#define O_SIGSET       "\304"
#define O_PFDS         "\306"
#define O_WSTATUS      "\307"
#define O_ADDR         "\310"
#define IO_POLL        "\340"
#define IO_TIME        "\342"
#define IO_TIMEV       "\343"
#define WAT_IOCTL      "\375"
#define WAT_FCNTL      "\376"
#define WAT_PSELECT    "\377"

#define RC0        I32
#define PID        I32
#define UID        I32
#define GID        I32
#define ADDRLEN    I32
#define FD         I32_FD
#define DIRFD      I32_DIRFD
#define SIG        I32_SIG
#define MODE       I32_OCTAL
#define PROT       I32_PROT
#define CLOCK      I32_CLOCK
#define WHENCE     I32_WHENCE
#define OFLAGS     I32_OFLAGS
#define MSFLAGS    I32_MSFLAGS
#define ATFLAGS    I32_ATFLAGS
#define FAMILY     I32_FAMILY
#define SOCKTYPE   I32_SOCKTYPE
#define MAPFLAGS   I32_MAPFLAGS
#define RENFLAGS   I32_RENFLAGS
#define WAITFLAGS  I32_WAITFLAGS
#define SOCKFLAGS  I32_SOCKFLAGS
#define CLONEFLAGS I32_CLONEFLAGS
#define SIZE       I64_HEX
#define SSIZE      I64
#define OFF        I64_HEX
#define HEX        I64_HEX
#define PTR        I64_HEX
#define STR        I_STR
#define PATH       I_STR
#define BUFSZ      I64_HEX
#define O_RUSAGE   I64_HEX

// clang-format off
//      SYSCALL             RET    ARG1       ARG2       ARG3      ARG4      ARG5     ARG6
#define STRACE_0            HEX    UN         UN         UN        UN        UN       UN
#define STRACE_1            HEX    HEX        UN         UN        UN        UN       UN
#define STRACE_2            HEX    HEX        HEX        UN        UN        UN       UN
#define STRACE_3            HEX    HEX        HEX        HEX       UN        UN       UN
#define STRACE_4            HEX    HEX        HEX        HEX       HEX       UN       UN
#define STRACE_5            HEX    HEX        HEX        HEX       HEX       HEX      UN
#define STRACE_6            HEX    HEX        HEX        HEX       HEX       HEX      HEX
#define STRACE_READ         SSIZE  FD         O_BUF      BUFSZ     UN        UN       UN
#define STRACE_WRITE        SSIZE  FD         I_BUF      BUFSZ     UN        UN       UN
#define STRACE_OPEN         I32    PATH       OFLAGS     MODE      UN        UN       UN
#define STRACE_OPENAT       I32    DIRFD      STR        OFLAGS    MODE      UN       UN
#define STRACE_CREAT        I32    PATH       MODE       UN        UN        UN       UN
#define STRACE_CLOSE        RC0    FD         UN         UN        UN        UN       UN
#define STRACE_STAT         RC0    PATH       STAT       UN        UN        UN       UN
#define STRACE_FSTAT        RC0    FD         STAT       UN        UN        UN       UN
#define STRACE_LSTAT        RC0    PATH       STAT       UN        UN        UN       UN
#define STRACE_POLL         I32    IO_POLL    I32        UN        UN        UN       UN
#define STRACE_PPOLL        RC0    IO_POLL    IO_TIME    I_SIGSET  SSIZE     UN       UN
#define STRACE_LSEEK        OFF    FD         OFF        WHENCE    UN        UN       UN
#define STRACE_MMAP         PTR    PTR        SIZE       PROT      MAPFLAGS  FD       OFF
#define STRACE_PAUSE        RC0    UN         UN         UN        UN        UN       UN
#define STRACE_SYNC         RC0    UN         UN         UN        UN        UN       UN
#define STRACE_FSYNC        RC0    FD         UN         UN        UN        UN       UN
#define STRACE_FDATASYNC    RC0    FD         UN         UN        UN        UN       UN
#define STRACE_DUP          I32    FD         UN         UN        UN        UN       UN
#define STRACE_DUP2         I32    FD         I32        UN        UN        UN       UN
#define STRACE_DUP3         I32    FD         I32        OFLAGS    UN        UN       UN
#define STRACE_MSYNC        RC0    PTR        SIZE       MSFLAGS   UN        UN       UN
#define STRACE_ALARM        I32    I32        UN         UN        UN        UN       UN
#define STRACE_GETCWD       SSIZE  O_BUF      BUFSZ      UN        UN        UN       UN
#define STRACE_TRUNCATE     RC0    STR        OFF        UN        UN        UN       UN
#define STRACE_FTRUNCATE    RC0    FD         OFF        UN        UN        UN       UN
#define STRACE_MPROTECT     RC0    PTR        SIZE       PROT      UN        UN       UN
#define STRACE_MUNMAP       RC0    PTR        SIZE       UN        UN        UN       UN
#define STRACE_SIGACTION    RC0    SIG        I_HAND     O_HAND    SSIZE     UN       UN
#define STRACE_SIGPROCMASK  RC0    SIG        I_SIGSET   O_SIGSET  SSIZE     UN       UN
#define STRACE_CLOCK_SLEEP  RC0    CLOCK      I32        I_TIME    O_TIME    UN       UN
#define STRACE_NANOSLEEP    RC0    I_TIME     O_TIME     UN        UN        UN       UN
#define STRACE_IOCTL        I32    FD         WAT_IOCTL  UN        UN        UN       UN
#define STRACE_PREAD        SSIZE  FD         O_BUF      BUFSZ     OFF       UN       UN
#define STRACE_PWRITE       SSIZE  FD         I_BUF      BUFSZ     OFF       UN       UN
#define STRACE_READV        SSIZE  FD         O_IOVEC    BUFSZ     OFF       UN       UN
#define STRACE_WRITEV       SSIZE  FD         I_IOVEC    BUFSZ     OFF       UN       UN
#define STRACE_PIPE         SSIZE  O_PFDS     UN         UN        UN        UN       UN
#define STRACE_PIPE2        SSIZE  O_PFDS     OFLAGS     UN        UN        UN       UN
#define STRACE_SOCKETPAIR   RC0    FAMILY     SOCKTYPE   I32       O_PFDS    UN       UN
#define STRACE_SELECT       SSIZE  I32        FDSET      FDSET     FDSET     IO_TIMEV UN
#define STRACE_PSELECT      SSIZE  I32        FDSET      FDSET     FDSET     IO_TIME  WAT_PSELECT
#define STRACE_SCHED_YIELD  RC0    UN         UN         UN        UN        UN       UN
#define STRACE_FCNTL        I64    FD         WAT_FCNTL  UN        UN        UN       UN
#define STRACE_FORK         PID    UN         UN         UN        UN        UN       UN
#define STRACE_VFORK        PID    UN         UN         UN        UN        UN       UN
#define STRACE_WAIT4        PID    PID        O_WSTATUS  WAITFLAGS O_RUSAGE  UN       UN
#define STRACE_KILL         RC0    PID        SIG        UN        UN        UN       UN
#define STRACE_TKILL        RC0    PID        SIG        UN        UN        UN       UN
#define STRACE_ACCESS       RC0    PATH       MODE       UN        UN        UN       UN
#define STRACE_FACCESSAT    RC0    DIRFD      PATH       MODE      UN        UN       UN
#define STRACE_FACCESSAT2   RC0    DIRFD      PATH       MODE      ATFLAGS   UN       UN
#define STRACE_READLINK     SSIZE  PATH       O_BUF      BUFSZ     UN        UN       UN
#define STRACE_READLINKAT   SSIZE  DIRFD      STR        O_BUF     BUFSZ     UN       UN
#define STRACE_SYMLINK      RC0    PATH       PATH       UN        UN        UN       UN
#define STRACE_SYMLINKAT    RC0    PATH       DIRFD      PATH      UN        UN       UN
#define STRACE_UNLINK       RC0    PATH       UN         UN        UN        UN       UN
#define STRACE_UNLINKAT     RC0    DIRFD      PATH       ATFLAGS   UN        UN       UN
#define STRACE_LINK         RC0    PATH       PATH       UN        UN        UN       UN
#define STRACE_LINKAT       RC0    DIRFD      PATH       DIRFD     PATH      ATFLAGS  UN
#define STRACE_RENAME       RC0    PATH       PATH       UN        UN        UN       UN
#define STRACE_RENAMEAT     RC0    DIRFD      PATH       DIRFD     PATH      UN       UN
#define STRACE_RENAMEAT2    RC0    DIRFD      PATH       DIRFD     PATH      ATFLAGS  UN
#define STRACE_MKDIR        RC0    PATH       MODE       UN        UN        RENFLAGS UN
#define STRACE_MKDIRAT      RC0    DIRFD      PATH       MODE      UN        UN       UN
#define STRACE_RMDIR        RC0    PATH       UN         UN        UN        UN       UN
#define STRACE_CHMOD        RC0    PATH       MODE       UN        UN        UN       UN
#define STRACE_FCHMOD       RC0    FD         MODE       UN        UN        UN       UN
#define STRACE_FCHMODAT     RC0    DIRFD      PATH       MODE      ATFLAGS   UN       UN
#define STRACE_CHOWN        RC0    PATH       UID        GID       UN        UN       UN
#define STRACE_LCHOWN       RC0    PATH       UID        GID       UN        UN       UN
#define STRACE_FCHOWN       RC0    FD         UID        GID       UN        UN       UN
#define STRACE_CHOWNAT      RC0    DIRFD      PATH       UID       GID       ATFLAGS  UN
#define STRACE_GETTID       PID    UN         UN         UN        UN        UN       UN
#define STRACE_GETPID       PID    UN         UN         UN        UN        UN       UN
#define STRACE_GETPPID      PID    UN         UN         UN        UN        UN       UN
#define STRACE_GETUID       UID    UN         UN         UN        UN        UN       UN
#define STRACE_GETGID       GID    UN         UN         UN        UN        UN       UN
#define STRACE_GETEUID      UID    UN         UN         UN        UN        UN       UN
#define STRACE_GETEGID      GID    UN         UN         UN        UN        UN       UN
#define STRACE_SETSID       PID    UN         UN         UN        UN        UN       UN
#define STRACE_GETPGRP      PID    UN         UN         UN        UN        UN       UN
#define STRACE_SETUID       RC0    GID        UN         UN        UN        UN       UN
#define STRACE_SETGID       RC0    GID        UN         UN        UN        UN       UN
#define STRACE_UMASK        MODE   MODE       UN         UN        UN        UN       UN
#define STRACE_CHDIR        RC0    PATH       UN         UN        UN        UN       UN
#define STRACE_FCHDIR       RC0    FD         UN         UN        UN        UN       UN
#define STRACE_CLONE        PID    CLONEFLAGS PTR        PTR       PTR       PTR      PTR
#define STRACE_SOCKET       I32    FAMILY     SOCKTYPE   I32       UN        UN       UN
#define STRACE_CONNECT      I32    FD         I_ADDR     ADDRLEN   UN        UN       UN
#define STRACE_LISTEN       RC0    FD         UN         UN        UN        UN       UN
#define STRACE_ACCEPT       FD     FD         O_ADDR     I64       UN        UN       UN
#define STRACE_ACCEPT4      FD     FD         O_ADDR     I64       SOCKFLAGS UN       UN
#define STRACE_BIND         RC0    FD         I_ADDR     ADDRLEN   UN        UN       UN
#define STRACE_GETSOCKNAME  RC0    FD         O_ADDR     I64       UN        UN       UN
#define STRACE_GETPEERNAME  RC0    FD         O_ADDR     I64       UN        UN       UN
#define STRACE_SENDTO       SSIZE  FD         I_BUF      SIZE      I32       I_ADDR   ADDRLEN
#define STRACE_RECVFROM     SSIZE  FD         O_BUF      SIZE      I32       O_ADDR   I64
// clang-format on

void EnterStrace(struct Machine *, const char *, ...);
void LeaveStrace(struct Machine *, const char *, const char *, ...);

#endif /* BLINK_STRACE_H_ */
