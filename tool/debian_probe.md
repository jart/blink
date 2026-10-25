# Debian userspace diagnostics on macOS ARM64

These probes use a separately prepared, disposable Debian trixie-slim AMD64
rootfs. They require a VFS-enabled Blink (`./configure --enable-vfs`). They do
not download or unpack an image. Blink exposes host resources; use trusted
executables only. The rootfs should contain a usable `/etc/resolv.conf` for DNS.

```sh
python3 tool/debian_probe.py --blink o/blink/blink --rootfs /path/to/rootfs \
  --output o/probe-dns --case dns --trace
python3 tool/debian_probe.py --blink o/blink/blink --rootfs /path/to/rootfs \
  --output o/probe-apt --case apt --repeat 10
python3 tool/debian_probe.py --blink o/blink/blink --rootfs /path/to/rootfs \
  --output o/probe-fork --case fork
```

Each invocation requires a new output directory and runs both `-m` and `-jm`.
It retains stdout, stderr, Blink logs and a JSON summary. Timeouts kill the
process group. The APT probe requires no package indexes/caches and expects exit
100 (missing packages); any SIGBUS in child stderr still makes the probe fail.
It is not a package installation success test. Use a fresh rootfs without APT
indexes so packages cannot install. DNS requires network access; the fork probe
and APT missing-package probe do not need network access.

## Initial findings after the brk page-size fix

- Host DNS succeeds, but Debian `getent ahostsv4 deb.debian.org` exits 2 in
  both modes. Its trace ends with `setsockopt(SOL_IP, IP_RECVERR)` returning
  EINVAL, after which glibc closes the socket without sending a DNS query.
- macOS does not expose Linux's IP_RECVERR option. glibc's
  [resolver reopen path](https://github.com/bminor/glibc/blob/release/2.41/master/resolv/res_send.c)
  treats failure of `__res_enable_icmp` as fatal. Merely changing EINVAL to
  ENOPROTOOPT would not enable this path. Proper error-reporting compatibility
  needs investigation; the current PR does not pretend the option is supported.
- In an initial 10-run APT error-path sample, SIGBUS appeared twice with JIT
  and zero times without JIT. This suggests a JIT interaction but does not prove
  the cause. Traced runs and simple Bash fork/pipe runs passed. APT can still
  return 100 when its child crashes, so checking only the top-level status misses
  this problem.
- A subsequent five-run sample per mode using this tool did not reproduce
  SIGBUS. The failure remains intermittent; a passing run does not establish
  that it is fixed.

The tested base manifest was
`sha256:4876fdea29cac1a33a3195f1eb1952feb1fca7dd6dd00be3e2f1879cf20d34d2`.
These are opt-in diagnostic tools, separate from the deterministic function-test
suite.

## Fork/JIT follow-up

Temporary macOS ARM64 signal instrumentation located the host fault PC and fault
address inside `g_code`, reached from `ExecuteInstruction` while calling an
inherited compiled path immediately after guest clone/fork. That instrumentation
is not part of the patch.

The fork child now restores executable JIT access with `FixJitProtection`, using
the existing Apple write-protection helper. A clean control build reproduced
SIGBUS in 5/30 JIT APT probes and 0/30 interpreter probes. With the fix, all 30
probes per mode passed with no SIGBUS. This is integration evidence for an
intermittent failure, not proof against every possible SIGBUS. Use `--repeat 30` with
the command above to repeat this integration check. Simple warmed-code fork
smokes also pass before the fix, so they are not a regression reproducer.

## Next Debian build barriers

Setting guest `RES_OPTIONS=use-vc` enables glibc's TCP DNS path and permits APT
index downloads. It does not implement UDP IP_RECVERR compatibility.

After downloading the trixie indexes, `apt-cache gencaches` fails when growing
its default cache. The syscall trace contains:

```text
mremap(0x2000011ec000, 0x1800000, 0x1900000, 0x1, 0) -> -ENOMEM
```

At that checkpoint `SysMremap` returned ENOMEM unconditionally. The diagnostic command
`apt-cache -o APT::Cache-Start=134217728 gencaches` succeeds with those indexes.
Starting with a larger allocation bypasses this particular growth request; it
does not add mremap support or establish package installation compatibility.

APT then reached dependency resolution but failed its archive-directory space
check: `statvfs (2: No such file or directory)`. `SysStatfs` was calling host
`statfs` directly with a guest path, while `SysFstatfs` likewise bypassed VFS
descriptor translation. Both now dispatch through VFS; host-backed files use
the resolved host path or descriptor. Other filesystem implementations without
these operations return ENOSYS.

`test/func/statfs_vfs_test.c` creates a fresh guest directory and checks pathname,
descriptor, symlink, missing-path, and invalid-descriptor behavior. Under a VFS
root the old binary exits 2; the patched binary passes with `-m` and `-jm`.
The VFS-disabled code path also passes a native compilation check. With the
patch, APT gets past the space check and starts downloading the 50 dependencies.

## Private anonymous mremap support

Non-linear mode now supports resizing non-executable, private anonymous mappings
with uniform protection. It validates the source before changing it, grows in
place when possible, and relocates page-table entries when MAYMOVE is allowed.
Resident data and lazy zero pages are preserved without copying the cache.
File-backed/shared, executable, linear-mode, FIXED, and DONTUNMAP cases remain
unsupported. This is partial support, not a complete implementation of
[Linux mremap](https://man7.org/linux/man-pages/man2/mremap.2.html).

`tool/mremap_anon_probe.c` covers forced relocation, growth without MAYMOVE,
zero filling, shrinking, read-only mappings, malformed requests, and rejecting
shared mappings without damaging them. Compile it as a static AMD64 Linux ELF,
copy it into the supplied rootfs, and run it with `-m` and `-jm`. It is an opt-in
probe because linear mode is not yet implemented. The original stub exits 5;
both patched modes pass. For example:

```sh
x86_64-linux-musl-gcc -static -O tool/mremap_anon_probe.c -o /tmp/remap-probe
cp /tmp/remap-probe /path/to/rootfs/tmp/remap-probe
o/blink/blink -m -C /path/to/rootfs /tmp/remap-probe
o/blink/blink -jm -C /path/to/rootfs /tmp/remap-probe
```

With the trixie indexes already downloaded, `apt-cache gencaches` now exits 0
using its default cache size, without the Cache-Start override. Its syscall
trace shows successful growth both in place and to new addresses.

## Package installation and scripts

Debian's `fakeroot`/`libfakeroot` 1.37.1.1-1 packages were downloaded with APT
and their data archives extracted into the disposable root. `fakeroot-tcp`
works through Blink: `id` reports root; a guest `chown 1000:1000` is visible
through guest stat and saved in its `-s` metadata file, while host ownership
remains the macOS account's. This is an external userspace component, not
native guest credentials or permission enforcement in Blink.

Running APT through it reached dpkg unpacking and exposed two further defects:

- `VfsUtime` initialized its result to -1 and only changed it while following
  symlinks. With AT_SYMLINK_NOFOLLOW it never called the backend and returned
  stale errno. Initialize the result to 0 instead. The new
  `test/func/utimens_nofollow_test.c` fails at the regular-file update before
  the fix (exit 3) and passes in both modes afterward. It also verifies that
  updating a symlink does not update its target.
- Python's maintainer script starts with `#! /bin/sh`. The old shebang parser
  treated that leading space as an empty interpreter name and fell back to
  host execution. It now accepts leading spaces/tabs, separates an optional
  argument, and trims trailing spaces/tabs as Linux does. The new
  `test/func/shebang_space_test.c` fails on the spaced form before the fix
  (exit 11), then passes canonical, spaced, and tabbed forms with optional
  arguments in both modes. It checks that the interpreter reports Linux.

## Node event loop checkpoint

The AMD64 `node:22-bookworm-slim` manifest
`sha256:ea11b74b986d520137cdef9a5346e447a00f275ecc4abc0055b7151226231e09`
contains five layers. `node --version` reports v22.23.4 under Blink, but
`node -e` aborts in libuv loop initialization after epoll_create1 returns
ENOSYS. Blink's epoll handlers are conditional on HAVE_EPOLL_PWAIT1, which is
absent on this macOS build.

`tool/epoll_probe.c` isolates the prerequisite without Node. Compile it as a
static AMD64 ELF and run inside the rootfs in each mode; both currently print
`epoll_create1: Function not implemented` and exit 1. The later checks cover
CLOEXEC, socket readiness, event data, and removal, for validating a future
implementation. No epoll compatibility patch is included yet.

## CPU enumeration through procfs

`tool/proc_stat_probe.c` is an opt-in Linux guest probe for VFS builds. Compile
it as a static x86-64 Linux executable, place it in the guest rootfs, and run
it with `BLINK_PREFIX=/path/to/rootfs o/blink/blink -jm /tmp/proc_stat_probe`
and again with `-m`. It checks CPU rows, partial reads, pread preserving the
sequential position, EOF and rewind. The old VFS lacks `/proc/stat`; the patched
macOS build reports all 10 host CPUs and passes both modes.

On macOS, `/proc/stat` now supplies aggregate and per-CPU user, nice, system
and idle times from Mach, converted to Linux USER_HZ. Unsupported accounting
categories are zero; other Linux stat rows are not synthesized. Linux hosts
read their native `/proc/stat`. Other host implementations remain unsupported.
With the separate epoll/eventfd prototype, Node 22.23.4's `os.cpus()` now returns
10 entries instead of an empty list. Model and frequency remain unknown because
`/proc/cpuinfo` and CPU-frequency sysfs files are not implemented.
