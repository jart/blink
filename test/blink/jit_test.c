/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2022 Justine Alexandra Roberts Tunney                              │
│                                                                              │
│ Permission to use, copy, modify, and/or distribute this software for         │
│ any purpose with or without fee is hereby granted, provided that the         │
│ above copyright notice and this permission notice appear in all copies.      │
│                                                                              │
│ THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL                │
│ WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED                │
│ WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE             │
│ AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL         │
│ DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR        │
│ PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER               │
│ TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR             │
│ PERFORMANCE OF THIS SOFTWARE.                                                │
╚─────────────────────────────────────────────────────────────────────────────*/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blink/builtin.h"
#include "blink/jit.h"
#include "test/test.h"

#if (defined(__x86_64__) || defined(__aarch64__)) && \
    !__has_feature(memory_sanitizer)

typedef int (*add_f)(int, int);
typedef u64 (*noparam_f)(void);
typedef u64 (*dispatch_f)(u64, u64, u64, u64, u64, u64);

u64 g_r0;
u64 g_p0;
u64 g_p1;
u64 g_p2;
u64 g_p3;
u64 g_p4;
u64 g_p5;
struct Jit jit;

void SetUp(void) {
  InitJit(&jit);
  g_r0 = 0;
  g_p0 = 0;
  g_p1 = 0;
  g_p2 = 0;
  g_p3 = 0;
  g_p4 = 0;
  g_p5 = 0;
}

void TearDown(void) {
  DestroyJit(&jit);
}

u64 PickMask(u64 x) {
  switch (x % 9) {
    case 0:
      return 0xffffffffffffffff;
    case 1:
      return 0x00000000ffffffff;
    case 2:
      return 0x000000000000ffff;
    case 3:
      return 0x00000000ffff0000;
    case 4:
      return 0x0000ffff00000000;
    case 5:
      return 0xffff000000000000;
    case 6:
      return 0xffff00000000ffff;
    case 7:
      return 0xffff0000ffff0000;
    case 8:
      return 0xffffffffffff0000;
    default:
      abort();
  }
}

u64 Rando(void) {
  static u64 s;
  u64 z = (s += 0x9e3779b97f4a7c15);
  z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
  z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
  z = (z ^ (z >> 31)) & PickMask(z);
  return z;
}

u64 CallMe(u64 p0, u64 p1, u64 p2, u64 p3, u64 p4, u64 p5) {
  g_p0 = p0;
  g_p1 = p1;
  g_p2 = p2;
  g_p3 = p3;
  g_p4 = p4;
  g_p5 = p5;
  g_r0 = Rando();
  return g_r0;
}

int staging(int x, int y) {
  return 666;
}

TEST(jit, func) {
  add_f add;
  struct JitPage *jp;
#if defined(__x86_64__)
  u8 code[] = {
      0x8d, 0x04, 0x37,  // lea (%rdi,%rsi,1),%eax
  };
#elif defined(__aarch64__)
  u32 code[] = {
      0x0b000020,  // add w0,w1,w0
  };
#endif
  ASSERT_NOTNULL((jp = StartJit(&jit)));
  ASSERT_TRUE(AppendJit(jp, code, sizeof(code)));
  ASSERT_NE(0, FinishJit(&jit, jp, (hook_t *)&add, (intptr_t)staging));
  ASSERT_EQ(666, add(2, 2));
  ASSERT_EQ(1, FlushJit(&jit));
  ASSERT_EQ(4, add(2, 2));
  ASSERT_EQ(23, add(20, 3));
}

TEST(jit, call) {
  u64 r0;
  dispatch_f fun;
  struct JitPage *jp;
  ASSERT_NOTNULL((jp = StartJit(&jit)));
  ASSERT_TRUE(AppendJitCall(jp, (void *)&CallMe));
  ASSERT_NE(0, FinishJit(&jit, jp, (hook_t *)&fun, 0));
  ASSERT_EQ(1, FlushJit(&jit));
  r0 = fun(1, 2, 3, 4, 5, 6);
  ASSERT_EQ(g_r0, r0);
  ASSERT_EQ(1, g_p0);
  ASSERT_EQ(2, g_p1);
  ASSERT_EQ(3, g_p2);
  ASSERT_EQ(4, g_p3);
  ASSERT_EQ(5, g_p4);
  ASSERT_EQ(6, g_p5);
}

TEST(jit, param) {
  int i;
  noparam_f fun;
  struct JitPage *jp;
  u64 r0, p0, p1, p2, p3, p4, p5;
  for (i = 0; i < 2000; ++i) {
    ASSERT_NOTNULL((jp = StartJit(&jit)));
    ASSERT_TRUE(AppendJitSetArg(jp, 0, (p0 = Rando())));
    ASSERT_TRUE(AppendJitSetArg(jp, 1, (p1 = Rando())));
    ASSERT_TRUE(AppendJitSetArg(jp, 2, (p2 = (i - 1000) * 2)));
    ASSERT_TRUE(AppendJitSetArg(jp, 3, (p3 = Rando())));
    ASSERT_TRUE(AppendJitSetArg(jp, 4, (p4 = Rando())));
    ASSERT_TRUE(AppendJitSetArg(jp, 5, (p5 = i - 500)));
    ASSERT_TRUE(AppendJitCall(jp, (void *)&CallMe));
    ASSERT_NE(0, FinishJit(&jit, jp, (hook_t *)&fun, 0));
    ASSERT_EQ(1, FlushJit(&jit));
    r0 = fun();
    ASSERT_EQ(g_r0, r0);
    ASSERT_EQ(p0, g_p0);
    ASSERT_EQ(p1, g_p1);
    ASSERT_EQ(p2, g_p2);
    ASSERT_EQ(p3, g_p3);
    ASSERT_EQ(p4, g_p4);
    ASSERT_EQ(p5, g_p5);
  }
}

#else
void SetUp(void) {
}
void TearDown(void) {
}
#endif /* architecture check */
