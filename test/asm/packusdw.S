#include "test/asm/mac.inc"
.globl _start
_start:
 sub $32,%rsp
 .test "packusdw 0 register"
 movdqu left0(%rip),%xmm8
 movdqu right0(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $0,0(%rsp)
 .e
 cmpw $0,2(%rsp)
 .e
 cmpw $0,4(%rsp)
 .e
 cmpw $1,6(%rsp)
 .e
 cmpw $32767,8(%rsp)
 .e
 cmpw $65535,10(%rsp)
 .e
 cmpw $65535,12(%rsp)
 .e
 cmpw $65535,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $32767,16(%rsp)
 .e
 cmpl $65535,20(%rsp)
 .e
 cmpl $65536,24(%rsp)
 .e
 cmpl $2147483647,28(%rsp)
 .e
 .test "packusdw 1 memory"
 movdqu left1(%rip),%xmm8
 movdqu right1(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw right1(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $0,0(%rsp)
 .e
 cmpw $0,2(%rsp)
 .e
 cmpw $0,4(%rsp)
 .e
 cmpw $1,6(%rsp)
 .e
 cmpw $32767,8(%rsp)
 .e
 cmpw $65535,10(%rsp)
 .e
 cmpw $65535,12(%rsp)
 .e
 cmpw $65535,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $32767,16(%rsp)
 .e
 cmpl $65535,20(%rsp)
 .e
 cmpl $65536,24(%rsp)
 .e
 cmpl $2147483647,28(%rsp)
 .e
 .test "packusdw 2 alias"
 movdqu left2(%rip),%xmm8
 movdqu right2(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $0,0(%rsp)
 .e
 cmpw $0,2(%rsp)
 .e
 cmpw $0,4(%rsp)
 .e
 cmpw $1,6(%rsp)
 .e
 cmpw $0,8(%rsp)
 .e
 cmpw $0,10(%rsp)
 .e
 cmpw $0,12(%rsp)
 .e
 cmpw $1,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $32767,16(%rsp)
 .e
 cmpl $65535,20(%rsp)
 .e
 cmpl $65536,24(%rsp)
 .e
 cmpl $2147483647,28(%rsp)
 .e
 .test "packusdw 3 register"
 movdqu left3(%rip),%xmm8
 movdqu right3(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $32767,0(%rsp)
 .e
 cmpw $65535,2(%rsp)
 .e
 cmpw $65535,4(%rsp)
 .e
 cmpw $65535,6(%rsp)
 .e
 cmpw $0,8(%rsp)
 .e
 cmpw $0,10(%rsp)
 .e
 cmpw $0,12(%rsp)
 .e
 cmpw $1,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $-2147483648,16(%rsp)
 .e
 cmpl $-1,20(%rsp)
 .e
 cmpl $0,24(%rsp)
 .e
 cmpl $1,28(%rsp)
 .e
 .test "packusdw 4 memory"
 movdqu left4(%rip),%xmm8
 movdqu right4(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw right4(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $32767,0(%rsp)
 .e
 cmpw $65535,2(%rsp)
 .e
 cmpw $65535,4(%rsp)
 .e
 cmpw $65535,6(%rsp)
 .e
 cmpw $0,8(%rsp)
 .e
 cmpw $0,10(%rsp)
 .e
 cmpw $0,12(%rsp)
 .e
 cmpw $1,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $-2147483648,16(%rsp)
 .e
 cmpl $-1,20(%rsp)
 .e
 cmpl $0,24(%rsp)
 .e
 cmpl $1,28(%rsp)
 .e
 .test "packusdw 5 alias"
 movdqu left5(%rip),%xmm8
 movdqu right5(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $32767,0(%rsp)
 .e
 cmpw $65535,2(%rsp)
 .e
 cmpw $65535,4(%rsp)
 .e
 cmpw $65535,6(%rsp)
 .e
 cmpw $32767,8(%rsp)
 .e
 cmpw $65535,10(%rsp)
 .e
 cmpw $65535,12(%rsp)
 .e
 cmpw $65535,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $-2147483648,16(%rsp)
 .e
 cmpl $-1,20(%rsp)
 .e
 cmpl $0,24(%rsp)
 .e
 cmpl $1,28(%rsp)
 .e
 .test "packusdw 6 register"
 movdqu left6(%rip),%xmm8
 movdqu right6(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $65534,0(%rsp)
 .e
 cmpw $65535,2(%rsp)
 .e
 cmpw $65535,4(%rsp)
 .e
 cmpw $65535,6(%rsp)
 .e
 cmpw $0,8(%rsp)
 .e
 cmpw $0,10(%rsp)
 .e
 cmpw $32768,12(%rsp)
 .e
 cmpw $42,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $-2,16(%rsp)
 .e
 cmpl $-65535,20(%rsp)
 .e
 cmpl $32768,24(%rsp)
 .e
 cmpl $42,28(%rsp)
 .e
 .test "packusdw 7 memory"
 movdqu left7(%rip),%xmm8
 movdqu right7(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw right7(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $65534,0(%rsp)
 .e
 cmpw $65535,2(%rsp)
 .e
 cmpw $65535,4(%rsp)
 .e
 cmpw $65535,6(%rsp)
 .e
 cmpw $0,8(%rsp)
 .e
 cmpw $0,10(%rsp)
 .e
 cmpw $32768,12(%rsp)
 .e
 cmpw $42,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $-2,16(%rsp)
 .e
 cmpl $-65535,20(%rsp)
 .e
 cmpl $32768,24(%rsp)
 .e
 cmpl $42,28(%rsp)
 .e
 .test "packusdw 8 alias"
 movdqu left8(%rip),%xmm8
 movdqu right8(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $65534,0(%rsp)
 .e
 cmpw $65535,2(%rsp)
 .e
 cmpw $65535,4(%rsp)
 .e
 cmpw $65535,6(%rsp)
 .e
 cmpw $65534,8(%rsp)
 .e
 cmpw $65535,10(%rsp)
 .e
 cmpw $65535,12(%rsp)
 .e
 cmpw $65535,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $-2,16(%rsp)
 .e
 cmpl $-65535,20(%rsp)
 .e
 cmpl $32768,24(%rsp)
 .e
 cmpl $42,28(%rsp)
 .e
 .test "packusdw 9 register"
 movdqu left9(%rip),%xmm8
 movdqu right9(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $0,0(%rsp)
 .e
 cmpw $0,2(%rsp)
 .e
 cmpw $32768,4(%rsp)
 .e
 cmpw $42,6(%rsp)
 .e
 cmpw $65534,8(%rsp)
 .e
 cmpw $65535,10(%rsp)
 .e
 cmpw $65535,12(%rsp)
 .e
 cmpw $65535,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $65534,16(%rsp)
 .e
 cmpl $65535,20(%rsp)
 .e
 cmpl $65536,24(%rsp)
 .e
 cmpl $65537,28(%rsp)
 .e
 .test "packusdw 10 memory"
 movdqu left10(%rip),%xmm8
 movdqu right10(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw right10(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $0,0(%rsp)
 .e
 cmpw $0,2(%rsp)
 .e
 cmpw $32768,4(%rsp)
 .e
 cmpw $42,6(%rsp)
 .e
 cmpw $65534,8(%rsp)
 .e
 cmpw $65535,10(%rsp)
 .e
 cmpw $65535,12(%rsp)
 .e
 cmpw $65535,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $65534,16(%rsp)
 .e
 cmpl $65535,20(%rsp)
 .e
 cmpl $65536,24(%rsp)
 .e
 cmpl $65537,28(%rsp)
 .e
 .test "packusdw 11 alias"
 movdqu left11(%rip),%xmm8
 movdqu right11(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 packusdw %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpw $0,0(%rsp)
 .e
 cmpw $0,2(%rsp)
 .e
 cmpw $32768,4(%rsp)
 .e
 cmpw $42,6(%rsp)
 .e
 cmpw $0,8(%rsp)
 .e
 cmpw $0,10(%rsp)
 .e
 cmpw $32768,12(%rsp)
 .e
 cmpw $42,14(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 cmpl $65534,16(%rsp)
 .e
 cmpl $65535,20(%rsp)
 .e
 cmpl $65536,24(%rsp)
 .e
 cmpl $65537,28(%rsp)
 .e
 .exit
.section .rodata
.balign 16
left0:
 .long -2147483648,-1,0,1
right0:
 .long 32767,65535,65536,2147483647
left1:
 .long -2147483648,-1,0,1
right1:
 .long 32767,65535,65536,2147483647
left2:
 .long -2147483648,-1,0,1
right2:
 .long 32767,65535,65536,2147483647
left3:
 .long 32767,65535,65536,2147483647
right3:
 .long -2147483648,-1,0,1
left4:
 .long 32767,65535,65536,2147483647
right4:
 .long -2147483648,-1,0,1
left5:
 .long 32767,65535,65536,2147483647
right5:
 .long -2147483648,-1,0,1
left6:
 .long 65534,65535,65536,65537
right6:
 .long -2,-65535,32768,42
left7:
 .long 65534,65535,65536,65537
right7:
 .long -2,-65535,32768,42
left8:
 .long 65534,65535,65536,65537
right8:
 .long -2,-65535,32768,42
left9:
 .long -2,-65535,32768,42
right9:
 .long 65534,65535,65536,65537
left10:
 .long -2,-65535,32768,42
right10:
 .long 65534,65535,65536,65537
left11:
 .long -2,-65535,32768,42
right11:
 .long 65534,65535,65536,65537
