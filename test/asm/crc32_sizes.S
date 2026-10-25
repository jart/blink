#include "test/asm/mac.inc"
.globl _start
_start:
 sub $16,%rsp
 .test "crc32 2 0 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981097406464,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32w %r8w,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3994929351,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 2 0 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981097406464,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32w (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3994929351,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 2 4294967295 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690985392373759,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32w %r8w,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3766698218,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 2 4294967295 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690985392373759,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32w (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3766698218,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 2 305419896 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981402826360,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32w %r8w,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3151294711,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 2 305419896 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981402826360,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32w (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3151294711,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 1 0 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981097406464,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32b %r8b,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $274646895,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 1 0 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981097406464,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32b (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $274646895,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 1 4294967295 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690985392373759,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32b %r8b,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3185339329,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 1 4294967295 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690985392373759,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32b (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3185339329,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 1 305419896 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981402826360,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32b %r8b,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3943563259,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 1 305419896 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981402826360,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32b (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $3943563259,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 4 0 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981097406464,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32l %r8d,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $4002185234,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 4 0 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981097406464,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32l (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $4002185234,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 4 4294967295 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690985392373759,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32l %r8d,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $1494496298,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 4 4294967295 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690985392373759,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32l (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $1494496298,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 4 305419896 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981402826360,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32l %r8d,%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $351852070,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 4 305419896 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981402826360,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32l (%rsp),%r9d
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $351852070,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 8 0 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981097406464,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32q %r8,%r9
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $769102820,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 8 0 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981097406464,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32q (%rsp),%r9
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $769102820,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 8 4294967295 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690985392373759,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32q %r8,%r9
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $1577115281,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 8 4294967295 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690985392373759,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32q (%rsp),%r9
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $1577115281,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 8 305419896 False"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981402826360,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32q %r8,%r9
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $1738407667,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .test "crc32 8 305419896 True"
 movabs $18364758544493064720,%r8
 mov %r8,(%rsp)
 movabs $16045690981402826360,%r9
 xor %eax,%eax
 stc
 pushfq
 pop %r10
 crc32q (%rsp),%r9
 pushfq
 pop %r11
 cmp %r10,%r11
 .e
 mov $1738407667,%eax
 cmp %rax,%r9
 .e
 movabs $18364758544493064720,%rax
 cmp %rax,%r8
 .e
 cmp %rax,(%rsp)
 .e
 .exit
