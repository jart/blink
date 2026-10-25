#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static double Now(void) {
  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  return t.tv_sec + t.tv_nsec * 1e-9;
}

int main(void) {
  int pair[2], idle[2], i, status;
  char byte;
  fd_set readfds;
  struct timeval timeout;
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair) || pipe(idle)) return 1;
  pid_t pid = fork();
  if (pid < 0) return 2;
  if (!pid) {
    close(pair[0]);
    for (i = 0; i < 40; ++i) {
      if (read(pair[1], &byte, 1) != 1) _exit(3);
      usleep(1000);
      if (write(pair[1], &byte, 1) != 1) _exit(4);
    }
    _exit(0);
  }
  close(pair[1]);
  double start = Now();
  for (i = 0; i < 40; ++i) {
    if (write(pair[0], "x", 1) != 1) return 5;
    FD_ZERO(&readfds);
    FD_SET(pair[0], &readfds);
    FD_SET(idle[0], &readfds);
    timeout = (struct timeval){2, 0};
    if (select((pair[0] > idle[0] ? pair[0] : idle[0]) + 1,
               &readfds, 0, 0, &timeout) != 1 ||
        !FD_ISSET(pair[0], &readfds) || FD_ISSET(idle[0], &readfds)) return 6;
    if (read(pair[0], &byte, 1) != 1 || byte != 'x') return 7;
  }
  double elapsed = Now() - start;
  if (waitpid(pid, &status, 0) != pid || status) return 8;
  // EOF is readable even when the native backend reports only POLLHUP.
  FD_ZERO(&readfds);
  FD_SET(pair[0], &readfds);
  timeout = (struct timeval){0, 0};
  if (select(pair[0] + 1, &readfds, 0, 0, &timeout) != 1 ||
      read(pair[0], &byte, 1) != 0) return 9;
  printf("40 request/reply readiness waits: %.3f seconds\n", elapsed);
  // Opt-in performance regression; allow generous scheduling headroom.
  return elapsed < 1.0 ? 0 : 10;
}
