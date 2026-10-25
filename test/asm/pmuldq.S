#include "test/asm/mac.inc"
.globl _start
_start:
        sub $64,%rsp
.macro check a,b,c,d,e,f
        .test "pmuldq \a \b \c \d"
        movl $\a,0(%rsp)
        movl $0x80000001,4(%rsp)
        movl $\b,8(%rsp)
        movl $0xffffffff,12(%rsp)
        movl $\c,16(%rsp)
        movl $0x12345678,20(%rsp)
        movl $\d,24(%rsp)
        movl $0x7fffffff,28(%rsp)
.irp mem,0,1
        movdqu (%rsp),%xmm8
        movdqu 16(%rsp),%xmm9
        xor %eax,%eax
        stc
        pushfq
        pop %r10
.if \mem
        pmuldq 16(%rsp),%xmm8
.else
        pmuldq %xmm9,%xmm8
.endif
        pushfq
        pop %r11
        cmp %r10,%r11
        .e
        movdqu %xmm8,32(%rsp)
        movabs $\e,%rax
        cmp %rax,32(%rsp)
        .e
        movabs $\f,%rax
        cmp %rax,40(%rsp)
        .e
        movdqu %xmm9,48(%rsp)
        mov 16(%rsp),%rax
        cmp %rax,48(%rsp)
        .e
        mov 24(%rsp),%rax
        cmp %rax,56(%rsp)
        .e
.endr
.endm
        check -2147483648,2147483647,-2147483648,2147483647,0x4000000000000000,0x3fffffff00000001
        check -2147483648,2147483647,2147483647,-2147483648,0xc000000080000000,0xc000000080000000
        check -1,-2147483648,-1,-1,1,2147483648
        check 0,123456,2147483647,-7,0,-864192
        .test "pmuldq alias"
        movdqu (%rsp),%xmm8
        pmuldq %xmm8,%xmm8
        movdqu %xmm8,32(%rsp)
        cmpq $0,32(%rsp)
        .e
        movabs $15241383936,%rax
        cmp %rax,40(%rsp)
        .e
        .exit
