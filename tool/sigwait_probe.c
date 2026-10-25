/* Probe the sigwait subset: pending delivery, waits, timeouts, and validation. */
#define _GNU_SOURCE
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static pthread_t main_thread;
static volatile sig_atomic_t interrupted;
static void handler(int sig) { if (sig == SIGUSR2) interrupted = 1; }
static void *sender(void *arg) {
  usleep(30000);
  pthread_kill(main_thread, (int)(intptr_t)arg);
  return 0;
}
int main(void) {
  int sig;
  pthread_t thread;
  sigset_t set, old, actual;
  siginfo_t info;
  uint64_t raw = (uint64_t)1 << (SIGUSR1 - 1);
  struct timespec zero = {0}, short_wait = {0, 20000000};
  struct timespec invalid = {0, 1000000000};
  signal(SIGUSR1, handler);
  sigemptyset(&set);
  sigaddset(&set, SIGUSR1);
  if (pthread_sigmask(SIG_BLOCK, &set, &old)) return 1;
  errno = 0;
  if (syscall(SYS_rt_sigtimedwait, &raw, 0, &zero, 8) != -1 || errno != EAGAIN) return 2;
  if (raise(SIGUSR1) || sigwait(&set, &sig) || sig != SIGUSR1) return 3;
  errno = 0;
  if (syscall(SYS_rt_sigtimedwait, &raw, 0, &short_wait, 8) != -1 || errno != EAGAIN) return 4;
  main_thread = pthread_self();
  if (pthread_create(&thread, 0, sender, (void *)(intptr_t)SIGUSR1)) return 5;
  if (sigwait(&set, &sig) || sig != SIGUSR1 || pthread_join(thread, 0)) return 6;
  if (pthread_sigmask(SIG_SETMASK, 0, &actual) || !sigismember(&actual, SIGUSR1)) return 7;
  errno = 0;
  if (syscall(SYS_rt_sigtimedwait, &raw, 0, &invalid, 8) != -1 || errno != EINVAL) return 8;
  errno = 0;
  if (syscall(SYS_rt_sigtimedwait, &raw, 0, &zero, 7) != -1 || errno != EINVAL) return 9;
  errno = 0;
  if (syscall(SYS_rt_sigtimedwait, 1, 0, &zero, 8) != -1 || errno != EFAULT) return 10;
  if (kill(getpid(), SIGUSR1)) return 12;
  if (sigtimedwait(&set, &info, &zero) != SIGUSR1 || info.si_signo != SIGUSR1 ||
      info.si_code != SI_USER || info.si_pid != getpid() || info.si_uid != getuid()) return 13;
  signal(SIGUSR2, handler);
  if (pthread_create(&thread, 0, sender, (void *)(intptr_t)SIGUSR2)) return 14;
  errno = 0;
  /* This cross toolchain's musl sigwaitinfo wrapper retries EINTR internally. */
  if (syscall(SYS_rt_sigtimedwait, &raw, &info, 0, 8) != -1 ||
      errno != EINTR || !interrupted) return 15;
  if (pthread_join(thread, 0)) return 16;
  signal(SIGCHLD, handler);
  sigemptyset(&set);
  sigaddset(&set, SIGCHLD);
  if (pthread_sigmask(SIG_BLOCK, &set, 0)) return 17;
  pid_t child = fork();
  if (child == -1) return 18;
  if (!child) _exit(23);
  if (sigwaitinfo(&set, &info) != SIGCHLD || info.si_pid != child ||
      info.si_code != CLD_EXITED || info.si_status != 23) return 19;
  int status;
  if (waitpid(child, &status, 0) != child || !WIFEXITED(status) || WEXITSTATUS(status) != 23) return 20;
  return pthread_sigmask(SIG_SETMASK, &old, 0) ? 11 : 0;
}
