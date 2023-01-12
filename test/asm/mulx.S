#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$10,%r15
"test jit too":

//	unsigned multiply w/o affecting flags
//	make -j8 o//blink o//test/asm/mulx.elf
//	o//blink/blinkenlights o//test/asm/mulx.elf

	mov	$7,%eax			# extended features
	xor	%ecx,%ecx
	cpuid
	bt	$8,%ebx			# bmi2
	jnc	"test not possible"

	.test	"mulx 1"
	mov	$2,%eax
	mov	$3,%ebx
	mov	$4,%ecx
	mov	$5,%edx
	mulx	%eax,%ebx,%ecx
	cmp	$2,%eax		# unchanged
	.e
	cmp	$5,%edx		# unchanged
	.e
	cmp	$10,%ebx	# (2 * 5) & 0xffffffff
	.e
	cmp	$0,%ecx		# (2 * 5) >> 32
	.e

	.test	"mulx 2"
	mov	$2,%r10d
	mov	$3,%ebx
	mov	$4,%ecx
	mov	$5,%edx
	mulx	%r10d,%ebx,%ecx
	cmp	$2,%r10d	# unchanged
	.e
	cmp	$5,%edx		# unchanged
	.e
	cmp	$10,%ebx	# (2 * 5) & 0xffffffff
	.e
	cmp	$0,%ecx		# (2 * 5) >> 32
	.e

	.test	"mulx 3"
	mov	$2,%eax
	mov	$3,%r10d
	mov	$4,%ecx
	mov	$5,%edx
	mulx	%eax,%r10d,%ecx
	cmp	$2,%eax		# unchanged
	.e
	cmp	$5,%edx		# unchanged
	.e
	cmp	$10,%r10d	# (2 * 5) & 0xffffffff
	.e
	cmp	$0,%ecx		# (2 * 5) >> 32
	.e

	.test	"mulx 4"
	mov	$2,%eax
	mov	$3,%ebx
	mov	$4,%r10d
	mov	$5,%edx
	mulx	%eax,%ebx,%r10d
	cmp	$2,%eax		# unchanged
	.e
	cmp	$5,%edx		# unchanged
	.e
	cmp	$10,%ebx	# (2 * 5) & 0xffffffff
	.e
	cmp	$0,%r10d	# (2 * 5) >> 32
	.e

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
"test not possible":
	.exit
