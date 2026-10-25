#include "test/asm/mac.inc"
.globl _start
_start:
        sub $32,%rsp
        movq $11,0(%rsp)
        movq $22,8(%rsp)
        movq $33,16(%rsp)
        movq $44,24(%rsp)
        movdqu (%rsp),%xmm8
        movdqu 16(%rsp),%xmm9
        .test "unpckhpd distinct registers"
        cmp %rsp,%rsp
        unpckhpd %xmm9,%xmm8
        .e
        movdqu %xmm8,(%rsp)
        cmpq $22,0(%rsp)
        .e
        cmpq $44,8(%rsp)
        .e
        .test "unpckhpd memory source"
        movq $55,24(%rsp)
        unpckhpd 16(%rsp),%xmm8
        movdqu %xmm8,(%rsp)
        cmpq $44,0(%rsp)
        .e
        cmpq $55,8(%rsp)
        .e
        .test "unpckhpd aliased registers"
        unpckhpd %xmm9,%xmm9
        movdqu %xmm9,(%rsp)
        cmpq $44,0(%rsp)
        .e
        cmpq $44,8(%rsp)
        .e
        .exit
