// Checks for RHEL6+ level epoll() support.
#include <sys/epoll.h>

int main(int argc, char *argv[]) {
  struct epoll_event ev;
  epoll_create(-1);
  epoll_create1(-1);
  epoll_pwait(-1, &ev, 0, 0, 0);
  return 0;
}
