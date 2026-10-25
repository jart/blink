#!/usr/bin/env python3
"""Diagnose glibc DNS and intermittent APT crashes with an existing Debian rootfs.

Use a disposable, trusted trixie-slim AMD64 rootfs with no APT package indexes.
This script never downloads an image or changes resolver settings. Blink's VFS
is not a security sandbox. DNS testing needs network access; the APT case
deliberately exercises the missing-package error path without installing packages.
"""
import argparse
import json
import os
from pathlib import Path
import signal
import subprocess


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blink", type=Path, required=True)
    parser.add_argument("--rootfs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", choices=("dns", "apt", "fork"), required=True)
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--timeout", type=int, default=20)
    parser.add_argument("--trace", action="store_true")
    args = parser.parse_args()
    if args.repeat < 1 or args.timeout < 1:
        parser.error("repeat and timeout must be positive")
    blink = args.blink.resolve(strict=True)
    root = args.rootfs.resolve(strict=True)
    if args.case == "apt":
        # Keep this reproduction on the no-index error path observed originally.
        indexes = [p for p in (root / "var/lib/apt/lists").rglob("*")
                   if p.is_file() and p.name != "lock"]
        caches = [root / "var/cache/apt" / name
                  for name in ("pkgcache.bin", "srcpkgcache.bin")]
        if indexes or any(p.exists() for p in caches):
            parser.error("APT probe requires a fresh rootfs without indexes/caches")
    commands = {
        "dns": ["/usr/bin/getent", "ahostsv4", "deb.debian.org"],
        "fork": ["/bin/bash", "-c",
                 "for i in 1 2 3 4 5; do /usr/bin/true | /usr/bin/cat; done"],
        "apt": ["/usr/bin/apt-get", "install", "-y", "--no-install-recommends",
                "git", "curl", "ca-certificates", "python3", "dumb-init"],
    }
    expected = 100 if args.case == "apt" else 0
    args.output.mkdir(parents=True, exist_ok=False)
    results = []
    for mode in ("-m", "-jm"):
        for iteration in range(args.repeat):
            prefix = args.output / (args.case + mode + "-" + str(iteration))
            command = [str(blink), mode, "-C", str(root),
                       "-L", str(prefix.with_suffix(".log").resolve())]
            if args.trace:
                command.append("-sss")
            command.extend(commands[args.case])
            with prefix.with_suffix(".stdout").open("wb") as stdout, \
                    prefix.with_suffix(".stderr").open("wb") as stderr:
                child = subprocess.Popen(
                    command, env={"PATH": "/usr/bin:/bin:/usr/sbin:/sbin", "HOME": "/root"},
                    stdin=subprocess.DEVNULL, stdout=stdout, stderr=stderr,
                    start_new_session=True)
                timed_out = False
                try:
                    child.wait(timeout=args.timeout)
                except subprocess.TimeoutExpired:
                    timed_out = True
                    os.killpg(child.pid, signal.SIGKILL)
                    child.wait()
            error = prefix.with_suffix(".stderr").read_bytes()
            sigbus = b"SIGBUS" in error or child.returncode == -signal.SIGBUS
            row = dict(case=args.case, mode=mode, iteration=iteration,
                       returncode=child.returncode, expected_returncode=expected,
                       sigbus=sigbus, timeout=timed_out, artifacts=str(prefix),
                       passed=child.returncode == expected and not sigbus and not timed_out)
            results.append(row)
            print(json.dumps(row), flush=True)
            (args.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    return 0 if all(row["passed"] for row in results) else 1


if __name__ == "__main__":
    raise SystemExit(main())
