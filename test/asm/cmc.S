#include "test/asm/mac.inc"
.globl	_start
_start:

//	complement carry flag tests
//	make -j8 o//blink o//test/asm/page.elf
//	o//blink/tui o//test/asm/page.elf

	.test	"carry flag manipulation"
	stc
	.c
	clc
	.nc
	cmc
	.c
	cmc
	.nc
	stc
	.c

"test succeeded":
	.exit
