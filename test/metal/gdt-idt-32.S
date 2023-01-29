	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/gdt.bin
//	o//blink/blinkenlights -r o//test/metal/gdt.bin

	ljmpw	$0,$1f
1:
	mov	%cs,%ax
	cli
	mov	%ax,%ss
	mov	$_start,%sp
	mov	%ax,%ds
	mov	%ax,%es
	sidt	orig_idt

	cld
	mov	$actual_out,%di
	mov	$0xf6,%al
	mov	$out_size,%cx
	rep stosb

	lgdtw	dt1			// load GDTR, operand size 16 bits
					// => 24-bit base address
	sgdtw	out1a			// store GDTR: this will still dump a
					// 32-bit base address
	lgdtl	dt1			// load GDTR, operand size 32 bits
					// => 32-bit base address
	sgdtl	out1b			// store GDTR again

	lidtw	dt2			// now try messing with the IDTR
	sidtw	out2a
	lidtl	dt2
	sidtl	out2b

//	Compare actual output with expected output

	mov	$expect_out,%si
	mov	$actual_out,%di
	mov	$out_size,%cx
	repz cmpsb
	jnz	fail

//	Tests passed!  Try to "exit" emulator with success status

	cli
	lidt	bad_idt
	xor	%edi,%edi
	mov	$231,%eax
	syscall				// this will triple fault on a real PC

fail:
	lidt	orig_idt
	int3
	jmp	fail

	.balign	8
dt1:	.quad	0x5b1ebd8d858e9801
dt2:	.quad	0xc5a9b78fe8a862f1

expect_out:
	.quad	0xf6f6008d858e9801
	.quad	0xf6f6bd8d858e9801
	.quad	0xf6f6008fe8a862f1
	.quad	0xf6f6b78fe8a862f1
	out_size = . - expect_out

	.balign	8
bad_idt:
	.quad	0

	.bss

	.balign	8
orig_idt:
	.skip	8

actual_out:
out1a:	.skip	8
out1b:	.skip	8
out2a:	.skip	8
out2b:	.skip	8
