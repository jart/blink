#include "test/asm/mac.inc"
.globl	_start
_start:

//	left rotate (rol) example
//	make -j8 o//blink o//x86_64/test/asm/rol.elf
//	o//blink/tui o//x86_64/test/asm/rol.elf

	.test	"rol1"
	stc
	mov	$1,%eax
	rol	$0,%eax
	.c
	cmp	$1,%eax
	.e

	.test	"rol2"
	clc
	mov	$1,%eax
	rol	$0,%eax
	.nc
	.no
	cmp	$1,%eax
	.e

	.test	"rol3"
	clc
	mov	$0x0000000080000000,%rax
	mov	$1,%cl
	rol	%cl,%rax
	.nc
	.no
	mov	$0x0000000100000000,%rcx
	cmp	%rcx,%rax
	.e

	.test	"rol4"
	clc
	mov	$0x0000000080000000,%rax
	rol	%rax
	.nc
	.no
	mov	$0x0000000100000000,%rcx
	cmp	%rcx,%rax
	.e

	.test	"rol5"
	mov	$1,%eax
	rol	$1,%eax
	.nc
	.no
	cmp	$2,%eax
	.e

	.test	"rol 32-bit by 1"
	mov	$0x8000000080000000,%rax
	rol	%eax
	.c
	.o
	mov	$0x0000000000000001,%rcx
	cmp	%rcx,%rax
	.e

	.test	"rol 16-bit doesn't zero extend"
	mov	$0x8000000000008000,%rax
	rol	%ax
	.c
	.o
	mov	$0x8000000000000001,%rcx
	cmp	%rcx,%rax
	.e

	.test	"rol 8-bit doesn't zero extend"
	mov	$0x8000000000000080,%rax
	rol	%al
	.c
	.o
	mov	$0x8000000000000001,%rcx
	cmp	%rcx,%rax
	.e

	.test	"rol 8-bit hi register"
	mov	$0x8000000000008007,%rax
	rol	%ah
	.c
	.o
	mov	$0x8000000000000107,%rcx
	cmp	%rcx,%rax
	.e

	.test	"rol 8-bit rex register"
	mov	$0x8000000000000080,%rdi
	rol	%dil
	.c
	.o
	mov	$0x8000000000000001,%rcx
	cmp	%rcx,%rdi
	.e

	.test	"rolq by 2"
	mov	$0b1000000000000000000000000000000000000000000000001000000000000111,%rax
	rol	$2,%rax
	.nc
	mov	$0b0000000000000000000000000000000000000000000000100000000000011110,%rcx
	cmp	%rcx,%rax
	.e

	.test	"rolq by 2 carry"
	mov	$0b0100000000000000000000000000000000000000000000001000000000000111,%rax
	rol	$2,%rax
	.c
	mov	$0b0000000000000000000000000000000000000000000000100000000000011101,%rcx
	cmp	%rcx,%rax
	.e

	.test	"rol64imm(1,1) == 2"
	mov	$1,%rax
	rol	$1,%rax
	cmp	$2,%rax
	.e

	.test	"rol32imm(0x1234ul<<32|(u32)INT_MIN,1) == 1"
	mov	$0x123480000000,%rax
	rol	$1,%eax
	cmp	$1,%rax
	.e

	.test	"rol32(0x1234ul<<32|(u32)INT_MIN,0x1234ul<<32|(u32)1) == 1"
	mov	$0x123480000000,%rax
	mov	$0x123400000001,%rcx
	rol	%cl,%eax
	cmp	$1,%rax
	.e
	mov	$0x123400000001,%rax
	cmp	%rax,%rcx
	.e

	.test	"prepare for rol flags test"
	mov	$0x7fffffff,%eax
	add	$4,%eax
	.sop
	.test	"rol32 doesn't change flags if count is zero (imm)"
	rol	$0,%eax
	.sop
	.test	"If the masked count is 0, the flags are not affected (32-bit imm)"
	rol	$0xffffffe0,%eax
	.sop
	.test	"If the masked count is 0, the flags are not affected (16-bit imm)"
	rol	$0xffffffe0,%ax
	.sop
	.test	"If the masked count is 0, the flags are not affected (8-bit imm)"
	rol	$0xffffffe0,%ax
	.sop
	.test	"rol32 doesn't change flags if count is zero (cl)"
	mov	$0,%cl
	rol	%cl,%eax
	.sop
	.test	"rol16 doesn't change flags if count is zero (imm)"
	rol	$0,%ax
	.sop
	.test	"rol16 doesn't change flags if count is zero (cl)"
	mov	$0,%cl
	rol	%cl,%ax
	.sop
	.test	"rol8 doesn't change flags if count is zero (imm)"
	rol	$0,%al
	.sop
	.test	"rol8 doesn't change flags if count is zero (cl)"
	mov	$0,%cl
	rol	%cl,%al
	.sop
	.test	"rolb doesn't change flags if count is zero (mem imm)"
	rolb	$0,(%rsp)
	.sop
	.test	"rolw doesn't change flags if count is zero (mem imm)"
	rolw	$0,(%rsp)
	.sop
	.test	"roll doesn't change flags if count is zero (mem imm)"
	roll	$0,(%rsp)
	.sop
	.test	"rolq doesn't change flags if count is zero (mem imm)"
	rolq	$0,(%rsp)
	.sop

"test succeeded":
	.exit
