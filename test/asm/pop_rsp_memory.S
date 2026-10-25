#include "test/asm/mac.inc"
.globl _start
_start:
        sub $64,%rsp
        .test "popq memory uses incremented rsp"
        movq $0x1234,(%rsp)
        movq $0x5678,8(%rsp)
        movq $0x9abc,16(%rsp)
        mov %rsp,%r8
        cmp %r8,%r8
        popq 8(%rsp)
        .e
        cmpq $0x5678,(%rsp)
        .e
        cmpq $0x1234,8(%rsp)
        .e
        lea 8(%r8),%rax
        cmp %rax,%rsp
        .e
        .test "popw memory uses two-byte increment"
        movw $0x1234,(%rsp)
        movw $0x5678,8(%rsp)
        movw $0x9abc,10(%rsp)
        mov %rsp,%r8
        popw 8(%rsp)
        cmpw $0x5678,6(%rsp)
        .e
        cmpw $0x1234,8(%rsp)
        .e
        lea 2(%r8),%rax
        cmp %rax,%rsp
        .e
        .test "popq memory uses rsp and scaled index"
        movq $0x1234,(%rsp)
        movq $0x5678,16(%rsp)
        movq $0x9abc,24(%rsp)
        mov $2,%r9
        popq (%rsp,%r9,8)
        cmpq $0x5678,8(%rsp)
        .e
        cmpq $0x1234,16(%rsp)
        .e
        .exit
