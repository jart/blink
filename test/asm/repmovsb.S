#include "test/asm/mac.inc"
.globl	_start
_start:

//	rep movsb
//	make -j8 o//blink o//test/asm/repmovsb.elf
//	o//blink/blinkenlights o//test/asm/repmovsb.elf

	.test	"rep movsb"
	or	$0,A1
	lea	A1+4,%rdi
	lea	A1,%rsi
	mov	$A2-A1-4,%rcx
	rep movsb
	lea	A1-1,%rdi
	lea	B1-1,%rsi
	mov	$A2-A1+2,%rcx
	repz cmpsb
	.e

	.test	"reverse rep movsb"
	or	$0,C1
	lea	C1+(C2-C1-4),%rdi
	lea	C1+(C2-C1-1),%rsi
	mov	$C2-C1-3,%rcx
	std
	rep movsb
	cld
	lea	C1-1,%rdi
	lea	D1-1,%rsi
	mov	$C2-C1+2,%rcx
	repz cmpsb
	.e

"test succeeded":
	.exit

	.data

//	test case 1
	.align	64
	.byte	0
	.align	64
A1:	.ascii	"hello world!"
A2:
	.align	64
B1:	.ascii	"hellhellhell"
B2:	.byte	0
	.align	64

//	test case 2
	.align	64
	.byte	0
	.align	64
C1:	.ascii	"hello world!"
C2:
	.align	64
D1:	.ascii	"ld!ld!ld!ld!"
D2:	.byte	0
	.align	64
