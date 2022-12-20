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
Especially if you use multiple threads under heavy processor load.

## Getting Started

We regularly test that Blink is able run x86-64-linux binaries on the
following platforms:

- Linux (x86, ARM, MIPS, PowerPC, s390x)
- MacOS (x86, ARM)
- FreeBSD
- OpenBSD
- NetBSD

Blink depends on the following libraries:

- libc (POSIX.1-2017)

Blink can be compiled on UNIX systems that have:

- A C11 compiler (e.g. GCC 4.9.4+)
- Modern GNU Make (i.e. not the one that comes with XCode)

The instructions for compiling Blink are as follows:

```sh
$ make -j4
$ o//blink/blink -h
Usage: o//blink/blink [-hjms] PROG [ARGS...]
  -h        help
  -j        disable jit
  -m        enable memory safety
  -s        print statistics on exit
```

Here's how you can run a simple hello world program with Blink:

```sh
o//blink/blink third_party/cosmo/tinyhello.elf
```

Blink has a debugger TUI, which works with UTF-8 ANSI terminals. The
most important keystrokes in this interface are `?` for help, 's' for
step, `c` for continue, and scroll wheel for reverse debugging.

```sh
o//blink/tui third_party/cosmo/tinyhello.elf
```

## Testing

Blink is tested primarily using precompiled x86 binaries, which are
downloaded from Justine Tunney's web server. You can check how well
Blink works on your local platform by running:

```sh
make check
```

To check that Blink works on 11 different hardware `$(ARCHITECTURES)`
(see [Makefile](Makefile)), you can run the following command, which
will download statically-compiled builds of GCC and Qemu. Since our
toolchain binaries are intended for x86-64 Linux, Blink will bootstrap
itself locally first, so that it's possible to run these tests on other
operating systems and architectures.

```sh
make check2
make emulates  # potentially flaky
```

## Alternative Builds

For maximum performance, use `MODE=rel` or `MODE=opt`. Please note the
release mode builds will remove all the logging and assertion statements
and Blink isn't mature enough for that yet. So extra caution is advised.

```sh
make MODE=rel
o/rel/blink/blink -h
```

For maximum tinyness, use `MODE=tiny`. This build mode will not only
remove logging and assertion statements, but also reduce performance in
favor of smaller binary size whenever possible.

```sh
make MODE=tiny
strip o/tiny/blink/blink
ls -hal o/tiny/blink/blink
```

You can hunt down bugs in Blink using the following build modes:

- `MODE=asan` helps find memory safety bugs
- `MODE=tsan` helps find threading related bugs
- `MODE=ubsan` to find violations of the C standard
- `MODE=msan` helps find uninitialized memory errors

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
