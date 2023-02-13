// checks if fork() works
#include <sys/wait.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  int ws;
  pid_t pid, rc;
  pid = fork();
  if (pid == -1) return 1;
  if (!pid) _exit(42);
  rc = wait(&ws);
  if (rc == -1) return 2;
  if (rc != pid) return 3;
  if (!WIFEXITED(ws)) return 4;
  if (WEXITSTATUS(ws) != 42) return 5;
  return 0;
}
