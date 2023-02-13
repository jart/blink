// checks for pipe2() system call
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int fds[2];
  if (pipe2(fds, 0)) return 1;
  if (fcntl(fds[0], F_GETFD) != 0) return 2;
  if (fcntl(fds[1], F_GETFD) != 0) return 3;
  if (pipe2(fds, O_CLOEXEC)) return 4;
  if (fcntl(fds[0], F_GETFD) != FD_CLOEXEC) return 5;
  if (fcntl(fds[1], F_GETFD) != FD_CLOEXEC) return 6;
  return 0;
}
