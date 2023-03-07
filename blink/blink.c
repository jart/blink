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
#include <locale.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
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
#include "blink/map.h"
#include "blink/overlays.h"
#include "blink/pml4t.h"
#include "blink/signal.h"
#include "blink/sigwinch.h"
#include "blink/stats.h"
#include "blink/syscall.h"
#include "blink/thread.h"
#include "blink/tunables.h"
#include "blink/util.h"
#include "blink/web.h"
#include "blink/x86.h"
#include "blink/xlat.h"

#ifndef BUILD_TIMESTAMP
#define BUILD_TIMESTAMP __TIMESTAMP__
#endif
#ifndef BUILD_MODE
#define BUILD_MODE "BUILD_MODE_UNKNOWN"
#warning "-DBUILD_MODE=... should be passed to blink/blink.c"
#endif
#ifndef BUILD_TOOLCHAIN
#define BUILD_TOOLCHAIN "BUILD_TOOLCHAIN_UNKNOWN"
#warning "-DBUILD_TOOLCHAIN=... should be passed to blink/blink.c"
#endif
#ifndef BLINK_VERSION
#define BLINK_VERSION "BLINK_VERSION_UNKNOWN"
#warning "-DBLINK_VERSION=... should be passed to blink/blink.c"
#endif
#ifndef BLINK_COMMITS
#define BLINK_COMMITS "BLINK_COMMITS_UNKNOWN"
#warning "-DBLINK_COMMITS=... should be passed to blink/blink.c"
#endif
#ifndef BLINK_GITSHA
#define BLINK_GITSHA "BLINK_GITSHA_UNKNOWN"
#warning "-DBLINK_GITSHA=... should be passed to blink/blink.c"
#endif
#ifndef CONFIG_ARGUMENTS
#define CONFIG_ARGUMENTS "CONFIG_ARGUMENTS_UNKNOWN"
#warning "-DCONFIG_ARGUMENTS=... should be passed to blink/blink.c"
#endif

#define VERSION \
  "Blink Virtual Machine " BLINK_VERSION " (" BUILD_TIMESTAMP ")\n\
Copyright (c) 2023 Justine Alexandra Roberts Tunney\n\
Blink comes with absolutely NO WARRANTY of any kind.\n\
You may redistribute copies of Blink under the ISC License.\n\
For more information, see the file named LICENSE.\n\
Toolchain: " BUILD_TOOLCHAIN "\n\
Revision: #" BLINK_COMMITS " " BLINK_GITSHA "\n\
Config: ./configure MODE=" BUILD_MODE " " CONFIG_ARGUMENTS "\n"

#define OPTS "hvjemZs0L:C:"

_Alignas(1) static const char USAGE[] =
    " [-" OPTS "] PROG [ARGS...]\n"
    "Options:\n"
    "  -h                   help\n"
#ifndef DISABLE_JIT
    "  -j                   disable jit\n"
#endif
    "  -v                   show version\n"
#ifndef NDEBUG
    "  -e                   also log to stderr\n"
#endif
    "  -0                   to specify argv[0]\n"
    "  -m                   enable memory safety\n"
#if !defined(DISABLE_STRACE) && !defined(TINY)
    "  -s                   enable system call logging\n"
#endif
#ifndef NDEBUG
    "  -Z                   print internal statistics on exit\n"
    "  -L PATH              log filename (default is blink.log)\n"
#endif
#ifndef DISABLE_OVERLAYS
    "  -C PATH              sets chroot dir or overlay spec [default \":o\"]\n"
#endif
#if !defined(DISABLE_OVERLAYS) || !defined(NDEBUG)
    "Environment:\n"
#endif
#ifndef DISABLE_OVERLAYS
    "  $BLINK_OVERLAYS      file system roots [default \":o\"]\n"
#endif
#ifndef NDEBUG
    "  $BLINK_LOG_FILENAME  log filename (same as -L flag)\n"
#endif
    ;

extern char **environ;
static bool FLAG_nojit;
static char g_pathbuf[PATH_MAX];

static void OnSigSys(int sig) {
  // do nothing
}

static void PrintDiagnostics(struct Machine *m) {
  ERRF("additional information\n"
       "\t%s\n"
       "%s\n"
       "%s",
       GetBacktrace(m), FormatPml4t(m), GetBlinkBacktrace());
}

void TerminateSignal(struct Machine *m, int sig) {
  int syssig;
  struct sigaction sa;
  unassert(!IsSignalIgnoredByDefault(sig));
  if (IsSignalSerious(sig)) {
    ERRF("terminating due to %s (rip=%" PRIx64 " faultaddr=%" PRIx64 ")",
         DescribeSignal(sig), m->ip, m->faultaddr);
    PrintDiagnostics(m);
  }
  if ((syssig = XlatSignal(sig)) == -1) syssig = SIGKILL;
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

static void OnFatalSystemSignal(int sig, siginfo_t *si, void *ptr) {
  struct Machine *m = g_machine;
#ifdef __APPLE__
  // The fruit platform will raise SIGBUS on writes to anon mappings
  // which don't have the PROT_WRITE protection, even when aligned!!
  // We only create this kind of memory for getting notifications if
  // RWX memory is modified.
  if (HasLinearMapping() && sig == SIGBUS && si->si_code == BUS_ADRALN &&
      s->protectedsmc) {
    sig = SIGSEGV;
    si->si_signo = SIGSEGV;
    si->si_code = SEGV_ACCERR;
  }
#endif
  SIG_LOGF("OnFatalSystemSignal(%s, %p)", DescribeSignal(UnXlatSignal(sig)),
           si->si_addr);
#ifndef DISABLE_JIT
  if (IsSelfModifyingCodeSegfault(m, si)) return;
#endif
  g_siginfo = *si;
  unassert(m);
  unassert(m->canhalt);
  siglongjmp(m->onhalt, kMachineFatalSystemSignal);
}

static void ProgramLimit(struct System *s, int hresource, int gresource) {
  struct rlimit rlim;
  if (!getrlimit(hresource, &rlim)) {
    XlatRlimitToLinux(s->rlim + gresource, &rlim);
  }
}

static int Exec(char *execfn, char *prog, char **argv, char **envp) {
  int i;
  sigset_t oldmask;
  struct Machine *m, *old;
  if ((old = g_machine)) KillOtherThreads(old->system);
  unassert((g_machine = m = NewMachine(NewSystem(XED_MODE_LONG), 0)));
#ifdef HAVE_JIT
  if (FLAG_nojit) DisableJit(&m->system->jit);
#endif
  m->system->exec = Exec;
  if (!old) {
    // this is the first time a program is being loaded
    LoadProgram(m, execfn, prog, argv, envp);
    SetupCod(m);
    for (i = 0; i < 10; ++i) {
      AddStdFd(&m->system->fds, i);
    }
    ProgramLimit(m->system, RLIMIT_NOFILE, RLIMIT_NOFILE_LINUX);
  } else {
    unassert(!m->sysdepth);
    unassert(!m->pagelocks.i);
    unassert(!FreeVirtual(old->system, -0x800000000000, 0x1000000000000));
    for (i = 1; i <= 64; ++i) {
      if (Read64(old->system->hands[i - 1].handler) == SIG_IGN_LINUX) {
        Write64(m->system->hands[i - 1].handler, SIG_IGN_LINUX);
      }
    }
    memcpy(m->system->rlim, old->system->rlim, sizeof(old->system->rlim));
    LoadProgram(m, execfn, prog, argv, envp);
    m->system->fds.list = old->system->fds.list;
    old->system->fds.list = 0;
    // releasing the execve() lock must come after unlocking fds
    memcpy(&oldmask, &old->system->exec_sigmask, sizeof(oldmask));
    UNLOCK(&old->system->exec_lock);
    // freeing the last machine in a system will free its system too
    FreeMachine(old);
    // restore the signal mask we had before execve() was called
    unassert(!pthread_sigmask(SIG_SETMASK, &oldmask, 0));
  }
  Blink(m);
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

_Noreturn static void PrintVersion(void) {
  Print(1, VERSION);
  exit(0);
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
      case 's':
        ++FLAG_strace;
        break;
      case 'm':
        FLAG_nolinear = true;
        break;
      case 'Z':
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
      case 'v':
        PrintVersion();
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
  signal(SIGPIPE, SIG_IGN);
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
  unassert(!sigaction(SIGTERM, &sa, 0));
  unassert(!sigaction(SIGXCPU, &sa, 0));
  unassert(!sigaction(SIGXFSZ, &sa, 0));
#if !defined(__SANITIZE_THREAD__) && !defined(__SANITIZE_ADDRESS__)
  sa.sa_sigaction = OnFatalSystemSignal;
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
  GetStartDir();
#ifndef DISABLE_STRACE
  setlocale(LC_ALL, "");
#endif
  // Ensure utf-8 is printed correctly on windows, this method
  // has issues(http://stackoverflow.com/a/10884364/4279) but
  // should work for at least windows 7 and newer.
#if defined(_WIN32) && !defined(__CYGWIN__)
  SetConsoleOutputCP(CP_UTF8);
#endif
  g_blink_path = argc > 0 ? argv[0] : 0;
  WriteErrorInit();
  GetOpts(argc, argv);
  InitMap();
  if (optind_ == argc) {
    PrintUsage(argc, argv, 48, 2);
  }
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
