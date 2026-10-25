#include "test/asm/mac.inc"
.macro check op:req, left:req, right:req, expected:req, memory=0, alias=0
        .test "\op vector \@"
        movdqu \left(%rip),%xmm8
        movdqu \right(%rip),%xmm9
        cmp %rsp,%rsp
        stc
.if \alias
        \op %xmm8,%xmm8
.elseif \memory
        \op \right(%rip),%xmm8
.else
        \op %xmm9,%xmm8
.endif
        .c
        .e
        .p
        .ns
        .no
        pcmpeqb \expected(%rip),%xmm8
        pmovmskb %xmm8,%eax
        cmp $65535,%eax
        .e
        pcmpeqb \right(%rip),%xmm9
        pmovmskb %xmm9,%eax
        cmp $65535,%eax
        .e
.endm
.globl _start
_start:
        check pminsb,pminsb_left,pminsb_right,pminsb_expected
        check pminsb,pminsb_left,pminsb_right,pminsb_expected,1
        check pminsb,pminsb_right,pminsb_left,pminsb_expected
        check pminsb,pminsb_left,pminsb_right,pminsb_left,0,1
        check pmaxsb,pmaxsb_left,pmaxsb_right,pmaxsb_expected
        check pmaxsb,pmaxsb_left,pmaxsb_right,pmaxsb_expected,1
        check pmaxsb,pmaxsb_right,pmaxsb_left,pmaxsb_expected
        check pmaxsb,pmaxsb_left,pmaxsb_right,pmaxsb_left,0,1
        check pminsd,pminsd_left,pminsd_right,pminsd_expected
        check pminsd,pminsd_left,pminsd_right,pminsd_expected,1
        check pminsd,pminsd_right,pminsd_left,pminsd_expected
        check pminsd,pminsd_left,pminsd_right,pminsd_left,0,1
        check pmaxsd,pmaxsd_left,pmaxsd_right,pmaxsd_expected
        check pmaxsd,pmaxsd_left,pmaxsd_right,pmaxsd_expected,1
        check pmaxsd,pmaxsd_right,pmaxsd_left,pmaxsd_expected
        check pmaxsd,pmaxsd_left,pmaxsd_right,pmaxsd_left,0,1
        check pminuw,pminuw_left,pminuw_right,pminuw_expected
        check pminuw,pminuw_left,pminuw_right,pminuw_expected,1
        check pminuw,pminuw_right,pminuw_left,pminuw_expected
        check pminuw,pminuw_left,pminuw_right,pminuw_left,0,1
        check pmaxuw,pmaxuw_left,pmaxuw_right,pmaxuw_expected
        check pmaxuw,pmaxuw_left,pmaxuw_right,pmaxuw_expected,1
        check pmaxuw,pmaxuw_right,pmaxuw_left,pmaxuw_expected
        check pmaxuw,pmaxuw_left,pmaxuw_right,pmaxuw_left,0,1
        check pminud,pminud_left,pminud_right,pminud_expected
        check pminud,pminud_left,pminud_right,pminud_expected,1
        check pminud,pminud_right,pminud_left,pminud_expected
        check pminud,pminud_left,pminud_right,pminud_left,0,1
        check pmaxud,pmaxud_left,pmaxud_right,pmaxud_expected
        check pmaxud,pmaxud_left,pmaxud_right,pmaxud_expected,1
        check pmaxud,pmaxud_right,pmaxud_left,pmaxud_expected
        check pmaxud,pmaxud_left,pmaxud_right,pmaxud_left,0,1
        .exit
.section .rodata
.balign 16
pminsb_left:
        .byte 128,127,255,0,1,5,129,7,9,126,11,13,15,17,19,21
pminsb_right:
        .byte 127,128,0,255,2,5,130,6,10,125,12,13,14,18,19,20
pminsb_expected:
        .byte 128,128,255,255,1,5,129,6,9,125,11,13,14,17,19,20
pmaxsb_left:
        .byte 128,127,255,0,1,5,129,7,9,126,11,13,15,17,19,21
pmaxsb_right:
        .byte 127,128,0,255,2,5,130,6,10,125,12,13,14,18,19,20
pmaxsb_expected:
        .byte 127,127,0,0,2,5,130,7,10,126,12,13,15,18,19,21
pminsd_left:
        .byte 0,0,0,128,255,255,255,127,255,255,255,255,0,0,0,0
pminsd_right:
        .byte 255,255,255,127,0,0,0,128,0,0,0,0,255,255,255,255
pminsd_expected:
        .byte 0,0,0,128,0,0,0,128,255,255,255,255,255,255,255,255
pmaxsd_left:
        .byte 0,0,0,128,255,255,255,127,255,255,255,255,0,0,0,0
pmaxsd_right:
        .byte 255,255,255,127,0,0,0,128,0,0,0,0,255,255,255,255
pmaxsd_expected:
        .byte 255,255,255,127,255,255,255,127,0,0,0,0,0,0,0,0
pminuw_left:
        .byte 0,128,255,127,255,255,0,0,1,0,5,0,1,128,7,0
pminuw_right:
        .byte 255,127,0,128,0,0,255,255,2,0,5,0,2,128,6,0
pminuw_expected:
        .byte 255,127,255,127,0,0,0,0,1,0,5,0,1,128,6,0
pmaxuw_left:
        .byte 0,128,255,127,255,255,0,0,1,0,5,0,1,128,7,0
pmaxuw_right:
        .byte 255,127,0,128,0,0,255,255,2,0,5,0,2,128,6,0
pmaxuw_expected:
        .byte 0,128,0,128,255,255,255,255,2,0,5,0,2,128,7,0
pminud_left:
        .byte 0,0,0,128,255,255,255,127,255,255,255,255,0,0,0,0
pminud_right:
        .byte 255,255,255,127,0,0,0,128,0,0,0,0,255,255,255,255
pminud_expected:
        .byte 255,255,255,127,255,255,255,127,0,0,0,0,0,0,0,0
pmaxud_left:
        .byte 0,0,0,128,255,255,255,127,255,255,255,255,0,0,0,0
pmaxud_right:
        .byte 255,255,255,127,0,0,0,128,0,0,0,0,255,255,255,255
pmaxud_expected:
        .byte 0,0,0,128,0,0,0,128,255,255,255,255,255,255,255,255
