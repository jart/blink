// checks if sockatmark is available
#include <sys/socket.h>

int main(int argc, char *argv[]) {
  int rc;
  socket(AF_INET, SOCK_STREAM, 0);
  if (sockatmark(3) == -1) return 1;
  return 0;
}