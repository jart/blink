.globl	_start
_start:

//	make -j8 o//blink o//test/flat/flat.bin
//	o//blink/blink o//test/flat/flat.bin

	mov	$msg.len,%edx		# arg 3 = strlen(msg)
	lea	msg(%rip),%rsi		# arg 2 = msg
	mov	$1,%edi			# arg 1 = 1
	mov	$1,%eax			# __NR_write
	syscall

	mov	$0,%edi			# arg1 = 0
	mov	$231,%eax		# __NR_exit
	syscall

msg:	.ascii	"hello! i am a flat executable.\n"
	msg.len = . - msg
