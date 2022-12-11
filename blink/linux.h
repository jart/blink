#ifndef BLINK_LINUX_H_
#define BLINK_LINUX_H_
#include "blink/types.h"

#define AT_FDCWD_LINUX -100

#define O_RDONLY_LINUX  0
#define O_WRONLY_LINUX  1
#define O_RDWR_LINUX    2
#define O_ACCMODE_LINUX 3

#define O_APPEND_LINUX    0x000400
#define O_CREAT_LINUX     0x000040
#define O_EXCL_LINUX      0x000080
#define O_TRUNC_LINUX     0x000200
#define O_NDELAY_LINUX    0x000800
#define O_DIRECT_LINUX    0x004000
#define O_DIRECTORY_LINUX 0x010000
#define O_NOFOLLOW_LINUX  0x020000
#define O_CLOEXEC_LINUX   0x080000
#define O_NOCTTY_LINUX    0x000100
#define O_ASYNC_LINUX     0x002000
#define O_NOATIME_LINUX   0x040000
#define O_DSYNC_LINUX     0x001000
#define O_PATH_LINUX      0x200000
#define O_LARGEFILE_LINUX 0x008000

#define F_DUPFD_LINUX         0
#define F_DUPFD_CLOEXEC_LINUX 0x0406
#define F_GETFD_LINUX         1
#define F_SETFD_LINUX         2
#define FD_CLOEXEC_LINUX      1
#define F_GETFL_LINUX         3
#define F_SETFL_LINUX         4

#define SOCK_CLOEXEC_LINUX  O_CLOEXEC_LINUX
#define SOCK_NONBLOCK_LINUX O_NDELAY_LINUX

#define TIOCGWINSZ_LINUX    0x5413
#define TCGETS_LINUX        0x5401
#define TCSETS_LINUX        0x5402
#define TCSETSW_LINUX       0x5403
#define TCSETSF_LINUX       0x5404
#define ARCH_SET_GS_LINUX   0x1001
#define ARCH_SET_FS_LINUX   0x1002
#define ARCH_GET_FS_LINUX   0x1003
#define ARCH_GET_GS_LINUX   0x1004
#define O_CLOEXEC_LINUX     0x080000
#define POLLIN_LINUX        0x01
#define POLLPRI_LINUX       0x02
#define POLLOUT_LINUX       0x04
#define POLLERR_LINUX       0x08
#define POLLHUP_LINUX       0x10
#define POLLNVAL_LINUX      0x20
#define TIMER_ABSTIME_LINUX 0x01

#define MAP_SHARED_LINUX          0x00000001
#define MAP_PRIVATE_LINUX         0x00000002
#define MAP_FIXED_LINUX           0x00000010
#define MAP_FIXED_NOREPLACE_LINUX 0x08000000
#define MAP_ANONYMOUS_LINUX       0x00000020
#define MAP_GROWSDOWN_LINUX       0x00000100
#define MAP_STACK_LINUX           0x00020000
#define MAP_NORESERVE_LINUX       0x00004000
#define MAP_POPULATE_LINUX        0x00008000

#define CLONE_VM_LINUX             0x00000100
#define CLONE_THREAD_LINUX         0x00010000
#define CLONE_FS_LINUX             0x00000200
#define CLONE_FILES_LINUX          0x00000400
#define CLONE_SIGHAND_LINUX        0x00000800
#define CLONE_VFORK_LINUX          0x00004000
#define CLONE_SYSVSEM_LINUX        0x00040000
#define CLONE_SETTLS_LINUX         0x00080000
#define CLONE_PARENT_SETTID_LINUX  0x00100000
#define CLONE_CHILD_CLEARTID_LINUX 0x00200000
#define CLONE_CHILD_SETTID_LINUX   0x01000000

#define FUTEX_WAIT_LINUX         0
#define FUTEX_WAKE_LINUX         1
#define FUTEX_PRIVATE_FLAG_LINUX 128

#define DT_UNKNOWN_LINUX 0
#define DT_FIFO_LINUX    1
#define DT_CHR_LINUX     2
#define DT_DIR_LINUX     4
#define DT_BLK_LINUX     6
#define DT_REG_LINUX     8
#define DT_LNK_LINUX     10
#define DT_SOCK_LINUX    12

#define SEEK_SET_LINUX 0
#define SEEK_CUR_LINUX 1
#define SEEK_END_LINUX 2

#define F_OK_LINUX 0
#define X_OK_LINUX 1
#define W_OK_LINUX 2
#define R_OK_LINUX 4

#define SHUT_RD_LINUX   0
#define SHUT_WR_LINUX   1
#define SHUT_RDWR_LINUX 2

#define SIG_IGN_LINUX 1

#define SIG_BLOCK_LINUX   0
#define SIG_UNBLOCK_LINUX 1
#define SIG_SETMASK_LINUX 2

#define SIGHUP_LINUX    1
#define SIGINT_LINUX    2
#define SIGQUIT_LINUX   3
#define SIGILL_LINUX    4
#define SIGTRAP_LINUX   5
#define SIGABRT_LINUX   6
#define SIGBUS_LINUX    7
#define SIGFPE_LINUX    8
#define SIGKILL_LINUX   9
#define SIGUSR1_LINUX   10
#define SIGSEGV_LINUX   11
#define SIGUSR2_LINUX   12
#define SIGPIPE_LINUX   13
#define SIGALRM_LINUX   14
#define SIGTERM_LINUX   15
#define SIGSTKFLT_LINUX 16
#define SIGCHLD_LINUX   17
#define SIGCONT_LINUX   18
#define SIGSTOP_LINUX   19
#define SIGTSTP_LINUX   20
#define SIGTTIN_LINUX   21
#define SIGTTOU_LINUX   22
#define SIGURG_LINUX    23
#define SIGXCPU_LINUX   24
#define SIGXFSZ_LINUX   25
#define SIGVTALRM_LINUX 26
#define SIGPROF_LINUX   27
#define SIGWINCH_LINUX  28
#define SIGIO_LINUX     29
#define SIGSYS_LINUX    31
#define SIGINFO_LINUX   63
#define SIGEMT_LINUX    64
#define SIGPWR_LINUX    30
#define SIGTHR_LINUX    32
#define SIGRTMIN_LINUX  32
#define SIGRTMAX_LINUX  64

#define AT_RANDOM_LINUX 25
#define AT_EXECFN_LINUX 31

#define IFNAMSIZ_LINUX 16

#define SIOCGIFCONF_LINUX 0x8912

#define AF_UNSPEC_LINUX 0
#define AF_UNIX_LINUX   1
#define AF_INET_LINUX   2

#define SOL_SOCKET_LINUX 1
#define SOL_TCP_LINUX    6
#define SOL_UDP_LINUX    17

struct iovec_linux {
  u8 iov_base[8];
  u8 iov_len[8];
};

struct pollfd_linux {
  u8 fd[4];
  u8 events[2];
  u8 revents[2];
};

struct timeval_linux {
  u8 tv_sec[8];
  u8 tv_usec[8];
};

struct timespec_linux {
  u8 tv_sec[8];
  u8 tv_nsec[8];
};

struct timezone_linux {
  u8 tz_minuteswest[4];
  u8 tz_dsttime[4];
};

struct sigaction_linux {
  u8 handler[8];
  u8 flags[8];
  u8 restorer[8];
  u8 mask[8];
};

struct winsize_linux {
  u8 ws_row[2];
  u8 ws_col[2];
  u8 ws_xpixel[2];
  u8 ws_ypixel[2];
};

struct termios_linux {
  u8 c_iflag[4];
  u8 c_oflag[4];
  u8 c_cflag[4];
  u8 c_lflag[4];
  u8 c_cc[32];
  u8 c_ispeed[4];
  u8 c_ospeed[4];
};

struct sockaddr_in_linux {
  u8 sin_family[2];
  u16 sin_port;
  u32 sin_addr;
  u8 sin_zero[8];
};

struct stat_linux {
  u8 st_dev[8];
  u8 st_ino[8];
  u8 st_nlink[8];
  u8 st_mode[4];
  u8 st_uid[4];
  u8 st_gid[4];
  u8 __pad[4];
  u8 st_rdev[8];
  u8 st_size[8];
  u8 st_blksize[8];
  u8 st_blocks[8];
  struct timespec_linux st_atim;
  struct timespec_linux st_mtim;
  struct timespec_linux st_ctim;
};

struct itimerval_linux {
  struct timeval_linux it_interval;
  struct timeval_linux it_value;
};

struct rusage_linux {
  struct timeval_linux ru_utime;
  struct timeval_linux ru_stime;
  u8 ru_maxrss[8];
  u8 ru_ixrss[8];
  u8 ru_idrss[8];
  u8 ru_isrss[8];
  u8 ru_minflt[8];
  u8 ru_majflt[8];
  u8 ru_nswap[8];
  u8 ru_inblock[8];
  u8 ru_oublock[8];
  u8 ru_msgsnd[8];
  u8 ru_msgrcv[8];
  u8 ru_nsignals[8];
  u8 ru_nvcsw[8];
  u8 ru_nivcsw[8];
};

struct siginfo_linux {
  u8 si_signo[4];
  u8 si_errno[4];
  u8 si_code[4];
  u8 __pad[4];
  u8 payload[112];
};

struct fpstate_linux {
  u8 cwd[2];
  u8 swd[2];
  u8 ftw[2];
  u8 fop[2];
  u8 rip[8];
  u8 rdp[8];
  u8 mxcsr[4];
  u8 mxcr_mask[4];
  u8 st[8][16];
  u8 xmm[16][16];
  u8 __padding[96];
};

struct ucontext_linux {
  u8 uc_flags[8];
  u8 uc_link[8];
  u8 ss_sp[8];
  u8 ss_flags[4];
  u8 __pad0[4];
  u8 ss_size[8];
  u8 r8[8];
  u8 r9[8];
  u8 r10[8];
  u8 r11[8];
  u8 r12[8];
  u8 r13[8];
  u8 r14[8];
  u8 r15[8];
  u8 rdi[8];
  u8 rsi[8];
  u8 rbp[8];
  u8 rbx[8];
  u8 rdx[8];
  u8 rax[8];
  u8 rcx[8];
  u8 rsp[8];
  u8 rip[8];
  u8 eflags[8];
  u8 cs[2];
  u8 gs[2];
  u8 fs[2];
  u8 ss[2];
  u8 err[8];
  u8 trapno[8];
  u8 oldmask[8];
  u8 cr2[8];
  u8 fpstate[8];
  u8 __pad1[64];
  u8 uc_sigmask[8];
};

struct utsname_linux {
  char sysname[65];
  char nodename[65];
  char release[65];
  char version[65];
  char machine[65];
  char domainname[65];
};

struct rlimit_linux {
  u8 rlim_cur[8];
  u8 rlim_max[8];
};

struct dirent_linux {
  u8 d_ino[8];       // inode number
  u8 d_off[8];       // implementation-dependent location number
  u8 d_reclen[2];    // byte length of this whole struct and string
  u8 d_type[1];      // DT_REG, DT_DIR, DT_UNKNOWN, DT_BLK, etc.
  char d_name[256];  // NUL-terminated basename
};

struct ifconf_linux {
  u8 len[8];
  u8 buf[8];
};

struct ifreq_linux {
  u8 name[IFNAMSIZ_LINUX];
  union {
    struct sockaddr_in_linux addr;
    struct sockaddr_in_linux dstaddr;
    struct sockaddr_in_linux netmask;
    struct sockaddr_in_linux broadaddr;
    u8 flags[2];
    u8 pad[24];
  };
};

#endif /* BLINK_LINUX_H_ */
