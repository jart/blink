#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$3,%r15
"test jit too":

//	equivalent exchange
//	make -j8 o//blink o//test/asm/xchg.elf
//	o//blink/tui o//test/asm/xchg.elf

	.test	"xchgl reg/reg zero-extends"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	xchg	%eax,%ebx
	mov	$0x0000000012345678,%rcx
	mov	$0x0000000055555550,%rdx
	cmp	%rdx,%rax
	.e
	cmp	%rcx,%rbx
	.e

	.test	"xchgl mem/reg zero-extends"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rbx,(%rsp)
	xchg	%eax,(%rsp)
	mov	$0x5555555112345678,%rcx
	mov	$0x0000000055555550,%rdx
	cmp	%rdx,%rax
	.e
	cmp	%rcx,(%rsp)
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
