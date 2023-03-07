![Screenshot of Blink running GCC 9.4.0](blink/blink-gcc.png)

[![Test Status](https://github.com/jart/blink/actions/workflows/build.yml/badge.svg)](https://github.com/jart/blink/actions/workflows/build.yml)
[![Cygwin Test Status](https://github.com/jart/blink/actions/workflows/cygwin.yml/badge.svg)](https://github.com/jart/blink/actions/workflows/cygwin.yml)
# Blinkenlights

This project contains two programs:

`blink` is a virtual machine that runs x86-64-linux programs on
different operating systems and hardware architectures. It's designed to
do the same thing as the `qemu-x86_64` command, except that

1. Blink is 213kb in size (112kb with optional features disabled),
   whereas qemu-x86_64 is a 4mb binary.

2. Blink will run your Linux binaries on any POSIX system, whereas
   qemu-x86_64 only supports Linux.

3. Blink goes 2x faster than qemu-x86_64 on some benchmarks, such as SSE
   integer / floating point math. Blink is also much faster at running
   ephemeral programs such as compilers.

[`blinkenlights`](https://justine.lol/blinkenlights) is a terminal user
interface that may be used for debugging x86_64-linux or i8086 programs
across platforms. Unlike GDB, Blinkenlights focuses on visualizing
program execution. It uses UNICODE IBM Code Page 437 characters to
display binary memory panels, which change as you step through your
program's assembly code. These memory panels may be scrolled and zoomed
using your mouse wheel. Blinkenlights also permits reverse debugging,
where scroll wheeling over the assembly display allows the rewinding of
execution history.

## Getting Started

We regularly test that Blink is able run x86-64-linux binaries on the
following platforms:

- Linux (x86, ARM, RISC-V, MIPS, PowerPC, s390x)
- macOS (x86, ARM)
- FreeBSD
- OpenBSD
- Cygwin

Blink depends on the following libraries:

- libc (POSIX.1-2017 baseline, XSI not required)

Blink can be compiled on UNIX systems that have:

- A C11 compiler with atomics (e.g. GCC 4.9.4+)
- Modern GNU Make (i.e. not the one that comes with XCode)

The instructions for compiling Blink are as follows:

```sh
./configure
make -j4
doas make install  # note: doas is modern sudo
blink -v
man blink
```

Here's how you can run a simple hello world program with Blink:

```sh
blink third_party/cosmo/tinyhello.elf
```

Blink has a debugger TUI, which works with UTF-8 ANSI terminals. The
most important keystrokes in this interface are `?` for help, `s` for
step, `c` for continue, and scroll wheel for reverse debugging.

```sh
blinkenlights third_party/cosmo/tinyhello.elf
```

### Alternative Builds

For maximum tinyness, use `MODE=tiny`, since it makes Blink's binary
footprint 50% smaller. The Blink executable should be on the order of
200kb in size. Performance isn't impacted. Please note that all
assertions will be removed, as well as all logging. Use this mode if
you're confident that Blink is bug-free for your use case.

```sh
make MODE=tiny
strip o/tiny/blink/blink
ls -hal o/tiny/blink/blink
```

Some distros configure their compilers to add a lot of security bloat,
which might add 60kb or more to the above binary size. You can work
around that by using one of Blink's toolchains. This should produce
consistently the smallest possible executable size.

```sh
make MODE=tiny o/tiny/x86_64/blink/blink
o/third_party/gcc/x86_64/bin/x86_64-linux-musl-strip o/tiny/x86_64/blink/blink
ls -hal o/tiny/x86_64/blink/blink
```

If you want to make Blink *even tinier* (more on the order of 120kb
rather than 200kb) than you can tune the `./configure` script to disable
optional features such as jit, threads, sockets, x87, bcd, xsi, etc.

```sh
./configure --disable-all --posix
make MODE=tiny o/tiny/x86_64/blink/blink
o/third_party/gcc/x86_64/bin/x86_64-linux-musl-strip o/tiny/x86_64/blink/blink
ls -hal o/tiny/x86_64/blink/blink
```

The traditional `MODE=rel` or `MODE=opt` modes are available. Use this
mode if you're on a non-JIT architecture (since this won't improve
performance on AMD64 and ARM64) and you're confident that Blink is
bug-free for your use case, and would rather have Blink not create a
`blink.log` or print `SIGSEGV` delivery warnings to standard error,
since many apps implement their own crash reporting.

```sh
make MODE=rel
o/rel/blink/blink -h
```

You can hunt down bugs in Blink using the following build modes:

- `MODE=asan` helps find memory safety bugs
- `MODE=tsan` helps find threading related bugs
- `MODE=ubsan` to find violations of the C standard
- `MODE=msan` helps find uninitialized memory errors

You can check Blink's compliance with the POSIX standard using the
following configuration flags:

```sh
./configure --posix  # only use c11 with baseline posix standard
./configure --xopen  # same but also allow use of xsi extensions
```

### Testing

Blink is tested primarily using precompiled binaries downloaded
automatically. Blink has more than 700 test programs total. You can
check how well Blink works on your local platform by running:

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
make emulates
```

### Production Worthiness

Blink passes 194 test suites from the Cosmopolitan Libc project (see
[third_party/cosmo](third_party/cosmo). Blink passes 350 test suites
from the [Linux Test Project](https://github.com/linux-test-project/ltp)
(see [third_party/ltp](third_party/ltp). Blink passes 108 of [Musl
Libc's unit test suite](https://github.com/jart/libc-test) (see
[third_party/libc-test](third_party/libc-test)). The tests we haven't
included are usually due to (1) floating ULP rounding errors (Blink aims
to be fast and tiny, so we're on the fence about floating point
emulation), and (2) APIs that can't or won't be supported, e.g. System V
message queues. Blink runs the precompiled Linux test binaries above on
other operating systems too, e.g. Apple M1, FreeBSD, Cygwin.

## Reference

The Blinkenlights project provides two programs which may be launched on
the command line.

### `blink` Flags

The headless Blinkenlights virtual machine command (named `blink` by
convention) accepts command line arguments per the specification:

```
blink [FLAG...] PROGRAM [ARG...]
```

Where `PROGRAM` is an x86_64-linux binary that may be specified as:

1. An absolute path to an executable file, which will be run as-is
2. A relative path containing slashes, which will be run as-is
3. A path name without slashes, which will be `$PATH` searched

The following `FLAG` arguments are provided:

- `-h` shows help on command usage

- `-v` shows version and build configuration details

- `-e` means log to standard error (fd 2) in addition to the log file.
  If logging to *only* standard error is desired, then `-eL/dev/null`
  may be used.

- `-j` disables Just-In-Time (JIT) compilation, which will make Blink go
  ~10x slower.

- `-m` disables the linear memory optimization. This makes Blink memory
  safe, but comes at the cost of going ~4x slower. On some platforms
  this can help avoid the possibility of an mmap() crisis.

- `-0` allows `argv[0]` to be specified on the command line. Under
  normal circumstances, `blink cmd arg1` is equivalent to `execve("cmd",
  {"cmd", "arg1"})` since that's how most programs are launched. However
  if you need the full power of execve() process spawning, you can say
  `blink -0 cmd arg0 arg1` which is equivalent to `execve("cmd",
  {"arg0", "arg1"})`.

- `-L PATH` specifies the log path. The default log path is `blink.log`
  in the current directory at startup. This log file won't be created
  until something is actually logged. If logging to a file isn't
  desired, then `-L /dev/null` may be used. See also the `-e` flag for
  logging to standard error.

- `-s` enables system call logging. This will emit to the log file the
  names of system calls each time a SYSCALL instruction in executed,
  along with its arguments and result. System calls are logged once
  they've completed. If this option is specified twice, then system
  calls which are likely to block (e.g. poll) will be logged at entry
  too. If this option is specified thrice, then all cancellation points
  will be logged upon entry. System call logging isn't available in
  `MODE=rel` and `MODE=tiny` builds, in which case this flag is ignored.

- `-Z` will cause internal statistics to be printed to standard error on
  exit. Stats aren't available in `MODE=rel` and `MODE=tiny` builds, and
  this flag is ignored.

- `-C path` will cause blink to launch the program in a chroot'd
  environment. This flag is both equivalent to and overrides the
  `BLINK_OVERLAYS` environment variable.

### `blinkenlights` Flags

The Blinkenlights ANSI TUI interface command (named `blinkenlights` by
convention) accepts its command line arguments in accordance with the
following specification:

```
blinkenlights [FLAG...] PROGRAM [ARG...]
```

Where `PROGRAM` is an x86_64-linux binary that may be specified as:

1. An absolute path to an executable file, which will be run as-is
2. A relative path containing slashes, which will be run as-is
3. A path name without slashes, which will be `$PATH` searched

The following `FLAG` arguments are provided:

- `-h` shows help on command usage

- `-v` shows version and build configuration details

- `-r` puts your virtual machine in real mode. This may be used to run
  16-bit i8086 programs, such as SectorLISP. It's also used for booting
  programs from Blinkenlights's simulated BIOS.

- `-b ADDR` pushes a breakpoint, which may be specified as a raw
  hexadecimal address, or a symbolic name that's defined by your ELF
  binary (or its associated `.dbg` file). When pressing `c` (continue)
  or `C` (continue harder) in the TUI, Blink will immediately stop upon
  reaching an instruction that's listed as a breakpoint, after which a
  modal dialog is displayed. The modal dialog may be cleared by `ENTER`
  after which the TUI resumes its normal state.

- `-w ADDR` pushes a watchpoint, which may be specified as a raw
  hexadecimal address, or a symbolic name that's defined by your ELF
  binary (or its associated `.dbg` file). When pressing `c` (continue)
  or `C` (continue harder) in the TUI, Blink will immediately stop upon
  reaching an instruction that either (a) has a ModR/M encoding that
  references an address that's listed as a watchpoint, or (b) manages to
  mutate the memory stored at a watchpoint address by some other means.
  When Blinkenlights is stopped at a watchpoint, a modal dialog will be
  displayed which may be cleared by pressing `ENTER`, after which the
  TUI resumes its normal state.

- `-j` enables Just-In-Time (JIT) compilation. This will make
  Blinkenlights go significantly faster, at the cost of taking away the
  ability to step through each instruction. The TUI will visualize JIT
  path formation in the assembly display; see the JIT Path Glyphs
  section below to learn more. Please note this flag has the opposite
  meaning as it does in the `blink` command.

- `-m` enables the linear memory optimization. This makes blinkenlights
  capable of faster emulation, at the cost of losing some statistics. It
  no longer becomes possible to display which percentage of a memory map
  has been activated. Blinkenlights will also remove the commit /
  reserve / free page statistics from the status panel on the bottom
  right of the display. Please note this flag has the opposite meaning
  as it does in the `blink` command.

- `-t` may be used to disable Blinkenlights TUI mode. This makes the
  program behave similarly to the `blink` command, however not as good.
  We're currently using this flag for unit testing real mode programs,
  which are encouraged to use the `SYSCALL` instruction to report their
  exit status.

- `-L PATH` specifies the log path. The default log path is
  `$TMPDIR/blink.log` or `/tmp/blink.log` if `$TMPDIR` isn't defined.

- `-C path` will cause blink to launch the program in a chroot'd
  environment. This flag is both equivalent to and overrides the
  `BLINK_OVERLAYS` environment variable.

- `-s` enables system call logging. This will emit to the log file the
  names of system calls each time a SYSCALL instruction in executed,
  along with its arguments and result. System calls are logged once
  they've completed. If this option is specified twice, then system
  calls which are likely to block (e.g. poll) will be logged at entry
  too. If this option is specified thrice, then all cancellation points
  will be logged upon entry. System call logging isn't available in
  `MODE=rel` and `MODE=tiny` builds, in which case this flag is ignored.

- `-Z` will cause internal statistics to be printed to standard error on
  exit. Each line will display a monitoring metric. Most metrics will
  either be integer counters or floating point running averages. Most
  but not all integer counters are monotonic. In the interest of not
  negatively impacting Blink's performance, statistics are computed on a
  best effort basis which currently isn't guaranteed to be atomic in a
  multi-threaded environment. Stats aren't available in `MODE=rel` and
  `MODE=tiny` builds, and this flag is ignored.

- `-z` [repeatable] may be specified to zoom the memory panels, so they
  display a larger amount of memory in a smaller space. By default, one
  terminal cell corresponds to a single byte of memory. When memory has
  been zoomed the magic kernel is used (similar to Lanczos) to decimate
  the number of bytes by half, for each `-z` that's specified. Normally
  this would be accomplished by using `CTRL+MOUSEWHEEL` where the mouse
  cursor is hovered over the panel that should be zoomed. However, many
  terminal emulators (especially on Windows), do not support this xterm
  feature and as such, this flag is provided as an alternative.

- `-V` [repeatable] increases verbosity

- `-R` disables reactive error mode

- `-H` disables syntax highlighting

- `-N` enables natural scrolling

### Recommended Environments

Blinkenlights' TUI requires a UTF-8 VT100 / XTERM style terminal to use.
We recommend the following terminals, ordered by preference:

- [KiTTY](https://sw.kovidgoyal.net/kitty/) (Linux)
- [PuTTY](https://www.chiark.greenend.org.uk/~sgtatham/putty/latest.html) (Windows)
- Gnome Terminal (Linux)
- Terminal.app (macOS)
- CMD.EXE (Windows 10+)
- PowerShell (Windows 10+)
- Xterm (Linux)

The following fonts are recommended, ordered by preference:

- [PragmataPro Regular Mono](https://fsd.it/shop/fonts/pragmatapro/) (€59)
- Bitstream Vera Sans Mono (a.k.a. DejaVu Sans Mono)
- Consolas
- Menlo

#### JIT Path Glyphs

When the Blinkenlights TUI is run with JITing enabled (using the `-j`
flag) the assembly dump display will display a glyph next to the address
of each instruction, to indicate the status of JIT path formation. Those
glyphs are defined as follows:

- ` ` or space indicates no JIT path is associated with an address

- `S` means that a JIT path is currently being constructed which
  starts at this address. By continuing to press `s` (step) in the TUI
  interface, the JIT path will grow longer until it is eventually
  completed, and the `S` glyph is replaced by `*`.

- `*` (asterisk) means that a JIT path has been installed to the
  adjacent address. When `s` (step) is pressed at such addresses
  within the TUI display, stepping takes on a different meaning.
  Rather than stepping a single instruction, it will step the entire
  length of the JIT path. The next assembly line that'll be
  highlighted will be the instruction after where the path ends.

### Environment Variables

The following environment variables are recognized by both the `blink`
and `blinkenlights` commands:

- `BLINK_LOG_FILENAME` may be specified to supply a log path to be used
  in cases where the `-L PATH` flag isn't specified. This value should
  be an absolute path. If logging to standard error is desired, use the
  `blink -e` flag.

- `BLINK_OVERLAYS` specifies one or more directories to use as the root
  filesystem. Similar to `$PATH` this is a colon delimited list of
  pathnames. If relative paths are specified, they'll be resolved to an
  absolute path at startup time. Overlays only apply to IO system calls
  that specify an absolute path. The empty string overlay means use the
  normal `/` root filesystem. The default value is `:o`, which means if
  the absolute path `/$f` is opened, then first check if `/$f` exists,
  and if it doesn't, then check if `o/$f` exists, in which case open
  that instead. Blink uses this convention to open shared object tests.
  It favors the system version if it exists, but also downloads
  `ld-musl-x86_64.so.1` to `o/lib/ld-musl-x86_64.so.1` so the dynamic
  linker can transparently find it on platforms like Apple, that don't
  let users put files in the root folder. On the other hand, it's
  possible to say `BLINK_OVERLAYS=o:` so that `o/...` takes precedence
  over `/...` (noting again that empty string means root). If a single
  overlay is specified that isn't empty string, then it'll effectively
  act as a restricted chroot environment.

## Compiling and Running Programs under Blink

Blink can be picky about which Linux binaries it'll execute. It may also
be the case that your Linux binary will only run under Blink on Linux,
but report errors if run under Blink on another platform, e.g. macOS. In
our experience, how successfully a program can run under Blink depends
almost entirely on (1) how it was compiled, and (2) which C library it
uses. This section will provide guidance on which tools will work best.

First, some background. Blink's coverage of the x86_64 instruction set
is comprehensive. However the Linux system call ABI is much larger and
therefore not possible to fully support, unless Blink emulated a Linux
kernel image too. Blink has sought to support the subset of Linux ABIs
that are either (1) standardized by POSIX.1-2017 or (2) too popular to
*not* support. As an example, `AF_INET`, `AF_UNIX`, and `AF_INET6` are
supported, but Blink will return `EINVAL` if a program requests any of
the dozens of other ones, e.g. `AF_BLUETOOTH`. Such errors are usually
logged to `/tmp/blink.log`, to make it easy to file a feature request.
In other cases ABIs aren't supported simply because they're Linux-only
and difficult to polyfill on other POSIX platforms. For example, Blink
will polyfill `open(O_TMPFILE)` on non-Linux platforms so it works the
same way, but other Linux-specific ABIs like `membarrier()` we haven't
had the time to figure out yet. Since app developers usually don't use
non-portable APIs, it's usually the platform C library that's at fault
for calling them. Many Linux system calls, could be rightfully thought
of as an implementation detail of Glibc.

Blink's prime directive is to support binaries built with Cosmopolitan
Libc. Actually Portable Executables make up the bulk of Blink's unit
test suite. Anything created by Cosmopolitan is almost certainly going
to work very well. Since Cosmopolitan is closely related to Musl Libc,
programs compiled using Musl also tend to work very well. For example,
Alpine Linux is a Musl Libc based distro, so their prebuilt dynamic
binaries tend to all work well, and it's also a great platform to use
for compiling other software from source that's intended for Blink.

So the recommended approach is either:

1. Build your app using Cosmopolitan Libc, otherwise
2. Build your app using GNU Autotools on Alpine Linux
3. Build your app using Buildroot

For Cosmopolitan, please read [Getting Started with Cosmopolitan
Libc](https://jeskin.net/blog/getting-started-with-cosmopolitan-libc/)
for information on how to get started. Cosmopolitan comes with a lot of
third party software included that you can try with Blink right away,
e.g. SQLite, Python, QuickJS, and Antirez's Kilo editor.

```
git clone https://github.com/jart/cosmopolitan/
cd cosmopolitan

make -j8 o//third_party/python/python.com
blinkenlights -jm o//third_party/python/python.com

make -j8 o//third_party/quickjs/qjs.com
blinkenlights -jm o//third_party/quickjs/qjs.com

make -j8 o//third_party/sqlite3/sqlite3.com
blinkenlights -jm o//third_party/sqlite3/sqlite3.com

make -j8 o//examples/kilo.com
blinkenlights -jm o//examples/kilo.com
```

Blink is great for making single-file autonomous binaries like the above
easily copyable across platforms. If you're more interested in building
systems instead, then [Buildroot](https://buildroot.org/) is one way to
create a Linux userspace that'll run under Blink. All you have to do is
set the `$BLINK_OVERLAYS` environment variable to the buildroot target
folder, which will ask Blink to create a chroot'd environment.

```
cd ~/buildroot
export CC="gcc -Wl,-z,common-page-size=65536,-z,max-page-size=65536"
make menuconfig
make
cp -R output/target ~/blinkroot
doas mount -t devtmpfs none ~/blinkroot/dev
doas mount -t sysfs none ~/blinkroot/sys
doas mount -t proc none ~/blinkroot/proc
cd ~/blink
make -j8
export BLINK_OVERLAYS=$HOME/blinkroot
blink sh
uname -a
Linux hostname 4.5 blink-1.0 x86_64 GNU/Linux
```

If you want to build an Autotools project like Emacs, the best way to do
that is to spin up an Alpine Linux container and use
[jart/blink-isystem](https://github.com/jart/blink-isystem) as your
system header subset. blink-isystem is basically just the Musl Linux
headers with all the problematic APIs commented out. That way autoconf
won't think the APIs Blink doesn't have are available, and will instead
configure Emacs to use portable alternatives. Setting this up is simple:

```
./configure CFLAGS="-isystem $HOME/blink-isystem" \
            CXXFLAGS="-isystem $HOME/blink-isystem" \
            LDFLAGS="-static -Wl,-z,common-page-size=65536,-z,max-page-size=65536"
make -j
```

Another big issue is the host system page size may cause problems on
non-Linux platforms like Apple M1 (16kb) and Cygwin (64kb). On such
platforms, you may encounter an error like this:

```
p_vaddr p_offset skew unequal w.r.t. host page size
```

The simplest way to solve that is by disabling the linear memory
optimization (using the `blink -m` flag) but that'll slow down
performance. Another option is to try recompiling your executable so
that its ELF program headers will work on systems with a larger page
size. You can do that using these GCC flags:

```
gcc -static -Wl,-z,common-page-size=65536,-z,max-page-size=65536 ...
```

However that's just step one. The program also needs to be using APIs
like `sysconf(_SC_PAGESIZE)` which will return the true host page size,
rather than naively assuming it's 4096 bytes. Your C library gets this
information from Blink via `getauxval(AT_PAGESZ)`.

If you're using the Blinkenlights debugger TUI, then another important
set of flags to use are the following:

- `-fno-omit-frame-pointer`
- `-mno-omit-leaf-frame-pointer`

By default, GCC and Clang use the `%rbp` backtrace pointer as a general
purpose register, and as such, Blinkenlights won't be able to display a
frames panel visualizing your call stack. Using those flags solves that.
However it's tricky sometimes to correctly specify them in a complex
build environment, where other optimization flags might subsequently
turn them back off again.

The trick we recommend using for compiling your programs, is to create a
shell script that wraps your compiler command, and then use the script
in your `$CC` environment variable. The script should look something
like the following:

```sh
#!/bin/sh
set /usr/bin/gcc "$@" -g \
    -fno-omit-frame-pointer \
    -fno-optimize-sibling-calls \
    -mno-omit-leaf-frame-pointer \
    -Wl,-z,norelro \
    -Wl,-z,noseparate-code \
    -Wl,-z,max-page-size=65536 \
    -Wl,-z,common-page-size=65536
printf '%s\n' "$*" >>/tmp/gcc.log
exec "$@"
```

Those flags will go a long way towards helping your Linux binaries be
(1) capable of running under Blink on all of its supported operating
systems and microprocessor architectures, and (2) trading away some of
the modern security blankets in the interest of making the assembly
panel more readable, and less likely to be picky about memory.

If you're a Cosmopolitan Libc user, then Cosmopolitan already provides
such a script, which is the `cosmocc` and `cosmoc++` toolchain. Please
note that Cosmopolitan Libc uses a 64kb page size so it isn't impacted
by many of these issues that Glibc and Musl users may experience.

- [cosmopolitan/tool/scripts/cosmocc](https://github.com/jart/cosmopolitan/blob/master/tool/scripts/cosmocc)
- [cosmopolitan/tool/scripts/cosmoc++](https://github.com/jart/cosmopolitan/blob/master/tool/scripts/cosmoc%2B%2B)

If you're not a C / C++ developer, and you prefer to use high-level
languages instead, then one program you might consider emulating is
Actually Portable Python, which is an APE build of the CPython v3.6
interpreter. It can be built from source, and then used as follows:

```
git clone https://github.com/jart/cosmopolitan/
cd cosmopolitan
make -j8 o//third_party/python/python.com
blinkenlights -jm o//third_party/python/python.com
```

The `-jm` flags are helpful here, since they ask the Blinkenlights TUI
to enable JIT and the linear memory optimization. It's helpful to have
those flags because Python is a very complicated and compute intensive
program, that would otherwise move too slowly under the Blinkenlights
vizualization. You may also want to press the `CTRL-T` (TURBO) key a few
times, to make Python emulate in the TUI even faster.

## Technical Details

blink is an x86-64 interpreter for POSIX platforms that's written in
ANSI C11 that's compatible with C++ compilers. Instruction decoding is
done using our trimmed-down version of Intel's disassembler Xed.

The prime directive of this project is to act as a virtual machine for
userspace binaries compiled by Cosmopolitan Libc. However we've also had
success virtualizing programs compiled with Glibc and Musl Libc, such as
GCC and Qemu. Blink supports 500+ instructions and 150+ Linux syscalls,
including fork() and clone(). Linux system calls may only be used by
long mode programs via the `SYSCALL` instruction, as it is written in
the System V ABI.

### Instruction Sets

The following hardware ISAs are supported by Blink.

- i8086
- i386
- X87
- SSE2
- x86_64
- SSE3
- SSSE3
- CLMUL
- POPCNT
- ADX
- BMI2
- RDRND
- RDSEED
- RDTSCP

Programs may use `CPUID` to confirm the presence or absence of optional
instruction sets. Please note that Blink does not follow the same
monotonic progress as Intel's hardware. For example, BMI2 is supported;
this is an AVX2-encoded (VEX) instruction set, which Blink is able to
decode, even though the AVX2 ISA isn't supported. Therefore it's
important to not glob ISAs into "levels" (as Windows software tends to
do) where it's assumed that BMI2 support implies AVX2 support; because
with Blink that currently isn't the case.

On the other hand, Blink does share Windows' x87 behavior w.r.t. double
(rather than long double) precision. It's not possible to use 80-bit
floating point precision with Blink, because Blink simply passes along
floating point operations to the host architecture, and very few
architectures support `long double` precision. You can still use x87
with 80-bit words. Blink will just store 64-bit floating point values
inside them, and that's a legal configuration according to the x87 FPU
control word. If possible, it's recommended that `long double` simply be
avoided. If 64-bit floating point [is good enough for the rocket
scientists at
NASA](https://www.jpl.nasa.gov/edu/news/2016/3/16/how-many-decimals-of-pi-do-we-really-need/)
then it should be good enough for everybody. There are some peculiar
differences in behavior with `double` across architectures (which Blink
currently does nothing to address) but they tend to be comparatively
minor, e.g. an op returning `NAN` instead of `-NAN`.

Blink has reasonably comprehensive coverage of the baseline ISAs,
including even support for BCD operations (even in long mode!). But there
are some truly fringe instructions Blink hasn't implemented, such as
`BOUND` and `ENTER`. Most of the unsupported instructions, are usually
ring-0 system instructions, since Blink is primarily a user-mode VM, and
therefore only has limited support for bare metal operating system
software (which we'll discuss more in-depth in a later section).

Blink advertises itself as `Linux 4.5` in the `uname()` system call and
`uname -v` will report `blink-1.0`. Programs may detect they're running
in Blink by issuing a `CPUID` instruction where `EAX` is set to the leaf
number:

- Leaf `0x0` (or `0x80000000`) reports `GenuineIntel` in
  `EBX ‖ EDX ‖ ECX`

- Leaf `0x1` reports that Blink is a hypervisor in bit `31` of `ECX`

- Leaf `0x40000000` reports `GenuineBlink` as the hypervisor name in
  `EBX ‖ ECX ‖ EDX`

- Leaf `0x40031337` reports the underlying operating system name in
  `EBX ‖ ECX ‖ EDX` with zero filling for strings shorter than 12:

  - `Linux` for Linux
  - `XNU` for macOS
  - `FreeBSD` for FreeBSD
  - `NetBSD` for NetBSD
  - `OpenBSD` for OpenBSD
  - `Linux` for Linux
  - `Cygwin` for Windows under Cygwin
  - `Windows` for Windows under Cosmopolitan
  - `Unknown` if compiled on unrecognized platform

- Leaf `0x80000001` tells if Blink's JIT is enabled in bit `31` in `ECX`

### JIT

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
fully perfect on all platforms, the `blink -j` flag may be passed to
disable Blink's JIT. Please note that disabling JIT makes Blink go 10x
slower. With the `blinkenlights` command, the `-j` flag takes on the
opposite meaning, where it instead *enables* JIT. This can be useful for
troubleshooting the JIT, because the TUI display has a feature that lets
JIT path formation be visualized. Blink currently only enables the JIT
for programs running in long mode (64-bit) but we may support JITing
16-bit programs in the future.

### Virtualization

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

### Self-Modifying Code

Many CPU architectures require esoteric rituals for flushing CPU caches
when code modifies itself. That's not the case with x86 archihtecture,
which takes care of this chore automatically. Blink is able to offer the
same promises here as Intel and AMD, by abstracting fast and automatic
invalidation of caches for programs using self-modifying code (SMC).

When Blink's JIT isn't enabled, self-modifying code always causes
instruction caches to be invalidated immediately, at least within the
same thread. That's because Blink compares the raw instruction bytes
with what's in the instruction cache before fetching its decoded value.

When JITing is enabled, Blink will automatically invalidate JIT memory
associated with code that's been modified. This happens on a 4096-byte
page granularity. When a function like mprotect() is called that causes
memory pages to transition from a non-executable to executable state,
the impacted pages will be invalidated by the JIT. The JIT maintains a
hash table where the key is the virtual address at which a generated
function begins (which we call a "path") and the value is a function
pointer to the generated code. When Blink is generating paths, it is
careful to ensure that all the guest instructions which are added to a
page, only exist within the confines of a single 4096-byte page. Thus
when a page needs to be invalidated, Blink simply deletes any hook for
each address within the page.

When RWX memory is used, Blink can't rely on mprotect() to communicate
the intent of the guest program. What Blink will do instead is protect
any RWX guest memory, so that it's registered as read-only in the host
operating system. This way, whenever the guest writes to RWX memory, a
SIGSEGV signal will be delivered to Blink, which then re-enables write
permissions on the impacted RWX page, flips a bit to the thread in the
SMC state and then permits execution to resume for at least one opcode
before the interpreter loop notices the SMC state, invalidates the JIT
and re-enables the memory protection. This means that:

1. Memory ops in general aren't slowed down by Blink's SMC support
2. RWX memory can be written-to with some overhead
3. RWX memory can be read-from with zero overhead
4. Changes take effect when a JIT path ends

Intel's sixteen thousand page manual lays out the following guidelines
for conformant self-modifying code:

> To write self-modifying code and ensure that it is compliant with
> current and future versions of the IA-32 architectures, use one of
> the following coding options:
>
> (* OPTION 1 *)  
> Store modified code (as data) into code segment;  
> Jump to new code or an intermediate location;  
> Execute new code;
>
> (* OPTION 2 *)  
> Store modified code (as data) into code segment;  
> Execute a serializing instruction; (* For example, CPUID instruction *)  
> Execute new code;
>
> ──Quoth Intel Manual V.3, §8.1.3

Blink implements this behavior because branching instructions always
cause JIT paths to end, and serializing instructions are never added to
JIT paths in the first place.

Intel's rules allow Blink some leeway to make writiting to make this RWX
memory technique go fast without causing signal storms or incurring much
system call overhead. As an example, consider the internal statistics
printed by the [`smc2_test.c`](test/func/smc2_test.c) program:

```
make -j8 o//blink/blink o//test/func/smc2_test.elf
o//blink/blink -Z o//test/func/smc2_test.elf
[...]
icache_resets                    = 1
jit_blocks                       = 1
jit_hooks_installed              = 132
jit_hooks_deleted                = 19
jit_page_resets                  = 21
smc_checks                       = 22
smc_flushes                      = 22
smc_enqueued                     = 22
smc_segfaults                    = 22
[...]
```

The above program performs 300+ independent write operations to RWX
memory. However we can see very few of them resulted in segfaults, since
most of those ops happened in the SlowMemCpy() function which uses a
tight conditional branch loop rather than a proper jump. This let the
program get more accomplished, before dropping out of JIT code back into
the main interpreter loop, which is where Blink resets the SMC state.

## Pseudoteletypewriter

Blink has an xterm-compatible ANSI pseudoteletypewriter display
implementation which allows Blink's TUI interface to host other TUI
programs, within an embedded terminal display. For example, it's
possible to use Antirez's Kilo text editor inside Blink's TUI. For the
complete list of ANSI sequences which are supported, please refer to
[blink/pty.c](blink/pty.c).

In real mode, Blink's PTY can be configured via `INT $0x16` to convert
CGA memory stored at address `0xb0000` into UNICODE block characters,
thereby making retro video gaming in the terminal possible.

## Real Mode

Blink supports 16-bit BIOS programs, such as SectorLISP. To boot real
mode programs in Blink, the `blinkenlights -r` flag may be passed, which
puts the virtual machine in i8086 mode. Currently only a limited set of
BIOS APIs are available. For example, Blink supports IBM PC Serial UART,
CGA, and MDA. We hope to expand our real mode support in the near
future, in order to run operating systems like ELKS.

Blink supports troubleshooting operating system bootloaders. Blink was
designed for Cosmopolitan Libc, which embeds an operating system in each
binary it compiles. Blink has helped us debug our bare metal support,
since Blink is capable of running in the 16-bit, 32-bit, and 64-bit
modes a bootloader requires at various stages. In order to do that, we
needed to implement some ring0 hardware instructions. Blink has enough
to support Cosmopolitan, but it'll take much more time to get Blink to a
point where it can boot something like Windows.

## Executable Formats

Blink supports several different executable formats. You can run:

- x86-64-linux ELF executables (both static and dynamic).

- Actually Portable Executables, which have either the `MZqFpD` or
  `jartsr` magic.

- Flat executables, which must end with the file extension `.bin`. In
  this case, you can make executables as small as 10 bytes in size,
  since they're treated as raw x86-64 code. Blink always loads flat
  executables to the address `0x400000` and automatically appends 16mb
  of BSS memory.

- Real mode executables, which are loaded to the address `0x7c00`. These
  programs must be run using the `blinkenlights` command with the `-r`
  flag.

## Quirks

Here's the current list of Blink's known quirks and tradeoffs.

### Flags

Flag dependencies may not carry across function call boundaries under
long mode. This is because when Blink's JIT is speculating whether or
not it's necessary for an arithmetic instruction to compute flags, it
considers `RET` and `CALL` terminal ops that break the chain. As such
64-bit code shouldn't do things we did in the DOS days, such as using
carry flag as a return value to indicate error. This should work fine
when `STC` is used to set the carry flag, but if the code computes it
cleverly using instructions like `SUB`, then EFLAGS might not change.

### Faults

Blink may not report the precise program counter where a fault occurred
in `ucontext_t::uc_mcontext::rip` when signalling a segmentation fault.
This is currently only possible when `PUSH` or `POP` access bad memory.
That's because Blink's JIT tries to avoid updating `Machine::ip` on ops
it considers "pure" such as those that only access registers, which for
reasons of performance is defined to include pushing and popping.

### Threads

Blink doesn't have a working implementation of `set_robust_list()` yet,
which means robust mutexes might not get unlocked if a process crashes.

### Coherency

POSIX.1 provides almost no guarantees of coherency, synchronization, and
durability when it comes to `MAP_SHARED` mappings and recommends that
msync() be explicitly used to synchronize memory with file contents. The
Linux Kernel implements shared memory so well, that this is rarely
necessary. However some platforms like OpenBSD lack write coherency.
This means if you change a shared writable memory map and then call
pread() on the associated file region, you might get stale data. Blink
isn't able to polyfill incoherent platforms to be as coherent as Linux,
therefore apps that run in Blink should assume the POSIX rules apply.

### Signal Handling

Blink uses `SIGSYS` to deliver signals internally. This signal is
precious to Blink. It's currently not possible for guest applications to
capture it from external processes.

### Memory Protection

Blink offers guest programs a 48-bit virtual address space with a
4096-byte page size. When programs are run on (1) host systems that have
a larger page (e.g. Apple M1, Cygwin), and (2) the linear memory
optimization is enabled (i.e. you're *not* using `blink -m`) then Blink
may need to relax memory protections in cases where the memory intervals
defined by the guest aren't aligned to the host system page size. This
means that, on system with a larger than 4096 byte page size:

1. Misaligned read-only pages could become writable
2. JIT hooks might not invalidate automatically on misaligned RWX pages

It's recommended, when calling functions like mmap() and mprotect(),
that both `addr` and `addr + size` be aliged to the host page size.
Blink reports that value to the guest program in `getauxval(AT_PAGESZ)`,
which should be obtainable via the POSIX API `sysconf(_SC_PAGESIZE)` if
the C library is implemented correctly. Please note that when Blink is
running in full virtualization mode (i.e. `blink -m`) this concern no
longer applies. That's because Blink will allocate a full system page
for every 4096 byte page that's mapped from a file.
