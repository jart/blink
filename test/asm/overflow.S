#include "test/asm/mac.inc"
.globl	_start
_start:	mov	$3,%r15
"test jit too":

//	make -j8 o//blink o//test/asm/overflow.elf
//	o//blink/blinkenlights o//test/asm/overflow.elf

////////////////////////////////////////////////////////////////////////////////
// ADD INSTRUCTION

	.test	"overflow add 64"
	xor	%eax,%eax
	mov	$0x7ffffffffffffffe,%rax
	add	$1,%rax
	.no
	add	$1,%rax
	.o

	.test	"overflow add 32"
	xor	%eax,%eax
	mov	$0x7ffffffe,%eax
	add	$1,%eax
	.no
	add	$1,%eax
	.o

	.test	"overflow add 16"
	xor	%ax,%ax
	mov	$0x7ffe,%ax
	add	$1,%ax
	.no
	add	$1,%ax
	.o

	.test	"overflow add lo"
	xor	%al,%al
	mov	$0x7e,%al
	add	$1,%al
	.no
	add	$1,%al
	.o

	.test	"overflow add hi"
	xor	%ah,%ah
	mov	$0x7e,%ah
	add	$1,%ah
	.no
	add	$1,%ah
	.o

////////////////////////////////////////////////////////////////////////////////
// SUB INSTRUCTION

	.test	"overflow sub 64"
	xor	%eax,%eax
	mov	$0x7ffffffffffffffe,%rax
	sub	$-1,%rax
	.no
	sub	$-1,%rax
	.o

	.test	"overflow sub 32"
	xor	%eax,%eax
	mov	$0x7ffffffe,%eax
	sub	$-1,%eax
	.no
	sub	$-1,%eax
	.o

	.test	"overflow sub 16"
	xor	%ax,%ax
	mov	$0x7ffe,%ax
	sub	$-1,%ax
	.no
	sub	$-1,%ax
	.o

	.test	"overflow sub lo"
	xor	%al,%al
	mov	$0x7e,%al
	sub	$-1,%al
	.no
	sub	$-1,%al
	.o

	.test	"overflow sub hi"
	xor	%ah,%ah
	mov	$0x7e,%ah
	sub	$-1,%ah
	.no
	sub	$-1,%ah
	.o

	dec	%r15
	jnz	"test jit too"
"test succeeded":
	.exit
