#include <sys/epoll.h>

int main(int argc, char *argv[]) {
  struct epoll_event ev;
  epoll_pwait2(-1, &ev, 0, 0, 0);
  return 0;
}
