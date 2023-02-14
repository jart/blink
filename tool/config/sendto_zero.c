// checks if sendto(0.0.0.0) will copy address from connect()
#include <netinet/in.h>
#include <netinet/udp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  pid_t pid;
  int rc, ws;
  socklen_t socklen;
  char buf[8] = {0};
  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
  };
  socklen = sizeof(addr);
  if (socket(AF_INET, SOCK_DGRAM, 0) != 3) return 1;
  if (bind(3, (struct sockaddr *)&addr, sizeof(addr))) return 2;
  if (getsockname(3, (struct sockaddr *)&addr, &socklen)) return 3;
  if ((pid = fork()) == -1) return 4;
  if (!pid) {
    alarm(5);
    if (read(3, buf, 8) != 2) _exit(5);
    if (strcmp(buf, "hi")) _exit(6);
    _exit(0);
  }
  if (close(3)) return 7;
  if (socket(AF_INET, SOCK_DGRAM, 0) != 3) return 8;
  if (connect(3, (struct sockaddr *)&addr, sizeof(addr))) return 9;
  addr.sin_addr.s_addr = 0;
  if (sendto(3, "hi", 2, 0, (struct sockaddr *)&addr, sizeof(addr)) != 2) {
    // OpenBSD 7.2 fails with EISCONN here.
    perror("sendto");
    return 10;
  }
  if (wait(&ws) != pid) return 11;
  return WEXITSTATUS(ws);
}
