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
#include "blink/types.h"
#include <stdlib.h>
#include <string.h>

#include "blink/bitscan.h"
#include "blink/buffer.h"
#include "blink/builtin.h"
#include "blink/macros.h"
#include "blink/panel.h"

static int tpdecode(const char *s, wint_t *out) {
  u32 wc, cb, need, msb, j, i = 0;
  if ((wc = s[i++] & 255) == -1) return -1;
  while ((wc & 0300) == 0200) {
    if ((wc = s[i++] & 255) == -1) return -1;
  }
  if (!(0 <= wc && wc <= 0x7F)) {
    msb = wc < 252 ? bsr(~wc & 0xff) : 1;
    need = 7 - msb;
    wc &= ((1u << msb) - 1) | 0003;
    for (j = 1; j < need; ++j) {
      if ((cb = s[i++] & 255) == -1) return -1;
      if ((cb & 0300) == 0200) {
        wc = wc << 6 | (cb & 077);
      } else {
        if (out) *out = 0xFFFD;
        return -1;
      }
    }
  }
  if (out) *out = wc;
  return i;
}

/**
 * Renders panel div flex boxen inside terminal display for tui.
 *
 * You can use all the UNICODE and ANSI escape sequences you want.
 *
 * @param fd is file descriptor
 * @param pn is number of panels
 * @param p is panel list in logically sorted order
 * @param tyn is terminal height in cells
 * @param txn is terminal width in cells
 * @return -1 w/ errno if an error happened
 * @see nblack's notcurses project too!
 */
ssize_t PrintPanels(int fd, long pn, struct Panel *p, long tyn, long txn) {
  wint_t wc;
  ssize_t rc;
  struct Buffer b, *l;
  int x, y, i, j, width;
  enum { kUtf8, kAnsi, kAnsiCsi } s;
  memset(&b, 0, sizeof(b));
  AppendStr(&b, "\033[H");
  for (y = 0; y < tyn; ++y) {
    if (y) AppendFmt(&b, "\033[%dH", y + 1);
    for (x = i = 0; i < pn; ++i) {
      if (p[i].top <= y && y < p[i].bottom) {
        j = 0;
        s = kUtf8;
        l = &p[i].lines[y - p[i].top];
        while (x < p[i].left) {
          AppendChar(&b, ' ');
          x += 1;
        }
        while (x < p[i].right || j < l->i) {
          wc = '\0';
          width = 0;
          if (j < l->i) {
            wc = l->p[j];
            switch (s) {
              case kUtf8:
                switch (wc & 255) {
                  case 033:
                    s = kAnsi;
                    ++j;
                    break;
                  default:
                    j += abs(tpdecode(l->p + j, &wc));
                    if (x < p[i].right) {
                      width = wcwidth(wc);
                      width = MAX(1, width);
                    } else {
                      wc = 0;
                    }
                    break;
                }
                break;
              case kAnsi:
                switch (wc & 255) {
                  case '[':
                    s = kAnsiCsi;
                    ++j;
                    break;
                  case '@':
                  case ']':
                  case '^':
                  case '_':
                  case '\\':
                  case 'A':
                  case 'B':
                  case 'C':
                  case 'D':
                  case 'E':
                  case 'F':
                  case 'G':
                  case 'H':
                  case 'I':
                  case 'J':
                  case 'K':
                  case 'L':
                  case 'M':
                  case 'N':
                  case 'O':
                  case 'P':
                  case 'Q':
                  case 'R':
                  case 'S':
                  case 'T':
                  case 'U':
                  case 'V':
                  case 'W':
                  case 'X':
                  case 'Y':
                  case 'Z':
                    s = kUtf8;
                    ++j;
                    break;
                  default:
                    s = kUtf8;
                    continue;
                }
                break;
              case kAnsiCsi:
                switch (wc & 255) {
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
                    ++j;
                    break;
                  case '`':
                  case '~':
                  case '^':
                  case '@':
                  case '[':
                  case ']':
                  case '{':
                  case '}':
                  case '_':
                  case '|':
                  case '\\':
                  case 'A':
                  case 'B':
                  case 'C':
                  case 'D':
                  case 'E':
                  case 'F':
                  case 'G':
                  case 'H':
                  case 'I':
                  case 'J':
                  case 'K':
                  case 'L':
                  case 'M':
                  case 'N':
                  case 'O':
                  case 'P':
                  case 'Q':
                  case 'R':
                  case 'S':
                  case 'T':
                  case 'U':
                  case 'V':
                  case 'W':
                  case 'X':
                  case 'Y':
                  case 'Z':
                  case 'a':
                  case 'b':
                  case 'c':
                  case 'd':
                  case 'e':
                  case 'f':
                  case 'g':
                  case 'h':
                  case 'i':
                  case 'j':
                  case 'k':
                  case 'l':
                  case 'm':
                  case 'n':
                  case 'o':
                  case 'p':
                  case 'q':
                  case 'r':
                  case 's':
                  case 't':
                  case 'u':
                  case 'v':
                  case 'w':
                  case 'x':
                  case 'y':
                  case 'z':
                    s = kUtf8;
                    ++j;
                    break;
                  default:
                    s = kUtf8;
                    continue;
                }
                break;
              default:
                __builtin_unreachable();
            }
            if (x > p[i].right) {
              break;
            }
          } else if (x < p[i].right) {
            wc = ' ';
            width = 1;
          }
          if (wc) {
            x += width;
            AppendWide(&b, wc);
          }
        }
      }
    }
  }
  rc = WriteBuffer(&b, fd);
  free(b.p);
  return rc;
}
