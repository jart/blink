#include "test/asm/mac.inc"
.globl _start
_start:
 sub $32,%rsp
 .test "pcmpgtq 0 register"
 movdqu left0(%rip),%xmm8
 movdqu right0(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $-1,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $0,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-1,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 1 memory"
 movdqu left1(%rip),%xmm8
 movdqu right1(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right1(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $-1,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $0,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-1,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 2 alias"
 movdqu left2(%rip),%xmm8
 movdqu right2(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $0,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-1,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 3 register"
 movdqu left3(%rip),%xmm8
 movdqu right3(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $-1,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-9223372036854775808,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $9223372036854775807,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 4 memory"
 movdqu left4(%rip),%xmm8
 movdqu right4(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right4(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $-1,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-9223372036854775808,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $9223372036854775807,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 5 alias"
 movdqu left5(%rip),%xmm8
 movdqu right5(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-9223372036854775808,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $9223372036854775807,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 6 register"
 movdqu left6(%rip),%xmm8
 movdqu right6(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $-1,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-9223372036854775808,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $1,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 7 memory"
 movdqu left7(%rip),%xmm8
 movdqu right7(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right7(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $-1,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-9223372036854775808,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $1,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 8 alias"
 movdqu left8(%rip),%xmm8
 movdqu right8(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-9223372036854775808,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $1,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 9 register"
 movdqu left9(%rip),%xmm8
 movdqu right9(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $-1,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-1,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $0,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 10 memory"
 movdqu left10(%rip),%xmm8
 movdqu right10(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right10(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $-1,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-1,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $0,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 11 alias"
 movdqu left11(%rip),%xmm8
 movdqu right11(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-1,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $0,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 12 register"
 movdqu left12(%rip),%xmm8
 movdqu right12(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $-1,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $4294967297,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $4294967296,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 13 memory"
 movdqu left13(%rip),%xmm8
 movdqu right13(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right13(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $-1,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $4294967297,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $4294967296,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 14 alias"
 movdqu left14(%rip),%xmm8
 movdqu right14(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $4294967297,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $4294967296,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 15 register"
 movdqu left15(%rip),%xmm8
 movdqu right15(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $-1,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $4294967296,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $4294967297,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 16 memory"
 movdqu left16(%rip),%xmm8
 movdqu right16(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right16(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $-1,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $4294967296,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $4294967297,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 17 alias"
 movdqu left17(%rip),%xmm8
 movdqu right17(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $4294967296,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $4294967297,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 18 register"
 movdqu left18(%rip),%xmm8
 movdqu right18(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $-1,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-4294967295,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-4294967296,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 19 memory"
 movdqu left19(%rip),%xmm8
 movdqu right19(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right19(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $-1,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-4294967295,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-4294967296,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 20 alias"
 movdqu left20(%rip),%xmm8
 movdqu right20(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-4294967295,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-4294967296,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 21 register"
 movdqu left21(%rip),%xmm8
 movdqu right21(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $-1,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-4294967296,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-4294967295,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 22 memory"
 movdqu left22(%rip),%xmm8
 movdqu right22(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right22(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $-1,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-4294967296,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-4294967295,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 23 alias"
 movdqu left23(%rip),%xmm8
 movdqu right23(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $-4294967296,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-4294967295,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 24 register"
 movdqu left24(%rip),%xmm8
 movdqu right24(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $42,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-42,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 25 memory"
 movdqu left25(%rip),%xmm8
 movdqu right25(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right25(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $42,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-42,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 26 alias"
 movdqu left26(%rip),%xmm8
 movdqu right26(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $42,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-42,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 27 register"
 movdqu left27(%rip),%xmm8
 movdqu right27(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm9,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $42,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-42,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 28 memory"
 movdqu left28(%rip),%xmm8
 movdqu right28(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq right28(%rip),%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $42,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-42,%rax
 cmp %rax,24(%rsp)
 .e
 .test "pcmpgtq 29 alias"
 movdqu left29(%rip),%xmm8
 movdqu right29(%rip),%xmm9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 pcmpgtq %xmm8,%xmm8
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 movdqu %xmm8,(%rsp)
 cmpq $0,0(%rsp)
 .e
 cmpq $0,8(%rsp)
 .e
 movdqu %xmm9,16(%rsp)
 movabs $42,%rax
 cmp %rax,16(%rsp)
 .e
 movabs $-42,%rax
 cmp %rax,24(%rsp)
 .e
 .exit
.section .rodata
.balign 16
left0:
 .quad -9223372036854775808,9223372036854775807
right0:
 .quad 0,-1
left1:
 .quad -9223372036854775808,9223372036854775807
right1:
 .quad 0,-1
left2:
 .quad -9223372036854775808,9223372036854775807
right2:
 .quad 0,-1
left3:
 .quad 0,-1
right3:
 .quad -9223372036854775808,9223372036854775807
left4:
 .quad 0,-1
right4:
 .quad -9223372036854775808,9223372036854775807
left5:
 .quad 0,-1
right5:
 .quad -9223372036854775808,9223372036854775807
left6:
 .quad -1,0
right6:
 .quad -9223372036854775808,1
left7:
 .quad -1,0
right7:
 .quad -9223372036854775808,1
left8:
 .quad -1,0
right8:
 .quad -9223372036854775808,1
left9:
 .quad -9223372036854775808,1
right9:
 .quad -1,0
left10:
 .quad -9223372036854775808,1
right10:
 .quad -1,0
left11:
 .quad -9223372036854775808,1
right11:
 .quad -1,0
left12:
 .quad 4294967296,4294967297
right12:
 .quad 4294967297,4294967296
left13:
 .quad 4294967296,4294967297
right13:
 .quad 4294967297,4294967296
left14:
 .quad 4294967296,4294967297
right14:
 .quad 4294967297,4294967296
left15:
 .quad 4294967297,4294967296
right15:
 .quad 4294967296,4294967297
left16:
 .quad 4294967297,4294967296
right16:
 .quad 4294967296,4294967297
left17:
 .quad 4294967297,4294967296
right17:
 .quad 4294967296,4294967297
left18:
 .quad -4294967296,-4294967295
right18:
 .quad -4294967295,-4294967296
left19:
 .quad -4294967296,-4294967295
right19:
 .quad -4294967295,-4294967296
left20:
 .quad -4294967296,-4294967295
right20:
 .quad -4294967295,-4294967296
left21:
 .quad -4294967295,-4294967296
right21:
 .quad -4294967296,-4294967295
left22:
 .quad -4294967295,-4294967296
right22:
 .quad -4294967296,-4294967295
left23:
 .quad -4294967295,-4294967296
right23:
 .quad -4294967296,-4294967295
left24:
 .quad 42,-42
right24:
 .quad 42,-42
left25:
 .quad 42,-42
right25:
 .quad 42,-42
left26:
 .quad 42,-42
right26:
 .quad 42,-42
left27:
 .quad 42,-42
right27:
 .quad 42,-42
left28:
 .quad 42,-42
right28:
 .quad 42,-42
left29:
 .quad 42,-42
right29:
 .quad 42,-42
