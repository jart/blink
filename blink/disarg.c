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
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/dis.h"
#include "blink/endian.h"
#include "blink/high.h"
#include "blink/macros.h"
#include "blink/modrm.h"
#include "blink/util.h"

static const char kRiz[2][4] = {"eiz", "riz"};
static const char kRip[2][4] = {"eip", "rip"};
static const char kSka[4][4] = {"", ",2", ",4", ",8"};
static const char kSeg[8][3] = {"es", "cs", "ss", "ds", "fs", "gs"};
static const char kCtl[8][4] = {"cr0", "wut", "cr2", "cr3",
                                "cr4", "wut", "wut", "wut"};

static const char kBreg[2][2][8][5] = {
    {{"al", "cl", "dl", "bl", "ah", "ch", "dh", "bh"},
     {"al", "cl", "dl", "bl", "spl", "bpl", "sil", "dil"}},
    {{"wut", "wut", "wut", "wut", "wut", "wut", "wut", "wut"},
     {"r8b", "r9b", "r10b", "r11b", "r12b", "r13b", "r14b", "r15b"}},
};

static const char kGreg[2][2][2][8][5] = {
    {{{"eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"},
      {"r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"}},
     {{"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"},
      {"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"}}},
    {{{"ax", "cx", "dx", "bx", "sp", "bp", "si", "di"},
      {"r8w", "r9w", "r10w", "r11w", "r12w", "r13w", "r14w", "r15w"}},
     {{"rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi"},
      {"r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"}}},
};

static i64 RipRelative(struct Dis *d, i64 i) {
  return d->addr + d->xedd->length + i;
}

static i64 ZeroExtend(u64 rde, i64 i) {
  switch (Mode(rde)) {
    case XED_MODE_REAL:
      return i & 0xffff;
    case XED_MODE_LEGACY:
      return i & 0xffffffff;
    default:
      return i;
  }
}

static i64 Unrelative(u64 rde, i64 i) {
  switch (Eamode(rde)) {
    case XED_MODE_REAL:
      return i & 0xffff;
    case XED_MODE_LEGACY:
      return i & 0xffffffff;
    default:
      return i;
  }
}

static const char *GetAddrReg(struct Dis *d, u64 rde, u8 x, u8 r) {
  return kGreg[Eamode(rde) == XED_MODE_REAL][Eamode(rde) == XED_MODE_LONG]
              [x & 1][r & 7];
}

static char *DisRegister(char *p, const char *s) {
  p = HighStart(p, g_high.reg);
  *p++ = '%';
  p = stpcpy(p, s);
  p = HighEnd(p);
  return p;
}

static char *DisRegisterByte(struct Dis *d, u64 rde, char *p, bool g, int r) {
  return DisRegister(p, kBreg[g][Rex(rde)][r]);
}

static char *DisRegisterWord(struct Dis *d, u64 rde, char *p, bool g, int r) {
  return DisRegister(p, kGreg[Osz(rde)][Rexw(rde)][g][r]);
}

static char *DisInt(char *p, i64 x) {
  if (-15 <= x && x <= 15) {
    p += snprintf(p, 32, "%" PRId64, x);
  } else if (x == INT64_MIN) {
    p = stpcpy(p, "-0x8000000000000000");
  } else if (x < 0 && -x < 0xFFFFFFFF) {
    p += snprintf(p, 32, "-0x%" PRIx64, -x);
  } else {
    p += snprintf(p, 32, "0x%" PRIx64, x);
  }
  return p;
}

static char *DisSymImpl(struct Dis *d, char *p, i64 x, long sym) {
  i64 addend;
  const char *name;
  addend = x - d->syms.p[sym].addr;
  name = d->syms.stab + d->syms.p[sym].name;
  p = Demangle(p, name, DIS_MAX_SYMBOL_LENGTH);
  if (addend) {
    *p++ = '+';
    p = DisInt(p, addend);
  }
  return p;
}

static char *DisSym(struct Dis *d, char *p, i64 value, i64 addr) {
  long sym;
  if ((sym = DisFindSym(d, addr)) != -1 && d->syms.p[sym].name) {
    return DisSymImpl(d, p, addr, sym);
  } else {
    return DisInt(p, value);
  }
}

static char *DisSymLiteral(struct Dis *d, u64 rde, char *p, u64 addr, u64 ip) {
  *p++ = '$';
  p = HighStart(p, g_high.literal);
  p = DisSym(d, p, addr, addr);
  p = HighEnd(p);
  return p;
}

static char *DisGvqp(struct Dis *d, u64 rde, char *p) {
  return DisRegisterWord(d, rde, p, Rexr(rde), ModrmReg(rde));
}

static char *DisGdqp(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[0][Rexw(rde)][Rexr(rde)][ModrmReg(rde)]);
}

static char *DisGb(struct Dis *d, u64 rde, char *p) {
  return DisRegisterByte(d, rde, p, Rexr(rde), ModrmReg(rde));
}

static char *DisSego(struct Dis *d, u64 rde, char *p) {
  if (Sego(rde)) {
    p = DisRegister(p, kSeg[Sego(rde) - 1]);
    *p++ = ':';
  }
  return p;
}

static bool IsRealModrmAbsolute(u64 rde) {
  return Eamode(rde) == XED_MODE_REAL && ModrmRm(rde) == 6 && !ModrmMod(rde);
}

static char *DisDisp(struct Dis *d, u64 rde, char *p) {
  i64 disp;
  if (ModrmMod(rde) == 1 || ModrmMod(rde) == 2 || IsRipRelative(rde) ||
      IsRealModrmAbsolute(rde) ||
      (Eamode(rde) != XED_MODE_REAL && ModrmMod(rde) == 0 &&
       ModrmRm(rde) == 4 && SibBase(rde) == 5)) {
    disp = d->xedd->op.disp;
    if (IsRipRelative(rde)) {
      if (Mode(rde) == XED_MODE_LONG) {
        disp = RipRelative(d, disp);
      } else {
        disp = Unrelative(rde, disp);
      }
    } else if (IsRealModrmAbsolute(rde)) {
      disp = Unrelative(rde, disp);
    }
    p = DisSym(d, p, disp, disp);
  }
  return p;
}

static char *DisBis(struct Dis *d, u64 rde, char *p) {
  const char *base, *index, *scale;
  base = index = scale = NULL;
  if (Eamode(rde) != XED_MODE_REAL) {
    if (!SibExists(rde)) {
      if (IsRipRelative(rde)) {
        if (Mode(rde) == XED_MODE_LONG) {
          base = kRip[Eamode(rde) == XED_MODE_LONG];
        }
      } else {
        base = GetAddrReg(d, rde, Rexb(rde), ModrmRm(rde));
      }
    } else if (!SibIsAbsolute(rde)) {
      if (SibHasBase(rde)) {
        base = GetAddrReg(d, rde, Rexb(rde), SibBase(rde));
      }
      if (SibHasIndex(rde)) {
        index = GetAddrReg(d, rde, Rexx(rde), SibIndex(rde));
      } else if (SibScale(rde)) {
        index = kRiz[Eamode(rde) == XED_MODE_LONG];
      }
      scale = kSka[SibScale(rde)];
    }
  } else {
    switch (ModrmRm(rde)) {
      case 0:
        base = "bx";
        index = "si";
        break;
      case 1:
        base = "bx";
        index = "di";
        break;
      case 2:
        base = "bp";
        index = "si";
        break;
      case 3:
        base = "bp";
        index = "di";
        break;
      case 4:
        base = "si";
        break;
      case 5:
        base = "di";
        break;
      case 6:
        if (ModrmMod(rde)) base = "bp";
        break;
      case 7:
        base = "bx";
        break;
      default:
        __builtin_unreachable();
    }
  }
  if (base || index) {
    *p++ = '(';
    if (base) {
      p = DisRegister(p, base);
    }
    if (index) {
      *p++ = ',';
      p = DisRegister(p, index);
      if (scale) {
        p = stpcpy(p, scale);
      }
    }
    *p++ = ')';
  }
  *p = '\0';
  return p;
}

static char *DisM(struct Dis *d, u64 rde, char *p) {
  p = DisSego(d, rde, p);
  p = DisDisp(d, rde, p);
  p = DisBis(d, rde, p);
  return p;
}

static char *DisRegMem(struct Dis *d, u64 rde, char *p,
                       char *f(struct Dis *, u64, char *)) {
  if (IsModrmRegister(rde)) {
    return f(d, rde, p);
  } else {
    return DisM(d, rde, p);
  }
}

static char *DisE(struct Dis *d, u64 rde, char *p,
                  char *f(struct Dis *, u64, char *, bool, int)) {
  if (IsModrmRegister(rde)) {
    return f(d, rde, p, Rexb(rde), ModrmRm(rde));
  } else {
    return DisM(d, rde, p);
  }
}

static char *DisEb(struct Dis *d, u64 rde, char *p) {
  return DisE(d, rde, p, DisRegisterByte);
}

static char *DisEvqp(struct Dis *d, u64 rde, char *p) {
  return DisE(d, rde, p, DisRegisterWord);
}

static char *DisRv(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[Osz(rde)][0][Rexb(rde)][ModrmRm(rde)]);
}

static char *DisRvqp(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[Osz(rde)][Rexw(rde)][Rexb(rde)][ModrmRm(rde)]);
}

static char *DisRdqp(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[0][Rexw(rde)][Rexb(rde)][ModrmRm(rde)]);
}

static char *DisEdqp(struct Dis *d, u64 rde, char *p) {
  return DisRegMem(d, rde, p, DisRdqp);
}

static char *DisEv(struct Dis *d, u64 rde, char *p) {
  return DisRegMem(d, rde, p, DisRv);
}

static char *DisGvq(struct Dis *d, u64 rde, char *p, int r) {
  const char *s;
  if (Mode(rde) == XED_MODE_LONG) {
    s = kGreg[Osz(rde)][!Osz(rde)][Rexb(rde)][r];
  } else {
    s = kGreg[Osz(rde)][0][Rexb(rde)][r];
  }
  return DisRegister(p, s);
}

static char *DisZvq(struct Dis *d, u64 rde, char *p) {
  return DisGvq(d, rde, p, ModrmSrm(rde));
}

static char *DisEvqReg(struct Dis *d, u64 rde, char *p) {
  return DisGvq(d, rde, p, ModrmRm(rde));
}

static char *DisEvq(struct Dis *d, u64 rde, char *p) {
  return DisRegMem(d, rde, p, DisEvqReg);
}

static char *DisEdReg(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[0][0][Rexb(rde)][ModrmRm(rde)]);
}

static char *DisEd(struct Dis *d, u64 rde, char *p) {
  return DisRegMem(d, rde, p, DisEdReg);
}

static char *DisEqReg(struct Dis *d, u64 rde, char *p) {
  const char *r;
  if (Mode(rde) == XED_MODE_LONG) {
    r = kGreg[0][1][Rexb(rde)][ModrmRm(rde)];
  } else {
    r = kGreg[Osz(rde)][0][Rexb(rde)][ModrmRm(rde)];
  }
  return DisRegister(p, r);
}

static char *DisEq(struct Dis *d, u64 rde, char *p) {
  return DisRegMem(d, rde, p, DisEqReg);
}

static char *DisZvqp(struct Dis *d, u64 rde, char *p) {
  return DisRegisterWord(d, rde, p, Rexb(rde), ModrmSrm(rde));
}

static char *DisZb(struct Dis *d, u64 rde, char *p) {
  return DisRegisterByte(d, rde, p, Rexb(rde), ModrmSrm(rde));
}

static char *DisEax(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[Osz(rde)][0][0][0]);
}

static char *DisRax(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[Osz(rde)][Rexw(rde)][0][0]);
}

static char *DisRdx(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[Osz(rde)][Rexw(rde)][0][2]);
}

static char *DisPort(struct Dis *d, u64 rde, char *p) {
  *p++ = '(';
  p = DisRegister(p, kGreg[1][0][0][2]);
  *p++ = ')';
  *p = '\0';
  return p;
}

static char *DisCd(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kCtl[ModrmReg(rde)]);
}

static char *DisHd(struct Dis *d, u64 rde, char *p) {
  return DisRegister(p, kGreg[0][Mode(rde) == XED_MODE_LONG][0][ModrmRm(rde)]);
}

static char *DisImm(struct Dis *d, u64 rde, char *p) {
  return DisSymLiteral(d, rde, p, d->xedd->op.uimm0,
                       ZeroExtend(rde, d->xedd->op.uimm0));
}

static char *DisRvds(struct Dis *d, u64 rde, char *p) {
  return DisSymLiteral(d, rde, p, d->xedd->op.disp, d->xedd->op.disp);
}

static char *DisKpvds(struct Dis *d, u64 rde, char *p, u64 x) {
  *p++ = '$';
  p = HighStart(p, g_high.literal);
  p = DisInt(p, x);
  p = HighEnd(p);
  return p;
}

static char *DisKvds(struct Dis *d, u64 rde, char *p) {
  return DisKpvds(d, rde, p, d->xedd->op.uimm0);
}

static char *DisPvds(struct Dis *d, u64 rde, char *p) {
  return DisKpvds(d, rde, p,
                  d->xedd->op.disp & (Osz(rde) ? 0xffff : 0xffffffff));
}

static char *DisOne(struct Dis *d, u64 rde, char *p) {
  *p++ = '$';
  p = HighStart(p, g_high.literal);
  p = stpcpy(p, "1");
  p = HighEnd(p);
  return p;
}

static char *DisJbs(struct Dis *d, u64 rde, char *p) {
  if (d->xedd->op.disp > 0) *p++ = '+';
  p += snprintf(p, 32, "%" PRId64, d->xedd->op.disp);
  return p;
}

static char *DisJb(struct Dis *d, u64 rde, char *p) {
  if (d->xedd->op.disp > 0) *p++ = '+';
  p += snprintf(p, 32, "%d", (int)(d->xedd->op.disp & 0xff));
  return p;
}

static char *DisJvds(struct Dis *d, u64 rde, char *p) {
  return DisSym(d, p, RipRelative(d, d->xedd->op.disp),
                RipRelative(d, d->xedd->op.disp) - (d->m ? d->m->cs : 0));
}

static char *DisAbs(struct Dis *d, u64 rde, char *p) {
  return DisSym(d, p, d->xedd->op.disp, d->xedd->op.disp);
}

static char *DisSw(struct Dis *d, u64 rde, char *p) {
  if (kSeg[ModrmReg(rde)][0]) p = DisRegister(p, kSeg[ModrmReg(rde)]);
  return p;
}

static char *DisSpecialAddr(struct Dis *d, u64 rde, char *p, int r) {
  *p++ = '(';
  p = DisRegister(p, GetAddrReg(d, rde, 0, r));
  *p++ = ')';
  *p = '\0';
  return p;
}

static char *DisY(struct Dis *d, u64 rde, char *p) {
  return DisSpecialAddr(d, rde, p, 7);  // es:di
}

static char *DisX(struct Dis *d, u64 rde, char *p) {
  p = DisSego(d, rde, p);
  return DisSpecialAddr(d, rde, p, 6);  // ds:si
}

static char *DisBBb(struct Dis *d, u64 rde, char *p) {
  p = DisSego(d, rde, p);
  return DisSpecialAddr(d, rde, p, 3);  // ds:bx
}

static char *DisXmm(struct Dis *d, u64 rde, char *p, const char *s, int reg) {
  p = HighStart(p, g_high.reg);
  *p++ = '%';
  p = stpcpy(p, s);
  p += snprintf(p, 32, "%u", reg);
  p = HighEnd(p);
  return p;
}

static char *DisNq(struct Dis *d, u64 rde, char *p) {
  return DisXmm(d, rde, p, "mm", ModrmRm(rde));
}

static char *DisPq(struct Dis *d, u64 rde, char *p) {
  return DisXmm(d, rde, p, "mm", ModrmReg(rde));
}

static char *DisUq(struct Dis *d, u64 rde, char *p) {
  return DisXmm(d, rde, p, "xmm", RexbRm(rde));
}

static char *DisUdq(struct Dis *d, u64 rde, char *p) {
  return DisXmm(d, rde, p, "xmm", RexbRm(rde));
}

static char *DisVdq(struct Dis *d, u64 rde, char *p) {
  return DisXmm(d, rde, p, "xmm", RexrReg(rde));
}

static char *DisQq(struct Dis *d, u64 rde, char *p) {
  return DisRegMem(d, rde, p, DisNq);
}

static char *DisEst(struct Dis *d, u64 rde, char *p) {
  p = DisRegister(p, "st");
  if (ModrmRm(rde) != 0) {
    *p++ = '(';
    *p++ = '0' + ModrmRm(rde);
    *p++ = ')';
    *p = '\0';
  }
  return p;
}

static char *DisEst1(struct Dis *d, u64 rde, char *p) {
  if (ModrmRm(rde) != 1) {
    p = DisEst(d, rde, p);
  } else {
    *p = '\0';
  }
  return p;
}

static char *DisEssr(struct Dis *d, u64 rde, char *p) {
  return DisRegMem(d, rde, p, DisEst);
}

static char *DisWps(struct Dis *d, u64 rde, char *p) {
  return DisRegMem(d, rde, p, DisUdq);
}

#define DisEdr  DisM
#define DisEqp  DisEq
#define DisEsr  DisM
#define DisGv   DisGvqp
#define DisIb   DisImm
#define DisIbs  DisImm
#define DisIbss DisImm
#define DisIvds DisImm
#define DisIvqp DisImm
#define DisIvs  DisImm
#define DisIw   DisImm
#define DisMdi  DisM
#define DisMdq  DisM
#define DisMdqp DisM
#define DisMdr  DisM
#define DisMe   DisM
#define DisMer  DisM
#define DisMp   DisM
#define DisMps  DisM
#define DisMq   DisM
#define DisMqi  DisM
#define DisMs   DisM
#define DisMsr  DisEssr
#define DisMw   DisM
#define DisMwi  DisM
#define DisOb   DisAbs
#define DisOvqp DisAbs
#define DisPpi  DisPq
#define DisQpi  DisQq
#define DisVpd  DisVdq
#define DisVps  DisVdq
#define DisVq   DisVdq
#define DisVsd  DisVdq
#define DisVss  DisVdq
#define DisWdq  DisWps
#define DisWpd  DisWps
#define DisWpsq DisWps
#define DisWq   DisWps
#define DisWsd  DisWps
#define DisWss  DisWps
#define DisXb   DisX
#define DisXv   DisX
#define DisXvqp DisX
#define DisYb   DisY
#define DisYv   DisY
#define DisYvqp DisY
#define DisZv   DisZvqp

static const struct DisArg {
  char s[8];
  char *(*f)(struct Dis *, u64, char *);
} kDisArgs[] = /* <sorted> */ {
    {"$1", DisOne},      //
    {"%Cd", DisCd},      //
    {"%Gb", DisGb},      //
    {"%Gdqp", DisGdqp},  //
    {"%Gv", DisGv},      //
    {"%Gvqp", DisGvqp},  //
    {"%Hd", DisHd},      //
    {"%Nq", DisNq},      //
    {"%Ppi", DisPpi},    //
    {"%Pq", DisPq},      //
    {"%Rdqp", DisRdqp},  //
    {"%Rvqp", DisRvqp},  //
    {"%Sw", DisSw},      //
    {"%Udq", DisUdq},    //
    {"%Uq", DisUq},      //
    {"%Vdq", DisVdq},    //
    {"%Vpd", DisVpd},    //
    {"%Vps", DisVps},    //
    {"%Vq", DisVq},      //
    {"%Vsd", DisVsd},    //
    {"%Vss", DisVss},    //
    {"%Zb", DisZb},      //
    {"%Zv", DisZv},      //
    {"%Zvq", DisZvq},    //
    {"%Zvqp", DisZvqp},  //
    {"%eAX", DisEax},    //
    {"%rAX", DisRax},    //
    {"%rDX", DisRdx},    //
    {"BBb", DisBBb},     //
    {"DX", DisPort},     //
    {"EST", DisEst},     //
    {"EST1", DisEst1},   //
    {"ESsr", DisEssr},   //
    {"Eb", DisEb},       //
    {"Ed", DisEd},       //
    {"Edqp", DisEdqp},   //
    {"Edr", DisEdr},     //
    {"Eq", DisEq},       //
    {"Eqp", DisEqp},     //
    {"Esr", DisEsr},     //
    {"Ev", DisEv},       //
    {"Evq", DisEvq},     //
    {"Evqp", DisEvqp},   //
    {"Ew", DisEvqp},     //
    {"Ib", DisIb},       //
    {"Ibs", DisIbs},     //
    {"Ibss", DisIbss},   //
    {"Ivds", DisIvds},   //
    {"Ivqp", DisIvqp},   //
    {"Ivs", DisIvs},     //
    {"Iw", DisIw},       //
    {"Jb", DisJb},       //
    {"Jbs", DisJbs},     //
    {"Jvds", DisJvds},   //
    {"Kvds", DisKvds},   //
    {"M", DisM},         //
    {"Mdi", DisMdi},     //
    {"Mdq", DisMdq},     //
    {"Mdqp", DisMdqp},   //
    {"Mdr", DisMdr},     //
    {"Me", DisMe},       //
    {"Mer", DisMer},     //
    {"Mp", DisMp},       //
    {"Mps", DisMps},     //
    {"Mq", DisMq},       //
    {"Mqi", DisMqi},     //
    {"Ms", DisMs},       //
    {"Msr", DisMsr},     //
    {"Mw", DisMw},       //
    {"Mwi", DisMwi},     //
    {"Ob", DisOb},       //
    {"Ovqp", DisOvqp},   //
    {"Pvds", DisPvds},   //
    {"Qpi", DisQpi},     //
    {"Qq", DisQq},       //
    {"Rvds", DisRvds},   //
    {"Wdq", DisWdq},     //
    {"Wpd", DisWpd},     //
    {"Wps", DisWps},     //
    {"Wpsq", DisWpsq},   //
    {"Wq", DisWq},       //
    {"Wsd", DisWsd},     //
    {"Wss", DisWss},     //
    {"Xb", DisXb},       //
    {"Xv", DisXv},       //
    {"Xvqp", DisXvqp},   //
    {"Yb", DisYb},       //
    {"Yv", DisYv},       //
    {"Yvqp", DisYvqp},   //
};

static int CompareString8(const char a[8], const char b[8]) {
  u64 x, y;
  x = ((u64)(255 & a[0]) << 070 | (u64)(255 & a[1]) << 060 |
       (u64)(255 & a[2]) << 050 | (u64)(255 & a[3]) << 040 |
       (u64)(255 & a[4]) << 030 | (u64)(255 & a[5]) << 020 |
       (u64)(255 & a[6]) << 010 | (u64)(255 & a[7]) << 000);
  y = ((u64)(255 & b[0]) << 070 | (u64)(255 & b[1]) << 060 |
       (u64)(255 & b[2]) << 050 | (u64)(255 & b[3]) << 040 |
       (u64)(255 & b[4]) << 030 | (u64)(255 & b[5]) << 020 |
       (u64)(255 & b[6]) << 010 | (u64)(255 & b[7]) << 000);
  return x > y ? 1 : x < y ? -1 : 0;
}

char *DisArg(struct Dis *d, char *p, const char *s) {
  char key[8];
  int c, m, l, r;
  l = 0;
  r = ARRAYLEN(kDisArgs) - 1;
  memset(key, 0, sizeof(key));
  memcpy(key, s, MIN(8, strlen(s)));
  while (l <= r) {
    m = (l + r) >> 1;
    c = CompareString8(kDisArgs[m].s, key);
    if (c < 0) {
      l = m + 1;
    } else if (c > 0) {
      r = m - 1;
    } else {
      return kDisArgs[m].f(d, d->xedd->op.rde, p);
    }
  }
  if (*s == '%') {
    p = DisRegister(p, s + 1);
  } else {
    p = stpcpy(p, s);
  }
  return p;
}
