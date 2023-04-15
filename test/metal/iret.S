	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/iret.bin
//	o//blink/blinkenlights -r o//test/metal/iret.bin

0:	ljmpw	$0,$1f

	RET_TO = 5215
	.subsection RET_TO
1:	xor	%ax,%ax
	mov	%ax,%ss
	mov	$0b,%sp
	mov	$0x414,%ax
	mov	%ax,%ds
	mov	$0b11100011,%ah
	sahf
	jns	fail
	jnz	fail
	jp	fail
	jnc	fail
	pushfl
	lcalll	$0x655,$11f-0x6550

	.subsection 2480
11:	mov	$0b01000010,%ah
	sahf
	js	fail
	jnz	fail
	jp	fail
	jc	fail
	iretl

	.subsection RET_TO
	jns	fail
	jnz	fail
	jp	fail
	jnc	fail
	cmp	$0b01000010,%ah
	jnz	fail

	mov	$0b10011111,%ah
	sahf
	jns	fail
	jz	fail
	jnp	fail
	jnc	fail
	pushfw
	lcallw	$0x554,$21f-0x5540

	.subsection 1555
21:	mov	$0b00111000,%ah
	sahf
	js	fail
	jz	fail
	jp	fail
	jc	fail
	iretw

	.subsection RET_TO
	jns	fail
	jz	fail
	jnp	fail
	jnc	fail
	cmp	$0b00111000,%ah
	jnz	fail

	cli
	xor	%edi,%edi
	mov	%di,%ds
	lidt	bad_idt
	mov	$231,%eax
	syscall				// this will triple fault on a real PC

fail:
	int3
	hlt
	jmp	fail

	.balign	8
bad_idt:
	.quad	0
