#include <errno.h>
#include <sys/socket.h>

int main(void) {
  if (socket(-1, SOCK_STREAM, 0) != -1 || errno != EAFNOSUPPORT) return 1;
  return 0;
}
