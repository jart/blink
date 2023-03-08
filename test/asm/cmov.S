#include "test/asm/mac.inc"
.globl	_start
_start:

//	make -j8 o//blink o//test/asm/cmov.elf
//	o//blink/blinkenlights o//test/asm/cmov.elf

	.test	"cmovq taken"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	$0x5555555155555550,%rcx
	mov	$0x5555555155555550,%rdx
	stc
	cmovc	%rbx,%rax
	cmp	%rcx,%rax
	.e
	cmp	%rdx,%rbx
	.e

	.test	"cmovl taken"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	$0x0000000055555550,%rcx
	mov	$0x5555555155555550,%rdx
	stc
	cmovc	%ebx,%eax
	cmp	%rcx,%rax
	.e
	cmp	%rdx,%rbx
	.e

	.test	"cmovl not taken clears upper half"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555555555555,%rbx
	mov	$0x0000000012345678,%rcx
	mov	$0x5555555555555555,%rdx
	clc
	cmovc	%ebx,%eax
	cmp	%rcx,%rax
	.e
	cmp	%rdx,%rbx
	.e

"test succeeded":
	.exit
