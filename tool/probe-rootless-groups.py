#!/usr/bin/env python3
"""Guest group state must not expose or modify host supplementary groups."""
import ctypes
import errno
import json
import os
import subprocess

assert os.getgroups() == [], os.getgroups()
os.setgroups([42, 7, 42])
assert os.getgroups() == [7, 42, 42]
libc = ctypes.CDLL(None, use_errno=True)
short = (ctypes.c_uint * 1)()
assert libc.getgroups(1, short) == -1 and ctypes.get_errno() == errno.EINVAL
invalid = (ctypes.c_uint * 1)(0xffffffff)
assert libc.setgroups(1, invalid) == -1 and ctypes.get_errno() == errno.EINVAL
assert os.getgroups() == [7, 42, 42]
pid = os.fork()
if pid == 0:
    os.setgroups([99])
    os._exit(0 if os.getgroups() == [99] else 1)
assert os.waitpid(pid, 0)[1] == 0
assert os.getgroups() == [7, 42, 42]
result = subprocess.check_output(["/usr/bin/python3", "-c", "import os,json; print(json.dumps(os.getgroups()))"])
assert json.loads(result) == [7, 42, 42]
os.setgroups([])
assert os.getgroups() == []
print("guest groups, sorting, error handling, fork isolation and exec inheritance passed")
