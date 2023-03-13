#ifndef BLINK_JIT_H_
#define BLINK_JIT_H_
#include <inttypes.h>
#include <stdbool.h>

#include "blink/atomic.h"
#include "blink/builtin.h"
#include "blink/dll.h"
#include "blink/thread.h"
#include "blink/tunables.h"
#include "blink/types.h"

#define kJitFit          1000
#define kJitDepth        16
#define kJitAlign        16
#define kJitJumpTries    16
#define kJitBlockSize    262144
#define kJitRetireQueue  (int)(kJitMemorySize / kJitBlockSize * .10)
#define kJitInitialHooks 16384

#ifdef __x86_64__
#define kJitMemorySize 130023424
#else
#define kJitMemorySize 32505856
#endif

#ifdef __x86_64__
#define kJitRes0 kAmdAx
#define kJitRes1 kAmdDx
#ifdef __CYGWIN__
#define kJitArg0 kAmdCx
#define kJitArg1 kAmdDx
#define kJitArg2 8
#define kJitArg3 9
#else
#define kJitArg0 kAmdDi
#define kJitArg1 kAmdSi
#define kJitArg2 kAmdDx
#define kJitArg3 kAmdCx
#endif
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
#define kJitSav0 19
#define kJitSav1 20
#define kJitSav2 21
#define kJitSav3 22
#define kJitSav4 23
#endif

#ifdef __x86_64__
#define kAmdXor           0x31
#define kAmdRet           0xc3
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
#define kArmRet      0xd65f03c0u  // RET
#define kArmMovNex   0xf2800000u  // sets sub-word of register to immediate
#define kArmMovZex   0xd2800000u  // load immediate into reg w/ zero-extend
#define kArmMovSex   0x92800000u  // load 1's complement imm w/ sign-extend
#define kArmDispMin  -33554432    // can jump -2**25 ints backward
#define kArmDispMax  +33554431    // can jump +2**25-1 ints forward
#define kArmDispMask 0x03ffffffu  // mask of branch displacement
#define kArmRegOff   0            // bit offset of destination register
#define kArmRegMask  0x0000001fu  // mask of destination register
#define kArmImmOff   5            // bit offset of mov immediate value
#define kArmImmMask  0x001fffe0u  // bit offset of mov immediate value
#define kArmImmMax   0xffffu      // maximum immediate value per instruction
#define kArmIdxOff   21           // bit offset of u16[4] sub-word index
#define kArmIdxMask  0x00600000u  // mask of u16[4] sub-word index
#endif

#define JITJUMP_CONTAINER(e)   DLL_CONTAINER(struct JitJump, elem, e)
#define JITPAGE_CONTAINER(e)   DLL_CONTAINER(struct JitPage, elem, e)
#define JITSTAGE_CONTAINER(e)  DLL_CONTAINER(struct JitStage, elem, e)
#define JITBLOCK_CONTAINER(e)  DLL_CONTAINER(struct JitBlock, elem, e)
#define JITFREED_CONTAINER(e)  DLL_CONTAINER(struct JitFreed, elem, e)
#define AGEDBLOCK_CONTAINER(e) DLL_CONTAINER(struct JitBlock, aged, e)

struct JitJump {
  u8 *code;
  u64 virt;
  long tries;
  long addend;
  struct Dll elem;
};

struct JitStage {
  long start;
  long index;
  u64 virt;
  unsigned pagegen;
  struct Dll elem;
};

struct JitFreed {
  void *data;
  size_t size;
  struct Dll elem;
};

struct JitFreeds {
  int n;
  struct Dll *p;
  struct Dll *f;
};

struct JitBlockPages {
  int i, n;
  i64 *p;
};

struct JitPageEdge {
  short src;
  short dst;
};

struct JitPageEdges {
  int i, n;
  struct JitPageEdge *p;
};

struct JitPage {
  i64 page;
  u64 bitset;
  struct JitPageEdges edges;
  struct Dll elem;
};

struct JitBlock {
  u8 *addr;
  i64 virt;
  long cod;
  long start;
  long index;
  long committed;
  long lastaction;
  bool wasretired;
  bool isprotected;
  unsigned pagegen;
  struct Dll elem;
  struct Dll aged;
  struct Dll *jumps;
  struct Dll *staged;
  struct JitBlockPages pages;
};

struct JitHooks {
  unsigned i;
  _Atomic(unsigned) n;
  _Atomic(_Atomic(int) *) funcs;
  _Atomic(_Atomic(uintptr_t) *) virts;
};

struct Jit {
  int staging;
  bool threaded;
  _Atomic(bool) disabled;
  uintptr_t lastreset;
  struct JitHooks hooks;
  struct JitFreeds freeds;
  struct Dll *agedblocks;
  struct Dll *blocks;
  struct Dll *jumps;
  struct Dll *pages;
  pthread_mutex_t_ lock;
  _Alignas(kSemSize) _Atomic(unsigned) keygen;
  _Alignas(kSemSize) _Atomic(unsigned) pagegen;
};

extern const u8 kJitRes[2];
extern const u8 kJitSav[5];
extern const u8 kJitArg[4];

int ShutdownJit(void);
int EnableJit(struct Jit *);
int DisableJit(struct Jit *);
int DestroyJit(struct Jit *);
int FixJitProtection(struct Jit *);
int InitJit(struct Jit *, uintptr_t);
bool CanJitForImmediateEffect(void) nosideeffect;
bool AppendJit(struct JitBlock *, const void *, long);
bool AbandonJit(struct Jit *, struct JitBlock *);
int FlushJit(struct Jit *);
struct JitBlock *StartJit(struct Jit *, i64);
bool AlignJit(struct JitBlock *, int, int);
bool AppendJitRet(struct JitBlock *);
bool AppendJitNop(struct JitBlock *);
bool AppendJitTrap(struct JitBlock *);
bool AppendJitJump(struct JitBlock *, void *);
bool AppendJitCall(struct JitBlock *, void *);
bool AppendJitSetReg(struct JitBlock *, int, u64);
bool AppendJitMovReg(struct JitBlock *, int, int);
bool FinishJit(struct Jit *, struct JitBlock *);
bool RecordJitJump(struct JitBlock *, u64, int);
bool RecordJitEdge(struct Jit *, i64, i64);
uintptr_t GetJitHook(struct Jit *, u64, uintptr_t);
int ResetJitPage(struct Jit *, i64);

int CommitJit_(struct Jit *, struct JitBlock *);
void ReinsertJitBlock_(struct Jit *, struct JitBlock *);

/**
 * Returns number of bytes of space remaining in JIT memory block.
 *
 * @return number of bytes of space that can be appended into, or -1 if
 *     if an append operation previously failed due to lack of space
 */
static inline long GetJitRemaining(const struct JitBlock *jb) {
  return kJitBlockSize - jb->index;
}

/**
 * Returns current program counter or instruction pointer of JIT block.
 *
 * @return absolute instruction pointer memory address in bytes
 */
static inline uintptr_t GetJitPc(const struct JitBlock *jb) {
  return (uintptr_t)jb->addr + jb->index;
}

/**
 * Returns true if DisableJit() was called or AcquireJit() had failed.
 */
static inline bool IsJitDisabled(const struct Jit *jit) {
#ifdef HAVE_JIT
  return atomic_load_explicit(&jit->disabled, memory_order_acquire);
#else
  return true;
#endif
}

#endif /* BLINK_JIT_H_ */
