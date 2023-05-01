#include "test/metal/mac.inc"

	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/int3.bin
//	o//blink/blinkenlights -r o//test/metal/int3.bin

	ljmpw	$0,$1f
1:
	.test	"int3 should not halt execution if guest is hooking it"
	movw	$9f,%cs:(0x03*4)	// point int 3 vector at our own code
	movw	%cs,%cs:(0x03*4)+2
	int3				// then invoke int 3
2:					// if the above happens to fall
	ud2				// through to the next instruction,
	hlt				// force something really bad (not
	jmp	2b			// an int 3!) to happen

9:					// if things go well, execution
	.exit				// should reach here
