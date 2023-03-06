#include <sys/epoll.h>

int main(int argc, char *argv[]) {
  epoll_pwait2(-1, 0, 0, 0, 0);
  return 0;
}
