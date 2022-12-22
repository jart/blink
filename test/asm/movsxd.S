#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$5,%r15
"test jit too":

//	sign extending moves
//	make -j8 o//blink o//test/asm/movsxd.elf && o//blink/tui o//test/asm/movsxd.elf

	.test	"sign extend 32-bit to 32-bit"
	push	$-2147483648
	.byte	0x63,0x04,0x24			# movsll (%rsp),%eax
	movabs	$0x0000000080000000,%rcx
	cmp	%rcx,%rax
	.e

	.test	"sign extend 32-bit to 64-bit"
	push	$-2147483648
	movslq	(%rsp),%rax
	mov	$0xffffffff80000000,%rcx
	cmp	%rcx,%rax
	.e

	sub	$1,%r15
	jnz	"test jit too"
"test succeeded":
	.exit
