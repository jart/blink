#include "test/asm/mac.inc"
.globl _start
_start:
        sub $32,%rsp
.macro check n,imm,lo,hi,alo,ahi
.irp upper,0,248
.irp form,0,1,2
        .test "mpsadbw \n \imm \upper \form"
        movdqu left\n(%rip),%xmm8
        movdqu right\n(%rip),%xmm9
        xor %eax,%eax
        stc
        pushfq
        pop %r10
.if \form == 0
        mpsadbw $(\imm|\upper),%xmm9,%xmm8
.elseif \form == 1
        mpsadbw $(\imm|\upper),right\n(%rip),%xmm8
.else
        mpsadbw $(\imm|\upper),%xmm8,%xmm8
.endif
        pushfq
        pop %r11
        cmp %r10,%r11
        .e
        movdqu %xmm8,(%rsp)
.if \form == 2
        movabs $\alo,%rax
        movabs $\ahi,%rdx
.else
        movabs $\lo,%rax
        movabs $\hi,%rdx
.endif
        cmp %rax,(%rsp)
        .e
        cmp %rdx,8(%rsp)
        .e
        movdqu %xmm9,16(%rsp)
        mov right\n(%rip),%rax
        cmp %rax,16(%rsp)
        .e
        mov right\n+8(%rip),%rax
        cmp %rax,24(%rsp)
        .e
.endr
.endr
.endm
        check 0,0,0x26402a802ec0330,0x154019801dc0220,0xcc008800440000,0x1dc019801540110
        check 0,1,0x154019801dc0220,0x8800aa00cc0110,0x44008800cc0110,0xcc008800440000
        check 0,2,0x8800aa00cc0110,0xcc00aa00880088,0x154019801dc0220,0x44008800cc0110
        check 0,3,0xcc00aa00880088,0x1dc019801540110,0x26402a802ec0330,0x154019801dc0220
        check 0,4,0x154019801dc0220,0x8800aa00cc0110,0x1dc019801540110,0x2ec02a802640220
        check 0,5,0x8800aa00cc0110,0xcc00aa00880088,0xcc008800440000,0x1dc019801540110
        check 0,6,0xcc00aa00880088,0x1dc019801540110,0x44008800cc0110,0xcc008800440000
        check 0,7,0x1dc019801540110,0x2ec02a802640220,0x154019801dc0220,0x44008800cc0110
        check 1,0,0x3fc03fc03fc03fc,0x3fc03fc03fc03fc,0x0,0x0
        check 1,1,0x3fc03fc03fc03fc,0x3fc03fc03fc03fc,0x0,0x0
        check 1,2,0x3fc03fc03fc03fc,0x3fc03fc03fc03fc,0x0,0x0
        check 1,3,0x3fc03fc03fc03fc,0x3fc03fc03fc03fc,0x0,0x0
        check 1,4,0x3fc03fc03fc03fc,0x3fc03fc03fc03fc,0x0,0x0
        check 1,5,0x3fc03fc03fc03fc,0x3fc03fc03fc03fc,0x0,0x0
        check 1,6,0x3fc03fc03fc03fc,0x3fc03fc03fc03fc,0x0,0x0
        check 1,7,0x3fc03fc03fc03fc,0x3fc03fc03fc03fc,0x0,0x0
        check 2,0,0x5a02d5010802e1,0x229007e0276024c,0x297010002f50000,0xce0285026d0095
        check 2,1,0x248005b02a6014f,0x1dd0236013401cc,0x216017d02600095,0xf301f0024c0000
        check 2,2,0x200008b025e0177,0x1a3020000f601e2,0x1d102760081027a,0x1ae01f101a50293
        check 2,3,0x19800fd027601fd,0x2490186015001c8,0x1af01700179017c,0x14e018d0269017d
        check 2,4,0x229007e0276024c,0x16d0165016f0189,0xce0285026d0095,0x174017c0174027a
        check 2,5,0x1dd0236013401cc,0x15d01670161022b,0xf301f0024c0000,0x17d018901950293
        check 2,6,0x1a3020000f601e2,0x19b01a501ab01f1,0x1ae01f101a50293,0x11c011201080000
        check 2,7,0x2490186015001c8,0x251025d026902f5,0x14e018d0269017d,0xc001800240116
        .exit
.section .rodata
.balign 16
left0:
        .byte 0,17,34,51,68,85,102,119,136,153,170,187,204,221,238,255
right0:
        .byte 255,238,221,204,187,170,153,136,119,102,85,68,51,34,17,0
left1:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
right1:
        .byte 255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255
left2:
        .byte 0,255,1,128,7,250,90,80,255,2,3,4,13,14,15,16
right2:
        .byte 120,1,250,14,32,80,0,255,99,100,10,240,64,128,192,255
