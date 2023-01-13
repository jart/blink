#ifndef BLINK_LINUX_H_
#define BLINK_LINUX_H_
#include "blink/types.h"

#define ENOSYS_LINUX          38
#define EPERM_LINUX           1
#define ENOENT_LINUX          2
#define ESRCH_LINUX           3
#define EINTR_LINUX           4
#define EIO_LINUX             5
#define ENXIO_LINUX           6
#define E2BIG_LINUX           7
#define ENOEXEC_LINUX         8
#define EBADF_LINUX           9
#define ECHILD_LINUX          10
#define EAGAIN_LINUX          11
#define ENOMEM_LINUX          12
#define EACCES_LINUX          13
#define EFAULT_LINUX          14
#define ENOTBLK_LINUX         15
#define EBUSY_LINUX           16
#define EEXIST_LINUX          17
#define EXDEV_LINUX           18
#define ENODEV_LINUX          19
#define ENOTDIR_LINUX         20
#define EISDIR_LINUX          21
#define EINVAL_LINUX          22
#define ENFILE_LINUX          23
#define EMFILE_LINUX          24
#define ENOTTY_LINUX          25
#define ETXTBSY_LINUX         26
#define EFBIG_LINUX           27
#define ENOSPC_LINUX          28
#define EDQUOT_LINUX          122
#define ESPIPE_LINUX          29
#define EROFS_LINUX           30
#define EMLINK_LINUX          31
#define EPIPE_LINUX           32
#define EDOM_LINUX            33
#define ERANGE_LINUX          34
#define EDEADLK_LINUX         35
#define ENAMETOOLONG_LINUX    36
#define ENOLCK_LINUX          37
#define ENOTEMPTY_LINUX       39
#define ELOOP_LINUX           40
#define ENOMSG_LINUX          42
#define EIDRM_LINUX           43
#define EPROTO_LINUX          71
#define EOVERFLOW_LINUX       75
#define EILSEQ_LINUX          84
#define EUSERS_LINUX          87
#define ENOTSOCK_LINUX        88
#define EDESTADDRREQ_LINUX    89
#define EMSGSIZE_LINUX        90
#define EPROTOTYPE_LINUX      91
#define ENOPROTOOPT_LINUX     92
#define EPROTONOSUPPORT_LINUX 93
#define ESOCKTNOSUPPORT_LINUX 94
#define ENOTSUP_LINUX         95
#define EOPNOTSUPP_LINUX      95
#define EPFNOSUPPORT_LINUX    96
#define EAFNOSUPPORT_LINUX    97
#define EADDRINUSE_LINUX      98
#define EADDRNOTAVAIL_LINUX   99
#define ENETDOWN_LINUX        100
#define ENETUNREACH_LINUX     101
#define ENETRESET_LINUX       102
#define ECONNABORTED_LINUX    103
#define ECONNRESET_LINUX      104
#define ENOBUFS_LINUX         105
#define EISCONN_LINUX         106
#define ENOTCONN_LINUX        107
#define ESHUTDOWN_LINUX       108
#define ETOOMANYREFS_LINUX    109
#define ETIMEDOUT_LINUX       110
#define ETIME_LINUX           62
#define ECONNREFUSED_LINUX    111
#define EHOSTDOWN_LINUX       112
#define EHOSTUNREACH_LINUX    113
#define EALREADY_LINUX        114
#define EINPROGRESS_LINUX     115
#define ESTALE_LINUX          116
#define EREMOTE_LINUX         66
#define EBADMSG_LINUX         74
#define ECANCELED_LINUX       125
#define EOWNERDEAD_LINUX      130
#define ENOTRECOVERABLE_LINUX 131
#define ENONET_LINUX          64
#define ERESTART_LINUX        85
#define ENODATA_LINUX         61
#define ENOSR_LINUX           63
#define ENOSTR_LINUX          60
#define EMULTIHOP_LINUX       72
#define ENOLINK_LINUX         67
#define ENOMEDIUM_LINUX       123
#define EMEDIUMTYPE_LINUX     124
#define EBADFD_LINUX          77

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
#define CLONE_DETACHED_LINUX       0x00400000
#define CLONE_CHILD_SETTID_LINUX   0x01000000

#define FUTEX_WAIT_LINUX           0
#define FUTEX_WAKE_LINUX           1
#define FUTEX_WAIT_BITSET_LINUX    9
#define FUTEX_PRIVATE_FLAG_LINUX   128
#define FUTEX_CLOCK_REALTIME_LINUX 256

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

#define SIG_DFL_LINUX 0
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

#define AT_PAGESZ_LINUX 6
#define AT_UID_LINUX    11
#define AT_EUID_LINUX   12
#define AT_GID_LINUX    13
#define AT_EGID_LINUX   14
#define AT_CLKTCK_LINUX 17
#define AT_SECURE_LINUX 23
#define AT_RANDOM_LINUX 25
#define AT_EXECFN_LINUX 31

#define IFNAMSIZ_LINUX 16

#define SIOCGIFCONF_LINUX    0x8912
#define SIOCGIFADDR_LINUX    0x8915
#define SIOCGIFNETMASK_LINUX 0x891b
#define SIOCGIFBRDADDR_LINUX 0x8919
#define SIOCGIFDSTADDR_LINUX 0x8917

#define AF_UNSPEC_LINUX 0
#define AF_UNIX_LINUX   1
#define AF_INET_LINUX   2

#define SOL_SOCKET_LINUX 1
#define SOL_TCP_LINUX    6
#define SOL_UDP_LINUX    17

#define F_SETLK_LINUX  6
#define F_SETLKW_LINUX 7
#define F_GETLK_LINUX  5

#define F_RDLCK_LINUX 0
#define F_WRLCK_LINUX 1
#define F_UNLCK_LINUX 2

#define SA_NOCLDSTOP_LINUX 1
#define SA_NOCLDWAIT_LINUX 2
#define SA_SIGINFO_LINUX   4
#define SA_RESTORER_LINUX  0x04000000
#define SA_ONSTACK_LINUX   0x08000000
#define SA_RESTART_LINUX   0x10000000
#define SA_NODEFER_LINUX   0x40000000
#define SA_RESETHAND_LINUX 0x80000000

#define SCHED_OTHER_LINUX    0
#define SCHED_FIFO_LINUX     1
#define SCHED_RR_LINUX       2
#define SCHED_BATCH_LINUX    3
#define SCHED_IDLE_LINUX     5
#define SCHED_DEADLINE_LINUX 6

#define MSG_OOB_LINUX          0x00000001  // send, recv [portable]
#define MSG_PEEK_LINUX         0x00000002  // recv       [portable]
#define MSG_DONTROUTE_LINUX    0x00000004  // send       [portable]
#define MSG_TRUNC_LINUX        0x00000020  // recv       [portable]
#define MSG_DONTWAIT_LINUX     0x00000040  // send, recv [portable]
#define MSG_EOR_LINUX          0x00000080  // send       [portable]
#define MSG_WAITALL_LINUX      0x00000100  // recv       [portable]
#define MSG_NOSIGNAL_LINUX     0x00004000  // send       [portable]
#define MSG_CMSG_CLOEXEC_LINUX 0x40000000  // recv
#define MSG_CONFIRM_LINUX      0x00000800  // send
#define MSG_ERRQUEUE_LINUX     0x00002000  // recv
#define MSG_MORE_LINUX         0x00008000  // send

#define MSG_FASTOPEN_LINUX     0x20000000
#define MSG_CTRUNC_LINUX       8
#define MSG_NOERROR_LINUX      0x1000
#define MSG_WAITFORONE_LINUX   0x010000
#define MSG_BATCH_LINUX        0x040000
#define MSG_EXCEPT_LINUX       0x2000
#define MSG_FIN_LINUX          0x0200
#define MSG_EOF_LINUX          0x0200
#define MSG_INFO_LINUX         12
#define MSG_PARITY_ERROR_LINUX 9
#define MSG_PROXY_LINUX        0x10
#define MSG_RST_LINUX          0x1000
#define MSG_STAT_LINUX         11
#define MSG_SYN_LINUX          0x0400
#define MSG_NOTIFICATION_LINUX 0x8000

#define GRND_NONBLOCK_LINUX 1
#define GRND_RANDOM_LINUX   2

#define VMIN_LINUX     7
#define VTIME_LINUX    6
#define NCCS_LINUX     20
#define VINTR_LINUX    1
#define VQUIT_LINUX    2
#define VERASE_LINUX   3
#define VKILL_LINUX    4
#define VEOF_LINUX     5
#define VSWTC_LINUX    8
#define VSTART_LINUX   9
#define VSTOP_LINUX    10
#define VSUSP_LINUX    11
#define VEOL_LINUX     12
#define VREPRINT_LINUX 13
#define VDISCARD_LINUX 14
#define VWERASE_LINUX  15
#define VLNEXT_LINUX   16
#define VEOL2_LINUX    17

#define RLIM_INFINITY_LINUX 0xffffffffffffffffull

#define MINSIGSTKSZ_LINUX   2048
#define SS_ONSTACK_LINUX    1
#define SS_DISABLE_LINUX    2
#define SS_AUTODISARM_LINUX 0x80000000u

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

struct sockaddr_un_linux {
  u8 sun_family[2];
  char sun_path[108];
};

struct sockaddr_in_linux {
  u8 sin_family[2];
  u16 sin_port;
  u32 sin_addr;
  u8 sin_zero[8];
};

struct sockaddr_in6_linux {
  u8 sin6_family[2];
  u8 sin6_port[2];
  u8 sin6_flowinfo[4];
  u8 sin6_addr[16];
  u8 sin6_scope_id[4];
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

struct flock_linux {
  u8 l_type[2];
  u8 l_whence[2];
  u8 pad1_[4];
  u8 l_start[8];
  u8 l_len[8];
  u8 l_pid[4];
  u8 pad2_[4];
};

struct sysinfo_linux {
  u8 uptime[8];     // seconds since boot
  u8 loads[3][8];   // 1-5-15 min active process averages
  u8 totalram[8];   // system physical memory
  u8 freeram[8];    // amount of ram currently going to waste
  u8 sharedram[8];  // bytes w/ pages mapped into multiple progs
  u8 bufferram[8];  // lingering disk pages; see fadvise
  u8 totalswap[8];  // size of emergency memory
  u8 freeswap[8];   // hopefully equal to totalswap
  u8 procs[2];      // number of processes
  u8 __ignore[6];   // padding
  u8 totalhigh[8];  // wut
  u8 freehigh[8];   // wut
  u8 mem_unit[4];   // ram stuff above is multiples of this
};

struct tms_linux {
  u8 tms_utime[8];   // user time
  u8 tms_stime[8];   // system time
  u8 tms_cutime[8];  // user time of children
  u8 tms_cstime[8];  // system time of children
};

struct sigaltstack_linux {
  u8 ss_sp[8];     // base address of stack
  u8 ss_flags[4];  // SS_???_LINUX flags
  u8 pad1_[4];     //
  u8 ss_size[8];   // size of stack
};

int sysinfo_linux(struct sysinfo_linux *);

#endif /* BLINK_LINUX_H_ */
