#ifndef BLINK_XMM_H_
#define BLINK_XMM_H_
#include "blink/builtin.h"
#include "blink/types.h"

#ifdef __CYGWIN__
#define XMM_TYPE           void
#define RETURN_XMM(ax, dx) asm volatile("" : : "a"(ax), "d"(dx))
#else
struct Xmm {
  u64 lo;
  u64 hi;
};
#define XMM_TYPE struct Xmm
#define RETURN_XMM(ax, dx) \
  return (struct Xmm) {    \
    ax, dx                 \
  }
#endif

#endif /* BLINK_XMM_H_ */
