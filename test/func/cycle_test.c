// checks obvious cycle doesn't prevent asynchronous signals
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

void OnSig(int sig) {
  _exit(0);
}

int main(int argc, char *argv[]) {
  ssize_t got;
  int ws, pid;
  if (!(pid = fork())) {
    struct sigaction sa = {.sa_handler = OnSig};
    sigaction(SIGUSR1, &sa, 0);
    for (;;) {
    }
  }
  sleep(1);
  kill(pid, SIGUSR1);
  wait(&ws);
  return WEXITSTATUS(ws);
}
