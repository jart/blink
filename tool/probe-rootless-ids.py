#!/usr/bin/env python3
"""Check build-user identity and filesystem ownership without host ID changes."""
import ctypes
import os
from pathlib import Path
import tempfile

assert os.getresuid() == (0, 0, 0)
assert os.getresgid() == (0, 0, 0)
os.setresgid(10, 20, 30)
os.setresuid(100, 200, 300)
assert os.getresuid() == (100, 200, 300)
assert os.getresgid() == (10, 20, 30)
libc = ctypes.CDLL(None)
assert libc.setfsuid(400) == 200
assert libc.setfsgid(40) == 20
with tempfile.TemporaryDirectory() as tmp:
    p = Path(tmp) / "file"
    p.write_text("filesystem IDs")
    assert (p.stat().st_uid, p.stat().st_gid) == (400, 40)
assert os.geteuid() == 200 and os.getegid() == 20
assert libc.setfsuid(-1) == 400 and libc.setfsgid(-1) == 40
os.setresuid(-1, 201, -1)
os.setresgid(-1, 21, -1)
assert os.getresuid() == (100, 201, 300)
assert os.getresgid() == (10, 21, 30)
assert libc.setfsuid(-1) == 201 and libc.setfsgid(-1) == 21
os.setgid(1000)
os.setuid(1000)
assert os.getresuid() == (1000, 1000, 1000)
assert os.getresgid() == (1000, 1000, 1000)
print("build identity and filesystem-ID ownership passed")
