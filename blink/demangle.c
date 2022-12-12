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
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/lock.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/sigwinch.h"
#include "blink/util.h"

struct CxxFilt {
  pthread_mutex_t lock;
  int reader;
  int writer;
  int pid;
} g_cxxfilt = {
    PTHREAD_MUTEX_INITIALIZER,
};

static void CloseCxxFilt(void) {
  sigset_t ss, oldss;
  LOCK(&g_cxxfilt.lock);
  if (g_cxxfilt.pid > 0) {
    LOGF("closing pipe to c++filt");
    unassert(!sigemptyset(&ss));
    unassert(!sigaddset(&ss, SIGCHLD));
    unassert(!pthread_sigmask(SIG_BLOCK, &ss, &oldss));
    unassert(!close(g_cxxfilt.writer));
    unassert(waitpid(g_cxxfilt.pid, 0, 0) == g_cxxfilt.pid);
    unassert(!close(g_cxxfilt.reader));
    unassert(!pthread_sigmask(SIG_SETMASK, &oldss, 0));
    g_cxxfilt.pid = -1;
  }
  UNLOCK(&g_cxxfilt.lock);
}

static void SpawnCxxFilt(void) {
  sigset_t ss;
  const char *cxxfilt;
  char executable[PATH_MAX];
  int pipefds[2][2] = {{-1, -1}, {-1, -1}};
  cxxfilt = getenv("CXXFILT");
  if (cxxfilt && !*cxxfilt) {
    LOGF("$CXXFILT was empty string, disabling c++ demangling");
    g_cxxfilt.pid = -1;
    return;
  }
  if (!cxxfilt) cxxfilt = "c++filt";
  if (!Commandv(cxxfilt, executable, sizeof(executable))) {
    LOGF("path lookup of $CXXFILT (%s) failed!", cxxfilt);
    g_cxxfilt.pid = -1;
    return;
  }
  LOGF("spawning c++ symbol demangler %s", executable);
  if (pipe(pipefds[1]) == -1 ||  //
      pipe(pipefds[0]) == -1 ||  //
      (g_cxxfilt.pid = fork()) == -1) {
    LOGF("can't launch c++ demangler: %s", strerror(errno));
    close(pipefds[0][0]);
    close(pipefds[0][1]);
    close(pipefds[1][0]);
    close(pipefds[1][1]);
    g_cxxfilt.pid = -1;
    return;
  }
  if (!g_cxxfilt.pid) {
    unassert(!sigemptyset(&ss));
    unassert(!sigaddset(&ss, SIGINT));
    unassert(!sigaddset(&ss, SIGQUIT));
    unassert(!sigaddset(&ss, SIGWINCH));
    unassert(!sigprocmask(SIG_BLOCK, &ss, 0));
    unassert(dup2(pipefds[1][0], 0) == 0);
    unassert(dup2(pipefds[0][1], 1) == 1);
    if (pipefds[0][0] > 1) unassert(!close(pipefds[0][0]));
    if (pipefds[0][1] > 1) unassert(!close(pipefds[0][1]));
    if (pipefds[1][0] > 1) unassert(!close(pipefds[1][0]));
    if (pipefds[1][1] > 1) unassert(!close(pipefds[1][1]));
    execv(executable, (char *const[]){(char *)cxxfilt, 0});
    _exit(127);
  }
  unassert(!close(pipefds[0][1]));
  unassert(!close(pipefds[1][0]));
  g_cxxfilt.reader = pipefds[0][0];
  g_cxxfilt.writer = pipefds[1][1];
  atexit(CloseCxxFilt);
}

static char *CopySymbol(char *p, size_t pn, const char *s, size_t sn) {
  size_t extra;
  bool showdots, iscomplicated;
  unassert(pn >= 1 + 3 + 1 + 1);
  iscomplicated = memchr(s, ' ', sn) || memchr(s, '(', sn);
  extra = 1;
  if (iscomplicated) extra += 2;
  if (sn + extra > pn) {
    sn = pn - extra - 3;
    showdots = true;
  } else {
    showdots = false;
  }
  if (iscomplicated) *p++ = '"';
  if (sn) {
    memcpy(p, s, sn);
    p = p + sn;
  }
  if (showdots) p = stpcpy(p, "...");
  if (iscomplicated) *p++ = '"';
  *p = '\0';
  return p;
}

static char *DemangleCxxFilt(char *p, size_t pn, const char *s, size_t sn) {
  char *res;
  ssize_t rc;
  size_t got;
  struct iovec iov[2];
  static char buf[512];
  buf[0] = '\n';
  iov[0].iov_base = (void *)s;
  iov[0].iov_len = sn;
  iov[1].iov_base = buf;
  iov[1].iov_len = 1;
  if (writev(g_cxxfilt.writer, iov, ARRAYLEN(iov)) != -1) {
    if ((rc = read(g_cxxfilt.reader, buf, sizeof(buf))) != -1) {
      got = rc;
      if (got >= 2 && buf[got - 1] == '\n') {
        if (buf[got - 2] == '\r') --got;
        --got;
        res = CopySymbol(p, pn, buf, got);
      } else {
        LOGF("c++filt read didn't end with a newline!");
        res = 0;
      }
    } else {
      LOGF("failed to read from c++filt pipe: %s", strerror(errno));
      res = 0;
    }
  } else {
    LOGF("failed to write to c++filt pipe: %s", strerror(errno));
    res = 0;
  }
  return res;
}

/**
 * Decrypts C++ symbol.
 *
 * This function turns C++ symbols from their linkable name (e.g.
 * `_ZN15set_early_dwarfD1Ev`) into a readable name (e.g.
 * `set_early_dwarf::~set_early_dwarf()`). If the symbol in question
 * isn't from a C++ program, then the symbol is simply copied to the
 * output. The same applies if the translation fails, which means this
 * function always succeeds.
 *
 * Filtering is performed by piping symbols to the `c++filt` command. If
 * this program isn't on the `$PATH` or has a different name, then that
 * name or path may be set using the `$CXXFILT` environment variable. If
 * this environment variable is present and contains an empty string,
 * then C++ symbol translation will be disabled.
 *
 * @param p is output buffer
 * @param symbol is nul-terminated link-time symbol to translate
 * @param n is the number of bytes available in `p`
 * @return pointer to NUL byte, cf. stcpy()
 */
char *Demangle(char *p, const char *symbol, size_t n) {
  char *r;
  size_t sn;
  sigset_t ss, oldss;
  sn = strlen(symbol);
  if (startswith(symbol, "_Z")) {
    unassert(!sigemptyset(&ss));
    unassert(!sigaddset(&ss, SIGINT));
    unassert(!sigaddset(&ss, SIGALRM));
    unassert(!sigaddset(&ss, SIGWINCH));
    unassert(!pthread_sigmask(SIG_BLOCK, &ss, &oldss));
    LOCK(&g_cxxfilt.lock);
    if (!g_cxxfilt.pid) SpawnCxxFilt();
    if (g_cxxfilt.pid != -1) {
      r = DemangleCxxFilt(p, n, symbol, sn);
    } else {
      r = 0;
    }
    UNLOCK(&g_cxxfilt.lock);
    unassert(!pthread_sigmask(SIG_SETMASK, &oldss, 0));
    if (!r) {
      CloseCxxFilt();
    }
  } else {
    r = 0;
  }
  if (!r) {
    r = CopySymbol(p, n, symbol, sn);
  }
  return r;
}
