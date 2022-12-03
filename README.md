# blink

blink is a virtual machine for running x86-64-linux programs on different
operating systems and hardware architectures. It's designed to do the
same thing as the `qemu-x86_64` command, except rather than being a 10mb
binary, blink only has a ~154kb footprint. For further details on the
motivations for this tool, please read <https://justine.lol/ape.html>.

## Caveat Emptor

Welcome everyone from the Hacker News, Lobsters, and Reddit communities!
This project is a work in progress. Please don't use this for production
yet. If you try this be sure to calibrate your expectations accordingly.

## Getting Started

You can compile blink on x86-64 Linux, Darwin, FreeBSD, NetBSD, OpenBSD,
Apple Silicon, and Raspberry Pi using your operating system's toolchain.

```sh
# for all x86-64 platforms
$ build/bootstrap/make.com -j8 o//blink/blink

# for apple m1 arm silicon
# don't use the ancient version of gnu make that comes with xcode
$ make -j8 o//blink/blink

# for linux raspberry pi
$ build/bootstrap/blink-linux-aarch64 build/bootstrap/make.com -j8 o//blink/blink

# run actually portable executable in virtual machine
$ o//blink/blink third_party/cosmo/hello.com
hello world

# run static elf binary in virtual machine
$ o//blink/blink third_party/cosmo/tinyhello.elf
hello world
```

There's a terminal interface for debugging:

```
$ build/bootstrap/make.com -j8 o///blink/tui
$ o//blink/tui -t third_party/cosmo/tinyhello.elf
```

You can run our test executables to check your local platform build:

```sh
$ build/bootstrap/make.com -j8 check
```

For maximum performance, use `MODE=rel` or `MODE=opt`.

```sh
$ build/bootstrap/make.com MODE=opt -j8 check
```

For maximum tinyness, use `MODE=tiny`.

```
$ build/bootstrap/make.com MODE=tiny -j8 check
$ strip o/tiny/blink/blink
$ ls -hal o/tiny/blink/blink
```

You can sanitize using `MODE=asan`, `MODE=ubsan`, `MODE=tsan`, and
`MODE=msan`.

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
done using our trimmed-down version of Intel's disassembler Xed. Blink
does *some* code generation at runtime, using a just-in-time approach,
where functions are generated that thread statically-compiled operation
functions together.

The prime directive of this project is to act as a virtual machine for
userspace binaries compiled by Cosmopolitan Libc. Much of the surface
area of the Linux SYSCALL ABI is supported, including fork() and
clone(). The SSE2, SSE3, SSSE3, POPCNT, CLMUL, RDTSCP, and RDRND ISAs
are supported. x87 currently only supports double (64-bit) precision.

Blink supports 32-bit and 16-bit BIOS programs, plus just enough ring0
instructions to test an operating system bootloader. Plus IBM PC Serial
UART, CGA, and MDA. However these legacy features might get sprung into
a sister project sometime soon.

## Flakes

![Blink Flakes: The Original and Best Unexplained Errors](test/flakes.png)

There aren't any known flakes at this time.
