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
#include "blink/bus.h"
#include "blink/case.h"
#include "blink/debug.h"
#include "blink/dll.h"
#include "blink/endian.h"
#include "blink/flag.h"
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
#include "blink/x86.h"
#include "blink/xlat.h"

#define OPTS "hjmsS0L:"
#define USAGE \
  " [-" OPTS "] PROG [ARGS...]\n\
  -h                   help\n\
  -j                   disable jit\n\
  -0                   to specify argv[0]\n\
  -m                   enable memory safety\n\
  -s                   print statistics on exit\n\
  -S                   enable system call logging\n\
  -L PATH              log filename (default ${TMPDIR:-/tmp}/blink.log)\n\
  $BLINK_LOG_FILENAME  log filename (same as -L flag)\n"

extern char **environ;
static bool FLAG_zero;
static bool FLAG_nojit;
static bool FLAG_nolinear;
static char g_pathbuf[PATH_MAX];

static void OnSigSys(int sig) {
  // do nothing
}

void TerminateSignal(struct Machine *m, int sig) {
  int syssig;
  struct sigaction sa;
  unassert(!IsSignalIgnoredByDefault(sig));
  ERRF("terminating due to signal %s", DescribeSignal(sig));
  if ((syssig = XlatSignal(sig)) == -1) syssig = SIGTERM;
  KillOtherThreads(m->system);
  FreeMachine(m);
  ShutdownJit();
  sa.sa_flags = 0;
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  if (syssig != SIGKILL && syssig != SIGSTOP) {
    unassert(!sigaction(syssig, &sa, 0));
  }
  unassert(!kill(getpid(), syssig));
  Abort();
}

static void OnSigSegv(int sig, siginfo_t *si, void *ptr) {
  int sig_linux;
  RestoreIp(g_machine);
  // TODO: Fix memory leak with FormatPml4t()
  // TODO(jart): Fix address translation in non-linear mode.
  g_machine->faultaddr = ToGuest(si->si_addr);
  ERRF("SEGMENTATION FAULT (%s) AT ADDRESS %" PRIx64 "\n\t%s\n%s",
       strsignal(sig), g_machine->faultaddr, GetBacktrace(g_machine),
       FormatPml4t(g_machine));
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
  unassert((g_machine = NewMachine(NewSystem(XED_MODE_LONG), 0)));
  if (FLAG_nojit) DisableJit(&g_machine->system->jit);
  g_machine->system->exec = Exec;
  g_machine->nolinear = FLAG_nolinear;
  g_machine->system->nolinear = FLAG_nolinear;
  if (!old) {
    // this is the first time a program is being loaded
    LoadProgram(g_machine, prog, argv, envp);
    SetupCod(g_machine);
    for (i = 0; i < 10; ++i) {
      AddStdFd(&g_machine->system->fds, i);
    }
  } else {
    // execve() was called and emulation has been requested.
    // we don't currently wipe out all the mappings that the last
    // program made so we need to disable the fixed map safety check
    g_wasteland = true;
    LoadProgram(g_machine, prog, argv, envp);
    // locks are only superficially required since we killed everything
    LOCK(&old->system->fds.lock);
    g_machine->system->fds.list = old->system->fds.list;
    old->system->fds.list = 0;
    UNLOCK(&old->system->fds.lock);
    // releasing the execve() lock must come after unlocking fds
    UNLOCK(&old->system->exec_lock);
    // freeing the last machine in a system will free its system too
    FreeMachine(old);
  }
  // meta interpreter loop
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
  (void)write(fd, s, strlen(s));
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
#if LOG_ENABLED
  FLAG_logpath = getenv("BLINK_LOG_FILENAME");
#endif
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
      case 'S':
        FLAG_strace = true;
        break;
      case 'm':
        FLAG_nolinear = true;
        break;
      case 's':
        FLAG_statistics = true;
        break;
      case 'L':
        FLAG_logpath = optarg_;
        break;
      case 'h':
        PrintUsage(argc, argv, 0, 1);
      default:
        PrintUsage(argc, argv, 48, 2);
    }
  }
#if LOG_ENABLED
  LogInit(FLAG_logpath);
#endif
}

static void HandleSigs(void) {
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = OnSigSys;
  unassert(!sigaction(SIGSYS, &sa, 0));
  sa.sa_sigaction = OnSignal;
  unassert(!sigaction(SIGHUP, &sa, 0));
  unassert(!sigaction(SIGINT, &sa, 0));
  unassert(!sigaction(SIGQUIT, &sa, 0));
  unassert(!sigaction(SIGPIPE, &sa, 0));
  unassert(!sigaction(SIGTERM, &sa, 0));
  unassert(!sigaction(SIGXCPU, &sa, 0));
  unassert(!sigaction(SIGXFSZ, &sa, 0));
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
  // main is called multiple times - don't perform cleanup
  _exit(status);
}
#endif

int main(int argc, char *argv[]) {
  SetupWeb();
  g_blink_path = argc > 0 ? argv[0] : 0;
  GetOpts(argc, argv);
  if (optind_ == argc) PrintUsage(argc, argv, 48, 2);
  WriteErrorInit();
  HandleSigs();
  InitBus();
  if (!Commandv(argv[optind_], g_pathbuf, sizeof(g_pathbuf))) {
    WriteErrorString(argv[0]);
    WriteErrorString(": command not found: ");
    WriteErrorString(argv[optind_]);
    WriteErrorString("\n");
    exit(127);
  }
  return Exec(g_pathbuf, argv + optind_ + FLAG_zero, environ);
}
