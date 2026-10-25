#include "test/asm/mac.inc"
.globl _start
_start:
 sub $32,%rsp
 .test "lock bts width=2 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btsw $15,8(%rsp)
 .nc
 movzwq 8(%rsp),%rax
 movabs $32768,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock bts width=2 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btsw %r9w,8(%rsp)
 .nc
 movzwq 8(%rsp),%rax
 movabs $32768,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock bts width=2 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $32768,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btsw $15,8(%rsp)
 .c
 movzwq 8(%rsp),%rax
 movabs $32768,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock bts width=2 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $32768,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btsw %r9w,8(%rsp)
 .c
 movzwq 8(%rsp),%rax
 movabs $32768,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock btr width=2 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btrw $15,8(%rsp)
 .nc
 movzwq 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock btr width=2 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btrw %r9w,8(%rsp)
 .nc
 movzwq 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock btr width=2 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $32768,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btrw $15,8(%rsp)
 .c
 movzwq 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock btr width=2 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $32768,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btrw %r9w,8(%rsp)
 .c
 movzwq 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock btc width=2 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btcw $15,8(%rsp)
 .nc
 movzwq 8(%rsp),%rax
 movabs $32768,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock btc width=2 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btcw %r9w,8(%rsp)
 .nc
 movzwq 8(%rsp),%rax
 movabs $32768,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock btc width=2 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $32768,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btcw $15,8(%rsp)
 .c
 movzwq 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock btc width=2 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $32768,%rax
 mov %ax,8(%rsp)
 mov $15,%r9
 lock btcw %r9w,8(%rsp)
 .c
 movzwq 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,10(%rsp)
 .e
 .test "lock bts width=4 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btsl $31,8(%rsp)
 .nc
 mov 8(%rsp),%eax
 movabs $2147483648,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock bts width=4 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btsl %r9d,8(%rsp)
 .nc
 mov 8(%rsp),%eax
 movabs $2147483648,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock bts width=4 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $2147483648,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btsl $31,8(%rsp)
 .c
 mov 8(%rsp),%eax
 movabs $2147483648,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock bts width=4 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $2147483648,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btsl %r9d,8(%rsp)
 .c
 mov 8(%rsp),%eax
 movabs $2147483648,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock btr width=4 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btrl $31,8(%rsp)
 .nc
 mov 8(%rsp),%eax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock btr width=4 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btrl %r9d,8(%rsp)
 .nc
 mov 8(%rsp),%eax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock btr width=4 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $2147483648,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btrl $31,8(%rsp)
 .c
 mov 8(%rsp),%eax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock btr width=4 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $2147483648,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btrl %r9d,8(%rsp)
 .c
 mov 8(%rsp),%eax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock btc width=4 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btcl $31,8(%rsp)
 .nc
 mov 8(%rsp),%eax
 movabs $2147483648,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock btc width=4 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btcl %r9d,8(%rsp)
 .nc
 mov 8(%rsp),%eax
 movabs $2147483648,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock btc width=4 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $2147483648,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btcl $31,8(%rsp)
 .c
 mov 8(%rsp),%eax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock btc width=4 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $2147483648,%rax
 mov %eax,8(%rsp)
 mov $31,%r9
 lock btcl %r9d,8(%rsp)
 .c
 mov 8(%rsp),%eax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,12(%rsp)
 .e
 .test "lock bts width=8 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btsq $63,8(%rsp)
 .nc
 mov 8(%rsp),%rax
 movabs $9223372036854775808,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock bts width=8 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btsq %r9,8(%rsp)
 .nc
 mov 8(%rsp),%rax
 movabs $9223372036854775808,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock bts width=8 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $9223372036854775808,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btsq $63,8(%rsp)
 .c
 mov 8(%rsp),%rax
 movabs $9223372036854775808,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock bts width=8 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $9223372036854775808,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btsq %r9,8(%rsp)
 .c
 mov 8(%rsp),%rax
 movabs $9223372036854775808,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock btr width=8 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btrq $63,8(%rsp)
 .nc
 mov 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock btr width=8 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btrq %r9,8(%rsp)
 .nc
 mov 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock btr width=8 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $9223372036854775808,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btrq $63,8(%rsp)
 .c
 mov 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock btr width=8 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $9223372036854775808,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btrq %r9,8(%rsp)
 .c
 mov 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock btc width=8 old=0 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btcq $63,8(%rsp)
 .nc
 mov 8(%rsp),%rax
 movabs $9223372036854775808,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock btc width=8 old=0 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $0,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btcq %r9,8(%rsp)
 .nc
 mov 8(%rsp),%rax
 movabs $9223372036854775808,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock btc width=8 old=1 form=imm"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $9223372036854775808,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btcq $63,8(%rsp)
 .c
 mov 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .test "lock btc width=8 old=1 form=reg"
 mov $-1,%rax
 mov %rax,(%rsp)
 mov %rax,8(%rsp)
 mov %rax,16(%rsp)
 movabs $9223372036854775808,%rax
 mov %rax,8(%rsp)
 mov $63,%r9
 lock btcq %r9,8(%rsp)
 .c
 mov 8(%rsp),%rax
 movabs $0,%r10
 cmp %r10,%rax
 .e
 cmpb $-1,7(%rsp)
 .e
 cmpb $-1,16(%rsp)
 .e
 .exit
