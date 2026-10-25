#include "test/asm/mac.inc"
.macro check op:req, control:req, input:req, expected:req, csr:req, status:req
        .test "\op control \control case \@"
        movl $\csr,(%rsp)
        ldmxcsr (%rsp)
        movdqu poison(%rip),%xmm8
        movdqu \input(%rip),%xmm9
        cmp %rsp,%rsp
        stc
        \op $\control,%xmm9,%xmm8
        .c
        .e
        .p
        .ns
        .no
        stmxcsr (%rsp)
        cmpl $\status,(%rsp)
        .e
        pcmpeqb \expected(%rip),%xmm8
        pmovmskb %xmm8,%eax
        cmp $65535,%eax
        .e
.endm
.globl _start
_start:
        sub $16,%rsp
        check roundps,0,input0,expected0,8064,8096
        check roundps,4,input0,expected0,8064,8096
        check roundps,1,input0,expected2,8064,8096
        check roundps,4,input0,expected2,16256,16288
        check roundps,2,input0,expected4,8064,8096
        check roundps,4,input0,expected4,24448,24480
        check roundps,3,input0,expected6,8064,8096
        check roundps,4,input0,expected6,32640,32672
        check roundps,11,input0,expected6,8064,8064
        check roundps,0,input9,expected9,8064,8096
        check roundps,1,input9,expected10,8064,8096
        check roundps,2,input9,expected11,8064,8096
        check roundps,3,input9,expected12,8064,8096
        check roundps,11,input9,expected12,8064,8064
        check roundps,0,input14,expected14,8064,8096
        check roundps,1,input14,expected15,8064,8096
        check roundps,2,input14,expected16,8064,8096
        check roundps,3,input14,expected17,8064,8096
        check roundps,11,input14,expected17,8064,8064
        check roundps,0,input19,expected19,8064,8065
        check roundps,1,input19,expected19,8064,8065
        check roundps,2,input19,expected19,8064,8065
        check roundps,3,input19,expected19,8064,8065
        check roundps,11,input19,expected19,8064,8065
        check roundps,0,input24,expected24,8064,8096
        check roundps,0,input24,expected24,8128,8128
        check roundps,1,input24,expected26,8064,8096
        check roundps,1,input24,expected24,8128,8128
        check roundps,2,input24,expected28,8064,8096
        check roundps,2,input24,expected24,8128,8128
        check roundps,3,input24,expected24,8064,8096
        check roundps,11,input24,expected24,8064,8064
        check roundps,3,input24,expected24,8128,8128
        check roundpd,0,input33,expected33,8064,8096
        check roundpd,4,input33,expected33,8064,8096
        check roundpd,1,input33,expected35,8064,8096
        check roundpd,4,input33,expected35,16256,16288
        check roundpd,2,input33,expected37,8064,8096
        check roundpd,4,input33,expected37,24448,24480
        check roundpd,3,input33,expected35,8064,8096
        check roundpd,4,input33,expected35,32640,32672
        check roundpd,11,input33,expected35,8064,8064
        check roundpd,0,input42,expected42,8064,8096
        check roundpd,4,input42,expected42,8064,8096
        check roundpd,1,input42,expected44,8064,8096
        check roundpd,4,input42,expected44,16256,16288
        check roundpd,2,input42,expected46,8064,8096
        check roundpd,4,input42,expected46,24448,24480
        check roundpd,3,input42,expected46,8064,8096
        check roundpd,4,input42,expected46,32640,32672
        check roundpd,11,input42,expected46,8064,8064
        check roundpd,0,input51,expected51,8064,8096
        check roundpd,1,input51,expected52,8064,8096
        check roundpd,2,input51,expected53,8064,8096
        check roundpd,3,input51,expected52,8064,8096
        check roundpd,11,input51,expected52,8064,8064
        check roundpd,0,input56,expected56,8064,8096
        check roundpd,1,input56,expected57,8064,8096
        check roundpd,2,input56,expected58,8064,8096
        check roundpd,3,input56,expected58,8064,8096
        check roundpd,11,input56,expected58,8064,8064
        check roundpd,0,input61,expected61,8064,8096
        check roundpd,1,input61,expected62,8064,8096
        check roundpd,2,input61,expected63,8064,8096
        check roundpd,3,input61,expected62,8064,8096
        check roundpd,11,input61,expected62,8064,8064
        check roundpd,0,input66,expected66,8064,8096
        check roundpd,1,input66,expected67,8064,8096
        check roundpd,2,input66,expected68,8064,8096
        check roundpd,3,input66,expected68,8064,8096
        check roundpd,11,input66,expected68,8064,8064
        check roundpd,0,input71,input71,8064,8064
        check roundpd,1,input71,input71,8064,8064
        check roundpd,2,input71,input71,8064,8064
        check roundpd,3,input71,input71,8064,8064
        check roundpd,11,input71,input71,8064,8064
        check roundpd,0,input76,expected76,8064,8065
        check roundpd,1,input76,expected76,8064,8065
        check roundpd,2,input76,expected76,8064,8065
        check roundpd,3,input76,expected76,8064,8065
        check roundpd,11,input76,expected76,8064,8065
        check roundpd,0,input81,expected81,8064,8096
        check roundpd,0,input81,expected81,8128,8128
        check roundpd,1,input81,expected83,8064,8096
        check roundpd,1,input81,expected81,8128,8128
        check roundpd,2,input81,expected85,8064,8096
        check roundpd,2,input81,expected81,8128,8128
        check roundpd,3,input81,expected81,8064,8096
        check roundpd,11,input81,expected81,8064,8064
        check roundpd,3,input81,expected81,8128,8128
        check roundpd,0,input90,input90,8064,8064
        check roundpd,0,input90,input90,8128,8128
        check roundpd,1,input90,input90,8064,8064
        check roundpd,1,input90,input90,8128,8128
        check roundpd,2,input90,input90,8064,8064
        check roundpd,2,input90,input90,8128,8128
        check roundpd,3,input90,input90,8064,8064
        check roundpd,11,input90,input90,8064,8064
        check roundpd,3,input90,input90,8128,8128
        check roundss,0,input0,expected99,8064,8096
        check roundss,4,input0,expected99,8064,8096
        check roundss,1,input0,expected99,8064,8096
        check roundss,4,input0,expected99,16256,16288
        check roundss,2,input0,expected103,8064,8096
        check roundss,4,input0,expected103,24448,24480
        check roundss,3,input0,expected99,8064,8096
        check roundss,4,input0,expected99,32640,32672
        check roundss,11,input0,expected99,8064,8064
        check roundss,0,input108,expected108,8064,8096
        check roundss,4,input108,expected108,8064,8096
        check roundss,1,input108,expected103,8064,8096
        check roundss,4,input108,expected103,16256,16288
        check roundss,2,input108,expected108,8064,8096
        check roundss,4,input108,expected108,24448,24480
        check roundss,3,input108,expected103,8064,8096
        check roundss,4,input108,expected103,32640,32672
        check roundss,11,input108,expected103,8064,8064
        check roundss,0,input117,expected117,8064,8096
        check roundss,4,input117,expected117,8064,8096
        check roundss,1,input117,expected119,8064,8096
        check roundss,4,input117,expected119,16256,16288
        check roundss,2,input117,expected117,8064,8096
        check roundss,4,input117,expected117,24448,24480
        check roundss,3,input117,expected117,8064,8096
        check roundss,4,input117,expected117,32640,32672
        check roundss,11,input117,expected117,8064,8064
        check roundss,0,input126,expected126,8064,8096
        check roundss,4,input126,expected126,8064,8096
        check roundss,1,input126,expected126,8064,8096
        check roundss,4,input126,expected126,16256,16288
        check roundss,2,input126,expected119,8064,8096
        check roundss,4,input126,expected119,24448,24480
        check roundss,3,input126,expected119,8064,8096
        check roundss,4,input126,expected119,32640,32672
        check roundss,11,input126,expected119,8064,8064
        check roundss,0,input9,expected108,8064,8096
        check roundss,1,input9,expected108,8064,8096
        check roundss,2,input9,expected137,8064,8096
        check roundss,3,input9,expected108,8064,8096
        check roundss,11,input9,expected108,8064,8064
        check roundss,0,input140,expected140,8064,8096
        check roundss,1,input140,expected137,8064,8096
        check roundss,2,input140,expected140,8064,8096
        check roundss,3,input140,expected137,8064,8096
        check roundss,11,input140,expected137,8064,8064
        check roundss,0,input145,expected126,8064,8096
        check roundss,1,input145,expected146,8064,8096
        check roundss,2,input145,expected126,8064,8096
        check roundss,3,input145,expected126,8064,8096
        check roundss,11,input145,expected126,8064,8064
        check roundss,0,input150,expected150,8064,8096
        check roundss,1,input150,expected150,8064,8096
        check roundss,2,input150,expected146,8064,8096
        check roundss,3,input150,expected146,8064,8096
        check roundss,11,input150,expected146,8064,8064
        check roundss,0,input14,expected103,8064,8096
        check roundss,1,input14,expected99,8064,8096
        check roundss,2,input14,expected103,8064,8096
        check roundss,3,input14,expected99,8064,8096
        check roundss,11,input14,expected99,8064,8064
        check roundss,0,input160,expected99,8064,8096
        check roundss,1,input160,expected99,8064,8096
        check roundss,2,input160,expected103,8064,8096
        check roundss,3,input160,expected99,8064,8096
        check roundss,11,input160,expected99,8064,8064
        check roundss,0,input165,expected119,8064,8096
        check roundss,1,input165,expected119,8064,8096
        check roundss,2,input165,expected117,8064,8096
        check roundss,3,input165,expected117,8064,8096
        check roundss,11,input165,expected117,8064,8064
        check roundss,0,input170,expected117,8064,8096
        check roundss,1,input170,expected119,8064,8096
        check roundss,2,input170,expected117,8064,8096
        check roundss,3,input170,expected117,8064,8096
        check roundss,11,input170,expected117,8064,8064
        check roundss,0,input19,expected175,8064,8064
        check roundss,1,input19,expected175,8064,8064
        check roundss,2,input19,expected175,8064,8064
        check roundss,3,input19,expected175,8064,8064
        check roundss,11,input19,expected175,8064,8064
        check roundss,0,input180,expected180,8064,8064
        check roundss,1,input180,expected180,8064,8064
        check roundss,2,input180,expected180,8064,8064
        check roundss,3,input180,expected180,8064,8064
        check roundss,11,input180,expected180,8064,8064
        check roundss,0,input185,expected185,8064,8064
        check roundss,1,input185,expected185,8064,8064
        check roundss,2,input185,expected185,8064,8064
        check roundss,3,input185,expected185,8064,8064
        check roundss,11,input185,expected185,8064,8064
        check roundss,0,input190,expected185,8064,8065
        check roundss,1,input190,expected185,8064,8065
        check roundss,2,input190,expected185,8064,8065
        check roundss,3,input190,expected185,8064,8065
        check roundss,11,input190,expected185,8064,8065
        check roundss,0,input24,expected99,8064,8096
        check roundss,0,input24,expected99,8128,8128
        check roundss,1,input24,expected99,8064,8096
        check roundss,1,input24,expected99,8128,8128
        check roundss,2,input24,expected103,8064,8096
        check roundss,2,input24,expected99,8128,8128
        check roundss,3,input24,expected99,8064,8096
        check roundss,11,input24,expected99,8064,8064
        check roundss,3,input24,expected99,8128,8128
        check roundss,0,input204,expected117,8064,8096
        check roundss,0,input204,expected117,8128,8128
        check roundss,1,input204,expected119,8064,8096
        check roundss,1,input204,expected117,8128,8128
        check roundss,2,input204,expected117,8064,8096
        check roundss,2,input204,expected117,8128,8128
        check roundss,3,input204,expected117,8064,8096
        check roundss,11,input204,expected117,8064,8064
        check roundss,3,input204,expected117,8128,8128
        check roundss,0,input213,expected117,8064,8064
        check roundss,0,input213,expected117,8128,8128
        check roundss,1,input213,expected117,8064,8064
        check roundss,1,input213,expected117,8128,8128
        check roundss,2,input213,expected117,8064,8064
        check roundss,2,input213,expected117,8128,8128
        check roundss,3,input213,expected117,8064,8064
        check roundss,11,input213,expected117,8064,8064
        check roundss,3,input213,expected117,8128,8128
        check roundss,0,input222,expected99,8064,8064
        check roundss,0,input222,expected99,8128,8128
        check roundss,1,input222,expected99,8064,8064
        check roundss,1,input222,expected99,8128,8128
        check roundss,2,input222,expected99,8064,8064
        check roundss,2,input222,expected99,8128,8128
        check roundss,3,input222,expected99,8064,8064
        check roundss,11,input222,expected99,8064,8064
        check roundss,3,input222,expected99,8128,8128
        check roundsd,0,input33,expected231,8064,8096
        check roundsd,4,input33,expected231,8064,8096
        check roundsd,1,input33,expected231,8064,8096
        check roundsd,4,input33,expected231,16256,16288
        check roundsd,2,input33,expected235,8064,8096
        check roundsd,4,input33,expected235,24448,24480
        check roundsd,3,input33,expected231,8064,8096
        check roundsd,4,input33,expected231,32640,32672
        check roundsd,11,input33,expected231,8064,8064
        check roundsd,0,input240,expected240,8064,8096
        check roundsd,4,input240,expected240,8064,8096
        check roundsd,1,input240,expected235,8064,8096
        check roundsd,4,input240,expected235,16256,16288
        check roundsd,2,input240,expected240,8064,8096
        check roundsd,4,input240,expected240,24448,24480
        check roundsd,3,input240,expected235,8064,8096
        check roundsd,4,input240,expected235,32640,32672
        check roundsd,11,input240,expected235,8064,8064
        check roundsd,0,input42,expected249,8064,8096
        check roundsd,4,input42,expected249,8064,8096
        check roundsd,1,input42,expected251,8064,8096
        check roundsd,4,input42,expected251,16256,16288
        check roundsd,2,input42,expected249,8064,8096
        check roundsd,4,input42,expected249,24448,24480
        check roundsd,3,input42,expected249,8064,8096
        check roundsd,4,input42,expected249,32640,32672
        check roundsd,11,input42,expected249,8064,8064
        check roundsd,0,input258,expected258,8064,8096
        check roundsd,4,input258,expected258,8064,8096
        check roundsd,1,input258,expected258,8064,8096
        check roundsd,4,input258,expected258,16256,16288
        check roundsd,2,input258,expected251,8064,8096
        check roundsd,4,input258,expected251,24448,24480
        check roundsd,3,input258,expected251,8064,8096
        check roundsd,4,input258,expected251,32640,32672
        check roundsd,11,input258,expected251,8064,8064
        check roundsd,0,input51,expected240,8064,8096
        check roundsd,1,input51,expected240,8064,8096
        check roundsd,2,input51,expected269,8064,8096
        check roundsd,3,input51,expected240,8064,8096
        check roundsd,11,input51,expected240,8064,8064
        check roundsd,0,input272,expected272,8064,8096
        check roundsd,1,input272,expected269,8064,8096
        check roundsd,2,input272,expected272,8064,8096
        check roundsd,3,input272,expected269,8064,8096
        check roundsd,11,input272,expected269,8064,8064
        check roundsd,0,input56,expected258,8064,8096
        check roundsd,1,input56,expected278,8064,8096
        check roundsd,2,input56,expected258,8064,8096
        check roundsd,3,input56,expected258,8064,8096
        check roundsd,11,input56,expected258,8064,8064
        check roundsd,0,input282,expected282,8064,8096
        check roundsd,1,input282,expected282,8064,8096
        check roundsd,2,input282,expected278,8064,8096
        check roundsd,3,input282,expected278,8064,8096
        check roundsd,11,input282,expected278,8064,8064
        check roundsd,0,input61,expected235,8064,8096
        check roundsd,1,input61,expected231,8064,8096
        check roundsd,2,input61,expected235,8064,8096
        check roundsd,3,input61,expected231,8064,8096
        check roundsd,11,input61,expected231,8064,8064
        check roundsd,0,input292,expected231,8064,8096
        check roundsd,1,input292,expected231,8064,8096
        check roundsd,2,input292,expected235,8064,8096
        check roundsd,3,input292,expected231,8064,8096
        check roundsd,11,input292,expected231,8064,8064
        check roundsd,0,input66,expected251,8064,8096
        check roundsd,1,input66,expected251,8064,8096
        check roundsd,2,input66,expected249,8064,8096
        check roundsd,3,input66,expected249,8064,8096
        check roundsd,11,input66,expected249,8064,8064
        check roundsd,0,input302,expected249,8064,8096
        check roundsd,1,input302,expected251,8064,8096
        check roundsd,2,input302,expected249,8064,8096
        check roundsd,3,input302,expected249,8064,8096
        check roundsd,11,input302,expected249,8064,8064
        check roundsd,0,input71,expected307,8064,8064
        check roundsd,1,input71,expected307,8064,8064
        check roundsd,2,input71,expected307,8064,8064
        check roundsd,3,input71,expected307,8064,8064
        check roundsd,11,input71,expected307,8064,8064
        check roundsd,0,input312,expected312,8064,8064
        check roundsd,1,input312,expected312,8064,8064
        check roundsd,2,input312,expected312,8064,8064
        check roundsd,3,input312,expected312,8064,8064
        check roundsd,11,input312,expected312,8064,8064
        check roundsd,0,input76,expected317,8064,8064
        check roundsd,1,input76,expected317,8064,8064
        check roundsd,2,input76,expected317,8064,8064
        check roundsd,3,input76,expected317,8064,8064
        check roundsd,11,input76,expected317,8064,8064
        check roundsd,0,input322,expected317,8064,8065
        check roundsd,1,input322,expected317,8064,8065
        check roundsd,2,input322,expected317,8064,8065
        check roundsd,3,input322,expected317,8064,8065
        check roundsd,11,input322,expected317,8064,8065
        check roundsd,0,input81,expected231,8064,8096
        check roundsd,0,input81,expected231,8128,8128
        check roundsd,1,input81,expected231,8064,8096
        check roundsd,1,input81,expected231,8128,8128
        check roundsd,2,input81,expected235,8064,8096
        check roundsd,2,input81,expected231,8128,8128
        check roundsd,3,input81,expected231,8064,8096
        check roundsd,11,input81,expected231,8064,8064
        check roundsd,3,input81,expected231,8128,8128
        check roundsd,0,input336,expected249,8064,8096
        check roundsd,0,input336,expected249,8128,8128
        check roundsd,1,input336,expected251,8064,8096
        check roundsd,1,input336,expected249,8128,8128
        check roundsd,2,input336,expected249,8064,8096
        check roundsd,2,input336,expected249,8128,8128
        check roundsd,3,input336,expected249,8064,8096
        check roundsd,11,input336,expected249,8064,8064
        check roundsd,3,input336,expected249,8128,8128
        check roundsd,0,input90,expected249,8064,8064
        check roundsd,0,input90,expected249,8128,8128
        check roundsd,1,input90,expected249,8064,8064
        check roundsd,1,input90,expected249,8128,8128
        check roundsd,2,input90,expected249,8064,8064
        check roundsd,2,input90,expected249,8128,8128
        check roundsd,3,input90,expected249,8064,8064
        check roundsd,11,input90,expected249,8064,8064
        check roundsd,3,input90,expected249,8128,8128
        check roundsd,0,input354,expected231,8064,8064
        check roundsd,0,input354,expected231,8128,8128
        check roundsd,1,input354,expected231,8064,8064
        check roundsd,1,input354,expected231,8128,8128
        check roundsd,2,input354,expected231,8064,8064
        check roundsd,2,input354,expected231,8128,8128
        check roundsd,3,input354,expected231,8064,8064
        check roundsd,11,input354,expected231,8064,8064
        check roundsd,3,input354,expected231,8128,8128
        .exit
.section .rodata
.balign 16
poison:
        .byte 170,170,170,170,170,170,170,170,170,170,170,170,170,170,170,170
input0:
        .byte 0,0,0,63,0,0,192,63,0,0,0,191,0,0,192,191
expected0:
        .byte 0,0,0,0,0,0,0,64,0,0,0,128,0,0,0,192
expected2:
        .byte 0,0,0,0,0,0,128,63,0,0,128,191,0,0,0,192
expected4:
        .byte 0,0,128,63,0,0,0,64,0,0,0,128,0,0,128,191
expected6:
        .byte 0,0,0,0,0,0,128,63,0,0,0,128,0,0,128,191
input9:
        .byte 0,0,32,64,0,0,96,64,0,0,32,192,0,0,96,192
expected9:
        .byte 0,0,0,64,0,0,128,64,0,0,0,192,0,0,128,192
expected10:
        .byte 0,0,0,64,0,0,64,64,0,0,64,192,0,0,128,192
expected11:
        .byte 0,0,64,64,0,0,128,64,0,0,0,192,0,0,64,192
expected12:
        .byte 0,0,0,64,0,0,64,64,0,0,0,192,0,0,64,192
input14:
        .byte 0,0,64,63,0,0,128,62,0,0,64,191,0,0,128,190
expected14:
        .byte 0,0,128,63,0,0,0,0,0,0,128,191,0,0,0,128
expected15:
        .byte 0,0,0,0,0,0,0,0,0,0,128,191,0,0,128,191
expected16:
        .byte 0,0,128,63,0,0,128,63,0,0,0,128,0,0,0,128
expected17:
        .byte 0,0,0,0,0,0,0,0,0,0,0,128,0,0,0,128
input19:
        .byte 0,0,128,127,0,0,128,255,123,0,192,127,123,0,128,127
expected19:
        .byte 0,0,128,127,0,0,128,255,123,0,192,127,123,0,192,127
input24:
        .byte 1,0,0,0,1,0,0,128,0,0,0,128,0,0,0,0
expected24:
        .byte 0,0,0,0,0,0,0,128,0,0,0,128,0,0,0,0
expected26:
        .byte 0,0,0,0,0,0,128,191,0,0,0,128,0,0,0,0
expected28:
        .byte 0,0,128,63,0,0,0,128,0,0,0,128,0,0,0,0
input33:
        .byte 0,0,0,0,0,0,224,63,0,0,0,0,0,0,248,63
expected33:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,64
expected35:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,63
expected37:
        .byte 0,0,0,0,0,0,240,63,0,0,0,0,0,0,0,64
input42:
        .byte 0,0,0,0,0,0,224,191,0,0,0,0,0,0,248,191
expected42:
        .byte 0,0,0,0,0,0,0,128,0,0,0,0,0,0,0,192
expected44:
        .byte 0,0,0,0,0,0,240,191,0,0,0,0,0,0,0,192
expected46:
        .byte 0,0,0,0,0,0,0,128,0,0,0,0,0,0,240,191
input51:
        .byte 0,0,0,0,0,0,4,64,0,0,0,0,0,0,12,64
expected51:
        .byte 0,0,0,0,0,0,0,64,0,0,0,0,0,0,16,64
expected52:
        .byte 0,0,0,0,0,0,0,64,0,0,0,0,0,0,8,64
expected53:
        .byte 0,0,0,0,0,0,8,64,0,0,0,0,0,0,16,64
input56:
        .byte 0,0,0,0,0,0,4,192,0,0,0,0,0,0,12,192
expected56:
        .byte 0,0,0,0,0,0,0,192,0,0,0,0,0,0,16,192
expected57:
        .byte 0,0,0,0,0,0,8,192,0,0,0,0,0,0,16,192
expected58:
        .byte 0,0,0,0,0,0,0,192,0,0,0,0,0,0,8,192
input61:
        .byte 0,0,0,0,0,0,232,63,0,0,0,0,0,0,208,63
expected61:
        .byte 0,0,0,0,0,0,240,63,0,0,0,0,0,0,0,0
expected62:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
expected63:
        .byte 0,0,0,0,0,0,240,63,0,0,0,0,0,0,240,63
input66:
        .byte 0,0,0,0,0,0,232,191,0,0,0,0,0,0,208,191
expected66:
        .byte 0,0,0,0,0,0,240,191,0,0,0,0,0,0,0,128
expected67:
        .byte 0,0,0,0,0,0,240,191,0,0,0,0,0,0,240,191
expected68:
        .byte 0,0,0,0,0,0,0,128,0,0,0,0,0,0,0,128
input71:
        .byte 0,0,0,0,0,0,240,127,0,0,0,0,0,0,240,255
input76:
        .byte 123,0,0,0,0,0,248,127,123,0,0,0,0,0,240,127
expected76:
        .byte 123,0,0,0,0,0,248,127,123,0,0,0,0,0,248,127
input81:
        .byte 1,0,0,0,0,0,0,0,1,0,0,0,0,0,0,128
expected81:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,128
expected83:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,240,191
expected85:
        .byte 0,0,0,0,0,0,240,63,0,0,0,0,0,0,0,128
input90:
        .byte 0,0,0,0,0,0,0,128,0,0,0,0,0,0,0,0
expected99:
        .byte 0,0,0,0,170,170,170,170,170,170,170,170,170,170,170,170
expected103:
        .byte 0,0,128,63,170,170,170,170,170,170,170,170,170,170,170,170
input108:
        .byte 0,0,192,63,0,0,0,191,0,0,192,191,0,0,0,63
expected108:
        .byte 0,0,0,64,170,170,170,170,170,170,170,170,170,170,170,170
input117:
        .byte 0,0,0,191,0,0,192,191,0,0,0,63,0,0,192,63
expected117:
        .byte 0,0,0,128,170,170,170,170,170,170,170,170,170,170,170,170
expected119:
        .byte 0,0,128,191,170,170,170,170,170,170,170,170,170,170,170,170
input126:
        .byte 0,0,192,191,0,0,0,63,0,0,192,63,0,0,0,191
expected126:
        .byte 0,0,0,192,170,170,170,170,170,170,170,170,170,170,170,170
expected137:
        .byte 0,0,64,64,170,170,170,170,170,170,170,170,170,170,170,170
input140:
        .byte 0,0,96,64,0,0,32,192,0,0,96,192,0,0,32,64
expected140:
        .byte 0,0,128,64,170,170,170,170,170,170,170,170,170,170,170,170
input145:
        .byte 0,0,32,192,0,0,96,192,0,0,32,64,0,0,96,64
expected146:
        .byte 0,0,64,192,170,170,170,170,170,170,170,170,170,170,170,170
input150:
        .byte 0,0,96,192,0,0,32,64,0,0,96,64,0,0,32,192
expected150:
        .byte 0,0,128,192,170,170,170,170,170,170,170,170,170,170,170,170
input160:
        .byte 0,0,128,62,0,0,64,191,0,0,128,190,0,0,64,63
input165:
        .byte 0,0,64,191,0,0,128,190,0,0,64,63,0,0,128,62
input170:
        .byte 0,0,128,190,0,0,64,63,0,0,128,62,0,0,64,191
expected175:
        .byte 0,0,128,127,170,170,170,170,170,170,170,170,170,170,170,170
input180:
        .byte 0,0,128,255,123,0,192,127,123,0,128,127,0,0,128,127
expected180:
        .byte 0,0,128,255,170,170,170,170,170,170,170,170,170,170,170,170
input185:
        .byte 123,0,192,127,123,0,128,127,0,0,128,127,0,0,128,255
expected185:
        .byte 123,0,192,127,170,170,170,170,170,170,170,170,170,170,170,170
input190:
        .byte 123,0,128,127,0,0,128,127,0,0,128,255,123,0,192,127
input204:
        .byte 1,0,0,128,0,0,0,128,0,0,0,0,1,0,0,0
input213:
        .byte 0,0,0,128,0,0,0,0,1,0,0,0,1,0,0,128
input222:
        .byte 0,0,0,0,1,0,0,0,1,0,0,128,0,0,0,128
expected231:
        .byte 0,0,0,0,0,0,0,0,170,170,170,170,170,170,170,170
expected235:
        .byte 0,0,0,0,0,0,240,63,170,170,170,170,170,170,170,170
input240:
        .byte 0,0,0,0,0,0,248,63,0,0,0,0,0,0,224,191
expected240:
        .byte 0,0,0,0,0,0,0,64,170,170,170,170,170,170,170,170
expected249:
        .byte 0,0,0,0,0,0,0,128,170,170,170,170,170,170,170,170
expected251:
        .byte 0,0,0,0,0,0,240,191,170,170,170,170,170,170,170,170
input258:
        .byte 0,0,0,0,0,0,248,191,0,0,0,0,0,0,224,63
expected258:
        .byte 0,0,0,0,0,0,0,192,170,170,170,170,170,170,170,170
expected269:
        .byte 0,0,0,0,0,0,8,64,170,170,170,170,170,170,170,170
input272:
        .byte 0,0,0,0,0,0,12,64,0,0,0,0,0,0,4,192
expected272:
        .byte 0,0,0,0,0,0,16,64,170,170,170,170,170,170,170,170
expected278:
        .byte 0,0,0,0,0,0,8,192,170,170,170,170,170,170,170,170
input282:
        .byte 0,0,0,0,0,0,12,192,0,0,0,0,0,0,4,64
expected282:
        .byte 0,0,0,0,0,0,16,192,170,170,170,170,170,170,170,170
input292:
        .byte 0,0,0,0,0,0,208,63,0,0,0,0,0,0,232,191
input302:
        .byte 0,0,0,0,0,0,208,191,0,0,0,0,0,0,232,63
expected307:
        .byte 0,0,0,0,0,0,240,127,170,170,170,170,170,170,170,170
input312:
        .byte 0,0,0,0,0,0,240,255,123,0,0,0,0,0,248,127
expected312:
        .byte 0,0,0,0,0,0,240,255,170,170,170,170,170,170,170,170
expected317:
        .byte 123,0,0,0,0,0,248,127,170,170,170,170,170,170,170,170
input322:
        .byte 123,0,0,0,0,0,240,127,0,0,0,0,0,0,240,127
input336:
        .byte 1,0,0,0,0,0,0,128,0,0,0,0,0,0,0,128
input354:
        .byte 0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0
