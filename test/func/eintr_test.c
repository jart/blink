/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

// signal delivery causes blocking i/o calls to return EINTR
// see also norestart_test.c
// see also restart_test.c

int fds[2];
volatile int gotsig;

void OnSig(int sig, siginfo_t *si, void *ptr) {
  gotsig = 1;
}

int main(int argc, char *argv[]) {
  char b;
  ssize_t got;
  int ws, pid;
  sigaction(SIGUSR1, &(struct sigaction){.sa_sigaction = OnSig}, 0);
  pipe(fds);
  if (!(pid = fork())) {
    close(fds[1]);
    got = read(fds[0], &b, 1);
    if (got == -1 && errno == EINTR && gotsig) {
      _exit(0);
    } else {
      _exit(1);
    }
  }
  close(fds[0]);
  sleep(1);
  kill(pid, SIGUSR1);
  wait(&ws);
  return WEXITSTATUS(ws);
}
