#ifndef BLINK_STRACE_H_
#define BLINK_STRACE_H_
#include "blink/machine.h"

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
#define IS_BUF(c)    (0200 == (0277 & (c)))
#define IS_TIME(c)   (0202 == (0237 & (c)))
#define IS_HAND(c)   (0203 == (0277 & (c)))
#define IS_ADDR(c)   (0210 == (0277 & (c)))
#define IS_SIGSET(c) (0204 == (0277 & (c)))

#define ST_HAS(s, f) \
  (f(s[1]) || f(s[2]) || f(s[3]) || f(s[4]) || f(s[5]) || f(s[6]))

#define NORMAL "\000"  // (1) unlikely to block and (2) has no io_* args
#define TWOWAY "\001"  // (1) likely to block, or (2) has io args

#define UN
#define NUL            "\000"
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
#define I32_SIGHOW     "\023"
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
#define O_TIME2        "\311"
#define IO_POLL        "\340"
#define IO_TIME        "\342"
#define IO_TIMEV       "\343"
#define IO_FDSET       "\344"
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
#define SIGHOW     I32_SIGHOW
#define SIZE       I64_HEX
#define SSIZE      I64
#define OFF        I64_HEX
#define HEX        I64_HEX
#define PTR        I64_HEX
#define STR        I_STR
#define PATH       I_STR
#define BUFSZ      I64
#define O_RUSAGE   I64_HEX

// clang-format off
//      SYSCALL             KIND    RET    ARG1       ARG2       ARG3      ARG4      ARG5     ARG6
#define STRACE_0            NORMAL  HEX    NUL        UN         UN        UN        UN       UN
#define STRACE_1            NORMAL  HEX    HEX        NUL        UN        UN        UN       UN
#define STRACE_2            NORMAL  HEX    HEX        HEX        NUL       UN        UN       UN
#define STRACE_3            NORMAL  HEX    HEX        HEX        HEX       NUL       UN       UN
#define STRACE_4            NORMAL  HEX    HEX        HEX        HEX       HEX       NUL      UN
#define STRACE_5            NORMAL  HEX    HEX        HEX        HEX       HEX       HEX      UN
#define STRACE_6            NORMAL  HEX    HEX        HEX        HEX       HEX       HEX      HEX
#define STRACE_READ         TWOWAY  SSIZE  FD         O_BUF      BUFSZ     NUL       UN       UN
#define STRACE_WRITE        NORMAL  SSIZE  FD         I_BUF      BUFSZ     NUL       UN       UN
#define STRACE_OPEN         NORMAL  I32    PATH       OFLAGS     MODE      NUL       UN       UN
#define STRACE_OPENAT       NORMAL  I32    DIRFD      STR        OFLAGS    MODE      NUL      UN
#define STRACE_CREAT        NORMAL  I32    PATH       MODE       NUL       UN        UN       UN
#define STRACE_CLOSE        NORMAL  RC0    FD         NUL        UN        UN        UN       UN
#define STRACE_STAT         NORMAL  RC0    PATH       O_STAT     NUL       UN        UN       UN
#define STRACE_FSTAT        NORMAL  RC0    FD         O_STAT     NUL       UN        UN       UN
#define STRACE_LSTAT        NORMAL  RC0    PATH       O_STAT     NUL       UN        UN       UN
#define STRACE_FSTATAT      NORMAL  RC0    DIRFD      PATH       O_STAT    ATFLAGS   NUL      UN
#define STRACE_UTIMENSAT    NORMAL  RC0    DIRFD      PATH       O_TIME2   ATFLAGS   NUL      UN
#define STRACE_POLL         TWOWAY  I32    IO_POLL    I32        NUL       UN        UN       UN
#define STRACE_PPOLL        TWOWAY  RC0    IO_POLL    IO_TIME    I_SIGSET  SSIZE     NUL      UN
#define STRACE_LSEEK        NORMAL  OFF    FD         OFF        WHENCE    NUL       UN       UN
#define STRACE_MMAP         NORMAL  PTR    PTR        SIZE       PROT      MAPFLAGS  FD       OFF
#define STRACE_PAUSE        TWOWAY  RC0    NUL        UN         UN        UN        UN       UN
#define STRACE_SYNC         TWOWAY  RC0    NUL        UN         UN        UN        UN       UN
#define STRACE_FSYNC        TWOWAY  RC0    FD         NUL        UN        UN        UN       UN
#define STRACE_FDATASYNC    TWOWAY  RC0    FD         NUL        UN        UN        UN       UN
#define STRACE_DUP          NORMAL  I32    FD         NUL        UN        UN        UN       UN
#define STRACE_DUP2         NORMAL  I32    FD         I32        NUL       UN        UN       UN
#define STRACE_DUP3         NORMAL  I32    FD         I32        OFLAGS    NUL       UN       UN
#define STRACE_MSYNC        NORMAL  RC0    PTR        SIZE       MSFLAGS   NUL       UN       UN
#define STRACE_ALARM        NORMAL  I32    I32        NUL        UN        UN        UN       UN
#define STRACE_GETCWD       NORMAL  SSIZE  O_BUF      BUFSZ      NUL       UN        UN       UN
#define STRACE_TRUNCATE     TWOWAY  RC0    STR        OFF        NUL       UN        UN       UN
#define STRACE_FTRUNCATE    NORMAL  RC0    FD         OFF        NUL       UN        UN       UN
#define STRACE_MPROTECT     NORMAL  RC0    PTR        SIZE       PROT      NUL       UN       UN
#define STRACE_MUNMAP       NORMAL  RC0    PTR        SIZE       NUL       UN        UN       UN
#define STRACE_SIGACTION    NORMAL  RC0    SIG        I_HAND     O_HAND    SSIZE     NUL      UN
#define STRACE_SIGPROCMASK  NORMAL  RC0    SIGHOW     I_SIGSET   O_SIGSET  SSIZE     NUL      UN
#define STRACE_CLOCK_SLEEP  TWOWAY  RC0    CLOCK      I32        I_TIME    O_TIME    NUL      UN
#define STRACE_SIGSUSPEND   TWOWAY  RC0    I_SIGSET   SSIZE      NUL       UN        UN       UN
#define STRACE_NANOSLEEP    TWOWAY  RC0    I_TIME     O_TIME     NUL       UN        UN       UN
#define STRACE_IOCTL        NORMAL  I32    FD         WAT_IOCTL  NUL       UN        UN       UN
#define STRACE_PREAD        TWOWAY  SSIZE  FD         O_BUF      BUFSZ     OFF       NUL      UN
#define STRACE_PWRITE       NORMAL  SSIZE  FD         I_BUF      BUFSZ     OFF       NUL      UN
#define STRACE_READV        TWOWAY  SSIZE  FD         O_IOVEC    BUFSZ     OFF       NUL      UN
#define STRACE_WRITEV       NORMAL  SSIZE  FD         I_IOVEC    BUFSZ     OFF       NUL      UN
#define STRACE_PIPE         NORMAL  SSIZE  O_PFDS     NUL        UN        UN        UN       UN
#define STRACE_PIPE2        NORMAL  SSIZE  O_PFDS     OFLAGS     NUL       UN        UN       UN
#define STRACE_SOCKETPAIR   NORMAL  RC0    FAMILY     SOCKTYPE   I32       O_PFDS    NUL      UN
#define STRACE_SELECT       TWOWAY  SSIZE  I32        IO_FDSET   IO_FDSET  IO_FDSET  IO_TIMEV UN
#define STRACE_PSELECT      TWOWAY  SSIZE  I32        IO_FDSET   IO_FDSET  IO_FDSET  IO_TIME  WAT_PSELECT
#define STRACE_SCHED_YIELD  NORMAL  RC0    NUL        UN         UN        UN        UN       UN
#define STRACE_FCNTL        NORMAL  I64    FD         WAT_FCNTL  NUL       UN        UN       UN
#define STRACE_FORK         NORMAL  PID    NUL        UN         UN        UN        UN       UN
#define STRACE_VFORK        NORMAL  PID    NUL        UN         UN        UN        UN       UN
#define STRACE_WAIT4        TWOWAY  PID    PID        O_WSTATUS  WAITFLAGS O_RUSAGE  NUL      UN
#define STRACE_KILL         NORMAL  RC0    PID        SIG        NUL       UN        UN       UN
#define STRACE_TKILL        NORMAL  RC0    PID        SIG        NUL       UN        UN       UN
#define STRACE_ACCESS       NORMAL  RC0    PATH       MODE       NUL       UN        UN       UN
#define STRACE_FACCESSAT    NORMAL  RC0    DIRFD      PATH       MODE      NUL       UN       UN
#define STRACE_FACCESSAT2   NORMAL  RC0    DIRFD      PATH       MODE      ATFLAGS   NUL      UN
#define STRACE_READLINK     NORMAL  SSIZE  PATH       O_BUF      BUFSZ     NUL       UN       UN
#define STRACE_READLINKAT   NORMAL  SSIZE  DIRFD      STR        O_BUF     BUFSZ     NUL      UN
#define STRACE_SYMLINK      NORMAL  RC0    PATH       PATH       NUL       UN        UN       UN
#define STRACE_SYMLINKAT    NORMAL  RC0    PATH       DIRFD      PATH      NUL       UN       UN
#define STRACE_UNLINK       NORMAL  RC0    PATH       NUL        UN        UN        UN       UN
#define STRACE_UNLINKAT     NORMAL  RC0    DIRFD      PATH       ATFLAGS   NUL       UN       UN
#define STRACE_LINK         NORMAL  RC0    PATH       PATH       NUL       UN        UN       UN
#define STRACE_LINKAT       NORMAL  RC0    DIRFD      PATH       DIRFD     PATH      ATFLAGS  UN
#define STRACE_RENAME       NORMAL  RC0    PATH       PATH       NUL       UN        UN       UN
#define STRACE_RENAMEAT     NORMAL  RC0    DIRFD      PATH       DIRFD     PATH      NUL      UN
#define STRACE_RENAMEAT2    NORMAL  RC0    DIRFD      PATH       DIRFD     PATH      ATFLAGS  UN
#define STRACE_MKDIR        NORMAL  RC0    PATH       MODE       NUL       UN        RENFLAGS UN
#define STRACE_MKDIRAT      NORMAL  RC0    DIRFD      PATH       MODE      NUL       UN       UN
#define STRACE_RMDIR        NORMAL  RC0    PATH       NUL        UN        UN        UN       UN
#define STRACE_CHMOD        NORMAL  RC0    PATH       MODE       NUL       UN        UN       UN
#define STRACE_FCHMOD       NORMAL  RC0    FD         MODE       NUL       UN        UN       UN
#define STRACE_FCHMODAT     NORMAL  RC0    DIRFD      PATH       MODE      ATFLAGS   NUL      UN
#define STRACE_CHOWN        NORMAL  RC0    PATH       UID        GID       NUL       UN       UN
#define STRACE_LCHOWN       NORMAL  RC0    PATH       UID        GID       NUL       UN       UN
#define STRACE_FCHOWN       NORMAL  RC0    FD         UID        GID       NUL       UN       UN
#define STRACE_CHOWNAT      NORMAL  RC0    DIRFD      PATH       UID       GID       ATFLAGS  UN
#define STRACE_GETTID       NORMAL  PID    NUL        UN         UN        UN        UN       UN
#define STRACE_GETPID       NORMAL  PID    NUL        UN         UN        UN        UN       UN
#define STRACE_GETPPID      NORMAL  PID    NUL        UN         UN        UN        UN       UN
#define STRACE_GETPGID      NORMAL  PID    PID        NUL        UN        UN        UN       UN
#define STRACE_GETUID       NORMAL  UID    NUL        UN         UN        UN        UN       UN
#define STRACE_GETGID       NORMAL  GID    NUL        UN         UN        UN        UN       UN
#define STRACE_GETEUID      NORMAL  UID    NUL        UN         UN        UN        UN       UN
#define STRACE_GETEGID      NORMAL  GID    NUL        UN         UN        UN        UN       UN
#define STRACE_SETSID       NORMAL  PID    NUL        UN         UN        UN        UN       UN
#define STRACE_GETPGRP      NORMAL  PID    NUL        UN         UN        UN        UN       UN
#define STRACE_SETUID       NORMAL  RC0    UID        NUL        UN        UN        UN       UN
#define STRACE_SETGID       NORMAL  RC0    GID        NUL        UN        UN        UN       UN
#define STRACE_SETREUID     NORMAL  RC0    UID        UID        NUL       UN        UN       UN
#define STRACE_SETREGID     NORMAL  RC0    GID        GID        NUL       UN        UN       UN
#define STRACE_SETRESUID    NORMAL  RC0    UID        UID        UID       NUL       UN       UN
#define STRACE_SETRESGID    NORMAL  RC0    GID        GID        GID       NUL       UN       UN
#define STRACE_UMASK        NORMAL  MODE   MODE       NUL        UN        UN        UN       UN
#define STRACE_CHDIR        NORMAL  RC0    PATH       NUL        UN        UN        UN       UN
#define STRACE_CHROOT       NORMAL  RC0    PATH       NUL        UN        UN        UN       UN
#define STRACE_FCHDIR       NORMAL  RC0    FD         NUL        UN        UN        UN       UN
#define STRACE_CLONE        NORMAL  PID    CLONEFLAGS PTR        PTR       PTR       PTR      PTR
#define STRACE_SOCKET       NORMAL  I32    FAMILY     SOCKTYPE   I32       NUL       UN       UN
#define STRACE_CONNECT      NORMAL  I32    FD         I_ADDR     ADDRLEN   NUL       UN       UN
#define STRACE_LISTEN       NORMAL  RC0    FD         NUL        UN        UN        UN       UN
#define STRACE_ACCEPT       TWOWAY  FD     FD         O_ADDR     I64       NUL       UN       UN
#define STRACE_ACCEPT4      TWOWAY  FD     FD         O_ADDR     I64       SOCKFLAGS NUL      UN
#define STRACE_BIND         NORMAL  RC0    FD         I_ADDR     ADDRLEN   NUL       UN       UN
#define STRACE_GETSOCKNAME  NORMAL  RC0    FD         O_ADDR     I64       NUL       UN       UN
#define STRACE_GETPEERNAME  NORMAL  RC0    FD         O_ADDR     I64       NUL       UN       UN
#define STRACE_SENDTO       NORMAL  SSIZE  FD         I_BUF      SIZE      I32       I_ADDR   ADDRLEN
#define STRACE_RECVFROM     TWOWAY  SSIZE  FD         O_BUF      SIZE      I32       O_ADDR   I64
// clang-format on

void EnterStrace(struct Machine *, const char *, const char *, ...);
void LeaveStrace(struct Machine *, const char *, const char *, ...);

#endif /* BLINK_STRACE_H_ */
