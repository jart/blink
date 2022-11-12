# blink

blink is a virtual machine for running x86-64-linux programs on different
operating systems and hardware architectures. It's designed to do the
same thing as the `qemu-x86_64` command, except rather than being a 10mb
binary, blink only has a ~142kb footprint. For further details on the
motivations for this tool, please read <https://justine.lol/ape.html>.

## Getting Started

You can compile blink on x86-64 Linux, Darwin, FreeBSD, NetBSD, OpenBSD,
or Apple Silicon with Rosetta installed, using your platform toolchain.

```sh
$ build/bootstrap/make.com -j8 o//blink/blink
$ o//blink/blink third_party/cosmo/hello.com
hello world
$ o//blink/blink third_party/cosmo/tinyhello.elf
hello world
```

You can run some test binaries from the Cosmopolitan Libc project:

```sh
$ build/bootstrap/make.com -j8 check
```

There's a terminal interface for debugging:

```
$ build/bootstrap/make.com -j8 o///blink/tui
$ o//blink/tui -t third_party/cosmo/tinyhello.elf
```

If you're building your code on an x86-64 Linux machine, then the
following command will cross-compile blink for i386, arm, m68k, riscv,
mips, s390x. Then it'll launch all the cross-compiled binaries in qemu
to ensure the test programs above work on all architectures.

```sh
$ build/bootstrap/make.com -j8 emulates
$ o/third_party/qemu/qemu-aarch64 o//aarch64/blink/blink third_party/cosmo/hello.com
hello world
```

## Technical Details

blink is an x86-64 interpreter for POSIX platforms that's written in
ANSI C11 that's compatible with C++ compilers. Instruction decoding is
done using our trimmed-down version of Intel's disassembler Xed. Like
Bochs, Blink doesn't do code generation; the primary focus is having a
smaller binary footprint with a readable codebase. However we're still
likely to add something like jit in the near future.

The primary focus is acting as a virtual machine for userspace binaries
that were compiled using Cosmopolitan Libc. Much of the surface area of
the Linux SYSCALL ABI is supported, including fork() and clone(). The
SSE2, SSE3, SSSE3, POPCNT, CLMUL, RDTSCP, and RDRND ISAs are supported.
x87 currently only supports double (64-bit) precision.

Blink supports 32-bit and 16-bit BIOS programs, plus just enough ring0
instructions to test an operating system bootloader. Plus IBM PC Serial
UART, CGA, and MDA. However these legacy features might get sprung into
a sister project sometime soon.
