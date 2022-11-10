#ifndef BLINK_MACHINE_H_
#define BLINK_MACHINE_H_
#include <setjmp.h>
#include <stdbool.h>

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
  int freed;
  int resizes;
  int reserved;
  int committed;
  int allocated;
  int reclaimed;
  int pagetables;
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
  struct MachineReal {
    size_t i, n;
    uint8_t *p;
  } real;
  uint64_t cr3;
  uint8_t xmm[16][16];
  uint8_t es[8];
  uint8_t ds[8];
  uint8_t fs[8];
  uint8_t gs[8];
  struct MachineFpu fpu;
  uint64_t cr0;
  uint64_t cr2;
  uint64_t cr4;
  uint64_t gdt_base;
  uint64_t idt_base;
  uint16_t gdt_limit;
  uint16_t idt_limit;
  uint32_t mxcsr;
  struct MachineRealFree {
    uint64_t i;
    uint64_t n;
    struct MachineRealFree *next;
  } * realfree;
  struct FreeList {
    size_t n;
    void **p;
  } freelist;
  struct MachineMemstat memstat;
  int64_t brk;
  int64_t bofram[2];
  jmp_buf onhalt;
  int64_t faultaddr;
  bool dlab;
  bool isfork;
  struct MachineFds fds;
  void (*onbinbase)(struct Machine *);
  void (*onlongbranch)(struct Machine *);
  void (*redraw)(void);
  int (*exec)(char *, char **, char **);
  struct sigaction_linux hands[32];
  uint8_t sigmask[8];
  int sig;
  uint64_t siguc;
  uint64_t sigfp;
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

#endif /* BLINK_MACHINE_H_ */
