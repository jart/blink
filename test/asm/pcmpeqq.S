#include "test/asm/mac.inc"
.globl _start
_start:
        sub $64,%rsp
.macro check a,b,c,d,e,f
        .test "pcmpeqq \a \b \c \d"
        movabs $\a,%rax
        mov %rax,0(%rsp)
        movabs $\b,%rax
        mov %rax,8(%rsp)
        movabs $\c,%rax
        mov %rax,16(%rsp)
        movabs $\d,%rax
        mov %rax,24(%rsp)
.irp mem,0,1
        movdqu (%rsp),%xmm8
        movdqu 16(%rsp),%xmm9
        xor %eax,%eax
        stc
        pushfq
        pop %r10
.if \mem
        pcmpeqq 16(%rsp),%xmm8
.else
        pcmpeqq %xmm9,%xmm8
.endif
        pushfq
        pop %r11
        cmp %r10,%r11
        .e
        movdqu %xmm8,32(%rsp)
        cmpq $\e,32(%rsp)
        .e
        cmpq $\f,40(%rsp)
        .e
        movdqu %xmm9,48(%rsp)
        mov 16(%rsp),%rax
        cmp %rax,48(%rsp)
        .e
        mov 24(%rsp),%rax
        cmp %rax,56(%rsp)
        .e
.endr
        pcmpeqq %xmm9,%xmm9
        movdqu %xmm9,32(%rsp)
        cmpq $-1,32(%rsp)
        .e
        cmpq $-1,40(%rsp)
        .e
.endm
        check 0,-1,0,-1,-1,-1
        check 1,-1,0,-1,0,-1
        check 0,1,0,-1,-1,0
        check 0x100000000,0x100000001,0,1,0,0
        check 0x100000000,0x100000001,0x100000001,0x100000000,0,0
        check 0x8000000000000000,0x7fffffffffffffff,0x8000000000000000,0x7fffffffffffffff,-1,-1
        .exit
