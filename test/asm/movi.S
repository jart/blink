#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$3,%r15
"test jit too":

//	immediate moves
//	make -j8 o//blink o//test/asm/movi.elf
//	o//blink/blinkenlights o//test/asm/movi.elf

	.test	"mov imm reg 32"
	mov	$0x3133731337,%rax
	mov	$0x3133731337,%rdx
	mov	$0x31337,%eax
	mov	$0x31337,%edx
	push	$0x31337
	pop	%r8
	cmp	%r8,%rax
	.e
	cmp	%r8,%rdx
	.e

	.test	"mov imm reg 16"
	mov	$0x3133731337,%rax
	mov	$0x3133731337,%rdx
	mov	$0xdead,%ax
	mov	$0xdead,%dx
	mov	$0x313373dead,%r8
	cmp	%r8,%rax
	.e
	cmp	%r8,%rdx
	.e

	.test	"mov imm reg 8"
	mov	$0x3133731337,%rax
	mov	$0x3133731337,%rdx
	mov	$0xbb,%al
	mov	$0xcc,%dh
	mov	$0x31337313bb,%r8
	cmp	%r8,%rax
	.e
	mov	$0x313373cc37,%r8
	cmp	%r8,%rdx
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
