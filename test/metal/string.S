	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/string.bin
//	o//blink/blinkenlights -r o//test/metal/string.bin

0:	ljmpw	$0,$1f

ehead	= 0b+0x200

1:	xor	%ax,%ax
	mov	%ax,%ss
	mov	$ehead+0x1000,%sp

	orl	$-1,%ss:ehead		// place something that is not the
					// same as our first instruction just
					// after our loaded boot sector

	mov	$0x41a0/0x10,%ax
	mov	%ax,%ds
	sub	$0x200/0x10,%ax
	mov	%ax,%es
	mov	$ehead-1-0x41a0,%si	// try to make 9 copies of our own
	mov	%si,%di			// code, by copying backwards
	mov	$9*0x200,%cx		// starting from 0x7c00+0x200-1
	std
	rep movsb

	mov	$0x1250/0x10,%ax	// check there are now exactly 10
	mov	%ax,%ds			// copies of the same boot code
	mov	$0x27e0/0x10,%ax
	mov	%ax,%es
	mov	$-1,%cx
	mov	$0b-0x1250-9*0x200,%si
	mov	$0b-0x27e0-8*0x200,%di
	cld
	repe cmpsw
	je	fail
	cmp	$-2-9*0x200/2,%cx
	jne	fail

	cli				// test succeeded
	xor	%edi,%edi
	mov	%di,%ds
	lidt	bad_idt
	mov	$231,%eax
	syscall				// this will triple fault on a real PC

fail:
	ud2
	hlt
	jmp	fail

	.balign	8
bad_idt:
	.quad	0
