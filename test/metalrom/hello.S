#include "blink/biosrom.h"

.macro	hvcall	callno:req
 .if	(\callno) == 0
	.byte	0x0F
	.byte	0xFF
	.byte	0077
 .else
   .if	(\callno) >= -0x80 && (\callno) <= 0x7f
	.byte	0x0F
	.byte	0xFF
	.byte	0177
	.byte	(\callno)
   .else
	addr16
	.byte	0x0F
	.byte	0xFF
	.byte	0277
	.short	(\callno)
   .endif
 .endif
.endm

	.section .text,"ax",@progbits
	.code16

//	make -j8 o//blink o//test/metalrom/hello.bin
//	o//blink/blinkenlights -r -B o//test/metalrom/hello.bin \
//	  third_party/sectorlisp/sectorlisp-friendly.bin

_start_1:
	mov	%cs,%ax
	mov	%ax,%ds

	cmp	$kBiosSeg,%ax		// test that %cs is sane
	jnz	fail

	cmp	$-4,%sp			// test that the stack pointer is sane
	ja	fail

	call	1f			// test that %ip is sane
1:
	pop	%ax
	cmp	$1b,%ax
	jnz	fail

	pop	%ax			// test that we arrived here from
	pop	%dx			// the call near linear address 0xFFFF0
	movzwl	%ax,%eax		// (per visible value of caller %cs)
	movzwl	%dx,%edx
	shl	$4,%edx
	add	%edx,%eax
	cmp	$kBiosBase+9f,%eax
	jnz	fail

	mov	$0x40000000,%eax	// test that we are running under a
	cltd				// Blink VM
	xor	%ebx,%ebx
	xor	%ecx,%ecx
	cpuid
	xor	$0x322809ae,%ebx
	cmp	$('G' | 'e' << 8 | 'n' << 16 | 'u' << 24) ^ 0x322809ae,%ebx
	jnz	fail
	xor	$0xd1b18562,%ecx
	cmp	$('i' | 'n' << 8 | 'e' << 16 | 'B' << 24) ^ 0xd1b18562,%ecx
	jnz	fail
	xor	$0x7421628a,%edx
	cmp	$('l' | 'i' << 8 | 'n' << 16 | 'k' << 24) ^ 0x7421628a,%edx
	jnz	fail

	mov	$hello,%si		// say hello
	cld
2:
	lodsb
	test	%al,%al
	jz	3f
	out	%al,$0xE9
	mov	$0x0e,%ah
	mov	$0x0007,%bx
	hvcall	0x10
	jmp	2b

3:
	cli				// report success
	lidtw	%cs:bad_idtr
	xor	%edi,%edi
	mov	$231,%eax
	syscall				// this will triple fault on a real PC

fail:
	cli
	hlt
	jmp	fail

	.section .rodata,"a",@progbits

	.balign	8
bad_idtr:
	.quad	0
hello:
	.asciz	"Hello world!\r\n"

	.section .fixedaddr.0xfff0,"ax",@progbits
.globl	_start
_start:
	cli
	xor	%esp,%esp
	mov	%sp,%ss
	lcallw	$kBiosSeg,$_start_1
9:
