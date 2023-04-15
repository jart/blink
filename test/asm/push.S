#include "test/asm/mac.inc"
.globl	_start
_start:

//	make -j8 o//blink o//test/asm/push.elf
//	o//blink/blinkenlights o//test/asm/push.elf

	sub	$1,%rsp

	.test	"push moves stack by 8"
	mov	%rsp,%rax
	push	%rax
	mov	%rsp,%rbx
	sub	%rbx,%rax
	cmp	$8,%rax
	.e

	.test	"osz push moves stack by 2"
	mov	%rsp,%rax
	push	%ax
	mov	%rsp,%rbx
	sub	%rbx,%rax
	cmp	$2,%rax
	.e

	.test	"osz push changes two bytes"
	xor	%eax,%eax
	mov	%rsp,%rbp
	movl	$0xffffffff,-4(%rbp)
	push	%ax
	cmpl	$0x0000ffff,-4(%rbp)
	.e

	.test	"osz pop changes low two bytes only"
	or	$-1,%rax
	pop	%ax
	mov	$0xffffffffffff0000,%rbx
	cmp	%rbx,%rax
	.e

"test succeeded":
	.exit
