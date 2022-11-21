#ifndef BLINK_MACHINE_H_
#define BLINK_MACHINE_H_
#include <pthread.h>
#include <setjmp.h>
#include <stdbool.h>

#include "blink/dll.h"
#include "blink/fds.h"
#include "blink/linux.h"
#include "blink/tsan.h"
#include "blink/x86.h"

#define kMachineHalt                 -1
#define kMachineDecodeError          -2
#define kMachineUndefinedInstruction -3
#define kMachineSegmentationFault    -4
#define kMachineExit                 -5
#define kMachineDivideError          -6
#define kMachineFpuException         -7
#define kMachineProtectionFault      -8
#define kMachineSimdException        -9

#define kInstructionBytes 40

#define MACHINE_CONTAINER(e) DLL_CONTAINER(struct Machine, list, e)

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

struct OpCache {
  u64 codevirt;    // current rip page in guest memory
  u8 *codehost;    // current rip page in host memory
  u32 stashsize;   // for writes that overlap page
  i64 stashaddr;   // for writes that overlap page
  u8 stash[4096];  // for writes that overlap page
  u64 icache[1024][kInstructionBytes / 8];
};

struct Machine;

struct System {
  u64 cr3;
  i64 brk;
  bool dlab;
  bool isfork;
  pthread_mutex_t real_lock;
  struct SystemReal real GUARDED_BY(real_lock);
  pthread_mutex_t realfree_lock;
  struct SystemRealFree *realfree PT_GUARDED_BY(realfree_lock);
  struct MachineMemstat memstat;
  pthread_mutex_t machines_lock;
  dll_list machines GUARDED_BY(machines_lock);
  unsigned next_tid GUARDED_BY(machines_lock);
  struct Fds fds;
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

struct MachineTlb {
  i64 virt;
  u64 entry;
};

struct Machine {
  struct XedDecodedInst *xedd;
  u64 ip;
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
_Noreturn void OpUd(struct Machine *, u64);
_Noreturn void OpHlt(struct Machine *, u64);
void OpSsePclmulqdq(struct Machine *, u64);
void OpCvt0f2a(struct Machine *, u64);
void OpCvtt0f2c(struct Machine *, u64);
void OpCvt0f2d(struct Machine *, u64);
void OpCvt0f5a(struct Machine *, u64);
void OpCvt0f5b(struct Machine *, u64);
void OpCvt0fE6(struct Machine *, u64);
void OpCpuid(struct Machine *, u64);
void OpDivAlAhAxEbSigned(struct Machine *, u64);
void OpDivAlAhAxEbUnsigned(struct Machine *, u64);
void OpDivRdxRaxEvqpSigned(struct Machine *, u64);
void OpDivRdxRaxEvqpUnsigned(struct Machine *, u64);
void OpImulGvqpEvqp(struct Machine *, u64);
void OpImulGvqpEvqpImm(struct Machine *, u64);
void OpMulAxAlEbSigned(struct Machine *, u64);
void OpMulAxAlEbUnsigned(struct Machine *, u64);
void OpMulRdxRaxEvqpSigned(struct Machine *, u64);
void OpMulRdxRaxEvqpUnsigned(struct Machine *, u64);
u64 OpIn(struct Machine *, u16);
int OpOut(struct Machine *, u16, u32);
void Op101(struct Machine *, u64);
void OpRdrand(struct Machine *, u64);
void OpRdseed(struct Machine *, u64);
void Op171(struct Machine *, u64);
void Op172(struct Machine *, u64);
void Op173(struct Machine *, u64);
void Push(struct Machine *, u64, u64);
u64 Pop(struct Machine *, u64, u16);
void OpCallJvds(struct Machine *, u64);
void OpRet(struct Machine *, u64);
void OpRetf(struct Machine *, u64);
void OpLeave(struct Machine *, u64);
void OpCallEq(struct Machine *, u64);
void OpPopEvq(struct Machine *, u64);
void OpPopZvq(struct Machine *, u64);
void OpPushZvq(struct Machine *, u64);
void OpPushEvq(struct Machine *, u64);
void PopVq(struct Machine *, u64);
void PushVq(struct Machine *, u64);
void OpJmpEq(struct Machine *, u64);
void OpPusha(struct Machine *, u64);
void OpPopa(struct Machine *, u64);
void OpCallf(struct Machine *, u64);
void OpXchgGbEb(struct Machine *, u64);
void OpXchgGvqpEvqp(struct Machine *, u64);
void OpCmpxchgEbAlGb(struct Machine *, u64);
void OpCmpxchgEvqpRaxGvqp(struct Machine *, u64);

#endif /* BLINK_MACHINE_H_ */
