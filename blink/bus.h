#ifndef BLINK_MOP_H_
#define BLINK_MOP_H_
#include <limits.h>
#include <stdbool.h>

#include "blink/assert.h"
#include "blink/atomic.h"
#include "blink/builtin.h"
#include "blink/dll.h"
#include "blink/endian.h"
#include "blink/spin.h"
#include "blink/thread.h"
#include "blink/tsan.h"
#include "blink/tunables.h"
#include "blink/types.h"

#define FUTEX_CONTAINER(e) DLL_CONTAINER(struct Futex, elem, e)

struct Futex {
  i64 addr;
  int waiters;
  struct Dll elem;
  pthread_cond_t_ cond;
  pthread_mutex_t_ lock;
};

struct Futexes {
  struct Dll *active;
  struct Dll *free;
  pthread_mutex_t_ lock;
  struct Futex mem[kFutexMax];
};

struct Bus {
  /* When software uses locks or semaphores to synchronize processes,
     threads, or other code sections; Intel recommends that only one lock
     or semaphore be present within a cache line (or 128 byte sector, if
     128-byte sector is supported). In processors based on Intel NetBurst
     microarchitecture (which support 128-byte sector consisting of two
     cache lines), following this recommendation means that each lock or
     semaphore should be contained in a 128-byte block of memory that
     begins on a 128-byte boundary. The practice minimizes the bus traffic
     required to service locks. ──Intel V.3 §8.10.6.7 */
  _Alignas(kSemSize) _Atomic(u32) lock[kBusCount][kSemSize / sizeof(int)];
  struct Futexes futexes;
};

extern struct Bus *g_bus;

void InitBus(void);
void LockBus(const u8 *);
void UnlockBus(const u8 *);

i64 Load8(const u8[1]) nosideeffect;
i64 Load16(const u8[2]) nosideeffect;
i64 Load32(const u8[4]) nosideeffect;
i64 Load64(const u8[8]) nosideeffect;
i64 Load64Unlocked(const u8[8]) nosideeffect;

void Store8(u8[1], u64);
void Store16(u8[2], u64);
void Store32(u8[4], u64);
void Store64(u8[8], u64);
void Store64Unlocked(u8[8], u64);

i64 ReadRegisterSigned(u64, u8[8]);
u64 ReadMemory(u64, u8[8]);
u64 ReadMemorySigned(u64, u8[8]);
u64 ReadMemoryUnlocked(u64, u8[8]);
u64 ReadRegister(u64, u8[8]);
void WriteMemory(u64, u8[8], u64);
void WriteRegister(u64, u8[8], u64);
void WriteRegisterOrMemory(u64, u8[8], u64);

i64 ReadMemoryBW(u64, u8[8]);
i64 ReadRegisterBW(u64, u8[8]);
void WriteMemoryBW(u64, u8[8], u64);
void WriteRegisterBW(u64, u8[8], u64);
i64 ReadRegisterOrMemoryBW(u64, u8[8]);
void WriteRegisterOrMemoryBW(u64, u8[8], u64);
u64 LoadPte32_(const u8 *) nosideeffect;
bool CasPte32_(u8 *, u64, u64);
void StorePte32_(u8 *, u64);

nosideeffect static inline u64 LoadPte(const u8 *pte) {
#if CAN_64BIT
  return Little64(
      atomic_load_explicit((_Atomic(u64) *)pte, memory_order_acquire));
#else
  return LoadPte32_(pte);
#endif
}

static inline void StorePte(u8 *pte, u64 val) {
#if CAN_64BIT
  atomic_store_explicit((_Atomic(u64) *)pte, Little64(val),
                        memory_order_release);
#else
  StorePte32_(pte, val);
#endif
}

static inline bool CasPte(u8 *pte, u64 oldval, u64 newval) {
#if CAN_64BIT
  oldval = Little64(oldval);
  return atomic_compare_exchange_strong_explicit(
      (_Atomic(u64) *)pte, &oldval, Little64(newval),  //
      memory_order_release, memory_order_relaxed);
#else
  return CasPte32_(pte, oldval, newval);
#endif
}

#endif /* BLINK_MOP_H_ */
