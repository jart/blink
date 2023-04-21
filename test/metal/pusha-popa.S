	.section .head,"ax",@progbits
	.code16

.globl	_start
_start:

//	make -j8 o//blink o//test/metal/pusha-popa.bin
//	o//blink/blinkenlights -r o//test/metal/pusha-popa.bin

0:	ljmpw	$0,$1f

1:	xor	%ax,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss

	/* longword with high half being z & low half being w */
#define SMUSH(z, w)	((((z) & 0xffff) << 16) | ((w) & 0xffff))

#define MAGIC0		0x84de
#define INI_SP		0x7c00
#define INI_ESP		SMUSH(MAGIC0, INI_SP)

	mov	$INI_ESP,%esp

	int	$0x13			// reset boot device (%ah = 0)
	mov	$0x0200+_sectors-1,%ax	// read remaining sectors of this
	mov	$0b+512,%bx		// test case
	mov	$0x0002,%cx
	mov	$0,%dh
	int	$0x13
	jc	1b

#define MAGIC1		0x6f692002
#define MAGIC2		0x8556ab1b
#define MAGIC3		0x1bc6f7f7
#define MAGIC4		0x60540962
#define MAGIC5		0xf2b8c5fd
#define MAGIC6		0x705a9a7f
#define MAGIC7		0x08500349
#define MAGIC8		0xdf7601c3

	mov	$MAGIC1,%eax
	mov	$MAGIC2,%ecx
	mov	$MAGIC3,%edx
	mov	$MAGIC4,%ebx
	mov	$MAGIC5,%ebp
	mov	$MAGIC6,%esi
	mov	$MAGIC7,%edi
	pushal
/*
 * now %sp                                                      INI_SP
 * ↓       +4      +8      +12     +16     +20     +24     +28     ↓
 * ┌───────┬───────┬───────┬───────┬───────┬───────┬───────┬───────┐
 * │MAGIC7 │MAGIC6 │MAGIC5 │INI_ESP│MAGIC4 │MAGIC3 │MAGIC2 │MAGIC1 │
 * └───────┴───────┴───────┴───────┴───────┴───────┴───────┴───────┘
 *   %edi    %esi    %ebp   initial  %edx    %ecx    %ebx    %eax
 *                           %esp
 */

	cmp	$INI_ESP-32,%esp
	jnz	fail
	mov	$MAGIC8,%ebp
	mov	%sp,%bp
	cmpl	$MAGIC1,28(%bp)
	jnz	fail
	cmpl	$MAGIC2,24(%bp)
	jnz	fail
	cmpl	$MAGIC3,20(%bp)
	jnz	fail
	cmpl	$MAGIC4,16(%bp)
	jnz	fail
	cmpl	$INI_ESP,12(%bp)
	jnz	fail
	cmpl	$MAGIC5,8(%bp)
	jnz	fail
	cmpl	$MAGIC6,4(%bp)
	jnz	fail
	cmpl	$MAGIC7,(%bp)
	jnz	fail

#define MAGIC9		0x63b5007e

	mov	$MAGIC9,%edi
	addr32 popaw  // the `addr32' (0x67) prefix should have no effect!
/*
 *                              now %sp                         INI_SP
 *                                 ↓                               ↓
 * ┌───┬───┬───┬───┬───┬───┬───┬───┬───────┬───────┬───────┬───────┐
 * │MAGIC7 │MAGIC6 │MAGIC5 │INI_ESP│MAGIC4 │MAGIC3 │MAGIC2 │MAGIC1 │
 * └───┴───┴───┴───┴───┴───┴───┴───┴───────┴───────┴───────┴───────┘
 *  %di %si %bp     %bx %dx %cx %ax
 */

	/* high half of x combined with low half of y */
#define HL(x, y)	(((x) & ~0xffff) | ((y) & 0xffff))

	cmp	$HL(MAGIC1, MAGIC0),%eax
	jnz	fail
	cmp	$HL(MAGIC2, INI_ESP),%ecx
	jnz	fail
	cmp	$HL(MAGIC3, MAGIC5 >> 16),%edx
	jnz	fail
	cmp	$HL(MAGIC4, MAGIC5),%ebx
	jnz	fail
	cmp	$INI_ESP-16,%esp
	jnz	fail
	cmp	$HL(MAGIC8, MAGIC6),%ebp
	jnz	fail
	cmp	$HL(MAGIC6, MAGIC7 >> 16),%esi
	jnz	fail
	cmp	$HL(MAGIC9, MAGIC7),%edi
	jnz	fail

#define MAGIC10		0x35ff
#define MAGIC11		0x45de
#define MAGIC12		0xe0e4
#define MAGIC13		0x43ab
#define MAGIC14		0x44819457
#define MAGIC15		0x92d11944
#define MAGIC16		0xf5136466

	mov	$MAGIC10,%ax
	mov	$MAGIC11,%cx
	mov	$MAGIC12,%dx
	mov	$MAGIC13,%bx
	mov	$MAGIC14,%ebp
	mov	$MAGIC15,%esi
	mov	$MAGIC16,%edi
	addr32 pushaw			// `addr32' (0x67) should be ignored
/*
 * now %sp                        sp₂                           INI_SP
 * ↓       +4      +8      +12     ↓                               ↓
 * ┌───┬───┬───┬───┬───┬───┬───┬───┬───────┬───────┬───────┬───────┐
 * │M₁₆│M₁₅│M₁₄│sp₂│M₁₃│M₁₂│M₁₁│M₁₀│MAGIC4 │MAGIC3 │MAGIC2 │MAGIC1 │
 * └───┴───┴───┴───┴───┴───┴───┴───┴───────┴───────┴───────┴───────┘
 *  %di %si %bp     %bx %dx %cx %ax
 */

	mov	%sp,%bp
	cmpl	$SMUSH(MAGIC10, MAGIC11),12(%bp)
	jnz	fail

	popal
/*
 *                                sp₂                        %sp = INI_SP
 *                                 ↓                               ↓
 * ┌───┬───┬───┬───┬───┬───┬───┬───┬───────┬───────┬───────┬───────┐
 * │M₁₆│M₁₅│M₁₄│sp₂│M₁₃│M₁₂│M₁₁│M₁₀│MAGIC4 │MAGIC3 │MAGIC2 │MAGIC1 │
 * └───┴───┴───┴───┴───┴───┴───┴───┴───────┴───────┴───────┴───────┘
 *  └─────┘ └─────┘ └─────┘          %ebx    %edx    %ecx    %eax
 *   %edi    %esi    %ebp
 */

	cmp	$MAGIC1,%eax
	jnz	fail
	cmp	$MAGIC2,%ecx
	jnz	fail
	cmp	$MAGIC3,%edx
	jnz	fail
	cmp	$MAGIC4,%ebx
	jnz	fail
	cmp	$INI_ESP,%esp
	jnz	fail
	cmp	$SMUSH(MAGIC12, MAGIC13),%ebp
	jnz	fail
	cmp	$SMUSH(INI_SP - 16, MAGIC14),%esi
	jnz	fail
	cmp	$SMUSH(MAGIC15, MAGIC16),%edi
	jnz	fail

#define GDT_LEGACY_CODE	8
#define GDT_LEGACY_DATA	16

	cli
	lgdtl	good_gdtr
	mov	%cr0,%eax
	or	$1,%al
	mov	%eax,%cr0
	ljmpw	$GDT_LEGACY_CODE,$2f

fail:
	int3
	hlt
	jmp	fail

	.balign	8
bad_idt:
	.quad	0

	.section .text,"ax",@progbits

	.code32

#define PM_ESP		0x00020004	// in 32-bit mode, try starting with
					// a value of %esp that will force
					// a carry/borrow in the high half
					// when we push or pop stuff

2:	mov	$GDT_LEGACY_DATA,%ax
	mov	%ax,%ds
	mov	%ax,%es
	mov	%ax,%ss
	mov	$PM_ESP,%esp

#define MAGIC17		0xcd2814e8
#define MAGIC18		0xb7912036
#define MAGIC19		0xf98a2750
#define MAGIC20		0x1f6574af
#define MAGIC21		0xbb4a9d2a
#define MAGIC22		0x9a86ec5a
#define MAGIC23		0x33662e67

	mov	$MAGIC17,%eax
	mov	$MAGIC18,%ecx
	mov	$MAGIC19,%edx
	mov	$MAGIC20,%ebx
	mov	$MAGIC21,%ebp
	mov	$MAGIC22,%esi
	mov	$MAGIC23,%edi
	pushal
	cmp	$PM_ESP-32,%esp
	jnz	fail

#define MAGIC24		0xad707a8a

	mov	$MAGIC24,%edi
	addr16 popaw			// `addr16' (0x67) should be ignored
/*
 *                             now %esp                         PM_ESP
 *                                 ↓                               ↓
 * ┌───┬───┬───┬───┬───┬───┬───┬───┬───────┬───────┬───────┬───────┐
 * │MAGIC23│MAGIC22│MAGIC21│PM_ESP │MAGIC20│MAGIC19│MAGIC18│MAGIC17│
 * └───┴───┴───┴───┴───┴───┴───┴───┴───────┴───────┴───────┴───────┘
 *  %di %si %bp     %bx %dx %cx %ax
 */

	cmpw	$(MAGIC22 >> 16),-10(%esp)
	jnz	fail
	cmp	$HL(MAGIC17, PM_ESP >> 16),%eax
	jnz	fail
	cmp	$HL(MAGIC18, PM_ESP),%ecx
	jnz	fail
	cmp	$HL(MAGIC19, MAGIC21 >> 16),%edx
	jnz	fail
	cmp	$HL(MAGIC20, MAGIC21),%ebx
	jnz	fail
	cmp	$PM_ESP-16,%esp
	jnz	fail
	cmp	$HL(MAGIC21, MAGIC22),%ebp
	jnz	fail
	cmp	$HL(MAGIC22, MAGIC23 >> 16),%esi
	jnz	fail
	cmp	$HL(MAGIC24, MAGIC23),%edi
	jnz	fail

#define MAGIC25		0x205de5bd

	mov	$MAGIC25,%eax
	popaw
/*
 *                                                             now %esp =
 *                                                              PM_ESP
 *                                                                 ↓
 * ┌───────┬───────┬───────┬───────┬───┬───┬───┬───┬───┬───┬───┬───┐
 * │MAGIC23│MAGIC22│MAGIC21│PM_ESP │MAGIC20│MAGIC19│MAGIC18│MAGIC17│
 * └───────┴───────┴───────┴───────┴───┴───┴───┴───┴───┴───┴───┴───┘
 *                                  %di %si %bp     %bx %dx %cx %ax
 */
	cmpw	$(MAGIC19 >> 16),-10(%esp)
	jnz	fail
	cmp	$HL(MAGIC25, MAGIC17 >> 16),%eax
	jnz	fail
	cmp	$HL(MAGIC18, MAGIC17),%ecx
	jnz	fail
	cmp	$HL(MAGIC19, MAGIC18 >> 16),%edx
	jnz	fail
	cmp	$HL(MAGIC20, MAGIC18),%ebx
	jnz	fail
	cmp	$PM_ESP,%esp
	jnz	fail
	cmp	$HL(MAGIC21, MAGIC19),%ebp
	jnz	fail
	cmp	$HL(MAGIC22, MAGIC20 >> 16),%esi
	jnz	fail
	cmp	$HL(MAGIC24, MAGIC20),%edi
	jnz	fail

	cli				// tests passed
	xor	%edi,%edi
	lidt	bad_idt
	mov	$231,%eax
	syscall				// this will triple fault on a real PC

	.section .rodata,"a",@progbits

good_gdtr:
	.short	good_gdt_end-good_gdt-1
	.long	good_gdt

	.balign	8

good_gdt:
.quad	0b0000000000000000000000000000000000000000000000000000000000000000 # 0
.quad	0b0000000011001111100110100000000000000000000000001111111111111111 # 8
.quad	0b0000000011001111100100100000000000000000000000001111111111111111 #16
good_gdt_end:
