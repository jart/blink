#ifndef BLINK_HOSTFS_H_
#define BLINK_HOSTFS_H_

#include <sys/types.h>
#include <sys/un.h>

#include "blink/vfs.h"

struct HostfsInfo {
  int mode;
  int filefd;
  int socketfamily;
  union {
    DIR *dirstream;
    struct sockaddr *socketaddr;
  };
  socklen_t socketaddrlen;
  struct sockaddr *socketpeeraddr;
  socklen_t socketpeeraddrlen;
};

int HostfsInit(const char *, u64, const void *, struct VfsDevice **,
               struct VfsMount **);
int HostfsCreateInfo(struct HostfsInfo **);
int HostfsFreeInfo(void *);
int HostfsFreeDevice(void *);
int HostfsFinddir(struct VfsInfo *, const char *, struct VfsInfo **);
ssize_t HostfsReadlink(struct VfsInfo *, char **);
int HostfsMkdir(struct VfsInfo *, const char *, mode_t);
int HostfsOpen(struct VfsInfo *, const char *, int, int, struct VfsInfo **);
int HostfsAccess(struct VfsInfo *, const char *, mode_t, int);
int HostfsStat(struct VfsInfo *, const char *, struct stat *, int);
int HostfsFstat(struct VfsInfo *, struct stat *);
int HostfsChmod(struct VfsInfo *, const char *, mode_t, int);
int HostfsFchmod(struct VfsInfo *, mode_t);
int HostfsChown(struct VfsInfo *, const char *, uid_t, gid_t, int);
int HostfsFchown(struct VfsInfo *, uid_t, gid_t);
int HostfsFtruncate(struct VfsInfo *, off_t);
int HostfsLink(struct VfsInfo *, const char *, struct VfsInfo *, const char *,
               int);
int HostfsUnlink(struct VfsInfo *, const char *, int);
ssize_t HostfsRead(struct VfsInfo *, void *, size_t);
ssize_t HostfsWrite(struct VfsInfo *, const void *, size_t);
ssize_t HostfsPread(struct VfsInfo *, void *, size_t, off_t);
ssize_t HostfsPwrite(struct VfsInfo *, const void *, size_t, off_t);
ssize_t HostfsReadv(struct VfsInfo *, const struct iovec *, int);
ssize_t HostfsWritev(struct VfsInfo *, const struct iovec *, int);
ssize_t HostfsPreadv(struct VfsInfo *, const struct iovec *, int, off_t);
ssize_t HostfsPwritev(struct VfsInfo *, const struct iovec *, int, off_t);
off_t HostfsSeek(struct VfsInfo *, off_t, int);
int HostfsFsync(struct VfsInfo *);
int HostfsFdatasync(struct VfsInfo *);
int HostfsFlock(struct VfsInfo *, int);
int HostfsFcntl(struct VfsInfo *, int, va_list);
int HostfsIoctl(struct VfsInfo *, unsigned long, const void *);
int HostfsDup(struct VfsInfo *, struct VfsInfo **);
#ifdef HAVE_DUP3
int HostfsDup3(struct VfsInfo *, struct VfsInfo **, int);
#endif
int HostfsPoll(struct VfsInfo **, struct pollfd *, nfds_t, int);
int HostfsOpendir(struct VfsInfo *, struct VfsInfo **);
#ifdef HAVE_SEEKDIR
void HostfsSeekdir(struct VfsInfo *, long);
long HostfsTelldir(struct VfsInfo *);
#endif
struct dirent *HostfsReaddir(struct VfsInfo *);
void HostfsRewinddir(struct VfsInfo *);
int HostfsClosedir(struct VfsInfo *);
int HostfsBind(struct VfsInfo *, const struct sockaddr *, socklen_t);
int HostfsConnect(struct VfsInfo *, const struct sockaddr *, socklen_t);
int HostfsConnectUnix(struct VfsInfo *, struct VfsInfo *,
                      const struct sockaddr_un *, socklen_t);
int HostfsAccept(struct VfsInfo *, struct sockaddr *, socklen_t *,
                 struct VfsInfo **);
int HostfsListen(struct VfsInfo *, int);
int HostfsShutdown(struct VfsInfo *, int);
ssize_t HostfsRecvmsg(struct VfsInfo *, struct msghdr *, int);
ssize_t HostfsSendmsg(struct VfsInfo *, const struct msghdr *, int);
ssize_t HostfsRecvmsgUnix(struct VfsInfo *, struct VfsInfo *, struct msghdr *,
                          int);
ssize_t HostfsSendmsgUnix(struct VfsInfo *, struct VfsInfo *,
                          const struct msghdr *, int);
int HostfsGetsockopt(struct VfsInfo *, int, int, void *, socklen_t *);
int HostfsSetsockopt(struct VfsInfo *, int, int, const void *, socklen_t);
int HostfsGetsockname(struct VfsInfo *, struct sockaddr *, socklen_t *);
int HostfsGetpeername(struct VfsInfo *, struct sockaddr *, socklen_t *);
int HostfsRename(struct VfsInfo *, const char *, struct VfsInfo *,
                 const char *);
int HostfsUtime(struct VfsInfo *, const char *, const struct timespec[2], int);
int HostfsFutime(struct VfsInfo *, const struct timespec[2]);
int HostfsSymlink(const char *, struct VfsInfo *, const char *);

void *HostfsMmap(struct VfsInfo *, void *, size_t, int, int, off_t);
int HostfsMunmap(struct VfsInfo *, void *, size_t);
int HostfsMprotect(struct VfsInfo *, void *, size_t, int);
int HostfsMsync(struct VfsInfo *, void *, size_t, int);

int HostfsPipe(struct VfsInfo *[2]);
#ifdef HAVE_PIPE2
int HostfsPipe2(struct VfsInfo *[2], int);
#endif
int HostfsSocket(int, int, int, struct VfsInfo **);
int HostfsSocketpair(int, int, int, struct VfsInfo *[2]);

int HostfsTcgetattr(struct VfsInfo *, struct termios *);
int HostfsTcsetattr(struct VfsInfo *, int, const struct termios *);
int HostfsTcflush(struct VfsInfo *, int);
int HostfsTcdrain(struct VfsInfo *);
int HostfsTcsendbreak(struct VfsInfo *, int);
int HostfsTcflow(struct VfsInfo *, int);
pid_t HostfsTcgetsid(struct VfsInfo *);
pid_t HostfsTcgetpgrp(struct VfsInfo *);
int HostfsTcsetpgrp(struct VfsInfo *, pid_t);
int HostfsSockatmark(struct VfsInfo *);
int HostfsFexecve(struct VfsInfo *, char *const *, char *const *);

int HostfsWrapFd(int fd, bool dodup, struct VfsInfo **output);

extern struct VfsSystem g_hostfs;

#endif  // BLINK_HOSTFS_H_
