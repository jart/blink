#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$10,%r15
"test jit too":

//	unsigned multiply w/o affecting flags
//	make -j8 o//blink o//test/asm/mulx.elf
//	o//blink/blinkenlights o//test/asm/mulx.elf

	.test	"mulx"
	mov	$2,%eax
	mov	$3,%ebx
	mov	$4,%ecx
	mov	$5,%edx
	mulx	%eax,%ebx,%ecx
	cmp	$2,%eax		# unchanged
	.e
	cmp	$5,%edx		# unchanged
	.e
	cmp	$10,%ebx	# (2 * 5) & 0xffffffff
	.e
	cmp	$0,%ecx		# (2 * 5) >> 32
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
