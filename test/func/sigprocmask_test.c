#include <signal.h>

volatile int gotsigusr1;
volatile int gotsigusr2;

void OnSigUsr1(int sig) {
  ++gotsigusr1;
}

void OnSigUsr2(int sig) {
  ++gotsigusr2;
}

int main(int argc, char *argv[]) {
  sigset_t ss;
  if (signal(SIGUSR1, OnSigUsr1) == SIG_ERR) return 1;
  if (signal(SIGUSR2, OnSigUsr2) == SIG_ERR) return 2;
  if (sigemptyset(&ss)) return 3;
  if (sigaddset(&ss, SIGUSR1)) return 4;
  if (sigaddset(&ss, SIGUSR2)) return 5;
  if (sigprocmask(SIG_BLOCK, &ss, 0)) return 6;
  // tkill() enqueues masked signals
  if (raise(SIGUSR1)) return 7;
  if (raise(SIGUSR1)) return 8;
  if (raise(SIGUSR2)) return 9;
  if (raise(SIGUSR2)) return 10;
  if (gotsigusr1) return 11;
  if (gotsigusr2) return 12;
  // sigprocmask() triggers immediate delivery of deliverable signals
  if (sigprocmask(SIG_UNBLOCK, &ss, 0)) return 13;
  if (!gotsigusr1) return 14;
  if (!gotsigusr2) return 15;
  // a signal enqueued multiple times is only delivered once
  if (gotsigusr1 != 1) return 16;
  if (gotsigusr2 != 1) return 17;
  return 0;
}
