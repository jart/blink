#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$3,%r15
"test jit too":

//	make -j8 o//blink o//test/asm/movsreg.elf
//	o//blink/blinkenlights -j o//test/asm/movsreg.elf

	.test	"mov cs"
	mov	%cs,%eax
	mov	%al,%dl
	and	$3,%dl
	cmp	$3,%dl			// default cs should have RPL 3
	.e

	shr	$2,%ax			// default cs should not be null
	.ne

	.test	"mov ss"
	mov	%ss,%eax
	mov	%al,%dl
	and	$3,%dl
	cmp	$3,%dl			// default ss should have RPL 3
	.e

	shr	$2,%ax
	.ne				// default ss should not be null

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
