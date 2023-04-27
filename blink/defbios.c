/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ This is free and unencumbered software released into the public domain.      │
│                                                                              │
│ Anyone is free to copy, modify, publish, use, compile, sell, or              │
│ distribute this software, either in source code form or as a compiled        │
│ binary, for any purpose, commercial or non-commercial, and by any            │
│ means.                                                                       │
│                                                                              │
│ In jurisdictions that recognize copyright laws, the author or authors        │
│ of this software dedicate any and all copyright interest in the              │
│ software to the public domain. We make this dedication for the benefit       │
│ of the public at large and to the detriment of our heirs and                 │
│ successors. We intend this dedication to be an overt act of                  │
│ relinquishment in perpetuity of all present and future rights to this        │
│ software under copyright law.                                                │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,              │
│ EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF           │
│ MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.       │
│ IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR            │
│ OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,        │
│ ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR        │
│ OTHER DEALINGS IN THE SOFTWARE.                                              │
╚─────────────────────────────────────────────────────────────────────────────*/
#include "blink/defbios.h"

#include <string.h>

#include "blink/endian.h"
#include "blink/macros.h"

#define kBiosArrayBase  ROUNDDOWN(kBiosEntry - (0x1D + 8) * 4, 0x10)
#define kBiosDefInt0x00 kBiosArrayBase
#define kBiosDefInt0x01 (kBiosDefInt0x00 + 4)
#define kBiosDefInt0x02 (kBiosDefInt0x01 + 4)
#define kBiosDefInt0x03 (kBiosDefInt0x02 + 4)
#define kBiosDefInt0x04 (kBiosDefInt0x03 + 4)
#define kBiosDefInt0x05 (kBiosDefInt0x04 + 4)
#define kBiosDefInt0x06 (kBiosDefInt0x05 + 4)
#define kBiosDefInt0x07 (kBiosDefInt0x06 + 4)
#define kBiosDefInt0x08 (kBiosDefInt0x07 + 4)
#define kBiosDefInt0x09 (kBiosDefInt0x08 + 4)
#define kBiosDefInt0x0A (kBiosDefInt0x09 + 4)
#define kBiosDefInt0x0B (kBiosDefInt0x0A + 4)
#define kBiosDefInt0x0C (kBiosDefInt0x0B + 4)
#define kBiosDefInt0x0D (kBiosDefInt0x0C + 4)
#define kBiosDefInt0x0E (kBiosDefInt0x0D + 4)
#define kBiosDefInt0x0F (kBiosDefInt0x0E + 4)
#define kBiosDefInt0x10 (kBiosDefInt0x0F + 4)
#define kBiosDefInt0x11 (kBiosDefInt0x10 + 4)
#define kBiosDefInt0x12 (kBiosDefInt0x11 + 4)
#define kBiosDefInt0x13 (kBiosDefInt0x12 + 4)
#define kBiosDefInt0x14 (kBiosDefInt0x13 + 4)
#define kBiosDefInt0x15 (kBiosDefInt0x14 + 4)
#define kBiosDefInt0x16 (kBiosDefInt0x15 + 4)
#define kBiosDefInt0x17 (kBiosDefInt0x16 + 4)
#define kBiosDefInt0x18 (kBiosDefInt0x17 + 4)
#define kBiosDefInt0x19 (kBiosDefInt0x18 + 4)
#define kBiosDefInt0x1A (kBiosDefInt0x19 + 4)
#define kBiosDefInt0x1B (kBiosDefInt0x1A + 4)
#define kBiosDefInt0x1C (kBiosDefInt0x1B + 4)
#define kBiosDefInt0x70 (kBiosDefInt0x1C + 4)
#define kBiosDefInt0x71 (kBiosDefInt0x70 + 4)
#define kBiosDefInt0x72 (kBiosDefInt0x71 + 4)
#define kBiosDefInt0x73 (kBiosDefInt0x72 + 4)
#define kBiosDefInt0x74 (kBiosDefInt0x73 + 4)
#define kBiosDefInt0x75 (kBiosDefInt0x74 + 4)
#define kBiosDefInt0x76 (kBiosDefInt0x75 + 4)
#define kBiosDefInt0x77 (kBiosDefInt0x76 + 4)

static const u8 defbios[] = {[kBiosDefInt0x00 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0067,  // hvtailcall 0x00
                             [kBiosDefInt0x01 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x01,  // hvtailcall 0x01
                             [kBiosDefInt0x02 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x02,  // hvtailcall 0x02
                             [kBiosDefInt0x03 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x03,  // hvtailcall 0x03
                             [kBiosDefInt0x04 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x04,  // hvtailcall 0x04
                             [kBiosDefInt0x05 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x05,  // hvtailcall 0x05
                             [kBiosDefInt0x06 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x06,  // hvtailcall 0x06
                             [kBiosDefInt0x07 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x07,  // hvtailcall 0x07
                             [kBiosDefInt0x08 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x08,  // hvtailcall 0x08
                             [kBiosDefInt0x09 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x09,  // hvtailcall 0x09
                             [kBiosDefInt0x0A - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x0A,  // hvtailcall 0x0A
                             [kBiosDefInt0x0B - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x0B,  // hvtailcall 0x0B
                             [kBiosDefInt0x0C - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x0C,  // hvtailcall 0x0C
                             [kBiosDefInt0x0D - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x0D,  // hvtailcall 0x0D
                             [kBiosDefInt0x0E - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x0E,  // hvtailcall 0x0E
                             [kBiosDefInt0x0F - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x0F,  // hvtailcall 0x0F
                             [kBiosDefInt0x10 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x10,  // hvtailcall 0x10
                             [kBiosDefInt0x11 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x11,  // hvtailcall 0x11
                             [kBiosDefInt0x12 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x12,  // hvtailcall 0x12
                             [kBiosDefInt0x13 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x13,  // hvtailcall 0x13
                             [kBiosDefInt0x14 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x14,  // hvtailcall 0x14
                             [kBiosDefInt0x15 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x15,  // hvtailcall 0x15
                             [kBiosDefInt0x16 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x16,  // hvtailcall 0x16
                             [kBiosDefInt0x17 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x17,  // hvtailcall 0x17
                             [kBiosDefInt0x18 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x18,  // hvtailcall 0x18
                             [kBiosDefInt0x19 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x19,  // hvtailcall 0x19
                             [kBiosDefInt0x1A - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x1A,  // hvtailcall 0x1A
                             [kBiosDefInt0x1B - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x1B,  // hvtailcall 0x1B
                             [kBiosDefInt0x1C - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x1C,  // hvtailcall 0x1C
                             [kBiosDefInt0x70 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x70,  // hvtailcall 0x70
                             [kBiosDefInt0x71 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x71,  // hvtailcall 0x71
                             [kBiosDefInt0x72 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x72,  // hvtailcall 0x72
                             [kBiosDefInt0x73 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x73,  // hvtailcall 0x73
                             [kBiosDefInt0x74 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x74,  // hvtailcall 0x74
                             [kBiosDefInt0x75 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x75,  // hvtailcall 0x75
                             [kBiosDefInt0x76 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x76,  // hvtailcall 0x76
                             [kBiosDefInt0x77 - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0167,
                             0x77,  // hvtailcall 0x77
                             [kBiosEntry - kBiosArrayBase] = 0x0F,
                             0xFF,
                             0177,
                             0x19,  // hvcall 0x19
                             0xEB,
                             0xFA,  // jmp .-4
                             [kBiosEnd - 1 - kBiosArrayBase] = 0x00};

void LoadDefaultBios(struct Machine *m) {
  size_t kBiosSize = sizeof(defbios);
  memcpy(m->system->real + kBiosEnd - kBiosSize, defbios, kBiosSize);
  m->cs.sel = kBiosSeg;
  m->cs.base = kBiosBase;
  m->ip = kBiosEntry - kBiosBase;
}

void SetDefaultBiosIntVectors(struct Machine *m) {
  struct System *s = m->system;
  s->idt_base = 0;
  s->idt_limit = 0x100 * 4 - 1;
  memset(s->real + 0x1D * 4, 0, (0x100 - 0x1D) * 4);
  Put32(s->real + 0x00 * 4, kBiosSeg << 16 | (kBiosDefInt0x00 - kBiosBase));
  Put32(s->real + 0x01 * 4, kBiosSeg << 16 | (kBiosDefInt0x01 - kBiosBase));
  Put32(s->real + 0x02 * 4, kBiosSeg << 16 | (kBiosDefInt0x02 - kBiosBase));
  Put32(s->real + 0x03 * 4, kBiosSeg << 16 | (kBiosDefInt0x03 - kBiosBase));
  Put32(s->real + 0x04 * 4, kBiosSeg << 16 | (kBiosDefInt0x04 - kBiosBase));
  Put32(s->real + 0x05 * 4, kBiosSeg << 16 | (kBiosDefInt0x05 - kBiosBase));
  Put32(s->real + 0x06 * 4, kBiosSeg << 16 | (kBiosDefInt0x06 - kBiosBase));
  Put32(s->real + 0x07 * 4, kBiosSeg << 16 | (kBiosDefInt0x07 - kBiosBase));
  Put32(s->real + 0x08 * 4, kBiosSeg << 16 | (kBiosDefInt0x08 - kBiosBase));
  Put32(s->real + 0x09 * 4, kBiosSeg << 16 | (kBiosDefInt0x09 - kBiosBase));
  Put32(s->real + 0x0A * 4, kBiosSeg << 16 | (kBiosDefInt0x0A - kBiosBase));
  Put32(s->real + 0x0B * 4, kBiosSeg << 16 | (kBiosDefInt0x0B - kBiosBase));
  Put32(s->real + 0x0C * 4, kBiosSeg << 16 | (kBiosDefInt0x0C - kBiosBase));
  Put32(s->real + 0x0D * 4, kBiosSeg << 16 | (kBiosDefInt0x0D - kBiosBase));
  Put32(s->real + 0x0E * 4, kBiosSeg << 16 | (kBiosDefInt0x0E - kBiosBase));
  Put32(s->real + 0x0F * 4, kBiosSeg << 16 | (kBiosDefInt0x0F - kBiosBase));
  Put32(s->real + 0x10 * 4, kBiosSeg << 16 | (kBiosDefInt0x10 - kBiosBase));
  Put32(s->real + 0x11 * 4, kBiosSeg << 16 | (kBiosDefInt0x11 - kBiosBase));
  Put32(s->real + 0x12 * 4, kBiosSeg << 16 | (kBiosDefInt0x12 - kBiosBase));
  Put32(s->real + 0x13 * 4, kBiosSeg << 16 | (kBiosDefInt0x13 - kBiosBase));
  Put32(s->real + 0x14 * 4, kBiosSeg << 16 | (kBiosDefInt0x14 - kBiosBase));
  Put32(s->real + 0x15 * 4, kBiosSeg << 16 | (kBiosDefInt0x15 - kBiosBase));
  Put32(s->real + 0x16 * 4, kBiosSeg << 16 | (kBiosDefInt0x16 - kBiosBase));
  Put32(s->real + 0x17 * 4, kBiosSeg << 16 | (kBiosDefInt0x17 - kBiosBase));
  Put32(s->real + 0x18 * 4, kBiosSeg << 16 | (kBiosDefInt0x18 - kBiosBase));
  Put32(s->real + 0x19 * 4, kBiosSeg << 16 | (kBiosDefInt0x19 - kBiosBase));
  Put32(s->real + 0x1A * 4, kBiosSeg << 16 | (kBiosDefInt0x1A - kBiosBase));
  Put32(s->real + 0x1B * 4, kBiosSeg << 16 | (kBiosDefInt0x1B - kBiosBase));
  Put32(s->real + 0x1C * 4, kBiosSeg << 16 | (kBiosDefInt0x1C - kBiosBase));
  Put32(s->real + 0x70 * 4, kBiosSeg << 16 | (kBiosDefInt0x70 - kBiosBase));
  Put32(s->real + 0x71 * 4, kBiosSeg << 16 | (kBiosDefInt0x71 - kBiosBase));
  Put32(s->real + 0x72 * 4, kBiosSeg << 16 | (kBiosDefInt0x72 - kBiosBase));
  Put32(s->real + 0x73 * 4, kBiosSeg << 16 | (kBiosDefInt0x73 - kBiosBase));
  Put32(s->real + 0x74 * 4, kBiosSeg << 16 | (kBiosDefInt0x74 - kBiosBase));
  Put32(s->real + 0x75 * 4, kBiosSeg << 16 | (kBiosDefInt0x75 - kBiosBase));
  Put32(s->real + 0x76 * 4, kBiosSeg << 16 | (kBiosDefInt0x76 - kBiosBase));
  Put32(s->real + 0x77 * 4, kBiosSeg << 16 | (kBiosDefInt0x77 - kBiosBase));
}

void SetDefaultBiosDataArea(struct Machine *m) {
  memset(m->system->real + 0x400, 0, 0x100);
  Write16(m->system->real + 0x400, 0x3F8);
  Write16(m->system->real + 0x40E, 0xb0000 >> 4);
  Write16(m->system->real + 0x410, 1 << 0 |      // floppy drive
                                       1 << 1 |  // math coprocessor
                                       3 << 4 |  // initial video mode
                                       0 << 6 |  // no. of floppy drives - 1
                                       1 << 9);  // no. of serial devices
  Write16(m->system->real + 0x413, 0xb0000 / 1024);
  Write16(m->system->real + 0x44A, 80);
}
