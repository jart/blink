#include "test/asm/mac.inc"
.globl	_start
_start:

//	make -j8 o//blink o//test/asm/enter.elf
//	o//blink/blinkenlights o//test/asm/enter.elf

#define MAGIC1		0x2b8cfa3db6a78c9a
#define MAGIC2		0x58b8
#define MAGIC3		0x225e
#define MAGIC4		0x4ea0

	.test	"enterq stores frame pointer of size 8"
	movabs	$MAGIC1,%rbx
	lea	-8-MAGIC2(%rsp),%rax
	lea	-8(%rsp),%rcx
	mov	%rsp,%rdx

	mov	%rbx,%rbp
	enterq	$MAGIC2,$0
	cmp	%rsp,%rax
	.e
	cmp	%rbp,%rcx
	.e
	cmp	(%rbp),%rbx
	.e

	.test	"leaveq restores frame pointer of size 8"
	leaveq
	cmp	%rdx,%rsp
	.e
	cmp	%rbx,%rbp
	.e

	.test	"enterw stores frame pointer of size 2"
	xor	%sp,%sp			// make sure %sp does not wrap
	sub	$8,%rsp			// around 64 KiB boundary

	mov	%rsp,%rbx
	mov	$MAGIC3,%bx
	lea	-2-MAGIC4(%rsp),%rax
	lea	-2(%rsp),%rcx
	mov	%rsp,%rdx

	mov	%rbx,%rbp
	enterw	$MAGIC4,$0
	cmp	%rsp,%rax
	.e
	cmp	%rbp,%rcx
	.e
	cmp	(%rbp),%bx
	.e

	.test	"leavew restores frame pointer of size 2"
	leavew
	cmp	%rdx,%rsp
	.e
	mov	%bx,%dx
	cmp	%rdx,%rbp
	.e

"test succeeded":
	.exit
