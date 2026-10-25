#include "test/asm/mac.inc"
.macro check left:req, right:req, expected:req, memory=0
        .test "ptest case \@"
        movdqu \left(%rip),%xmm8
        movdqu \right(%rip),%xmm9
        pushq $0xcd5
        popfq
.if \memory
        ptest \right(%rip),%xmm8
.else
        ptest %xmm9,%xmm8
.endif
        pushfq
        pop %rax
        cld
        and $0xcd5,%eax
        cmp $(\expected|0x400),%eax
        .e
        pcmpeqb \left(%rip),%xmm8
        pmovmskb %xmm8,%eax
        cmp $65535,%eax
        .e
        pcmpeqb \right(%rip),%xmm9
        pmovmskb %xmm9,%eax
        cmp $65535,%eax
        .e
.endm
.globl _start
_start:
        check zero,zero,65,0
        check zero,zero,65,1
        check zero,ones,64,0
        check zero,ones,64,1
        check ones,zero,65,0
        check ones,zero,65,1
        check ones,ones,1,0
        check ones,ones,1,1
        check low1,low2,64,0
        check low1,low2,64,1
        check low1,low3,0,0
        check low1,low3,0,1
        check upper1,upper2,64,0
        check upper1,upper2,64,1
        check upper1,upper3,0,0
        check upper1,upper3,0,1
        check upper1,upper1,1,0
        check upper1,upper1,1,1
        .test "ptest aliased operands"
        movdqu upper1(%rip),%xmm8
        ptest %xmm8,%xmm8
        .c
        .nz
        .exit
.section .rodata
.balign 16
zero: .quad 0,0
ones: .quad -1,-1
low1: .quad 1,0
low2: .quad 2,0
low3: .quad 3,0
upper1: .quad 0,1
upper2: .quad 0,2
upper3: .quad 0,3
