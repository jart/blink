#include "test/metal/mac.inc"

	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/biosdisk.bin
//	o//blink/blinkenlights -r o//test/metal/biosdisk.bin

	ljmpw	$0,$1f
1:
	.test	"int 0x1E diskette parameter table should contain sane values"
	lds	%cs:(0x1e*4),%bx	// %ds:%bx := table pointer
	mov	%ds,%ax			// test that pointer is not null
	or	%bx,%ax
	.ne
	cmpb	$2,3(%bx)		// test that diskette I/O is using
	.e				// 512 bytes per sector
	cmpb	$8,4(%bx)		// ...& at least 8 sectors per track
	.nc

	test	%dl,%dl			// skip next test in unlikely case
	jne	2f			// this was not booted from floppy
					// drive A:

	.test	"floppy parameters from int 0x13 should be sane"
	xor	%di,%di			// get parameters for A:
	mov	%di,%es
	mov	$8,%ah
	xor	%cx,%cx
	int	$0x13
	.nc
	cmp	$8,%cl			// test that maximum sector number
	.nc				// per track makes sense
	cmp	$40-1,%ch		// test that maximum track number
	.nc				// makes sense
	test	%dl,%dl			// test that drive count makes sense
	.ne
	mov	%es,%ax			// test that parameter table pointer
	or	%di,%ax			// is not null
	.ne
	cmpb	$2,%es:3(%di)		// test parameter table values for
	.e				// this drive
	cmpb	$8,%es:4(%di)
	.nc

2:
	.exit

// fabricate a 160 KiB disk image (40 tracks × 1 side × 8 sectors per track)

	.global	_sectors
	_sectors = 320

	.section .pad,"a",@progbits
