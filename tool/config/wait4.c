// checks if wait4() will break build
#include <errno.h>
#include <sys/resource.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
  int ws;
  pid_t rc;
  struct rusage ru;
  rc = wait4(-1, &ws, -1, &ru);
  if (rc != (pid_t)-1) return 1;
  if (errno != EINVAL) return 2;
  return 0;
}
