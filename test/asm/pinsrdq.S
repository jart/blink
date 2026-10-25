#include "test/asm/mac.inc"
.globl _start
_start:
        sub $16,%rsp
        .test "pinsrd lane 0 register"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov $0x12345678,%r8d
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrd $252,%r8d,%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpl $0x12345678,0(%rsp)
        .e
        cmpl $0xffffffff,4(%rsp)
        .e
        cmpl $0xffffffff,8(%rsp)
        .e
        cmpl $0xffffffff,12(%rsp)
        .e
        .test "pinsrd lane 1 register"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov $0x12345678,%r8d
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrd $253,%r8d,%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpl $0xffffffff,0(%rsp)
        .e
        cmpl $0x12345678,4(%rsp)
        .e
        cmpl $0xffffffff,8(%rsp)
        .e
        cmpl $0xffffffff,12(%rsp)
        .e
        .test "pinsrd lane 2 register"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov $0x12345678,%r8d
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrd $254,%r8d,%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpl $0xffffffff,0(%rsp)
        .e
        cmpl $0xffffffff,4(%rsp)
        .e
        cmpl $0x12345678,8(%rsp)
        .e
        cmpl $0xffffffff,12(%rsp)
        .e
        .test "pinsrd lane 3 register"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov $0x12345678,%r8d
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrd $255,%r8d,%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpl $0xffffffff,0(%rsp)
        .e
        cmpl $0xffffffff,4(%rsp)
        .e
        cmpl $0xffffffff,8(%rsp)
        .e
        cmpl $0x12345678,12(%rsp)
        .e
        .test "pinsrd lane 0 memory"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov $0x12345678,%r8d
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrd $252,(%rsp),%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpl $0x12345678,0(%rsp)
        .e
        cmpl $0xffffffff,4(%rsp)
        .e
        cmpl $0xffffffff,8(%rsp)
        .e
        cmpl $0xffffffff,12(%rsp)
        .e
        .test "pinsrd lane 1 memory"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov $0x12345678,%r8d
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrd $253,(%rsp),%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpl $0xffffffff,0(%rsp)
        .e
        cmpl $0x12345678,4(%rsp)
        .e
        cmpl $0xffffffff,8(%rsp)
        .e
        cmpl $0xffffffff,12(%rsp)
        .e
        .test "pinsrd lane 2 memory"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov $0x12345678,%r8d
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrd $254,(%rsp),%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpl $0xffffffff,0(%rsp)
        .e
        cmpl $0xffffffff,4(%rsp)
        .e
        cmpl $0x12345678,8(%rsp)
        .e
        cmpl $0xffffffff,12(%rsp)
        .e
        .test "pinsrd lane 3 memory"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov $0x12345678,%r8d
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrd $255,(%rsp),%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpl $0xffffffff,0(%rsp)
        .e
        cmpl $0xffffffff,4(%rsp)
        .e
        cmpl $0xffffffff,8(%rsp)
        .e
        cmpl $0x12345678,12(%rsp)
        .e
        .test "pinsrq lane 0 register"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrq $254,%r8,%xmm8
        .e
        movdqu %xmm8,(%rsp)
        movabs $0x123456789abcdef0,%rax
        cmp %rax,0(%rsp)
        .e
        movabs $0xffffffffffffffff,%rax
        cmp %rax,8(%rsp)
        .e
        .test "pinsrq lane 1 register"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrq $255,%r8,%xmm8
        .e
        movdqu %xmm8,(%rsp)
        movabs $0xffffffffffffffff,%rax
        cmp %rax,0(%rsp)
        .e
        movabs $0x123456789abcdef0,%rax
        cmp %rax,8(%rsp)
        .e
        .test "pinsrq lane 0 memory"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrq $254,(%rsp),%xmm8
        .e
        movdqu %xmm8,(%rsp)
        movabs $0x123456789abcdef0,%rax
        cmp %rax,0(%rsp)
        .e
        movabs $0xffffffffffffffff,%rax
        cmp %rax,8(%rsp)
        .e
        .test "pinsrq lane 1 memory"
        pcmpeqb %xmm8,%xmm8
        movabs $0x123456789abcdef0,%r8
        mov %r8,(%rsp)
        cmp %r8,%r8
        pinsrq $255,(%rsp),%xmm8
        .e
        movdqu %xmm8,(%rsp)
        movabs $0xffffffffffffffff,%rax
        cmp %rax,0(%rsp)
        .e
        movabs $0x123456789abcdef0,%rax
        cmp %rax,8(%rsp)
        .e
        .exit
