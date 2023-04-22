// -*- mode: unix-assembly; indent-tabs-mode: t; tab-width: 8; coding: utf-8 -*-

	.macro	.load32 entry:req
	ljmp	$0,$101f
101:	xor	%ax,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss
	mov	$_start,%sp
	int	$0x13			// reset boot device (%ah = 0)
	mov	$0x0200+_sectors-1,%ax	// read remaining sectors of this
	mov	$_start+512,%bx		// test case
	mov	$0x0002,%cx
	mov	$0,%dh
	int	$0x13
	jc	101b
	cli				// switch to protected mode
	lgdtl	103f
	mov	%cr0,%eax
	or	$1,%al
	mov	%eax,%cr0
	jmp	102f
102:	mov	$16,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss
	movzwl	0x0413,%esp		// start stack at base memory top
	shll	$10,%esp
	ljmpl	$8,$(\entry)		// really transition to 32-bit mode
	.balign	8
103:	.short	105f-104f-1
	.long	104f
	.balign	8
104:
.quad	0b0000000000000000000000000000000000000000000000000000000000000000 # 0
.quad	0b0000000011001111100110100000000000000000000000001111111111111111 # 8
.quad	0b0000000011001111100100100000000000000000000000001111111111111111 #16
105:
	.endm

	.macro	.exit
	cli
	lidtl	%cs:106f
	xor	%edi,%edi
	xor	%eax,%eax
	mov	$231,%al
	syscall
	.balign	8
106:	.quad	0
	.endm

	.macro	.test name:req
prologue\@:
	jmp	101f
100:	int3
	hlt
	jmp	100b
101:	nop
"\name":
	.endm

	.macro	.c
	jnc	100b
	.endm
	.macro	.nc
	jc	100b
	.endm

	.macro	.e
	jne	100b
	.endm
	.macro	.ne
	je	100b
	.endm

	.macro	.z
	jnz	100b
	.endm
	.macro	.nz
	jz	100b
	.endm

	.macro	.s
	jns	100b
	.endm
	.macro	.ns
	js	100b
	.endm

	.macro	.o
	jno	100b
	.endm
	.macro	.no
	jo	100b
	.endm

	.macro	.p
	jnp	100b
	.endm
	.macro	.np
	jp	100b
	.endm

	.macro	.sop
	.nz
	.s
	.nc
	.o
	.p
	.endm
