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
#include "blink/pty.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/endian.h"
#include "blink/macros.h"
#include "blink/thompike.h"
#include "blink/util.h"

#ifdef IUTF8
#define CURSOR L'▂'
#else
#define CURSOR '_'
#endif

/**
 * @fileoverview Pseudoteletypewriter
 *
 *   \t                                  TAB
 *   \a                                  BELL
 *   \177                                BACKSPACE
 *   \r                                  CURSOR START
 *   \b                                  CURSOR LEFT OR CURSOR REWIND
 *   \n                                  CURSOR DOWN AND START IF OPOST
 *   \f                                  CURSOR DOWN AND START IF OPOST
 *   \v                                  CURSOR DOWN AND START OR \e[H\e[J
 *   \eE                                 CURSOR DOWN AND START
 *   \eD                                 CURSOR DOWN
 *   \eM                                 CURSOR UP
 *   \ec                                 FULL RESET
 *   \e7                                 SAVE CURSOR POSITION
 *   \e8                                 RESTORE CURSOR POSITION
 *   \e(0                                DEC SPECIAL GRAPHICS
 *   \e(B                                USAS X3.4-1967
 *   \e#5                                SINGLE WIDTH
 *   \e#6                                DOUBLE WIDTH
 *   \e#8                                SO MANY E
 *   \eZ → \e/Z                          REPORT IDENTITY
 *   \e[𝑛A                               CURSOR UP             [CLAMPED]
 *   \e[𝑛B                               CURSOR DOWN           [CLAMPED]
 *   \e[𝑛C                               CURSOR RIGHT          [CLAMPED]
 *   \e[𝑛D                               CURSOR LEFT           [CLAMPED]
 *   \e[𝑦;𝑥H                             SET CURSOR POSITION   [CLAMPED]
 *   \e[𝑥G                               SET CURSOR COLUMN     [CLAMPED]
 *   \e[𝑦d                               SET CURSOR ROW        [CLAMPED]
 *   \e[𝑛E                               CURSOR DOWN AND START [CLAMPED]
 *   \e[𝑛F                               CURSOR UP AND START   [CLAMPED]
 *   \e[𝑛S                               SCROLL UP
 *   \e[𝑛T                               SCROLL DOWN
 *   \e[𝑛@                               INSERT CELLS
 *   \e[𝑛P                               DELETE CELLS
 *   \e[𝑛L                               INSERT LINES
 *   \e[𝑛M                               DELETE LINES
 *   \e[J                                ERASE DISPLAY FORWARDS
 *   \e[1J                               ERASE DISPLAY BACKWARDS
 *   \e[2J                               ERASE DISPLAY
 *   \e[K                                ERASE LINE FORWARD
 *   \e[1K                               ERASE LINE BACKWARD
 *   \e[2K                               ERASE LINE
 *   \e[𝑛X                               ERASE CELLS
 *   \e[0m                               RESET
 *   \e[1m                               BOLD
 *   \e[2m                               FAINT
 *   \e[3m                               ITALIC
 *   \e[4m                               UNDER
 *   \e[5m                               BLINK
 *   \e[7m                               INVERT
 *   \e[8m                               CONCEAL
 *   \e[9m                               STRIKE
 *   \e[20m                              FRAKTUR
 *   \e[21m                              DUNDER
 *   \e[22m                              RESET BOLD & FAINT
 *   \e[23m                              RESET ITALIC & FRAKTUR
 *   \e[24m                              RESET UNDER & DUNDER
 *   \e[25m                              RESET BLINK
 *   \e[27m                              RESET INVERT
 *   \e[28m                              RESET CONCEAL
 *   \e[29m                              RESET STRIKE
 *   \e[39m                              RESET FOREGROUND
 *   \e[49m                              RESET BACKGROUND
 *   \e[30m                              BLACK FOREGROUND
 *   \e[31m                              RED FOREGROUND
 *   \e[32m                              GREEN FOREGROUND
 *   \e[33m                              YELLOW FOREGROUND
 *   \e[34m                              BLUE FOREGROUND
 *   \e[35m                              MAGENTA FOREGROUND
 *   \e[36m                              CYAN FOREGROUND
 *   \e[37m                              WHITE FOREGROUND
 *   \e[40m                              BLACK BACKGROUND
 *   \e[41m                              RED BACKGROUND
 *   \e[42m                              GREEN BACKGROUND
 *   \e[44m                              YELLOW BACKGROUND
 *   \e[44m                              BLUE BACKGROUND
 *   \e[45m                              MAGENTA BACKGROUND
 *   \e[46m                              CYAN BACKGROUND
 *   \e[47m                              WHITE BACKGROUND
 *   \e[90m                              BRIGHT BLACK FOREGROUND
 *   \e[91m                              BRIGHT RED FOREGROUND
 *   \e[92m                              BRIGHT GREEN FOREGROUND
 *   \e[93m                              BRIGHT YELLOW FOREGROUND
 *   \e[94m                              BRIGHT BLUE FOREGROUND
 *   \e[95m                              BRIGHT MAGENTA FOREGROUND
 *   \e[96m                              BRIGHT CYAN FOREGROUND
 *   \e[97m                              BRIGHT WHITE FOREGROUND
 *   \e[100m                             BRIGHT BLACK BACKGROUND
 *   \e[101m                             BRIGHT RED BACKGROUND
 *   \e[102m                             BRIGHT GREEN BACKGROUND
 *   \e[103m                             BRIGHT YELLOW BACKGROUND
 *   \e[104m                             BRIGHT BLUE BACKGROUND
 *   \e[105m                             BRIGHT MAGENTA BACKGROUND
 *   \e[106m                             BRIGHT CYAN BACKGROUND
 *   \e[107m                             BRIGHT WHITE BACKGROUND
 *   \e[38;5;𝑥m                          XTERM256 FOREGROUND
 *   \e[48;5;𝑥m                          XTERM256 BACKGROUND
 *   \e[38;2;𝑟;𝑔;𝑏m                      RGB FOREGROUND
 *   \e[48;2;𝑟;𝑔;𝑏m                      RGB BACKGROUND
 *   \e[?25h                             SHOW CURSOR
 *   \e[?25l                             HIDE CURSOR
 *   \e[s                                SAVE CURSOR POSITION
 *   \e[u                                RESTORE CURSOR POSITION
 *   \e[?5h ... \e[?5l                   REVERSE VIDEO EPILEPSY
 *   \e[0q                               RESET LEDS
 *   \e[1q                               TURN ON FIRST LED
 *   \e[2q                               TURN ON SECOND LED
 *   \e[3q                               TURN ON THIRD LED
 *   \e[4q                               TURN ON FOURTH LED
 *   \e[c → \e[?1;0c                     REPORT DEVICE TYPE
 *   \e[5n → \e[0n                       REPORT DEVICE STATUS
 *   \e[6n → \e[𝑦;𝑥R                     REPORT CURSOR POSITION
 *   \e7\e[9979;9979H\e[6n\e8 → \e[𝑦;𝑥R  REPORT DISPLAY DIMENSIONS
 *
 * @see https://vt100.net/docs/vt220-rm/chapter4.html
 * @see https://invisible-island.net/xterm/
 * @see ANSI X3.64-1979
 * @see ISO/IEC 6429
 * @see FIPS-86
 * @see ECMA-48
 */

struct Pty *NewPty(void) {
  struct Pty *pty;
  if ((pty = (struct Pty *)calloc(1, sizeof(*pty)))) {
    PtyResize(pty, 25, 80);
    PtyFullReset(pty);
    PtyErase(pty, 0, pty->yn * pty->xn);
  }
  return pty;
}

static void FreePtyPlanes(struct Pty *pty) {
  free(pty->wcs);
  free(pty->fgs);
  free(pty->bgs);
  free(pty->prs);
}

void FreePty(struct Pty *pty) {
  if (pty) {
    FreePtyPlanes(pty);
    free(pty);
  }
}

static wchar_t *GetXlatAscii(void) {
  unsigned i;
  static bool once;
  static wchar_t xlat[128];
  if (!once) {
    for (i = 0; i < 128; ++i) {
      xlat[i] = i;
    }
    once = true;
  }
  return xlat;
}

static wchar_t *GetXlatLineDrawing(void) {
  unsigned i;
  static bool once;
  static wchar_t xlat[128];
  if (!once) {
    for (i = 0; i < 128; ++i) {
      if (0x5F <= i && i <= 0x7E) {
        xlat[i] = L" ◆▒␉␌␍␊°±␤␋┘┐┌└┼⎺⎻─⎼⎽├┤┴┬│≤≥π≠£·"[i - 0x5F];
      } else {
        xlat[i] = i;
      }
    }
    once = true;
  }
  return xlat;
}

static void XlatAlphabet(wchar_t xlat[128], int a, int b) {
  unsigned i;
  for (i = 0; i < 128; ++i) {
    if ('a' <= i && i <= 'z') {
      xlat[i] = i - 'a' + a;
    } else if ('A' <= i && i <= 'Z') {
      xlat[i] = i - 'A' + b;
    } else {
      xlat[i] = i;
    }
  }
}

static wchar_t *GetXlatItalic(void) {
  static bool once;
  static wchar_t xlat[128];
  if (!once) {
#if WCHAR_MAX > 0x10000
    XlatAlphabet(xlat, L'𝑎', L'𝐴');
#else
    XlatAlphabet(xlat, L'a', L'A');
#endif
    once = true;
  }
  return xlat;
}

static wchar_t *GetXlatBoldItalic(void) {
  static bool once;
  static wchar_t xlat[128];
  if (!once) {
#if WCHAR_MAX > 0x10000
    XlatAlphabet(xlat, L'𝒂', L'𝑨');
#else
    XlatAlphabet(xlat, L'a', L'A');
#endif
    once = true;
  }
  return xlat;
}

static wchar_t *GetXlatBoldFraktur(void) {
  static bool once;
  static wchar_t xlat[128];
  if (!once) {
#if WCHAR_MAX > 0x10000
    XlatAlphabet(xlat, L'𝖆', L'𝕬');
#else
    XlatAlphabet(xlat, L'a', L'A');
#endif
    once = true;
  }
  return xlat;
}

static wchar_t *GetXlatFraktur(void) {
  unsigned i;
  static bool once;
  static wchar_t xlat[128];
  if (!once) {
#if WCHAR_MAX > 0x10000
    for (i = 0; i < ARRAYLEN(xlat); ++i) {
      if ('A' <= i && i <= 'Z') {
        xlat[i] = L"𝔄𝔅ℭ𝔇𝔈𝔉𝔊ℌℑ𝔍𝔎𝔏𝔐𝔑𝔒𝔓𝔔ℜ𝔖𝔗𝔘𝔙𝔚𝔛𝔜ℨ"[i - 'A'];
      } else if ('a' <= i && i <= 'z') {
        xlat[i] = i - 'a' + L'𝔞';
      } else {
        xlat[i] = i;
      }
    }
#else
    XlatAlphabet(xlat, L'a', L'A');
#endif
    once = true;
  }
  return xlat;
}

static wchar_t *GetXlatDoubleWidth(void) {
  unsigned i;
  static bool once;
  static wchar_t xlat[128];
  if (!once) {
    for (i = 0; i < ARRAYLEN(xlat); ++i) {
      if ('!' <= i && i <= '~') {
        xlat[i] = -(i - '!' + L'！');
      } else {
        xlat[i] = i;
      }
    }
    once = true;
  }
  return xlat;
}

static wchar_t *GetXlatSgr(struct Pty *pty) {
  switch (!!(pty->pr & kPtyFraktur) << 2 | !!(pty->pr & kPtyItalic) << 1 |
          !!(pty->pr & kPtyBold) << 0) {
    case 4:
    case 6:
      return GetXlatFraktur();
    case 5:
    case 7:
      return GetXlatBoldFraktur();
    case 3:
      return GetXlatBoldItalic();
    case 2:
      return GetXlatItalic();
    default:
      return GetXlatAscii();
  }
}

static void PtySetXlat(struct Pty *pty, wchar_t *xlat) {
  pty->xlat = xlat;
  pty->pr &= ~(kPtyItalic | kPtyFraktur);
}

static void PtySetCodepage(struct Pty *pty, char id) {
  switch (id) {
    default:
    case 'B':
      PtySetXlat(pty, GetXlatAscii());
      break;
    case '0':
      PtySetXlat(pty, GetXlatLineDrawing());
      break;
  }
}

static u32 *u32set(u32 *p, u32 c, size_t n) {
  size_t i;
  for (i = 0; i < n; ++i) {
    p[i] = c;
  }
  return p;
}

void PtyErase(struct Pty *pty, long dst, long n) {
  unassert(dst + n <= pty->yn * pty->xn);
  u32set(pty->wcs + dst, ' ', n);
  u32set(pty->prs + dst, 0, n);
}

void PtyMemmove(struct Pty *pty, long dst, long src, long n) {
  unassert(src + n <= pty->yn * pty->xn);
  unassert(dst + n <= pty->yn * pty->xn);
  memmove(pty->wcs + dst, pty->wcs + src, n * 4);
  memmove(pty->fgs + dst, pty->fgs + src, n * 4);
  memmove(pty->bgs + dst, pty->bgs + src, n * 4);
  memmove(pty->prs + dst, pty->prs + src, n * 4);
}

void PtyFullReset(struct Pty *pty) {
  pty->y = 0;
  pty->x = 0;
  pty->pr = 0;
  pty->u8 = 0;
  pty->n8 = 0;
  pty->conf = 0;
  pty->save = 0;
  pty->esc.i = 0;
  pty->input.i = 0;
  pty->state = kPtyAscii;
  pty->xlat = GetXlatAscii();
  PtyErase(pty, 0, pty->yn * pty->xn);
}

void PtySetY(struct Pty *pty, int y) {
  pty->conf &= ~kPtyRedzone;
  pty->y = MAX(0, MIN(pty->yn - 1, y));
}

void PtySetX(struct Pty *pty, int x) {
  pty->conf &= ~kPtyRedzone;
  pty->x = MAX(0, MIN(pty->xn - 1, x));
}

void PtyResize(struct Pty *pty, int yn, int xn) {
  unsigned y, ym, xm, y0;
  u32 *wcs, *fgs, *bgs, *prs;
  if (xn < 80) xn = 80;
  if (yn < 25) yn = 25;
  if (xn == pty->xn && yn == pty->yn) return;
  wcs = (u32 *)calloc(yn * xn, 4);
  fgs = (u32 *)calloc(yn * xn, 4);
  bgs = (u32 *)calloc(yn * xn, 4);
  prs = (u32 *)calloc(yn * xn, 4);
  y0 = yn > pty->y + 1 ? 0 : pty->y + 1 - yn;
  ym = MIN(yn, pty->yn) + y0;
  xm = MIN(xn, pty->xn);
  for (y = y0; y < ym; ++y) {
    memcpy(wcs + y * xn, pty->wcs + y * pty->xn, xm * 4);
    memcpy(fgs + y * xn, pty->fgs + y * pty->xn, xm * 4);
    memcpy(bgs + y * xn, pty->bgs + y * pty->xn, xm * 4);
    memcpy(prs + y * xn, pty->prs + y * pty->xn, xm * 4);
  }
  FreePtyPlanes(pty);
  pty->wcs = wcs;
  pty->fgs = fgs;
  pty->bgs = bgs;
  pty->prs = prs;
  pty->yn = yn;
  pty->xn = xn;
  PtySetY(pty, pty->y);
  PtySetX(pty, pty->x);
}

static void PtyConcatInput(struct Pty *pty, const char *data, size_t n) {
  AppendData(&pty->input, data, n);
}

static void PtyScroll(struct Pty *pty) {
  PtyMemmove(pty, 0, pty->xn, pty->xn * (pty->yn - 1));
  PtyErase(pty, pty->xn * (pty->yn - 1), pty->xn);
}

static void PtyReverse(struct Pty *pty) {
  PtyMemmove(pty, pty->xn, 0, pty->xn * (pty->yn - 1));
  PtyErase(pty, 0, pty->xn);
}

static void PtyIndex(struct Pty *pty) {
  if (pty->y < pty->yn - 1) {
    ++pty->y;
  } else {
    PtyScroll(pty);
  }
}

static void PtyReverseIndex(struct Pty *pty) {
  if (pty->y) {
    --pty->y;
  } else {
    PtyReverse(pty);
  }
}

static void PtyCarriageReturn(struct Pty *pty) {
  PtySetX(pty, 0);
}

static void PtyNewline(struct Pty *pty) {
  PtyIndex(pty);
  if (!(pty->conf & kPtyNoopost)) {
    PtyCarriageReturn(pty);
  }
}

static void PtyAdvance(struct Pty *pty) {
  unassert(pty->xn - 1 == pty->x);
  unassert(pty->conf & kPtyRedzone);
  pty->conf &= ~kPtyRedzone;
  pty->x = 0;
  if (pty->y < pty->yn - 1) {
    ++pty->y;
  } else {
    PtyScroll(pty);
  }
}

static void PtyWriteGlyph(struct Pty *pty, wint_t wc, int w) {
  u32 i;
  unassert(!(0x00 <= wc && wc <= 0x1F));
  unassert(!(0x7F <= wc && wc <= 0x9F));
  if (w < 1) wc = ' ', w = 1;
  if ((pty->conf & kPtyRedzone) || pty->x + w > pty->xn) {
    PtyAdvance(pty);
  }
  i = pty->y * pty->xn + pty->x;
  pty->wcs[i] = wc;
  if ((pty->prs[i] = pty->pr) & (kPtyFg | kPtyBg)) {
    pty->fgs[i] = pty->fg;
    pty->bgs[i] = pty->bg;
  }
  if ((pty->x += w) >= pty->xn) {
    pty->x = pty->xn - 1;
    pty->conf |= kPtyRedzone;
  }
}

static void PtyWriteTab(struct Pty *pty) {
  unsigned x, x2;
  if (pty->conf & kPtyRedzone) {
    PtyAdvance(pty);
  }
  x2 = MIN(pty->xn, ROUNDUP(pty->x + 1, 8));
  for (x = pty->x; x < x2; ++x) {
    pty->wcs[pty->y * pty->xn + x] = ' ';
  }
  if (x2 < pty->xn) {
    pty->x = x2;
  } else {
    pty->x = pty->xn - 1;
    pty->conf |= kPtyRedzone;
  }
}

int PtyAtoi(const char *s, const char **e) {
  int i;
  for (i = 0; isdigit(*s); ++s) i *= 10, i += *s - '0';
  if (e) *e = s;
  return i;
}

static int PtyGetMoveParam(struct Pty *pty) {
  int x = PtyAtoi(pty->esc.s, NULL);
  if (x < 1) x = 1;
  return x;
}

static void PtySetCursorPosition(struct Pty *pty) {
  int row, col;
  const char *s = pty->esc.s;
  row = PtyAtoi(s, &s);
  row = MAX(1, row);
  if (*s == ';') ++s;
  col = PtyAtoi(s, &s);
  col = MAX(1, col);
  PtySetY(pty, row - 1);
  PtySetX(pty, col - 1);
}

static void PtySetCursorRow(struct Pty *pty) {
  PtySetY(pty, PtyGetMoveParam(pty) - 1);
}

static void PtySetCursorColumn(struct Pty *pty) {
  PtySetX(pty, PtyGetMoveParam(pty) - 1);
}

static void PtyMoveCursor(struct Pty *pty, int dy, int dx) {
  int n = PtyGetMoveParam(pty);
  PtySetY(pty, pty->y + dy * n);
  PtySetX(pty, pty->x + dx * n);
}

static void PtyScrollUp(struct Pty *pty) {
  int n = PtyGetMoveParam(pty);
  while (n--) PtyScroll(pty);
}

static void PtyScrollDown(struct Pty *pty) {
  int n = PtyGetMoveParam(pty);
  while (n--) PtyReverse(pty);
}

static void PtySetCursorStatus(struct Pty *pty, bool status) {
  if (status) {
    pty->conf &= ~kPtyNocursor;
  } else {
    pty->conf |= kPtyNocursor;
  }
}

static void PtySetMode(struct Pty *pty, bool status) {
  const char *p = pty->esc.s;
  switch (*p++) {
    case '?':
      while (isdigit(*p)) {
        switch (PtyAtoi(p, &p)) {
          case 25:
            PtySetCursorStatus(pty, status);
            break;
          default:
            break;
        }
        if (*p == ';') {
          ++p;
        }
      }
      break;
    default:
      break;
  }
}

static void PtySaveCursorPosition(struct Pty *pty) {
  pty->save = (pty->y & 0x7FFF) | (pty->x & 0x7FFF) << 16;
}

static void PtyRestoreCursorPosition(struct Pty *pty) {
  PtySetY(pty, (pty->save & 0x00007FFF) >> 000);
  PtySetX(pty, (pty->save & 0x7FFF0000) >> 020);
}

static void PtyEraseDisplay(struct Pty *pty) {
  switch (PtyAtoi(pty->esc.s, NULL)) {
    case 0:
      PtyErase(pty, pty->y * pty->xn + pty->x,
               pty->yn * pty->xn - (pty->y * pty->xn + pty->x));
      break;
    case 1:
      PtyErase(pty, 0, pty->y * pty->xn + pty->x);
      break;
    case 2:
    case 3:
      PtyErase(pty, 0, pty->yn * pty->xn);
      break;
    default:
      break;
  }
}

static void PtyEraseLine(struct Pty *pty) {
  switch (PtyAtoi(pty->esc.s, NULL)) {
    case 0:
      PtyErase(pty, pty->y * pty->xn + pty->x, pty->xn - pty->x);
      break;
    case 1:
      PtyErase(pty, pty->y * pty->xn, pty->x);
      break;
    case 2:
      PtyErase(pty, pty->y * pty->xn, pty->xn);
      break;
    default:
      break;
  }
}

static void PtyEraseCells(struct Pty *pty) {
  int i, n, x;
  i = pty->y * pty->xn + pty->x;
  n = pty->yn * pty->xn;
  x = PtyAtoi(pty->esc.s, NULL);
  x = MIN(MAX(x, 1), n - i);
  PtyErase(pty, i, x);
}

static int PtyArg1(struct Pty *pty) {
  int x = PtyAtoi(pty->esc.s, NULL);
  return MAX(1, x);
}

static void PtyInsertCells(struct Pty *pty) {
  int n = PtyArg1(pty);
  n = MIN(pty->xn - pty->x, n);
  PtyMemmove(pty, pty->y * pty->xn + pty->x + n, pty->y * pty->xn + pty->x,
             pty->xn - (pty->x + n));
  PtyErase(pty, pty->y * pty->xn + pty->x, n);
}

static void PtyInsertLines(struct Pty *pty) {
  int n = PtyArg1(pty);
  n = MIN(pty->yn - pty->y, n);
  PtyMemmove(pty, (pty->y + n) * pty->xn, pty->y * pty->xn,
             (pty->yn - pty->y - n) * pty->xn);
  PtyErase(pty, pty->y * pty->xn, n * pty->xn);
}

static void PtyDeleteCells(struct Pty *pty) {
  int n = PtyArg1(pty);
  n = MIN(pty->xn - pty->x, n);
  PtyMemmove(pty, pty->y * pty->xn + pty->x, pty->y * pty->xn + pty->x + n,
             pty->xn - (pty->x + n));
  PtyErase(pty, pty->y * pty->xn + pty->x, n);
}

static void PtyDeleteLines(struct Pty *pty) {
  int n = PtyArg1(pty);
  n = MIN(pty->yn - pty->y, n);
  PtyMemmove(pty, pty->y * pty->xn, (pty->y + n) * pty->xn,
             (pty->yn - pty->y - n) * pty->xn);
  PtyErase(pty, (pty->y + n) * pty->xn, n * pty->xn);
}

static void PtyReportDeviceStatus(struct Pty *pty) {
  PtyWriteInput(pty, "\033[0n", 4);
}

static void PtyReportPreferredVtType(struct Pty *pty) {
  PtyWriteInput(pty, "\033[?1;0c", 4);
}

static void PtyReportPreferredVtIdentity(struct Pty *pty) {
  PtyWriteInput(pty, "\033/Z", 4);
}

static void PtyBell(struct Pty *pty) {
  pty->conf |= kPtyBell;
}

static void PtyLed(struct Pty *pty) {
  switch (PtyAtoi(pty->esc.s, NULL)) {
    case 0:
      pty->conf &= ~kPtyLed1;
      pty->conf &= ~kPtyLed2;
      pty->conf &= ~kPtyLed3;
      pty->conf &= ~kPtyLed4;
      break;
    case 1:
      pty->conf |= kPtyLed1;
      break;
    case 2:
      pty->conf |= kPtyLed2;
      break;
    case 3:
      pty->conf |= kPtyLed3;
      break;
    case 4:
      pty->conf |= kPtyLed4;
      break;
    default:
      break;
  }
}

static void PtyReportCursorPosition(struct Pty *pty) {
  char buf[2 + 10 + 1 + 10 + 1];
  PtyWriteInput(pty, buf,
                sprintf(buf, "\033[%d;%dR", (pty->y + 1) & 0x7fff,
                        (pty->x + 1) & 0x7fff));
}

static void PtyCsiN(struct Pty *pty) {
  switch (PtyAtoi(pty->esc.s, NULL)) {
    case 5:
      PtyReportDeviceStatus(pty);
      break;
    case 6:
      PtyReportCursorPosition(pty);
      break;
    default:
      break;
  }
}

static void PtySelectGraphicsRendition(struct Pty *pty) {
  char *p, c;
  unsigned x;
  u8 code[4];
  enum Sgr {
    kSgr,
    kSgrFg = 010,
    kSgrFgTrue = 012,
    kSgrFgXterm = 015,
    kSgrBg = 020,
    kSgrBgTrue = 022,
    kSgrBgXterm = 025,
  } t;
  x = 0;
  t = kSgr;
  p = pty->esc.s;
  memset(code, 0, sizeof(code));
  for (;;) {
    c = *p++;
    switch (c) {
      case '\0':
        return;
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
        x *= 10;
        x += c - '0';
        break;
      case ';':
      case 'm':
        code[code[3]] = x;
        x = 0;
        switch (t) {
          case kSgr:
            switch (code[0]) {
              case 38:
                t = kSgrFg;
                break;
              case 48:
                t = kSgrBg;
                break;
              case 0:
                pty->pr = 0;
                pty->xlat = GetXlatSgr(pty);
                break;
              case 1:
                pty->pr |= kPtyBold;
                pty->xlat = GetXlatSgr(pty);
                break;
              case 2:
                pty->pr |= kPtyFaint;
                break;
              case 3:
                pty->pr |= kPtyItalic;
                pty->xlat = GetXlatSgr(pty);
                break;
              case 4:
                pty->pr |= kPtyUnder;
                break;
              case 5:
                pty->pr |= kPtyBlink;
                break;
              case 7:
                pty->pr |= kPtyFlip;
                break;
              case 8:
                pty->pr |= kPtyConceal;
                break;
              case 9:
                pty->pr |= kPtyStrike;
                break;
              case 20:
                pty->pr |= kPtyFraktur;
                pty->xlat = GetXlatSgr(pty);
                break;
              case 21:
                pty->pr |= kPtyUnder | kPtyDunder;
                break;
              case 22:
                pty->pr &= ~(kPtyFaint | kPtyBold);
                pty->xlat = GetXlatSgr(pty);
                break;
              case 23:
                pty->pr &= ~kPtyItalic;
                pty->xlat = GetXlatSgr(pty);
                break;
              case 24:
                pty->pr &= ~(kPtyUnder | kPtyDunder);
                break;
              case 25:
                pty->pr &= ~kPtyBlink;
                break;
              case 27:
                pty->pr &= ~kPtyFlip;
                break;
              case 28:
                pty->pr &= ~kPtyConceal;
                break;
              case 29:
                pty->pr &= ~kPtyStrike;
                break;
              case 39:
                pty->pr &= ~kPtyFg;
                break;
              case 49:
                pty->pr &= ~kPtyBg;
                break;
              case 90:
              case 91:
              case 92:
              case 93:
              case 94:
              case 95:
              case 96:
              case 97:
                code[0] -= 90 - 30;
                code[0] += 8;
                /* fallthrough */
              case 30:
              case 31:
              case 32:
              case 33:
              case 34:
              case 35:
              case 36:
              case 37:
                pty->fg = code[0] - 30;
                pty->pr |= kPtyFg;
                pty->pr &= ~kPtyTrue;
                break;
              case 100:
              case 101:
              case 102:
              case 103:
              case 104:
              case 105:
              case 106:
              case 107:
                code[0] -= 100 - 40;
                code[0] += 8;
                /* fallthrough */
              case 40:
              case 41:
              case 42:
              case 43:
              case 44:
              case 45:
              case 46:
              case 47:
                pty->bg = code[0] - 40;
                pty->pr |= kPtyBg;
                pty->pr &= ~kPtyTrue;
                break;
              default:
                break;
            }
            break;
          case kSgrFg:
          case kSgrBg:
            switch (code[0]) {
              case 2:
              case 5:
                t = (enum Sgr)((int)t + code[0]);
                break;
              default:
                t = kSgr;
                break;
            }
            break;
          case kSgrFgTrue:
            if (++code[3] == 3) {
              code[3] = 0;
              t = kSgr;
              pty->fg = Read32(code);
              pty->pr |= kPtyFg;
              pty->pr |= kPtyTrue;
            }
            break;
          case kSgrBgTrue:
            if (++code[3] == 3) {
              code[3] = 0;
              t = kSgr;
              pty->bg = Read32(code);
              pty->pr |= kPtyBg;
              pty->pr |= kPtyTrue;
            }
            break;
          case kSgrFgXterm:
            t = kSgr;
            pty->fg = code[0];
            pty->pr |= kPtyFg;
            pty->pr &= ~kPtyTrue;
            break;
          case kSgrBgXterm:
            t = kSgr;
            pty->bg = code[0];
            pty->pr |= kPtyBg;
            pty->pr &= ~kPtyTrue;
            break;
          default:
            Abort();
        }
        break;
      default:
        break;
    }
  }
}

static void PtyCsi(struct Pty *pty) {
  switch (pty->esc.s[pty->esc.i - 1]) {
    case 'f':
    case 'H':
      PtySetCursorPosition(pty);
      break;
    case 'G':
      PtySetCursorColumn(pty);
      break;
    case 'd':
      PtySetCursorRow(pty);
      break;
    case 'F':
      pty->x = 0;
      /* fallthrough */
    case 'A':
      PtyMoveCursor(pty, -1, +0);
      break;
    case 'E':
      pty->x = 0;
      /* fallthrough */
    case 'B':
      PtyMoveCursor(pty, +1, +0);
      break;
    case 'C':
      PtyMoveCursor(pty, +0, +1);
      break;
    case 'D':
      PtyMoveCursor(pty, +0, -1);
      break;
    case 'S':
      PtyScrollUp(pty);
      break;
    case 'T':
      PtyScrollDown(pty);
      break;
    case '@':
      PtyInsertCells(pty);
      break;
    case 'P':
      PtyDeleteCells(pty);
      break;
    case 'L':
      PtyInsertLines(pty);
      break;
    case 'M':
      PtyDeleteLines(pty);
      break;
    case 'J':
      PtyEraseDisplay(pty);
      break;
    case 'K':
      PtyEraseLine(pty);
      break;
    case 'X':
      PtyEraseCells(pty);
      break;
    case 's':
      PtySaveCursorPosition(pty);
      break;
    case 'u':
      PtyRestoreCursorPosition(pty);
      break;
    case 'n':
      PtyCsiN(pty);
      break;
    case 'm':
      PtySelectGraphicsRendition(pty);
      break;
    case 'h':
      PtySetMode(pty, true);
      break;
    case 'l':
      PtySetMode(pty, false);
      break;
    case 'c':
      PtyReportPreferredVtType(pty);
      break;
    case 'q':
      PtyLed(pty);
      break;
    default:
      break;
  }
}

static void PtyScreenAlignmentDisplay(struct Pty *pty) {
  u32set(pty->wcs, 'E', pty->yn * pty->xn);
}

static void PtyEscHash(struct Pty *pty) {
  switch (pty->esc.s[1]) {
    case '5':
      PtySetXlat(pty, GetXlatAscii());
      break;
    case '6':
      PtySetXlat(pty, GetXlatDoubleWidth());
      break;
    case '8':
      PtyScreenAlignmentDisplay(pty);
      break;
    default:
      break;
  }
}

static void PtyEsc(struct Pty *pty) {
  switch (pty->esc.s[0]) {
    case 'c':
      PtyFullReset(pty);
      break;
    case '7':
      PtySaveCursorPosition(pty);
      break;
    case '8':
      PtyRestoreCursorPosition(pty);
      break;
    case 'E':
      pty->x = 0;
    case 'D':
      PtyIndex(pty);
      break;
    case 'M':
      PtyReverseIndex(pty);
      break;
    case 'Z':
      PtyReportPreferredVtIdentity(pty);
      break;
    case '(':
      PtySetCodepage(pty, pty->esc.s[1]);
      break;
    case '#':
      PtyEscHash(pty);
      break;
    default:
      break;
  }
}

static void PtyCntrl(struct Pty *pty, int c01) {
  switch (c01) {
    case '\a':
      PtyBell(pty);
      break;
    case 0205:
    case '\f':
    case '\v':
    case '\n':
      PtyNewline(pty);
      break;
    case '\r':
      PtyCarriageReturn(pty);
      break;
    case '\t':
      PtyWriteTab(pty);
      break;
    case '\b':
    case 0177:
      pty->x = MAX(0, pty->x - 1);
      break;
    case 033:
      pty->state = kPtyEsc;
      pty->esc.i = 0;
      break;
    case 0204:
      PtyIndex(pty);
      break;
    case 0215:
      PtyReverseIndex(pty);
      break;
    case 0233:
      pty->state = kPtyCsi;
      break;
    default:
      break;
  }
}

static void PtyEscAppend(struct Pty *pty, char c) {
  pty->esc.i = MIN(pty->esc.i + 1, ARRAYLEN(pty->esc.s) - 1);
  pty->esc.s[pty->esc.i - 1] = c;
  pty->esc.s[pty->esc.i - 0] = 0;
}

ssize_t PtyWrite(struct Pty *pty, const void *data, size_t n) {
  int i;
  wchar_t wc;
  const u8 *p;
  for (p = (const u8 *)data, i = 0; i < n; ++i) {
    switch (pty->state) {
      case kPtyAscii:
        if (0x00 <= p[i] && p[i] <= 0x7F) {
          if (0x20 <= p[i] && p[i] <= 0x7E) {
            if ((wc = pty->xlat[p[i]]) >= 0) {
              PtyWriteGlyph(pty, wc, 1);
            } else {
              PtyWriteGlyph(pty, -wc, 2);
            }
          } else {
            PtyCntrl(pty, p[i]);
          }
        } else if (!ThomPikeCont(p[i])) {
          pty->state = kPtyUtf8;
          pty->u8 = ThomPikeByte(p[i]);
          pty->n8 = ThomPikeLen(p[i]) - 1;
        }
        break;
      case kPtyUtf8:
        if (ThomPikeCont(p[i])) {
          pty->u8 = ThomPikeMerge(pty->u8, p[i]);
          if (--pty->n8) break;
        }
        wc = pty->u8;
        if ((0x00 <= wc && wc <= 0x1F) || (0x7F <= wc && wc <= 0x9F)) {
          PtyCntrl(pty, wc);
        } else {
          PtyWriteGlyph(pty, wc, wcwidth(wc));
        }
        pty->state = kPtyAscii;
        pty->u8 = 0;
        --i;
        break;
      case kPtyEsc:
        if (p[i] == '[') {
          pty->state = kPtyCsi;
        } else if (0x30 <= p[i] && p[i] <= 0x7E) {
          PtyEscAppend(pty, p[i]);
          PtyEsc(pty);
          pty->state = kPtyAscii;
        } else if (0x20 <= p[i] && p[i] <= 0x2F) {
          PtyEscAppend(pty, p[i]);
        } else {
          pty->state = kPtyAscii;
        }
        break;
      case kPtyCsi:
        PtyEscAppend(pty, p[i]);
        switch (p[i]) {
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
            PtyCsi(pty);
            pty->state = kPtyAscii;
            break;
          default:
            pty->state = kPtyAscii;
            continue;
        }
        break;
      default:
        __builtin_unreachable();
    }
  }
  return n;
}

ssize_t PtyWriteInput(struct Pty *pty, const void *data, size_t n) {
  int c;
  bool cr;
  char *p;
  const char *q;
  size_t i, j, m;
  q = (char *)data;
  p = pty->input.p;
  i = pty->input.i;
  m = pty->input.n;
  if (i + n * 2 + 1 > m) {
    m = MAX(m, 8);
    do m += m >> 1;
    while (i + n * 2 + 1 > m);
    if (!(p = (char *)realloc(p, m))) {
      return -1;
    }
    pty->input.p = p;
    pty->input.n = m;
  }
  // TODO(jart): Figure out how translation settings for these work.
  cr = i && p[i - 1] == '\r';
  for (j = 0; j < n; ++j) {
    c = q[j] & 255;
    if (c == '\r') {
      cr = true;
    } else if (cr) {
      if (c != '\n') {
        p[i++] = '\n';
      }
      cr = false;
    }
    p[i++] = c;
  }
  /* if (cr) { */
  /*   p[i++] = '\n'; */
  /* } */
  if (!(pty->conf & kPtyNoecho)) {
    PtyWrite(pty, p + pty->input.i, i - pty->input.i);
  }
  pty->input.i = i;
  return n;
}

ssize_t PtyRead(struct Pty *pty, void *buf, size_t size) {
  char *p;
  size_t n;
  n = MIN(size, pty->input.i);
  if (!(pty->conf & kPtyNocanon)) {
    if ((p = (char *)memchr(pty->input.p, '\n', n))) {
      n = MIN(n, pty->input.p - p + 1);
    } else {
      n = 0;
    }
  }
  memcpy(buf, pty->input.p, n);
  memcpy(pty->input.p, pty->input.p + n, pty->input.i - n);
  pty->input.i -= n;
  return n;
}

static char *PtyEncodeRgb(char *p, int rgb) {
  return p + sprintf(p, "2;%u;%u;%u", (rgb & 0x0000ff) >> 000,
                     (rgb & 0x00ff00) >> 010, (rgb & 0xff0000) >> 020);
}

static char *PtyEncodeXterm256(char *p, int xt) {
  return p + sprintf(p, "5;%u", xt);
}

char *PtyEncodeStyle(char *p, u32 xr, u32 pr, u32 fg, u32 bg) {
  *p++ = 033;
  *p++ = '[';
  if (pr & (kPtyBold | kPtyFaint | kPtyFlip | kPtyUnder | kPtyDunder |
            kPtyBlink | kPtyStrike | kPtyFg | kPtyBg)) {
    if (xr & (kPtyBold | kPtyFaint)) {
      if ((xr & (kPtyBold | kPtyFaint)) ^ (pr & (kPtyBold | kPtyFaint))) {
        *p++ = '2';
        *p++ = '2';
        *p++ = ';';
      }
      if (pr & kPtyBold) {
        *p++ = '1';
        *p++ = ';';
      }
      if (pr & kPtyFaint) {
        *p++ = '2';
        *p++ = ';';
      }
    }
    if (xr & (kPtyUnder | kPtyDunder)) {
      if ((xr & (kPtyUnder | kPtyDunder)) ^ (pr & (kPtyUnder | kPtyDunder))) {
        *p++ = '2';
        *p++ = '4';
        *p++ = ';';
      }
      if (pr & kPtyUnder) {
        *p++ = '4';
        *p++ = ';';
      }
      if (pr & kPtyDunder) {
        *p++ = '2';
        *p++ = '1';
        *p++ = ';';
      }
    }
    if (xr & (kPtyFlip | kPtyBlink | kPtyStrike)) {
      if (xr & kPtyFlip) {
        if (!(pr & kPtyFlip)) *p++ = '2';
        *p++ = '7';
        *p++ = ';';
      }
      if (xr & kPtyBlink) {
        if (!(pr & kPtyBlink)) *p++ = '2';
        *p++ = '5';
        *p++ = ';';
      }
      if (xr & kPtyStrike) {
        if (!(pr & kPtyStrike)) *p++ = '2';
        *p++ = '9';
        *p++ = ';';
      }
    }
    if (xr & (kPtyFg | kPtyTrue)) {
      *p++ = '3';
      if (pr & kPtyFg) {
        *p++ = '8';
        *p++ = ';';
        if (pr & kPtyTrue) {
          p = PtyEncodeRgb(p, fg);
        } else {
          p = PtyEncodeXterm256(p, fg);
        }
      } else {
        *p++ = '9';
      }
      *p++ = ';';
    }
    if (xr & (kPtyBg | kPtyTrue)) {
      *p++ = '4';
      if (pr & kPtyBg) {
        *p++ = '8';
        *p++ = ';';
        if (pr & kPtyTrue) {
          p = PtyEncodeRgb(p, bg);
        } else {
          p = PtyEncodeXterm256(p, bg);
        }
      } else {
        *p++ = '9';
      }
      *p++ = ';';
    }
    unassert(';' == p[-1]);
    p[-1] = 'm';
  } else {
    *p++ = '0';
    *p++ = 'm';
  }
  return p;
}

int PtyAppendLine(struct Pty *pty, struct Buffer *buf, unsigned y) {
  u64 u;
  char *p, *pb;
  u32 i, j, n, wc, np, xp, pr, fg, bg, ci;
  int w;
  if (y >= pty->yn) {
    errno = EINVAL;
    return -1;
  }
  n = buf->i + pty->xn * 60; /* torture character length */
  if (n > buf->n) {
    if (!(p = (char *)realloc(buf->p, n))) return -1;
    buf->p = p;
    buf->n = n;
  }
  i = y * pty->xn;
  j = (y + 1) * pty->xn;
  pb = buf->p + buf->i;
  ci = !(pty->conf & kPtyNocursor) && y == pty->y ? i + pty->x : -1;
  for (pr = 0; i < j; i += w) {
    np = pty->prs[i];
    if (!(np & kPtyConceal)) {
      wc = pty->wcs[i];
      unassert(!(0x00 <= wc && wc <= 0x1F));
      unassert(!(0x7F <= wc && wc <= 0x9F));
      if (0x20 <= wc && wc <= 0x7E) {
        u = wc;
        w = 1;
      } else {
        u = tpenc(wc);
        w = wcwidth(wc);
        w = MAX(1, w);
      }
    } else {
      u = ' ';
      w = 1;
    }
    if (i == ci) {
      if (u != ' ') {
        np ^= kPtyFlip;
      } else {
        u = tpenc(CURSOR);
        if (pty->conf & kPtyBlinkcursor) {
          np |= kPtyBlink;
        }
      }
    }
    fg = bg = -1;
    xp = pr ^ np;
    if (np & (kPtyFg | kPtyBg)) {
      if (np & kPtyFg) {
        if (pty->fgs[i] != fg) xp |= kPtyFg;
        fg = pty->fgs[i];
      }
      if (np & kPtyBg) {
        if (pty->bgs[i] != bg) xp |= kPtyBg;
        bg = pty->bgs[i];
      }
    }
    p = pb;
    if (xp) {
      pr = np;
      p = PtyEncodeStyle(p, xp, pr, fg, bg);
    }
    do {
      *p++ = u & 0xFF;
      u >>= 8;
    } while (u);
    unassert(p - pb <= 60);
    pb = p;
  }
  unassert(pb - buf->p <= buf->n);
  buf->i = pb - buf->p;
  return 0;
}
