#include "test/asm/mac.inc"
.globl _start
_start:
        .test "ELF entry has no dynamic linker finalizer"
        test %rdx,%rdx
        .e
        .exit
