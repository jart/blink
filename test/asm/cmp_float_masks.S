#include "test/asm/mac.inc"
.macro check op:req, imm:req, left:req, right:req, expected:req
        movdqu \left(%rip),%xmm8
        movdqu \right(%rip),%xmm9
        cmp %rsp,%rsp
        stc
        \op $\imm,%xmm9,%xmm8
        .c
        .e
        .p
        .ns
        .no
        pcmpeqb \expected(%rip),%xmm8
        pmovmskb %xmm8,%eax
        cmp $65535,%eax
        .e
.endm
.globl _start
_start:
        .test "cmpps predicate 0 rotation 0"
        check cmpps,0,cmpps_0_left,cmpps_0_right,cmpps_0_0_expected
        .test "cmpps predicate 1 rotation 0"
        check cmpps,1,cmpps_0_left,cmpps_0_right,cmpps_0_1_expected
        .test "cmpps predicate 2 rotation 0"
        check cmpps,2,cmpps_0_left,cmpps_0_right,cmpps_0_2_expected
        .test "cmpps predicate 3 rotation 0"
        check cmpps,3,cmpps_0_left,cmpps_0_right,cmpps_0_3_expected
        .test "cmpps predicate 4 rotation 0"
        check cmpps,4,cmpps_0_left,cmpps_0_right,cmpps_0_4_expected
        .test "cmpps predicate 5 rotation 0"
        check cmpps,5,cmpps_0_left,cmpps_0_right,cmpps_0_5_expected
        .test "cmpps predicate 6 rotation 0"
        check cmpps,6,cmpps_0_left,cmpps_0_right,cmpps_0_6_expected
        .test "cmpps predicate 7 rotation 0"
        check cmpps,7,cmpps_0_left,cmpps_0_right,cmpps_0_7_expected
        .test "cmpps predicate 0 rotation 1"
        check cmpps,0,cmpps_1_left,cmpps_1_right,cmpps_1_0_expected
        .test "cmpps predicate 1 rotation 1"
        check cmpps,1,cmpps_1_left,cmpps_1_right,cmpps_1_1_expected
        .test "cmpps predicate 2 rotation 1"
        check cmpps,2,cmpps_1_left,cmpps_1_right,cmpps_1_2_expected
        .test "cmpps predicate 3 rotation 1"
        check cmpps,3,cmpps_1_left,cmpps_1_right,cmpps_1_3_expected
        .test "cmpps predicate 4 rotation 1"
        check cmpps,4,cmpps_1_left,cmpps_1_right,cmpps_1_4_expected
        .test "cmpps predicate 5 rotation 1"
        check cmpps,5,cmpps_1_left,cmpps_1_right,cmpps_1_5_expected
        .test "cmpps predicate 6 rotation 1"
        check cmpps,6,cmpps_1_left,cmpps_1_right,cmpps_1_6_expected
        .test "cmpps predicate 7 rotation 1"
        check cmpps,7,cmpps_1_left,cmpps_1_right,cmpps_1_7_expected
        .test "cmpps predicate 0 rotation 2"
        check cmpps,0,cmpps_2_left,cmpps_2_right,cmpps_2_0_expected
        .test "cmpps predicate 1 rotation 2"
        check cmpps,1,cmpps_2_left,cmpps_2_right,cmpps_2_1_expected
        .test "cmpps predicate 2 rotation 2"
        check cmpps,2,cmpps_2_left,cmpps_2_right,cmpps_2_2_expected
        .test "cmpps predicate 3 rotation 2"
        check cmpps,3,cmpps_2_left,cmpps_2_right,cmpps_2_3_expected
        .test "cmpps predicate 4 rotation 2"
        check cmpps,4,cmpps_2_left,cmpps_2_right,cmpps_2_4_expected
        .test "cmpps predicate 5 rotation 2"
        check cmpps,5,cmpps_2_left,cmpps_2_right,cmpps_2_5_expected
        .test "cmpps predicate 6 rotation 2"
        check cmpps,6,cmpps_2_left,cmpps_2_right,cmpps_2_6_expected
        .test "cmpps predicate 7 rotation 2"
        check cmpps,7,cmpps_2_left,cmpps_2_right,cmpps_2_7_expected
        .test "cmpps predicate 0 rotation 3"
        check cmpps,0,cmpps_3_left,cmpps_3_right,cmpps_3_0_expected
        .test "cmpps predicate 1 rotation 3"
        check cmpps,1,cmpps_3_left,cmpps_3_right,cmpps_3_1_expected
        .test "cmpps predicate 2 rotation 3"
        check cmpps,2,cmpps_3_left,cmpps_3_right,cmpps_3_2_expected
        .test "cmpps predicate 3 rotation 3"
        check cmpps,3,cmpps_3_left,cmpps_3_right,cmpps_3_3_expected
        .test "cmpps predicate 4 rotation 3"
        check cmpps,4,cmpps_3_left,cmpps_3_right,cmpps_3_4_expected
        .test "cmpps predicate 5 rotation 3"
        check cmpps,5,cmpps_3_left,cmpps_3_right,cmpps_3_5_expected
        .test "cmpps predicate 6 rotation 3"
        check cmpps,6,cmpps_3_left,cmpps_3_right,cmpps_3_6_expected
        .test "cmpps predicate 7 rotation 3"
        check cmpps,7,cmpps_3_left,cmpps_3_right,cmpps_3_7_expected
        .test "cmppd predicate 0 rotation 0"
        check cmppd,0,cmppd_0_left,cmppd_0_right,cmppd_0_0_expected
        .test "cmppd predicate 1 rotation 0"
        check cmppd,1,cmppd_0_left,cmppd_0_right,cmppd_0_1_expected
        .test "cmppd predicate 2 rotation 0"
        check cmppd,2,cmppd_0_left,cmppd_0_right,cmppd_0_2_expected
        .test "cmppd predicate 3 rotation 0"
        check cmppd,3,cmppd_0_left,cmppd_0_right,cmppd_0_3_expected
        .test "cmppd predicate 4 rotation 0"
        check cmppd,4,cmppd_0_left,cmppd_0_right,cmppd_0_4_expected
        .test "cmppd predicate 5 rotation 0"
        check cmppd,5,cmppd_0_left,cmppd_0_right,cmppd_0_5_expected
        .test "cmppd predicate 6 rotation 0"
        check cmppd,6,cmppd_0_left,cmppd_0_right,cmppd_0_6_expected
        .test "cmppd predicate 7 rotation 0"
        check cmppd,7,cmppd_0_left,cmppd_0_right,cmppd_0_7_expected
        .test "cmppd predicate 0 rotation 1"
        check cmppd,0,cmppd_1_left,cmppd_1_right,cmppd_1_0_expected
        .test "cmppd predicate 1 rotation 1"
        check cmppd,1,cmppd_1_left,cmppd_1_right,cmppd_1_1_expected
        .test "cmppd predicate 2 rotation 1"
        check cmppd,2,cmppd_1_left,cmppd_1_right,cmppd_1_2_expected
        .test "cmppd predicate 3 rotation 1"
        check cmppd,3,cmppd_1_left,cmppd_1_right,cmppd_1_3_expected
        .test "cmppd predicate 4 rotation 1"
        check cmppd,4,cmppd_1_left,cmppd_1_right,cmppd_1_4_expected
        .test "cmppd predicate 5 rotation 1"
        check cmppd,5,cmppd_1_left,cmppd_1_right,cmppd_1_5_expected
        .test "cmppd predicate 6 rotation 1"
        check cmppd,6,cmppd_1_left,cmppd_1_right,cmppd_1_6_expected
        .test "cmppd predicate 7 rotation 1"
        check cmppd,7,cmppd_1_left,cmppd_1_right,cmppd_1_7_expected
        .test "cmppd predicate 0 rotation 2"
        check cmppd,0,cmppd_2_left,cmppd_2_right,cmppd_2_0_expected
        .test "cmppd predicate 1 rotation 2"
        check cmppd,1,cmppd_2_left,cmppd_2_right,cmppd_2_1_expected
        .test "cmppd predicate 2 rotation 2"
        check cmppd,2,cmppd_2_left,cmppd_2_right,cmppd_2_2_expected
        .test "cmppd predicate 3 rotation 2"
        check cmppd,3,cmppd_2_left,cmppd_2_right,cmppd_2_3_expected
        .test "cmppd predicate 4 rotation 2"
        check cmppd,4,cmppd_2_left,cmppd_2_right,cmppd_2_4_expected
        .test "cmppd predicate 5 rotation 2"
        check cmppd,5,cmppd_2_left,cmppd_2_right,cmppd_2_5_expected
        .test "cmppd predicate 6 rotation 2"
        check cmppd,6,cmppd_2_left,cmppd_2_right,cmppd_2_6_expected
        .test "cmppd predicate 7 rotation 2"
        check cmppd,7,cmppd_2_left,cmppd_2_right,cmppd_2_7_expected
        .test "cmppd predicate 0 rotation 3"
        check cmppd,0,cmppd_3_left,cmppd_3_right,cmppd_3_0_expected
        .test "cmppd predicate 1 rotation 3"
        check cmppd,1,cmppd_3_left,cmppd_3_right,cmppd_3_1_expected
        .test "cmppd predicate 2 rotation 3"
        check cmppd,2,cmppd_3_left,cmppd_3_right,cmppd_3_2_expected
        .test "cmppd predicate 3 rotation 3"
        check cmppd,3,cmppd_3_left,cmppd_3_right,cmppd_3_3_expected
        .test "cmppd predicate 4 rotation 3"
        check cmppd,4,cmppd_3_left,cmppd_3_right,cmppd_3_4_expected
        .test "cmppd predicate 5 rotation 3"
        check cmppd,5,cmppd_3_left,cmppd_3_right,cmppd_3_5_expected
        .test "cmppd predicate 6 rotation 3"
        check cmppd,6,cmppd_3_left,cmppd_3_right,cmppd_3_6_expected
        .test "cmppd predicate 7 rotation 3"
        check cmppd,7,cmppd_3_left,cmppd_3_right,cmppd_3_7_expected
        .test "cmpss predicate 0 rotation 0"
        check cmpss,0,cmpss_0_left,cmpss_0_right,cmpss_0_0_expected
        .test "cmpss predicate 1 rotation 0"
        check cmpss,1,cmpss_0_left,cmpss_0_right,cmpss_0_1_expected
        .test "cmpss predicate 2 rotation 0"
        check cmpss,2,cmpss_0_left,cmpss_0_right,cmpss_0_2_expected
        .test "cmpss predicate 3 rotation 0"
        check cmpss,3,cmpss_0_left,cmpss_0_right,cmpss_0_3_expected
        .test "cmpss predicate 4 rotation 0"
        check cmpss,4,cmpss_0_left,cmpss_0_right,cmpss_0_4_expected
        .test "cmpss predicate 5 rotation 0"
        check cmpss,5,cmpss_0_left,cmpss_0_right,cmpss_0_5_expected
        .test "cmpss predicate 6 rotation 0"
        check cmpss,6,cmpss_0_left,cmpss_0_right,cmpss_0_6_expected
        .test "cmpss predicate 7 rotation 0"
        check cmpss,7,cmpss_0_left,cmpss_0_right,cmpss_0_7_expected
        .test "cmpss predicate 0 rotation 1"
        check cmpss,0,cmpss_1_left,cmpss_1_right,cmpss_1_0_expected
        .test "cmpss predicate 1 rotation 1"
        check cmpss,1,cmpss_1_left,cmpss_1_right,cmpss_1_1_expected
        .test "cmpss predicate 2 rotation 1"
        check cmpss,2,cmpss_1_left,cmpss_1_right,cmpss_1_2_expected
        .test "cmpss predicate 3 rotation 1"
        check cmpss,3,cmpss_1_left,cmpss_1_right,cmpss_1_3_expected
        .test "cmpss predicate 4 rotation 1"
        check cmpss,4,cmpss_1_left,cmpss_1_right,cmpss_1_4_expected
        .test "cmpss predicate 5 rotation 1"
        check cmpss,5,cmpss_1_left,cmpss_1_right,cmpss_1_5_expected
        .test "cmpss predicate 6 rotation 1"
        check cmpss,6,cmpss_1_left,cmpss_1_right,cmpss_1_6_expected
        .test "cmpss predicate 7 rotation 1"
        check cmpss,7,cmpss_1_left,cmpss_1_right,cmpss_1_7_expected
        .test "cmpss predicate 0 rotation 2"
        check cmpss,0,cmpss_2_left,cmpss_2_right,cmpss_2_0_expected
        .test "cmpss predicate 1 rotation 2"
        check cmpss,1,cmpss_2_left,cmpss_2_right,cmpss_2_1_expected
        .test "cmpss predicate 2 rotation 2"
        check cmpss,2,cmpss_2_left,cmpss_2_right,cmpss_2_2_expected
        .test "cmpss predicate 3 rotation 2"
        check cmpss,3,cmpss_2_left,cmpss_2_right,cmpss_2_3_expected
        .test "cmpss predicate 4 rotation 2"
        check cmpss,4,cmpss_2_left,cmpss_2_right,cmpss_2_4_expected
        .test "cmpss predicate 5 rotation 2"
        check cmpss,5,cmpss_2_left,cmpss_2_right,cmpss_2_5_expected
        .test "cmpss predicate 6 rotation 2"
        check cmpss,6,cmpss_2_left,cmpss_2_right,cmpss_2_6_expected
        .test "cmpss predicate 7 rotation 2"
        check cmpss,7,cmpss_2_left,cmpss_2_right,cmpss_2_7_expected
        .test "cmpss predicate 0 rotation 3"
        check cmpss,0,cmpss_3_left,cmpss_3_right,cmpss_3_0_expected
        .test "cmpss predicate 1 rotation 3"
        check cmpss,1,cmpss_3_left,cmpss_3_right,cmpss_3_1_expected
        .test "cmpss predicate 2 rotation 3"
        check cmpss,2,cmpss_3_left,cmpss_3_right,cmpss_3_2_expected
        .test "cmpss predicate 3 rotation 3"
        check cmpss,3,cmpss_3_left,cmpss_3_right,cmpss_3_3_expected
        .test "cmpss predicate 4 rotation 3"
        check cmpss,4,cmpss_3_left,cmpss_3_right,cmpss_3_4_expected
        .test "cmpss predicate 5 rotation 3"
        check cmpss,5,cmpss_3_left,cmpss_3_right,cmpss_3_5_expected
        .test "cmpss predicate 6 rotation 3"
        check cmpss,6,cmpss_3_left,cmpss_3_right,cmpss_3_6_expected
        .test "cmpss predicate 7 rotation 3"
        check cmpss,7,cmpss_3_left,cmpss_3_right,cmpss_3_7_expected
        .test "cmpsd predicate 0 rotation 0"
        check cmpsd,0,cmpsd_0_left,cmpsd_0_right,cmpsd_0_0_expected
        .test "cmpsd predicate 1 rotation 0"
        check cmpsd,1,cmpsd_0_left,cmpsd_0_right,cmpsd_0_1_expected
        .test "cmpsd predicate 2 rotation 0"
        check cmpsd,2,cmpsd_0_left,cmpsd_0_right,cmpsd_0_2_expected
        .test "cmpsd predicate 3 rotation 0"
        check cmpsd,3,cmpsd_0_left,cmpsd_0_right,cmpsd_0_3_expected
        .test "cmpsd predicate 4 rotation 0"
        check cmpsd,4,cmpsd_0_left,cmpsd_0_right,cmpsd_0_4_expected
        .test "cmpsd predicate 5 rotation 0"
        check cmpsd,5,cmpsd_0_left,cmpsd_0_right,cmpsd_0_5_expected
        .test "cmpsd predicate 6 rotation 0"
        check cmpsd,6,cmpsd_0_left,cmpsd_0_right,cmpsd_0_6_expected
        .test "cmpsd predicate 7 rotation 0"
        check cmpsd,7,cmpsd_0_left,cmpsd_0_right,cmpsd_0_7_expected
        .test "cmpsd predicate 0 rotation 1"
        check cmpsd,0,cmpsd_1_left,cmpsd_1_right,cmpsd_1_0_expected
        .test "cmpsd predicate 1 rotation 1"
        check cmpsd,1,cmpsd_1_left,cmpsd_1_right,cmpsd_1_1_expected
        .test "cmpsd predicate 2 rotation 1"
        check cmpsd,2,cmpsd_1_left,cmpsd_1_right,cmpsd_1_2_expected
        .test "cmpsd predicate 3 rotation 1"
        check cmpsd,3,cmpsd_1_left,cmpsd_1_right,cmpsd_1_3_expected
        .test "cmpsd predicate 4 rotation 1"
        check cmpsd,4,cmpsd_1_left,cmpsd_1_right,cmpsd_1_4_expected
        .test "cmpsd predicate 5 rotation 1"
        check cmpsd,5,cmpsd_1_left,cmpsd_1_right,cmpsd_1_5_expected
        .test "cmpsd predicate 6 rotation 1"
        check cmpsd,6,cmpsd_1_left,cmpsd_1_right,cmpsd_1_6_expected
        .test "cmpsd predicate 7 rotation 1"
        check cmpsd,7,cmpsd_1_left,cmpsd_1_right,cmpsd_1_7_expected
        .test "cmpsd predicate 0 rotation 2"
        check cmpsd,0,cmpsd_2_left,cmpsd_2_right,cmpsd_2_0_expected
        .test "cmpsd predicate 1 rotation 2"
        check cmpsd,1,cmpsd_2_left,cmpsd_2_right,cmpsd_2_1_expected
        .test "cmpsd predicate 2 rotation 2"
        check cmpsd,2,cmpsd_2_left,cmpsd_2_right,cmpsd_2_2_expected
        .test "cmpsd predicate 3 rotation 2"
        check cmpsd,3,cmpsd_2_left,cmpsd_2_right,cmpsd_2_3_expected
        .test "cmpsd predicate 4 rotation 2"
        check cmpsd,4,cmpsd_2_left,cmpsd_2_right,cmpsd_2_4_expected
        .test "cmpsd predicate 5 rotation 2"
        check cmpsd,5,cmpsd_2_left,cmpsd_2_right,cmpsd_2_5_expected
        .test "cmpsd predicate 6 rotation 2"
        check cmpsd,6,cmpsd_2_left,cmpsd_2_right,cmpsd_2_6_expected
        .test "cmpsd predicate 7 rotation 2"
        check cmpsd,7,cmpsd_2_left,cmpsd_2_right,cmpsd_2_7_expected
        .test "cmpsd predicate 0 rotation 3"
        check cmpsd,0,cmpsd_3_left,cmpsd_3_right,cmpsd_3_0_expected
        .test "cmpsd predicate 1 rotation 3"
        check cmpsd,1,cmpsd_3_left,cmpsd_3_right,cmpsd_3_1_expected
        .test "cmpsd predicate 2 rotation 3"
        check cmpsd,2,cmpsd_3_left,cmpsd_3_right,cmpsd_3_2_expected
        .test "cmpsd predicate 3 rotation 3"
        check cmpsd,3,cmpsd_3_left,cmpsd_3_right,cmpsd_3_3_expected
        .test "cmpsd predicate 4 rotation 3"
        check cmpsd,4,cmpsd_3_left,cmpsd_3_right,cmpsd_3_4_expected
        .test "cmpsd predicate 5 rotation 3"
        check cmpsd,5,cmpsd_3_left,cmpsd_3_right,cmpsd_3_5_expected
        .test "cmpsd predicate 6 rotation 3"
        check cmpsd,6,cmpsd_3_left,cmpsd_3_right,cmpsd_3_6_expected
        .test "cmpsd predicate 7 rotation 3"
        check cmpsd,7,cmpsd_3_left,cmpsd_3_right,cmpsd_3_7_expected
        .exit
.section .rodata
.balign 16
cmpps_0_left:
        .byte 0,0,128,63,0,0,0,128,0,0,192,127,0,0,64,64
cmpps_0_right:
        .byte 0,0,0,64,0,0,0,0,0,0,64,64,0,0,0,64
cmpps_0_0_expected:
        .byte 0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0
cmpps_0_1_expected:
        .byte 255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0
cmpps_0_2_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmpps_0_3_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0
cmpps_0_4_expected:
        .byte 255,255,255,255,0,0,0,0,255,255,255,255,255,255,255,255
cmpps_0_5_expected:
        .byte 0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255
cmpps_0_6_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmpps_0_7_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255
cmpps_1_left:
        .byte 0,0,0,128,0,0,192,127,0,0,64,64,0,0,128,63
cmpps_1_right:
        .byte 0,0,0,0,0,0,64,64,0,0,0,64,0,0,0,64
cmpps_1_0_expected:
        .byte 255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0
cmpps_1_1_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255
cmpps_1_2_expected:
        .byte 255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255
cmpps_1_3_expected:
        .byte 0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0
cmpps_1_4_expected:
        .byte 0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255
cmpps_1_5_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0
cmpps_1_6_expected:
        .byte 0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0
cmpps_1_7_expected:
        .byte 255,255,255,255,0,0,0,0,255,255,255,255,255,255,255,255
cmpps_2_left:
        .byte 0,0,192,127,0,0,64,64,0,0,128,63,0,0,0,128
cmpps_2_right:
        .byte 0,0,64,64,0,0,0,64,0,0,0,64,0,0,0,0
cmpps_2_0_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255
cmpps_2_1_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0
cmpps_2_2_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmpps_2_3_expected:
        .byte 255,255,255,255,0,0,0,0,0,0,0,0,0,0,0,0
cmpps_2_4_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0
cmpps_2_5_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255
cmpps_2_6_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmpps_2_7_expected:
        .byte 0,0,0,0,255,255,255,255,255,255,255,255,255,255,255,255
cmpps_3_left:
        .byte 0,0,64,64,0,0,128,63,0,0,0,128,0,0,192,127
cmpps_3_right:
        .byte 0,0,0,64,0,0,0,64,0,0,0,0,0,0,64,64
cmpps_3_0_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,0,0,0,0
cmpps_3_1_expected:
        .byte 0,0,0,0,255,255,255,255,0,0,0,0,0,0,0,0
cmpps_3_2_expected:
        .byte 0,0,0,0,255,255,255,255,255,255,255,255,0,0,0,0
cmpps_3_3_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,255,255,255,255
cmpps_3_4_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,255,255,255,255
cmpps_3_5_expected:
        .byte 255,255,255,255,0,0,0,0,255,255,255,255,255,255,255,255
cmpps_3_6_expected:
        .byte 255,255,255,255,0,0,0,0,0,0,0,0,255,255,255,255
cmpps_3_7_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,0,0,0,0
cmppd_0_left:
        .byte 0,0,0,0,0,0,240,63,0,0,0,0,0,0,0,128
cmppd_0_right:
        .byte 0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,0
cmppd_0_0_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmppd_0_1_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmppd_0_2_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
cmppd_0_3_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cmppd_0_4_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmppd_0_5_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmppd_0_6_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cmppd_0_7_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
cmppd_1_left:
        .byte 0,0,0,0,0,0,0,128,0,0,0,0,0,0,248,127
cmppd_1_right:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,64
cmppd_1_0_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmppd_1_1_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cmppd_1_2_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmppd_1_3_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmppd_1_4_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmppd_1_5_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
cmppd_1_6_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmppd_1_7_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmppd_2_left:
        .byte 0,0,0,0,0,0,248,127,0,0,0,0,0,0,8,64
cmppd_2_right:
        .byte 0,0,0,0,0,0,8,64,0,0,0,0,0,0,0,64
cmppd_2_0_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cmppd_2_1_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cmppd_2_2_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cmppd_2_3_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmppd_2_4_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
cmppd_2_5_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
cmppd_2_6_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
cmppd_2_7_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmppd_3_left:
        .byte 0,0,0,0,0,0,8,64,0,0,0,0,0,0,240,63
cmppd_3_right:
        .byte 0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,64
cmppd_3_0_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cmppd_3_1_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmppd_3_2_expected:
        .byte 0,0,0,0,0,0,0,0,255,255,255,255,255,255,255,255
cmppd_3_3_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cmppd_3_4_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
cmppd_3_5_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmppd_3_6_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,0
cmppd_3_7_expected:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
cmpss_0_left:
        .byte 0,0,128,63,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_0_right:
        .byte 0,0,0,64,0,0,0,0,0,0,64,64,0,0,0,64
cmpss_0_0_expected:
        .byte 0,0,0,0,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_0_1_expected:
        .byte 255,255,255,255,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_0_2_expected:
        .byte 255,255,255,255,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_0_3_expected:
        .byte 0,0,0,0,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_0_4_expected:
        .byte 255,255,255,255,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_0_5_expected:
        .byte 0,0,0,0,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_0_6_expected:
        .byte 0,0,0,0,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_0_7_expected:
        .byte 255,255,255,255,0,0,0,128,0,0,192,127,0,0,64,64
cmpss_1_left:
        .byte 0,0,0,128,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_1_right:
        .byte 0,0,0,0,0,0,64,64,0,0,0,64,0,0,0,64
cmpss_1_0_expected:
        .byte 255,255,255,255,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_1_1_expected:
        .byte 0,0,0,0,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_1_2_expected:
        .byte 255,255,255,255,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_1_3_expected:
        .byte 0,0,0,0,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_1_4_expected:
        .byte 0,0,0,0,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_1_5_expected:
        .byte 255,255,255,255,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_1_6_expected:
        .byte 0,0,0,0,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_1_7_expected:
        .byte 255,255,255,255,0,0,192,127,0,0,64,64,0,0,128,63
cmpss_2_left:
        .byte 0,0,192,127,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_2_right:
        .byte 0,0,64,64,0,0,0,64,0,0,0,64,0,0,0,0
cmpss_2_0_expected:
        .byte 0,0,0,0,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_2_1_expected:
        .byte 0,0,0,0,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_2_2_expected:
        .byte 0,0,0,0,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_2_3_expected:
        .byte 255,255,255,255,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_2_4_expected:
        .byte 255,255,255,255,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_2_5_expected:
        .byte 255,255,255,255,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_2_6_expected:
        .byte 255,255,255,255,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_2_7_expected:
        .byte 0,0,0,0,0,0,64,64,0,0,128,63,0,0,0,128
cmpss_3_left:
        .byte 0,0,64,64,0,0,128,63,0,0,0,128,0,0,192,127
cmpss_3_right:
        .byte 0,0,0,64,0,0,0,64,0,0,0,0,0,0,64,64
cmpss_3_0_expected:
        .byte 0,0,0,0,0,0,128,63,0,0,0,128,0,0,192,127
cmpss_3_1_expected:
        .byte 0,0,0,0,0,0,128,63,0,0,0,128,0,0,192,127
cmpss_3_2_expected:
        .byte 0,0,0,0,0,0,128,63,0,0,0,128,0,0,192,127
cmpss_3_3_expected:
        .byte 0,0,0,0,0,0,128,63,0,0,0,128,0,0,192,127
cmpss_3_4_expected:
        .byte 255,255,255,255,0,0,128,63,0,0,0,128,0,0,192,127
cmpss_3_5_expected:
        .byte 255,255,255,255,0,0,128,63,0,0,0,128,0,0,192,127
cmpss_3_6_expected:
        .byte 255,255,255,255,0,0,128,63,0,0,0,128,0,0,192,127
cmpss_3_7_expected:
        .byte 255,255,255,255,0,0,128,63,0,0,0,128,0,0,192,127
cmpsd_0_left:
        .byte 0,0,0,0,0,0,240,63,0,0,0,0,0,0,0,128
cmpsd_0_right:
        .byte 0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,0
cmpsd_0_0_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128
cmpsd_0_1_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,128
cmpsd_0_2_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,128
cmpsd_0_3_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128
cmpsd_0_4_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,128
cmpsd_0_5_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128
cmpsd_0_6_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128
cmpsd_0_7_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,0,128
cmpsd_1_left:
        .byte 0,0,0,0,0,0,0,128,0,0,0,0,0,0,248,127
cmpsd_1_right:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,64
cmpsd_1_0_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,248,127
cmpsd_1_1_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,127
cmpsd_1_2_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,248,127
cmpsd_1_3_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,127
cmpsd_1_4_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,127
cmpsd_1_5_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,248,127
cmpsd_1_6_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,248,127
cmpsd_1_7_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,248,127
cmpsd_2_left:
        .byte 0,0,0,0,0,0,248,127,0,0,0,0,0,0,8,64
cmpsd_2_right:
        .byte 0,0,0,0,0,0,8,64,0,0,0,0,0,0,0,64
cmpsd_2_0_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,64
cmpsd_2_1_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,64
cmpsd_2_2_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,64
cmpsd_2_3_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,8,64
cmpsd_2_4_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,8,64
cmpsd_2_5_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,8,64
cmpsd_2_6_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,8,64
cmpsd_2_7_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,8,64
cmpsd_3_left:
        .byte 0,0,0,0,0,0,8,64,0,0,0,0,0,0,240,63
cmpsd_3_right:
        .byte 0,0,0,0,0,0,0,64,0,0,0,0,0,0,0,64
cmpsd_3_0_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,63
cmpsd_3_1_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,63
cmpsd_3_2_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,63
cmpsd_3_3_expected:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,63
cmpsd_3_4_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,240,63
cmpsd_3_5_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,240,63
cmpsd_3_6_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,240,63
cmpsd_3_7_expected:
        .byte 255,255,255,255,255,255,255,255,0,0,0,0,0,0,240,63
