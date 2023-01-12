#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$10,%r15
"test jit too":

//	sarx/shlx/shrx -- shift w/o flags
//	make -j8 o//blink o//test/asm/shx.elf
//	o//blink/blinkenlights o//test/asm/shx.elf

	mov	$7,%eax			# extended features
	xor	%ecx,%ecx
	cpuid
	bt	$8,%ebx			# bmi2
	jnc	"test not possible"

	.test	"shlx 1"
	stc
	mov	$0xf0000fff,%eax	# input value
	mov	$0x31337133,%ebx	# random data
	mov	$4,%ecx
	shlx	%ecx,%eax,%ebx
	.c
	cmp	$0xf0000fff,%eax	# unchanged
	.e
	cmp	$0x0000fff0,%ebx	# output value
	.e

	.test	"shrx 1"
	stc
	mov	$0xf0000fff,%eax	# input value
	mov	$0x31337133,%ebx	# random data
	mov	$4,%ecx
	shrx	%ecx,%eax,%ebx
	.c
	cmp	$0xf0000fff,%eax	# unchanged
	.e
	cmp	$0x0f0000ff,%ebx	# output value
	.e

	.test	"sarx 1"
	stc
	mov	$0xf0000fff,%eax	# input value
	mov	$0x31337133,%ebx	# random data
	mov	$4,%ecx
	sarx	%ecx,%eax,%ebx
	.c
	cmp	$0xf0000fff,%eax	# unchanged
	.e
	cmp	$0xff0000ff,%ebx	# output value
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
"test not possible":
	.exit
