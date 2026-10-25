.global _start
.text
_start:
 xor %eax,%eax
 mov $0x12345678,%r12
 movq $1,(%rax)
 ud2
