#include "test/asm/mac.inc"
.globl _start
_start:
        sub $16,%rsp
        .test "pcmpistri control 0 vector 0"
        movdqu ab(%rip),%xmm8
        movdqu cabba(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistri $0,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        cmp $1,%rcx
        .e
        .test "pcmpistri control 64 vector 1"
        movdqu ab(%rip),%xmm8
        movdqu cabba(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistri $64,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        cmp $4,%rcx
        .e
        .test "pcmpistrm control 0 vector 2"
        movdqu ab(%rip),%xmm8
        movdqu cabba(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $0,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $30,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 16 vector 3"
        movdqu ab(%rip),%xmm8
        movdqu cabba(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $16,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $2241,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $65505,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 32 vector 4"
        movdqu ab(%rip),%xmm8
        movdqu cabba(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $32,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $30,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 48 vector 5"
        movdqu ab(%rip),%xmm8
        movdqu cabba(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $48,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $2241,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $1,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 64 vector 6"
        movdqu ab(%rip),%xmm8
        movdqu cabba(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $64,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $1099511627520,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistri control 8 vector 7"
        movdqu ab(%rip),%xmm8
        movdqu ac(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistri $8,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $2241,%r11
        .e
        cmp $0,%rcx
        .e
        .test "pcmpistri control 24 vector 8"
        movdqu ab(%rip),%xmm8
        movdqu ac(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistri $24,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        cmp $1,%rcx
        .e
        .test "pcmpistrm control 8 vector 9"
        movdqu ab(%rip),%xmm8
        movdqu ac(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $8,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $2241,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $65533,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 56 vector 10"
        movdqu ab(%rip),%xmm8
        movdqu ac(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $56,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $65534,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistri control 24 vector 11"
        movdqu ab(%rip),%xmm8
        movdqu ab(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistri $24,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $192,%r11
        .e
        cmp $16,%rcx
        .e
        .test "pcmpistrm control 12 vector 12"
        movdqu ab(%rip),%xmm8
        movdqu zabxab(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $12,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $18,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 12 vector 13"
        movdqu empty(%rip),%xmm8
        movdqu zabxab(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $12,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $2241,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $65535,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 12 vector 14"
        movdqu alphabet(%rip),%xmm8
        movdqu tail(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $12,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $1,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $32768,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 4 vector 15"
        movdqu ranges(%rip),%xmm8
        movdqu rangehay(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $4,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $22,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 6 vector 16"
        movdqu signedrange(%rip),%xmm8
        movdqu signedhay(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $6,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $2241,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $3,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 4 vector 17"
        movdqu signedrange(%rip),%xmm8
        movdqu signedhay(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $4,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $192,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $0,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpestrm control 0 vector 18"
        movdqu embedded(%rip),%xmm8
        movdqu embeddedhay(%rip),%xmm9
        mov $3,%eax
        mov $3,%edx
        mov $-1,%rcx
        pcmpestrm $0,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $6,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpestri control 0 vector 19"
        movdqu embedded(%rip),%xmm8
        movdqu embeddedhay(%rip),%xmm9
        mov $-3,%eax
        mov $-3,%edx
        mov $-1,%rcx
        pcmpestri $0,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        cmp $1,%rcx
        .e
        .test "pcmpestri control 0 vector 20"
        movdqu embedded(%rip),%xmm8
        movdqu embeddedhay(%rip),%xmm9
        mov $0,%eax
        mov $3,%edx
        mov $-1,%rcx
        pcmpestri $0,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $192,%r11
        .e
        cmp $16,%rcx
        .e
        .test "pcmpestrm control 4 vector 21"
        movdqu ranges(%rip),%xmm8
        movdqu rangehay(%rip),%xmm9
        mov $1,%eax
        mov $5,%edx
        mov $-1,%rcx
        pcmpestrm $4,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $192,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $0,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpestrm control 8 vector 22"
        movdqu alphabet(%rip),%xmm8
        movdqu alphabet(%rip),%xmm9
        mov $-2147483648,%eax
        mov $2147483647,%edx
        mov $-1,%rcx
        pcmpestrm $8,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $2049,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $65535,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 1 vector 23"
        movdqu worda(%rip),%xmm8
        movdqu wordb(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $1,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $2,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 65 vector 24"
        movdqu worda(%rip),%xmm8
        movdqu wordb(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $65,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $4294901760,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistri control 1 vector 25"
        movdqu worda(%rip),%xmm8
        movdqu wordb(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistri $1,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $193,%r11
        .e
        cmp $1,%rcx
        .e
        .test "pcmpistrm control 7 vector 26"
        movdqu wordrange(%rip),%xmm8
        movdqu wordhay(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $7,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $2241,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $3,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistrm control 5 vector 27"
        movdqu wordrange(%rip),%xmm8
        movdqu wordhay(%rip),%xmm9
        mov $0,%eax
        mov $0,%edx
        mov $-1,%rcx
        pcmpistrm $5,%xmm9,%xmm8
        pushfq
        pop %r11
        and $0x8d5,%r11
        cmp $192,%r11
        .e
        movdqu %xmm0,(%rsp)
        movabs $0,%r10
        cmp %r10,(%rsp)
        .e
        cmpq $0,8(%rsp)
        .e
        .test "pcmpistri unaligned memory source"
        movdqu ab(%rip),%xmm8
        pcmpistri $0,cabba(%rip),%xmm8
        cmp $1,%rcx
        .e
        .test "pcmpistrm output aliases first source"
        movdqu ab(%rip),%xmm0
        pcmpistrm $0,cabba(%rip),%xmm0
        movq %xmm0,%rax
        cmp $0x1e,%rax
        .e
        .test "pcmpestri REX.W negative lengths"
        movdqu embedded(%rip),%xmm8
        movdqu embeddedhay(%rip),%xmm9
        mov $-3,%rax
        mov $-3,%rdx
        .byte 0x66,0x4d,0x0f,0x3a,0x61,0xc1,0x00
        cmp $1,%rcx
        .e
        .test "pcmpestri 64-bit minimum length saturates"
        movdqu alphabet(%rip),%xmm8
        movdqu alphabet(%rip),%xmm9
        movabs $0x8000000000000000,%rax
        movabs $0x7fffffffffffffff,%rdx
        .byte 0x66,0x4d,0x0f,0x3a,0x61,0xc1,0x08
        .ns
        .nz
        .c
        .o
        cmp $0,%rcx
        .e
        .test "pcmpestri default length ignores upper register bits"
        movabs $0x100000003,%rax
        movabs $0x100000003,%rdx
        pcmpestri $8,%xmm9,%xmm8
        .s
        .z
        .c
        .o
        cmp $0,%rcx
        .e
        .exit
.section .rodata
ab:
        .byte 97,98,0,0,0,0,0,0,0,0,0,0,0,0,0,0
cabba:
        .byte 99,97,98,98,97,0,0,0,0,0,0,0,0,0,0,0
ac:
        .byte 97,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0
zabxab:
        .byte 122,97,98,120,97,98,0,0,0,0,0,0,0,0,0,0
empty:
        .byte 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
alphabet:
        .byte 97,98,99,100,101,102,103,104,105,106,107,108,109,110,111,112
tail:
        .byte 120,120,120,120,120,120,120,120,120,120,120,120,120,120,120,97
ranges:
        .byte 97,122,65,90,0,0,0,0,0,0,0,0,0,0,0,0
rangehay:
        .byte 49,97,90,48,98,0,0,0,0,0,0,0,0,0,0,0
signedrange:
        .byte 240,16,0,0,0,0,0,0,0,0,0,0,0,0,0,0
signedhay:
        .byte 245,5,32,0,0,0,0,0,0,0,0,0,0,0,0,0
embedded:
        .byte 97,0,98,0,0,0,0,0,0,0,0,0,0,0,0,0
embeddedhay:
        .byte 99,0,98,0,0,0,0,0,0,0,0,0,0,0,0,0
worda:
        .byte 52,18,0,0,0,0,0,0,0,0,0,0,0,0,0,0
wordb:
        .byte 120,86,52,18,0,0,0,0,0,0,0,0,0,0,0,0
wordrange:
        .byte 0,128,1,0,0,0,0,0,0,0,0,0,0,0,0,0
wordhay:
        .byte 1,128,1,0,2,0,0,0,0,0,0,0,0,0,0,0
