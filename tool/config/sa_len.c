// checks for bsd style sockaddr length field
#include <sys/socket.h>

int main(int argc, char *argv[]) {
  struct sockaddr sa;
  sa.sa_len = 1;
  (void)sa;
  return 0;
}
