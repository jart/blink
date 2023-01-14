#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$10,%r15
"test jit too":

//	non-temporal move
//	make -j8 o//blink o//test/asm/movntdq.elf
//	o//blink/blinkenlights o//test/asm/movntdq.elf

	.test	"movntdq"
	movdqa	data,%xmm12
	movntdq	%xmm12,(%rsp)
	cmpq	$123,(%rsp)
	.e
	cmpq	$456,8(%rsp)
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit

	.align	16
data:	.quad	123
	.quad	456
