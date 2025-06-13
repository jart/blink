/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│ vi: set et ft=c ts=2 sts=2 sw=2 fenc=utf-8                               :vi │
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
#include "blink/log.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/fspath.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/thread.h"
#include "blink/tsan.h"
#include "blink/types.h"
#include "blink/util.h"

#define LOG_ERR  0
#define LOG_INFO 1

#define DEFAULT_LOG_PATH "blink.log"

#define APPEND(F, ...) \
  n += F(b + n, n > PIPE_BUF ? 0 : PIPE_BUF - n, __VA_ARGS__)

static struct Log {
  pthread_once_t_ once;
  int level;
  int fd;
  char *path;
} g_log = {
    PTHREAD_ONCE_INIT_,
    LOG_ERR,
};

static char *GetTimestamp(void) {
  int x;
  struct timespec ts;
  _Thread_local static i64 last;
  _Thread_local static char s[27];
  _Thread_local static struct tm tm;
  IGNORE_RACES_START();
  clock_gettime(CLOCK_REALTIME, &ts);
  if (ts.tv_sec != last) {
    localtime_r(&ts.tv_sec, &tm);
    x = tm.tm_year + 1900;
    s[0] = '0' + x / 1000;
    s[1] = '0' + x / 100 % 10;
    s[2] = '0' + x / 10 % 10;
    s[3] = '0' + x % 10;
    s[4] = '-';
    x = tm.tm_mon + 1;
    s[5] = '0' + x / 10;
    s[6] = '0' + x % 10;
    s[7] = '-';
    x = tm.tm_mday;
    s[8] = '0' + x / 10;
    s[9] = '0' + x % 10;
    s[10] = 'T';
    x = tm.tm_hour;
    s[11] = '0' + x / 10;
    s[12] = '0' + x % 10;
    s[13] = ':';
    x = tm.tm_min;
    s[14] = '0' + x / 10;
    s[15] = '0' + x % 10;
    s[16] = ':';
    x = tm.tm_sec;
    s[17] = '0' + x / 10;
    s[18] = '0' + x % 10;
    s[19] = '.';
    s[26] = 0;
    last = ts.tv_sec;
  }
  IGNORE_RACES_END();
  x = ts.tv_nsec;
  s[20] = '0' + x / 100000000;
  s[21] = '0' + x / 10000000 % 10;
  s[22] = '0' + x / 1000000 % 10;
  s[23] = '0' + x / 100000 % 10;
  s[24] = '0' + x / 10000 % 10;
  s[25] = '0' + x / 1000 % 10;
  return s;
}

static void OpenLog(void) {
  int fd;
  if (!g_log.path) return;
  if (!strcmp(g_log.path, "-") ||  //
      !strcmp(g_log.path, "/dev/stderr")) {
    fd = 2;
  } else if (!strcmp(g_log.path, "/dev/stdout")) {
    fd = 1;
  } else {
    fd = open(g_log.path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (fd == -1) {
      perror(g_log.path);
      g_log.fd = -1;
      return;
    }
  }
#ifndef F_DUPFD_CLOEXEC
  unassert((g_log.fd = fcntl(fd, F_DUPFD, kMinBlinkFd)) != -1);
  unassert(fcntl(g_log.fd, F_SETFL, FD_CLOEXEC) != -1);
#else
  unassert((g_log.fd = fcntl(fd, F_DUPFD_CLOEXEC, kMinBlinkFd)) != -1);
#endif
  unassert(!close(fd));
}

static void Log(const char *file, int line, const char *fmt, va_list va,
                int level) {
  char b[4096];
  int err, n = 0;
  err = errno;
  unassert(!pthread_once_(&g_log.once, OpenLog));
  APPEND(snprintf, "%c%s:%s:%d:%d ", "EI"[level], GetTimestamp(), file, line,
         g_machine ? g_machine->tid : 0);
  APPEND(vsnprintf, fmt, va);
  APPEND(snprintf, "\n");
  if (n > PIPE_BUF - 1) {
    n = PIPE_BUF - 1;
    b[n - 1] = '\n';
    b[n - 2] = '.';
    b[n - 3] = '.';
    b[n - 4] = '.';
  }
  if (g_log.fd != -1) {
    WriteError(g_log.fd, b, n);
  }
  if (FLAG_alsologtostderr || (!FLAG_nologstderr && level <= g_log.level)) {
    WriteError(2, b, n);
  }
  errno = err;
}

void LogErr(const char *file, int line, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  Log(file, line, fmt, va, LOG_ERR);
  va_end(va);
}

void LogInfo(const char *file, int line, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  Log(file, line, fmt, va, LOG_INFO);
  va_end(va);
}

void LogSys(const char *file, int line, const char *fmt, ...) {
  va_list va;
  va_start(va, fmt);
  Log(file, line, fmt, va, LOG_INFO);
  va_end(va);
}

static void FreeLogPath(void) {
  free(g_log.path);
  g_log.path = 0;
}

static void SetLogPath(const char *path) {
  if (path) {
    g_log.path = ExpandUser(path);
  } else {
    g_log.path = JoinPath(GetStartDir(), DEFAULT_LOG_PATH);
  }
  atexit(FreeLogPath);
}

void LogInit(const char *path) {
  WriteErrorInit();
  SetLogPath(path);
}
