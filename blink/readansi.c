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
#include <ctype.h>
#include <errno.h>
#include <poll.h>
#include <unistd.h>

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#include "blink/builtin.h"
#include "blink/log.h"
#include "blink/macros.h"
#include "blink/thompike.h"
#include "blink/util.h"

ssize_t readansi(int fd, char *buf, size_t size) {
  u8 c;
  int rc, i, j;
  struct pollfd pfd[1];
  enum { kAscii, kUtf8, kEsc, kCsi, kSs } t;
  if (size) buf[0] = 0;
  for (j = i = 0, t = kAscii;;) {
    if (i + 2 >= size) {
      errno = ENOMEM;
      return -1;
    }
    if ((rc = read(fd, &c, 1)) != 1) {
      if (rc == -1 && errno == EINTR && i) {
        continue;
      }
      if (rc == -1 && errno == EAGAIN) {
#ifdef __EMSCRIPTEN__
        emscripten_sleep(50);
#else
        pfd[0].fd = fd;
        pfd[0].events = POLLIN;
        poll(pfd, 1, 0);
#endif
        continue;
      }
      return rc;
    }
    buf[i++] = c;
    buf[i] = 0;
    switch (t) {
      case kAscii:
        if (c < 0200) {
          if (c == 033) {
            t = kEsc;
          } else {
            return i;
          }
        } else if (c >= 0300) {
          t = kUtf8;
          j = ThomPikeLen(c) - 1;
        }
        break;
      case kUtf8:
        if (!--j) return i;
        break;
      case kEsc:
        switch (c) {
          case '[':
            t = kCsi;
            break;
          case 'N':
          case 'O':
            t = kSs;
            break;
          case 0x20:
          case 0x21:
          case 0x22:
          case 0x23:
          case 0x24:
          case 0x25:
          case 0x26:
          case 0x27:
          case 0x28:
          case 0x29:
          case 0x2A:
          case 0x2B:
          case 0x2C:
          case 0x2D:
          case 0x2E:
          case 0x2F:
            break;
          default:
            return i;
        }
        break;
      case kCsi:
        switch (c) {
          case ':':
          case ';':
          case '<':
          case '=':
          case '>':
          case '?':
          case '0':
          case '1':
          case '2':
          case '3':
          case '4':
          case '5':
          case '6':
          case '7':
          case '8':
          case '9':
            break;
          default:
            return i;
        }
        break;
      case kSs:
        return i;
      default:
        __builtin_unreachable();
    }
  }
}
