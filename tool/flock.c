#include <stdlib.h>
#include <string.h>
#include <sys/file.h>

#if defined(sun) || defined(__sun)
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#endif

int main(int argc, char *argv[]) {
  if (argc != 3) return 1;

#if defined(sun) || defined(__sun)
  if (!strcmp(argv[1], "-x")) {
    int fd;
    do {
      fd = open(argv[2], O_CREAT | O_RDWR | O_EXCL, 0666);
      if (fd < 0) {
        if (errno != EEXIST) {
          return 3;
        }
        sleep(1);
      }
    } while (fd < 0);
  } else if (!strcmp(argv[1], "-u")) {
    if (unlink(argv[2]) < 0) {
      return 3;
    }
  } else {
    return 2;
  }
#else
  if (strcmp(argv[1], "-x")) return 2;
  if (flock(atoi(argv[2]), LOCK_EX)) return 3;
#endif

  return 0;
}
