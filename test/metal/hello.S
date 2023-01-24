	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/hello.bin
//	o//blink/blinkenlights -r o//test/metal/hello.bin

	ljmpw	$0,$1f
1:
	mov	%cs,%ax
	mov	%ax,%ds
	mov	$msg,%si
	cld
2:
	lodsb
	test	%al,%al
	jz	3f
	mov	$0x0e,%ah
	mov	$0x0007,%bx
	int	$0x10
	jmp	2b
3:
	cli
	lidt	bad_idt
	xor	%edi,%edi
	mov	$231,%eax
	syscall				// this will triple fault on a real PC

msg:
	.asciz	"Hello world!\r\n"

	.balign	8
bad_idt:
	.quad	0
