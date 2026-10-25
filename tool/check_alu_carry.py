#!/usr/bin/env python3
"""Check warmed guest arithmetic against host integer arithmetic.

Pass a Blink runtime, compiled tool/alu_carry_probe.c and output JSON path.
"""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

runtime, probe, output = map(Path, sys.argv[1:])
results = []
with tempfile.TemporaryDirectory(prefix="blink-alu-") as root:
    shutil.copy2(probe, Path(root) / "probe")
    for mode in ["-m", "-jm"]:
        run = subprocess.run([str(runtime.resolve()), mode, "/probe"],
            env={**os.environ, "BLINK_PREFIX": root}, capture_output=True,
            text=True, timeout=60)
        failures = []
        lines = run.stdout.splitlines()
        for line in lines:
            bits, op, carry, x, actual, cf, zf = line.split()
            bits, carry, x = int(bits), int(carry), int(x)
            mask = (1 << bits) - 1
            cin = carry if op in ("adc", "sbb") else 0
            wide = x + mask + cin if op in ("add", "adc") else x - mask - cin
            expected = [wide & mask, int(wide > mask if op in ("add", "adc") else wide < 0), int((wide & mask) == 0)]
            if list(map(int, [actual, cf, zf])) != expected:
                failures.append({"actual": line, "expected": expected})
        results.append({"mode": mode, "exit_code": run.returncode,
                        "cases": len(lines), "failures": failures, "stderr": run.stderr})
output.write_text(json.dumps({"runtime_sha256": hashlib.sha256(runtime.read_bytes()).hexdigest(),
    "probe_sha256": hashlib.sha256(probe.read_bytes()).hexdigest(), "results": results}, indent=2) + "\n")
for row in results:
    print(row["mode"], "cases", row["cases"], "failures", len(row["failures"]))
sys.exit(any(r["exit_code"] or r["cases"] != 72 or r["failures"] for r in results))
