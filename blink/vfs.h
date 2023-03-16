#ifndef BLINK_VFS_H_
#define BLINK_VFS_H_
#include <dirent.h>
#include <poll.h>
#include <stdarg.h>
#include <stdbool.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <termios.h>
#include <unistd.h>

#include "blink/dll.h"
#include "blink/macros.h"
#include "blink/preadv.h"
#include "blink/thread.h"
#include "blink/tsan.h"
#include "blink/types.h"

#define VFS_SYSTEM_ROOT_MOUNT "/SystemRoot"
#define VFS_PATH_MAX          MAX(PATH_MAX, 4096)
#define VFS_NAME_MAX          256

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
  int (*Init)(const char *, u64, const void *, struct VfsDevice **,
              struct VfsMount **);
  int (*Freeinfo)(void *);
  int (*Freedevice)(void *);
  int (*Readmountentry)(struct VfsDevice *, char **, char **, char **);
  int (*Finddir)(struct VfsInfo *, const char *, struct VfsInfo **);
  int (*Traverse)(struct VfsInfo **, const char **, struct VfsInfo *);
  ssize_t (*Readlink)(struct VfsInfo *, char **);
  int (*Mkdir)(struct VfsInfo *, const char *, mode_t);
  int (*Mkfifo)(struct VfsInfo *, const char *, mode_t);
  int (*Open)(struct VfsInfo *, const char *, int, int, struct VfsInfo **);
  int (*Access)(struct VfsInfo *, const char *, mode_t, int);
  int (*Stat)(struct VfsInfo *, const char *, struct stat *, int);
  int (*Fstat)(struct VfsInfo *, struct stat *);
  int (*Chmod)(struct VfsInfo *, const char *, mode_t, int);
  int (*Fchmod)(struct VfsInfo *, mode_t);
  int (*Chown)(struct VfsInfo *, const char *, uid_t, gid_t, int);
  int (*Fchown)(struct VfsInfo *, uid_t, gid_t);
  int (*Ftruncate)(struct VfsInfo *, off_t);
  int (*Close)(struct VfsInfo *);
  int (*Link)(struct VfsInfo *, const char *, struct VfsInfo *, const char *,
              int);
  int (*Unlink)(struct VfsInfo *, const char *, int);
  ssize_t (*Read)(struct VfsInfo *, void *, size_t);
  ssize_t (*Write)(struct VfsInfo *, const void *, size_t);
  ssize_t (*Pread)(struct VfsInfo *, void *, size_t, off_t);
  ssize_t (*Pwrite)(struct VfsInfo *, const void *, size_t, off_t);
  ssize_t (*Readv)(struct VfsInfo *, const struct iovec *, int);
  ssize_t (*Writev)(struct VfsInfo *, const struct iovec *, int);
  ssize_t (*Preadv)(struct VfsInfo *, const struct iovec *, int, off_t);
  ssize_t (*Pwritev)(struct VfsInfo *, const struct iovec *, int, off_t);
  off_t (*Seek)(struct VfsInfo *, off_t, int);
  int (*Fsync)(struct VfsInfo *);
  int (*Fdatasync)(struct VfsInfo *);
  int (*Flock)(struct VfsInfo *, int);
  int (*Fcntl)(struct VfsInfo *, int, va_list);
  int (*Ioctl)(struct VfsInfo *, unsigned long, const void *);
  int (*Dup)(struct VfsInfo *, struct VfsInfo **);
#ifdef HAVE_DUP3
  int (*Dup3)(struct VfsInfo *, struct VfsInfo **, int);
#endif
  int (*Poll)(struct VfsInfo **, struct pollfd *, nfds_t, int);
  int (*Opendir)(struct VfsInfo *, struct VfsInfo **);
#ifdef HAVE_SEEKDIR
  void (*Seekdir)(struct VfsInfo *, long);
  long (*Telldir)(struct VfsInfo *);
#endif
  struct dirent *(*Readdir)(struct VfsInfo *);
  void (*Rewinddir)(struct VfsInfo *);
  int (*Closedir)(struct VfsInfo *);
  int (*Bind)(struct VfsInfo *, const struct sockaddr *, socklen_t);
  int (*Connect)(struct VfsInfo *, const struct sockaddr *, socklen_t);
  int (*Connectunix)(struct VfsInfo *, struct VfsInfo *,
                     const struct sockaddr_un *, socklen_t);
  int (*Accept)(struct VfsInfo *, struct sockaddr *, socklen_t *,
                struct VfsInfo **);
  int (*Listen)(struct VfsInfo *, int);
  int (*Shutdown)(struct VfsInfo *, int);
  ssize_t (*Recvmsg)(struct VfsInfo *, struct msghdr *, int);
  ssize_t (*Sendmsg)(struct VfsInfo *, const struct msghdr *, int);
  ssize_t (*Recvmsgunix)(struct VfsInfo *, struct VfsInfo *, struct msghdr *,
                         int);
  ssize_t (*Sendmsgunix)(struct VfsInfo *, struct VfsInfo *,
                         const struct msghdr *, int);
  int (*Getsockopt)(struct VfsInfo *, int, int, void *, socklen_t *);
  int (*Setsockopt)(struct VfsInfo *, int, int, const void *, socklen_t);
  int (*Getsockname)(struct VfsInfo *, struct sockaddr *, socklen_t *);
  int (*Getpeername)(struct VfsInfo *, struct sockaddr *, socklen_t *);
  int (*Rename)(struct VfsInfo *, const char *, struct VfsInfo *, const char *);
  int (*Utime)(struct VfsInfo *, const char *, const struct timespec[2], int);
  int (*Futime)(struct VfsInfo *, const struct timespec[2]);
  int (*Symlink)(const char *, struct VfsInfo *, const char *);
  void *(*Mmap)(struct VfsInfo *, void *, size_t, int, int, off_t);
  int (*Munmap)(struct VfsInfo *, void *, size_t);
  int (*Mprotect)(struct VfsInfo *, void *, size_t, int);
  int (*Msync)(struct VfsInfo *, void *, size_t, int);
  int (*Pipe)(struct VfsInfo *[2]);
#ifdef HAVE_PIPE2
  int (*Pipe2)(struct VfsInfo *[2], int);
#endif
  int (*Socket)(int, int, int, struct VfsInfo **);
  int (*Socketpair)(int, int, int, struct VfsInfo *[2]);
  int (*Tcgetattr)(struct VfsInfo *, struct termios *);
  int (*Tcsetattr)(struct VfsInfo *, int, const struct termios *);
  int (*Tcflush)(struct VfsInfo *, int);
  int (*Tcdrain)(struct VfsInfo *);
  int (*Tcsendbreak)(struct VfsInfo *, int);
  int (*Tcflow)(struct VfsInfo *, int);
  pid_t (*Tcgetsid)(struct VfsInfo *);
  pid_t (*Tcgetpgrp)(struct VfsInfo *);
  int (*Tcsetpgrp)(struct VfsInfo *, pid_t);
  int (*Sockatmark)(struct VfsInfo *);
  int (*Fexecve)(struct VfsInfo *, char *const *, char *const *);
};

#define VFS_SYSTEM_NAME_MAX 8

struct VfsSystem {
  struct Dll elem;
  struct VfsOps ops;
  char name[VFS_SYSTEM_NAME_MAX];
  bool nodev;
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
  size_t namelen;
  void *data;
  u64 ino;
  u32 dev;
  u32 mode;
  _Atomic(u32) refcount;
};

extern struct Vfs g_vfs;
extern struct VfsInfo *g_cwdinfo;
extern struct VfsInfo *g_rootinfo;
extern struct VfsInfo *g_actualrootinfo;

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
int VfsIoctl(int, unsigned long, void *);
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

int VfsInit(const char *);
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
ssize_t VfsPathBuildFull(struct VfsInfo *, struct VfsInfo *, char **);
ssize_t VfsPathBuild(struct VfsInfo *, struct VfsInfo *, bool,
                     char[VFS_PATH_MAX]);
#elif !defined(DISABLE_OVERLAYS)
#define VfsChown       OverlaysChown
#define VfsAccess      OverlaysAccess
#define VfsStat        OverlaysStat
#define VfsChdir       OverlaysChdir
#define VfsGetcwd      OverlaysGetcwd
#define VfsMkdir       OverlaysMkdir
#define VfsChmod       OverlaysChmod
#define VfsReadlink    OverlaysReadlink
#define VfsOpen        OverlaysOpen
#define VfsSymlink     OverlaysSymlink
#define VfsMkfifo      OverlaysMkfifo
#define VfsUnlink      OverlaysUnlink
#define VfsRename      OverlaysRename
#define VfsLink        OverlaysLink
#define VfsUtime       OverlaysUtime
#define VfsFchown      fchown
#define VfsFchdir      fchdir
#define VfsFchmod      fchmod
#define VfsFutime      futimens
#define VfsFstat       fstat
#define VfsFtruncate   ftruncate
#define VfsClose       close
#define VfsRead        read
#define VfsWrite       write
#define VfsPread       pread
#define VfsPwrite      pwrite
#define VfsReadv       readv
#define VfsWritev      writev
#define VfsPreadv      preadv
#define VfsPwritev     pwritev
#define VfsSeek        lseek
#define VfsFsync       fsync
#define VfsFdatasync   fdatasync
#define VfsFlock       flock
#define VfsFcntl       fcntl
#define VfsDup         dup
#define VfsDup2        dup2
#define VfsDup3        dup3
#define VfsPoll        poll
#define VfsSelect      pselect
#define VfsOpendir     fdopendir
#define VfsSeekdir     seekdir
#define VfsTelldir     telldir
#define VfsReaddir     readdir
#define VfsRewinddir   rewinddir
#define VfsClosedir    closedir
#define VfsPipe        pipe
#define VfsPipe2       pipe2
#define VfsSocket      socket
#define VfsSocketpair  socketpair
#define VfsBind        bind
#define VfsConnect     connect
#define VfsAccept      accept
#define VfsListen      listen
#define VfsShutdown    shutdown
#define VfsRecvmsg     recvmsg
#define VfsSendmsg     sendmsg
#define VfsGetsockopt  getsockopt
#define VfsSetsockopt  setsockopt
#define VfsGetsockname getsockname
#define VfsGetpeername getpeername
#define VfsIoctl       ioctl
#define VfsTcgetattr   tcgetattr
#define VfsTcsetattr   tcsetattr
#define VfsTcgetpgrp   tcgetpgrp
#define VfsTcsetpgrp   tcsetpgrp
#define VfsTcgetsid    tcgetsid
#define VfsTcsendbreak tcsendbreak
#define VfsTcflow      tcflow
#define VfsTcflush     tcflush
#define VfsTcdrain     tcdrain
#define VfsSockatmark  sockatmark
#define VfsExecve      execve
#define VfsMmap        mmap
#define VfsMunmap      munmap
#define VfsMprotect    mprotect
#define VfsMsync       msync
#else
#define VfsChown       fchownat
#define VfsAccess      faccessat
#define VfsStat        fstatat
#define VfsChdir       chdir
#define VfsGetcwd      getcwd
#define VfsMkdir       mkdirat
#define VfsChmod       fchmodat
#define VfsReadlink    readlinkat
#define VfsOpen        openat
#define VfsSymlink     symlinkat
#define VfsMkfifo      mkfifoat
#define VfsUnlink      unlinkat
#define VfsRename      renameat
#define VfsLink        linkat
#define VfsUtime       utimensat
#define VfsFchown      fchown
#define VfsFchdir      fchdir
#define VfsFchmod      fchmod
#define VfsFutime      futimens
#define VfsFstat       fstat
#define VfsFtruncate   ftruncate
#define VfsClose       close
#define VfsRead        read
#define VfsWrite       write
#define VfsPread       pread
#define VfsPwrite      pwrite
#define VfsReadv       readv
#define VfsWritev      writev
#define VfsPreadv      preadv
#define VfsPwritev     pwritev
#define VfsSeek        lseek
#define VfsFsync       fsync
#define VfsFdatasync   fdatasync
#define VfsFlock       flock
#define VfsFcntl       fcntl
#define VfsDup         dup
#define VfsDup2        dup2
#define VfsDup3        dup3
#define VfsPoll        poll
#define VfsSelect      pselect
#define VfsOpendir     fdopendir
#define VfsSeekdir     seekdir
#define VfsTelldir     telldir
#define VfsReaddir     readdir
#define VfsRewinddir   rewinddir
#define VfsClosedir    closedir
#define VfsPipe        pipe
#define VfsPipe2       pipe2
#define VfsSocket      socket
#define VfsSocketpair  socketpair
#define VfsBind        bind
#define VfsConnect     connect
#define VfsAccept      accept
#define VfsListen      listen
#define VfsShutdown    shutdown
#define VfsRecvmsg     recvmsg
#define VfsSendmsg     sendmsg
#define VfsGetsockopt  getsockopt
#define VfsSetsockopt  setsockopt
#define VfsGetsockname getsockname
#define VfsGetpeername getpeername
#define VfsIoctl       ioctl
#define VfsTcgetattr   tcgetattr
#define VfsTcsetattr   tcsetattr
#define VfsTcgetpgrp   tcgetpgrp
#define VfsTcsetpgrp   tcsetpgrp
#define VfsTcgetsid    tcgetsid
#define VfsTcsendbreak tcsendbreak
#define VfsTcflow      tcflow
#define VfsTcflush     tcflush
#define VfsTcdrain     tcdrain
#define VfsSockatmark  sockatmark
#define VfsExecve      execve
#define VfsMmap        mmap
#define VfsMunmap      munmap
#define VfsMprotect    mprotect
#define VfsMsync       msync
#endif

#endif /* BLINK_VFS_H_ */
