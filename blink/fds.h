#ifndef BLINK_FDS_H_
#define BLINK_FDS_H_
#include <dirent.h>
#include <limits.h>
#include <poll.h>
#include <pthread.h>
#include <sys/uio.h>
#include <termios.h>

#include "blink/dll.h"
#include "blink/types.h"

#define FD_CONTAINER(e) DLL_CONTAINER(struct Fd, elem, e)

struct FdCb {
  int (*close)(int);
  ssize_t (*readv)(int, const struct iovec *, int);
  ssize_t (*writev)(int, const struct iovec *, int);
  int (*ioctl)(int, unsigned long, ...);
  int (*poll)(struct pollfd *, nfds_t, int);
  int (*tcgetattr)(int, struct termios *);
  int (*tcsetattr)(int, int, const struct termios *);
};

struct Fd {
  int fildes;
  int oflags;
  DIR *dirstream;
  struct Dll elem;
  pthread_mutex_t lock;
  const struct FdCb *cb;
};

struct Fds {
  struct Dll *list;
  pthread_mutex_t lock;
};

extern const struct FdCb kFdCbHost;

void InitFds(struct Fds *);
void LockFds(struct Fds *);
struct Fd *AddFd(struct Fds *, int, int);
struct Fd *GetFd(struct Fds *, int);
void LockFd(struct Fd *);
void UnlockFd(struct Fd *);
int CountFds(struct Fds *);
void FreeFd(struct Fds *, struct Fd *);
void UnlockFds(struct Fds *);
void DestroyFds(struct Fds *);

#endif /* BLINK_FDS_H_ */
