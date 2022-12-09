.globl	_start
_start:

//	exit(0) example
//	make -j8 o//blink o//test/asm/exit.elf
//	o//blink/tui o//test/asm/exit.elf

"call write(1, msg, len)":
	mov	$1,%edi
	lea	1f(%rip),%rsi
	mov	$2f-1f,%edx
	mov	$1,%eax
	syscall

"call exit(0)":
	mov	$0,%edi
	mov	$231,%eax
	syscall

1:	.ascii	"hello world\n"
2:
