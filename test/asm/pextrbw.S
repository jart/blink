#include "test/asm/mac.inc"
.globl _start
_start:
        sub $32,%rsp
        movabs $0x7766554433221100,%rax
        mov %rax,(%rsp)
        movabs $0xffeeddccbbaa9988,%rax
        mov %rax,8(%rsp)
        movdqu (%rsp),%xmm9
        movdqa %xmm9,%xmm0
.macro check_extract op, lane, expected, size, high
        .test "\op lane \lane"
        mov $-1,%r8
        xor %eax,%eax
        stc
        pushfq
        pop %r10
        \op $(\lane|\high),%xmm9,%r8d
        pushfq
        pop %r11
        cmp %r10,%r11
        .e
        cmp $\expected,%r8
        .e
        movq $-1,16(%rsp)
        \op $(\lane|\high),%xmm9,19(%rsp)
        cmpb $-1,18(%rsp)
        .e
        cmpb $-1,(19+\size)(%rsp)
        .e
.if \size == 1
        movzbl 19(%rsp),%eax
.else
        movzwl 19(%rsp),%eax
.endif
        cmp $\expected,%eax
        .e
        // Register field 5 addresses EBP, not CH, even without REX.
        mov $-1,%rbp
        \op $\lane,%xmm0,%ebp
        cmp $\expected,%rbp
        .e
        movdqu %xmm9,(%rsp)
        movabs $0x7766554433221100,%rax
        cmp %rax,(%rsp)
        .e
        movabs $0xffeeddccbbaa9988,%rax
        cmp %rax,8(%rsp)
        .e
.endm
.irp lane,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        check_extract pextrb,\lane,(\lane*17),1,240
.endr
.irp lane,0,1,2,3,4,5,6,7
        check_extract pextrw,\lane,((\lane*34)|((\lane*34+17)<<8)),2,248
.endr
        .test "SSE4 PEXTRW register encoding ignores REX.W"
        mov $-1,%r8
        .byte 0x66,0x4d,0x0f,0x3a,0x15,0xc8,0xff
        cmp $0xffee,%r8
        .e
        .test "PEXTRB register encoding ignores REX.W"
        mov $-1,%r8
        .byte 0x66,0x4d,0x0f,0x3a,0x14,0xc8,0xff
        cmp $0xff,%r8
        .e
        .exit
