#include "test/asm/mac.inc"
.globl	_start
_start:

//	cmp compares registers and memory
//	make -j8 o//blink o//x86_64/test/asm/cmp.elf
//	o//blink/tui -j o//x86_64/test/asm/cmp.elf

	.test	"cmp jit test"
	mov	$3,%ecx
1:	mov	$1,%eax
	mov	$1,%ebx
	cmp	%eax,%ebx
	.e
	loop	1b

"test succeeded":
	.exit
