#ifndef BLINK_MACHINE_H_
#define BLINK_MACHINE_H_
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>

#include "blink/dll.h"
#include "blink/elf.h"
#include "blink/fds.h"
#include "blink/jit.h"
#include "blink/linux.h"
#include "blink/tsan.h"
#include "blink/x86.h"

#define kParamRde   1
#define kParamDisp  2
#define kParamUimm0 3
#define kParamArg   4

#define kOpNormal    0
#define kOpBranching 1
#define kOpPrecious  2

#define kInstructionBytes 40

#define kMachineExit                 256
#define kMachineHalt                 -1
#define kMachineDecodeError          -2
#define kMachineUndefinedInstruction -3
#define kMachineSegmentationFault    -4
#define kMachineDivideError          -6
#define kMachineFpuException         -7
#define kMachineProtectionFault      -8
#define kMachineSimdException        -9

#define P                struct Machine *m, u64 rde, i64 disp, u64 uimm0
#define A                m, rde, disp, uimm0
#define DISPATCH_NOTHING m, 0, 0, 0

#define MACHINE_CONTAINER(e) DLL_CONTAINER(struct Machine, elem, e)

struct Machine;

typedef void (*nexgen32e_f)(P);

struct FreeList {
  int n;
  void **p;
};

struct SystemReal {
  long i;
  long n;  // monotonic
  u8 *p;   // never relocates
};

struct SystemRealFree {
  long i;
  long n;
  struct SystemRealFree *next;
};

struct MachineFpu {
  double st[8];
  u32 cw;
  u32 sw;
  int tw;
  int op;
  i64 ip;
  i64 dp;
};

struct MachineMemstat {
#if !defined(__m68k__) && !defined(__mips__)
  _Atomic(int) freed;
  _Atomic(int) resizes;
  _Atomic(int) reserved;
  _Atomic(int) committed;
  _Atomic(int) allocated;
  _Atomic(int) reclaimed;
  _Atomic(int) pagetables;
#else
  int freed;
  int resizes;
  int reserved;
  int committed;
  int allocated;
  int reclaimed;
  int pagetables;
#endif
};

struct MachineState {
  u64 ip;
  u64 cs;
  u64 ss;
  u64 es;
  u64 ds;
  u64 fs;
  u64 gs;
  u8 weg[16][8];
  u8 xmm[16][16];
  u32 mxcsr;
  struct MachineFpu fpu;
  struct MachineMemstat memstat;
};

struct Elf {
  const char *prog;
  Elf64_Ehdr *ehdr;
  long size;
  i64 base;
  char *map;
  long mapsize;
  bool debugonce;
};

struct OpCache {
  u8 stash[16];   // for memory ops that overlap page
  u64 codevirt;   // current rip page in guest memory
  u8 *codehost;   // current rip page in host memory
  u32 stashsize;  // for writes that overlap page
  bool writable;
  u64 icache[1024][kInstructionBytes / 8];
};

struct System {
  u64 cr3;
  i64 brk;
  bool dlab;
  bool isfork;
  i64 codestart;
  unsigned long codesize;
  pthread_mutex_t real_lock;
  _Atomic(nexgen32e_f) *fun;
  struct SystemReal real GUARDED_BY(real_lock);
  pthread_mutex_t realfree_lock;
  struct SystemRealFree *realfree PT_GUARDED_BY(realfree_lock);
  struct MachineMemstat memstat;
  pthread_mutex_t machines_lock;
  struct Dll *machines GUARDED_BY(machines_lock);
  unsigned next_tid GUARDED_BY(machines_lock);
  struct Jit jit;
  struct Fds fds;
  struct Elf elf;
  pthread_mutex_t sig_lock;
  struct sigaction_linux hands[32] GUARDED_BY(sig_lock);
  void (*onbinbase)(struct Machine *);
  void (*onlongbranch)(struct Machine *);
  int (*exec)(char *, char **, char **);
  void (*redraw)(void);
  u64 gdt_base;
  u64 idt_base;
  u16 gdt_limit;
  u16 idt_limit;
  u64 cr0;
  u64 cr2;
  u64 cr4;
};

struct Path {
  i64 start;
  int elements;
  struct JitPage *jp;
};

struct MachineTlb {
  i64 page;
  u64 entry;
};

struct Machine {                           //
  _Atomic(nexgen32e_f) *fun;               // DISPATCHER
  u64 ip;                                  //
  u64 oldip;                               //
  i64 stashaddr;                           //
  u64 cs;                                  //
  u64 ss;                                  //
  int mode;                                //
  u32 flags;                               //
  union {                                  // GENERAL REGISTER FILE
    u64 align8_;                           //
    u8 beg[128];                           //
    u8 weg[16][8];                         //
    struct {                               //
      union {                              //
        u8 ax[8];                          // [vol] accumulator, result:1/2
        struct {                           //
          u8 al;                           //
          u8 ah;                           //
        };                                 //
      };                                   //
      union {                              //
        u8 cx[8];                          // [vol] param:4/6
        struct {                           //
          u8 cl;                           //
          u8 ch;                           //
        };                                 //
      };                                   //
      union {                              //
        u8 dx[8];                          // [vol] param:3/6, result:2/2
        struct {                           //
          u8 dl;                           //
          u8 dh;                           //
        };                                 //
      };                                   //
      union {                              //
        u8 bx[8];                          // [sav] base index
        struct {                           //
          u8 bl;                           //
          u8 bh;                           //
        };                                 //
      };                                   //
      u8 sp[8];                            // [sav] stack pointer
      u8 bp[8];                            // [sav] backtrace pointer
      u8 si[8];                            // [vol] param:2/6
      u8 di[8];                            // [vol] param:1/6
      u8 r8[8];                            // [vol] param:5/6
      u8 r9[8];                            // [vol] param:6/6
      u8 r10[8];                           // [vol]
      u8 r11[8];                           // [vol]
      u8 r12[8];                           // [sav]
      u8 r13[8];                           // [sav]
      u8 r14[8];                           // [sav]
      u8 r15[8];                           // [sav]
    };                                     //
  };                                       //
  _Alignas(64) struct MachineTlb tlb[16];  // TRANSLATION LOOKASIDE BUFFER
  _Alignas(16) u8 xmm[16][16];             // 128-BIT VECTOR REGISTER FILE
  struct XedDecodedInst *xedd;             // ->opcache->icache if non-jit
  i64 readaddr;                            // so tui can show memory reads
  i64 writeaddr;                           // so tui can show memory write
  u32 readsize;                            // bytes length of last read op
  u32 writesize;                           // byte length of last write op
  u64 fs;                                  // thred-local segment register
  u64 gs;                                  // winple thread-local register
  u64 ds;                                  // data segment (legacy / real)
  u64 es;                                  // xtra segment (legacy / real)
  struct MachineFpu fpu;                   // FLOATING-POINT REGISTER FILE
  _Atomic(int) tlb_invalidated;            // 1 if thread should clear tlb
  u32 mxcsr;                               // SIMD status control register
  pthread_t thread;                        // POSIX thread of this machine
  struct FreeList freelist;                // to make system calls simpler
  struct Path path;                        // under construction jit route
  i64 bofram[2];                           // helps debug bootloading code
  i64 faultaddr;                           // used for tui error reporting
  u64 signals;                             // signals waiting for delivery
  u64 sigmask;                             // signals that've been blocked
  u32 tlbindex;                            //
  int sig;                                 //
  u64 siguc;                               //
  u64 sigfp;                               //
  struct System *system;                   //
  jmp_buf onhalt;                          //
  i64 ctid;                                //
  int tid;                                 //
  sigset_t thread_sigmask;                 //
  struct Dll elem GUARDED_BY(system->machines_lock);
  struct OpCache opcache[1];
};

extern _Thread_local struct Machine *g_machine;

struct System *NewSystem(void);
void FreeSystem(struct System *);
_Noreturn void Actor(struct Machine *);
struct Machine *NewMachine(struct System *, struct Machine *);
void FreeMachine(struct Machine *);
void FreeMachineUnlocked(struct Machine *);
void KillOtherThreads(struct System *);
void ResetMem(struct Machine *);
void ResetCpu(struct Machine *);
void ResetTlb(struct Machine *);
void CollectGarbage(struct Machine *);
void ResetInstructionCache(struct Machine *);
void GeneralDispatch(P);
nexgen32e_f GetOp(long);
void LoadInstruction(struct Machine *);
void ExecuteInstruction(struct Machine *);
long AllocateLinearPage(struct System *);
long AllocateLinearPageRaw(struct System *);
int ReserveReal(struct System *, long);
int ReserveVirtual(struct System *, i64, size_t, u64);
char *FormatPml4t(struct Machine *);
i64 FindVirtual(struct System *, i64, size_t);
int FreeVirtual(struct System *, i64, size_t);
void LoadArgv(struct Machine *, char *, char **, char **);
_Noreturn void HaltMachine(struct Machine *, int);
void RaiseDivideError(struct Machine *);
_Noreturn void ThrowSegmentationFault(struct Machine *, i64);
_Noreturn void ThrowProtectionFault(struct Machine *);
_Noreturn void OpUdImpl(struct Machine *);
_Noreturn void OpUd(P);
_Noreturn void OpHlt(P);
void JitlessDispatch(P);

void Push(P, u64);
u64 Pop(P, u16);
void PopVq(P);
void PushVq(P);
int OpOut(struct Machine *, u16, u32);
u64 OpIn(struct Machine *, u16);

void Op0fe(P);
void Op101(P);
void Op171(P);
void Op172(P);
void Op173(P);
void OpAaa(P);
void OpAad(P);
void OpAam(P);
void OpAas(P);
void OpAlubAdc(P);
void OpAlubAdd(P);
void OpAlubAnd(P);
void OpAlubOr(P);
void OpAlubSbb(P);
void OpAlubSub(P);
void OpAlubXor(P);
void OpAluw(P);
void OpCallEq(P);
void OpCallJvds(P);
void OpCallf(P);
void OpCmpxchgEbAlGb(P);
void OpCmpxchgEvqpRaxGvqp(P);
void OpCpuid(P);
void OpCvt0f2a(P);
void OpCvt0f2d(P);
void OpCvt0f5a(P);
void OpCvt0f5b(P);
void OpCvt0fE6(P);
void OpCvtt0f2c(P);
void OpDas(P);
void OpDecEvqp(P);
void OpDivAlAhAxEbSigned(P);
void OpDivAlAhAxEbUnsigned(P);
void OpDivRdxRaxEvqpSigned(P);
void OpDivRdxRaxEvqpUnsigned(P);
void OpImulGvqpEvqp(P);
void OpImulGvqpEvqpImm(P);
void OpIncEvqp(P);
void OpJmpEq(P);
void OpLeave(P);
void OpMulAxAlEbSigned(P);
void OpMulAxAlEbUnsigned(P);
void OpMulRdxRaxEvqpSigned(P);
void OpMulRdxRaxEvqpUnsigned(P);
void OpNegEb(P);
void OpNegEvqp(P);
void OpNotEb(P);
void OpNotEvqp(P);
void OpPopEvq(P);
void OpPopZvq(P);
void OpPopa(P);
void OpPushEvq(P);
void OpPushZvq(P);
void OpPusha(P);
void OpRdrand(P);
void OpRdseed(P);
void OpRet(P);
void OpRetf(P);
void OpSsePclmulqdq(P);
void OpXaddEbGb(P);
void OpXaddEvqpGvqp(P);
void OpXchgGbEb(P);
void OpXchgGvqpEvqp(P);

#endif /* BLINK_MACHINE_H_ */
