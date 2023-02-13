// checks if mmap(MAP_SHARED) works
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  FILE *f;
  void *map;
  int ws, *x;
  pid_t pid, rc;
  if (!(f = tmpfile())) return 1;
  if (ftruncate(fileno(f), sizeof(int))) return 2;
  map = mmap(0, sizeof(int), PROT_READ | PROT_WRITE, MAP_SHARED, fileno(f), 0);
  if (map == MAP_FAILED) return 3;
  x = (int *)map;
  pid = fork();
  if (pid == -1) return 4;
  if (!pid) {
    *x = 42;
    msync(map, sizeof(int), MS_INVALIDATE);
    _exit(0);
  }
  rc = wait(&ws);
  if (rc == -1) return 5;
  if (rc != pid) return 6;
  if (!WIFEXITED(ws)) return 7;
  if (WEXITSTATUS(ws)) return 8;
  if (*x != 42) return 9;
  return 0;
}
