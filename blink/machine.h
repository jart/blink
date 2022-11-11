#ifndef BLINK_MACHINE_H_
#define BLINK_MACHINE_H_
#include <pthread.h>
#include <setjmp.h>
#include <stdbool.h>

#include "blink/dll.h"
#include "blink/fds.h"
#include "blink/linux.h"
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

struct FreeList {
  size_t n;
  void **p;
};

struct MachineReal {
  size_t i, n;
  uint8_t *p;
};

struct MachineRealFree {
  uint64_t i;
  uint64_t n;
  struct MachineRealFree *next;
};

struct MachineFpu {
  double st[8];
  uint32_t cw;
  uint32_t sw;
  int tw;
  int op;
  int64_t ip;
  int64_t dp;
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
  uint64_t ip;
  uint8_t cs[8];
  uint8_t ss[8];
  uint8_t es[8];
  uint8_t ds[8];
  uint8_t fs[8];
  uint8_t gs[8];
  uint8_t reg[16][8];
  uint8_t xmm[16][16];
  uint32_t mxcsr;
  struct MachineFpu fpu;
  struct MachineMemstat memstat;
};

struct OpCache {
  uint64_t codevirt;   /* current rip page in guest memory */
  uint8_t *codehost;   /* current rip page in host memory */
  uint32_t stashsize;  /* for writes that overlap page */
  int64_t stashaddr;   /* for writes that overlap page */
  uint8_t stash[4096]; /* for writes that overlap page */
  uint64_t icache[1024][kInstructionBytes / 8];
};

struct Machine;

struct System {
  uint64_t cr3;
  bool dlab;
  bool isfork;
  pthread_mutex_t lock;
  struct MachineReal real;
  struct MachineRealFree *realfree;
  struct MachineMemstat memstat;
  struct MachineFds fds;
  struct sigaction_linux hands[32];
  int64_t brk;
  void (*onbinbase)(struct Machine *);
  void (*onlongbranch)(struct Machine *);
  int (*exec)(char *, char **, char **);
  void (*redraw)(void);
  uint64_t gdt_base;
  uint64_t idt_base;
  uint16_t gdt_limit;
  uint16_t idt_limit;
  uint64_t cr0;
  uint64_t cr2;
  uint64_t cr4;
};

struct Machine {
  struct XedDecodedInst *xedd;
  struct OpCache *opcache;
  uint64_t ip;
  uint8_t cs[8];
  uint8_t ss[8];
  uint32_t mode;
  uint32_t flags;
  uint32_t tlbindex;
  int64_t readaddr;
  int64_t writeaddr;
  uint32_t readsize;
  uint32_t writesize;
  union {
    uint8_t reg[16][8];
    struct {
      uint8_t ax[8];
      uint8_t cx[8];
      uint8_t dx[8];
      uint8_t bx[8];
      uint8_t sp[8];
      uint8_t bp[8];
      uint8_t si[8];
      uint8_t di[8];
      uint8_t r8[8];
      uint8_t r9[8];
      uint8_t r10[8];
      uint8_t r11[8];
      uint8_t r12[8];
      uint8_t r13[8];
      uint8_t r14[8];
      uint8_t r15[8];
    };
  };
  struct MachineTlb {
    int64_t virt;
    uint64_t entry;
  } tlb[16];
  uint8_t xmm[16][16];
  uint8_t es[8];
  uint8_t ds[8];
  uint8_t fs[8];
  uint8_t gs[8];
  struct MachineFpu fpu;
  uint32_t mxcsr;
  struct FreeList freelist;
  int64_t bofram[2];
  int64_t faultaddr;
  uint8_t sigmask[8];
  int sig;
  uint64_t siguc;
  uint64_t sigfp;
  struct System *system;
  jmp_buf onhalt;
  pthread_mutex_t lock;
  int64_t ctid;
  _Atomic(int) tid;
};

struct Machine *NewMachine(void);
void FreeMachine(struct Machine *);
void ResetMem(struct Machine *);
void ResetCpu(struct Machine *);
void ResetTlb(struct Machine *);
void ResetInstructionCache(struct Machine *);
void LoadInstruction(struct Machine *);
void ExecuteInstruction(struct Machine *);
long AllocateLinearPage(struct Machine *);
long AllocateLinearPageRaw(struct Machine *);
int ReserveReal(struct Machine *, size_t);
int ReserveVirtual(struct Machine *, int64_t, size_t, uint64_t);
char *FormatPml4t(struct Machine *);
int64_t FindVirtual(struct Machine *, int64_t, size_t);
int FreeVirtual(struct Machine *, int64_t, size_t);
void LoadArgv(struct Machine *, char *, char **, char **);
_Noreturn void HaltMachine(struct Machine *, int);
_Noreturn void ThrowDivideError(struct Machine *);
_Noreturn void ThrowSegmentationFault(struct Machine *, int64_t);
_Noreturn void ThrowProtectionFault(struct Machine *);
_Noreturn void OpUd(struct Machine *, uint32_t);
_Noreturn void OpHlt(struct Machine *, uint32_t);
void OpSyscall(struct Machine *, uint32_t);
void OpSsePclmulqdq(struct Machine *, uint32_t);
void OpCvt0f2a(struct Machine *, uint32_t);
void OpCvtt0f2c(struct Machine *, uint32_t);
void OpCvt0f2d(struct Machine *, uint32_t);
void OpCvt0f5a(struct Machine *, uint32_t);
void OpCvt0f5b(struct Machine *, uint32_t);
void OpCvt0fE6(struct Machine *, uint32_t);
void OpCpuid(struct Machine *, uint32_t);
void OpDivAlAhAxEbSigned(struct Machine *, uint32_t);
void OpDivAlAhAxEbUnsigned(struct Machine *, uint32_t);
void OpDivRdxRaxEvqpSigned(struct Machine *, uint32_t);
void OpDivRdxRaxEvqpUnsigned(struct Machine *, uint32_t);
void OpImulGvqpEvqp(struct Machine *, uint32_t);
void OpImulGvqpEvqpImm(struct Machine *, uint32_t);
void OpMulAxAlEbSigned(struct Machine *, uint32_t);
void OpMulAxAlEbUnsigned(struct Machine *, uint32_t);
void OpMulRdxRaxEvqpSigned(struct Machine *, uint32_t);
void OpMulRdxRaxEvqpUnsigned(struct Machine *, uint32_t);
uint64_t OpIn(struct Machine *, uint16_t);
void OpOut(struct Machine *, uint16_t, uint32_t);
void Op101(struct Machine *, uint32_t);
void OpRdrand(struct Machine *, uint32_t);
void OpRdseed(struct Machine *, uint32_t);
void Op171(struct Machine *, uint32_t);
void Op172(struct Machine *, uint32_t);
void Op173(struct Machine *, uint32_t);
void Push(struct Machine *, uint32_t, uint64_t);
uint64_t Pop(struct Machine *, uint32_t, uint16_t);
void OpCallJvds(struct Machine *, uint32_t);
void OpRet(struct Machine *, uint32_t);
void OpRetf(struct Machine *, uint32_t);
void OpLeave(struct Machine *, uint32_t);
void OpCallEq(struct Machine *, uint32_t);
void OpPopEvq(struct Machine *, uint32_t);
void OpPopZvq(struct Machine *, uint32_t);
void OpPushZvq(struct Machine *, uint32_t);
void OpPushEvq(struct Machine *, uint32_t);
void PopVq(struct Machine *, uint32_t);
void PushVq(struct Machine *, uint32_t);
void OpJmpEq(struct Machine *, uint32_t);
void OpPusha(struct Machine *, uint32_t);
void OpPopa(struct Machine *, uint32_t);
void OpCallf(struct Machine *, uint32_t);
void OpXchgGbEb(struct Machine *, uint32_t);
void OpXchgGvqpEvqp(struct Machine *, uint32_t);
void OpCmpxchgEbAlGb(struct Machine *, uint32_t);
void OpCmpxchgEvqpRaxGvqp(struct Machine *, uint32_t);

#endif /* BLINK_MACHINE_H_ */
