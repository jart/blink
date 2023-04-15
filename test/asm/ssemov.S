#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$5,%r15
"test jit too":

//	sse moves
//	make -j8 o//blink o//test/asm/ssemov.elf
//	o//blink/blinkenlights o//test/asm/ssemov.elf

	.test	"sse mov unaligned"
	and	$-16,%rsp
	sub	$17,%rsp
	movdqu	onetwo(%rip),%xmm0
	movdqu	%xmm0,%xmm0
	movdqu	%xmm0,(%rsp)
	mov	(%rsp),%rax
	mov	8(%rsp),%rdx
	mov	$0x1111111111111111,%rcx
	cmp	%rcx,%rax
	mov	$0x2222222222222222,%rcx
	cmp	%rcx,%rdx

	.test	"sse mov aligned"
	and	$-16,%rsp
	sub	$16,%rsp
	movdqa	onetwo(%rip),%xmm9
	movdqa	%xmm9,%xmm6
	movdqa	%xmm6,(%rsp)
	mov	(%rsp),%rax
	mov	8(%rsp),%rdx
	mov	$0x1111111111111111,%rcx
	cmp	%rcx,%rax
	mov	$0x2222222222222222,%rcx
	cmp	%rcx,%rdx

	.test	"sse mov unaligned #2"
	and	$-16,%rsp
	sub	$16,%rsp
	movups	onetwo(%rip),%xmm9
	movups	%xmm9,%xmm6
	movups	%xmm6,(%rsp)
	mov	(%rsp),%rax
	mov	8(%rsp),%rdx
	mov	$0x1111111111111111,%rcx
	cmp	%rcx,%rax
	mov	$0x2222222222222222,%rcx
	cmp	%rcx,%rdx

	sub	$1,%r15
	jnz	"test jit too"
"test succeeded":
	.exit

	.section .rodata
	.align	16
onetwo:	.quad	0x1111111111111111,0x2222222222222222
