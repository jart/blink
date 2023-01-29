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

//	Test that we can load arbitrary values into the GDTR & IDTR, & that
//	the loaded base addresses are of the correct widths

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

//	Now load an actual GDT, switch to protected mode, & test that the
//	LSL instruction reads the correct limits from it

	lgdtw	good_gdtr
	mov	%cr0,%eax
	or	$1,%al
	mov	%eax,%cr0
	jmp	2f
2:

	mov	$GDT_DATA16_32K,%cx
	lsl	%ecx,%eax
	mov	%eax,out3a
	mov	$GDT_DATA16_4G,%cl
	lsl	%ecx,%eax
	mov	%eax,out3b
	mov	$GDT_DATA32_64K,%cl
	lsl	%ecx,%eax
	mov	%eax,out3c
	mov	$GDT_DATA32_384M,%cl
	lsl	%ecx,%eax
	mov	%eax,out3d

//	Back to real mode

	mov	%cr0,%eax
	and	$~1,%al
	mov	%eax,%cr0
	jmp	3f
3:

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
	.quad	0xf6f6008d858e9801	// expected result of 1st SGDT
	.quad	0xf6f6bd8d858e9801	// expected result of 2nd SGDT
	.quad	0xf6f6008fe8a862f1	// expected result of 1st SIDT
	.quad	0xf6f6b78fe8a862f1	// expected result of 2nd SIDT
	.long	0x00007fff		// expected result of 1st LSL
	.long	0xffffffff		// expected result of 2nd LSL
	.long	0x0000ffff		// expected result of 3rd LSL
	.long	0x17ffffff		// expected result of 4th LSL
	out_size = . - expect_out

good_gdtr:
	.short	good_gdt_end-good_gdt-1
	.long	good_gdt

good_gdt:
.quad	0b0000000000000000000000000000000000000000000000000000000000000000 # 0
.quad	0b0000000000000000100100100000000000000000000000000111111111111111 # 8
.quad	0b0000000010001111100100100000000000000000000000001111111111111111 #16
.quad	0b0000000001000000100100100000000000000000000000001111111111111111 #24
.quad	0b0000000011000001100100100000000000000000000000000111111111111111 #32
good_gdt_end:

	GDT_DATA16_32K	= 8
	GDT_DATA16_4G	= 16
	GDT_DATA32_64K	= 24
	GDT_DATA32_384M	= 32

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
out3a:	.skip	4
out3b:	.skip	4
out3c:	.skip	4
out3d:	.skip	4
