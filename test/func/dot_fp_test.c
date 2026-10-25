#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef __x86_64__
#define RUN(op, imm) \
  __asm__ volatile("ldmxcsr %3\n\tmovdqu %4,%%xmm0\n\tmovdqu %5,%%xmm1\n\t" \
                   op " $" #imm ",%%xmm1,%%xmm0\n\tmovdqu %%xmm0,%0\n\t" \
                   "stmxcsr %1\n\tldmxcsr %2" \
                   : "=m"(out), "=m"(status) \
                   : "m"(normal), "m"(control), "m"(left), "m"(right) \
                   : "xmm0", "xmm1", "memory")
static int Check(int wide, unsigned mode, uint64_t a, uint64_t b,
                 uint64_t c, uint64_t d, uint64_t expected, unsigned flags,
                 unsigned extra) {
  uint64_t left[2], right[2], out[2];
  unsigned status, normal = 0x1f80, control = normal | mode << 13 | extra;
  if (wide) {
    left[0] = a; left[1] = c; right[0] = b; right[1] = d;
    RUN("dppd", 49);
  } else {
    left[0] = a | c << 32; left[1] = 0x7f8000017f800001;
    right[0] = b | d << 32; right[1] = 0x7f8000017f800001;
    RUN("dpps", 49);
  }
  if (out[0] != expected || out[1] || (status & 63) != flags) {
    printf("wide=%d mode=%u extra=%x got=%llx status=%x expected=%llx flags=%x\n",
           wide, mode, extra, (unsigned long long)out[0], status,
           (unsigned long long)expected, flags);
    return 1;
  }
  return 0;
}
#endif

int main(void) {
#ifdef __x86_64__
  unsigned mode;
  for (mode = 0; mode < 4; ++mode) {
    if (Check(0, mode, 0x3f800000, 0x3f800000, 0x33800000, 0x3f800000,
              0x3f800000 + (mode == 2), 0x20, 0)) return 1;
    if (Check(1, mode, UINT64_C(0x3ff0000000000000), UINT64_C(0x3ff0000000000000),
              UINT64_C(0x3ca0000000000000), UINT64_C(0x3ff0000000000000),
              UINT64_C(0x3ff0000000000000) + (mode == 2), 0x20, 0)) return 2;
  }
  if (Check(0, 0, 0, 0x7f800000, 0, 0, 0xffc00000, 1, 0)) return 20;
  if (Check(1, 0, 0, UINT64_C(0x7ff0000000000000), 0, 0,
            UINT64_C(0xfff8000000000000), 1, 0)) return 21;
  if (Check(0, 0, 0x7fc12345, 0x7f801234, 0, 0, 0x7fc12345, 1, 0)) return 22;
  if (Check(0, 0, 0xff812345, 0x7fc01234, 0, 0, 0xffc12345, 1, 0)) return 23;
  if (Check(0, 0, 0x7fc12345, 1, 0, 0, 0x7fc12345, 0, 0)) return 24;
  if (Check(1, 0, UINT64_C(0x7ff8000000012345), UINT64_C(0x7ff0000000001234),
            0, 0, UINT64_C(0x7ff8000000012345), 1, 0)) return 25;
  if (Check(0, 0, 1, 0x3f800000, 0, 0, 1, 2, 0)) return 3;
  if (Check(0, 0, 1, 0x3f800000, 0, 0, 0, 0, 0x40)) return 4;
  if (Check(0, 0, 1, 0x3f800000, 0, 0, 0, 0x32, 0x8000)) return 5;
  if (Check(1, 0, 1, UINT64_C(0x3ff0000000000000), 0, 0, 1, 2, 0)) return 6;
  if (Check(1, 0, 1, UINT64_C(0x3ff0000000000000), 0, 0, 0, 0, 0x40)) return 7;
  if (Check(0, 0, 0x7f7fffff, 0x40000000, 0, 0, 0x7f800000, 0x28, 0)) return 8;
#endif
  return 0;
}
