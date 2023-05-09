#include "config.h"
#include "test/metal/mac.inc"

	.section .head,"ax",@progbits
	.code16
.globl	_start
_start:

//	sse moves in bare metal mode
//	make -j8 o//blink o//test/metal/ssemov.bin
//	o//blink/blinkenlights -r o//test/metal/ssemov.bin

#define CR0_MP		(1<<1)
#define CR0_EM		(1<<2)
#define CR4_OSFXSR	(1<<9)
#define CR4_OSXMMEXCPT	(1<<10)

	.load16

	.test	"enable sse2"
	xor	%eax,%eax
	inc	%ax
	cpuid
	test	$1<<26,%edx		// test for sse2 capability
	.ne
	mov	%cr0,%eax		// enable sse
	and	$~CR0_EM,%al
	or	$CR0_MP,%al
	mov	%eax,%cr0
	mov	%cr4,%eax
	or	$CR4_OSFXSR|CR4_OSXMMEXCPT,%ax
	mov	%eax,%cr4

	.test	"sse mov unaligned"
	and	$-16,%sp
	sub	$17,%sp
	movdqu	onetwo,%xmm0
	movdqu	%xmm0,%xmm0
	movdqu	%xmm0,(%esp)
	mov	(%esp),%eax
	mov	4(%esp),%esi
	mov	8(%esp),%edx
	mov	12(%esp),%edi
	mov	$0x11111111,%ecx
	cmp	%ecx,%eax
	.e
	cmp	%ecx,%esi
	.e
	mov	$0x22222222,%ecx
	cmp	%ecx,%edx
	.e
	cmp	%ecx,%edi
	.e

	.test	"sse mov aligned"
	and	$-16,%sp
	sub	$16,%sp
	movdqa	onetwo,%xmm7
	movdqa	%xmm7,%xmm6
	movdqa	%xmm6,(%esp)
	mov	(%esp),%eax
	mov	4(%esp),%esi
	mov	8(%esp),%edx
	mov	12(%esp),%edi
	mov	$0x11111111,%ecx
	cmp	%ecx,%eax
	.e
	cmp	%ecx,%esi
	.e
	mov	$0x22222222,%ecx
	cmp	%ecx,%edx
	.e
	cmp	%ecx,%edi
	.e

	.test	"sse mov unaligned #2"
	and	$-16,%sp
	sub	$16,%sp
	movups	onetwo,%xmm7
	movups	%xmm7,%xmm6
	movups	%xmm6,(%esp)
	mov	(%esp),%eax
	mov	4(%esp),%esi
	mov	8(%esp),%edx
	mov	12(%esp),%edi
	mov	$0x11111111,%ecx
	cmp	%ecx,%eax
	.e
	cmp	%ecx,%esi
	.e
	mov	$0x22222222,%ecx
	cmp	%ecx,%edx
	.e
	cmp	%ecx,%edi
	.e
	jmp	2f

	.section .text,"ax",@progbits

2:
#ifndef DISABLE_ROM
	.test	"sse mov unaligned into ROM area"
	mov	$0xf000,%ax
	mov	%ax,%es
	mov	$0xfff0-1,%bx
	movdqu	%es:(%bx),%xmm3
	movdqu	%xmm3,saverom
	mov	%es:16(%bx),%al
	mov	%al,saverom+16
	movdqu	onetwo,%xmm1
	movdqu	%xmm1,%xmm2
	movdqu	%xmm2,%es:(%bx)
	movdqu	%es:(%bx),%xmm4
	pcmpeqb	%xmm4,%xmm1
	pmovmskb %xmm1,%eax
	inc	%ax
	.ne
	movdqu	%es:(%bx),%xmm5
	pcmpeqb	%xmm3,%xmm5
	pmovmskb %xmm5,%eax
	inc	%ax
	.e
	mov	%es:(%bx),%eax
	cmp	saverom,%eax
	.e
	mov	%es:4(%bx),%eax
	cmp	saverom+4,%eax
	.e
	mov	%es:8(%bx),%eax
	cmp	saverom+8,%eax
	.e
	mov	%es:12(%bx),%eax
	cmp	saverom+12,%eax
	.e

	.test	"sse mov aligned into ROM area"
	inc	%bx
	movdqa	onetwo,%xmm1
	movdqa	%xmm1,%xmm2
	movdqa	%xmm2,%es:(%bx)
	pcmpeqb	%es:(%bx),%xmm1
	pmovmskb %xmm1,%eax
	inc	%ax
	.ne
	movdqu	saverom+1,%xmm6
	pcmpeqb	%es:(%bx),%xmm6
	pmovmskb %xmm6,%eax
	inc	%ax
	.e
	mov	%es:(%bx),%eax
	cmp	saverom+1,%eax
	.e
	mov	%es:4(%bx),%eax
	cmp	saverom+1+4,%eax
	.e
	mov	%es:8(%bx),%eax
	cmp	saverom+1+8,%eax
	.e
	mov	%es:12(%bx),%eax
	cmp	saverom+1+12,%eax
	.e
#endif /* !DISABLE_ROM */

"test succeeded":
	.exit

	.align	16
onetwo:	.quad	0x1111111111111111,0x2222222222222222

.lcomm	saverom,17
