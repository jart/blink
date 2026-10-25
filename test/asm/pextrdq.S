#include "test/asm/mac.inc"
.globl _start
_start:
 sub $32,%rsp
 movabs $0x89abcdef01234567,%rax
 mov %rax,0(%rsp)
 movabs $0xfedcba9876543210,%rax
 mov %rax,8(%rsp)
 movdqu (%rsp),%xmm9
 .test "pextr4 lane 0 mem False high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $0,%xmm9,%r8d
 .e
 movabs $0x1234567,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 0 mem False high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $252,%xmm9,%r8d
 .e
 movabs $0x1234567,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 0 mem True high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $0,%xmm9,19(%rsp)
 .e
 movl 19(%rsp),%r8d
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,23(%rsp)
 .e
 movabs $0x1234567,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 0 mem True high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $252,%xmm9,19(%rsp)
 .e
 movl 19(%rsp),%r8d
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,23(%rsp)
 .e
 movabs $0x1234567,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 1 mem False high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $1,%xmm9,%r8d
 .e
 movabs $0x89abcdef,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 1 mem False high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $253,%xmm9,%r8d
 .e
 movabs $0x89abcdef,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 1 mem True high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $1,%xmm9,19(%rsp)
 .e
 movl 19(%rsp),%r8d
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,23(%rsp)
 .e
 movabs $0x89abcdef,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 1 mem True high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $253,%xmm9,19(%rsp)
 .e
 movl 19(%rsp),%r8d
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,23(%rsp)
 .e
 movabs $0x89abcdef,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 2 mem False high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $2,%xmm9,%r8d
 .e
 movabs $0x76543210,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 2 mem False high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $254,%xmm9,%r8d
 .e
 movabs $0x76543210,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 2 mem True high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $2,%xmm9,19(%rsp)
 .e
 movl 19(%rsp),%r8d
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,23(%rsp)
 .e
 movabs $0x76543210,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 2 mem True high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $254,%xmm9,19(%rsp)
 .e
 movl 19(%rsp),%r8d
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,23(%rsp)
 .e
 movabs $0x76543210,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 3 mem False high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $3,%xmm9,%r8d
 .e
 movabs $0xfedcba98,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 3 mem False high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $255,%xmm9,%r8d
 .e
 movabs $0xfedcba98,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 3 mem True high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $3,%xmm9,19(%rsp)
 .e
 movl 19(%rsp),%r8d
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,23(%rsp)
 .e
 movabs $0xfedcba98,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr4 lane 3 mem True high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrd $255,%xmm9,19(%rsp)
 .e
 movl 19(%rsp),%r8d
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,23(%rsp)
 .e
 movabs $0xfedcba98,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr8 lane 0 mem False high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrq $0,%xmm9,%r8
 .e
 movabs $0x89abcdef01234567,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr8 lane 0 mem False high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrq $254,%xmm9,%r8
 .e
 movabs $0x89abcdef01234567,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr8 lane 0 mem True high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrq $0,%xmm9,19(%rsp)
 .e
 movq 19(%rsp),%r8
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,27(%rsp)
 .e
 movabs $0x89abcdef01234567,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr8 lane 0 mem True high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrq $254,%xmm9,19(%rsp)
 .e
 movq 19(%rsp),%r8
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,27(%rsp)
 .e
 movabs $0x89abcdef01234567,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr8 lane 1 mem False high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrq $1,%xmm9,%r8
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr8 lane 1 mem False high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrq $255,%xmm9,%r8
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr8 lane 1 mem True high False"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrq $1,%xmm9,19(%rsp)
 .e
 movq 19(%rsp),%r8
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,27(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .test "pextr8 lane 1 mem True high True"
 mov $-1,%r8
 mov %r8,16(%rsp)
 mov %r8,24(%rsp)
 cmp %rax,%rax
 pextrq $255,%xmm9,19(%rsp)
 .e
 movq 19(%rsp),%r8
 cmpb $-1,18(%rsp)
 .e
 cmpb $-1,27(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,%r8
 .e
 movdqu %xmm9,(%rsp)
 movabs $0x89abcdef01234567,%rax
 cmp %rax,0(%rsp)
 .e
 movabs $0xfedcba9876543210,%rax
 cmp %rax,8(%rsp)
 .e
 .exit
