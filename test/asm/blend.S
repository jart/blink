#include "test/asm/mac.inc"
.macro check op:req, control:req, source:req, dest:req, mask:req, expected:req
        .test "\op vector \@"
        movdqu left(%rip),%xmm8
        movdqu right(%rip),%xmm9
        movdqu \mask(%rip),%xmm0
        cmp %rsp,%rsp
        stc
.if \control < 0
        \op \source,\dest
.else
        \op $\control,\source,\dest
.endif
        .c
        .e
        .p
        .ns
        .no
        pcmpeqb \expected(%rip),\dest
        pmovmskb \dest,%eax
        cmp $65535,%eax
        .e
.ifnc \dest,%xmm0
        pcmpeqb \mask(%rip),%xmm0
        pmovmskb %xmm0,%eax
        cmp $65535,%eax
        .e
.endif
.endm
.globl _start
_start:
        check pblendvb,-1,%xmm9,%xmm8,mixed,expected0
        check pblendvb,-1,right(%rip),%xmm8,mixed,expected1
        check pblendvb,-1,%xmm9,%xmm8,zero,expected2
        check pblendvb,-1,right(%rip),%xmm8,zero,expected3
        check pblendvb,-1,%xmm9,%xmm8,ones,expected4
        check pblendvb,-1,right(%rip),%xmm8,ones,expected5
        check pblendvb,-1,%xmm9,%xmm0,mixed,expected6
        check pblendvb,-1,%xmm0,%xmm8,mixed,expected7
        check blendvps,-1,%xmm9,%xmm8,mixed,expected8
        check blendvps,-1,right(%rip),%xmm8,mixed,expected9
        check blendvps,-1,%xmm9,%xmm8,zero,expected10
        check blendvps,-1,right(%rip),%xmm8,zero,expected11
        check blendvps,-1,%xmm9,%xmm8,ones,expected12
        check blendvps,-1,right(%rip),%xmm8,ones,expected13
        check blendvps,-1,%xmm9,%xmm0,mixed,expected14
        check blendvps,-1,%xmm0,%xmm8,mixed,expected15
        check blendvpd,-1,%xmm9,%xmm8,mixed,expected16
        check blendvpd,-1,right(%rip),%xmm8,mixed,expected17
        check blendvpd,-1,%xmm9,%xmm8,zero,expected18
        check blendvpd,-1,right(%rip),%xmm8,zero,expected19
        check blendvpd,-1,%xmm9,%xmm8,ones,expected20
        check blendvpd,-1,right(%rip),%xmm8,ones,expected21
        check blendvpd,-1,%xmm9,%xmm0,mixed,expected22
        check blendvpd,-1,%xmm0,%xmm8,mixed,expected23
        check pblendw,165,%xmm9,%xmm8,mixed,expected24
        check pblendw,165,right(%rip),%xmm8,mixed,expected25
        check pblendw,0,%xmm9,%xmm8,mixed,expected26
        check pblendw,0,right(%rip),%xmm8,mixed,expected27
        check pblendw,255,%xmm9,%xmm8,mixed,expected28
        check pblendw,255,right(%rip),%xmm8,mixed,expected29
        check pblendw,90,%xmm9,%xmm8,mixed,expected30
        check pblendw,90,right(%rip),%xmm8,mixed,expected31
        check blendps,165,%xmm9,%xmm8,mixed,expected32
        check blendps,165,right(%rip),%xmm8,mixed,expected33
        check blendps,0,%xmm9,%xmm8,mixed,expected34
        check blendps,0,right(%rip),%xmm8,mixed,expected35
        check blendps,255,%xmm9,%xmm8,mixed,expected36
        check blendps,255,right(%rip),%xmm8,mixed,expected37
        check blendps,90,%xmm9,%xmm8,mixed,expected38
        check blendps,90,right(%rip),%xmm8,mixed,expected39
        check blendpd,165,%xmm9,%xmm8,mixed,expected40
        check blendpd,165,right(%rip),%xmm8,mixed,expected41
        check blendpd,0,%xmm9,%xmm8,mixed,expected42
        check blendpd,0,right(%rip),%xmm8,mixed,expected43
        check blendpd,255,%xmm9,%xmm8,mixed,expected44
        check blendpd,255,right(%rip),%xmm8,mixed,expected45
        check blendpd,90,%xmm9,%xmm8,mixed,expected46
        check blendpd,90,right(%rip),%xmm8,mixed,expected47
        .exit
.section .rodata
.balign 16
left:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
right:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
mixed:
        .byte 128,1,127,128,255,0,255,127,0,128,128,128,255,255,255,0
zero:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
ones:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
expected0:
        .byte 128,17,18,131,132,21,134,23,24,137,138,139,140,141,142,31
expected1:
        .byte 128,17,18,131,132,21,134,23,24,137,138,139,140,141,142,31
expected2:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected3:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected4:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected5:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected6:
        .byte 128,1,127,131,132,0,134,127,0,137,138,139,140,141,142,0
expected7:
        .byte 128,17,18,128,255,21,255,23,24,128,128,128,255,255,255,31
expected8:
        .byte 128,129,130,131,20,21,22,23,136,137,138,139,28,29,30,31
expected9:
        .byte 128,129,130,131,20,21,22,23,136,137,138,139,28,29,30,31
expected10:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected11:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected12:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected13:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected14:
        .byte 128,129,130,131,255,0,255,127,136,137,138,139,255,255,255,0
expected15:
        .byte 128,1,127,128,20,21,22,23,0,128,128,128,28,29,30,31
expected16:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected17:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected18:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected19:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected20:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected21:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected22:
        .byte 128,1,127,128,255,0,255,127,0,128,128,128,255,255,255,0
expected23:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected24:
        .byte 128,129,18,19,132,133,22,23,24,25,138,139,28,29,142,143
expected25:
        .byte 128,129,18,19,132,133,22,23,24,25,138,139,28,29,142,143
expected26:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected27:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected28:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected29:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected30:
        .byte 16,17,130,131,20,21,134,135,136,137,26,27,140,141,30,31
expected31:
        .byte 16,17,130,131,20,21,134,135,136,137,26,27,140,141,30,31
expected32:
        .byte 128,129,130,131,20,21,22,23,136,137,138,139,28,29,30,31
expected33:
        .byte 128,129,130,131,20,21,22,23,136,137,138,139,28,29,30,31
expected34:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected35:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected36:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected37:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected38:
        .byte 16,17,18,19,132,133,134,135,24,25,26,27,140,141,142,143
expected39:
        .byte 16,17,18,19,132,133,134,135,24,25,26,27,140,141,142,143
expected40:
        .byte 128,129,130,131,132,133,134,135,24,25,26,27,28,29,30,31
expected41:
        .byte 128,129,130,131,132,133,134,135,24,25,26,27,28,29,30,31
expected42:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected43:
        .byte 16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
expected44:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected45:
        .byte 128,129,130,131,132,133,134,135,136,137,138,139,140,141,142,143
expected46:
        .byte 16,17,18,19,20,21,22,23,136,137,138,139,140,141,142,143
expected47:
        .byte 16,17,18,19,20,21,22,23,136,137,138,139,140,141,142,143
