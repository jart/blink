#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$10,%r15
"test jit too":

//	cmp compares registers and memory
//	make -j8 o//blink o//test/asm/cmp.elf
//	o//blink/blinkenlights -j o//test/asm/cmp.elf

	.test	"cmp hi reg"
	mov	$3,%bh
	movb	$3,(%rsp)
	cmp	%bh,(%rsp)
	.e

	.test	"cmp jit test"
	mov	$3,%ecx
1:	mov	$1,%eax
	mov	$1,%ebx
	cmp	%eax,%ebx
	.e
	loop	1b

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
