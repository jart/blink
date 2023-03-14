#ifndef BLINK_MACHINE_H_
#define BLINK_MACHINE_H_
#include <limits.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>

#include "blink/atomic.h"
#include "blink/builtin.h"
#include "blink/dll.h"
#include "blink/elf.h"
#include "blink/fds.h"
#include "blink/jit.h"
#include "blink/linux.h"
#include "blink/log.h"
#include "blink/thread.h"
#include "blink/tsan.h"
#include "blink/tunables.h"

#define kArgRde   1
#define kArgDisp  2
#define kArgUimm0 3
#define kArgArg   4

#define kOpNormal      0
#define kOpBranching   1
#define kOpPrecious    2
#define kOpSerializing kOpPrecious

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
#define kMachineExitTrap             -10
#define kMachineFatalSystemSignal    -11

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

/* Long Mode Paging
   @see Intel Manual V.3A §4.1 §4.5
                                       IsValid (ignored on CR3) V┐
  ┌XD:No Inst. Fetches (if NXE)   IsWritable (ignored on CR3) RW┐│
  │                                 Permit User-Mode Access - u┐││
  │                             Page-level Write-Through - PWT┐│││
  │                            Page-level Cache Disable - PCD┐││││
  │                          Set if has been read - Accessed┐│││││
  │                         Set if has been written - Dirty┐││││││
  │           IsPage 2MB/1GB (if PDPTE/PDE) or PAT (if PT)┐│││││││
  │                (If this maps page and CR4.PGE) Global┐││││││││
  │      (If IsPage 2MB/1GB, see Intel V3A § 11.12) PAT  │││││││││
  │                                                  │   │││││││││
  │            ┌─────────────────────────────────────┤   │││││││││
  │  Must Be 0┐│ Next Page Table Address (!IsPage)   │   │││││││││
  │           │├─────────────────────────────────────┤   │││││││││
  │           ││ Physical Address 4KB                │   │││││││││
  │┌──┐┌─────┐│├────────────────────────────┐        │ign│││││││││
  ││PK││ ign │││ Physical Address 2MB       │        │┌┴┐│││││││││
  ││  ││     ││├───────────────────┐        │        ││ ││││││││││
  ││  ││     │││ Phys. Addr. 1GB   │        │        ││ ││││││││││
  ││  ││     │││                   │        │        ││ ││││││││││
  ───────────────────────────────────────────────────────────────*/
#define PAGE_V     0x0000000000000001  // valid
#define PAGE_RW    0x0000000000000002  // writeable
#define PAGE_U     0x0000000000000004  // permit ring3 access or read protect
#define PAGE_PS    0x0000000000000080  // IsPage (PDPTE/PDE) or PAT (PT)
#define PAGE_G     0x0000000000000100  // global
#define PAGE_RSRV  0x0000000000000200  // PAGE_TA bits havent been chosen yet
#define PAGE_HOST  0x0000000000000400  // PAGE_TA bits point to system memory
#define PAGE_MAP   0x0000000000000800  // PAGE_TA bits are a linear host mmap
#define PAGE_TA    0x0000fffffffff000  // bits used for host, or real address
#define PAGE_GROW  0x0010000000000000  // for future support of MAP_GROWSDOWN
#define PAGE_MUG   0x0020000000000000  // host page magic mapped individually
#define PAGE_FILE  0x0040000000000000  // page has tracking bit in s->filemap
#define PAGE_LOCK  0x0080000000000000  // a bit used to increment lock counts
#define PAGE_LOCKS 0x7f80000000000000  // a page can be locked by 255 threads
#define PAGE_XD    0x8000000000000000  // disable executing memory if bit set

#define SREG_ES 0
#define SREG_CS 1
#define SREG_SS 2
#define SREG_DS 3
#define SREG_FS 4
#define SREG_GS 5

#define P                struct Machine *m, u64 rde, i64 disp, u64 uimm0
#define A                m, rde, disp, uimm0
#define DISPATCH_NOTHING m, 0, 0, 0

#define MACHINE_CONTAINER(e)  DLL_CONTAINER(struct Machine, elem, e)
#define FILEMAP_CONTAINER(e)  DLL_CONTAINER(struct FileMap, elem, e)
#define HOSTPAGE_CONTAINER(e) DLL_CONTAINER(struct HostPage, elem, e)

#if defined(NOLINEAR) || defined(__SANITIZE_THREAD__) || \
    defined(__CYGWIN__) || defined(__NetBSD__) || defined(__COSMOPOLITAN__)
#define CanHaveLinearMemory() false
#else
#define CanHaveLinearMemory() CAN_64BIT
#endif

#ifdef HAVE_JIT
#define IsMakingPath(m) m->path.jb
#else
#define IsMakingPath(m) 0
#endif

#define HasLinearMapping() (CanHaveLinearMemory() && !FLAG_nolinear)

#if CAN_64BIT
#define _Atomicish(t) _Atomic(t)
#else
#define _Atomicish(t) t
#endif

MICRO_OP_SAFE u8 *ToHost(i64 v) {
  return (u8 *)(uintptr_t)(v + kSkew);
}

static inline i64 ToGuest(void *r) {
  i64 v = (uintptr_t)r - kSkew;
  return v;
}

struct Dis;
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

struct PageLock {
  i64 page;
  u8 *pslot;
  int sysdepth;
};

struct SmcQueue {
  i64 p[kSmcQueueSize];
};

struct PageLocks {
  int i, n;
  struct PageLock *p;
};

struct FileMap {
  i64 virt;         // start address of map
  i64 size;         // bytes originally mapped
  u64 pages;        // population count of present
  i64 offset;       // file offset (-1 if descriptive)
  char *path;       // duplicated (owned) pointer to filename
  u64 *present;     // bitset of present pages in [virt,virt+size)
  struct Dll elem;  // see System::filemaps
};

struct MachineFpu {
#ifndef DISABLE_X87
  double st[8];
  u32 sw;
  int tw;
  int op;
  i64 ip;
#endif
  u32 cw;
  i64 dp;
};

struct MachineMemstat {
  long tables;
  long reserved;
  long committed;
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
  char *prog;
  char *execfn;
  char *interpreter;
  i64 base;
  i64 aslr;
  u8 rng[16];
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
  bool exited;
  bool loaded;
  bool iscosmo;
  bool trapexit;
  bool brkchanged;
  _Atomic(bool) killer;
  u16 gdt_limit;
  u16 idt_limit;
  int exitcode;
  u32 efer;
  int pid;
  unsigned next_tid;
  u8 *real;
  u64 gdt_base;
  u64 idt_base;
  u64 cr0;
  u64 cr2;
  u64 cr3;
  u64 cr4;
  i64 brk;
  i64 automap;
  i64 memchurn;
  i64 codestart;
  long codesize;
  _Atomic(long) rss;
  _Atomic(long) vss;
  struct Dis *dis;
  struct Dll *filemaps;
  struct MachineMemstat memstat;
  struct Dll *machines;
  uintptr_t ender;
  struct Jit jit;
  struct Fds fds;
  struct Elf elf;
  sigset_t exec_sigmask;
  struct sigaction_linux hands[64];
  u64 blinksigs;  // signals blink itself handles
  struct rlimit_linux rlim[RLIM_NLIMITS_LINUX];
#ifdef HAVE_THREADS
  pthread_cond_t machines_cond;
  pthread_mutex_t machines_lock;
  pthread_cond_t pagelocks_cond;
  pthread_mutex_t pagelocks_lock;
  pthread_mutex_t exec_lock;
  pthread_mutex_t sig_lock;
  pthread_mutex_t mmap_lock;
#endif
  void (*onfilemap)(struct System *, struct FileMap *);
  void (*onsymbols)(struct System *);
  void (*onbinbase)(struct Machine *);
  void (*onlongbranch)(struct Machine *);
  int (*exec)(char *, char *, char **, char **);
  void (*redraw)(bool);
};

// Default segment selector values in non-metal mode, per Linux 5.9
#define USER_DS_LINUX 0x2b  // default selector for ss (N.B.)
#define USER_CS_LINUX 0x33  // default selector for cs

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

struct Machine {                         //
  u64 ip;                                // instruction pointer
  u8 oplen;                              // length of operation
  u8 mode;                               // [dup] XED_MODE_{REAL,LEGACY,LONG}
  bool threaded;                         // must use synchronization
  _Atomic(bool) attention;               // signals main interpreter loop
  u32 flags;                             // x86 eflags register
  i64 stashaddr;                         // it's our page overlap buffer
  union {                                // GENERAL REGISTER FILE
    u64 align8_;                         //
    u8 beg[128];                         //
    u8 weg[16][8];                       //
    struct {                             //
      union {                            //
        u8 ax[8];                        // [vol] accumulator, result:1/2
        struct {                         //
          u8 al;                         // lo byte of ax
          u8 ah;                         // hi byte of ax
        };                               //
      };                                 //
      union {                            //
        u8 cx[8];                        // [vol] param:4/6
        struct {                         //
          u8 cl;                         // lo byte of cx
          u8 ch;                         // hi byte of cx
        };                               //
      };                                 //
      union {                            //
        u8 dx[8];                        // [vol] param:3/6, result:2/2
        struct {                         //
          u8 dl;                         // lo byte of dx
          u8 dh;                         // hi byte of dx
        };                               //
      };                                 //
      union {                            //
        u8 bx[8];                        // [sav] base index
        struct {                         //
          u8 bl;                         // lo byte of bx
          u8 bh;                         // hi byte of bx
        };                               //
      };                                 //
      u8 sp[8];                          // [sav] stack pointer
      u8 bp[8];                          // [sav] backtrace pointer
      u8 si[8];                          // [vol] param:2/6
      u8 di[8];                          // [vol] param:1/6
      u8 r8[8];                          // [vol] param:5/6
      u8 r9[8];                          // [vol] param:6/6
      u8 r10[8];                         // [vol]
      u8 r11[8];                         // [vol]
      u8 r12[8];                         // [sav]
      u8 r13[8];                         // [sav]
      u8 r14[8];                         // [sav]
      u8 r15[8];                         // [sav]
    };                                   //
  };                                     //
  _Alignas(16) u8 xmm[16][16];           // 128-BIT VECTOR REGISTER FILE
  struct XedDecodedInst *xedd;           // ->opcache->icache if non-jit
  i64 readaddr;                          // so tui can show memory reads
  i64 writeaddr;                         // so tui can show memory write
  i64 readsize;                          // bytes length of last read op
  i64 writesize;                         // byte length of last write op
  union {                                //
    struct DescriptorCache seg[8];       //
    struct {                             //
      struct DescriptorCache es;         // xtra segment (legacy / real)
      struct DescriptorCache cs;         // code segment (legacy / real)
      struct DescriptorCache ss;         // stak segment (legacy / real)
      struct DescriptorCache ds;         // data segment (legacy / real)
      struct DescriptorCache fs;         // thred-local segment register
      struct DescriptorCache gs;         // winple thread-local register
    };                                   //
  };                                     //
  struct MachineFpu fpu;                 // FLOATING-POINT REGISTER FILE
  u32 mxcsr;                             // SIMD status control register
  pthread_t thread;                      // POSIX thread of this machine
  struct FreeList freelist;              // to make system calls simpler
  struct PageLocks pagelocks;            // track page table entry locks
  struct JitPath path;                   // under construction jit route
  _Atomicish(u64) signals;               // [attention] pending delivery
  _Atomicish(u64) sigmask;               // signals that've been blocked
  i64 bofram[2];                         // helps debug bootloading code
  i64 faultaddr;                         // used for tui error reporting
  struct System *system;                 //
  int sigdepth;                          //
  int sysdepth;                          //
  _Atomic(bool) killed;                  // [attention] slay this thread
  _Atomic(bool) invalidated;             // the tlb must be flushed
  bool restored;                         // [attention] rt_sigreturn()'d
  bool selfmodifying;                    // [attention] need usmc restore
  bool reserving;                        //
  bool insyscall;                        //
  bool nofault;                          //
  bool canhalt;                          //
  bool metal;                            //
  bool interrupted;                      //
  bool issigsuspend;                     //
  bool traprdtsc;                        //
  bool trapcpuid;                        //
  bool boop;                             //
  i8 trapno;                             //
  i8 segvcode;                           //
  struct MachineTlb tlb[32];             //
  sigjmp_buf onhalt;                     //
  struct sigaltstack_linux sigaltstack;  //
  i64 robust_list;                       //
  i64 ctid;                              //
  int tid;                               //
  sigset_t spawn_sigmask;                //
  struct Dll elem;                       //
  struct SmcQueue smcqueue;              //
  struct OpCache opcache[1];             //
};                                       //

extern _Thread_local siginfo_t g_siginfo;
extern _Thread_local struct Machine *g_machine;
extern const nexgen32e_f kConvert[3];
extern const nexgen32e_f kSax[3];

struct System *NewSystem(int);
void FreeSystem(struct System *);
void SignalActor(struct Machine *);
void SetMachineMode(struct Machine *, int);
struct Machine *NewMachine(struct System *, struct Machine *);
i64 AreAllPagesUnlocked(struct System *) nosideeffect;
bool IsOrphan(struct Machine *) nosideeffect;
_Noreturn void Blink(struct Machine *);
_Noreturn void Actor(struct Machine *);
void Jitter(P, const char *, ...);
void FreeMachine(struct Machine *);
void InvalidateSystem(struct System *, bool, bool);
void RemoveOtherThreads(struct System *);
void KillOtherThreads(struct System *);
void ResetCpu(struct Machine *);
void ResetTlb(struct Machine *);
void CollectGarbage(struct Machine *, size_t);
void ResetInstructionCache(struct Machine *);
void GeneralDispatch(P);
nexgen32e_f GetOp(long);
void LoadInstruction(struct Machine *, u64);
int LoadInstruction2(struct Machine *, u64);
void ExecuteInstruction(struct Machine *);
u64 AllocatePageTable(struct System *);
u64 AllocateAnonymousPage(struct System *);
void FreeAnonymousPage(struct System *, u8 *);
u64 FindPageTableEntry(struct Machine *, u64);
bool CheckMemoryInvariants(struct System *) nosideeffect dontdiscard;
i64 ReserveVirtual(struct System *, i64, i64, u64, int, i64, bool, bool);
char *FormatPml4t(struct Machine *);
i64 FindVirtual(struct System *, i64, i64);
int FreeVirtual(struct System *, i64, i64);
void CleanseMemory(struct System *, size_t);
void LoadArgv(struct Machine *, char *, char *, char **, char **, u8[16]);
_Noreturn void HaltMachine(struct Machine *, int);
_Noreturn void RaiseDivideError(struct Machine *);
_Noreturn void ThrowSegmentationFault(struct Machine *, i64);
_Noreturn void ThrowProtectionFault(struct Machine *);
_Noreturn void OpUdImpl(struct Machine *);
_Noreturn void OpUd(P);
_Noreturn void OpHlt(P);
void JitlessDispatch(P);
void RestoreIp(struct Machine *);
void CheckForSignals(struct Machine *);
void UnlockRobustFutexes(struct Machine *);
void *AddToFreeList(struct Machine *, void *);

bool IsValidAddrSize(i64, i64) pureconst;
char **CopyStrList(struct Machine *, i64);
char *CopyStr(struct Machine *, i64);
char *LoadStr(struct Machine *, i64);
void *Schlep(struct Machine *, i64, size_t, u64, u64);
void *SchlepR(struct Machine *, i64, size_t);
void *SchlepW(struct Machine *, i64, size_t);
void *SchlepRW(struct Machine *, i64, size_t);
bool IsValidMemory(struct Machine *, i64, i64, int);
int RegisterMemory(struct Machine *, i64, void *, size_t);
i64 ConvertHostToGuestAddress(struct System *, void *, u64 *);
u8 *GetPageAddress(struct System *, u64, bool);
u8 *GetHostAddress(struct Machine *, u64, long);
u8 *AccessRam(struct Machine *, i64, size_t, void *[2], u8 *, bool);
u8 *BeginLoadStore(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *BeginStore(struct Machine *, i64, size_t, void *[2], u8 *);
u8 *BeginStoreNp(struct Machine *, i64, size_t, void *[2], u8 *);
int GetFileDescriptorLimit(struct System *);
bool HasPageLock(const struct Machine *, i64) nosideeffect;
void CollectPageLocks(struct Machine *);
u8 *LookupAddress(struct Machine *, i64);
u8 *LookupAddress2(struct Machine *, i64, u64, u64);
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
int GetDescriptor(struct Machine *, int, u64 *);
void ResetRam(struct Machine *);
void SetReadAddr(struct Machine *, i64, u32);
void SetWriteAddr(struct Machine *, i64, u32);
int SyncVirtual(struct System *, i64, i64, int);
int ProtectVirtual(struct System *, i64, i64, int, bool);
bool IsFullyMapped(struct System *, i64, i64);
bool IsFullyUnmapped(struct System *, i64, i64);
int GetProtection(u64);
u64 SetProtection(int);
int ClassifyOp(u64) pureconst;
void Terminate(P, void (*)(struct Machine *, u64));
long GetMaxRss(struct System *);
long GetMaxVss(struct System *);

void FlushSmcQueue(struct Machine *);
bool IsPageInSmcQueue(struct Machine *, i64);
void AddPageToSmcQueue(struct Machine *, i64);
i64 ProtectRwxMemory(struct System *, i64, i64, i64, long, int);
void HandleFatalSystemSignal(struct Machine *, const siginfo_t *);
bool IsSelfModifyingCodeSegfault(struct Machine *, const siginfo_t *);

int FixXnuSignal(struct Machine *, int, siginfo_t *);
int FixPpcSignal(struct Machine *, int, siginfo_t *);

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

void OpBit(P);
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
void OpDaa(P);
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

void OpIncZv(P);
void OpDecZv(P);
void OpLes(P);
void OpLds(P);
void OpJmpf(P);
void OpRdmsr(P);
void OpWrmsr(P);
void OpMovRqCq(P);
void OpMovCqRq(P);
void OpInAlImm(P);
void OpInAxImm(P);
void OpInAlDx(P);
void OpInAxDx(P);
void OpOutImmAl(P);
void OpOutImmAx(P);
void OpOutDxAl(P);
void OpOutDxAx(P);
void OpMovSwEvqp(P);
void OpMovEvqpSw(P);
void OpPushSeg(P);
void OpPopSeg(P);

extern void (*AddPath_StartOp_Hook)(P);

bool AddPath(P);
void FlushSkew(P);
bool CreatePath(P);
void CompletePath(P);
void AddPath_EndOp(P);
bool FuseBranchTest(P);
void AddPath_StartOp(P);
void Connect(P, u64, bool);
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

void OpPsdMuls1(u8 *, struct Machine *, long);
void OpPsdAdds1(u8 *, struct Machine *, long);
void OpPsdSubs1(u8 *, struct Machine *, long);
void OpPsdDivs1(u8 *, struct Machine *, long);
void OpPsdMins1(u8 *, struct Machine *, long);
void OpPsdMaxs1(u8 *, struct Machine *, long);

void OpPsdMuld1(u8 *, struct Machine *, long);
void OpPsdAddd1(u8 *, struct Machine *, long);
void OpPsdSubd1(u8 *, struct Machine *, long);
void OpPsdDivd1(u8 *, struct Machine *, long);
void OpPsdMind1(u8 *, struct Machine *, long);
void OpPsdMaxd1(u8 *, struct Machine *, long);

void Int64ToFloat(i64, struct Machine *, long);
void Int32ToFloat(i32, struct Machine *, long);
void Int64ToDouble(i64, struct Machine *, long);
void Int32ToDouble(i32, struct Machine *, long);
void MovsdWpsVpsOp(u8 *, struct Machine *, long);

void LogCpu(struct Machine *);
const char *GetBacktrace(struct Machine *);
const char *DescribeOp(struct Machine *, i64);
int GetInstruction(struct Machine *, i64, struct XedDecodedInst *);

struct FileMap *GetFileMap(struct System *, i64);
const char *GetDirFildesPath(struct System *, int);
struct FileMap *AddFileMap(struct System *, i64, i64, const char *, u64);

void FlushCod(struct JitBlock *);
#if LOG_COD
void SetupCod(struct Machine *);
void WriteCod(const char *, ...) printf_attr(1);
void LogCodOp(struct Machine *, const char *);
#else
#define SetupCod(m)    (void)0
#define LogCodOp(m, s) (void)0
#define WriteCod(...)  (void)0
#endif

MICRO_OP_SAFE u8 Cpl(struct Machine *m) {
  return m->cs.sel & 3u;
}

#define BEGIN_NO_PAGE_FAULTS \
  {                          \
    bool nofault_;           \
    nofault_ = m->nofault;   \
    m->nofault = true

#define END_NO_PAGE_FAULTS \
  m->nofault = nofault_;   \
  }

#endif /* BLINK_MACHINE_H_ */
