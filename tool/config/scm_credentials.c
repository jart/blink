// checks that SCM_CREDENTIALS won't break the build
#include <sys/socket.h>
#include <sys/un.h>

int main(int argc, char *argv[]) {
  struct ucred ucred;
  ucred.pid = SCM_CREDENTIALS;
  ucred.uid = SCM_CREDENTIALS;
  ucred.gid = SCM_CREDENTIALS;
  (void)ucred;
  return 0;
}
