#include "test/asm/mac.inc"
.macro check op:req, operand:req, dest:req, expected:req
        cmp %rsp,%rsp
        stc
        \op \operand,\dest
        .c
        .e
        .p
        .ns
        .no
        pcmpeqb \expected(%rip),\dest
        pmovmskb \dest,%eax
        cmp $65535,%eax
        .e
.endm
.globl _start
_start:
        .test "pmov guard page setup"
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
        mov source(%rip),%rax
        mov %rax,4088(%r12)
        lea 4096(%r12),%rdi
        mov $4096,%esi
        xor %edx,%edx
        mov $10,%eax
        syscall
        test %eax,%eax
        .z
        .test "pmovzxbw extended registers"
        movdqu source(%rip),%xmm9
        check pmovzxbw,%xmm9,%xmm8,pmovzxbw_expected
        .test "pmovzxbw register alias"
        movdqu source(%rip),%xmm8
        check pmovzxbw,%xmm8,%xmm8,pmovzxbw_expected
        .test "pmovzxbw exact memory width"
        check pmovzxbw,4088(%r12),%xmm8,pmovzxbw_guard
        .test "pmovzxbd extended registers"
        movdqu source(%rip),%xmm9
        check pmovzxbd,%xmm9,%xmm8,pmovzxbd_expected
        .test "pmovzxbd register alias"
        movdqu source(%rip),%xmm8
        check pmovzxbd,%xmm8,%xmm8,pmovzxbd_expected
        .test "pmovzxbd exact memory width"
        check pmovzxbd,4092(%r12),%xmm8,pmovzxbd_guard
        .test "pmovzxbq extended registers"
        movdqu source(%rip),%xmm9
        check pmovzxbq,%xmm9,%xmm8,pmovzxbq_expected
        .test "pmovzxbq register alias"
        movdqu source(%rip),%xmm8
        check pmovzxbq,%xmm8,%xmm8,pmovzxbq_expected
        .test "pmovzxbq exact memory width"
        check pmovzxbq,4094(%r12),%xmm8,pmovzxbq_guard
        .test "pmovzxwd extended registers"
        movdqu source(%rip),%xmm9
        check pmovzxwd,%xmm9,%xmm8,pmovzxwd_expected
        .test "pmovzxwd register alias"
        movdqu source(%rip),%xmm8
        check pmovzxwd,%xmm8,%xmm8,pmovzxwd_expected
        .test "pmovzxwd exact memory width"
        check pmovzxwd,4088(%r12),%xmm8,pmovzxwd_guard
        .test "pmovzxwq extended registers"
        movdqu source(%rip),%xmm9
        check pmovzxwq,%xmm9,%xmm8,pmovzxwq_expected
        .test "pmovzxwq register alias"
        movdqu source(%rip),%xmm8
        check pmovzxwq,%xmm8,%xmm8,pmovzxwq_expected
        .test "pmovzxwq exact memory width"
        check pmovzxwq,4092(%r12),%xmm8,pmovzxwq_guard
        .test "pmovzxdq extended registers"
        movdqu source(%rip),%xmm9
        check pmovzxdq,%xmm9,%xmm8,pmovzxdq_expected
        .test "pmovzxdq register alias"
        movdqu source(%rip),%xmm8
        check pmovzxdq,%xmm8,%xmm8,pmovzxdq_expected
        .test "pmovzxdq exact memory width"
        check pmovzxdq,4088(%r12),%xmm8,pmovzxdq_guard
        .test "pmovsxbw extended registers"
        movdqu source(%rip),%xmm9
        check pmovsxbw,%xmm9,%xmm8,pmovsxbw_expected
        .test "pmovsxbw register alias"
        movdqu source(%rip),%xmm8
        check pmovsxbw,%xmm8,%xmm8,pmovsxbw_expected
        .test "pmovsxbw exact memory width"
        check pmovsxbw,4088(%r12),%xmm8,pmovsxbw_guard
        .test "pmovsxbd extended registers"
        movdqu source(%rip),%xmm9
        check pmovsxbd,%xmm9,%xmm8,pmovsxbd_expected
        .test "pmovsxbd register alias"
        movdqu source(%rip),%xmm8
        check pmovsxbd,%xmm8,%xmm8,pmovsxbd_expected
        .test "pmovsxbd exact memory width"
        check pmovsxbd,4092(%r12),%xmm8,pmovsxbd_guard
        .test "pmovsxbq extended registers"
        movdqu source(%rip),%xmm9
        check pmovsxbq,%xmm9,%xmm8,pmovsxbq_expected
        .test "pmovsxbq register alias"
        movdqu source(%rip),%xmm8
        check pmovsxbq,%xmm8,%xmm8,pmovsxbq_expected
        .test "pmovsxbq exact memory width"
        check pmovsxbq,4094(%r12),%xmm8,pmovsxbq_guard
        .test "pmovsxwd extended registers"
        movdqu source(%rip),%xmm9
        check pmovsxwd,%xmm9,%xmm8,pmovsxwd_expected
        .test "pmovsxwd register alias"
        movdqu source(%rip),%xmm8
        check pmovsxwd,%xmm8,%xmm8,pmovsxwd_expected
        .test "pmovsxwd exact memory width"
        check pmovsxwd,4088(%r12),%xmm8,pmovsxwd_guard
        .test "pmovsxwq extended registers"
        movdqu source(%rip),%xmm9
        check pmovsxwq,%xmm9,%xmm8,pmovsxwq_expected
        .test "pmovsxwq register alias"
        movdqu source(%rip),%xmm8
        check pmovsxwq,%xmm8,%xmm8,pmovsxwq_expected
        .test "pmovsxwq exact memory width"
        check pmovsxwq,4092(%r12),%xmm8,pmovsxwq_guard
        .test "pmovsxdq extended registers"
        movdqu source(%rip),%xmm9
        check pmovsxdq,%xmm9,%xmm8,pmovsxdq_expected
        .test "pmovsxdq register alias"
        movdqu source(%rip),%xmm8
        check pmovsxdq,%xmm8,%xmm8,pmovsxdq_expected
        .test "pmovsxdq exact memory width"
        check pmovsxdq,4088(%r12),%xmm8,pmovsxdq_guard
        .exit
.section .rodata
.balign 16
source:
        .byte 128,127,255,128,1,128,255,127,1,2,3,4,5,6,7,8
pmovzxbw_expected:
        .byte 128,0,127,0,255,0,128,0,1,0,128,0,255,0,127,0
pmovzxbw_guard:
        .byte 128,0,127,0,255,0,128,0,1,0,128,0,255,0,127,0
pmovzxbd_expected:
        .byte 128,0,0,0,127,0,0,0,255,0,0,0,128,0,0,0
pmovzxbd_guard:
        .byte 1,0,0,0,128,0,0,0,255,0,0,0,127,0,0,0
pmovzxbq_expected:
        .byte 128,0,0,0,0,0,0,0,127,0,0,0,0,0,0,0
pmovzxbq_guard:
        .byte 255,0,0,0,0,0,0,0,127,0,0,0,0,0,0,0
pmovzxwd_expected:
        .byte 128,127,0,0,255,128,0,0,1,128,0,0,255,127,0,0
pmovzxwd_guard:
        .byte 128,127,0,0,255,128,0,0,1,128,0,0,255,127,0,0
pmovzxwq_expected:
        .byte 128,127,0,0,0,0,0,0,255,128,0,0,0,0,0,0
pmovzxwq_guard:
        .byte 1,128,0,0,0,0,0,0,255,127,0,0,0,0,0,0
pmovzxdq_expected:
        .byte 128,127,255,128,0,0,0,0,1,128,255,127,0,0,0,0
pmovzxdq_guard:
        .byte 128,127,255,128,0,0,0,0,1,128,255,127,0,0,0,0
pmovsxbw_expected:
        .byte 128,255,127,0,255,255,128,255,1,0,128,255,255,255,127,0
pmovsxbw_guard:
        .byte 128,255,127,0,255,255,128,255,1,0,128,255,255,255,127,0
pmovsxbd_expected:
        .byte 128,255,255,255,127,0,0,0,255,255,255,255,128,255,255,255
pmovsxbd_guard:
        .byte 1,0,0,0,128,255,255,255,255,255,255,255,127,0,0,0
pmovsxbq_expected:
        .byte 128,255,255,255,255,255,255,255,127,0,0,0,0,0,0,0
pmovsxbq_guard:
        .byte 255,255,255,255,255,255,255,255,127,0,0,0,0,0,0,0
pmovsxwd_expected:
        .byte 128,127,0,0,255,128,255,255,1,128,255,255,255,127,0,0
pmovsxwd_guard:
        .byte 128,127,0,0,255,128,255,255,1,128,255,255,255,127,0,0
pmovsxwq_expected:
        .byte 128,127,0,0,0,0,0,0,255,128,255,255,255,255,255,255
pmovsxwq_guard:
        .byte 1,128,255,255,255,255,255,255,255,127,0,0,0,0,0,0
pmovsxdq_expected:
        .byte 128,127,255,128,255,255,255,255,1,128,255,127,0,0,0,0
pmovsxdq_guard:
        .byte 128,127,255,128,255,255,255,255,1,128,255,127,0,0,0,0
