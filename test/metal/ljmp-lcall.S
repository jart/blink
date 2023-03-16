	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/ljmp-lcall.bin
//	o//blink/blinkenlights -r o//test/metal/ljmp-lcall.bin

0:	ljmpw	$0,$1f
1:	xor	%ax,%ax
	mov	%ax,%ss
	mov	$0b,%sp
	ljmpw	*%cs:21f

	.subsection 1027
21:	.hword	11f-0x6aa0,0x6aa

	RET_TO = 4703
	.subsection RET_TO
11:	mov	$0x30e,%ax
	mov	%ax,%ds
	mov	$0xa9e,%bx
	lcalll	*22f-0xa9e-0x30e0(%bx)

	.subsection 2822
22:	.long	12f-0x26b0
	.hword	0x26b

	.subsection 2997
12:	add	$0x35fc-0x30e,%ax
	lretl

	.subsection RET_TO
	cmp	$0x35fc,%ax
	jnz	fail
	movl	$-0xafa8b409,%ecx
	ljmpl	*23f+0xafa8b409-0x30e0(%ecx)

	.subsection 367
23:	.long	13f-0x6480
	.hword	0x648

	RET_TO = 8148
	.subsection RET_TO
13:	mov	$0x2ab,%ax
	mov	%ax,%es
	movl	$-0x8f45a4df,%ebp
	lcallw	*%es:24f+0x8f45a4df-0x2ab0(%ebp)

	.subsection 7391
24:	.hword	14f-0x6820,0x682

	.subsection 141
14:	xor	%ebp,%ecx
	lretw

	.subsection RET_TO
	cmp	$(-0xafa8b409)^(-0x8f45a4df),%ecx
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
