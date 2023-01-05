#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$10,%r15
"test jit too":

//	clear high bits
//	make -j8 o//blink o//test/asm/bzhi.elf
//	o//blink/blinkenlights o//test/asm/bzhi.elf

	.test	"bzhi 1"
	mov	$0xffffffff,%eax	# input value
	mov	$0x31337133,%ebx	# random data
	mov	$0xffffff08,%ecx	# clear bit 8 and above
	bzhi	%ecx,%eax,%ebx
	cmp	$0xffffffff,%eax	# unchanged
	.e
	cmp	$0x000000ff,%ebx	# output value
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
