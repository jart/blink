#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$3,%r15
"test jit too":

//	extract floating point sign mask
//	make -j8 o//blink o//test/asm/movmskpd.elf
//	o//blink/tui o//test/asm/movmskpd.elf

	.test	"movmskpd neg neg"
	movaps	negneg(%rip),%xmm0
	movmskpd %xmm0,%eax
	cmp	$0b11,%eax
	.e

	.test	"movmskpd neg pos"
	movaps	negpos(%rip),%xmm0
	movmskpd %xmm0,%eax
	cmp	$0b01,%eax
	.e

	.test	"movmskpd pos pos"
	movaps	pospos(%rip),%xmm0
	movmskpd %xmm0,%eax
	cmp	$0b00,%eax
	.e

	.test	"movmskps neg neg neg neg"
	movaps	nnnn(%rip),%xmm0
	movmskps %xmm0,%eax
	cmp	$0b1111,%eax
	.e

	.test	"movmskps neg pos neg pos"
	movaps	npnp(%rip),%xmm0
	movmskps %xmm0,%eax
	cmp	$0b0101,%eax
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit

	.section .rodata
	.align	16
negneg:	.double	-1.1, -2.2
negpos:	.double	-1.1, +2.2
pospos:	.double	+1.1, +2.2
nnnn:	.float	-1.1, -2.2, -2.2, -4.2
npnp:	.float	-1.1, +2.2, -2.2, +4.2
