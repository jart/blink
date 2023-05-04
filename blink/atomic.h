#ifndef BLINK_ATOMIC_H_
#define BLINK_ATOMIC_H_
#include "blink/builtin.h"
#include "blink/thread.h"
#if defined(HAVE_FORK) || defined(HAVE_THREADS)
#include <stdatomic.h>
#else
#include "blink/types.h"

#define _Atomic(t) t

#define atomic_thread_fence(order) (void)0

#define atomic_load_explicit(ptr, order) *(ptr)

#define atomic_store_explicit(ptr, val, order) (*(ptr) = (val))

#define atomic_exchange(ptr, val)                        \
  (sizeof(*(ptr)) == 8   ? Exchange64((u64 *)(ptr), val) \
   : sizeof(*(ptr)) == 4 ? Exchange32((u32 *)(ptr), val) \
   : sizeof(*(ptr)) == 2 ? Exchange16((u16 *)(ptr), val) \
                         : Exchange8((u8 *)(ptr), val))

#define atomic_exchange_explicit(ptr, val, order) atomic_exchange(ptr, val)

#define atomic_compare_exchange_strong_explicit(                             \
    ifthing, isequaltome, replaceitwithme, success_order, failure_order)     \
  (*(ifthing) == *(isequaltome)                                              \
       ? (*(isequaltome) = *(ifthing), *(ifthing) = (replaceitwithme), true) \
       : (*(isequaltome) = *(ifthing), false))

#define atomic_compare_exchange_weak_explicit(x, y, z, s, f) \
  atomic_compare_exchange_strong_explicit(x, y, z, s, f)

#define atomic_fetch_add_explicit(ptr, delta, order)       \
  (sizeof(*(ptr)) == 8   ? FetchAdd64((u64 *)(ptr), delta) \
   : sizeof(*(ptr)) == 4 ? FetchAdd32((u32 *)(ptr), delta) \
                         : FetchAddAbort())

static inline u64 Exchange64(u64 *ptr, u64 val) {
  u64 tmp = *ptr;
  *ptr = val;
  return tmp;
}

static inline u32 Exchange32(u32 *ptr, u32 val) {
  u32 tmp = *ptr;
  *ptr = val;
  return tmp;
}

static inline u16 Exchange16(u16 *ptr, u16 val) {
  u16 tmp = *ptr;
  *ptr = val;
  return tmp;
}

static inline u8 Exchange8(u8 *ptr, u8 val) {
  u8 tmp = *ptr;
  *ptr = val;
  return tmp;
}

static inline u64 FetchAdd64(u64 *ptr, u64 val) {
  u64 res = *ptr;
  *ptr += val;
  return res;
}

static inline u32 FetchAdd32(u32 *ptr, u32 val) {
  u32 res = *ptr;
  *ptr += val;
  return res;
}

static inline u32 FetchAddAbort(void) {
  volatile u32 x = 0;
  return 1 / x;
}

#endif /* !HAVE_FORK && !HAVE_THREADS */
#endif /* BLINK_ATOMIC_H_ */
