# blink

blink is an emulator for running x86-64-linux programs on different
operating systems and hardware architectures. It's designed to do the
same thing as the `qemu-x86_64` command, except rather than being a 10mb
binary, blink only has a ~200kb footprint. For further details on the
motivations for this tool, please read <https://justine.lol/ape.html>.

## Getting Started

You can compile blink on x86-64 Linux, Darwin, FreeBSD, NetBSD, OpenBSD,
or Apple Silicon with Rosetta installed, using your platform toolchain.

```sh
$ make -j8 o///blink/blink
$ o///blink/blink third_party/cosmo/hello.com
hello world
$ o///blink/blink third_party/cosmo/tinyhello.elf
hello world
```

There's a terminal interface for debugging:

```
$ make -j8 o///blink/tui
$ o///blink/tui third_party/cosmo/tinyhello.elf
```

On x86-64 Linux you can cross-compile blink for Linux systems with x86,
arm, m68k, riscv, mips, s390x, powerpc, or microblaze cpus. This happens
using vendored musl-cross-make toolchains and static qemu for testing.

```sh
$ make -j8 test
$ o/third_party/qemu/qemu-aarch64 o//aarch64/blink/blink third_party/cosmo/hello.com
hello world
```

## Technical Details

blink is an x86-64 interpreter written in straightforward POSIX ANSI C.
Similar to Bochs, there's no JIT or code generation currently in blink.
Therefore you're trading away performance for a tinier emulator that'll
just work, is ISC (rather than GPL) licensed and it won't let untrusted
code get too close to your hardware. Instruction decoding is done using
our trimmed-down version of Intel's disassembler Xed.
