#include "test/asm/mac.inc"
.globl _start
_start:
        sub $32,%rsp
.macro check n,expected,index
.irp form,0,1,2
        .test "phminposuw \n \form"
        movdqu data\n(%rip),%xmm9
        pcmpeqb %xmm8,%xmm8
.if \form == 2
        movdqa %xmm9,%xmm8
.endif
        xor %eax,%eax
        stc
        pushfq
        pop %r10
.if \form == 0
        phminposuw %xmm9,%xmm8
.elseif \form == 1
        phminposuw data\n(%rip),%xmm8
.else
        phminposuw %xmm8,%xmm8
.endif
        pushfq
        pop %r11
        cmp %r10,%r11
        .e
        movdqu %xmm8,(%rsp)
        cmpq $(\expected|(\index<<16)),(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        movdqu %xmm9,16(%rsp)
        mov data\n(%rip),%rax
        cmp %rax,16(%rsp)
        .e
        mov data\n+8(%rip),%rax
        cmp %rax,24(%rsp)
        .e
.endr
.endm
        check 0,32768,0
        check 1,32768,1
        check 2,32768,2
        check 3,32768,3
        check 4,32768,4
        check 5,32768,5
        check 6,32768,6
        check 7,32768,7
        check 8,3,0
        check 9,2,2
        check 10,0,7
        check 11,32767,2
        .exit
.section .rodata
.balign 16
data0:
        .short 32768,65535,65535,65535,65535,65535,65535,65535
data1:
        .short 65535,32768,65535,65535,65535,65535,65535,65535
data2:
        .short 65535,65535,32768,65535,65535,65535,65535,65535
data3:
        .short 65535,65535,65535,32768,65535,65535,65535,65535
data4:
        .short 65535,65535,65535,65535,32768,65535,65535,65535
data5:
        .short 65535,65535,65535,65535,65535,32768,65535,65535
data6:
        .short 65535,65535,65535,65535,65535,65535,32768,65535
data7:
        .short 65535,65535,65535,65535,65535,65535,65535,32768
data8:
        .short 3,3,3,3,3,3,3,3
data9:
        .short 9,4,2,4,2,8,7,6
data10:
        .short 65535,32768,32767,65534,40000,60000,50000,0
data11:
        .short 65535,32768,32767,65534,40000,60000,50000,32768
