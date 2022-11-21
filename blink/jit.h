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
intptr_t FinishJit(struct Jit *, struct JitPage *, hook_t *, intptr_t);
intptr_t SpliceJit(struct Jit *, struct JitPage *, hook_t *, intptr_t,
                   intptr_t);

#endif /* BLINK_JIT_H_ */
