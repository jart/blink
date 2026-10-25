#include "test/asm/mac.inc"
.globl _start
_start:
 .test "rep movsb self addr32=False reverse=False count=0"
 lea buffer(%rip),%rdi
 mov %rdi,%rsi
 mov $0,%rcx
 cld
 cmp %rax,%rax
 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+(0)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=False reverse=False count=1"
 lea buffer(%rip),%rdi
 mov %rdi,%rsi
 mov $1,%rcx
 cld
 cmp %rax,%rax
 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+(1)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=False reverse=False count=31"
 lea buffer(%rip),%rdi
 mov %rdi,%rsi
 mov $31,%rcx
 cld
 cmp %rax,%rax
 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+(31)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=False reverse=False count=8192"
 lea buffer(%rip),%rdi
 mov %rdi,%rsi
 mov $8192,%rcx
 cld
 cmp %rax,%rax
 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+(8192)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=False reverse=True count=0"
 lea buffer+8191(%rip),%rdi
 mov %rdi,%rsi
 mov $0,%rcx
 std
 cmp %rax,%rax
 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+8191+(0)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=False reverse=True count=1"
 lea buffer+8191(%rip),%rdi
 mov %rdi,%rsi
 mov $1,%rcx
 std
 cmp %rax,%rax
 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+8191+(-1)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=False reverse=True count=31"
 lea buffer+8191(%rip),%rdi
 mov %rdi,%rsi
 mov $31,%rcx
 std
 cmp %rax,%rax
 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+8191+(-31)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=False reverse=True count=8192"
 lea buffer+8191(%rip),%rdi
 mov %rdi,%rsi
 mov $8192,%rcx
 std
 cmp %rax,%rax
 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+8191+(-8192)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=True reverse=False count=0"
 lea buffer(%rip),%rdi
 mov %rdi,%rsi
 mov $0,%rcx
 cld
 cmp %rax,%rax
 addr32 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+(0)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=True reverse=False count=1"
 lea buffer(%rip),%rdi
 mov %rdi,%rsi
 mov $1,%rcx
 cld
 cmp %rax,%rax
 addr32 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+(1)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=True reverse=False count=31"
 lea buffer(%rip),%rdi
 mov %rdi,%rsi
 mov $31,%rcx
 cld
 cmp %rax,%rax
 addr32 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+(31)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=True reverse=False count=8192"
 lea buffer(%rip),%rdi
 mov %rdi,%rsi
 mov $8192,%rcx
 cld
 cmp %rax,%rax
 addr32 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+(8192)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=True reverse=True count=0"
 lea buffer+8191(%rip),%rdi
 mov %rdi,%rsi
 mov $0,%rcx
 std
 cmp %rax,%rax
 addr32 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+8191+(0)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=True reverse=True count=1"
 lea buffer+8191(%rip),%rdi
 mov %rdi,%rsi
 mov $1,%rcx
 std
 cmp %rax,%rax
 addr32 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+8191+(-1)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=True reverse=True count=31"
 lea buffer+8191(%rip),%rdi
 mov %rdi,%rsi
 mov $31,%rcx
 std
 cmp %rax,%rax
 addr32 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+8191+(-31)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "rep movsb self addr32=True reverse=True count=8192"
 lea buffer+8191(%rip),%rdi
 mov %rdi,%rsi
 mov $8192,%rcx
 std
 cmp %rax,%rax
 addr32 rep movsb
 .e
 cld
 test %rcx,%rcx
 .z
 lea buffer+8191+(-8192)(%rip),%rax
 cmp %rax,%rdi
 .e
 cmp %rax,%rsi
 .e
 .test "self copy preserves contents"
 lea buffer(%rip),%rsi
 lea expected(%rip),%rdi
 mov $8192,%rcx
 repe cmpsb
 .e
 .exit
.data
.balign 4096
buffer:
.fill 8192,1,0x5a
expected:
.fill 8192,1,0x5a
