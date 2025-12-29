#ifndef BLINK_FLOCK_H_
#define BLINK_FLOCK_H_

#if defined(sun) || defined(__sun)

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#define LOCK_SH 1  // shared lock
#define LOCK_EX 2  // exclusive lock
#define LOCK_UN 8  // unlock
#define LOCK_NB 4  // non-blocking

static int flock(int fd, int operation) {
  struct flock fl = {0};

  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0; // Lock the whole file

  switch (operation & (LOCK_EX | LOCK_SH | LOCK_UN)) {
    case LOCK_EX:
      fl.l_type = F_WRLCK;
      break;
    case LOCK_SH:
      fl.l_type = F_RDLCK;
      break;
    case LOCK_UN:
      fl.l_type = F_UNLCK;
      break;
    default:
      errno = EINVAL;
      return -1;
  }

  return fcntl(fd, (operation & LOCK_NB) ? F_SETLK : F_SETLKW, &fl);
}

#endif

#endif /* BLINK_FLOCK_H_ */
