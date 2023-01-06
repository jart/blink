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
#include "blink/builtin.h"
#include "blink/debug.h"
#include "blink/flags.h"
#include "blink/log.h"
#include "blink/modrm.h"
#include "blink/stats.h"

bool GetParity(u8 b) {
  b ^= b >> 4;
  b ^= b >> 2;
  b ^= b >> 1;
  return ~b & 1;
}

void ImportFlags(struct Machine *m, u64 flags) {
  u64 mask = 0;
  mask |= 1 << FLAGS_CF;
  mask |= 1 << FLAGS_PF;
  mask |= 1 << FLAGS_AF;
  mask |= 1 << FLAGS_ZF;
  mask |= 1 << FLAGS_SF;
  mask |= 1 << FLAGS_TF;
  mask |= 1 << FLAGS_IF;
  mask |= 1 << FLAGS_DF;
  mask |= 1 << FLAGS_OF;
  mask |= 1 << FLAGS_NT;
  mask |= 1 << FLAGS_AC;
  mask |= 1 << FLAGS_ID;
  m->flags = (flags & mask) | (m->flags & ~mask);
  m->flags = SetFlag(m->flags, FLAGS_RF, false);
  m->flags = SetLazyParityByte(m->flags, !((m->flags >> FLAGS_PF) & 1));
}

u64 ExportFlags(u64 flags) {
  flags = SetFlag(flags, FLAGS_IOPL, 3);
  flags = SetFlag(flags, FLAGS_F1, true);
  flags = SetFlag(flags, FLAGS_F0, false);
  flags = flags & ~(1ull << FLAGS_PF);
  flags |= GetLazyParityBool(flags) << FLAGS_PF;
  return flags;
}

static bool IsJump(u64 rde) {
  int op = Mopcode(rde);
  return op == 0x0E9 ||  // jmp  Jvds
         op == 0x0EB ||  // jmp  Jbs
         op == 0x0E8;    // call Jvds
}

static bool IsConditionalJump(u64 rde) {
  int op = Mopcode(rde);
  return (0x070 <= op && op <= 0x07F) ||  // Jcc Jbs
         (0x180 <= op && op <= 0x18F);    // Jcc Jvds
}

static int CrawlFlags(struct Machine *m,  //
                      i64 pc,             //
                      int myflags,        //
                      int look,           //
                      int depth) {
  int need = 0;
  for (;;) {
    SPX_LOGF("%" PRIx64 " %*s%s", pc, depth * 2, "", DescribeOp(m, pc));
    if (LoadInstruction2(m, pc)) return -1;
    pc += Oplength(m->xedd->op.rde);
    need |= GetFlagDeps(m->xedd->op.rde) & myflags;
    if (!(myflags &= ~GetFlagClobbers(m->xedd->op.rde))) {
      return need;
    } else if (!--look) {
      return -1;
    } else if (IsJump(m->xedd->op.rde)) {
      pc += m->xedd->op.disp;
    } else if (IsConditionalJump(m->xedd->op.rde)) {
      need |= CrawlFlags(m, pc + m->xedd->op.disp, myflags, look, depth + 1);
      if (need == -1) return -1;
    } else if (ClassifyOp(m->xedd->op.rde) != kOpNormal) {
      return -1;
    }
  }
}

// returns bitset of flags read by code at pc, or -1 if unknown
int GetNeededFlags(struct Machine *m, i64 pc, int myflags) {
  return CrawlFlags(m, pc, myflags, 16, 0);
}

// returns bitset of flags set or undefined by operation
int GetFlagClobbers(u64 rde) {
  switch (Mopcode(rde)) {
    default:
      return 0;
    case 0x000:  // add byte
    case 0x001:  // add word
    case 0x002:  // add byte flip
    case 0x003:  // add word flip
    case 0x004:  // add %al  $ib
    case 0x005:  // add %rax $ivds
    case 0x008:  // or  byte
    case 0x009:  // or  word
    case 0x00A:  // or  byte flip
    case 0x00B:  // or  word flip
    case 0x00C:  // or  %al  $ib
    case 0x00D:  // or  %rax $ivds
    case 0x010:  // adc byte
    case 0x011:  // adc word
    case 0x012:  // adc byte flip
    case 0x013:  // adc word flip
    case 0x014:  // adc %al  $ib
    case 0x015:  // adc %rax $ivds
    case 0x018:  // sbb byte
    case 0x019:  // sbb word
    case 0x01A:  // sbb byte flip
    case 0x01B:  // sbb word flip
    case 0x01C:  // sbb %al  $ib
    case 0x01D:  // sbb %rax $ivds
    case 0x020:  // and byte
    case 0x021:  // and word
    case 0x022:  // and byte flip
    case 0x023:  // and word flip
    case 0x024:  // and %al  $ib
    case 0x025:  // and %rax $ivds
    case 0x028:  // sub byte
    case 0x029:  // sub word
    case 0x02A:  // sub byte flip
    case 0x02B:  // sub word flip
    case 0x02C:  // sub %al  $ib
    case 0x02D:  // sub %rax $ivds
    case 0x030:  // xor byte
    case 0x031:  // xor word
    case 0x032:  // xor byte flip
    case 0x033:  // xor word flip
    case 0x034:  // xor %al  $ib
    case 0x035:  // xor %rax $ivds
    case 0x038:  // cmp byte
    case 0x039:  // cmp word
    case 0x03A:  // cmp byte flip
    case 0x03B:  // cmp word flip
    case 0x03C:  // cmp %al  $ib
    case 0x03D:  // cmp %rax $ivds
    case 0x080:  // alubireg
    case 0x081:  // aluwireg
    case 0x082:  // alubireg
    case 0x083:  // aluwireg
    case 0x084:  // alubtest
    case 0x085:  // aluwtest
    case 0x0A6:  // cmps
    case 0x0A7:  // cmps
    case 0x0A8:  // test %al  $ib
    case 0x0A9:  // test %rax $ivds
    case 0x0AE:  // scas
    case 0x0AF:  // scas
    case 0x069:  // imul
    case 0x06B:  // Imul
    case 0x1AF:  // imul
    case 0x12E:  // comisd
    case 0x12F:  // comisd
    case 0x1A4:  // shld $ib
    case 0x1A5:  // shld %cl
    case 0x1AC:  // shrd $ib
    case 0x1AD:  // shrd %cl
    case 0x1B0:  // cmpxchg byte
    case 0x1B1:  // cmpxchg word
    case 0x1BC:  // bsf
    case 0x1BD:  // bsr
    case 0x1C0:  // xadd byte
    case 0x1C1:  // xadd word
    case 0x02F:  // das
    case 0x037:  // aaa
    case 0x03F:  // aas
    case 0x0D5:  // aad
      return CF | ZF | SF | OF | AF | PF;
    case 0x0C0:  // bsu $ib byte
    case 0x0C1:  // bsu $ib word
    case 0x0D0:  // bsu $1  byte
    case 0x0D1:  // bsu $1  word
    case 0x0D2:  // bsu %cl byte
    case 0x0D3:  // bsu %cl word
      switch (ModrmReg(rde)) {
        case 0:  // rol
        case 1:  // ror
        case 2:  // rcl
        case 3:  // rcr
          return OF | CF;
        case 4:  // shl
        case 5:  // shr
        case 6:  // sal
        case 7:  // sar
          return CF | ZF | SF | OF | AF | PF;
        default:
          __builtin_unreachable();
      }
    case 0x0DB:  // fpu
    case 0x0DF:  // fpu
      if (ModrmReg(rde) == 5 || ModrmReg(rde) == 6) {
        return OF | SF | AF;
      } else {
        return 0;
      }
    case 0x0F5:  // cmc
    case 0x0F8:  // clc
    case 0x0F9:  // stc
      return CF;
    case 0x0F6:  // 0f6
    case 0x0F7:  // 0f7
      switch (ModrmReg(rde)) {
        case 2:  // not
          return 0;
        case 0:  // test
        case 1:  // test
        case 3:  // neg
        case 4:  // mul
        case 5:  // imul
        case 6:  // div
        case 7:  // idiv
          return CF | ZF | SF | OF | AF | PF;
        default:
          __builtin_unreachable();
      }
    case 0x0FE:
    case 0x0FF:
      switch (ModrmReg(rde)) {
        case 0:  // inc
        case 1:  // dec
          return ZF | SF | OF | AF | PF;
        default:  // call, callf, jmp, jmpf, push
          return 0;
      }
    case 0x1A3:  // bit bt
    case 0x1AB:  // bit bts
    case 0x1B3:  // bit btr
    case 0x1BA:  // bit
    case 0x1BB:  // bit
      return CF | SF | OF | AF | PF;
    case 0x09E:  // sahf
      return CF | ZF | SF | AF | PF;
    case 0x09D:  // popf
      return 0x00ffffff;
    case 0x2f5:
      if (Rep(rde)) {
        return 0;  // pdep, pext
      } else if (!Osz(rde)) {
        return CF | ZF | SF | OF | AF | PF;  // bzhi
      } else {
        return 0;
      }
    case 0x2f6:
      if (Osz(rde)) {
        return CF;  // adcx
      } else if (Rep(rde) == 3) {
        return OF;  // adox
      } else {
        return 0;
      }
  }
}

static int GetFlagDepsImpl(u64 rde) {
  switch (Mopcode(rde)) {
    default:
      return 0;
    case 0x010:  // adc byte
    case 0x011:  // adc word
    case 0x012:  // adc byte flip
    case 0x013:  // adc word flip
    case 0x014:  // adc %al  $ib
    case 0x015:  // adc %rax $ivds
    case 0x018:  // sbb byte
    case 0x019:  // sbb word
    case 0x01A:  // sbb byte flip
    case 0x01B:  // sbb word flip
    case 0x01C:  // sbb %al  $ib
    case 0x01D:  // sbb %rax $ivds
    case 0x072:  // jb
    case 0x073:  // jae
    case 0x182:  // jb
    case 0x183:  // jae
    case 0x192:  // setb
    case 0x193:  // setae
    case 0x0D6:  // salc
    case 0x0F5:  // cmc
      return CF;
    case 0x070:  // jo
    case 0x071:  // jno
    case 0x140:  // cmovo
    case 0x141:  // cmovno
    case 0x180:  // jo
    case 0x181:  // jno
    case 0x190:  // seto
    case 0x191:  // setno
    case 0x0CE:  // into
      return OF;
    case 0x074:  // je
    case 0x075:  // jne
    case 0x144:  // cmove
    case 0x145:  // cmovne
    case 0x184:  // je
    case 0x185:  // jne
    case 0x194:  // sete
    case 0x195:  // setne
    case 0x0E0:  // loopne
    case 0x0E1:  // loope
      return ZF;
    case 0x076:  // jbe
    case 0x077:  // ja
    case 0x146:  // cmovbe
    case 0x147:  // cmova
    case 0x186:  // jbe
    case 0x187:  // ja
    case 0x196:  // setbe
    case 0x197:  // seta
      return CF | ZF;
    case 0x078:  // js
    case 0x079:  // jns
    case 0x148:  // cmovs
    case 0x149:  // cmovns
    case 0x188:  // js
    case 0x189:  // jns
    case 0x198:  // sets
    case 0x199:  // setns
      return SF;
    case 0x07A:  // jp
    case 0x07B:  // jnp
    case 0x14A:  // cmovp
    case 0x14B:  // cmovnp
    case 0x18A:  // jp
    case 0x18B:  // jnp
    case 0x19A:  // setp
    case 0x19B:  // setnp
      return PF;
    case 0x07C:  // jl
    case 0x07D:  // jge
    case 0x14C:  // cmovl
    case 0x14D:  // cmovge
    case 0x18C:  // jl
    case 0x18D:  // jge
    case 0x19C:  // setl
    case 0x19D:  // setge
      return OF | SF;
    case 0x07E:  // jle
    case 0x07F:  // jg
    case 0x14E:  // cmovle
    case 0x14F:  // cmovg
    case 0x18E:  // jle
    case 0x18F:  // jg
    case 0x19E:  // setle
    case 0x19F:  // setg
      return OF | SF | ZF;
    case 0x080:  // alu byte $ib
    case 0x081:  // alu word $iw
    case 0x082:  // alu byte $ib
    case 0x083:  // alu word $ibs
    case 0x0C0:  // bsu byte $ib
    case 0x0C1:  // bsu word $ib
    case 0x0D0:  // bsu byte $1
    case 0x0D1:  // bsu word $1
    case 0x0D2:  // bsu byte %cl
    case 0x0D3:  // bsu word %cl
      switch (ModrmReg(rde)) {
        case 2:  // adc, rcl
        case 3:  // sbb, rcr
          return CF;
        default:
          return 0;
      }
    case 0x0DA:  // fpu
    case 0x0DB:  // fpu
      switch (ModrmReg(rde)) {
        case 0:  // fcmovb
          return CF;
        case 1:  // fcmove
        case 2:  // fcmovbe
          return ZF;
        case 3:  // fcmovu
          return PF;
        default:
          return 0;
      }
    case 0x09F:  // lahf
      return CF | ZF | SF | AF | PF;
    case 0x02F:  // das
    case 0x037:  // aaa
      return CF | AF;
    case 0x09C:  // pushf
      return 0x00ffffff;
    case 0x2f6:
      if (Osz(rde)) {
        return CF;  // adcx
      } else if (Rep(rde) == 3) {
        return OF;  // adox
      } else {
        return 0;
      }
  }
}

// returns bitset of flags read by operation
int GetFlagDeps(u64 rde) {
  int flags = GetFlagDepsImpl(rde);
  if (Rep(rde) >= 2) flags |= ZF;
  return flags;
}
