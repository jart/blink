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
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/debug.h"
#include "blink/dis.h"
#include "blink/high.h"
#include "blink/jit.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/macros.h"
#include "blink/overlays.h"
#include "blink/rde.h"
#include "blink/stats.h"

#ifdef DISABLE_OVERLAYS
#define OverlaysOpen openat
#endif

#define APPEND(...) o += snprintf(b + o, n - o, __VA_ARGS__)

#ifdef HAVE_JIT

void (*AddPath_StartOp_Hook)(P);

#if LOG_COD
static int g_cod;
static struct Dis g_dis;
#endif

static void StartPath(struct Machine *m, i64 pc) {
  JIX_LOGF("%" PRIx64 ":%" PRIx64 " <path>", GetPc(m), pc);
}

static void DebugOp(struct Machine *m, i64 expected_ip) {
  if (m->ip != expected_ip) {
    LOGF("IP was %" PRIx64 " but it should have been %" PRIx64, m->ip,
         expected_ip);
  }
  unassert(m->ip == expected_ip);
}

static void StartOp(struct Machine *m, i64 pc) {
  JIX_LOGF("%" PRIx64 ":%" PRIx64 "   <op>", GetPc(m), pc);
  JIX_LOGF("%" PRIx64 ":%" PRIx64 "     %s", GetPc(m), pc, DescribeOp(m, pc));
  unassert(!IsMakingPath(m));
}

static void EndOp(struct Machine *m, i64 pc) {
  JIX_LOGF("%" PRIx64 ":%" PRIx64 "   </op>", GetPc(m), pc);
  m->oplen = 0;
  if (m->stashaddr) {
    CommitStash(m);
  }
}

static void EndPath(struct Machine *m, i64 pc) {
  JIX_LOGF("%" PRIx64 ":%" PRIx64 "   %s", GetPc(m), GetPc(m),
           DescribeOp(m, GetPc(m)));
  JIX_LOGF("%" PRIx64 ":%" PRIx64 " </path>", GetPc(m), GetPc(m));
}

void FuseOp(struct Machine *m, i64 pc) {
  JIX_LOGF("%" PRIx64 ":%" PRIx64 "     %s", GetPc(m), pc, DescribeOp(m, pc));
  JIX_LOGF("%" PRIx64 ":%" PRIx64 "   </op>", GetPc(m), pc);
}

#ifdef HAVE_JIT
#if defined(__x86_64__)
static const u8 kEnter[] = {
    0x55,                    // push %rbp
    0x48, 0x89, 0345,        // mov  %rsp,%rbp
#ifdef __CYGWIN__            //
    0x48, 0x83, 0354, 0x40,  // sub  $0x40,%rsp
    0x48, 0x89, 0175, 0xc8,  // mov  %rdi,-0x38(%rbp)
    0x48, 0x89, 0165, 0xd0,  // mov  %rsi,-0x30(%rbp)
#else                        //
    0x48, 0x83, 0354, 0x30,  // sub  $0x30,%rsp
#endif                       //
    0x48, 0x89, 0135, 0xd8,  // mov  %rbx,-0x28(%rbp)
    0x4c, 0x89, 0145, 0xe0,  // mov  %r12,-0x20(%rbp)
    0x4c, 0x89, 0155, 0xe8,  // mov  %r13,-0x18(%rbp)
    0x4c, 0x89, 0165, 0xf0,  // mov  %r14,-0x10(%rbp)
    0x4c, 0x89, 0175, 0xf8,  // mov  %r15,-0x08(%rbp)
#ifdef __CYGWIN__            //
    0x48, 0x89, 0313,        // mov  %rcx,%rbx
#else                        //
    0x48, 0x89, 0373,        // mov  %rdi,%rbx
#endif
};
static const u8 kLeave[] = {
    0x4c, 0x8b, 0175, 0xf8,  // mov -0x08(%rbp),%r15
    0x4c, 0x8b, 0165, 0xf0,  // mov -0x10(%rbp),%r14
    0x4c, 0x8b, 0155, 0xe8,  // mov -0x18(%rbp),%r13
    0x4c, 0x8b, 0145, 0xe0,  // mov -0x20(%rbp),%r12
    0x48, 0x8b, 0135, 0xd8,  // mov -0x28(%rbp),%rbx
#ifdef __CYGWIN__
    0x48, 0x8b, 0165, 0xd0,  // mov -0x30(%rbp),%rsi
    0x48, 0x8b, 0175, 0xc8,  // mov -0x38(%rbp),%rdi
    0x48, 0x83, 0304, 0x40,  // add $0x40,%rsp
#else
    0x48, 0x83, 0304, 0x30,  // add $0x30,%rsp
#endif
    0x5d,  // pop %rbp
};
#elif defined(__aarch64__)
static const u32 kEnter[] = {
    0xa9bc7bfd,  // stp x29, x30, [sp, #-64]!
    0x910003fd,  // mov x29, sp
    0xa90153f3,  // stp x19, x20, [sp, #16]
    0xa9025bf5,  // stp x21, x22, [sp, #32]
    0xa90363f7,  // stp x23, x24, [sp, #48]
    0xaa0003f3,  // mov x19, x0
};
static const u32 kLeave[] = {
    0xa94153f3,  // ldp x19, x20, [sp, #16]
    0xa9425bf5,  // ldp x21, x22, [sp, #32]
    0xa94363f7,  // ldp x23, x24, [sp, #48]
    0xa8c47bfd,  // ldp x29, x30, [sp], #64
};
#endif /* __x86_64__ */
#endif /* HAVE_JIT */

long GetPrologueSize(void) {
#ifdef HAVE_JIT
  return sizeof(kEnter);
#else
  return 0;
#endif
}

void(SetupCod)(struct Machine *m) {
#if LOG_COD
  LoadDebugSymbols(&m->system->elf);
  DisLoadElf(&g_dis, &m->system->elf);
  g_cod = OverlaysOpen(AT_FDCWD_LINUX, "/tmp/blink.s",
                       O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
  g_cod = fcntl(g_cod, F_DUPFD_CLOEXEC, kMinBlinkFd);
#endif
}

void(LogCodOp)(struct Machine *m, const char *s) {
#if LOG_COD
  // FlushCod(m->path.jb);
  WriteCod("/\t%s\n", s);
#endif
}

void(WriteCod)(const char *fmt, ...) {
#if LOG_COD
  int n;
  va_list va;
  char buf[256];
  if (!g_cod) return;
  va_start(va, fmt);
  n = vsnprintf(buf, sizeof(buf), fmt, va);
  va_end(va);
  write(g_cod, buf, MIN(n, sizeof(buf)));
#endif
}

void BeginCod(struct Machine *m, i64 pc) {
#if LOG_COD
  char b[256];
  char spec[64];
  int i, o = 0, n = sizeof(b);
  if (!g_cod) return;
  DISABLE_HIGHLIGHT_BEGIN;
  APPEND("/\t");
  unassert(!GetInstruction(m, pc, g_dis.xedd));
  DisInst(&g_dis, b + o, DisSpec(g_dis.xedd, spec));
  o = strlen(b);
  APPEND(" #");
  for (i = 0; i < g_dis.xedd->length; ++i) {
    APPEND(" %02x", g_dis.xedd->bytes[i]);
  }
  APPEND(" @ %" PRIx64, m->ip);
  APPEND("\n");
  write(g_cod, b, MIN(o, n));
  DISABLE_HIGHLIGHT_END;
#endif
}

void FlushCod(struct JitBlock *jb) {
#if LOG_COD
  char b[256];
  char spec[64];
  if (!g_cod) return;
  if (jb->index == jb->blocksize + 1) {
    WriteCod("/\tOOM!\n");
    jb->cod = jb->index;
    return;
  }
  DISABLE_HIGHLIGHT_BEGIN;
  for (; jb->cod < jb->index; jb->cod += g_dis.xedd->length) {
    unassert(!DecodeInstruction(g_dis.xedd, jb->addr + jb->cod,
                                jb->index - jb->cod, XED_MODE_LONG));
    g_dis.addr = (intptr_t)jb->addr + jb->cod;
    DisInst(&g_dis, b, DisSpec(g_dis.xedd, spec));
    WriteCod("\t%s\n", b);
  }
  DISABLE_HIGHLIGHT_END;
#endif
}

static bool IsPure(u64 rde) {
  switch (Mopcode(rde)) {
    case 0x004:  // OpAluAlIbAdd
    case 0x005:  // OpAluRaxIvds
    case 0x00C:  // OpAluAlIbOr
    case 0x00D:  // OpAluRaxIvds
    case 0x014:  // OpAluAlIbAdc
    case 0x015:  // OpAluRaxIvds
    case 0x01C:  // OpAluAlIbSbb
    case 0x01D:  // OpAluRaxIvds
    case 0x024:  // OpAluAlIbAnd
    case 0x025:  // OpAluRaxIvds
    case 0x02C:  // OpAluAlIbSub
    case 0x02D:  // OpAluRaxIvds
    case 0x034:  // OpAluAlIbXor
    case 0x035:  // OpAluRaxIvds
    case 0x03C:  // OpCmpAlIb
    case 0x03D:  // OpCmpRaxIvds
    case 0x090:  // OpNop
    case 0x091:  // OpXchgZvqp
    case 0x092:  // OpXchgZvqp
    case 0x093:  // OpXchgZvqp
    case 0x094:  // OpXchgZvqp
    case 0x095:  // OpXchgZvqp
    case 0x096:  // OpXchgZvqp
    case 0x097:  // OpXchgZvqp
    case 0x098:  // OpSax
    case 0x099:  // OpConvert
    case 0x09E:  // OpSahf
    case 0x09F:  // OpLahf
    case 0x0A1:  // OpMovRaxOvqp
    case 0x0A3:  // OpMovOvqpRax
    case 0x0A8:  // OpTestAlIb
    case 0x0A9:  // OpTestRaxIvds
    case 0x0B0:  // OpMovZbIb
    case 0x0B1:  // OpMovZbIb
    case 0x0B2:  // OpMovZbIb
    case 0x0B3:  // OpMovZbIb
    case 0x0B4:  // OpMovZbIb
    case 0x0B5:  // OpMovZbIb
    case 0x0B6:  // OpMovZbIb
    case 0x0B7:  // OpMovZbIb
    case 0x0B8:  // OpMovZvqpIvqp
    case 0x0B9:  // OpMovZvqpIvqp
    case 0x0BA:  // OpMovZvqpIvqp
    case 0x0BB:  // OpMovZvqpIvqp
    case 0x0BC:  // OpMovZvqpIvqp
    case 0x0BD:  // OpMovZvqpIvqp
    case 0x0BE:  // OpMovZvqpIvqp
    case 0x0BF:  // OpMovZvqpIvqp
    case 0x0D6:  // OpSalc
    case 0x0F5:  // OpCmc
    case 0x0F8:  // OpClc
    case 0x0F9:  // OpStc
    case 0x11F:  // OpNopEv
    case 0x150:  // OpMovmskpsd
    case 0x1D7:  // OpPmovmskbGdqpNqUdq
    case 0x1C8:  // OpBswapZvqp
    case 0x1C9:  // OpBswapZvqp
    case 0x1CA:  // OpBswapZvqp
    case 0x1CB:  // OpBswapZvqp
    case 0x1CC:  // OpBswapZvqp
    case 0x1CD:  // OpBswapZvqp
    case 0x1CE:  // OpBswapZvqp
    case 0x1CF:  // OpBswapZvqp
    case 0x050:  // OpPushZvq
    case 0x051:  // OpPushZvq
    case 0x052:  // OpPushZvq
    case 0x053:  // OpPushZvq
    case 0x054:  // OpPushZvq
    case 0x055:  // OpPushZvq
    case 0x056:  // OpPushZvq
    case 0x057:  // OpPushZvq
    case 0x058:  // OpPopZvq
    case 0x059:  // OpPopZvq
    case 0x05A:  // OpPopZvq
    case 0x05B:  // OpPopZvq
    case 0x05C:  // OpPopZvq
    case 0x05D:  // OpPopZvq
    case 0x05E:  // OpPopZvq
    case 0x05F:  // OpPopZvq
      return true;
    case 0x000:  // OpAlub
    case 0x001:  // OpAluw
    case 0x002:  // OpAlubFlip
    case 0x003:  // OpAluwFlip
    case 0x008:  // OpAlub
    case 0x009:  // OpAluw
    case 0x00A:  // OpAlubFlip
    case 0x00B:  // OpAluwFlip
    case 0x010:  // OpAlub
    case 0x011:  // OpAluw
    case 0x012:  // OpAlubFlip
    case 0x013:  // OpAluwFlip
    case 0x018:  // OpAlub
    case 0x019:  // OpAluw
    case 0x01A:  // OpAlubFlip
    case 0x01B:  // OpAluwFlip
    case 0x020:  // OpAlub
    case 0x021:  // OpAluw
    case 0x022:  // OpAlubFlip
    case 0x023:  // OpAluwFlip
    case 0x028:  // OpAlub
    case 0x029:  // OpAluw
    case 0x02A:  // OpAlubFlip
    case 0x02B:  // OpAluwFlip
    case 0x030:  // OpAlub
    case 0x031:  // OpAluw
    case 0x032:  // OpAlubFlip
    case 0x033:  // OpAluwFlip
    case 0x038:  // OpAlubCmp
    case 0x039:  // OpAluwCmp
    case 0x03A:  // OpAlubFlipCmp
    case 0x03B:  // OpAluwFlipCmp
    case 0x063:  // OpMovsxdGdqpEd
    case 0x069:  // OpImulGvqpEvqpImm
    case 0x06B:  // OpImulGvqpEvqpImm
    case 0x080:  // OpAlubiReg
    case 0x081:  // OpAluwiReg
    case 0x082:  // OpAlubiReg
    case 0x083:  // OpAluwiReg
    case 0x084:  // OpAlubTest
    case 0x085:  // OpAluwTest
    case 0x086:  // OpXchgGbEb
    case 0x087:  // OpXchgGvqpEvqp
    case 0x088:  // OpMovEbGb
    case 0x089:  // OpMovEvqpGvqp
    case 0x08A:  // OpMovGbEb
    case 0x08B:  // OpMovGvqpEvqp
    case 0x0C0:  // OpBsubiImm
    case 0x0C1:  // OpBsuwiImm
    case 0x0C6:  // OpMovEbIb
    case 0x0C7:  // OpMovEvqpIvds
    case 0x0D0:  // OpBsubi1
    case 0x0D1:  // OpBsuwi1
    case 0x0D2:  // OpBsubiCl
    case 0x0D3:  // OpBsuwiCl
    case 0x0F6:  // Op0f6
    case 0x0F7:  // Op0f7
    case 0x140:  // OpCmovo
    case 0x141:  // OpCmovno
    case 0x142:  // OpCmovb
    case 0x143:  // OpCmovae
    case 0x144:  // OpCmove
    case 0x145:  // OpCmovne
    case 0x146:  // OpCmovbe
    case 0x147:  // OpCmova
    case 0x148:  // OpCmovs
    case 0x149:  // OpCmovns
    case 0x14A:  // OpCmovp
    case 0x14B:  // OpCmovnp
    case 0x14C:  // OpCmovl
    case 0x14D:  // OpCmovge
    case 0x14E:  // OpCmovle
    case 0x14F:  // OpCmovg
    case 0x190:  // OpSeto
    case 0x191:  // OpSetno
    case 0x192:  // OpSetb
    case 0x193:  // OpSetae
    case 0x194:  // OpSete
    case 0x195:  // OpSetne
    case 0x196:  // OpSetbe
    case 0x197:  // OpSeta
    case 0x198:  // OpSets
    case 0x199:  // OpSetns
    case 0x19A:  // OpSetp
    case 0x19B:  // OpSetnp
    case 0x19C:  // OpSetl
    case 0x19D:  // OpSetge
    case 0x19E:  // OpSetle
    case 0x19F:  // OpSetg
    case 0x1A3:  // OpBit
    case 0x1A4:  // OpDoubleShift
    case 0x1A5:  // OpDoubleShift
    case 0x1AB:  // OpBit
    case 0x1AC:  // OpDoubleShift
    case 0x1AD:  // OpDoubleShift
    case 0x1AF:  // OpImulGvqpEvqp
    case 0x1B3:  // OpBit
    case 0x1B6:  // OpMovzbGvqpEb
    case 0x1B7:  // OpMovzwGvqpEw
    case 0x1BA:  // OpBit
    case 0x1BB:  // OpBit
    case 0x1BC:  // OpBsf
    case 0x1BD:  // OpBsr
    case 0x1BE:  // OpMovsbGvqpEb
    case 0x1BF:  // OpMovswGvqpEw
    case 0x110:  // sse moves
    case 0x111:  // sse moves
    case 0x112:  // sse moves
    case 0x113:  // sse moves
    case 0x114:  // unpcklpsd
    case 0x115:  // unpckhpsd
    case 0x116:  // sse moves
    case 0x117:  // sse moves
    case 0x128:  // sse moves
    case 0x129:  // sse moves
    case 0x12A:  // sse convs
    case 0x12B:  // sse moves
    case 0x12C:  // sse convs
    case 0x12D:  // sse convs
    case 0x160:  // OpSsePunpcklbw
    case 0x161:  // OpSsePunpcklwd
    case 0x162:  // OpSsePunpckldq
    case 0x163:  // OpSsePacksswb
    case 0x164:  // OpSsePcmpgtb
    case 0x165:  // OpSsePcmpgtw
    case 0x166:  // OpSsePcmpgtd
    case 0x167:  // OpSsePackuswb
    case 0x168:  // OpSsePunpckhbw
    case 0x169:  // OpSsePunpckhwd
    case 0x16A:  // OpSsePunpckhdq
    case 0x16B:  // OpSsePackssdw
    case 0x16C:  // OpSsePunpcklqdq
    case 0x16D:  // OpSsePunpckhqdq
    case 0x16E:  // OpMov0f6e
    case 0x16F:  // OpMov0f6f
    case 0x170:  // OpShuffle
    case 0x171:  // Op171
    case 0x172:  // Op172
    case 0x173:  // Op173
    case 0x174:  // OpSsePcmpeqb
    case 0x175:  // OpSsePcmpeqw
    case 0x176:  // OpSsePcmpeqd
    case 0x17E:  // OpMov0f7e
    case 0x17F:  // OpMov0f7f
    case 0x1D1:  // OpSsePsrlwv
    case 0x1D2:  // OpSsePsrldv
    case 0x1D3:  // OpSsePsrlqv
    case 0x1D4:  // OpSsePaddq
    case 0x1D5:  // OpSsePmullw
    case 0x1D8:  // OpSsePsubusb
    case 0x1D9:  // OpSsePsubusw
    case 0x1DA:  // OpSsePminub
    case 0x1DB:  // OpSsePand
    case 0x1DC:  // OpSsePaddusb
    case 0x1DD:  // OpSsePaddusw
    case 0x1DE:  // OpSsePmaxub
    case 0x1DF:  // OpSsePandn
    case 0x1E0:  // OpSsePavgb
    case 0x1E1:  // OpSsePsrawv
    case 0x1E2:  // OpSsePsradv
    case 0x1E3:  // OpSsePavgw
    case 0x1E4:  // OpSsePmulhuw
    case 0x1E5:  // OpSsePmulhw
    case 0x1E8:  // OpSsePsubsb
    case 0x1E9:  // OpSsePsubsw
    case 0x1EA:  // OpSsePminsw
    case 0x1EB:  // OpSsePor
    case 0x1EC:  // OpSsePaddsb
    case 0x1ED:  // OpSsePaddsw
    case 0x1EE:  // OpSsePmaxsw
    case 0x1EF:  // OpSsePxor
    case 0x1F1:  // OpSsePsllwv
    case 0x1F2:  // OpSsePslldv
    case 0x1F3:  // OpSsePsllqv
    case 0x1F4:  // OpSsePmuludq
    case 0x1F5:  // OpSsePmaddwd
    case 0x1F6:  // OpSsePsadbw
    case 0x1F8:  // OpSsePsubb
    case 0x1F9:  // OpSsePsubw
    case 0x1FA:  // OpSsePsubd
    case 0x1FB:  // OpSsePsubq
    case 0x1FC:  // OpSsePaddb
    case 0x1FD:  // OpSsePaddw
    case 0x1FE:  // OpSsePaddd
    case 0x200:  // OpSsePshufb
    case 0x201:  // OpSsePhaddw
    case 0x202:  // OpSsePhaddd
    case 0x203:  // OpSsePhaddsw
    case 0x204:  // OpSsePmaddubsw
    case 0x205:  // OpSsePhsubw
    case 0x206:  // OpSsePhsubd
    case 0x207:  // OpSsePhsubsw
    case 0x208:  // OpSsePsignb
    case 0x209:  // OpSsePsignw
    case 0x20A:  // OpSsePsignd
    case 0x20B:  // OpSsePmulhrsw
    case 0x2F6:  // adcx, adox, mulx
    case 0x344:  // pclmulqdq
      return IsModrmRegister(rde);
    case 0x08D:  // OpLeaGvqpM
      return !IsRipRelative(rde);
    case 0x0FF:
      return IsModrmRegister(rde) &&  //
             (ModrmReg(rde) == 0 ||   // inc
              ModrmReg(rde) == 1);    // dec
    default:
      return false;
  }
}

static bool MustUpdateIp(P) {
  u64 next;
  if (!IsPure(rde)) return true;
  next = m->ip + Oplength(rde);
  if (GetJitHook(&m->system->jit, next, 0)) return true;
  return false;
}

static void InitPaths(struct System *s) {
#ifdef HAVE_JIT
  struct JitBlock *jb;
  if (!s->ender) {
    unassert((jb = StartJit(&s->jit)));
    WriteCod("\nJit_%p:\n", jb->addr + jb->index);
    s->ender = GetJitPc(jb);
#if LOG_JIX
    AppendJitMovReg(jb, kJitArg0, kJitSav0);
    AppendJitCall(jb, (void *)EndPath);
#endif
    AppendJit(jb, kLeave, sizeof(kLeave));
    AppendJitRet(jb);
    FlushCod(jb);
    unassert(FinishJit(&s->jit, jb, 0));
  }
#endif
}

bool CreatePath(P) {
#ifdef HAVE_JIT
  bool res;
  i64 pc, jpc;
  unassert(!IsMakingPath(m));
  InitPaths(m->system);
  if (m->path.skip > 0) {
    --m->path.skip;
    return false;
  }
  if ((pc = GetPc(m))) {
    if ((m->path.jb = StartJit(&m->system->jit))) {
      JIT_LOGF("starting new path jit_pc:%" PRIxPTR " at pc:%" PRIx64,
               GetJitPc(m->path.jb), pc);
      FlushCod(m->path.jb);
      jpc = (intptr_t)m->path.jb->addr + m->path.jb->index;
      (void)jpc;
      AppendJit(m->path.jb, kEnter, sizeof(kEnter));
#if LOG_JIX
      Jitter(A,
             "a1i"  // arg1 = ip
             "q"    // arg0 = machine
             "c"    // call function (StartPath)
             "q",   // arg0 = machine
             GetPc(m), StartPath);
#endif
      WriteCod("\nJit_%" PRIx64 "_%" PRIx64 ":\n", pc, jpc);
      FlushCod(m->path.jb);
      m->path.start = pc;
      m->path.elements = 0;
      SetJitHook(&m->system->jit, pc, (intptr_t)JitlessDispatch);
      res = true;
    } else {
      res = false;
    }
  } else {
    res = false;
  }
  return res;
#else
  return false;
#endif
}

void CompletePath(P) {
  unassert(IsMakingPath(m));
  FlushSkew(A);
  AppendJitJump(m->path.jb, (void *)m->system->ender);
  FinishPath(m);
}

void FinishPath(struct Machine *m) {
  unassert(IsMakingPath(m));
  FlushCod(m->path.jb);
  STATISTIC(path_longest_bytes =
                MAX(path_longest_bytes, m->path.jb->index - m->path.jb->start));
  STATISTIC(path_longest = MAX(path_longest, m->path.elements));
  STATISTIC(AVERAGE(path_average_elements, m->path.elements));
  STATISTIC(AVERAGE(path_average_bytes, m->path.jb->index - m->path.jb->start));
  if (FinishJit(&m->system->jit, m->path.jb, m->path.start)) {
    STATISTIC(++path_count);
    JIT_LOGF("staged path to %" PRIx64, m->path.start);
  } else {
    STATISTIC(++path_ooms);
    JIT_LOGF("path starting at %" PRIx64 " ran out of space", m->path.start);
    SetJitHook(&m->system->jit, m->path.start, 0);
  }
  m->path.jb = 0;
}

void AbandonPath(struct Machine *m) {
  WriteCod("/\tABANDONED\n");
  unassert(IsMakingPath(m));
  STATISTIC(++path_abandoned);
  JIT_LOGF("abandoning path jit_pc:%" PRIxPTR " which started at pc:%" PRIx64,
           GetJitPc(m->path.jb), m->path.start);
  AbandonJit(&m->system->jit, m->path.jb);
  SetJitHook(&m->system->jit, m->path.start, 0);
  m->path.skew = 0;
  m->path.jb = 0;
}

void FlushSkew(P) {
  unassert(IsMakingPath(m));
  if (m->path.skew) {
    JIT_LOGF("adding %" PRId64 " to ip", m->path.skew);
    Jitter(A,
           "a1i"  // arg1 = skew
           "q"    // arg0 = machine
           "m",   // arg0 = machine
           m->path.skew, AdvanceIp);
    m->path.skew = 0;
  }
}

void AddPath_StartOp(P) {
#if LOG_CPU
  Jitter(A, "qmq", LogCpu);
#endif
  BeginCod(m, GetPc(m));
#ifndef NDEBUG
  if (FLAG_statistics) {
    Jitter(A,
           "a0i"  // arg0 = &instructions_jitted
           "m",   // call micro-op (CountOp)
           &instructions_jitted, CountOp);
  }
#endif
  if (AddPath_StartOp_Hook) {
    AddPath_StartOp_Hook(A);
  }
#if LOG_JIX || defined(DEBUG)
  Jitter(A,
         "a1i"  // arg1 = m->ip
         "q"    // arg0 = machine
         "c",   // call function (DebugOp)
         m->ip - m->path.skew, DebugOp);
#endif
#if LOG_JIX
  Jitter(A,
         "a1i"  // arg1 = pc
         "q"    // arg0 = machine
         "c",   // call function (StartOp)
         m->ip, StartOp);
#endif
  if (MustUpdateIp(A)) {
    if (!m->path.skew) {
      JIT_LOGF("adding %" PRId64 " to ip", Oplength(rde));
      Jitter(A,
             "a1i"  // arg1 = Oplength(rde)
             "q"    // arg0 = machine
             "m",   // call micro-op (AddIp)
             Oplength(rde), AddIp);
    } else {
      JIT_LOGF("adding %" PRId64 "+%" PRId64 " to ip", m->path.skew,
               Oplength(rde));
      Jitter(A,
             "a2i"  // arg1 = program counter delta
             "a1i"  // arg1 = instruction length
             "q"    // arg0 = machine
             "m",   // call micro-op (SkewIp)
             m->path.skew + Oplength(rde), Oplength(rde), SkewIp);
      m->path.skew = 0;
    }
  } else {
    m->path.skew += Oplength(rde);
  }
  AppendJitMovReg(m->path.jb, kJitArg0, kJitSav0);
  m->reserving = false;
}

void AddPath_EndOp(P) {
  _Static_assert(offsetof(struct Machine, stashaddr) < 128, "");
  if (m->reserving) {
    WriteCod("/\tflush reserve\n");
  }
#if !LOG_JIX && defined(__x86_64__)
  if (m->reserving) {
    AppendJitMovReg(m->path.jb, kJitArg0, kJitSav0);
    u8 sa = offsetof(struct Machine, stashaddr);
    u8 code[] = {
        // cmpq $0x0,0x18(%rdi)
        0x48 | (kJitArg0 > 7 ? kAmdRexb : 0),
        0x83,
        0170 | (kJitArg0 & 7),
        sa,
        0x00,
        // jz +5
        0x74,
        0x05,
    };
    AppendJit(m->path.jb, code, sizeof(code));
    AppendJitCall(m->path.jb, (void *)CommitStash);
  }
#elif !LOG_JIX && defined(__aarch64__)
  if (m->reserving) {
    AppendJitMovReg(m->path.jb, kJitArg0, kJitSav0);
    u32 sa = offsetof(struct Machine, stashaddr);
    u32 code[] = {
        0xf9400001 | (sa / 8) << 10,  // ldr x1, [x0, #stashaddr]
        0xb4000001 | 2 << 5,          // cbz x1, +2
    };
    AppendJit(m->path.jb, code, sizeof(code));
    AppendJitCall(m->path.jb, (void *)CommitStash);
  }
#else
  Jitter(A,
         "a1i"  // arg1 = pc
         "q"    // arg0 = machine
         "c",   // call function (EndOp)
         m->ip, EndOp);
#endif
  FlushCod(m->path.jb);
}

bool AddPath(P) {
  unassert(IsMakingPath(m));
  Jitter(A,
         "a3i"  // arg2 = uimm0
         "a2i"  // arg2 = disp
         "a1i"  // arg1 = rde
         "c",   // call function
         uimm0, disp, rde, GetOp(Mopcode(rde)));
  return true;
}

#endif /* HAVE_JIT */
