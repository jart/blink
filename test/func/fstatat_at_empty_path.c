#define _GNU_SOURCE
#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  struct stat st;
  char path[] = "/tmp/blink.test.XXXXXX";
  if (mkstemp(path) != 3) return 1;
  if (unlink(path)) return 2;
  if (write(3, "hi", 2) != 2) return 3;
  if (fstatat(3, "", &st, AT_EMPTY_PATH)) return 4;
  if (st.st_size != 2) return 5;
  return 0;
}
