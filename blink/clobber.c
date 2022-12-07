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
#include "blink/flags.h"
#include "blink/machine.h"
#include "blink/modrm.h"

int GetClobbers(u64 rde) {
  switch (Mopcode(rde)) {
    default:
      return 0;
    case 0x000:  // add OpAlub
    case 0x001:  // add OpAluw
    case 0x002:  // add OpAlubFlip
    case 0x003:  // add OpAluwFlip
    case 0x004:  // add OpAluAlIbAdd
    case 0x005:  // add OpAluRaxIvds
    case 0x008:  // or  OpAlub
    case 0x009:  // or  OpAluw
    case 0x00A:  // or  OpAlubFlip
    case 0x00B:  // or  OpAluwFlip
    case 0x00C:  // or  OpAluAlIbOr
    case 0x00D:  // or  OpAluRaxIvds
    case 0x010:  // adc OpAlub
    case 0x011:  // adc OpAluw
    case 0x012:  // adc OpAlubFlip
    case 0x013:  // adc OpAluwFlip
    case 0x014:  // adc OpAluAlIbAdc
    case 0x015:  // adc OpAluRaxIvds
    case 0x018:  // sbb OpAlub
    case 0x019:  // sbb OpAluw
    case 0x01A:  // sbb OpAlubFlip
    case 0x01B:  // sbb OpAluwFlip
    case 0x01C:  // sbb OpAluAlIbSbb
    case 0x01D:  // sbb OpAluRaxIvds
    case 0x020:  // and OpAlub
    case 0x021:  // and OpAluw
    case 0x022:  // and OpAlubFlip
    case 0x023:  // and OpAluwFlip
    case 0x024:  // and OpAluAlIbAnd
    case 0x025:  // and OpAluRaxIvds
    case 0x028:  // sub OpAlub
    case 0x029:  // sub OpAluw
    case 0x02A:  // sub OpAlubFlip
    case 0x02B:  // sub OpAluwFlip
    case 0x02C:  // sub OpAluAlIbSub
    case 0x02D:  // sub OpAluRaxIvds
    case 0x030:  // xor OpAlub
    case 0x031:  // xor OpAluw
    case 0x032:  // xor OpAlubFlip
    case 0x033:  // xor OpAluwFlip
    case 0x034:  // xor OpAluAlIbXor
    case 0x035:  // xor OpAluRaxIvds
    case 0x038:  // cmp OpAlubCmp
    case 0x039:  // cmp OpAluwCmp
    case 0x03A:  // cmp OpAlubFlipCmp
    case 0x03B:  // cmp OpAluwFlipCmp
    case 0x03C:  // cmp OpCmpAlIb
    case 0x03D:  // cmp OpCmpRaxIvds
    case 0x080:  // OpAlubiReg
    case 0x081:  // OpAluwiReg
    case 0x082:  // OpAlubiReg
    case 0x083:  // OpAluwiReg
    case 0x084:  // OpAlubTest
    case 0x085:  // OpAluwTest
    case 0x0A6:  // OpCmps
    case 0x0A7:  // OpCmps
    case 0x0A8:  // OpTestAlIb
    case 0x0A9:  // OpTestRaxIvds
    case 0x0AE:  // OpScas
    case 0x0AF:  // OpScas
    case 0x069:  // OpImulGvqpEvqpImm
    case 0x06B:  // OpImulGvqpEvqpImm
    case 0x1AF:  // OpImulGvqpEvqp
    case 0x0C0:  // OpBsubiImm
    case 0x0C1:  // OpBsuwiImm
    case 0x0D0:  // OpBsubi1
    case 0x0D1:  // OpBsuwi1
    case 0x0D2:  // OpBsubiCl
    case 0x0D3:  // OpBsuwiCl
    case 0x12E:  // OpComissVsWs
    case 0x12F:  // OpComissVsWs
    case 0x1A4:  // OpDoubleShift
    case 0x1A5:  // OpDoubleShift
    case 0x1AC:  // OpDoubleShift
    case 0x1AD:  // OpDoubleShift
    case 0x1B0:  // OpCmpxchgEbAlGb
    case 0x1B1:  // OpCmpxchgEvqpRaxGvqp
    case 0x1BC:  // OpBsf
    case 0x1BD:  // OpBsr
    case 0x1C0:  // OpXaddEbGb
    case 0x1C1:  // OpXaddEvqpGvqp
      return FLAGS_CF | FLAGS_ZF | FLAGS_SF | FLAGS_OF | FLAGS_AF | FLAGS_PF;
    case 0x09E:  // OpSahf
      return FLAGS_CF | FLAGS_ZF | FLAGS_SF | FLAGS_AF | FLAGS_PF;
    case 0x0DB:  // OpFpu
    case 0x0DF:  // OpFpu
      if (ModrmReg(rde) == 5 || ModrmReg(rde) == 6) {
        return FLAGS_OF | FLAGS_SF | FLAGS_AF;
      } else {
        return 0;
      }
    case 0x0F5:  // OpCmc
    case 0x0F8:  // OpClc
    case 0x0F9:  // OpStc
      return FLAGS_CF;
    case 0x0F6:  // Op0f6
    case 0x0F7:  // Op0f7
      if (ModrmReg(rde) != 2) {
        return FLAGS_CF | FLAGS_ZF | FLAGS_SF | FLAGS_OF | FLAGS_AF | FLAGS_PF;
      } else {
        return 0;
      }
    case 0x0FE:  // Op0fe
    case 0x0FF:  // Op0ff
      if (ModrmReg(rde) < 2) {
        return FLAGS_ZF | FLAGS_SF | FLAGS_OF | FLAGS_AF | FLAGS_PF;
      } else {
        return 0;
      }
    case 0x1A3:  // OpBit bt
    case 0x1AB:  // OpBit bts
    case 0x1B3:  // OpBit btr
    case 0x1BA:  // OpBit
    case 0x1BB:  // OpBit
      return FLAGS_CF | FLAGS_SF | FLAGS_OF | FLAGS_AF | FLAGS_PF;
  }
}
