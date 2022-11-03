/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
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
#include <assert.h>
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/case.h"
#include "blink/endian.h"
#include "blink/loader.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/signal.h"
#include "blink/syscall.h"
#include "blink/xlat.h"

struct Machine *m;
struct Signals signals;

static void OnSignal(int sig, siginfo_t *si, void *uc) {
  EnqueueSignal(m, &signals, sig, si->si_code);
}

static void AddHostFd(struct Machine *m, int fd) {
  int i = m->fds.i++;
  m->fds.p[i].fd = dup(fd);
  assert(m->fds.p[i].fd != -1);
  m->fds.p[i].cb = &kMachineFdCbHost;
}

static int Exec(char *prog, char **argv, char **envp) {
  int rc;
  struct Elf elf;
  struct Machine *o;
  /* execve(prog, argv, envp); */
  o = m;
  m = NewMachine();
  m->exec = Exec;
  m->mode = XED_MACHINE_MODE_LONG_64;
  LoadProgram(m, prog, argv, envp, &elf);
  if (!o) {
    m->fds.n = 8;
    m->fds.p = calloc(m->fds.n, sizeof(struct MachineFd));
    AddHostFd(m, 0);
    AddHostFd(m, 1);
    AddHostFd(m, 2);
  } else {
    m->fds = o->fds;
    FreeMachine(o);
  }
  if (!(rc = setjmp(m->onhalt))) {
    for (;;) {
      LoadInstruction(m);
      ExecuteInstruction(m);
      if (signals.i < signals.n) {
        ConsumeSignal(m, &signals);
      }
    }
  } else {
    return rc;
  }
}

int main(int argc, char *argv[], char **envp) {
  struct sigaction sa;
  if (argc < 2) {
    fputs("Usage: ", stderr);
    fputs(argv[0], stderr);
    fputs(" PROG [ARGS...]\r\n", stderr);
    return 48;
  }
  memset(&sa, 0, sizeof(sa));
  sigfillset(&sa.sa_mask);
  sa.sa_flags |= SA_SIGINFO;
  sa.sa_sigaction = OnSignal;
  sigaction(SIGHUP, &sa, 0);
  sigaction(SIGINT, &sa, 0);
  sigaction(SIGQUIT, &sa, 0);
  sigaction(SIGABRT, &sa, 0);
  sigaction(SIGUSR1, &sa, 0);
  sigaction(SIGUSR2, &sa, 0);
  sigaction(SIGPIPE, &sa, 0);
  sigaction(SIGALRM, &sa, 0);
  sigaction(SIGTERM, &sa, 0);
  sigaction(SIGCHLD, &sa, 0);
  sigaction(SIGWINCH, &sa, 0);
  return Exec(argv[1], argv + 1, envp);
}
