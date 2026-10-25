/* An epoll_pwait handler must allow another thread to wake sigsuspend.
   Requires the epoll backend; run with an external timeout. */
#include <errno.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
static volatile sig_atomic_t seen;
static pthread_t main_thread;
static void second(int sig) { seen = 1; }
static void first(int sig) {
  sigset_t mask;
  sigemptyset(&mask);
  if (sigsuspend(&mask) != -1 || errno != EINTR || !seen) _exit(6);
}
static void *sender(void *arg) {
  usleep(100000);
  if (pthread_kill(main_thread, SIGUSR2)) _exit(7);
  return 0;
}
int main(void) {
  sigset_t mask, old;
  struct sigaction sa = {0};
  pthread_t thread;
  struct epoll_event event;
  int ep = epoll_create1(0);
  if (ep == -1) return 9;
  main_thread = pthread_self();
  sigemptyset(&sa.sa_mask);
  sa.sa_handler = first;
  if (sigaction(SIGUSR1, &sa, 0)) return 1;
  sa.sa_handler = second;
  if (sigaction(SIGUSR2, &sa, 0)) return 2;
  sigemptyset(&mask);
  sigaddset(&mask, SIGUSR1);
  if (sigprocmask(SIG_BLOCK, &mask, &old) || raise(SIGUSR1)) return 3;
  if (pthread_create(&thread, 0, sender, 0)) return 4;
  if (epoll_pwait(ep, &event, 1, 5000, &old) != -1 || errno != EINTR || !seen) return 5;
  close(ep);
  return pthread_join(thread, 0) ? 8 : 0;
}
