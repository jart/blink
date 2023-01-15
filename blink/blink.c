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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/case.h"
#include "blink/debug.h"
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
#include "blink/util.h"
#include "blink/web.h"
#include "blink/xlat.h"

#define OPTS "hjms0"
#define USAGE \
  " [-" OPTS "] PROG [ARGS...]\n\
  -h        help\n\
  -j        disable jit\n\
  -0        to specify argv[0]\n\
  -m        enable memory safety\n\
  -s        print statistics on exit\n"

extern char **environ;
static bool FLAG_zero;
static bool FLAG_nojit;
static bool FLAG_nolinear;

static void OnSigSys(int sig) {
  // do nothing
}

void TerminateSignal(struct Machine *m, int sig) {
  int syssig;
  struct sigaction sa;
  LOGF("terminating due to signal %s", DescribeSignal(sig));
  if ((syssig = XlatSignal(sig)) == -1) syssig = SIGTERM;
  LOCK(&m->system->sig_lock);
  sa.sa_flags = 0;
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  unassert(!sigaction(syssig, &sa, 0));
  unassert(!kill(getpid(), syssig));
  Abort();
}

static void OnSigSegv(int sig, siginfo_t *si, void *ptr) {
  int sig_linux;
  RestoreIp(g_machine);
  g_machine->faultaddr = ToGuest(si->si_addr);
  LOGF("SEGMENTATION FAULT (%s) AT ADDRESS %" PRIx64 "\n\t%s", strsignal(sig),
       g_machine->faultaddr, GetBacktrace(g_machine));
#ifdef DEBUG
  PrintBacktrace();
#endif
  if ((sig_linux = UnXlatSignal(sig)) != -1) {
    DeliverSignalToUser(g_machine, sig_linux);
  }
  siglongjmp(g_machine->onhalt, kMachineSegmentationFault);
}

static int Exec(char *prog, char **argv, char **envp) {
  int i;
  struct Machine *old;
  if ((old = g_machine)) KillOtherThreads(old->system);
  unassert((g_machine = NewMachine(NewSystem(), 0)));
  if (FLAG_nojit) DisableJit(&g_machine->system->jit);
  SetMachineMode(g_machine, XED_MODE_LONG);
  g_machine->system->brand = "GenuineBlink";
  g_machine->system->exec = Exec;
  g_machine->nolinear = FLAG_nolinear;
  g_machine->system->nolinear = FLAG_nolinear;
  if (!old) {
    LoadProgram(g_machine, prog, argv, envp);
    SetupCod(g_machine);
    for (i = 0; i < 10; ++i) {
      AddStdFd(&g_machine->system->fds, i);
    }
  } else {
    LoadProgram(g_machine, prog, argv, envp);
    LOCK(&old->system->fds.lock);
    g_machine->system->fds.list = old->system->fds.list;
    old->system->fds.list = 0;
    UNLOCK(&old->system->fds.lock);
    FreeMachine(old);
  }
  for (;;) {
    if (!sigsetjmp(g_machine->onhalt, 1)) {
      g_machine->canhalt = true;
      Actor(g_machine);
    }
    if (IsMakingPath(g_machine)) {
      AbandonPath(g_machine);
    }
  }
}

static void Print(int fd, const char *s) {
  write(fd, s, strlen(s));
}

_Noreturn static void PrintUsage(int argc, char *argv[], int rc, int fd) {
  Print(fd, "Usage: ");
  Print(fd, argc > 0 && argv[0] ? argv[0] : "blink");
  Print(fd, USAGE);
  exit(rc);
}

static void GetOpts(int argc, char *argv[]) {
  int opt;
  FLAG_nolinear = !CanHaveLinearMemory();
#ifdef __CYGWIN__
  FLAG_nojit = true;
  FLAG_nolinear = true;
#endif
#ifdef __COSMOPOLITAN__
  if (IsWindows()) {
    FLAG_nojit = true;
    FLAG_nolinear = true;
  }
#endif
  while ((opt = GetOpt(argc, argv, OPTS)) != -1) {
    switch (opt) {
      case '0':
        FLAG_zero = true;
        break;
      case 'j':
        FLAG_nojit = true;
        break;
      case 'm':
        FLAG_nolinear = true;
        break;
      case 's':
        FLAG_statistics = true;
        break;
      case 'h':
        PrintUsage(argc, argv, 0, 1);
      default:
        PrintUsage(argc, argv, 48, 2);
    }
  }
}

static void HandleSigs(void) {
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = OnSigSys;
  unassert(!sigaction(SIGSYS, &sa, 0));
#if !defined(__SANITIZE_THREAD__) && !defined(__SANITIZE_ADDRESS__)
  sa.sa_sigaction = OnSigSegv;
  sa.sa_flags = SA_SIGINFO;
  unassert(!sigaction(SIGBUS, &sa, 0));
  unassert(!sigaction(SIGILL, &sa, 0));
  unassert(!sigaction(SIGTRAP, &sa, 0));
  unassert(!sigaction(SIGSEGV, &sa, 0));
#endif
}

#if defined(__EMSCRIPTEN__)
void exit(int status) {
  EM_ASM({ throw new ExitStatus(status); });
}
#endif

int main(int argc, char *argv[]) {
  SetupWeb();
  g_blink_path = argc > 0 ? argv[0] : 0;
  GetOpts(argc, argv);
  if (optind_ == argc) PrintUsage(argc, argv, 48, 2);
  WriteErrorInit();
  HandleSigs();
  return Exec(argv[optind_], argv + optind_ + FLAG_zero, environ);
}
