/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
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
#include <string.h>

#include "blink/linux.h"
#include "blink/util.h"

const char *DescribeSignal(int sig) {
  char *p;
  _Thread_local static char buf[30];
  switch (sig) {
    case SIGHUP_LINUX:
      return "SIGHUP";
    case SIGINT_LINUX:
      return "SIGINT";
    case SIGQUIT_LINUX:
      return "SIGQUIT";
    case SIGILL_LINUX:
      return "SIGILL";
    case SIGTRAP_LINUX:
      return "SIGTRAP";
    case SIGABRT_LINUX:
      return "SIGABRT";
    case SIGBUS_LINUX:
      return "SIGBUS";
    case SIGFPE_LINUX:
      return "SIGFPE";
    case SIGKILL_LINUX:
      return "SIGKILL";
    case SIGUSR1_LINUX:
      return "SIGUSR1";
    case SIGSEGV_LINUX:
      return "SIGSEGV";
    case SIGUSR2_LINUX:
      return "SIGUSR2";
    case SIGPIPE_LINUX:
      return "SIGPIPE";
    case SIGALRM_LINUX:
      return "SIGALRM";
    case SIGTERM_LINUX:
      return "SIGTERM";
    case SIGSTKFLT_LINUX:
      return "SIGSTKFLT";
    case SIGCHLD_LINUX:
      return "SIGCHLD";
    case SIGCONT_LINUX:
      return "SIGCONT";
    case SIGSTOP_LINUX:
      return "SIGSTOP";
    case SIGTSTP_LINUX:
      return "SIGTSTP";
    case SIGTTIN_LINUX:
      return "SIGTTIN";
    case SIGTTOU_LINUX:
      return "SIGTTOU";
    case SIGURG_LINUX:
      return "SIGURG";
    case SIGXCPU_LINUX:
      return "SIGXCPU";
    case SIGXFSZ_LINUX:
      return "SIGXFSZ";
    case SIGVTALRM_LINUX:
      return "SIGVTALRM";
    case SIGPROF_LINUX:
      return "SIGPROF";
    case SIGWINCH_LINUX:
      return "SIGWINCH";
    case SIGIO_LINUX:
      return "SIGIO";
    case SIGSYS_LINUX:
      return "SIGSYS";
    case SIGINFO_LINUX:
      return "SIGINFO";
    case SIGPWR_LINUX:
      return "SIGPWR";
    default:
      break;
  }
  p = buf;
  if (SIGRTMIN_LINUX <= sig && sig <= SIGRTMAX_LINUX) {
    p = stpcpy(p, "SIGRTMIN+");
    p = FormatInt64(p, sig - SIGRTMIN_LINUX);
  } else {
    p = FormatInt64(p, sig);
  }
  return buf;
}
