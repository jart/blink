#include "config.h"
#include "test/metal/mac.inc"

#define TF	(1 << 8)

	.section .head,"ax",@progbits
	.code16
.globl	_start
_start:

//	make -j8 o//blink o//test/metal/trap_tf.bin
//	o//blink/blinkenlights -r o//test/metal/trap_tf.bin

	xor	%ax,%ax
	mov	%ds,%ax
	mov	%ss,%ax
	mov	$_start,%esp

	.test	"trap flag"
	movw	$int1_isr,(0x01*4)
	movw	%cs,(0x01*4)+2
	movw	$int4_isr,(0x04*4)
	movw	%cs,(0x04*4)+2
	mov	$expect,%di
	pushfw
	pop	%ax
	or	$TF,%ax
	pushfw
	push	%ax
	pushfw
	push	%ax
	popfw
	cbtw
0:	mov	$0x35,%dh
1:	mov	$0xC7,%dl
2:	add	$0x80-0x35,%dh		// create an overflow condition
3:	into				// no int1 right after `into` taken
	add	$0x3C,%dh		// create a non-overflow condition
4:	into				// int1 right after `into` not taken
5:	nop
6:	int	$0x01
7:	popfw
8:	mov	%dl,%ah			// no int1 after, as TF = 0 from
	es popfw			// last popfw
	add	$-0xC7,%ah		// %ah := 0
9:	or	$-1,%cx
10:	int	$0x1A			// no int1 right after software int
	push	%cx
11:	push	%dx
12:	pop	%eax
13:	cmp	$0x1800B0,%eax
14:	lahf
15:	cs popfw
16:	sahf
	.c
	cmp	$expect.end,%di
	.e
	.exit

int1_isr:
	cmp	$expect.end-1,%di
	.c
	pop	%bp			// get caller %ip
	push	%bp
	cmp	%bp,(%di)		// compare with expected %ip
	.e
	inc	%di
	inc	%di
	iretw

int4_isr:
	iretw

	.align	2
expect:
	.hword	0b
	.hword	1b
	.hword	2b
	.hword	3b
	.hword	4b
	.hword	5b
	.hword	6b
	.hword	7b
	.hword	8b
	.hword	9b
	.hword	10b
	.hword	11b
	.hword	12b
	.hword	13b
	.hword	14b
	.hword	15b
	.hword	16b
expect.end:
