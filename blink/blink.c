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
#include "blink/dll.h"
#include "blink/endian.h"
#include "blink/jit.h"
#include "blink/loader.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/signal.h"
#include "blink/sigwinch.h"
#include "blink/stats.h"
#include "blink/syscall.h"
#include "blink/xlat.h"

static void OnSignal(int sig, siginfo_t *si, void *uc) {
  EnqueueSignal(g_machine, sig);
}

static int Exec(char *prog, char **argv, char **envp) {
  int rc;
  struct System *s;
  struct Machine *old;
  if ((old = g_machine)) KillOtherThreads(old->system);
  unassert((g_machine = NewMachine(NewSystem(), 0)));
  // DisableJit(&g_machine->system->jit);
  g_machine->system->exec = Exec;
  g_machine->mode = XED_MODE_LONG;
  if (!old) {
    LoadProgram(g_machine, prog, argv, envp);
    AddStdFd(&g_machine->system->fds, 0);
    AddStdFd(&g_machine->system->fds, 1);
    AddStdFd(&g_machine->system->fds, 2);
  } else {
    LoadProgram(g_machine, prog, argv, envp);
    LOCK(&old->system->fds.lock);
    g_machine->system->fds.list = old->system->fds.list;
    old->system->fds.list = 0;
    UNLOCK(&old->system->fds.lock);
    // FreeMachine(old);
    // FreeSystem(old->system);
  }
  if (!(rc = setjmp(g_machine->onhalt))) {
    Actor(g_machine);
  } else {
    s = g_machine->system;
    KillOtherThreads(s);
    FreeMachine(g_machine);
    FreeSystem(s);
    // PrintStats();
    return rc;
  }
}

int main(int argc, char *argv[], char **envp) {
  struct sigaction sa;
  WriteErrorInit();
  if (argc < 2) {
    WriteErrorString("Usage: ");
    WriteErrorString(argv[0]);
    WriteErrorString(" PROG [ARGS...]\n");
    return 48;
  }
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_sigaction = OnSignal;
  unassert(!sigaction(SIGHUP, &sa, 0));
  unassert(!sigaction(SIGINT, &sa, 0));
  unassert(!sigaction(SIGQUIT, &sa, 0));
  unassert(!sigaction(SIGUSR1, &sa, 0));
  unassert(!sigaction(SIGUSR2, &sa, 0));
  unassert(!sigaction(SIGPIPE, &sa, 0));
  unassert(!sigaction(SIGALRM, &sa, 0));
  unassert(!sigaction(SIGTERM, &sa, 0));
  unassert(!sigaction(SIGWINCH, &sa, 0));
  return Exec(argv[1], argv + 1, envp);
}
