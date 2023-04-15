#include "test/asm/mac.inc"
.globl	_start
_start:

//	exit(0) example
//	make -j8 o//blink o//test/asm/exit.elf
//	o//blink/blinkenlights o//test/asm/exit.elf

#define SYS_write 1
#define SYS_exit  231
#define EBADF     9

	.test	"call write(1, msg, len)"
	mov	$1,%edi
	lea	1f(%rip),%rsi
	mov	$2f-1f,%edx
	mov	$SYS_write,%eax
	syscall
	cmp	$2f-1f,%rax
	.e

	.test	"write(-1, 0, 0) -> EBADF"
	mov	$-1,%edi
	mov	$0,%esi
	mov	$0,%edx
	mov	$SYS_write,%eax
	syscall
	cmp	$-EBADF,%rax
	.e

	.test	"call exit(0)"
	mov	$0,%edi
	mov	$SYS_exit,%eax
	syscall

1:	.ascii	"hello world\n"
2:
