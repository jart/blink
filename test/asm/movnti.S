#include "test/asm/mac.inc"
.globl _start
_start:
 sub $32,%rsp
 .test "movnti rax width 4 offset 8"
 mov $-1,%r10
 mov %r10,(%rsp)
 mov %r10,8(%rsp)
 mov %r10,16(%rsp)
 pxor %xmm0,%xmm0
 movabs $0x123456789abcdef0,%rax
 cmp %r10,%r10
 movnti %eax,8(%rsp)
 sfence
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 cmpl $0x9abcdef0,8(%rsp)
 .e
 .test "movnti rax width 4 offset 9"
 mov $-1,%r10
 mov %r10,(%rsp)
 mov %r10,8(%rsp)
 mov %r10,16(%rsp)
 pxor %xmm0,%xmm0
 movabs $0x123456789abcdef0,%rax
 cmp %r10,%r10
 movnti %eax,9(%rsp)
 sfence
 .e
 cmpb $-1,8(%rsp)
 .e
 cmpb $-1,13(%rsp)
 .e
 cmpl $0x9abcdef0,9(%rsp)
 .e
 .test "movnti rax width 8 offset 8"
 mov $-1,%r10
 mov %r10,(%rsp)
 mov %r10,8(%rsp)
 mov %r10,16(%rsp)
 pxor %xmm0,%xmm0
 movabs $0x123456789abcdef0,%rax
 cmp %r10,%r10
 movnti %rax,8(%rsp)
 sfence
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 movabs $0x123456789abcdef0,%r10
 cmp %r10,8(%rsp)
 .e
 .test "movnti rax width 8 offset 9"
 mov $-1,%r10
 mov %r10,(%rsp)
 mov %r10,8(%rsp)
 mov %r10,16(%rsp)
 pxor %xmm0,%xmm0
 movabs $0x123456789abcdef0,%rax
 cmp %r10,%r10
 movnti %rax,9(%rsp)
 sfence
 .e
 cmpb $-1,8(%rsp)
 .e
 cmpb $-1,17(%rsp)
 .e
 movabs $0x123456789abcdef0,%r10
 cmp %r10,9(%rsp)
 .e
 .test "movnti r9 width 4 offset 8"
 mov $-1,%r10
 mov %r10,(%rsp)
 mov %r10,8(%rsp)
 mov %r10,16(%rsp)
 pxor %xmm9,%xmm9
 movabs $0x123456789abcdef0,%r9
 cmp %r10,%r10
 movnti %r9d,8(%rsp)
 sfence
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 cmpl $0x9abcdef0,8(%rsp)
 .e
 .test "movnti r9 width 4 offset 9"
 mov $-1,%r10
 mov %r10,(%rsp)
 mov %r10,8(%rsp)
 mov %r10,16(%rsp)
 pxor %xmm9,%xmm9
 movabs $0x123456789abcdef0,%r9
 cmp %r10,%r10
 movnti %r9d,9(%rsp)
 sfence
 .e
 cmpb $-1,8(%rsp)
 .e
 cmpb $-1,13(%rsp)
 .e
 cmpl $0x9abcdef0,9(%rsp)
 .e
 .test "movnti r9 width 8 offset 8"
 mov $-1,%r10
 mov %r10,(%rsp)
 mov %r10,8(%rsp)
 mov %r10,16(%rsp)
 pxor %xmm9,%xmm9
 movabs $0x123456789abcdef0,%r9
 cmp %r10,%r10
 movnti %r9,8(%rsp)
 sfence
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 movabs $0x123456789abcdef0,%r10
 cmp %r10,8(%rsp)
 .e
 .test "movnti r9 width 8 offset 9"
 mov $-1,%r10
 mov %r10,(%rsp)
 mov %r10,8(%rsp)
 mov %r10,16(%rsp)
 pxor %xmm9,%xmm9
 movabs $0x123456789abcdef0,%r9
 cmp %r10,%r10
 movnti %r9,9(%rsp)
 sfence
 .e
 cmpb $-1,8(%rsp)
 .e
 cmpb $-1,17(%rsp)
 .e
 movabs $0x123456789abcdef0,%r10
 cmp %r10,9(%rsp)
 .e
 .exit
