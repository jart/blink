#include "test/asm/mac.inc"
.globl	_start
_start:

//	make -j8 o//blink o//x86_64/test/asm/lahf.elf
//	o//blink/tui o//x86_64/test/asm/lahf.elf

	.test	"LAHF stores flags into AH"
	mov	$0x7fffffff,%eax
	add	$4,%eax
	.sop
	mov	$-1,%eax
	lahf

//	we need to remove AF from the result to be
//	consistent with the actual hardware result
//
//	                              cf ──┐
//	                              1 ──┐│
//	                            pf ──┐││
//	                            0 ──┐│││
//	                          af ──┐││││
//	                          0 ──┐│││││
//	                        zf ──┐││││││
//	                       sf ──┐│││││││
//	                            ││││││││
	and	$0b11111111111111111111011111111111,%eax
	cmp	$0b11111111111111111001011011111111,%eax
	.e

"test succeeded":
	.exit
