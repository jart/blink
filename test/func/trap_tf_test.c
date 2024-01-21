/*-*- mode:c;indent-tabs-mode:nil;c-basic-offset:2;tab-width:8;coding:utf-8 -*-│
│vi: set net ft=c ts=2 sts=2 sw=2 fenc=utf-8                                :vi│
╞══════════════════════════════════════════════════════════════════════════════╡
│ Copyright 2023 Justine Alexandra Roberts Tunney                              │
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
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "blink/macros.h"

static volatile size_t expect_next = 0;
static const unsigned char expect[][3] = {
    {0x98, 0xb6},  // [0]
    {0x35, 0xb2},  // [1]
    {0xc7, 0xf1},  // [2]
    {0xf1, 0x9d},  // [3]
    {0x9d, 0x67},  // [4]
    {0x66, 0x58},  // [5]
    {0x58, 0x0f},  // [6]
    {0x9d, 0x99},  // [7]
};

void OnSig(int sig, siginfo_t *si, void *ptr) {
  const unsigned char *addr = si->si_addr;
  if (expect_next >= ARRAYLEN(expect)) _exit(1);
  if (addr[-1] != expect[expect_next][0] ||
      addr[ 0] != expect[expect_next][1]) {
    _exit(2 + expect_next);
  }
  ++expect_next;
}

int main(int argc, char *argv[]) {
  unsigned i;
  sigaction(SIGTRAP, &(struct sigaction){.sa_sigaction = OnSig,
                                         .sa_flags = SA_SIGINFO}, 0);
  for (i = 0; i < 15; ++i) {
    asm("pushfq\n\t"
        "pop\t%%rax\n\t"
        "or\t$0x100,%%ax\n\t"
        "pushfq\n\t"
        "push\t%%rax\n\t"
        "pushfq\n\t"
        "push\t%%rax\n\t"
        "popfq\n\t"
        "cwtl\n\t"             // 98     [0]
        "mov\t$0x35,%%dh\n\t"  // b6 35  [1]
        "mov\t$0xc7,%%dl\n\t"  // b2 c7  [2]
        ".byte 0xf1\n\t"       // f1     [3] explicit int1
        "popfq\n\t"            // 9d     [4]
        "addr32 popfq\n\t"     // 67 9d      no int1 as TF = 0 from last popfq
        "push $102\n\t"        // 6a 66  [5] SYS_getuid
        "pop %%rax\n\t"        // 58     [6]
        "syscall\n\t"          // 0f 05      no int1 right after syscall
                               //            opcode; may clobber rcx & r11
        "cs popfq\n\t"         // 2e 9d  [7]
        "cltd"                 // 99
        : : : "rax", "rcx", "rdx", "r11", "flags");
    if (expect_next != ARRAYLEN(expect)) return 255;
    expect_next = 0;
  }
  return 0;
}
