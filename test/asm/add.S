#include "test/asm/mac.inc"
.globl	_start
_start:

//	make -j8 o//blink o//test/asm/add.elf
//	o//blink/blinkenlights o//test/asm/add.elf

	.test	"add64"
	mov	$0x1234567812345678,%rax
	mov	$0x0000000000000001,%rbx
	mov	$0x1234567812345679,%rcx
	add	%rbx,%rax
	.nz
	.nc
	.ns
	.no
	.np
	cmp	%rcx,%rax
	.e

	.test	"add32"
	mov	$0x1234567812345678,%rax
	mov	$0x0001000100000001,%rbx
	mov	$0x0000000012345679,%rcx
	mov	$0x0001000100000001,%rdx
	add	%ebx,%eax
	.nz
	.nc
	.ns
	.no
	.np
	cmp	%rcx,%rax
	.e
	cmp	%rdx,%rbx
	.e

	.test	"add16"
	mov	$0x1234567812345678,%rax
	mov	$0x0001000100010001,%rbx
	mov	$0x1234567812345679,%rcx
	mov	$0x0001000100010001,%rdx
	add	%bx,%ax
	.nz
	.nc
	.ns
	.no
	.np
	cmp	%rcx,%rax
	.e
	cmp	%rdx,%rbx
	.e

"test succeeded":
	.exit
