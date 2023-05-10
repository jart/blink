#include "config.h"

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

#ifndef DISABLE_ROM
	mov	$0x3370/0x10,%ax
	mov	%ax,%ds
	mov	$0xf000+0xe920/0x10,%ax
	mov	%ax,%es
	mov	$0xfff0-0xe920,%di

	mov	%es:(%di),%eax		// make a copy of the CPU "power on
	mov	%eax,good_poweron-0x3370 // restart" ROM code at 0xf000:0xfff0
	mov	%es:4(%di),%eax
	mov	%eax,good_poweron+4-0x3370
	mov	%es:8(%di),%eax
	mov	%eax,good_poweron+8-0x3370
	mov	%es:12(%di),%eax
	mov	%eax,good_poweron+12-0x3370

	mov	$16,%cx			// try to overwrite 0xf000:0xfff0
	mov	$bad_poweron-0x3370,%si
	cld
	rep movsb

	mov	$0xf000+0xaa30/0x10,%ax	// check that the ROM is unchanged
	mov	%ax,%ds
	mov	$0x7340/0x10,%ax
	mov	%ax,%es
	mov	$0xfff0-0xaa30,%si
	mov	$good_poweron-0x7340,%di
	mov	$4,%cl
	repe cmpsl
	jne	fail

	dec	%si			// also check that the ROM does not
	dec	%si			// match the bad "power on" code we
	mov	$8,%cl			// tried to fill it with
	mov	$bad_poweron+(16-2)-0x7340,%di
	std
	cmp	%ax,%ax			// force ZF = 1 here as an extra test
	repe cmpsw
	je	fail

	mov	$0x06e0/0x10,%ax	// try to overwrite 0xf000:0xfff0,
	mov	%ax,%ds			// but backwards, shortword-wise,
	mov	$0xf000+0xe470/0x10,%ax	// & unaligned
	mov	%ax,%es
	mov	$0xfffd-0xe470,%di
	mov	$bad_poweron+14-0x06e0,%si
	mov	$7,%cx
	std
	rep movsw

	mov	$0xf000+0xb400/0x10,%ax	// check again that the ROM is
	mov	%ax,%ds			// unchanged
	mov	$0x1c90/0x10,%ax
	mov	%ax,%es
	mov	%es:good_poweron-0x1c90,%eax
	cmp	0xfff0-0xb400,%eax
	jnz	fail
	mov	%es:good_poweron+4-0x1c90,%eax
	cmp	0xfff4-0xb400,%eax
	jnz	fail
	mov	%es:good_poweron+8-0x1c90,%eax
	cmp	0xfff8-0xb400,%eax
	jnz	fail
	mov	%es:good_poweron+12-0x1c90,%eax
	cmp	0xfffc-0xb400,%eax
	jnz	fail
#endif /* !DISABLE_ROM */

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
bad_poweron:
	.quad	0x31bc50ea79cfccb5,0x11ade0c823d42d59
good_poweron:
	.skip	16
