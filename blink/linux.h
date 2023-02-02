#ifndef BLINK_LINUX_H_
#define BLINK_LINUX_H_
#include "blink/types.h"

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
#define ESPIPE_LINUX          29
#define EROFS_LINUX           30
#define EMLINK_LINUX          31
#define EPIPE_LINUX           32
#define EDOM_LINUX            33
#define ERANGE_LINUX          34
#define EDEADLK_LINUX         35
#define ENAMETOOLONG_LINUX    36
#define ENOLCK_LINUX          37
#define ENOSYS_LINUX          38
#define ENOTEMPTY_LINUX       39
#define ELOOP_LINUX           40
#define ENOMSG_LINUX          42
#define EIDRM_LINUX           43
#define ENOSTR_LINUX          60
#define ENODATA_LINUX         61
#define ETIME_LINUX           62
#define ENOSR_LINUX           63
#define ENONET_LINUX          64
#define EREMOTE_LINUX         66
#define ENOLINK_LINUX         67
#define EPROTO_LINUX          71
#define EMULTIHOP_LINUX       72
#define EBADMSG_LINUX         74
#define EOVERFLOW_LINUX       75
#define EBADFD_LINUX          77
#define EILSEQ_LINUX          84
#define ERESTART_LINUX        85
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
#define ECONNREFUSED_LINUX    111
#define EHOSTDOWN_LINUX       112
#define EHOSTUNREACH_LINUX    113
#define EALREADY_LINUX        114
#define EINPROGRESS_LINUX     115
#define ESTALE_LINUX          116
#define EDQUOT_LINUX          122
#define ENOMEDIUM_LINUX       123
#define EMEDIUMTYPE_LINUX     124
#define ECANCELED_LINUX       125
#define EOWNERDEAD_LINUX      130
#define ENOTRECOVERABLE_LINUX 131
#define ERFKILL_LINUX         132
#define EHWPOISON_LINUX       133

#define AT_FDCWD_LINUX            -100
#define AT_SYMLINK_NOFOLLOW_LINUX 0x0100
#define AT_REMOVEDIR_LINUX        0x0200
#define AT_EACCESS_LINUX          0x0200
#define AT_SYMLINK_FOLLOW_LINUX   0x0400
#define AT_EMPTY_PATH_LINUX       0x1000

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
#define __O_TMPFILE_LINUX 0x400000
#define O_TMPFILE_LINUX   (__O_TMPFILE_LINUX | O_DIRECTORY_LINUX)
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

#define F_GETFD_LINUX    1
#define F_SETFD_LINUX    2
#define FD_CLOEXEC_LINUX 1

#define F_GETFL_LINUX 3
#define F_SETFL_LINUX 4

#define F_GETLK_LINUX  5
#define F_SETLK_LINUX  6
#define F_SETLKW_LINUX 7
#define F_RDLCK_LINUX  0
#define F_WRLCK_LINUX  1
#define F_UNLCK_LINUX  2

#define F_SETSIG_LINUX 10
#define F_GETSIG_LINUX 11

#define F_SETOWN_LINUX        8
#define F_GETOWN_LINUX        9
#define F_SETOWN_EX_LINUX     15
#define F_GETOWN_EX_LINUX     16
#define F_GETOWNER_UIDS_LINUX 17
#define F_OWNER_TID_LINUX     0
#define F_OWNER_PID_LINUX     1
#define F_OWNER_PGRP_LINUX    2

#define SOCK_CLOEXEC_LINUX  O_CLOEXEC_LINUX
#define SOCK_NONBLOCK_LINUX O_NDELAY_LINUX

#define TIOCGWINSZ_LINUX    0x5413
#define TIOCSWINSZ_LINUX    0x5414
#define TCGETS_LINUX        0x5401
#define TCSETS_LINUX        0x5402
#define TCSETSW_LINUX       0x5403
#define TCSETSF_LINUX       0x5404
#define TIOCGPGRP_LINUX     0x540f
#define TIOCSPGRP_LINUX     0x5410
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

#define MAP_TYPE_LINUX            0x0000000f
#define MAP_FILE_LINUX            0x00000000
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

#define AT_NULL_LINUX          0
#define AT_IGNORE_LINUX        1
#define AT_EXECFD_LINUX        2
#define AT_PHDR_LINUX          3
#define AT_PHENT_LINUX         4
#define AT_PHNUM_LINUX         5
#define AT_PAGESZ_LINUX        6
#define AT_BASE_LINUX          7
#define AT_FLAGS_LINUX         8
#define AT_ENTRY_LINUX         9
#define AT_NOTELF_LINUX        10
#define AT_UID_LINUX           11
#define AT_EUID_LINUX          12
#define AT_GID_LINUX           13
#define AT_EGID_LINUX          14
#define AT_PLATFORM_LINUX      15
#define AT_HWCAP_LINUX         16
#define AT_CLKTCK_LINUX        17
#define AT_SECURE_LINUX        23
#define AT_BASE_PLATFORM_LINUX 24
#define AT_RANDOM_LINUX        25
#define AT_HWCAP2_LINUX        26
#define AT_EXECFN_LINUX        31
#define AT_MINSIGSTKSZ_LINUX   51

#define IFNAMSIZ_LINUX 16

#define SIOCGIFCONF_LINUX    0x8912
#define SIOCGIFADDR_LINUX    0x8915
#define SIOCGIFNETMASK_LINUX 0x891b
#define SIOCGIFBRDADDR_LINUX 0x8919
#define SIOCGIFDSTADDR_LINUX 0x8917

#define AF_UNSPEC_LINUX 0
#define AF_UNIX_LINUX   1
#define AF_INET_LINUX   2
#define AF_INET6_LINUX  10

#define SOL_SOCKET_LINUX 1
#define SOL_TCP_LINUX    6
#define SOL_UDP_LINUX    17

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

#define NCCS_LINUX     19
#define VINTR_LINUX    0
#define VQUIT_LINUX    1
#define VERASE_LINUX   2
#define VKILL_LINUX    3
#define VEOF_LINUX     4
#define VTIME_LINUX    5
#define VMIN_LINUX     6
#define VSWTC_LINUX    7
#define VSTART_LINUX   8
#define VSTOP_LINUX    9
#define VSUSP_LINUX    10
#define VEOL_LINUX     11
#define VREPRINT_LINUX 12
#define VDISCARD_LINUX 13
#define VWERASE_LINUX  14
#define VLNEXT_LINUX   15
#define VEOL2_LINUX    16

#define RLIM_INFINITY_LINUX 0xffffffffffffffffull

#define MINSIGSTKSZ_LINUX   2048
#define SS_ONSTACK_LINUX    1
#define SS_DISABLE_LINUX    2
#define SS_AUTODISARM_LINUX 0x80000000u

// termios::c_iflag
#define IGNBRK_LINUX  0000001
#define BRKINT_LINUX  0000002
#define IGNPAR_LINUX  0000004
#define PARMRK_LINUX  0000010
#define INPCK_LINUX   0000020
#define ISTRIP_LINUX  0000040
#define INLCR_LINUX   0000100
#define IGNCR_LINUX   0000200
#define ICRNL_LINUX   0000400
#define IUCLC_LINUX   0001000
#define IXON_LINUX    0002000
#define IXANY_LINUX   0004000
#define IXOFF_LINUX   0010000
#define IMAXBEL_LINUX 0020000
#define IUTF8_LINUX   0040000

// termios::c_oflag
#define OPOST_LINUX  0000001
#define OLCUC_LINUX  0000002
#define ONLCR_LINUX  0000004
#define OCRNL_LINUX  0000010
#define ONOCR_LINUX  0000020
#define ONLRET_LINUX 0000040
#define OFILL_LINUX  0000100
#define OFDEL_LINUX  0000200
#define NLDLY_LINUX  0000400
#define NL0_LINUX    0000000
#define NL1_LINUX    0000400
#define CRDLY_LINUX  0003000
#define CR0_LINUX    0000000
#define CR1_LINUX    0001000
#define CR2_LINUX    0002000
#define CR3_LINUX    0003000
#define TABDLY_LINUX 0014000
#define TAB0_LINUX   0000000
#define TAB1_LINUX   0004000
#define TAB2_LINUX   0010000
#define TAB3_LINUX   0014000
#define XTABS_LINUX  0014000
#define BSDLY_LINUX  0020000
#define BS0_LINUX    0000000
#define BS1_LINUX    0020000
#define VTDLY_LINUX  0040000
#define VT0_LINUX    0000000
#define VT1_LINUX    0040000
#define FFDLY_LINUX  0100000
#define FF0_LINUX    0000000
#define FF1_LINUX    0100000

// termios::c_cflag
#define EXTA_LINUX     B19200
#define EXTB_LINUX     B38400
#define CSIZE_LINUX    0000060
#define CS5_LINUX      0000000
#define CS6_LINUX      0000020
#define CS7_LINUX      0000040
#define CS8_LINUX      0000060
#define CSTOPB_LINUX   0000100
#define CREAD_LINUX    0000200
#define PARENB_LINUX   0000400
#define PARODD_LINUX   0001000
#define HUPCL_LINUX    0002000
#define CLOCAL_LINUX   0004000
#define CBAUD_LINUX    0010017
#define CBAUDEX_LINUX  0010000
#define B0_LINUX       0000000  // shut it down
#define B50_LINUX      0000001
#define B75_LINUX      0000002
#define B110_LINUX     0000003
#define B134_LINUX     0000004
#define B150_LINUX     0000005
#define B200_LINUX     0000006
#define B300_LINUX     0000007
#define B600_LINUX     0000010
#define B1200_LINUX    0000011
#define B1800_LINUX    0000012
#define B2400_LINUX    0000013
#define B4800_LINUX    0000014
#define B9600_LINUX    0000015
#define B19200_LINUX   0000016
#define B38400_LINUX   0000017
#define B57600_LINUX   0010001
#define B115200_LINUX  0010002
#define B230400_LINUX  0010003
#define B460800_LINUX  0010004
#define B500000_LINUX  0010005
#define B576000_LINUX  0010006
#define B921600_LINUX  0010007
#define B1000000_LINUX 0010010
#define B1152000_LINUX 0010011
#define B1500000_LINUX 0010012
#define B2000000_LINUX 0010013
#define B2500000_LINUX 0010014
#define B3000000_LINUX 0010015
#define B3500000_LINUX 0010016
#define B4000000_LINUX 0010017
#define CIBAUD_LINUX   002003600000  // input baud rate (isn't used)
#define CMSPAR_LINUX   010000000000  // sticky parity
#define CRTSCTS_LINUX  020000000000  // flow control

// termios::c_lflag
#define ISIG_LINUX    0000001
#define ICANON_LINUX  0000002
#define XCASE_LINUX   0000004
#define ECHO_LINUX    0000010
#define ECHOE_LINUX   0000020
#define ECHOK_LINUX   0000040
#define ECHONL_LINUX  0000100
#define NOFLSH_LINUX  0000200
#define TOSTOP_LINUX  0000400
#define ECHOCTL_LINUX 0001000
#define ECHOPRT_LINUX 0002000
#define ECHOKE_LINUX  0004000
#define FLUSHO_LINUX  0010000
#define PENDIN_LINUX  0040000
#define IEXTEN_LINUX  0100000

#define FD_SETSIZE_LINUX 1024

#define FUTEX_WAITERS_LINUX    0x80000000
#define FUTEX_OWNER_DIED_LINUX 0x40000000
#define FUTEX_TID_MASK_LINUX   0x3fffffff

#define UTIME_NOW_LINUX  ((1l << 30) - 1l)
#define UTIME_OMIT_LINUX ((1l << 30) - 2l)

#define ITIMER_REAL_LINUX    0
#define ITIMER_VIRTUAL_LINUX 1
#define ITIMER_PROF_LINUX    2

#define FIOASYNC_LINUX           0x5452
#define FIOCLEX_LINUX            0x5451
#define FIONBIO_LINUX            0x5421
#define FIONCLEX_LINUX           0x5450
#define FIONREAD_LINUX           0x541b
#define FIOQSIZE_LINUX           0x5460
#define TCFLSH_LINUX             0x540b
#define TCGETA_LINUX             0x5405
#define TCGETS_LINUX             0x5401
#define TCGETX_LINUX             0x5432
#define TCSBRK_LINUX             0x5409
#define TCSBRKP_LINUX            0x5425
#define TCSETA_LINUX             0x5406
#define TCSETAF_LINUX            0x5408
#define TCSETAW_LINUX            0x5407
#define TCSETS_LINUX             0x5402
#define TCSETSF_LINUX            0x5404
#define TCSETSW_LINUX            0x5403
#define TCSETX_LINUX             0x5433
#define TCSETXF_LINUX            0x5434
#define TCSETXW_LINUX            0x5435
#define TCXONC_LINUX             0x540a
#define TIOCCBRK_LINUX           0x5428
#define TIOCCONS_LINUX           0x541d
#define TIOCEXCL_LINUX           0x540c
#define TIOCGDEV_LINUX           0x80045432
#define TIOCGETD_LINUX           0x5424
#define TIOCGEXCL_LINUX          0x80045440
#define TIOCGICOUNT_LINUX        0x545d
#define TIOCGISO7816_LINUX       0x80285442
#define TIOCGLCKTRMIOS_LINUX     0x5456
#define TIOCGPGRP_LINUX          0x540f
#define TIOCGPKT_LINUX           0x80045438
#define TIOCGPTLCK_LINUX         0x80045439
#define TIOCGPTN_LINUX           0x80045430
#define TIOCGPTPEER_LINUX        0x5441
#define TIOCGRS485_LINUX         0x542e
#define TIOCGSERIAL_LINUX        0x541e
#define TIOCGSID_LINUX           0x5429
#define TIOCGSOFTCAR_LINUX       0x5419
#define TIOCGWINSZ_LINUX         0x5413
#define TIOCINQ_LINUX            0x541b
#define TIOCLINUX_LINUX          0x541c
#define TIOCMBIC_LINUX           0x5417
#define TIOCMBIS_LINUX           0x5416
#define TIOCMGET_LINUX           0x5415
#define TIOCMIWAIT_LINUX         0x545c
#define TIOCMSET_LINUX           0x5418
#define TIOCNOTTY_LINUX          0x5422
#define TIOCNXCL_LINUX           0x540d
#define TIOCOUTQ_LINUX           0x5411
#define TIOCPKT_LINUX            0x5420
#define TIOCPKT_DATA_LINUX       0
#define TIOCPKT_DOSTOP_LINUX     0x20
#define TIOCPKT_FLUSHREAD_LINUX  0x1
#define TIOCPKT_FLUSHWRITE_LINUX 0x2
#define TIOCPKT_IOCTL_LINUX      0x40
#define TIOCPKT_NOSTOP_LINUX     0x10
#define TIOCPKT_START_LINUX      0x8
#define TIOCPKT_STOP_LINUX       0x4
#define TIOCSBRK_LINUX           0x5427
#define TIOCSCTTY_LINUX          0x540e
#define TIOCSERCONFIG_LINUX      0x5453
#define TIOCSERGETLSR_LINUX      0x5459
#define TIOCSERGETMULTI_LINUX    0x545a
#define TIOCSERGSTRUCT_LINUX     0x5458
#define TIOCSERGWILD_LINUX       0x5454
#define TIOCSERSETMULTI_LINUX    0x545b
#define TIOCSERSWILD_LINUX       0x5455
#define TIOCSER_TEMT_LINUX       0x1
#define TIOCSETD_LINUX           0x5423
#define TIOCSIG_LINUX            0x40045436
#define TIOCSISO7816_LINUX       0xc0285443
#define TIOCSLCKTRMIOS_LINUX     0x5457
#define TIOCSPGRP_LINUX          0x5410
#define TIOCSPTLCK_LINUX         0x40045431
#define TIOCSRS485_LINUX         0x542f
#define TIOCSSERIAL_LINUX        0x541f
#define TIOCSSOFTCAR_LINUX       0x541a
#define TIOCSTI_LINUX            0x5412
#define TIOCSWINSZ_LINUX         0x5414
#define TIOCVHANGUP_LINUX        0x5437

struct iovec_linux {
  u8 base[8];
  u8 len[8];
};

struct pollfd_linux {
  u8 fd[4];
  u8 events[2];
  u8 revents[2];
};

struct timeval_linux {
  u8 sec[8];
  u8 usec[8];
};

struct timespec_linux {
  u8 sec[8];
  u8 nsec[8];
};

struct timezone_linux {
  u8 minuteswest[4];
  u8 dsttime[4];
};

struct sigaction_linux {
  u8 handler[8];
  u8 flags[8];
  u8 restorer[8];
  u8 mask[8];
};

struct winsize_linux {
  u8 row[2];
  u8 col[2];
  u8 xpixel[2];
  u8 ypixel[2];
};

struct termios_linux {
  u8 iflag[4];
  u8 oflag[4];
  u8 cflag[4];
  u8 lflag[4];
  u8 line;
  u8 cc[NCCS_LINUX];
};

struct sockaddr_linux {
  u8 family[2];
};

struct sockaddr_un_linux {
  u8 family[2];
  char path[108];
};

struct sockaddr_in_linux {
  u8 family[2];
  u16 port;
  u32 addr;
  u8 zero[8];
};

struct sockaddr_in6_linux {
  u8 family[2];
  u16 port;
  u8 flowinfo[4];
  u8 addr[16];
  u8 scope_id[4];
};

struct sockaddr_storage_linux {
  union {
    u8 family[2];
    char storage[128];
  };
};

struct stat_linux {
  u8 dev[8];
  u8 ino[8];
  u8 nlink[8];
  u8 mode[4];
  u8 uid[4];
  u8 gid[4];
  u8 pad_[4];
  u8 rdev[8];
  u8 size[8];
  u8 blksize[8];
  u8 blocks[8];
  struct timespec_linux atim;
  struct timespec_linux mtim;
  struct timespec_linux ctim;
};

struct itimerval_linux {
  struct timeval_linux interval;
  struct timeval_linux value;
};

struct rusage_linux {
  struct timeval_linux utime;
  struct timeval_linux stime;
  u8 maxrss[8];
  u8 ixrss[8];
  u8 idrss[8];
  u8 isrss[8];
  u8 minflt[8];
  u8 majflt[8];
  u8 nswap[8];
  u8 inblock[8];
  u8 oublock[8];
  u8 msgsnd[8];
  u8 msgrcv[8];
  u8 nsignals[8];
  u8 nvcsw[8];
  u8 nivcsw[8];
};

struct siginfo_linux {
  u8 si_signo[4];
  u8 si_errno[4];
  u8 si_code[4];
  u8 pad_[4];
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
  u8 padding_[96];
};

struct ucontext_linux {
  u8 uc__flags[8];
  u8 uc__link[8];
  u8 ss__sp[8];
  u8 ss__flags[4];
  u8 pad0_[4];
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
  u8 pad1_[64];
  u8 sigmask[8];
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
  u8 cur[8];
  u8 max[8];
};

struct dirent_linux {
  u8 ino[8];       // inode number
  u8 off[8];       // implementation-dependent location number
  u8 reclen[2];    // byte length of this whole struct and string
  u8 type[1];      // DT_REG, DT_DIR, DT_UNKNOWN, DT_BLK, etc.
  char name[256];  // NUL-terminated basename
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
  u8 type[2];
  u8 whence[2];
  u8 pad1_[4];
  u8 start[8];
  u8 len[8];
  u8 pid[4];
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
  u8 ignore_[6];    // padding
  u8 totalhigh[8];  // wut
  u8 freehigh[8];   // wut
  u8 mem_unit[4];   // ram stuff above is multiples of this
};

struct tms_linux {
  u8 utime[8];   // user time
  u8 stime[8];   // system time
  u8 cutime[8];  // user time of children
  u8 cstime[8];  // system time of children
};

struct sigaltstack_linux {
  u8 sp[8];     // base address of stack
  u8 flags[4];  // SS_???_LINUX flags
  u8 pad1_[4];  //
  u8 size[8];   // size of stack
};

struct pselect6_linux {
  u8 sigmaskaddr[8];
  u8 sigmasksize[8];
};

struct sigset_linux {
  u8 sigmask[8];
};

struct robust_list_linux {
  u8 next[8];
  u8 offset[8];
  u8 pending[8];
};

struct utimbuf_linux {
  u8 actime[8];
  u8 modtime[8];
};

struct f_owner_ex_linux {
  u8 type[4];
  u8 pid[4];
};

struct statfs_linux {
  u8 type[8];     // type of filesystem
  u8 bsize[8];    // optimal transfer block size
  u8 blocks[8];   // total data blocks in filesystem
  u8 bfree[8];    // free blocks in filesystem
  u8 bavail[8];   // free blocks available to
  u8 files[8];    // total file nodes in filesystem
  u8 ffree[8];    // free file nodes in filesystem
  u8 fsid[2][4];  // filesystem id
  u8 namelen[8];  // maximum length of filenames
  u8 frsize[8];   // fragment size
  u8 flags[8];    // mount flags of filesystem 2.6.36
  u8 spare[4][8];
};

int sysinfo_linux(struct sysinfo_linux *);

#endif /* BLINK_LINUX_H_ */
