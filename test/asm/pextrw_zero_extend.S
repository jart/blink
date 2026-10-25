#include "test/asm/mac.inc"
.globl _start
_start:
        pcmpeqw %xmm8,%xmm8
        mov $33024,%eax
        pinsrw $0,%eax,%xmm8
        mov $33025,%eax
        pinsrw $1,%eax,%xmm8
        mov $33026,%eax
        pinsrw $2,%eax,%xmm8
        mov $33027,%eax
        pinsrw $3,%eax,%xmm8
        mov $33028,%eax
        pinsrw $4,%eax,%xmm8
        mov $33029,%eax
        pinsrw $5,%eax,%xmm8
        mov $33030,%eax
        pinsrw $6,%eax,%xmm8
        mov $33031,%eax
        pinsrw $7,%eax,%xmm8
        .test "pextrw SSE2 lane 0 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $248,%xmm8,%r9d
        .e
        cmp $33024,%r9
        .e
        .test "pextrw SSE2 lane 1 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $249,%xmm8,%r9d
        .e
        cmp $33025,%r9
        .e
        .test "pextrw SSE2 lane 2 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $250,%xmm8,%r9d
        .e
        cmp $33026,%r9
        .e
        .test "pextrw SSE2 lane 3 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $251,%xmm8,%r9d
        .e
        cmp $33027,%r9
        .e
        .test "pextrw SSE2 lane 4 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $252,%xmm8,%r9d
        .e
        cmp $33028,%r9
        .e
        .test "pextrw SSE2 lane 5 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $253,%xmm8,%r9d
        .e
        cmp $33029,%r9
        .e
        .test "pextrw SSE2 lane 6 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $254,%xmm8,%r9d
        .e
        cmp $33030,%r9
        .e
        .test "pextrw SSE2 lane 7 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $255,%xmm8,%r9d
        .e
        cmp $33031,%r9
        .e
        pcmpeqw %mm3,%mm3
        mov $33024,%eax
        pinsrw $0,%eax,%mm3
        mov $33025,%eax
        pinsrw $1,%eax,%mm3
        mov $33026,%eax
        pinsrw $2,%eax,%mm3
        mov $33027,%eax
        pinsrw $3,%eax,%mm3
        .test "pextrw MMX lane 0 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $252,%mm3,%r9d
        .e
        cmp $33024,%r9
        .e
        .test "pextrw MMX lane 1 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $253,%mm3,%r9d
        .e
        cmp $33025,%r9
        .e
        .test "pextrw MMX lane 2 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $254,%mm3,%r9d
        .e
        cmp $33026,%r9
        .e
        .test "pextrw MMX lane 3 zero extends"
        mov $-1,%r9
        cmp %r9,%r9
        pextrw $255,%mm3,%r9d
        .e
        cmp $33027,%r9
        .e
        emms
        .exit
