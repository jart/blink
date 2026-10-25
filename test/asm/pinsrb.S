#include "test/asm/mac.inc"
.macro check lane:req, memory=0
        .test "pinsrb lane \lane case \@"
        pcmpeqb %xmm8,%xmm8
        movdqu %xmm8,(%rsp)
        movb $0xab,\lane(%rsp)
        mov $0x123456ab,%r9d
        cmp %rsp,%rsp
        stc
.if \memory
        pinsrb $(240+\lane),source(%rip),%xmm8
.else
        pinsrb $(240+\lane),%r9d,%xmm8
.endif
        .c
        .e
        .p
        .ns
        .no
        movdqu (%rsp),%xmm9
        pcmpeqb %xmm9,%xmm8
        pmovmskb %xmm8,%eax
        cmp $65535,%eax
        .e
.endm
.globl _start
_start:
        sub $16,%rsp
.irp lane,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15
        check \lane
        check \lane,1
.endr
        .test "pinsrb reads BPL rather than CH without REX"
        mov $0x123456ab,%ebp
        mov $0x9900,%ecx
        pxor %xmm0,%xmm0
        pinsrb $0,%ebp,%xmm0
        movd %xmm0,%eax
        cmp $0xab,%eax
        .e
        .exit
.section .rodata
source: .byte 0xab
