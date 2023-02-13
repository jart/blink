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
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/overlays.h"
#include "blink/pml4t.h"
#include "blink/signal.h"
#include "blink/sigwinch.h"
#include "blink/stats.h"
#include "blink/syscall.h"
#include "blink/thread.h"
#include "blink/util.h"
#include "blink/web.h"
#include "blink/x86.h"
#include "blink/xlat.h"

#define OPTS "hjemsS0L:C:"
#define USAGE \
  " [-" OPTS "] PROG [ARGS...]\n\
  -h                   help\n\
  -j                   disable jit\n\
  -e                   also log to stderr\n\
  -0                   to specify argv[0]\n\
  -m                   enable memory safety\n\
  -s                   print statistics on exit\n\
  -S                   enable system call logging\n\
  -L PATH              log filename (default ${TMPDIR:-/tmp}/blink.log)\n\
  -C PATH              sets chroot dir or overlay spec [default \":o\"]\n\
  $BLINK_OVERLAYS      file system roots [default \":o\"]\n\
  $BLINK_LOG_FILENAME  log filename (same as -L flag)\n"

extern char **environ;
static bool FLAG_zero;
static bool FLAG_nojit;
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
#ifdef HAVE_JIT
  ShutdownJit();
#endif
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

static int Exec(char *execfn, char *prog, char **argv, char **envp) {
  int i;
  sigset_t oldmask;
  struct Machine *old;
  if ((old = g_machine)) KillOtherThreads(old->system);
  unassert((g_machine = NewMachine(NewSystem(XED_MODE_LONG), 0)));
#ifdef HAVE_JIT
  if (FLAG_nojit) DisableJit(&g_machine->system->jit);
#endif
  g_machine->system->exec = Exec;
  if (!old) {
    // this is the first time a program is being loaded
    LoadProgram(g_machine, execfn, prog, argv, envp);
    SetupCod(g_machine);
    for (i = 0; i < 10; ++i) {
      AddStdFd(&g_machine->system->fds, i);
    }
  } else {
    unassert(!FreeVirtual(old->system, -0x800000000000, 0x1000000000000));
    for (i = 1; i <= 64; ++i) {
      if (Read64(old->system->hands[i - 1].handler) == SIG_IGN_LINUX) {
        Write64(g_machine->system->hands[i - 1].handler, SIG_IGN_LINUX);
      }
    }
    memcpy(g_machine->system->rlim, old->system->rlim,
           sizeof(old->system->rlim));
    LoadProgram(g_machine, execfn, prog, argv, envp);
    g_machine->system->fds.list = old->system->fds.list;
    old->system->fds.list = 0;
    // releasing the execve() lock must come after unlocking fds
    memcpy(&oldmask, &old->system->exec_sigmask, sizeof(oldmask));
    UNLOCK(&old->system->exec_lock);
    // freeing the last machine in a system will free its system too
    FreeMachine(old);
    // restore the signal mask we had before execve() was called
    unassert(!pthread_sigmask(SIG_SETMASK, &oldmask, 0));
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
#ifndef DISABLE_OVERLAYS
  FLAG_overlays = getenv("BLINK_OVERLAYS");
  if (!FLAG_overlays) FLAG_overlays = DEFAULT_OVERLAYS;
#endif
#if LOG_ENABLED
  FLAG_logpath = getenv("BLINK_LOG_FILENAME");
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
      case 'e':
        FLAG_alsologtostderr = true;
        break;
      case 'L':
        FLAG_logpath = optarg_;
        break;
      case 'C':
#ifndef DISABLE_OVERLAYS
        FLAG_overlays = optarg_;
#else
        WriteErrorString("error: overlays support was disabled\n");
#endif
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
#ifdef HAVE_THREADS
  sa.sa_handler = OnSigSys;
  unassert(!sigaction(SIGSYS, &sa, 0));
#endif
  sa.sa_sigaction = OnSignal;
  unassert(!sigaction(SIGINT, &sa, 0));
  unassert(!sigaction(SIGQUIT, &sa, 0));
  unassert(!sigaction(SIGHUP, &sa, 0));
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
  // Ensure utf-8 is printed correctly on windows, this method
  // has issues(http://stackoverflow.com/a/10884364/4279) but
  // should work for at least windows 7 and newer.
#if defined(_WIN32) && !defined(__CYGWIN__)
  SetConsoleOutputCP(CP_UTF8);
#endif
  g_blink_path = argc > 0 ? argv[0] : 0;
  WriteErrorInit();
  GetOpts(argc, argv);
  if (optind_ == argc) PrintUsage(argc, argv, 48, 2);
#ifndef DISABLE_OVERLAYS
  if (SetOverlays(FLAG_overlays, true)) {
    WriteErrorString("bad blink overlays spec; see log for details\n");
    exit(1);
  }
#endif
  HandleSigs();
  InitBus();
  if (!Commandv(argv[optind_], g_pathbuf, sizeof(g_pathbuf))) {
    WriteErrorString(argv[0]);
    WriteErrorString(": command not found: ");
    WriteErrorString(argv[optind_]);
    WriteErrorString("\n");
    exit(127);
  }
  argv[optind_] = g_pathbuf;
  return Exec(g_pathbuf, g_pathbuf, argv + optind_ + FLAG_zero, environ);
}
