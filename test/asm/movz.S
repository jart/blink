#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$3,%r15
"test jit too":

//	zero-extending moves
//	make -j8 o//blink o//test/asm/cmp.elf
//	o//blink/blinkenlights -j o//test/asm/cmp.elf

	.test	"movzbl reg"
	mov	$0x0102030405060708,%rax
	movzbl	%al,%eax
	mov	$0x0000000000000008,%rbx
	cmp	%rbx,%rax
	.e

	.test	"movzwl reg"
	mov	$0x0102030405060708,%rax
	movzwl	%ax,%eax
	mov	$0x0000000000000708,%rbx
	cmp	%rbx,%rax
	.e

	.test	"movzwl mem"
	mov	$0x0102030405060708,%rax
	mov	%rax,(%rsp)
	or	$-1,%rax
	movzwl	(%rsp),%eax
	mov	$0x0000000000000708,%rbx
	cmp	%rbx,%rax
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
