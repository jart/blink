# Blink's Assembly Tests

Blink's "assembly tests" are tests written in pure assembly that we run
both in Blink and on bare metal.

Compilers only utilize a subset of what the instruction set architecture
is capable of doing. For example, `CMPXCHG` is able to set the overflow
flag under certain conditions, but that would never matter to any C code
that's compiled by GCC. Another example is the clearing of the upper
half of registers on 32-bit operations. We need to ensure that Blink
will always do that when appropriate, just as the host hardware would.
However C code may be unlikely to catch when this happening.

In host environments that aren't AMD64 Linux, bare metal testing will be
stubbed out using `$(VM)`. Therefore this test suite will still pass,
but it'll only be 100% useful when it's run on an `x86_64-linux` system.
