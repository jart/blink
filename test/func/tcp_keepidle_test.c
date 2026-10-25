#define _GNU_SOURCE
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>

int main(void) {
  int fd, value = 60, actual = 0, enabled = 1;
  socklen_t size = sizeof(actual);
  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) return 1;
  if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &enabled, sizeof(enabled))) return 2;
  if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &value, sizeof(value))) return 3;
  if (getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &actual, &size) ||
      size != sizeof(actual) || actual != value) return 4;
  value = 30;
  if (setsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &value, sizeof(value))) return 5;
  if (getsockopt(fd, IPPROTO_TCP, TCP_KEEPIDLE, &actual, &size) || actual != value)
    return 6;
  return close(fd) ? 7 : 0;
}
