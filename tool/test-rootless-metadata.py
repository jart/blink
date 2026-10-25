#!/usr/bin/env python3
"""Run rootless metadata regressions against a prepared Debian/Python rootfs.

Use an idle rootfs. This runs trusted guests directly under Blink, without
fakeroot; the rootless backend is metadata bookkeeping, not a sandbox.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--blink", type=Path, required=True)
    parser.add_argument("--rootfs", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    runtime, root = args.blink.resolve(strict=True), args.rootfs.resolve(strict=True)
    scripts = Path(__file__).parent
    probes = ["probe-creation-ownership.py", "probe-rootless-lifetime.py",
              "probe-rootless-pipe.py", "probe-rootless-chown-mode.py",
              "probe-rootless-groups.py", "probe-rootless-ids.py"]
    results = []
    env = {**os.environ, "BLINK_PREFIX": str(root), "BLINK_ROOTLESS": "1"}
    env.pop("LD_PRELOAD", None)
    with tempfile.TemporaryDirectory(prefix="rootless-tests-", dir=root / "tmp") as tmp:
        directory = Path(tmp)
        for probe in probes:
            shutil.copy2(scripts / probe, directory / probe)
        guest = "/" + str(directory.relative_to(root))
        for mode in ("-m", "-jm"):
            fixture = directory / ("fixture" + mode)
            fixture.mkdir()
            commands = [
                ["/usr/bin/python3", "-c", "import os; os.setgid(1000); os.setuid(1000); "
                 "os.execv('/usr/bin/python3', ['python3', " + repr(guest + "/" + probes[0]) + "])"] ,
                ["/usr/bin/python3", guest + "/" + probes[1], "create", guest + "/" + fixture.name],
                ["/usr/bin/python3", guest + "/" + probes[1], "check", guest + "/" + fixture.name],
                ["/usr/bin/python3", guest + "/" + probes[2]],
                ["/usr/bin/python3", guest + "/" + probes[3]],
                ["/usr/bin/python3", guest + "/" + probes[4]],
                ["/usr/bin/python3", guest + "/" + probes[5]],
            ]
            for argv in commands:
                p = subprocess.run([str(runtime), mode, *argv], env=env,
                                   capture_output=True, text=True, timeout=60)
                results.append({"mode": mode, "argv": argv, "exit_code": p.returncode,
                                "stdout": p.stdout, "stderr": p.stderr})
                print(mode, argv[1], p.returncode, flush=True)
    report = {"runtime_sha256": hashlib.sha256(runtime.read_bytes()).hexdigest(),
              "probes": {name: hashlib.sha256((scripts / name).read_bytes()).hexdigest()
                         for name in probes}, "results": results}
    args.output.write_text(json.dumps(report, indent=2) + "\n")
    return int(any(row["exit_code"] for row in results))


if __name__ == "__main__":
    raise SystemExit(main())
