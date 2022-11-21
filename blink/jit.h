#ifndef BLINK_JIT_H_
#define BLINK_JIT_H_
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>

#include "blink/dll.h"
#include "blink/tsan.h"
#include "blink/types.h"

#define kJitPageSize  65536
#define kJitPageFit   600
#define kJitPageAlign 16

#define kAmdXor      0x31
#define kAmdJmp      0xe9
#define kAmdCall     0xe8
#define kAmdJmpAx    "\377\340"
#define kAmdCallAx   "\377\320"
#define kAmdDispMin  INT32_MIN
#define kAmdDispMax  INT32_MAX
#define kAmdDispMask 0xffffffffu
#define kAmdRex      0x40  // turns ah/ch/dh/bh into spl/bpl/sil/dil
#define kAmdRexb     0x41  // turns 0007 (r/m) of modrm into r8..r15
#define kAmdRexr     0x44  // turns 0070 (reg) of modrm into r8..r15
#define kAmdRexw     0x48  // makes instruction 64-bit
#define kAmdMovImm   0xb8
#define kAmdAx       0  // first function result
#define kAmdCx       1  // third function parameter
#define kAmdDx       2  // fourth function parameter, second function result
#define kAmdBx       3  // generic saved register
#define kAmdSp       4  // stack pointer
#define kAmdBp       5  // backtrace pointer
#define kAmdSi       6  // second function parameter
#define kAmdDi       7  // first function parameter

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

#define JITPAGE_CONTAINER(e) DLL_CONTAINER(struct JitPage, list, e)

struct JitPage {
  u8 *addr;
  int start;
  int index;
  int saved;
  int setargs;
  int committed;
  dll_list staged;
  dll_element list;
};

struct Jit {
  u8 *brk;
  pthread_mutex_t lock;
  _Atomic(int) disabled;
  dll_list pages GUARDED_BY(lock);
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
