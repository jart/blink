#ifndef BLINK_LINUX_H_
#define BLINK_LINUX_H_
#include "blink/types.h"

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

#endif /* BLINK_LINUX_H_ */
