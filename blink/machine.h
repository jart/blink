#ifndef BLINK_MACHINE_H_
#define BLINK_MACHINE_H_
#include <limits.h>
#include <pthread.h>
#include <setjmp.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdbool.h>

#include "blink/builtin.h"
#include "blink/dll.h"
#include "blink/elf.h"
#include "blink/fds.h"
#include "blink/jit.h"
#include "blink/linux.h"
#include "blink/tsan.h"
#include "blink/tunables.h"

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

#define CR0_PE 0x01        // protected mode enabled
#define CR0_MP 0x02        // monitor coprocessor
#define CR0_EM 0x04        // no x87 fpu present if set
#define CR0_TS 0x08        // task switched x87
#define CR0_ET 0x10        // extension type 287 or 387
#define CR0_NE 0x20        // enable x87 error reporting
#define CR0_WP 0x00010000  // write protect read-only pages @pl0
#define CR0_AM 0x00040000  // alignment mask
#define CR0_NW 0x20000000  // global write-through cache disable
#define CR0_CD 0x40000000  // global cache disable
#define CR0_PG 0x80000000  // paging enabled

#define PAGE_V    0x0001  // valid
#define PAGE_RW   0x0002  // writeable
#define PAGE_U    0x0004  // permit user-mode access
#define PAGE_4KB  0x0080  // IsPage (if PDPTE/PDE) or PAT (if PT)
#define PAGE_2MB  0x0180
#define PAGE_1GB  0x0180
#define PAGE_RSRV 0x0200  // no actual memory associated
#define PAGE_HOST 0x0400  // PAGE_TA bits point to host (not real) memory
#define PAGE_MAP  0x0800  // PAGE_TA bits were mmmap()'d
#define PAGE_EOF  0x0010000000000000
#define PAGE_MUG  0x0020000000000000  // each 4096 byte page is a system page
#define PAGE_XD   0x8000000000000000
#define PAGE_TA   0x00007ffffffff000

#define P                struct Machine *m, u64 rde, i64 disp, u64 uimm0
#define A                m, rde, disp, uimm0
#define DISPATCH_NOTHING m, 0, 0, 0

#define MACHINE_CONTAINER(e)  DLL_CONTAINER(struct Machine, elem, e)
#define HOSTPAGE_CONTAINER(e) DLL_CONTAINER(struct HostPage, elem, e)

#if defined(NOLINEAR) || defined(__SANITIZE_THREAD__)
#define CanHaveLinearMemory() false
#else
#define CanHaveLinearMemory() CAN_64BIT
#endif

#ifdef HAVE_JIT
#define IsMakingPath(m) m->path.jb
#else
#define IsMakingPath(m) 0
#endif

#define HasLinearMapping(x) (CanHaveLinearMemory() && !(x)->nolinear)

#if CAN_64BIT
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

// Segment descriptor cache
// @see Intel Manual V3A §3.4.3
// Currently only the base address for each segment register is maintained;
// a full implementation will also cache its limit & access rights
struct DescriptorCache {
  u16 sel;   // visible selector value (or paragraph value in real mode)
  u64 base;  // base linear address
};

struct MachineState {
  u64 ip;
  struct DescriptorCache cs;
  struct DescriptorCache ss;
  struct DescriptorCache es;
  struct DescriptorCache ds;
  struct DescriptorCache fs;
  struct DescriptorCache gs;
  u8 weg[16][8];
  u8 xmm[16][16];
  u32 mxcsr;
  struct MachineFpu fpu;
  struct MachineMemstat memstat;
};

struct Elf {
  const char *prog;
  Elf64_Ehdr_ *ehdr;
  long size;
  i64 base;
  u64 aslr;
  char *map;
  long mapsize;
  bool debugonce;
  i64 at_base;
  i64 at_phdr;
  i64 at_phent;
  i64 at_entry;
  i64 at_phnum;
};

struct OpCache {
  u8 stash[16];   // for memory ops that overlap page
  u64 codevirt;   // current rip page in guest memory
  u8 *codehost;   // current rip page in host memory
  u32 stashsize;  // for writes that overlap page
  bool writable;
  _Atomic(bool) invalidated;
  u64 icache[512][kInstructionBytes / 8];
};

struct System {
  u8 mode;
  bool dlab;
  bool isfork;
  bool nolinear;
  u16 gdt_limit;
  u16 idt_limit;
  int pid;
  u8 *real;
  u64 gdt_base;
  u64 idt_base;
  u64 cr0;
  u64 cr2;
  u64 cr3;
  u64 cr4;
  i64 brk;
  i64 automap;
  i64 codestart;
  unsigned long codesize;
  struct MachineMemstat memstat;
  pthread_cond_t machines_cond;
  pthread_mutex_t machines_lock;
  struct Dll *machines GUARDED_BY(machines_lock);
  unsigned next_tid GUARDED_BY(machines_lock);
  intptr_t ender;
  struct Jit jit;
  struct Fds fds;
  struct Elf elf;
  sigset_t exec_sigmask;
  pthread_mutex_t exec_lock;
  pthread_mutex_t sig_lock;
  struct sigaction_linux hands[64] GUARDED_BY(sig_lock);
  u64 blinksigs;  // signals blink itself handles
  pthread_mutex_t mmap_lock;
  void (*onbinbase)(struct Machine *);
  void (*onlongbranch)(struct Machine *);
  int (*exec)(char *, char **, char **);
  void (*redraw)(bool);
};

struct JitPath {
  int skip;
  int elements;
  u64 skew;
  i64 start;
  struct JitBlock *jb;
};

struct MachineTlb {
  i64 page;
  u64 entry;
};

struct Machine {                           //
  u64 ip;                                  // instruction pointer
  u8 oplen;                                // length of operation
  u8 mode;                                 // [dup] XED_MODE_{REAL,LEGACY,LONG}
  bool nolinear;                           // [dup] no linear address resolution
  bool reserving;                          // did it call ReserveAddress?
  u32 flags;                               // x86 eflags register
  i64 stashaddr;                           // page overlap buffer
  _Atomic(bool) invalidated;               // the tlb must be flushed
  _Atomic(bool) killed;                    // used to send a soft SIGKILL
  _Atomicish(u64) signals;                 // signals waiting for delivery
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
    struct DescriptorCache seg[8];         //
    struct {                               //
      struct DescriptorCache es;           // xtra segment (legacy / real)
      struct DescriptorCache cs;           // code segment (legacy / real)
      struct DescriptorCache ss;           // stak segment (legacy / real)
      struct DescriptorCache ds;           // data segment (legacy / real)
      struct DescriptorCache fs;           // thred-local segment register
      struct DescriptorCache gs;           // winple thread-local register
    };                                     //
  };                                       //
  struct MachineFpu fpu;                   // FLOATING-POINT REGISTER FILE
  u32 mxcsr;                               // SIMD status control register
  pthread_t thread;                        // POSIX thread of this machine
  struct FreeList freelist;                // to make system calls simpler
  struct JitPath path;                     // under construction jit route
  i64 bofram[2];                           // helps debug bootloading code
  i64 faultaddr;                           // used for tui error reporting
  _Atomicish(u64) sigmask;                 // signals that've been blocked
  u32 tlbindex;                            //
  int sig;                                 // signal under active delivery
  u64 siguc;                               // hosted address of ucontext_t
  u64 sigfp;                               // virtual address of fpstate_t
  struct System *system;                   //
  bool canhalt;                            //
  bool metal;                              //
  bool interrupted;                        //
  sigjmp_buf onhalt;                       //
  struct sigaltstack_linux sigaltstack;    //
  i64 robust_list;                         //
  i64 ctid;                                //
  int tid;                                 //
  sigset_t spawn_sigmask;                  //
  struct Dll elem;                         //
  struct OpCache opcache[1];               //
};                                         //

extern _Thread_local struct Machine *g_machine;
extern const nexgen32e_f kConvert[3];
extern const nexgen32e_f kSax[3];
extern bool g_wasteland;

struct System *NewSystem(int);
void FreeSystem(struct System *);
_Noreturn void Actor(struct Machine *);
void SetMachineMode(struct Machine *, int);
void ChangeMachineMode(struct Machine *, int);
struct Machine *NewMachine(struct System *, struct Machine *);
bool IsOrphan(struct Machine *);
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
int LoadInstruction2(struct Machine *, u64);
void ExecuteInstruction(struct Machine *);
u64 AllocatePage(struct System *);
u64 AllocatePageTable(struct System *);
int ReserveVirtual(struct System *, i64, i64, u64, int, i64, bool);
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
void UnlockRobustFutexes(struct Machine *);

bool IsValidAddrSize(i64, i64) pureconst;
bool OverlapsPrecious(i64, i64) pureconst;
char **CopyStrList(struct Machine *, i64);
char *CopyStr(struct Machine *, i64);
char *LoadStr(struct Machine *, i64);
void *Schlep(struct Machine *, i64, size_t);
bool IsValidMemory(struct Machine *, i64, i64, int);
int RegisterMemory(struct Machine *, i64, void *, size_t);
u8 *GetPageAddress(struct System *, u64);
u8 *GetHostAddress(struct Machine *, u64, long);
u8 *AccessRam(struct Machine *, i64, size_t, void *[2], u8 *, bool);
u8 *BeginLoadStore(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *BeginStore(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *BeginStoreNp(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *LookupAddress(struct Machine *, i64);
u8 *Load(struct Machine *, i64, size_t, u8 *);
u8 *MallocPage(void);
u8 *RealAddress(struct Machine *, i64);
u8 *ReserveAddress(struct Machine *, i64, size_t, bool);
u8 *ResolveAddress(struct Machine *, i64);
u8 *GetAddress(struct Machine *, i64);
void CommitStash(struct Machine *);
int CopyFromUser(struct Machine *, void *, i64, u64);
int CopyFromUserRead(struct Machine *, void *, i64, u64);
int CopyToUser(struct Machine *, i64, void *, u64);
int CopyToUserWrite(struct Machine *, i64, void *, u64);
void EndStore(struct Machine *, i64, size_t, void *[2], u8 *);
void EndStoreNp(struct Machine *, i64, size_t, void *[2], u8 *);
void ResetRam(struct Machine *);
void SetReadAddr(struct Machine *, i64, u32);
void SetWriteAddr(struct Machine *, i64, u32);
int ProtectVirtual(struct System *, i64, i64, int);
bool IsFullyMapped(struct System *, i64, i64);
bool IsFullyUnmapped(struct System *, i64, i64);
int GetProtection(u64);
u64 SetProtection(int);
int ClassifyOp(u64) pureconst;
void Terminate(P, void (*)(struct Machine *, u64));

void CountOp(long *);
void FastPush(struct Machine *, long);
void FastPop(struct Machine *, long);
void FastCall(struct Machine *, u64);
void FastCallAbs(u64, struct Machine *);
void FastJmp(struct Machine *, u64);
void FastJmpAbs(u64, struct Machine *);
void FastLeave(struct Machine *);
void FastRet(struct Machine *);

typedef void (*putreg64_f)(u64, struct Machine *);
extern const putreg64_f kPutReg64[16];

u64 Pick(u32, u64, u64);
typedef u32 (*cc_f)(struct Machine *);
extern const cc_f kConditionCode[16];

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
void OpAluwi(P);
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
void Op2f5(P);
void Op2f6(P);
void OpShx(P);
void OpRorx(P);

void FreeBig(void *, size_t);
void *AllocateBig(size_t, int, int, int, off_t);

u64 MaskAddress(u32, u64);
i64 GetIp(struct Machine *);
i64 GetPc(struct Machine *);
u64 AddressOb(P);
u64 AddressDi(P);
i64 AddressSi(P);
u64 GetSegmentBase(P, unsigned);
void SetCs(P, u16);
i64 DataSegment(P, u64);
i64 AddSegment(P, u64, u64);

void OpLddquVdqMdq(P);
void OpMaskMovDiXmmRegXmmRm(P);
void OpMov0f10(P);
void OpMov0f12(P);
void OpMov0f13(P);
void OpMov0f16(P);
void OpMov0f17(P);
void OpMov0f28(P);
void OpMov0f29(P);
void OpMov0f2b(P);
void OpMov0f6e(P);
void OpMov0f6f(P);
void OpMov0f7e(P);
void OpMov0f7f(P);
void OpMov0fD6(P);
void OpMov0fE7(P);
void OpMovWpsVps(P);
void OpMovntdqaVdqMdq(P);
void OpMovntiMdqpGdqp(P);
void OpPmovmskbGdqpNqUdq(P);

void OpUnpcklpsd(P);
void OpUnpckhpsd(P);
void OpPextrwGdqpUdqIb(P);
void OpPinsrwVdqEwIb(P);
void OpShuffle(P);
void OpShufpsd(P);
void OpSqrtpsd(P);
void OpRsqrtps(P);
void OpRcpps(P);
void OpComissVsWs(P);
void OpAddpsd(P);
void OpMulpsd(P);
void OpSubpsd(P);
void OpDivpsd(P);
void OpMinpsd(P);
void OpMaxpsd(P);
void OpCmppsd(P);
void OpAndpsd(P);
void OpAndnpsd(P);
void OpOrpsd(P);
void OpXorpsd(P);
void OpHaddpsd(P);
void OpHsubpsd(P);
void OpAddsubpsd(P);
void OpMovmskpsd(P);

extern void (*AddPath_StartOp_Hook)(P);

bool AddPath(P);
void FlushSkew(P);
bool CreatePath(P);
void Connect(P, u64);
void CompletePath(P);
void AddPath_EndOp(P);
bool FuseBranchTest(P);
void AddPath_StartOp(P);
long GetPrologueSize(void);
bool FuseBranchCmp(P, bool);
i64 GetIp(struct Machine *);
void FinishPath(struct Machine *);
void FuseOp(struct Machine *, i64);
void AbandonPath(struct Machine *);
void AddIp(struct Machine *, long);
void BeginCod(struct Machine *, i64);
void AdvanceIp(struct Machine *, long);
void SkewIp(struct Machine *, long, long);

void Op2f01(P);
void OpTest(P);
void OpAlui(P);
void LoadAluArgs(P);
void LoadAluFlipArgs(P);
i64 FastAnd8(struct Machine *, u64, u64);
i64 FastSub8(struct Machine *, u64, u64);
void ZeroRegFlags(struct Machine *, long);

i32 Imul32(i32, i32, struct Machine *);
i64 Imul64(i64, i64, struct Machine *);
void Mulx64(u64, struct Machine *, long, long);
u32 JustMul32(u32, u32, struct Machine *);
u64 JustMul64(u64, u64, struct Machine *);
void MulAxDx(u64, struct Machine *);
void JustMulAxDx(u64, struct Machine *);

void OpPsdMuld1(u8 *, struct Machine *, long);
void OpPsdAddd1(u8 *, struct Machine *, long);
void OpPsdSubd1(u8 *, struct Machine *, long);
void OpPsdDivd1(u8 *, struct Machine *, long);
void OpPsdMind1(u8 *, struct Machine *, long);
void OpPsdMaxd1(u8 *, struct Machine *, long);

void Int64ToDouble(i64, struct Machine *, long);
void Int32ToDouble(i32, struct Machine *, long);
void MovsdWpsVpsOp(u8 *, struct Machine *, long);

void LogCpu(struct Machine *);
const char *GetBacktrace(struct Machine *);
const char *DescribeOp(struct Machine *, i64);
int GetInstruction(struct Machine *, i64, struct XedDecodedInst *);

void SetupCod(struct Machine *);
void FlushCod(struct JitBlock *);
#if LOG_COD
void WriteCod(const char *, ...) printf_attr(1);
void LogCodOp(struct Machine *, const char *);
#else
#define LogCodOp(m, s) (void)0
#define WriteCod(...)  (void)0
#endif

#endif /* BLINK_MACHINE_H_ */
