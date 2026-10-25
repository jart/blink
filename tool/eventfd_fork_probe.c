#define _GNU_SOURCE
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

/* Linux eventfd counters are shared across fork. */
int main(void) {
  uint64_t value = 1, got = 0;
  int status, fd = eventfd(0, EFD_NONBLOCK);
  if (fd == -1) return 1;
  pid_t child = fork();
  if (child == -1) return 2;
  if (!child) {
    ssize_t n = write(fd, &value, sizeof(value));
    if (n != sizeof(value)) {
      dprintf(2, "inherited eventfd write: errno=%d\n", errno);
      _exit(3);
    }
    _exit(0);
  }
  if (waitpid(child, &status, 0) != child) return 4;
  if (!WIFEXITED(status) || WEXITSTATUS(status)) return 5;
  if (read(fd, &got, sizeof(got)) != sizeof(got) || got != 1) return 6;
  return close(fd) ? 7 : 0;
}
