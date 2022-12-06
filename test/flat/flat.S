.globl	_start
_start:

//	make -j8 o//blink o//test/flat/flat.bin
//	o//blink/tui o//test/flat/flat.bin

	mov	$0,%edi
	mov	$231,%eax
	syscall
