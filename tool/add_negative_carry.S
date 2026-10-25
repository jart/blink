/* A sign-extended imm8 must be truncated before 32-bit carry calculation.
   This is the countdown sequence used by Claude's bundled JavaScript engine. */
.global _start
.text
_start:
  mov $2000, %r9d
1:
  mov $4, %r15d
  mov $16, %r10d
2:
  addl $-1, %r15d
  jae 3f
  dec %r10d
  jnz 2b
  mov $42, %edi
  jmp 4f
3:
  cmp $-1, %r15d
  jne 5f
  dec %r9d
  jnz 1b
  xor %edi, %edi
4:
  mov $60, %eax
  syscall
5:
  mov $43, %edi
  jmp 4b
