#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$10,%r15
"test jit too":

//	unsigned addition with special flags
//	make -j8 o//blink o//test/asm/adx.elf
//	o//blink/blinkenlights o//test/asm/adx.elf

	mov	$7,%eax			# extended features
	xor	%ecx,%ecx
	cpuid
	bt	$19,%ebx		# adx
	jnc	"test not possible"

	.test	"adcx"
	mov	$0xF0000000,%eax
	mov	$0x10000000,%ebx
	stc
	adcx	%ebx,%eax
	.c
	cmp	$0x00000001,%eax
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
"test not possible":
	.exit
