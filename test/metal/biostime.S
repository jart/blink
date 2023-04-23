#include "test/metal/mac.inc"

	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/biostime.bin
//	o//blink/blinkenlights -r o//test/metal/biostime.bin

	ljmpw	$0,$1f
1:
	.test	"real time clock should not move much slower than system clock"
	mov	$5,%bp
2:
	mov	$2,%ah			// get starting RTC time
	int	$0x1a
	.nc
	mov	%cl,%dl
	mov	%dx,%si
	mov	$0,%ah			// get starting system time
	int	$0x1a
	push	%cx
	push	%dx
	pop	%edi
3:
	mov	$0,%ah			// wait until at least 20 system clock
	int	$0x1a			// ticks (slightly more than 1s)
	push	%cx			// have elapsed
	push	%dx
	pop	%edx
	cmp	$0x1800b0,%edx
	.c
	sub	%edi,%edx
	jnc	4f
	add	$0x1800b0,%edx
4:
	cmp	$20,%edx
	jb	3b
	cmp	$0x1800b0/2,%edx
	ja	1b			// lolwut?
	mov	$2,%ah			// get new RTC time
	int	$0x1a
	.nc
	mov	%cl,%dl
	cmp	%dx,%si			// RTC time should have changed by now
	.nz
	dec	%bp			// retry this 5 times
	jnz	2b

	.test	"system clock should not move much slower than real time clock"
5:
	mov	$2,%ah			// wait until RTC time rolls over
	int	$0x1a			// to next second
	.nc
	mov	%cl,%dl
	mov	%dx,%si
6:
	mov	$2,%ah
	int	$0x1a
	mov	%cl,%dl
	cmp	%dx,%si
	je	6b
	mov	$5,%bp
7:
	mov	$0,%ah			// get starting system time
	int	$0x1a
	push	%cx
	push	%dx
	pop	%edi
	mov	$2,%ah			// get starting RTC time
	int	$0x1a
	.nc
	mov	%cl,%dl
	mov	%dx,%si
8:
	mov	$2,%ah			// wait until RTC time rolls over
	int	$0x1a			// to next second
	.nc
	mov	%cl,%dl
	cmp	%dx,%si
	jz	8b
	mov	$0,%ah			// system time should have advanced
	int	$0x1a			// by at least 16 ticks by now
	push	%cx
	push	%dx
	pop	%edx
	sub	%edi,%edx
	jnc	9f
	add	$0x1800b0,%edx
9:
	cmp	$16,%edx
	.nc
	cmp	$0x1800b0/2,%edx
	ja	5b
	dec	%bp
	jnz	7b

	.exit
