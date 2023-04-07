#include "test/asm/mac.inc"
.globl	_start
_start:

//	compare and exchange tests
//	make -j8 o//blink o//test/asm/cmpxchg16b.elf
//	o//blink/blinkenlights o//test/asm/cmpxchg16b.elf

	mov	$0,%rax
	mov	%rax,-16(%rsp)
	mov	%rax,-8(%rsp)

	.test	"lock cmpxchg16b 64 not taken"
//	if memory is equal to me
	mov	$0x5555555155555550,%rdx
	mov	$0x1234567812345678,%rax
//	replace it with me
	mov	$0x5500005155550050,%rcx
	mov	$0x1200067810000078,%rbx
	lock cmpxchg16b -16(%rsp)
	.nz
	cmp	$0,%rdx
	.z
	cmp	$0,%rax
	.z

	.test	"lock cmpxchg16b 64 taken"
//	if memory is equal to me
	mov	$0x5555555155555550,%rdx
	mov	$0x1234567812345678,%rax
	mov	%rax,-16(%rsp)
	mov	%rdx,-8(%rsp)
//	replace it with me
	mov	$0x5500005155550050,%rcx
	mov	$0x1200067810000078,%rbx
	lock cmpxchg16b -16(%rsp)
	.z
	cmp	%rbx,-16(%rsp)
	.z
	cmp	%rcx,-8(%rsp)
	.z

"test succeeded":
	.exit
