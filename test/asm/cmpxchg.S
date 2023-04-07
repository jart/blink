#include "test/asm/mac.inc"
.globl	_start
_start:

//	compare and exchange tests
//	make -j8 o//blink o//test/asm/cmpxchg.elf
//	o//blink/blinkenlights o//test/asm/cmpxchg.elf

	xor	%ebp,%ebp
	and	$-16,%rsp
	push	%rbp
	mov	%rsp,%rbp
	sub	$16,%rsp

////////////////////////////////////////////////////////////////////////////////
//	locked

	.test	"lock cmpxchg 64 taken"
	mov	$0x7fffffff,%eax
	add	$1,%eax
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rax,-8(%rbp)
	lock cmpxchg %rbx,-8(%rbp)
	.z
	.no
	.ns
	.nc
	.p
	mov	$0x1234567812345678,%rcx
	mov	$0x5555555155555550,%rdx
	cmp	%rcx,%rax			# rax stays the same
	.e
	cmp	%rdx,%rbx			# reg stays the same
	.e
	cmp	%rdx,-8(%rbp)			# memory got swapped
	.e

	.test	"lock cmpxchg 64 not taken"
	mov	$0x7fffffff,%eax
	add	$1,%eax
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rbx,-8(%rbp)
	lock cmpxchg %rbx,-8(%rbp)
	.nz
	.no
	.s
	.c
	.p
	mov	$0x5555555155555550,%rdx
	cmp	%rdx,%rax			# mem is put in rax
	.e
	cmp	%rdx,%rbx			# reg stays the same
	.e
	cmp	%rdx,-8(%rbp)			# memory is the same
	.e

	.test	"lock cmpxchg 64 unaligned taken"
	mov	$0x7fffffff,%eax
	add	$1,%eax
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rax,-9(%rbp)
	lock cmpxchg %rbx,-9(%rbp)
	.z
	.no
	.ns
	.nc
	.p
	mov	$0x1234567812345678,%rcx
	mov	$0x5555555155555550,%rdx
	cmp	%rcx,%rax			# rax stays the same
	.e
	cmp	%rdx,%rbx			# reg stays the same
	.e
	cmp	%rdx,-9(%rbp)			# memory got swapped
	.e

"are we running in qemu":
	mov	$0x40000001,%eax
	cpuid
	cmp	%ebx,qemu
	je	"qemu cmpxchg 32 broken"
	.exit

	.test	"lock cmpxchg 32 taken"
	mov	$0x7fffffff,%eax
	add	$1,%eax
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rax,-8(%rbp)
	lock cmpxchg %ebx,-8(%rbp)
	.z
	.p
	.ns
	.nc
	.no
	mov	$0x1234567812345678,%rcx
	mov	$0x5555555155555550,%rdx
	mov	$0x1234567855555550,%r8
	cmp	%rcx,%rax			# rax stays the same
	.e					# is NOT upper zerod
	cmp	%rdx,%rbx			# reg stays the same
	.e					# is NOT upper zerod
	cmp	%r8,-8(%rbp)			# memory got swapped
	.e

"qemu cmpxchg 32 broken":

	.test	"lock cmpxchg 32 not taken"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rbx,-8(%rbp)
	lock cmpxchg %ebx,-8(%rbp)
	.nz
	.p
	.s
	.c
	.no
	mov	$0x0000000055555550,%rdx	# mem is put in rax
	cmp	%rdx,%rax
	.e
	mov	$0x5555555155555550,%rdx	# reg stays the same
	cmp	%rdx,%rbx
	.e
	cmp	%rdx,-8(%rbp)			# memory is the same
	.e

	.test	"lock cmpxchg 8 taken"
	mov	$0x7fffffff,%eax
	add	$1,%eax
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rax,-8(%rbp)
	lock cmpxchg %bl,-8(%rbp)
	.z
	.p
	.ns
	.nc
	.no
	mov	$0x1234567812345678,%rcx
	mov	$0x5555555155555550,%rdx
	mov	$0x1234567812345650,%r8
	cmp	%rcx,%rax			# rax stays the same
	.e					# is NOT upper zerod
	cmp	%rdx,%rbx			# reg stays the same
	.e					# is NOT upper zerod
	cmp	%r8,-8(%rbp)			# memory got swapped
	.e

	.test	"lock cmpxchg 8 not taken"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rbx,-8(%rbp)
	lock cmpxchg %bl,-8(%rbp)
	.nz
	.p
	.ns
	.nc
	.no
	mov	$0x1234567812345650,%rdx	# mem is put in rax
	cmp	%rdx,%rax
	.e
	mov	$0x5555555155555550,%rdx	# reg stays the same
	cmp	%rdx,%rbx
	.e
	cmp	%rdx,-8(%rbp)			# memory is the same
	.e

////////////////////////////////////////////////////////////////////////////////
//	unlocked

	.test	"cmpxchg 64 taken"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rax,-8(%rbp)
	cmpxchg	%rbx,-8(%rbp)
	.z
	mov	$0x1234567812345678,%rcx
	mov	$0x5555555155555550,%rdx
	cmp	%rcx,%rax			# rax stays the same
	.e
	cmp	%rdx,%rbx			# reg stays the same
	.e
	cmp	%rdx,-8(%rbp)			# memory got swapped
	.e

	.test	"cmpxchg 64 not taken"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rbx,-8(%rbp)
	cmpxchg	%rbx,-8(%rbp)
	.nz
	mov	$0x5555555155555550,%rdx
	cmp	%rdx,%rax			# mem is put in rax
	.e
	cmp	%rdx,%rbx			# reg stays the same
	.e
	cmp	%rdx,-8(%rbp)			# memory is the same
	.e

	.test	"cmpxchg 32 taken"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rax,-8(%rbp)
	cmpxchg	%ebx,-8(%rbp)
	.z
	mov	$0x1234567812345678,%rcx
	mov	$0x5555555155555550,%rdx
	mov	$0x1234567855555550,%r8
	cmp	%rcx,%rax			# rax stays the same
	.e					# is NOT upper zerod
	cmp	%rdx,%rbx			# reg stays the same
	.e					# is NOT upper zerod
	cmp	%r8,-8(%rbp)			# memory got swapped
	.e

	.test	"cmpxchg 32 not taken"
	mov	$0x1234567812345678,%rax
	mov	$0x5555555155555550,%rbx
	mov	%rbx,-8(%rbp)
	cmpxchg	%ebx,-8(%rbp)
	.nz
	mov	$0x0000000055555550,%rdx	# mem is put in rax
	cmp	%rdx,%rax
	.e
	mov	$0x5555555155555550,%rdx	# reg stays the same
	cmp	%rdx,%rbx
	.e
	cmp	%rdx,-8(%rbp)			# memory is the same
	.e

"test succeeded":
	.exit

qemu:	.ascii	"TCGTCGTCGTCG"
