#include "test/asm/mac.inc"
.globl	_start
_start:

//	memory page tests
//	make -j8 o//blink o//x86_64/test/asm/page.elf
//	o//blink/tui o//x86_64/test/asm/page.elf

	sub	$8,%rsp
	and	$-4096,%rsp

	.test	"read/write page overlap"
	mov	$0x1234567812345678,%rax
	mov	%rax,-1(%rsp)
	mov	-1(%rsp),%rbx
	cmp	%rbx,%rax
	.e

"test succeeded":
	.exit
