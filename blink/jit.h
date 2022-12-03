#ifndef BLINK_JIT_H_
#define BLINK_JIT_H_
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>

#include "blink/dll.h"
#include "blink/types.h"

#define kJitPageSize  65536
#define kJitPageFit   600
#define kJitPageAlign 16

#ifdef __x86_64__
#define kJitRes0 kAmdAx
#define kJitRes1 kAmdDx
#define kJitArg0 kAmdDi
#define kJitArg1 kAmdSi
#define kJitArg2 kAmdDx
#define kJitArg3 kAmdCx
#define kJitArg4 8
#define kJitArg5 9
#define kJitSav0 kAmdBx
#define kJitSav1 12
#define kJitSav2 13
#define kJitSav3 14
#define kJitSav4 15
#else
#define kJitRes0 0
#define kJitRes1 1
#define kJitArg0 0
#define kJitArg1 1
#define kJitArg2 2
#define kJitArg3 3
#define kJitArg4 4
#define kJitArg5 5
#define kJitSav0 19
#define kJitSav1 20
#define kJitSav2 21
#define kJitSav3 22
#define kJitSav4 23
#endif

#ifdef __x86_64__
#define kAmdXor           0x31
#define kAmdJmp           0xe9
#define kAmdCall          0xe8
#define kAmdJmpAx         "\377\340"
#define kAmdCallAx        "\377\320"
#define kAmdDispMin       INT32_MIN
#define kAmdDispMax       INT32_MAX
#define kAmdDispMask      0xffffffffu
#define kAmdRex           0x40  // turns ah/ch/dh/bh into spl/bpl/sil/dil
#define kAmdRexb          0x41  // turns 0007 (r/m) of modrm into r8..r15
#define kAmdRexr          0x44  // turns 0070 (reg) of modrm into r8..r15
#define kAmdRexw          0x48  // makes register 64-bit
#define kAmdAx            0     // first function result
#define kAmdCx            1     // third function parameter
#define kAmdDx            2     // fourth function parameter, second result
#define kAmdBx            3     // generic saved register
#define kAmdSp            4     // stack pointer
#define kAmdBp            5     // backtrace pointer
#define kAmdSi            6     // second function parameter
#define kAmdDi            7     // first function parameter
#define kAmdWord          1     // turns `op Eb,Gb` into `op Evqp,Gvqp`
#define kAmdFlip          2     // turns `op r/m,r` into `op r,r/m`
#define kAmdModrmRdiDisp8 0107  // ModR/M [-128,127](%rdi)
#define kAmdMovImm        0xb8  // or'd with register, imm size is same as reg
#define kAmdMov           0x88  // may be or'd with kAmdWord, kAmdFlip
#endif

#ifdef __aarch64__
#define kArmJmp      0x14000000u  // B
#define kArmCall     0x94000000u  // BL
#define kArmMovNex   0xf2800000u  // sets sub-word of register to immediate
#define kArmMovZex   0xd2800000u  // load immediate into reg w/ zero-extend
#define kArmMovSex   0x92800000u  // load 1's complement imm w/ sign-extend
#define kArmDispMin  -33554432    // can jump -2**25 ints backward
#define kArmDispMax  +33554431    // can jump +2**25-1 ints forward
#define kArmDispMask 0x03ffffff   // mask of branch displacement
#define kArmRegOff   0            // bit offset of destination register
#define kArmRegMask  0x0000001fu  // mask of destination register
#define kArmImmOff   5            // bit offset of mov immediate value
#define kArmImmMask  0x001fffe0u  // bit offset of mov immediate value
#define kArmImmMax   0xffffu      // maximum immediate value per instruction
#define kArmIdxOff   21           // bit offset of u16[4] sub-word index
#define kArmIdxMask  0x00600000u  // mask of u16[4] sub-word index
#endif

#define JITPAGE_CONTAINER(e) DLL_CONTAINER(struct JitPage, elem, e)

struct JitPage {
  u8 *addr;
  int start;
  int index;
  int saved;
  int setargs;
  int committed;
  struct Dll *staged;
  struct Dll elem;
};

struct Jit {
  u8 *brk;
  pthread_mutex_t lock;
  _Atomic(int) disabled;
  struct Dll *pages;
};

typedef _Atomic(intptr_t) hook_t;

int InitJit(struct Jit *);
int DestroyJit(struct Jit *);
int DisableJit(struct Jit *);
bool IsJitDisabled(struct Jit *);
struct JitPage *AcquireJit(struct Jit *, long);
intptr_t GetJitPc(struct JitPage *);
long GetJitRemaining(struct JitPage *);
bool AppendJit(struct JitPage *, const void *, long);
intptr_t ReleaseJit(struct Jit *, struct JitPage *, hook_t *, intptr_t);
int AbandonJit(struct Jit *, struct JitPage *);
int FlushJit(struct Jit *);
struct JitPage *StartJit(struct Jit *);
bool AppendJitJmp(struct JitPage *, void *);
bool AppendJitCall(struct JitPage *, void *);
bool AppendJitSetReg(struct JitPage *, int, u64);
bool AppendJitSetArg(struct JitPage *, int, u64);
bool AppendJitMovReg(struct JitPage *, int, int);
intptr_t FinishJit(struct Jit *, struct JitPage *, hook_t *, intptr_t);
intptr_t SpliceJit(struct Jit *, struct JitPage *, hook_t *, intptr_t,
                   intptr_t);

#endif /* BLINK_JIT_H_ */
