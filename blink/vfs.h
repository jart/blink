#ifndef BLINK_VFS_H_
#define BLINK_VFS_H_
#include <stdbool.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int VfsChdir(const char *);
int VfsInit(void);
char *VfsGetcwd(char *, size_t);
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

#endif /* BLINK_VFS_H_ */
