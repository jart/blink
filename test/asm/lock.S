#include "test/asm/mac.inc"
.globl	_start
_start:

//	make -j8 o//blink o//test/asm/lock.elf
//	o//blink/blinkenlights o//test/asm/lock.elf

	.test	"lock aligned"
	movl	$0,(%rsp)
	lock incl (%rsp)
	lock incl (%rsp)
	cmpl	$2,(%rsp)
	.e

	.test	"lock misaligned"
	movl	$0,1(%rsp)
	lock incl 1(%rsp)
	lock incl 1(%rsp)
	cmpl	$2,1(%rsp)
	.e

"test succeeded":
	.exit
