// Checks for RHEL6+ level epoll() support.
#include <sys/epoll.h>

int main(int argc, char *argv[]) {
  epoll_create(-1);
  epoll_create1(-1);
  epoll_pwait(-1, 0, 0, 0, 0);
  return 0;
}
