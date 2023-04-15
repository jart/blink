//	exit(0) example
//	make -j8 o//blink o//test/asm/exit.elf
//	o//blink/blinkenlights o//test/asm/exit.elf

	.globl	_start
_start:	mov	$0,%edi
	mov	$231,%eax
	syscall
