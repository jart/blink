# Blink Functional Tests

Blink's "functional tests" are simple `x86_64-linux` programs written in
C that are run both under Blink, and under the host `x86_64-linux`
system for comparison. These tests are built using a statically compiled
musl-cross-make toolchain that the build system downloads automatically.
If the host system is not AMD64 Linux, then we fake it using `$(VM)`.

Functional tests also useful for testing various flag combinations:

- `blink` checks JIT + LINEAR MEMORY works
- `blink -j` checks INTERPRETED + LINEAR MEMORY works
- `blink -m` checks JIT + VIRTUALIZED MEMORY works
- `blink -jm` checks INTERPRETED + VIRTUALIZED MEMORY works
- `blink -s` checks system call logging doesn't break things
