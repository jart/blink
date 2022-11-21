// -*- mode: unix-assembly; indent-tabs-mode: t; tab-width: 8; coding: utf-8 -*-

	.macro	.test name:req
prologue\@:
	jmp	101f
100:	int3
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
