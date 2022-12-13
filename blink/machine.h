#ifndef BLINK_MACHINE_H_
#define BLINK_MACHINE_H_
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "blink/assert.h"
#include "blink/builtin.h"
#include "blink/dll.h"
#include "blink/elf.h"
#include "blink/end.h"
#include "blink/fds.h"
#include "blink/jit.h"
#include "blink/linux.h"
#include "blink/tsan.h"
#include "blink/x86.h"

#define kArgRde   1
#define kArgDisp  2
#define kArgUimm0 3
#define kArgArg   4

#define kOpNormal    0
#define kOpBranching 1
#define kOpPrecious  2

#define kMaxThreadIds 32768
#define kMinThreadId  262144

#define kInstructionBytes 40

#define kMachineExit                 256
#define kMachineHalt                 -1
#define kMachineDecodeError          -2
#define kMachineUndefinedInstruction -3
#define kMachineSegmentationFault    -4
#define kMachineEscape               -5
#define kMachineDivideError          -6
#define kMachineFpuException         -7
#define kMachineProtectionFault      -8
#define kMachineSimdException        -9

#if LONG_BIT == 64
#ifndef __SANITIZE_ADDRESS__
#define kSkew 0x088800000000
#else
#define kSkew 0x000000000000
#endif
#define kAutomapStart  0x200000000000
#define kPreciousStart 0x444000000000  // 1 tb
#define kPreciousEnd   0x454000000000
#define kStackTop      0x500000000000
#else
#define kAutomapStart  0x20000000
#define kSkew          0x00000000
#define kStackTop      0xf8000000
#define kPreciousStart 0x44000000  // 192 mb
#define kPreciousEnd   0x50000000
#endif
#define kRealSize  (16 * 1024 * 1024)
#define kStackSize (8 * 1024 * 1024)
#define kMinBrk    (2 * 1024 * 1024)

#define PAGE_V    0x0001  // valid
#define PAGE_RW   0x0002  // writeable
#define PAGE_U    0x0004  // permit user-mode access
#define PAGE_4KB  0x0080  // IsPage (if PDPTE/PDE) or PAT (if PT)
#define PAGE_2MB  0x0180
#define PAGE_1GB  0x0180
#define PAGE_RSRV 0x0200  // no actual memory associated
#define PAGE_HOST 0x0400  // PAGE_TA bits point to host memory
#define PAGE_MAP  0x0800  // PAGE_TA bits were mmmap()'d
#define PAGE_EOF  0x0010000000000000
#define PAGE_XD   0x8000000000000000
#define PAGE_TA   0x00007ffffffff000

#define P                struct Machine *m, u64 rde, i64 disp, u64 uimm0
#define A                m, rde, disp, uimm0
#define DISPATCH_NOTHING m, 0, 0, 0

#define FUTEX_CONTAINER(e)    DLL_CONTAINER(struct Futex, elem, e)
#define MACHINE_CONTAINER(e)  DLL_CONTAINER(struct Machine, elem, e)
#define HOSTPAGE_CONTAINER(e) DLL_CONTAINER(struct HostPage, elem, e)

#define HasHook(m, pc) (((u64)(pc)) - m->codestart < m->codesize)
#define GetHook(m, pc)       \
  ((nexgen32e_f)(IMAGE_END + \
                 atomic_load_explicit(m->fun + (pc), memory_order_relaxed)))
#define SetHook(m, pc, func)                                   \
  atomic_store_explicit(m->fun + (pc), (u8 *)(func)-IMAGE_END, \
                        memory_order_relaxed)

#if defined(NOLINEAR) || defined(__SANITIZE_THREAD__)
#define CanHaveLinearMemory() false
#else
#define CanHaveLinearMemory() (LONG_BIT == 64)
#endif

#define HasLinearMapping(x) \
  (CanHaveLinearMemory() && \
   !atomic_load_explicit(&(x)->nolinear, memory_order_relaxed))

#if LONG_BIT == 64
#define _Atomicish(t) _Atomic(t)
#else
#define _Atomicish(t) t
#endif

#if !defined(__m68k__) && !defined(__mips__)
typedef atomic_uint memstat_t;
#else
typedef unsigned memstat_t;
#endif

MICRO_OP_SAFE u8 *ToHost(i64 v) {
  return (u8 *)(intptr_t)(v + kSkew);
}

static inline i64 ToGuest(void *r) {
  i64 v = (intptr_t)r - kSkew;
  unassert(!(v & ~PAGE_TA));
  return v;
}

struct Machine;
typedef void (*nexgen32e_f)(P);

struct FreeList {
  int n;
  void **p;
};

struct HostPage {
  u8 *page;
  struct HostPage *next;
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
  memstat_t freed;
  memstat_t reserved;
  memstat_t committed;
  memstat_t allocated;
  memstat_t reclaimed;
  memstat_t pagetables;
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
  _Atomic(bool) invalidated;
  u64 icache[1024][kInstructionBytes / 8];
};

struct Futex {
  i64 addr;
  int waiters;
  struct Dll elem;
  pthread_cond_t cond;
  pthread_mutex_t lock;
};

struct System {
  u8 mode;
  bool dlab;
  bool isfork;
  bool nolinear;
  u16 gdt_limit;
  u16 idt_limit;
  int pid;
  u64 gdt_base;
  u64 idt_base;
  u64 cr0;
  u64 cr2;
  u64 cr3;
  u64 cr4;
  i64 brk;
  i64 automap;
  i64 codestart;
  const char *brand;
  _Atomic(int) *fun;
  unsigned long codesize;
  pthread_mutex_t lock_lock;
  struct MachineMemstat memstat;
  pthread_mutex_t machines_lock;
  struct Dll *machines GUARDED_BY(machines_lock);
  unsigned next_tid GUARDED_BY(machines_lock);
  struct Jit jit;
  struct Fds fds;
  struct Elf elf;
  struct Dll *futexes;
  pthread_mutex_t futex_lock;
  pthread_mutex_t sig_lock;
  struct sigaction_linux hands[64] GUARDED_BY(sig_lock);
  u64 blinksigs;  // signals blink itself handles
  pthread_mutex_t mmap_lock;
  void (*onbinbase)(struct Machine *);
  void (*onlongbranch)(struct Machine *);
  int (*exec)(char *, char **, char **);
  void (*redraw)(void);
  _Alignas(4096) u8 real[kRealSize];
};

struct JitPath {
  i64 start;
  int elements;
  struct JitBlock *jb;
};

struct MachineTlb {
  i64 page;
  u64 entry;
};

struct Machine {                           //
  u64 ip;                                  // instruction pointer
  u64 oldip;                               // ip saved at start of op, or -1
  i64 stashaddr;                           // page overlap buffer
  u32 flags;                               // x86 eflags register
  bool reserving;                          // did it call ReserveAddress?
  _Atomic(bool) invalidated;               // the tlb must be flushed
  _Atomic(bool) nolinear;                  // [dup] no linear address resolution
  _Atomic(bool) killed;                    // used to send a soft SIGKILL
  u8 mode;                                 // [dup] XED_MODE_{REAL,LEGACY,LONG}
  _Atomic(int) *fun;                       // [dup] jit hooks for code bytes
  unsigned long codesize;                  // [dup] size of exe code section
  i64 codestart;                           // [dup] virt of exe code section
  union {                                  // GENERAL REGISTER FILE
    u64 align8_;                           //
    u8 beg[128];                           //
    u8 weg[16][8];                         //
    struct {                               //
      union {                              //
        u8 ax[8];                          // [vol] accumulator, result:1/2
        struct {                           //
          u8 al;                           // lo byte of ax
          u8 ah;                           // hi byte of ax
        };                                 //
      };                                   //
      union {                              //
        u8 cx[8];                          // [vol] param:4/6
        struct {                           //
          u8 cl;                           // lo byte of cx
          u8 ch;                           // hi byte of cx
        };                                 //
      };                                   //
      union {                              //
        u8 dx[8];                          // [vol] param:3/6, result:2/2
        struct {                           //
          u8 dl;                           // lo byte of dx
          u8 dh;                           // hi byte of dx
        };                                 //
      };                                   //
      union {                              //
        u8 bx[8];                          // [sav] base index
        struct {                           //
          u8 bl;                           // lo byte of bx
          u8 bh;                           // hi byte of bx
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
  union {                                  //
    u64 seg[8];                            //
    struct {                               //
      u64 es;                              // xtra segment (legacy / real)
      u64 cs;                              // code segment (legacy / real)
      u64 ss;                              // stak segment (legacy / real)
      u64 ds;                              // data segment (legacy / real)
      u64 fs;                              // thred-local segment register
      u64 gs;                              // winple thread-local register
    };                                     //
  };                                       //
  struct MachineFpu fpu;                   // FLOATING-POINT REGISTER FILE
  u32 mxcsr;                               // SIMD status control register
  pthread_t thread;                        // POSIX thread of this machine
  struct FreeList freelist;                // to make system calls simpler
  struct JitPath path;                     // under construction jit route
  i64 bofram[2];                           // helps debug bootloading code
  i64 faultaddr;                           // used for tui error reporting
  _Atomicish(u64) signals;                 // signals waiting for delivery
  _Atomicish(u64) sigmask;                 // signals that've been blocked
  u32 tlbindex;                            //
  int sig;                                 // signal under active delivery
  u64 siguc;                               // hosted address of ucontext_t
  u64 sigfp;                               // virtual address of fpstate_t
  struct System *system;                   //
  bool canhalt;                            //
  bool metal;                              //
  jmp_buf onhalt;                          //
  i64 ctid;                                //
  int tid;                                 //
  sigset_t spawn_sigmask;                  //
  struct Dll elem GUARDED_BY(system->machines_lock);
  struct OpCache opcache[1];
};

extern _Thread_local struct Machine *g_machine;

struct System *NewSystem(void);
void FreeSystem(struct System *);
_Noreturn void Actor(struct Machine *);
void SetMachineMode(struct Machine *, int);
void ChangeMachineMode(struct Machine *, int);
struct Machine *NewMachine(struct System *, struct Machine *);
void Jitter(P, const char *, ...);
void FreeMachine(struct Machine *);
void InvalidateSystem(struct System *, bool, bool);
void RemoveOtherThreads(struct System *);
void KillOtherThreads(struct System *);
void ResetCpu(struct Machine *);
void ResetTlb(struct Machine *);
void CollectGarbage(struct Machine *);
void ResetInstructionCache(struct Machine *);
void GeneralDispatch(P);
nexgen32e_f GetOp(long);
void LoadInstruction(struct Machine *, u64);
void ExecuteInstruction(struct Machine *);
u64 AllocatePage(struct System *);
u64 AllocatePageTable(struct System *);
int ReserveVirtual(struct System *, i64, i64, u64, int, bool);
char *FormatPml4t(struct Machine *);
i64 FindVirtual(struct System *, i64, i64);
int FreeVirtual(struct System *, i64, i64);
void LoadArgv(struct Machine *, char *, char **, char **);
_Noreturn void HaltMachine(struct Machine *, int);
_Noreturn void RaiseDivideError(struct Machine *);
_Noreturn void ThrowSegmentationFault(struct Machine *, i64);
_Noreturn void ThrowProtectionFault(struct Machine *);
_Noreturn void OpUdImpl(struct Machine *);
_Noreturn void OpUd(P);
_Noreturn void OpHlt(P);
void JitlessDispatch(P);
void RestoreIp(struct Machine *);

bool IsValidAddrSize(i64, i64) pureconst;
bool OverlapsPrecious(i64, i64) pureconst;
char **LoadStrList(struct Machine *, i64);
char *LoadStr(struct Machine *, i64);
int RegisterMemory(struct Machine *, i64, void *, size_t);
u8 *GetPageAddress(struct System *, u64);
u8 *GetHostAddress(struct Machine *, u64, long);
u8 *AccessRam(struct Machine *, i64, size_t, void *[2], u8 *, bool);
u8 *BeginLoadStore(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *BeginStore(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *BeginStoreNp(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *CopyFromUser(struct Machine *, void *, i64, u64);
u8 *LookupAddress(struct Machine *, i64);
u8 *Load(struct Machine *, i64, size_t, u8 *);
u8 *MallocPage(void);
u8 *RealAddress(struct Machine *, i64);
u8 *ReserveAddress(struct Machine *, i64, size_t, bool);
u8 *ResolveAddress(struct Machine *, i64);
u8 *GetAddress(struct Machine *, i64);
void CommitStash(struct Machine *);
void CopyFromUserRead(struct Machine *, void *, i64, u64);
void CopyToUser(struct Machine *, i64, void *, u64);
void CopyToUserWrite(struct Machine *, i64, void *, u64);
void EndStore(struct Machine *, i64, size_t, void *[2], u8 *);
void EndStoreNp(struct Machine *, i64, size_t, void *[2], u8 *);
void ResetRam(struct Machine *);
void SetReadAddr(struct Machine *, i64, u32);
void SetWriteAddr(struct Machine *, i64, u32);
void ProtectVirtual(struct System *, i64, i64, u64, u64);
int ClassifyOp(u64) pureconst;

void CountOp(long *);
void FastJmp(struct Machine *, i32);
void FastPush(struct Machine *, long);
void FastPop(struct Machine *, long);
void FastCall(struct Machine *, u64);
void FastRet(struct Machine *);

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
void OpAlub(P);
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
void OpRetIw(P);
void OpSsePclmulqdq(P);
void OpXaddEbGb(P);
void OpXaddEvqpGvqp(P);
void OpXchgGbEb(P);
void OpXchgGvqpEvqp(P);

#endif /* BLINK_MACHINE_H_ */
