/* sigwait must work without installing a signal handler first. */
#define _GNU_SOURCE
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
  sigset_t set;
  siginfo_t info;
  struct timespec timeout = {2, 0};
  int status;
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  sigaddset(&set, SIGCHLD);
  if (sigprocmask(SIG_BLOCK, &set, 0)) return 1;
  pid_t child = fork();
  if (child < 0) return 2;
  if (!child) {
    usleep(10000);
    kill(getppid(), SIGUSR1);
    _exit(23);
  }
  if (sigtimedwait(&set, &info, &timeout) != SIGUSR1 ||
      info.si_code != SI_USER || info.si_pid != child) return 3;
  sigdelset(&set, SIGUSR1);
  if (sigtimedwait(&set, &info, &timeout) != SIGCHLD ||
      info.si_code != CLD_EXITED || info.si_status != 23) return 4;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) ||
      WEXITSTATUS(status) != 23) return 5;
  return 0;
}
