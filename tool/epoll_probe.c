#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>

/* Opt-in prerequisite probe for libuv/Node on hosts without native epoll. */
int main(void) {
  int ep, pair[2];
  char c;
  struct epoll_event want = {.events = EPOLLIN}, got;
  want.data.u64 = UINT64_C(0x123456789abcdef0);
  if ((ep = epoll_create1(EPOLL_CLOEXEC)) == -1) {
    perror("epoll_create1");
    return 1;
  }
  if (!(fcntl(ep, F_GETFD) & FD_CLOEXEC)) return 2;
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pair)) return 3;
  if (epoll_ctl(ep, EPOLL_CTL_ADD, pair[0], &want)) return 4;
  if (epoll_wait(ep, &got, 1, 0)) return 5;
  if (write(pair[1], "x", 1) != 1) return 6;
  if (epoll_wait(ep, &got, 1, 1000) != 1 || !(got.events & EPOLLIN) ||
      got.data.u64 != want.data.u64) return 7;
  if (read(pair[0], &c, 1) != 1 || c != 'x') return 8;
  if (epoll_ctl(ep, EPOLL_CTL_DEL, pair[0], 0)) return 9;
  if (epoll_wait(ep, &got, 1, 0)) return 10;
  close(pair[0]);
  close(pair[1]);
  close(ep);
  return 0;
}
