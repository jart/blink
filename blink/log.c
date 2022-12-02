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
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/tsan.h"
#include "blink/types.h"

#define APPEND(F, ...) n += F(b + n, PIPE_BUF - n, __VA_ARGS__)

static int g_log;

static char *GetTimestamp(void) {
  int x;
  struct timespec ts;
  static _Thread_local char s[27];
  static _Thread_local i64 last;
  static _Thread_local struct tm tm;
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

void Log(const char *file, int line, const char *fmt, ...) {
  int n = 0;
  va_list va;
  char b[PIPE_BUF];
  va_start(va, fmt);
  APPEND(snprintf, "I%s:%s:%d: %d: ", GetTimestamp(), file, line,
         g_machine ? g_machine->tid : 0);
  APPEND(vsnprintf, fmt, va);
  APPEND(snprintf, "\n");
  va_end(va);
  if (n > PIPE_BUF - 1) {
    n = PIPE_BUF - 1;
    b[--n] = '\n';
    b[--n] = '.';
    b[--n] = '.';
    b[--n] = '.';
  }
  WriteError(g_log, b, n);
}

void LogInit(const char *path) {
  int fd;
  WriteErrorInit();
  if (!path) {
    path = "/tmp/blink.log";
  }
  fd = open(path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
  if (fd == -1) {
    perror(path);
    exit(1);
  }
  unassert((g_log = fcntl(fd, F_DUPFD_CLOEXEC, 100)) != -1);
  unassert(!close(fd));
}
