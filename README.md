![Screenshot of Blink running GCC 9.4.0](blink/blink-gcc.png)

# blink

blink is a virtual machine for running statically-compiled x86-64-linux
programs on different operating systems and hardware architectures. It's
designed to do the same thing as the `qemu-x86_64` command, except (a)
rather than being a 4mb binary, Blink only has a ~158kb footprint; and
(b) Blink goes faster than Qemu on some benchmarks, such as emulating
GCC. The tradeoff is Blink doesn't have as many systems integrations as
Qemu. Blink is a great fit when you want a virtual machine that's
embeddable, readable, hackable, and easy to compile. For further details
on the motivations for this tool, please read
<https://justine.lol/ape.html>.

## Caveat Emptor

Welcome everyone from the Hacker News, Lobsters, and Reddit communities!
This project is a work in progress. Please don't use this for production
yet. If you try this be sure to calibrate your expectations accordingly.

## Getting Started

You can compile Blink on x86-64 Linux, Darwin, FreeBSD, NetBSD, OpenBSD,
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
$ build/bootstrap/make.com -j8 o//blink/tui
$ o//blink/tui third_party/cosmo/tinyhello.elf
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

The prime directive of this project is to act as a virtual machine for
userspace binaries compiled by Cosmopolitan Libc. However we've also had
success virtualizing programs compiled with Glibc and Musl Libc, such as
GCC and Qemu. Blink supports more than a hundred Linux system call ABIs,
including fork() and clone(). The SSE2, SSE3, SSSE3, POPCNT, CLMUL,
RDTSCP, and RDRND hardware ISAs are supported. Blink's legacy x87 FPU
currently only supports double (64-bit) precision, just like Windows.

Blink uses just-in-time compilation, which is supported on x86_64 and
aarch64. Blink takes the appropriate steps to work around restrictions
relating to JIT, on platforms like Apple and OpenBSD. We generate JIT
code using a printf-style domain-specific language. The JIT works by
generating functions at runtime which call the micro-op functions the
compiler created. To make micro-operations go faster, Blink determines
the byte length of the compiled function at runtime by scanning for a
RET instruction. Blink will then copy the compiled function into the
function that the JIT is generating. This works in most cases, however
some tools can cause problems. For example, OpenBSD RetGuard inserts
static memory relocations into every compiled function, which Blink's
JIT currently doesn't understand; so we need to use compiler flags to
disable that type of magic. In the event other such magic slips through,
Blink has a runtime check which will catch obvious problems, and then
gracefully fall back to using a CALL instruction. Since no JIT can be
fully perfect on all platforms, the `o//blink/blink -j` flag may be
passed to disable Blink's JIT. Please note that disabling JIT makes
Blink go 10x slower. With the `o//blink/tui` command, the `-j` flag
takes on the opposite meaning, where it instead *enables* JIT. This can
be useful for troubleshooting the JIT, because the TUI display has a
feature that lets JIT path formation be visualized. Blink currently only
enables the JIT for programs running in long mode (64-bit) but we may
support JITing 16-bit programs in the future.

Blink virtualizes memory using the same PML4T approach as the hardware
itself, where memory lookups are indirected through a four-level radix
tree. Since performing four separate page table lookups on every memory
access can be slow, Blink checks a translation lookaside buffer, which
contains the sixteen most recently used page table entries. The PML4T
allows all memory lookups in Blink to be "safe" but it still doesn't
offer the best possible performance. Therefore, on systems with a huge
address space (i.e. petabytes of virtual memory) Blink relies on itself
being loaded to a random location, and then identity maps guest memory
using a simple linear translation. For example, if the guest virtual
address is `0x400000` then the host address might be
`0x400000+0x088800000000`. This means that each time a memory operation
is executed, only a simple addition needs to be performed. This goes
extremely fast, however it may present issues for programs that use
`MAP_FIXED`. Some systems, such as modern Raspberry Pi, actually have a
larger address space than x86-64, which lets Blink offer the guest the
complete address space. However on some platforms, like 32-bit ones,
only a limited number of identity mappings are possible. There's also
compiler tools like TSAN which lay claim to much of the fixed address
space. Blink's solution is designed to meet the needs of Cosmopolitan
Libc, while working around Apple's restriction on 32-bit addresses, and
still remain fully compatible with ASAN's restrictions. In the event
that this translation scheme doesn't work on your system, the `blink -m`
flag may be passed to disable the linear translation optimization, and
instead use only the memory safe full virtualization approach of the
PML4T and TLB.

Blink has an xterm-compatible ANSI teletypewriter display implementation
which allows Blink's TUI interface to host other TUI programs, within an
embedded terminal display. For example, it's possible to use Antirez's
Kilo text editor inside Blink's TUI.

Blink supports 16-bit BIOS programs, such as SectorLISP. To boot real
mode programs in Blink, the `o//blink/tui -r` flag may be passed, which
puts the virtual machine in i8086 mode. Currently only a limited set of
BIOS APIs are available. For example, Blink supports IBM PC Serial UART,
CGA display, and the MDA display APIs which are rendered using block
characters in the TUI interface. We hope to expand our real mode support
in the near future, in order to run operating systems like ELKS.

Blink supports troubleshooting operating system bootloaders. Blink was
designed for Cosmopolitan Libc, which embeds an operating system in each
binary it compiles. Blink has helped us debug our bare metal support,
since Blink is capable of running in the 16-bit, 32-bit, and 64-bit
modes a bootloader requires at various stages. In order to do that, we
needed to implement some ring0 hardware instructions. Blink has enough
to support Cosmopolitan, but it'll take much more time to get Blink to a
point where it can boot something like Windows.

Blink supports several different executable formats, all of which are
static. You can run:

- Actually Portable Executables, which have either the `MZqFpD` or
  `jartsr` magic.

- Statically-compiled x86-64-linux ELF executables, so long as they
  don't use PIC/PIE or require a interpreter.

- Flat executables, which must end with the file extension `.bin`. In
  this case, you can make executables as small as 10 bytes in size,
  since they're treated as raw x86-64 code. Blink always loads flat
  executables to the address `0x400000` and automatically appends 16mb
  of BSS memory.

- Real mode executables, which are loaded to the address `0x7c00`. These
  programs must be run using the `tui` command with the `-r` flag.
