// checks if getdomainname() works
#include <string.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int rc;
  char buf[256];
  memset(buf, 0, 256);
  rc = getdomainname(buf, 256);
  if (rc == -1) return 1;           // library failed
  if (rc) return 2;                 // library broken
  if (strlen(buf) > 255) return 4;  // library broken
  return 0;
}
