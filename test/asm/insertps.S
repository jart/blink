#include "test/asm/mac.inc"
.globl _start
_start:
        sub $48,%rsp
        movl $0x100,16(%rsp)
        movl $0x101,20(%rsp)
        movl $0x102,24(%rsp)
        movl $0x103,28(%rsp)
.macro check imm
.irp form,0,1,2
        .test "insertps \imm \form"
        movl $0x200,0(%rsp)
        movl $0x201,4(%rsp)
        movl $0x202,8(%rsp)
        movl $0x203,12(%rsp)
        movdqu (%rsp),%xmm8
        movdqu 16(%rsp),%xmm9
        xor %eax,%eax
        stc
        pushfq
        pop %r10
.if \form == 0
        insertps $\imm,%xmm9,%xmm8
.elseif \form == 1
        insertps $\imm,16(%rsp),%xmm8
.else
        insertps $\imm,%xmm8,%xmm8
.endif
        pushfq
        pop %r11
        cmp %r10,%r11
        .e
        movdqu %xmm8,(%rsp)
.irp lane,0,1,2,3
.if (\imm & (1 << \lane))
        cmpl $0,(4*\lane)(%rsp)
.elseif \lane == ((\imm >> 4) & 3)
.if \form == 1
        cmpl $0x100,(4*\lane)(%rsp)
.elseif \form == 2
        cmpl $(0x200+(\imm >> 6)),(4*\lane)(%rsp)
.else
        cmpl $(0x100+(\imm >> 6)),(4*\lane)(%rsp)
.endif
.else
        cmpl $(0x200+\lane),(4*\lane)(%rsp)
.endif
        .e
.endr
        movdqu %xmm9,32(%rsp)
        mov 16(%rsp),%rax
        cmp %rax,32(%rsp)
        .e
        mov 24(%rsp),%rax
        cmp %rax,40(%rsp)
        .e
.endr
.endm
.irp imm,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95,96,97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112,113,114,115,116,117,118,119,120,121,122,123,124,125,126,127,128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143,144,145,146,147,148,149,150,151,152,153,154,155,156,157,158,159,160,161,162,163,164,165,166,167,168,169,170,171,172,173,174,175,176,177,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,196,197,198,199,200,201,202,203,204,205,206,207,208,209,210,211,212,213,214,215,216,217,218,219,220,221,222,223,224,225,226,227,228,229,230,231,232,233,234,235,236,237,238,239,240,241,242,243,244,245,246,247,248,249,250,251,252,253,254,255
        check \imm
.endr
        .test "insertps memory reads only four bytes"
        mov $9,%eax
        xor %edi,%edi
        mov $8192,%esi
        mov $3,%edx
        mov $0x22,%r10d
        mov $-1,%r8
        xor %r9d,%r9d
        syscall
        test %rax,%rax
        .ns
        mov %rax,%r12
        movl $0xffc12345,4092(%r12)
        lea 4096(%r12),%rdi
        mov $4096,%esi
        xor %edx,%edx
        mov $10,%eax
        syscall
        test %rax,%rax
        .z
        insertps $0xc0,4092(%r12),%xmm8
        movd %xmm8,%eax
        cmp $0xffc12345,%eax
        .e
        .exit
