#include "test/metal/mac.inc"

	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/enter.bin
//	o//blink/blinkenlights -r o//test/metal/enter.bin

	.load32	_start32

	.section .text,"ax",@progbits
	.code32

#define MAGIC1		0xacc31274
#define MAGIC2		0xa7fc
#define MAGIC3		0x40df
#define MAGIC4		0x8c34

_start32:
	.test	"enterl stores frame pointer of size 4"
	mov	$MAGIC1,%ebx
	lea	-4-MAGIC2(%esp),%eax
	lea	-4(%esp),%ecx
	mov	%esp,%edx

	mov	%ebx,%ebp
	enterl	$MAGIC2,$0
	cmp	%esp,%eax
	.e
	cmp	%ebp,%ecx
	.e
	cmp	(%ebp),%ebx
	.e

	.test	"leavel restores frame pointer of size 4"
	leavel
	cmp	%edx,%esp
	.e
	cmp	%ebx,%ebp
	.e

	.test	"enterw stores frame pointer of size 2"
	xor	%sp,%sp			// make sure %sp does not wrap
	sub	$4,%esp			// around 64 KiB boundary

	mov	%esp,%ebx
	mov	$MAGIC3,%bx
	lea	-2-MAGIC4(%esp),%eax
	lea	-2(%esp),%ecx
	mov	%esp,%edx

	mov	%ebx,%ebp
	enterw	$MAGIC4,$0
	cmp	%esp,%eax
	.e
	cmp	%ebp,%ecx
	.e
	cmp	(%ebp),%bx
	.e

	.test	"leavew restores frame pointer of size 2"
	leavew
	cmp	%edx,%esp
	.e
	mov	%bx,%dx
	cmp	%edx,%ebp
	.e

"test succeeded":
	.exit
