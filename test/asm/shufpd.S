#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$3,%r15
"test jit too":

//	packed interleave shuffle of pairs of double floats
//	make -j8 o//blink o//test/asm/shufpd.elf
//	o//blink/blinkenlights o//test/asm/shufpd.elf

	.test	"shufpd $1 A==B"
	movaps	A,%xmm0
	shufpd	$1,%xmm0,%xmm0
	movq	%xmm0,%rax			# test lower half
	cmp	%rax,B+0
	.e
	punpckhqdq %xmm0,%xmm0			# test upper half
	movq	%xmm0,%rax
	cmp	%rax,B+8
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit

	.section .rodata
	.align	16
A:	.quad	1,2
B:	.quad	2,1
