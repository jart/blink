#ifndef BLINK_SPIN_H_
#define BLINK_SPIN_H_
#include "blink/atomic.h"
#include "blink/types.h"

static inline void SpinLock(_Atomic(u32) *lock) {
  int x;
  for (;;) {
    x = atomic_exchange_explicit(lock, 1, memory_order_acquire);
    if (!x) break;
  }
}

static inline void SpinUnlock(_Atomic(u32) *lock) {
  atomic_store_explicit(lock, 0, memory_order_release);
}

#endif /* BLINK_SPIN_H_ */
