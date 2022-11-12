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
#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/case.h"
#include "blink/endian.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/signal.h"
#include "blink/xlat.h"

struct Machine *m;
struct Signals signals;

static void OnSignal(int sig, siginfo_t *si, void *uc) {
  EnqueueSignal(m, &signals, sig, si->si_code);
}

static void AddHostFd(struct System *s, int fd) {
  int i = s->fds.i++;
  s->fds.p[i].fd = dup(fd);
  unassert(s->fds.p[i].fd != -1);
  s->fds.p[i].cb = &kMachineFdCbHost;
}

static int Exec(char *prog, char **argv, char **envp) {
  int rc;
  struct Elf elf;
  struct System *s;
  struct Machine *o = m;
  unassert((s = NewSystem()));
  unassert((m = NewMachine(s, 0)));
  m->system->exec = Exec;
  m->mode = XED_MACHINE_MODE_LONG_64;
  LoadProgram(m, prog, argv, envp, &elf);
  if (!o) {
    m->system->fds.n = 8;
    m->system->fds.p =
        (struct MachineFd *)calloc(m->system->fds.n, sizeof(struct MachineFd));
    AddHostFd(m->system, 0);
    AddHostFd(m->system, 1);
    AddHostFd(m->system, 2);
  } else {
    m->system->fds = o->system->fds;
    o->system->fds.p = 0;
    FreeMachine(o);
    FreeSystem(s);
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
  unassert(!sigaction(SIGHUP, &sa, 0));
  unassert(!sigaction(SIGINT, &sa, 0));
  unassert(!sigaction(SIGQUIT, &sa, 0));
  unassert(!sigaction(SIGABRT, &sa, 0));
  unassert(!sigaction(SIGUSR1, &sa, 0));
  unassert(!sigaction(SIGUSR2, &sa, 0));
  unassert(!sigaction(SIGPIPE, &sa, 0));
  unassert(!sigaction(SIGALRM, &sa, 0));
  unassert(!sigaction(SIGTERM, &sa, 0));
  unassert(!sigaction(SIGCHLD, &sa, 0));
  unassert(!sigaction(SIGWINCH, &sa, 0));
  return Exec(argv[1], argv + 1, envp);
}
