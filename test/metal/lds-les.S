	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/lds-les.bin
//	o//blink/blinkenlights -r o//test/metal/lds-les.bin

	ljmpw	$0,$1f

#define SEG1		0x66d
#define OFF1		(loc1-0x66d0)
#define MAGIC1		0x5bd3610b

#define SEG2		0x79e
#define OFF2		(loc2-0x79e0)
#define MAGIC2		0x358f8b2e

#define SEG3		0x541
#define OFF3		(loc3-0x5410)
#define MAGIC3		0xca55bb07

#define SEG4		0x29f
#define OFF4		(loc4-0x29f0)
#define MAGIC4		0xb9fca8dc

1:
	or	$-1,%ecx
	mov	%ecx,%ebx
	mov	%ecx,%esi
	mov	%ecx,%edi
	mov	%cx,%ds
	mov	%cx,%es

	lds	%cs:fp1,%bx
	cmp	$OFF1,%bx
	jnz	fail
	mov	%ds,%ax
	cmp	$SEG1,%ax
	jnz	fail
	cmpl	$MAGIC1,(%bx)
	jnz	fail

	les	%cs:fp2,%si
	cmp	$OFF2,%si
	jnz	fail
	mov	%es,%ax
	cmp	$SEG2,%ax
	jnz	fail
	cmpl	$MAGIC2,%es:(%si)
	jnz	fail

	lds	%cs:fp3,%edi
	cmp	$OFF3,%edi
	jnz	fail
	mov	%ds,%ax
	cmp	$SEG3,%ax
	jnz	fail
	cmpl	$MAGIC3,(%edi)
	jnz	fail

	les	%cs:fp4,%ebp
	cmp	$OFF4,%ebp
	jnz	fail
	mov	%es,%ax
	cmp	$SEG4,%ax
	jnz	fail
	cmpl	$MAGIC4,%es:(%ebp)
	jnz	fail

//	Tests passed!  Try to "exit" emulator with success status

	cli
	lidt	bad_idt
	xor	%edi,%edi
	mov	$231,%eax
	syscall				// this will triple fault on a real PC

fail:
	int3
	jmp	fail

	.balign	2
fp1:	.hword	OFF1,SEG1
fp2:	.hword	OFF2,SEG2
fp3:	.long	OFF3
	.hword	SEG3
fp4:	.long	OFF4
	.hword	SEG4

loc1:	.long	MAGIC1
loc2:	.long	MAGIC2
loc3:	.long	MAGIC3
loc4:	.long	MAGIC4

	.balign	8
bad_idt:
	.quad	0
