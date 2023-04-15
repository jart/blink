#include "test/asm/mac.inc"
.globl	_start
_start:

//	make -j8 o//blink o//test/asm/nop.elf
//	o//blink/blinkenlights o//test/asm/nop.elf

	nop
	nopl	(%rax)
	nop
	nop
	nopl	(%rax)
	nop
	nopl	(%rax)
	nop
	nop

	push	$0
	mov	%rsp,%rdi
	xchg	(%rdi),%r12

	.test	"nop doesn't clear high bits of %rax"
	mov	$0x0102030405060708,%rax
	mov	%rax,%rbx
	nop
	cmp	%rbx,%rax
	.e

	.test	"rexb nop clears high bits and swaps with %r8d"
	mov	$0x7172737475767778,%r8
	rex.b
	nop
	mov	$0x0000000075767778,%rbx
	cmp	%rbx,%rax
	.e
	mov	$0x0000000005060708,%rbx
	cmp	%rbx,%r8
	.e

"test succeeded":
	.exit
