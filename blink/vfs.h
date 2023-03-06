#ifndef BLINK_VFS_H_
#define BLINK_VFS_H_
#include <dirent.h>
#include <stdbool.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>

#include "blink/dll.h"
#include "blink/preadv.h"
#include "blink/thread.h"
#include "blink/tsan.h"
#include "blink/types.h"

struct VfsDevice;
struct VfsMount;
struct VfsInfo;
struct VfsSystem;
struct VfsFd;
struct VfsMap;

struct Vfs {
  struct Dll *devices GUARDED_BY(lock);
  struct Dll *systems GUARDED_BY(lock);
  struct Dll *fds GUARDED_BY(lock);
  struct Dll *maps GUARDED_BY(mapslock);
  pthread_mutex_t_ lock;
  pthread_mutex_t_ mapslock;
};

struct VfsOps {
  int (*init)(const char *, u64, const void *, struct VfsDevice **,
              struct VfsMount **);
  int (*freeinfo)(void *);
  int (*freedevice)(void *);
  int (*finddir)(struct VfsInfo *, const char *, struct VfsInfo **);
  ssize_t (*readlink)(struct VfsInfo *, char **);
  int (*mkdir)(struct VfsInfo *, const char *, u32);
  int (*open)(struct VfsInfo *, const char *, int, int, struct VfsInfo **);
  int (*access)(struct VfsInfo *, const char *, mode_t, int);
  int (*stat)(struct VfsInfo *, const char *, struct stat *, int);
  int (*fstat)(struct VfsInfo *, struct stat *);
  int (*chmod)(struct VfsInfo *, const char *, mode_t, int);
  int (*fchmod)(struct VfsInfo *, mode_t);
  int (*chown)(struct VfsInfo *, const char *, uid_t, gid_t, int);
  int (*fchown)(struct VfsInfo *, uid_t, gid_t);
  int (*ftruncate)(struct VfsInfo *, off_t);
  int (*link)(struct VfsInfo *, const char *, struct VfsInfo *, const char *,
              int);
  int (*unlink)(struct VfsInfo *, const char *, int);
  ssize_t (*read)(struct VfsInfo *, void *, size_t);
  ssize_t (*write)(struct VfsInfo *, const void *, size_t);
  ssize_t (*pread)(struct VfsInfo *, void *, size_t, off_t);
  ssize_t (*pwrite)(struct VfsInfo *, const void *, size_t, off_t);
  ssize_t (*readv)(struct VfsInfo *, const struct iovec *, int);
  ssize_t (*writev)(struct VfsInfo *, const struct iovec *, int);
  ssize_t (*preadv)(struct VfsInfo *, const struct iovec *, int, off_t);
  ssize_t (*pwritev)(struct VfsInfo *, const struct iovec *, int, off_t);
  off_t (*seek)(struct VfsInfo *, off_t, int);
  int (*fsync)(struct VfsInfo *);
  int (*fdatasync)(struct VfsInfo *);
  int (*flock)(struct VfsInfo *, int);
  int (*fcntl)(struct VfsInfo *, int, va_list);
  int (*ioctl)(struct VfsInfo *, unsigned long, const void *);
  int (*dup)(struct VfsInfo *, struct VfsInfo **);
#ifdef HAVE_DUP3
  int (*dup3)(struct VfsInfo *, struct VfsInfo **, int);
#endif
  int (*poll)(struct VfsInfo **, struct pollfd *, nfds_t, int);
  int (*opendir)(struct VfsInfo *, struct VfsInfo **);
#ifdef HAVE_SEEKDIR
  void (*seekdir)(struct VfsInfo *, long);
  long (*telldir)(struct VfsInfo *);
#endif
  struct dirent *(*readdir)(struct VfsInfo *);
  void (*rewinddir)(struct VfsInfo *);
  int (*closedir)(struct VfsInfo *);
  int (*bind)(struct VfsInfo *, const struct sockaddr *, socklen_t);
  int (*connect)(struct VfsInfo *, const struct sockaddr *, socklen_t);
  int (*connectunix)(struct VfsInfo *, struct VfsInfo *,
                     const struct sockaddr_un *, socklen_t);
  int (*accept)(struct VfsInfo *, struct sockaddr *, socklen_t *,
                struct VfsInfo **);
  int (*listen)(struct VfsInfo *, int);
  int (*shutdown)(struct VfsInfo *, int);
  ssize_t (*recvmsg)(struct VfsInfo *, struct msghdr *, int);
  ssize_t (*sendmsg)(struct VfsInfo *, const struct msghdr *, int);
  ssize_t (*recvmsgunix)(struct VfsInfo *, struct VfsInfo *, struct msghdr *,
                         int);
  ssize_t (*sendmsgunix)(struct VfsInfo *, struct VfsInfo *,
                         const struct msghdr *, int);
  int (*getsockopt)(struct VfsInfo *, int, int, void *, socklen_t *);
  int (*setsockopt)(struct VfsInfo *, int, int, const void *, socklen_t);
  int (*getsockname)(struct VfsInfo *, struct sockaddr *, socklen_t *);
  int (*getpeername)(struct VfsInfo *, struct sockaddr *, socklen_t *);
  int (*rename)(struct VfsInfo *, const char *, struct VfsInfo *, const char *);
  int (*utime)(struct VfsInfo *, const char *, const struct timespec[2], int);
  int (*futime)(struct VfsInfo *, const struct timespec[2]);
  int (*symlink)(const char *, struct VfsInfo *, const char *);
  void *(*mmap)(struct VfsInfo *, void *, size_t, int, int, off_t);
  int (*munmap)(struct VfsInfo *, void *, size_t);
  int (*mprotect)(struct VfsInfo *, void *, size_t, int);
  int (*msync)(struct VfsInfo *, void *, size_t, int);
  int (*pipe)(struct VfsInfo *[2]);
#ifdef HAVE_PIPE2
  int (*pipe2)(struct VfsInfo *[2], int);
#endif
  int (*socket)(int, int, int, struct VfsInfo **);
  int (*socketpair)(int, int, int, struct VfsInfo *[2]);
  int (*tcgetattr)(struct VfsInfo *, struct termios *);
  int (*tcsetattr)(struct VfsInfo *, int, const struct termios *);
  int (*tcflush)(struct VfsInfo *, int);
  int (*tcdrain)(struct VfsInfo *);
  int (*tcsendbreak)(struct VfsInfo *, int);
  int (*tcflow)(struct VfsInfo *, int);
  pid_t (*tcgetsid)(struct VfsInfo *);
  pid_t (*tcgetpgrp)(struct VfsInfo *);
  int (*tcsetpgrp)(struct VfsInfo *, pid_t);
  int (*sockatmark)(struct VfsInfo *);
  int (*fexecve)(struct VfsInfo *, char *const *, char *const *);
};

struct VfsSystem {
  const char *name;
  struct Dll elem;
  struct VfsOps ops;
};

struct VfsMount {
  u64 baseino;
  struct VfsInfo *root;
  struct Dll elem;
};

struct VfsDevice {
  pthread_mutex_t_ lock;
  struct Dll *mounts;
  struct VfsOps *ops;
  struct VfsInfo *root;
  void *data;
  struct Dll elem;
  u64 flags;
  u32 dev;
  _Atomic(u32) refcount;
};

struct VfsInfo {
  struct VfsDevice *device;
  struct VfsInfo *parent;
  const char *name;
  void *data;
  u64 ino;
  u32 dev;
  u32 mode;
  _Atomic(u32) refcount;
};

#define VFS_SYSTEM_CONTAINER(e) DLL_CONTAINER(struct VfsSystem, elem, (e))
#define VFS_MOUNT_CONTAINER(e)  DLL_CONTAINER(struct VfsMount, elem, (e))
#define VFS_DEVICE_CONTAINER(e) DLL_CONTAINER(struct VfsDevice, elem, (e))

#if !defined(DISABLE_VFS)
int VfsChdir(const char *);
char *VfsGetcwd(char *, size_t);
int VfsChroot(const char *);
int VfsMount(const char *, const char *, const char *, u64, const void *);
int VfsUnlink(int, const char *, int);
int VfsMkdir(int, const char *, mode_t);
int VfsMkfifo(int, const char *, mode_t);
int VfsOpen(int, const char *, int, int);
int VfsChmod(int, const char *, mode_t, int);
int VfsAccess(int, const char *, mode_t, int);
int VfsSymlink(const char *, int, const char *);
int VfsStat(int, const char *, struct stat *, int);
int VfsChown(int, const char *, uid_t, gid_t, int);
int VfsRename(int, const char *, int, const char *);
ssize_t VfsReadlink(int, const char *, char *, size_t);
int VfsLink(int, const char *, int, const char *, int);
int VfsUtime(int, const char *, const struct timespec[2], int);
int VfsFutime(int, const struct timespec[2]);
int VfsFchown(int, uid_t, gid_t);
int VfsFstat(int, struct stat *);
int VfsFtruncate(int, off_t);
int VfsClose(int);
ssize_t VfsRead(int, void *, size_t);
ssize_t VfsWrite(int, const void *, size_t);
ssize_t VfsPread(int, void *, size_t, off_t);
ssize_t VfsPwrite(int, const void *, size_t, off_t);
ssize_t VfsReadv(int, const struct iovec *, int);
ssize_t VfsWritev(int, const struct iovec *, int);
ssize_t VfsPreadv(int, const struct iovec *, int, off_t);
ssize_t VfsPwritev(int, const struct iovec *, int, off_t);
off_t VfsSeek(int, off_t, int);
int VfsFchmod(int, mode_t);
int VfsFchdir(int);
int VfsFsync(int);
int VfsFdatasync(int);
int VfsFlock(int, int);
int VfsFcntl(int, int, ...);
int VfsIoctl(int, unsigned long, const void *);
int VfsDup(int);
int VfsDup2(int, int);
#ifdef HAVE_DUP3
int VfsDup3(int, int, int);
#endif
int VfsPoll(struct pollfd *, nfds_t, int);
int VfsSelect(int, fd_set *, fd_set *, fd_set *, struct timespec *, sigset_t *);
DIR *VfsOpendir(int);
#ifdef HAVE_SEEKDIR
void VfsSeekdir(DIR *, long);
long VfsTelldir(DIR *);
#endif
struct dirent *VfsReaddir(DIR *);
void VfsRewinddir(DIR *);
int VfsClosedir(DIR *);
int VfsBind(int, const struct sockaddr *, socklen_t);
int VfsConnect(int, const struct sockaddr *, socklen_t);
int VfsAccept(int, struct sockaddr *, socklen_t *);
int VfsListen(int, int);
int VfsShutdown(int, int);
ssize_t VfsRecvmsg(int, struct msghdr *, int);
ssize_t VfsSendmsg(int, const struct msghdr *, int);
int VfsGetsockopt(int, int, int, void *, socklen_t *);
int VfsSetsockopt(int, int, int, const void *, socklen_t);
int VfsGetsockname(int, struct sockaddr *, socklen_t *);
int VfsGetpeername(int, struct sockaddr *, socklen_t *);
int VfsPipe(int[2]);
#ifdef HAVE_PIPE2
int VfsPipe2(int[2], int);
#endif
int VfsSocket(int, int, int);
int VfsSocketpair(int, int, int, int[2]);

int VfsTcgetattr(int, struct termios *);
int VfsTcsetattr(int, int, const struct termios *);
pid_t VfsTcgetpgrp(int);
int VfsTcsetpgrp(int, pid_t);
int VfsTcdrain(int);
int VfsTcsendbreak(int, int);
int VfsTcflow(int, int);
pid_t VfsTcgetsid(int);
int VfsTcflush(int, int);
int VfsSockatmark(int);
int VfsExecve(const char *, char *const *, char *const *);

void *VfsMmap(void *, size_t, int, int, int, off_t);
int VfsMunmap(void *, size_t);
int VfsMprotect(void *, size_t, int);
int VfsMsync(void *, size_t, int);

int VfsInit(void);
int VfsRegister(struct VfsSystem *);
int VfsTraverse(const char *, struct VfsInfo **, bool);
int VfsCreateInfo(struct VfsInfo **);
int VfsAcquireInfo(struct VfsInfo *, struct VfsInfo **);
int VfsCreateDevice(struct VfsDevice **output);
int VfsAcquireDevice(struct VfsDevice *, struct VfsDevice **);
int VfsFreeDevice(struct VfsDevice *);
int VfsFreeInfo(struct VfsInfo *);
int VfsAddFd(struct VfsInfo *);
int VfsFreeFd(int, struct VfsInfo **);
int VfsSetFd(int, struct VfsInfo *);
int VfsPathBuild(struct VfsInfo *info, struct VfsInfo *root, char **output);
#elif !defined(DISABLE_OVERLAYS)
#define VfsChown        OverlaysChown
#define VfsAccess       OverlaysAccess
#define VfsStat         OverlaysStat
#define VfsChdir        OverlaysChdir
#define VfsGetcwd       OverlaysGetcwd
#define VfsMkdir        OverlaysMkdir
#define VfsChmod        OverlaysChmod
#define VfsReadlink     OverlaysReadlink
#define VfsOpen         OverlaysOpen
#define VfsSymlink      OverlaysSymlink
#define VfsMkfifo       OverlaysMkfifo
#define VfsUnlink       OverlaysUnlink
#define VfsRename       OverlaysRename
#define VfsLink         OverlaysLink
#define VfsUtime        OverlaysUtime
#define VfsFchown       fchown
#define VfsFchdir       fchdir
#define VfsFchmod       fchmod
#define VfsFutime       futimens
#define VfsFstat        fstat
#define VfsFtruncate    ftruncate
#define VfsClose        close
#define VfsRead         read
#define VfsWrite        write
#define VfsPread        pread
#define VfsPwrite       pwrite
#define VfsReadv        readv
#define VfsWritev       writev
#define VfsPreadv       preadv
#define VfsPwritev      pwritev
#define VfsSeek         lseek
#define VfsFsync        fsync
#define VfsFdatasync    fdatasync
#define VfsFlock        flock
#define VfsFcntl        fcntl
#define VfsDup          dup
#define VfsDup2         dup2
#define VfsDup3         dup3
#define VfsPoll         poll
#define VfsSelect       pselect
#define VfsOpendir      fdopendir
#define VfsSeekdir      seekdir
#define VfsTelldir      telldir
#define VfsReaddir      readdir
#define VfsRewinddir    rewinddir
#define VfsClosedir     closedir
#define VfsPipe2        pipe2
#define VfsSocket       socket
#define VfsSocketpair   socketpair
#define VfsBind         bind
#define VfsConnect      connect
#define VfsAccept       accept
#define VfsListen       listen
#define VfsShutdown     shutdown
#define VfsRecvmsg      recvmsg
#define VfsSendmsg      sendmsg
#define VfsGetsockopt   getsockopt
#define VfsSetsockopt   setsockopt
#define VfsGetsockname  getsockname
#define VfsGetpeername  getpeername
#define VfsIoctl        ioctl
#define VfsTcgetattr    tcgetattr
#define VfsTcsetattr    tcsetattr
#define VfsTcgetpgrp    tcgetpgrp
#define VfsTcsetpgrp    tcsetpgrp
#define VfsTcgetsid     tcgetsid
#define VfsTcsendbreak  tcsendbreak
#define VfsTcflow       tcflow
#define VfsTcflush      tcflush
#define VfsTcdrain      tcdrain
#define VfsSockatmark   sockatmark
#define VfsExecve       execve
#define VfsMmap         mmap
#define VfsMunmap       munmap
#define VfsMprotect     mprotect
#define VfsMsync        msync
#else
#define VfsChown        fchownat
#define VfsAccess       faccessat
#define VfsStat         fstatat
#define VfsChdir        chdir
#define VfsGetcwd       getcwd
#define VfsMkdir        mkdirat
#define VfsChmod        fchmodat
#define VfsReadlink     readlinkat
#define VfsOpen         openat
#define VfsSymlink      symlinkat
#define VfsMkfifo       mkfifoat
#define VfsUnlink       unlinkat
#define VfsRename       renameat
#define VfsLink         linkat
#define VfsUtime        utimensat
#define VfsFchown       fchown
#define VfsFchdir       fchdir
#define VfsFchmod       fchmod
#define VfsFutime       futimens
#define VfsFstat        fstat
#define VfsFtruncate    ftruncate
#define VfsClose        close
#define VfsRead         read
#define VfsWrite        write
#define VfsPread        pread
#define VfsPwrite       pwrite
#define VfsReadv        readv
#define VfsWritev       writev
#define VfsPreadv       preadv
#define VfsPwritev      pwritev
#define VfsSeek         lseek
#define VfsFsync        fsync
#define VfsFdatasync    fdatasync
#define VfsFlock        flock
#define VfsFcntl        fcntl
#define VfsDup          dup
#define VfsDup2         dup2
#define VfsDup3         dup3
#define VfsPoll         poll
#define VfsSelect       pselect
#define VfsOpendir      fdopendir
#define VfsSeekdir      seekdir
#define VfsTelldir      telldir
#define VfsReaddir      readdir
#define VfsRewinddir    rewinddir
#define VfsClosedir     closedir
#define VfsPipe2        pipe2
#define VfsSocket       socket
#define VfsSocketpair   socketpair
#define VfsBind         bind
#define VfsConnect      connect
#define VfsAccept       accept
#define VfsListen       listen
#define VfsShutdown     shutdown
#define VfsRecvmsg      recvmsg
#define VfsSendmsg      sendmsg
#define VfsGetsockopt   getsockopt
#define VfsSetsockopt   setsockopt
#define VfsGetsockname  getsockname
#define VfsGetpeername  getpeername
#define VfsIoctl        ioctl
#define VfsTcgetattr    tcgetattr
#define VfsTcsetattr    tcsetattr
#define VfsTcgetpgrp    tcgetpgrp
#define VfsTcsetpgrp    tcsetpgrp
#define VfsTcgetsid     tcgetsid
#define VfsTcsendbreak  tcsendbreak
#define VfsTcflow       tcflow
#define VfsTcflush      tcflush
#define VfsTcdrain      tcdrain
#define VfsSockatmark sockatmark
#define VfsExecve       execve
#define VfsMmap         mmap
#define VfsMunmap       munmap
#define VfsMprotect     mprotect
#define VfsMsync        msync
#endif

#define VFS_SYSTEM_ROOT_MOUNT "/SystemRoot"

#endif /* BLINK_VFS_H_ */
