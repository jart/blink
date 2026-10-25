#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/uio.h>
#include <unistd.h>

int main(void) {
  int fd, ep, sem;
  uint64_t value = 5, result = 0;
  struct epoll_event event = {.events = EPOLLIN, .data.u64 = 42}, got;
  struct iovec parts[2] = {{&value, 3}, {(char *)&value + 3, 5}};
  struct iovec dest[2] = {{&result, 5}, {(char *)&result + 5, 3}};
  if ((fd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) == -1) return 1;
  if (!(fcntl(fd, F_GETFD) & FD_CLOEXEC) || !(fcntl(fd, F_GETFL) & O_NONBLOCK)) return 2;
  if (read(fd, &result, 8) != -1 || errno != EAGAIN) return 3;
  if (read(fd, &result, 0) != -1 || errno != EINVAL) return 4;
  if (write(fd, &value, 7) != -1 || errno != EINVAL) return 5;
  if ((ep = epoll_create1(EPOLL_CLOEXEC)) == -1) return 6;
  if (epoll_ctl(ep, EPOLL_CTL_ADD, fd, &event)) return 7;
  if (writev(fd, parts, 2) != 8) return 8;
  if (epoll_wait(ep, &got, 1, 1000) != 1 || got.data.u64 != 42) return 9;
  if (readv(fd, dest, 2) != 8 || result != value) return 10;
  if (epoll_wait(ep, &got, 1, 0)) return 11;
  close(ep);
  close(fd);
  if ((sem = eventfd(2, EFD_SEMAPHORE | EFD_NONBLOCK)) == -1) return 12;
  if (read(sem, &result, 8) != 8 || result != 1) return 13;
  if (read(sem, &result, 8) != 8 || result != 1) return 14;
  if (read(sem, &result, 8) != -1 || errno != EAGAIN) return 15;
  close(sem);
  return 0;
}
