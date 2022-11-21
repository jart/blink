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

#define DISPATCH_PARAMETERS u64 rde, i64 disp, u64 uimm0
#define DISPATCH_ARGUMENTS  rde, disp, uimm0
#define DISPATCH_NOTHING    0, 0, 0

#define MACHINE_CONTAINER(e) DLL_CONTAINER(struct Machine, list, e)

struct Machine;

typedef void (*nexgen32e_f)(struct Machine *, DISPATCH_PARAMETERS);

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
  u8 reg[16][8];
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
  bool writable;
  u64 codevirt;   // current rip page in guest memory
  u8 *codehost;   // current rip page in host memory
  u32 stashsize;  // for writes that overlap page
  u8 stash[16];   // for writes that overlap page
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
  _Atomic(nexgen32e_f) * fun;
  struct SystemReal real GUARDED_BY(real_lock);
  pthread_mutex_t realfree_lock;
  struct SystemRealFree *realfree PT_GUARDED_BY(realfree_lock);
  struct MachineMemstat memstat;
  pthread_mutex_t machines_lock;
  dll_list machines GUARDED_BY(machines_lock);
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
  i64 virt;
  u64 entry;
};

struct Machine {
  _Atomic(nexgen32e_f) * fun;
  u64 ip;         // 0x08
  u64 oldip;      // 0x10
  i64 stashaddr;  // 0x18
  u64 cs;
  u64 ss;
  int mode;
  u32 flags;
  union {
    u64 align8_;
    u8 reg[16][8];
    struct {
      union {
        struct {
          u8 al;
          u8 ah;
        };
        u8 ax[8];
      };
      union {
        struct {
          u8 cl;
          u8 ch;
        };
        u8 cx[8];
      };
      union {
        struct {
          u8 dl;
          u8 dh;
        };
        u8 dx[8];
      };
      union {
        struct {
          u8 bl;
          u8 bh;
        };
        u8 bx[8];
      };
      u8 sp[8];
      u8 bp[8];
      u8 si[8];
      u8 di[8];
      u8 r8[8];
      u8 r9[8];
      u8 r10[8];
      u8 r11[8];
      u8 r12[8];
      u8 r13[8];
      u8 r14[8];
      u8 r15[8];
    };
  };
  _Alignas(64) struct MachineTlb tlb[16];
  _Alignas(16) u8 xmm[16][16];
  struct XedDecodedInst *xedd;
  dll_element list GUARDED_BY(system->machines_lock);
  i64 readaddr;
  i64 writeaddr;
  u32 readsize;
  u32 writesize;
  u64 es;
  u64 ds;
  u64 fs;
  u64 gs;
  struct MachineFpu fpu;
  u32 mxcsr;
  bool isthread;
  pthread_t thread;
  struct FreeList freelist;
  struct Path path;
  i64 bofram[2];
  i64 faultaddr;
  u64 signals;
  u64 sigmask;
  u32 tlbindex;
  int sig;
  u64 siguc;
  u64 sigfp;
  struct System *system;
  jmp_buf onhalt;
  i64 ctid;
  int tid;
  sigset_t thread_sigmask;
  struct OpCache opcache[1];
};

extern _Thread_local struct Machine *g_machine;

struct System *NewSystem(void);
void FreeSystem(struct System *);
_Noreturn void Actor(struct Machine *);
struct Machine *NewMachine(struct System *, struct Machine *);
void FreeMachine(struct Machine *);
void ResetMem(struct Machine *);
void ResetCpu(struct Machine *);
void ResetTlb(struct Machine *);
void CollectGarbage(struct Machine *);
void ResetInstructionCache(struct Machine *);
void GeneralDispatch(struct Machine *, DISPATCH_PARAMETERS);
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
_Noreturn void OpUd(struct Machine *, DISPATCH_PARAMETERS);
_Noreturn void OpHlt(struct Machine *, DISPATCH_PARAMETERS);
void OpSsePclmulqdq(struct Machine *, DISPATCH_PARAMETERS);
void OpCvt0f2a(struct Machine *, DISPATCH_PARAMETERS);
void OpCvtt0f2c(struct Machine *, DISPATCH_PARAMETERS);
void OpCvt0f2d(struct Machine *, DISPATCH_PARAMETERS);
void OpCvt0f5a(struct Machine *, DISPATCH_PARAMETERS);
void OpCvt0f5b(struct Machine *, DISPATCH_PARAMETERS);
void OpCvt0fE6(struct Machine *, DISPATCH_PARAMETERS);
void OpCpuid(struct Machine *, DISPATCH_PARAMETERS);
void OpDivAlAhAxEbSigned(struct Machine *, DISPATCH_PARAMETERS);
void OpDivAlAhAxEbUnsigned(struct Machine *, DISPATCH_PARAMETERS);
void OpDivRdxRaxEvqpSigned(struct Machine *, DISPATCH_PARAMETERS);
void OpDivRdxRaxEvqpUnsigned(struct Machine *, DISPATCH_PARAMETERS);
void OpImulGvqpEvqp(struct Machine *, DISPATCH_PARAMETERS);
void OpImulGvqpEvqpImm(struct Machine *, DISPATCH_PARAMETERS);
void OpMulAxAlEbSigned(struct Machine *, DISPATCH_PARAMETERS);
void OpMulAxAlEbUnsigned(struct Machine *, DISPATCH_PARAMETERS);
void OpMulRdxRaxEvqpSigned(struct Machine *, DISPATCH_PARAMETERS);
void OpMulRdxRaxEvqpUnsigned(struct Machine *, DISPATCH_PARAMETERS);
u64 OpIn(struct Machine *, u16);
int OpOut(struct Machine *, u16, u32);
void Op101(struct Machine *, DISPATCH_PARAMETERS);
void OpRdrand(struct Machine *, DISPATCH_PARAMETERS);
void OpRdseed(struct Machine *, DISPATCH_PARAMETERS);
void Op171(struct Machine *, DISPATCH_PARAMETERS);
void Op172(struct Machine *, DISPATCH_PARAMETERS);
void Op173(struct Machine *, DISPATCH_PARAMETERS);
void Push(struct Machine *, DISPATCH_PARAMETERS, u64);
u64 Pop(struct Machine *, DISPATCH_PARAMETERS, u16);
void OpCallJvds(struct Machine *, DISPATCH_PARAMETERS);
void OpRet(struct Machine *, DISPATCH_PARAMETERS);
void OpRetf(struct Machine *, DISPATCH_PARAMETERS);
void OpLeave(struct Machine *, DISPATCH_PARAMETERS);
void OpCallEq(struct Machine *, DISPATCH_PARAMETERS);
void OpPopEvq(struct Machine *, DISPATCH_PARAMETERS);
void OpPopZvq(struct Machine *, DISPATCH_PARAMETERS);
void OpPushZvq(struct Machine *, DISPATCH_PARAMETERS);
void OpPushEvq(struct Machine *, DISPATCH_PARAMETERS);
void PopVq(struct Machine *, DISPATCH_PARAMETERS);
void PushVq(struct Machine *, DISPATCH_PARAMETERS);
void OpJmpEq(struct Machine *, DISPATCH_PARAMETERS);
void OpPusha(struct Machine *, DISPATCH_PARAMETERS);
void OpPopa(struct Machine *, DISPATCH_PARAMETERS);
void OpCallf(struct Machine *, DISPATCH_PARAMETERS);
void OpXchgGbEb(struct Machine *, DISPATCH_PARAMETERS);
void OpXchgGvqpEvqp(struct Machine *, DISPATCH_PARAMETERS);
void OpCmpxchgEbAlGb(struct Machine *, DISPATCH_PARAMETERS);
void OpCmpxchgEvqpRaxGvqp(struct Machine *, DISPATCH_PARAMETERS);
void JitlessDispatch(struct Machine *, DISPATCH_PARAMETERS);

#endif /* BLINK_MACHINE_H_ */
