# blink

blink is a virtual machine for running x86-64-linux programs on different
operating systems and hardware architectures. It's designed to do the
same thing as the `qemu-x86_64` command, except rather than being a 10mb
binary, blink only has a ~146kb footprint. For further details on the
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

Mutex lock tests sometimes flake in a very specific way after joining
threads on qemu-aarch64, qemu-mips, and qemu-s390x:

```
// this happens on aarch64, mips, and s390x
error:test/libc/intrin/pthread_mutex_lock2_test.c:95: pthread_mutex_lock_contention(pthread_mutex_lock_recursive) on blink.local pid 22952 tid 22952
        EXPECT_EQ(THREADS, started)
                need 16 (or 0x10 or '►') =
                 got 15 (or 0xf or '☼')
        EUNKNOWN/0/No error information
        third_party/cosmo/pthread_mutex_lock2_test.com @ blink.local
1 / 284 tests failed
make: *** [test/test.mk:90: o//mips64/third_party/cosmo/pthread_mutex_lock2_test.com.emulates] Error 1

// instructions in question
  40d1fe:       f0 83 05 8e c2 06 00 01         lock addl $0x1,0x6c28e(%rip)        # 479494 <started>
...
  40d3c2:       4c 63 25 cb c0 06 00    movslq 0x6c0cb(%rip),%r12        # 479494 <started>

```

*NSYNC unit tests sometimes flake:

```
I2022-11-21T00:43:58.774807:blink/throw.c:59: 262144: SEGMENTATION FAULT AT ADDRESS 8
         PC 41b77f mov %rsi,8(%rcx)
         AX 00001000802985e8  CX 0000000000000000  DX 00001000802985e8  BX 0000100080252fe0
         SP 000010008007fcf0  BP 000010008007fcf0  SI 00001000802985e8  DI 0000000000486820
         R8 0000000000433ca0  R9 0000000000000004 R10 0000000000000000 R11 0000000000000000
        R12 00001000802985e0 R13 0000000000000004 R14 0000100080252fe8 R15 000000000000001d
         FS 0000100080040240  GS 0000000000000000 OPS 5555954          JIT 0
        third_party/cosmo/wait_test.com
        10008007fcf0 00000041b77f nsync_dll_make_last_in_list_+0x2f 0 bytes
        10008007fd20 00000040d0c9 counter_enqueue+0x39 48 bytes
        10008007fe90 00000040e289 nsync_wait_n+0x199 368 bytes
        10008007ff20 00000040a9c3 test_wait_n+0x3d3 144 bytes
        10008007ff50 00000040b1ad run_test+0x6d 48 bytes
        10008007ff70 00000040adbb closure_f0_testing+0x1b 32 bytes
        10008007ff80 00000040ab0b closure_run_body+0xb 16 bytes
        10008007ffa0 00000040ac6a body+0x1a 32 bytes
        10008007fff0 00000040c05c PosixThread+0xac 80 bytes
        000000000000 00000042f7e3 sys_clone_linux+0x26
I2022-11-21T00:43:58.774829:blink/syscall.c:195: 262144: halting machine from thread: -4
make: *** [third_party/cosmo/cosmo.mk:20: o//third_party/cosmo/wait_test.com.ok] Error 252

I2022-11-21T01:28:26.296387:blink/throw.c:59: 262144: SEGMENTATION FAULT AT ADDRESS 3450
         PC 41b80e mov (%rdi),%rcx
         AX 000010008007fda8  CX 0000000000000000  DX 000010008007fda8  BX 000010008004bfe0
         SP 000010008007fcf0  BP 000010008007fcf0  SI 000010008007fda8  DI 0000000000003450
         R8 0000000000433c40  R9 0000000000000001 R10 0000000000465740 R11 0000000000000000
        R12 000010008007fda0 R13 0000000000000001 R14 000010008004bfe8 R15 0000000000000001
         FS 0000100080040240  GS 0000000000000000 OPS 532982           JIT 0
        third_party/cosmo/once_test.com
        10008007fcf0 00000041b80e nsync_dll_make_last_in_list_+0x1e 0 bytes
        10008007fd20 00000040cbe9 counter_enqueue+0x39 48 bytes
        10008007fe90 00000040d7e9 nsync_wait_n+0x199 368 bytes
        10008007fee0 00000040cdd1 nsync_counter_wait+0x41 80 bytes
        10008007ff20 00000040a52c test_once_run+0xec 64 bytes
        10008007ff50 00000040ad9d run_test+0x6d 48 bytes
        10008007ff70 00000040a9ab closure_f0_testing+0x1b 32 bytes
        10008007ff80 00000040a6fb closure_run_body+0xb 16 bytes
        10008007ffa0 00000040a85a body+0x1a 32 bytes
        10008007fff0 00000040bc4c PosixThread+0xac 80 bytes
        000000000000 00000042f7e3 sys_clone_linux+0x26
I2022-11-21T01:28:26.303859:blink/syscall.c:195: 262144: halting machine from thread: -4
make: *** [third_party/cosmo/cosmo.mk:21: o/asan/third_party/cosmo/once_test.com.ok] Error 252

I2022-11-21T01:30:50.158305:blink/throw.c:59: 262144: SEGMENTATION FAULT AT ADDRESS 8
         PC 40e177 mov 8(%rax),%rdx
         AX 0000000000000000  CX 7fffffffffffffff  DX 0000100080040140  BX 000010008004ef80
         SP 000010008007fd30  BP 000010008007fe90  SI 000010008007fd1c  DI 0000000000000001
         R8 000000003b9ac9ff  R9 000000000000000f R10 0000000000000000 R11 0000000000000000
        R12 0000000000000000 R13 7fffffffffffffff R14 000000000000000e R15 0000000000000000
         FS 0000100080040240  GS 0000000000000000 OPS 1993605          JIT 0
        third_party/cosmo/wait_test.com
        10008007fe90 00000040e177 nsync_wait_n+0x87 352 bytes
        10008007ff20 00000040a9c3 test_wait_n+0x3d3 144 bytes
        10008007ff50 00000040b1ad run_test+0x6d 48 bytes
        10008007ff70 00000040adbb closure_f0_testing+0x1b 32 bytes
        10008007ff80 00000040ab0b closure_run_body+0xb 16 bytes
        10008007ffa0 00000040ac6a body+0x1a 32 bytes
        10008007fff0 00000040c05c PosixThread+0xac 80 bytes
        000000000000 00000042f7e3 sys_clone_linux+0x26
I2022-11-21T01:30:50.163956:blink/syscall.c:195: 262144: halting machine from thread: -4
make: *** [third_party/cosmo/cosmo.mk:20: o/tsan/third_party/cosmo/wait_test.com.ok] Error 252

I2022-11-21T00:05:25.934780:blink/throw.c:59: 262144: SEGMENTATION FAULT AT ADDRESS 0
         PC 40e180 call (%rdx)
         AX 00001000803ebe30  CX 0000000000000000  DX 0000000000000000  BX 000010008022bc00
         SP 000010008007fd30  BP 000010008007fe90  SI 0000000000000000  DI 00001000803e84d0
         R8 000000003b9ac9ff  R9 000000000000001e R10 0000000000000000 R11 0000000000000000
        R12 0000000000000002 R13 7fffffffffffffff R14 000000000000001d R15 0000000000000002
         FS 0000100080040240  GS 0000000000000000 OPS 2347174          JIT 0
        third_party/cosmo/wait_test.com
        10008007fe90 00000040e180 nsync_wait_n+0x90 352 bytes
        10008007ff20 00000040a9c3 test_wait_n+0x3d3 144 bytes
        10008007ff50 00000040b1ad run_test+0x6d 48 bytes
        10008007ff70 00000040adbb closure_f0_testing+0x1b 32 bytes
        10008007ff80 00000040ab0b closure_run_body+0xb 16 bytes
        10008007ffa0 00000040ac6a body+0x1a 32 bytes
        10008007fff0 00000040c05c PosixThread+0xac 80 bytes
        000000000000 00000042f7e3 sys_clone_linux+0x26
I2022-11-21T00:05:25.934796:blink/syscall.c:195: 262144: halting machine from thread: -4
```
