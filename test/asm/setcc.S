#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$5,%r15
"test jit too":

//	condition code moves
//	make -j8 o//blink o//test/asm/setcc.elf && o//blink/tui o//test/asm/setcc.elf

	.test	"set carry reg true"
	or	$-1,%eax
	stc
	setc	%al
	cmp	$1,%al
	.e

	.test	"set carry reg false"
	or	$-1,%eax
	clc
	setc	%ah
	cmp	$0,%ah
	.e

	.test	"set carry reg doesn't extend"
	or	$-1,%eax
	stc
	setc	%al
	cmp	$0xffffff01,%eax
	.e

	.test	"set carry mem"
	movl	$-1,(%rsp)
	stc
	setc	(%rsp)
	cmpl	$0xffffff01,(%rsp)
	.e

	.test	"set carry reg true r14"
	stc
	setc	%r14b
	cmp	$1,%r14b
	.e

	sub	$1,%r15
	jnz	"test jit too"
"test succeeded":
	.exit
