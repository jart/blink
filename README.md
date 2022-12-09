# blink

blink is a virtual machine for running statically-compiled x86-64-linux
programs on different operating systems and hardware architectures. It's
designed to do the same thing as the `qemu-x86_64` command, except
rather than being a 4mb binary, blink only has a ~154kb footprint. The
tradeoff is Blink goes half as fast as Qemu and doesn't have as many
systems integrations. Blink is a great fit when you want a virtual
machine that's embeddable, readable, hackable, and easy to compile. For
further details on the motivations for this tool, please read
<https://justine.lol/ape.html>.

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
done using our trimmed-down version of Intel's disassembler Xed.

Blink uses just-in-time compilation, which is supported on x86_64 and
aarch64. Blink takes the appropriate steps to work around restrictions
relating to JIT, on platforms like Apple and OpenBSD. We generate JIT
code using a printf-style domain-specific language. The JIT works by
generating functions at runtime which call the micro-op functions the
compiler created. Blink decode micro-ops at runtime by scanning their
memory for a RET instruction. This works in most cases, but some tools
can present problems, such as OpenBSD retguard which inserts static
memory relocations into every function. The Makefile config and headers
do a good job preventing tools from generating additional magic, but in
the event that fails, Blink has a runtime check for obvious problems in
the compiler output, and then falls back to using a CALL instruction.

Blink virtualizes memory using the same PML4T approach as the hardware
itself, where memory lookups are indirected through a four-level radix
tree and a translation lookaside buffer. On systems with a huge address
space, Blink can avoid the TLB and PML4T entirely in most circumstances
by identity-mapping memory for the guest program. This goes very fast
but it doesn't work if the upstream tools (like TSAN) have already laid
claim to the virtual addresses that the guest program wants. For example
on Apple platforms it isn't possible to allocate 32-bit addresses, so
the solution Blink uses is to just perform a simple addition whenever a
memory address is translated. This goes fast, but if it doesn't work on
your platform, then it's possible to pass the `blink -m` flag to enable
full portable memory safety.

The prime directive of this project is to act as a virtual machine for
userspace binaries compiled by Cosmopolitan Libc. Much of the surface
area of the Linux SYSCALL ABI is supported, including fork() and
clone(). The SSE2, SSE3, SSSE3, POPCNT, CLMUL, RDTSCP, and RDRND ISAs
are supported. x87 currently only supports double (64-bit) precision.

Blink supports 32-bit and 16-bit BIOS programs, plus just enough ring0
instructions to test an operating system bootloader. Plus IBM PC Serial
UART, CGA, and MDA.
