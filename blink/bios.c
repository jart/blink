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
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "blink/assert.h"
#include "blink/bda.h"
#include "blink/biosrom.h"
#include "blink/blinkenlights.h"
#include "blink/bus.h"
#include "blink/cga.h"
#include "blink/endian.h"
#include "blink/flags.h"
#include "blink/loader.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/mda.h"
#include "blink/pty.h"
#include "blink/timespec.h"
#include "blink/util.h"
#include "blink/vfs.h"

static const struct Chs {
  ssize_t imagesize;
  short c, h, s;
  bool isfloppy;
} kChs[] = {
    {163840, 40, 1, 8, true},    //
    {184320, 40, 1, 9, true},    //
    {327680, 40, 2, 8, true},    //
    {368640, 40, 2, 9, true},    //
    {737280, 80, 2, 9, true},    //
    {1228800, 80, 2, 15, true},  //
    {1474560, 80, 2, 18, true},  //
    {2949120, 80, 2, 36, true},  //
};

static off_t diskimagesize = 0;
static int diskcyls = 1023;
static int diskheads = 16;  // default to 16 heads/cylinder, following QEMU
static int disksects = 63;
static bool diskisfloppy = false;

static u64 prevday = 0;  // day number of last call to int 0x1A, ah = 0, for
                         // calculating elapsed midnight count

static size_t GetLastIndex(size_t size, unsigned unit, int i, unsigned limit) {
  unsigned q, r;
  if (!size) return 0;
  q = size / unit;
  r = size % unit;
  if (!r) --q;
  q += i;
  if (q > limit) q = limit;
  return q;
}

static void OnDiskServiceReset(void) {
  m->ah = 0x00;
  SetCarry(false);
}

static void OnDiskServiceBadCommand(void) {
  m->ah = 0x01;
  SetCarry(true);
}

static void DetermineChs(void) {
  int i;
  struct stat st;
  off_t size = diskimagesize;
  if (size) return;  // do nothing if disk geometry already detected
  unassert(!VfsStat(AT_FDCWD, m->system->elf.prog, &st, 0));
  diskimagesize = size = st.st_size;
  for (i = 0; i < ARRAYLEN(kChs); ++i) {
    if (size == kChs[i].imagesize) {
      diskcyls = kChs[i].c;
      diskheads = kChs[i].h;
      disksects = kChs[i].s;
      diskisfloppy = kChs[i].isfloppy;
      return;
    }
  }
}

static bool DetermineChsAndSanityCheck(u8 drive) {
  DetermineChs();
  if (diskisfloppy) {
    if ((drive & 0x80) != 0) {
      // reading from hard disk drive, but image is floppy image
      m->ah = 0x80;  // drive not ready
      SetCarry(true);
      return false;
    }
  } else {
    if ((drive & 0x80) == 0) {
      // reading from floppy drive, but image is hard disk image
      m->ah = 0x80;  // drive not ready
      SetCarry(true);
      return false;
    }
  }
  return true;
}

static void OnDiskServiceGetParams(void) {
  size_t lastsector, lastcylinder, lasthead;
  u8 drive = m->dl;
  if (!DetermineChsAndSanityCheck(drive)) return;
  lastcylinder =
      GetLastIndex(diskimagesize, 512 * disksects * diskheads, 0, 1023);
  lasthead = GetLastIndex(diskimagesize, 512 * disksects, 0, diskheads - 1);
  lastsector = GetLastIndex(diskimagesize, 512, 1, disksects);
  m->dl = 1;
  m->dh = lasthead;
  m->cl = lastcylinder >> 8 << 6 | lastsector;
  m->ch = lastcylinder;
  m->bl = 4;  // CMOS drive type: 1.4M floppy
  m->ah = 0;
  if ((drive & 0x80) == 0) {
    u32 ddpt = GetDefaultBiosDisketteParamTable();
    m->es.sel = ddpt >> 16;
    m->es.base = ddpt >> 16 << 4;
    Put16(m->di, (u16)ddpt);
  }
  SetCarry(false);
}

static void OnDiskServiceReadSectors(void) {
  int fd;
  i64 addr, size, rsize, ursize;
  i64 sectors, drive, head, cylinder, sector, offset;
  sectors = m->al;
  drive = m->dl;
  if (!DetermineChsAndSanityCheck(drive)) {
    m->al = 0x00;
    return;
  }
  head = m->dh;
  cylinder = (m->cl & 192) << 2 | m->ch;
  sector = (m->cl & 63) - 1;
  size = sectors * 512;
  offset = sector * 512 + head * 512 * disksects +
           cylinder * 512 * disksects * diskheads;
  ELF_LOGF("bios read sectors %" PRId64 " "
           "@ sector %" PRId64 " cylinder %" PRId64 " head %" PRId64
           " drive %" PRId64 " offset %#" PRIx64 " from %s",
           sectors, sector, cylinder, head, drive, offset, m->system->elf.prog);
  addr = m->es.base + Get16(m->bx);
  if (addr >= kRealSize || size > kRealSize || addr + size > kRealSize) {
    LOGF("bios disk read exceeded real memory");
    m->al = 0x00;
    m->ah = 0x02;  // cannot find address mark
    SetCarry(true);
    return;
  }
  errno = 0;
  if ((fd = VfsOpen(AT_FDCWD, m->system->elf.prog, O_RDONLY, 0)) != -1 &&
      (rsize = VfsPread(fd, m->system->real + addr, size, offset)) >= 0) {
    ursize = ROUNDUP(rsize, 512);
    if (ursize != rsize) {
      memset(m->system->real + addr + rsize, 0, ursize - rsize);
    }
    SetWriteAddr(m, addr, ursize);
    if (ursize == size) {
      m->ah = 0x00;  // success
      SetCarry(false);
    } else {
      sectors = ursize / 512;
      LOGF("bios read sectors: partial read %" PRId64 " sectors", sectors);
      m->al = sectors;
      m->ah = 0x04;  // sector not found
      SetCarry(true);
    }
  } else {
    LOGF("bios read sectors failed: %s", DescribeHostErrno(errno));
    m->al = 0x00;
    m->ah = 0x0d;  // invalid number of sector
    SetCarry(true);
  }
  VfsClose(fd);
}

static void OnDiskServiceProbeExtended(void) {
  u8 drive = m->dl;
  u16 magic = Get16(m->bx);
  (void)drive;
  if (magic == 0x55AA) {
    Put16(m->bx, 0xAA55);
    m->ah = 0x30;
    SetCarry(false);
  } else {
    m->ah = 0x01;
    SetCarry(true);
  }
}

static void OnDiskServiceReadSectorsExtended(void) {
  int fd;
  u8 drive = m->dl;
  i64 pkt_addr = m->ds.base + Get16(m->si), addr, sectors, size, lba, offset,
      rsize, ursize;
  u8 pkt_size, *pkt;
  SetReadAddr(m, pkt_addr, 1);
  pkt = m->system->real + pkt_addr;
  pkt_size = Get8(pkt);
  if ((pkt_size != 0x10 && pkt_size != 0x18) || Get8(pkt + 1) != 0) {
    m->ah = 0x01;
    SetCarry(true);
  } else {
    SetReadAddr(m, pkt_addr, pkt_size);
    addr = Read32(pkt + 4);
    if (addr == 0xFFFFFFFF && pkt_size == 0x18) {
      addr = Read64(pkt + 0x10);
    } else {
      addr = (addr >> 16 << 4) + (addr & 0xFFFF);
    }
    sectors = Read16(pkt + 2);
    size = sectors * 512;
    lba = Read32(pkt + 8);
    offset = lba * 512;
    ELF_LOGF("bios read sector ext "
             "lba=%" PRId64 " "
             "offset=%" PRIx64 " "
             "size=%" PRIx64,
             lba, offset, size);
    if (!DetermineChsAndSanityCheck(drive)) {
      Write16(pkt + 2, 0);
      return;
    }
    if (addr >= kRealSize || size > kRealSize || addr + size > kRealSize) {
      LOGF("bios disk read exceeded real memory");
      SetWriteAddr(m, pkt_addr + 2, 2);
      Write16(pkt + 2, 0);
      m->ah = 0x02;  // cannot find address mark
      SetCarry(true);
      return;
    }
    errno = 0;
    if ((fd = VfsOpen(AT_FDCWD, m->system->elf.prog, O_RDONLY, 0)) != -1 &&
        (rsize = VfsPread(fd, m->system->real + addr, size, offset)) >= 0) {
      ursize = ROUNDUP(rsize, 512);
      if (ursize != rsize) {
        memset(m->system->real + addr + rsize, 0, ursize - rsize);
      }
      SetWriteAddr(m, addr, ursize);
      if (ursize == size) {
        m->ah = 0x00;  // success
        SetCarry(false);
      } else {
        sectors = ursize / 512;
        LOGF("bios read sectors: partial read %" PRId64 " sectors", sectors);
        Write16(pkt + 2, sectors);
        m->ah = 0x04;  // sector not found
        SetCarry(true);
      }
    } else {
      LOGF("bios read sector failed: %s", DescribeHostErrno(errno));
      SetWriteAddr(m, pkt_addr + 2, 2);
      Write16(pkt + 2, 0);
      m->ah = 0x0d;  // invalid number of sectors
      SetCarry(true);
    }
    VfsClose(fd);
  }
}

static void OnDiskService(void) {
  switch (m->ah) {
    case 0x00:
      OnDiskServiceReset();
      break;
    case 0x02:
      OnDiskServiceReadSectors();
      break;
    case 0x08:
      OnDiskServiceGetParams();
      break;
    case 0x41:
      OnDiskServiceProbeExtended();
      break;
    case 0x42:
      OnDiskServiceReadSectorsExtended();
      break;
    default:
      OnDiskServiceBadCommand();
      break;
  }
}

#define page_offsetw()   0      // TODO(ghaerr): implement screen pages
#define video_ram()     (m->system->real + ((vidya == 7)? 0xb0000: 0xb8000))
#define ATTR_DEFAULT    0x07

/* clear screen from x1,y1 up to but not including x2, y2 */
static void VidyaServiceClearScreen(int x1, int y1, int x2, int y2, u8 attr) {
  int x, y, xn;
  u16 *vram;
  unassert(x1 >= 0 && x1 <= BdaCols);
  unassert(x2 >= 0 && x2 <= BdaCols);
  unassert(y1 >= 0 && y1 <= BdaLines);
  unassert(y2 >= 0 && y2 <= BdaLines);
  vram = (u16 *)video_ram();
  xn = BdaCols;
  for (y = y1; y < y2; y++) {
    for (x = x1; x < x2; x++) {
        vram[page_offsetw() + y * xn + x] = ' ' | (attr << 8);
    }
  }
}

/* clear line y from x1 up to and including x2 to attribute attr */
static void VidyaServiceClearLine(int x1, int x2, int y, u8 attr) {
  int x, xn;
  u16 *vram;
  unassert(x1 >= 0 && x1 < BdaCols);
  unassert(x2 >= 0 && x2 < BdaCols);
  unassert(y >= 0 && y < BdaLines);
  vram = (u16 *)video_ram();
  xn = BdaCols;
  for (x = x1; x <= x2; x++) {
    vram[page_offsetw() + y * xn + x] = ' ' | (attr << 8);
  }
}

/* scroll video ram up from line y1 up to and including line y2 */
static void VidyaServiceScrollUp(int y1, int y2, u8 attr) {
  int xn, pitch;
  u8 *vid;
  unassert(y1 >= 0 && y1 < BdaLines);
  unassert(y2 >= 0 && y2 < BdaLines);
  xn = BdaCols;
  vid = video_ram() + (page_offsetw() + y1 * xn) * 2;
  pitch = xn * 2;
  memcpy(vid, vid + pitch, (BdaLines - y1) * pitch);
  VidyaServiceClearLine(0, xn-1, y2, attr);
}

/* scroll adapter RAM down from line y1 up to and including line y2 */
static void VidyaServiceScrollDown(int y1, int y2, u8 attr)
{
  int y, xn, pitch;
  u8 *vid;
  unassert(y1 >= 0 && y1 < BdaLines);
  unassert(y2 >= 0 && y2 < BdaLines);
  xn = BdaCols;
  vid = video_ram() + (page_offsetw() + (BdaLines-1) * xn) * 2;
  pitch = xn * 2;
  y = y2;
  while (--y >= y1) {
    memcpy(vid, vid - pitch, pitch);
    vid -= pitch;
  }
  VidyaServiceClearLine(0, xn-1, y1, attr);
}

static void OnVidyaServiceScrollUp(void) {
  unsigned i, n;
  unassert(m->cl < BdaCols);
  unassert(m->ch < BdaLines);
  unassert(m->dl < BdaCols);
  unassert(m->dh < BdaLines);
  n = m->al;
  if (n > BdaLines) n = BdaLines;
  if (n == 0) {
    VidyaServiceClearScreen(m->cl, m->ch, m->dl+1, m->dh+1, m->bh);
  } else {
    for (i=n; i; i--) {
      VidyaServiceScrollUp(m->ch, m->dh, m->bh);
    }
  }
}

static void OnVidyaServiceScrollDown(void) {
  unsigned i, n;
  unassert(m->cl < BdaCols);
  unassert(m->ch < BdaLines);
  unassert(m->dl < BdaCols);
  unassert(m->dh < BdaLines);
  n = m->al;
  if (n > BdaLines) n = BdaLines;
  if (n == 0) {
    VidyaServiceClearScreen(m->cl, m->ch, m->dl+1, m->dh+1, m->bh);
  } else {
    for (i=n; i; i--) {
      VidyaServiceScrollDown(m->ch, m->dh, m->bh);
    }
  }
}

/* write char/attr to video adapter ram */
static void VidyaServiceWriteCharacter(u8 ch, u8 attr, int n, bool useattr) {
  int x, y, xn, offset;
  u8 *vram;
  x = BdaCurx;
  y = BdaCury;
  unassert(y < BdaLines);
  unassert(x < BdaCols);
  xn = BdaCols;
  vram = video_ram();
  while (n-- > 0) {
    offset = page_offsetw() + (y * xn + x) * 2;
    vram[offset] = ch;
    if (useattr) {
      vram[offset+1] = attr;
    }
    if (++x >= xn) {
      x = 0;
      y++;
    }
  }
}

/* write char only (no attribute) as teletype output */
static void VidyaServiceWriteTeletype(u8 ch) {
  int x, y, xn, idx;
  u8 attr;
  u8 *vram;
  x = BdaCurx;
  y = BdaCury;
  unassert(y < BdaLines);
  unassert(x < BdaCols);
  xn = BdaCols;
  vram = video_ram();
  switch (ch) {
  case '\r':
    x = 0;
    goto update;
  case '\n':
    goto scroll;
  case '\b':
    if (x > 0) {
      x--;
    }
    goto update;
  case '\0':
  case '\a':
    return;
  }
  vram[page_offsetw() + (y * xn + x) * 2] = ch;
  if (++x >= xn) {
    x = 0;
scroll:
    if (++y >= BdaLines) {
      y = BdaLines - 1;
      attr = vram[page_offsetw() + ((y * xn + BdaCols - 1) * 2) + 1];
      VidyaServiceScrollUp(0, BdaLines - 1, attr);
    }
  }
update:
  SetBdaCurx(x);
  SetBdaCury(y);
}

void VidyaServiceSetMode(int mode) {
  int cols, lines;
  vidya = mode;
  if (LookupAddress(m, 0xB0000)) {
    ptyisenabled = true;
    lines = 25;
    switch (mode) {
    case 0:     // CGA 40x25 16-gray
    case 1:     // CGA 40x25 16-color
        cols = 40;
        break;
    case 2:     // CGA 80x25 16-gray
    case 3:     // CGA 80x25 16-color
    case 7:     // MDA 80x25 4-gray
        cols = 80;
        break;
    default:
        unassert(mode == kModePty);
        pty->conf &= ~kPtyNocursor;
        break;
    }
    if (mode == kModePty) return;
    SetBdaVmode(mode);
    SetBdaLines(lines);   // EGA BIOS - max valid line #
    SetBdaCols(cols);
    SetBdaPagesz(cols * BdaLines * 2);
    SetBdaCurpage(0);
    SetBdaCurx(0);
    SetBdaCury(0);
    SetBdaCurstart(5);      // cursor ▂ scan lines 5..7 of 0..7
    SetBdaCurend(7);
    SetBdaCrtc(0x3D4);
    VidyaServiceClearScreen(0, 0, cols, BdaLines, ATTR_DEFAULT);
  } else {
    LOGF("maybe you forgot -r flag");
  }
}

static void OnVidyaServiceGetMode(void) {
  m->al = vidya;
  m->ah = BdaCols;
  m->bh = 0;   // page
}

static void OnVidyaServiceSetCursorType(void) {
    if (vidya == kModePty) {
      if (m->ch & 0x20) {
        pty->conf |= kPtyNocursor;
      } else {
        pty->conf &= ~kPtyNocursor;
      }
    } else {
      SetBdaCurstart(m->ch);
      SetBdaCurend(m->cl);
    }
}

static void OnVidyaServiceSetCursorPosition(void) {
  int x, y;
  x = m->dl;
  y = m->dh;
  if (vidya == kModePty) {
    PtySetX(pty, x);
    PtySetY(pty, y);
  } else {
    SetBdaCurx(x);
    SetBdaCury(y);
  }
}

static void OnVidyaServiceGetCursorPosition(void) {
  if (vidya == kModePty) {
    m->dh = pty->y;
    m->dl = pty->x;
    // cursor ▂ scan lines 5..7 of 0..7 and hidden bit 0x20
    m->ch = 5 | !!(pty->conf & kPtyNocursor) << 5;
    m->cl = 7;
  } else {
    m->dh = BdaCury;
    m->dl = BdaCurx;
    m->ch = BdaCurstart;
    m->cl = BdaCurend;
  }
}

static void OnVidyaServiceReadCharacter(void) {
  u16 *vram;
  u16 chattr;
  int x, y, xn;
  x = BdaCurx;
  y = BdaCury;
  xn = BdaCols;
  vram = (u16 *)video_ram();
  chattr = vram[page_offsetw() + y * xn + x];
  Put16(m->ax, chattr);
}

/* write character and possibly attribute, no cursor change */
static void OnVidyaServiceWriteCharacter(bool useattr) {
  int i, n;
  u64 w;
  char *p, buf[32];
  if (vidya != kModePty) {
    n = Get16(m->cx);
    unassert(n > 0 && n + BdaCurx <= BdaCols);
    unassert(BdaCury < BdaLines);
    VidyaServiceWriteCharacter(m->al, m->bl, n, useattr);
    return;
  }
  p = buf;
  p += FormatCga(m->bl, p);
  p = stpcpy(p, "\0337");
  w = tpenc(GetVidyaByte(m->al));
  do {
    *p++ = w;
  } while ((w >>= 8));
  p = stpcpy(p, "\0338");
  for (i = Get16(m->cx); i; --i) {
    PtyWrite(pty, buf, p - buf);
  }
}

static wint_t VidyaServiceXlatTeletype(u8 c) {
  switch (c) {
    case '\a':
    case '\b':
    case '\r':
    case '\n':
    case 0177:
      return c;
    default:
      return GetVidyaByte(c);
  }
}

static void OnVidyaServiceWriteTeletype(void) {
  int n;
  u64 w;
  char buf[12];
  if (!ptyisenabled) {
    ptyisenabled = true;
    ReactiveDraw();
  }
  if (vidya != kModePty) {
    VidyaServiceWriteTeletype(m->al);
    return;
  }
  n = 0 /* FormatCga(ATTR_DEFAULT, buf) */;
  w = tpenc(VidyaServiceXlatTeletype(m->al));
  do {
    buf[n++] = w;
  } while ((w >>= 8));
  PtyWrite(pty, buf, n);
}

static void OnVidyaService(void) {
  switch (m->ah) {
    case 0x00:
      VidyaServiceSetMode(m->al);
      break;
    case 0x01:
      OnVidyaServiceSetCursorType();
      break;
    case 0x02:
      OnVidyaServiceSetCursorPosition();
      break;
    case 0x03:
      OnVidyaServiceGetCursorPosition();
      break;
    case 0x06:
      OnVidyaServiceScrollUp();
      break;
    case 0x07:
      OnVidyaServiceScrollDown();
      break;
    case 0x08:
      OnVidyaServiceReadCharacter();
      break;
    case 0x09:
      OnVidyaServiceWriteCharacter(true);
      Redraw(false);
      DrawDisplayOnly();
      break;
    case 0x0A:
      OnVidyaServiceWriteCharacter(false);
      Redraw(false);
      DrawDisplayOnly();
      break;
    case 0x0E:
      OnVidyaServiceWriteTeletype();
      Redraw(false);
      DrawDisplayOnly();
      break;
    case 0x0F:
      OnVidyaServiceGetMode();
      break;
    default:
      LOGF("Unimplemented vidya service 0x%x\n", m->ah);
      break;
  }
}

static void OnSerialServiceReset(void) {
  if (Get16(m->dx) == 0) {
    m->al = 0xb0;  // following QEMU
    m->ah = 0x60;
  } else {
    m->al = 0x00;  // Ralf Brown's Interrupt List says "AX = 9E00h if
    m->ah = 0x9e;  // disconnected (ArtiCom)"
  }
}

static void OnSerialService(void) {
  switch (m->ah) {
    case 0x00:
      OnSerialServiceReset();
      break;
    default:
      m->al = 0x00;
      m->ah = 0x9e;
      break;
  }
}

/* Convert from ANSI keyboard sequence to scancode */
int AnsiToScancode(char *buf, int n)
{
    if (n >= 1 && buf[0] == 033) {
        if (buf[1] == '[') {
            if (n == 3) {                           /* xterm sequences */
                switch (buf[2]) {                   /* ESC [ A etc */
                case 'A':   return 0x48;    // kUpArrow
                case 'B':   return 0x50;    // kDownArrow
                case 'C':   return 0x4D;    // kRightArrow
                case 'D':   return 0x4B;    // kLeftArrow
                case 'F':   return 0x4F;    // kEnd
                case 'H':   return 0x47;    // kHome
                }
            } else if (n == 4 && buf[2] == '1') {   /* ESC [ 1 P etc */
                switch (buf[3]) {
                case 'P':   return 0x3B;    // kF1
                case 'Q':   return 0x3C;    // kF2
                case 'R':   return 0x3D;    // kF3
                case 'S':   return 0x3E;    // kF4
                }
            }
            if (n > 3 && buf[n-1] == '~') {         /* vt sequences */
                switch (atoi(buf+2)) {
                case 1:     return 0x47;    // kHome
                case 2:     return 0x52;    // kInsert
                case 3:     return 0x53;    // kDelete
                case 4:     return 0x4F;    // kEnd
                case 5:     return 0x49;    // kPageUp
                case 6:     return 0x51;    // kPageDown
                case 7:     return 0x47;    // kHome
                case 8:     return 0x4F;    // kEnd
                case 11:    return 0x3B;    // kF1
                case 12:    return 0x3C;    // kF2
                case 13:    return 0x3D;    // kF3
                case 14:    return 0x3E;    // kF4
                case 15:    return 0x3F;    // kF5
                case 17:    return 0x40;    // kF6
                case 18:    return 0x41;    // kF7
                case 19:    return 0x42;    // kF8
                case 20:    return 0x43;    // kF9
                case 21:    return 0x44;    // kF10
                case 23:    return 0x85;    // kF11
                case 24:    return 0x86;    // kF12
                }
            }
        }
    }
    return 0;
}

static bool savechar;   // TODO(ghaerr): implement kbd input queue
static u8   saveah;
static u8   saveal;

static void OnKeyboardServiceReadKeyPress(void) {
  uint8_t b;
  ssize_t rc;
  static char buf[32];
  static size_t pending;
  LOGF("OnKeyboardServiceReadKeyPress");
  if (savechar) {
    savechar = false;
    m->ah = saveah;
    m->al = saveal;
    return;
  }
  if (!ptyisenabled) {
    ptyisenabled = true;
    ReactiveDraw();
  }
  pty->conf |= kPtyBlinkcursor;
  if (!pending) {
    rc = ReadAnsi(ttyin, buf, sizeof(buf));
    if (rc > 0) {
      pending = rc;
    } else {
      HandleAppReadInterrupt(rc != -1 || errno != EINTR);
      return;
    }
  }
  pty->conf &= ~kPtyBlinkcursor;
  unassert((int)pending > 0 && pending < 32);
  ReactiveDraw();
  if (m->metal) {
    int r = AnsiToScancode(buf, pending);
    if (r) {
        m->al = 0;
        m->ah = r;
        pending = 0;
        return;
    }
  }
  b = buf[0];
  if (pending > 1) {
    memmove(buf, buf + 1, pending - 1);
  }
  --pending;
  if (b == 0177) b = '\b';
  m->al = b;
  m->ah = 0;
}

static void OnKeyboardServiceCheckKeyPress(void) {
  if (savechar) {
    m->ah = saveah;
    m->al = saveal;
    return;
  }
  bool b = HasPendingKeyboard();
  m->flags = SetFlag(m->flags, FLAGS_ZF, !b);   /* ZF=0 if key pressed */
  if (b) {
    OnKeyboardServiceReadKeyPress();
    savechar = true;
    saveah = m->ah;
    saveal = m->al;
  } else {
    m->ah = 0;
    m->al = 0;
  }
}

static void OnKeyboardService(void) {
  switch (m->ah) {
    case 0x00:
      OnKeyboardServiceReadKeyPress();
      break;
    case 0x01:
      OnKeyboardServiceCheckKeyPress();
      break;
    default:
      break;
  }
}

static void OnApmService(void) {
  if (Get16(m->ax) == 0x5300 && Get16(m->bx) == 0x0000) {
    Put16(m->bx, 'P' << 8 | 'M');
    SetCarry(false);
  } else if (Get16(m->ax) == 0x5301 && Get16(m->bx) == 0x0000) {
    SetCarry(false);
  } else if (Get16(m->ax) == 0x5307 && m->bl == 1 && m->cl == 3) {
    LOGF("APM SHUTDOWN");
    exit(0);
  } else {
    SetCarry(true);
  }
}

static void OnE820(void) {
  i64 addr;
  u8 p[20];
  addr = m->es.base + Get16(m->di);
  if (Get32(m->dx) == 0x534D4150 && Get32(m->cx) == 24 &&
      addr + (int)sizeof(p) <= kRealSize) {
    if (!Get32(m->bx)) {
      Store64(p + 0, 0);
      Store64(p + 8, kRealSize);
      Store32(p + 16, 1);
      memcpy(m->system->real + addr, p, sizeof(p));
      SetWriteAddr(m, addr, sizeof(p));
      Put32(m->cx, sizeof(p));
      Put32(m->bx, 1);
    } else {
      Put32(m->bx, 0);
      Put32(m->cx, 0);
    }
    Put32(m->ax, 0x534D4150);
    SetCarry(false);
  } else {
    SetCarry(true);
  }
}

static void OnInt15h(void) {
  int timeout;
  if (Get32(m->ax) == 0xE820) {
    OnE820();
  } else if (m->ah == 0x53) {
    OnApmService();
  } else if (m->ah == 0x86) {   // microsecond delay
    timeout = (((Get16(m->cx) << 16) | Get16(m->dx)) + 999) / 1000;
    poll(0, 0, timeout);
  } else {
    SetCarry(true);
  }
}

static void OnEquipmentListService(void) {
  Put16(m->ax, BdaEquip);
}

static void OnBaseMemSizeService(void) {
  Put16(m->ax, BdaMemsz);
}

static void OnPrinterService(void) {
  m->ah = 0xb0;  // "no printer": not busy, out of paper, selected
}

static void OnTimeServiceGetSystemTime(void) {
  u64 DAY_SECS = 24UL * 60 * 60;
  struct timespec now = GetTime();
  u64 currday, daytime;
  u8 midnights = 0;
  unassert(now.tv_sec >= DAY_SECS);
  currday = (u64)now.tv_sec / DAY_SECS;
  // calculate the number of midnights elapsed since the last call here
  // this will also reset the midnight count
  if (prevday != 0) {
    if (currday > prevday) midnights = currday - prevday;
  }
  prevday = currday;
  // calculate nanoseconds from day start
  daytime = (u64)now.tv_sec - currday * DAY_SECS;
  daytime = daytime * 1000000000L + now.tv_nsec;
  // calculate BIOS system timer ticks from day start
  daytime = daytime * (0x1800B0L / 80) / (DAY_SECS * 1000000000L / 80);
  Put16(m->cx, daytime >> 16);
  Put16(m->dx, daytime);
  m->al = midnights;
}

static u8 ToBcdByte(u8 binary) {
  static const u8 bcd[] = {
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x10, 0x11,
      0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23,
      0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
      0x36, 0x37, 0x38, 0x39, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
      0x48, 0x49, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
      0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x70, 0x71,
      0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x80, 0x81, 0x82, 0x83,
      0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95,
      0x96, 0x97, 0x98, 0x99};
  unassert(binary <= 99);
  return bcd[binary];
}

static void OnTimeServiceGetRtcTime(void) {
  struct timespec now = GetTime();
  struct tm tm;
  unassert(gmtime_r(&now.tv_sec, &tm));
  m->ch = ToBcdByte(tm.tm_hour);
  m->cl = ToBcdByte(tm.tm_min);
  m->dh = ToBcdByte(tm.tm_sec);
  m->dl = tm.tm_isdst;
  SetCarry(false);
}

static void OnTimeServiceGetRtcDate(void) {
  struct timespec now = GetTime();
  struct tm tm;
  unassert(gmtime_r(&now.tv_sec, &tm));
  m->ch = ToBcdByte((tm.tm_year / 100U + 19) % 100U);
  m->cl = ToBcdByte(tm.tm_year % 100U);
  m->dh = ToBcdByte(tm.tm_mon + 1U);
  m->dl = ToBcdByte(tm.tm_mday);
  SetCarry(false);
}

static void OnTimeService(void) {
  switch (m->ah) {
    case 0x00:
      OnTimeServiceGetSystemTime();
      break;
    case 0x02:
      OnTimeServiceGetRtcTime();
      break;
    case 0x04:
      OnTimeServiceGetRtcDate();
      break;
    default:
      SetCarry(true);
  }
}

bool OnCallBios(int interrupt) {
  switch (interrupt) {
    case 0x10:
      OnVidyaService();
      return true;
    case 0x13:
      OnDiskService();
      return true;
    case 0x14:
      OnSerialService();
      return true;
    case 0x15:
      OnInt15h();
      return true;
    case 0x16:
      OnKeyboardService();
      return true;
    case 0x11:
      OnEquipmentListService();
      return true;
    case 0x12:
      OnBaseMemSizeService();
      return true;
    case 0x17:
      OnPrinterService();
      return true;
    case 0x19:
      DetermineChs();
      BootProgram(m, &m->system->elf, diskisfloppy ? 0x00 : 0x80);
      VidyaServiceSetMode(vidya);
      return true;
    case 0x1A:
      OnTimeService();
      Redraw(false);
      return true;
  }
  return false;
}
