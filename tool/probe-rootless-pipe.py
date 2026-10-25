#!/usr/bin/env python3
"""Guest descriptor metadata regression, including duplication and fork."""
import os
import socket

os.setgid(1000)
os.setuid(1000)
r, w = os.pipe()
d = os.dup(r)
assert (os.fstat(r).st_uid, os.fstat(r).st_gid) == (1000, 1000)
os.fchown(d, 123, 456)
for fd in (r, w, d):
    assert (os.fstat(fd).st_uid, os.fstat(fd).st_gid) == (123, 456)
os.fchmod(w, 0o640)
assert os.fstat(d).st_mode & 0o7777 == 0o640
pid = os.fork()
if pid == 0:
    os.fchown(d, -1, 789)
    for fd in (r, w, d):
        os.close(fd)
    os._exit(0)
assert os.waitpid(pid, 0)[1] == 0
for fd in (r, w, d):
    assert (os.fstat(fd).st_uid, os.fstat(fd).st_gid) == (123, 789)
os.write(w, b"x")
assert os.read(d, 1) == b"x"
os.close(r)
assert os.fstat(d).st_uid == 123
os.close(d)
os.close(w)
a, b = socket.socketpair()
os.fchown(a.fileno(), 12, 34)
assert os.fstat(a.fileno()).st_uid == 12
assert os.fstat(b.fileno()).st_uid == 1000
a.close()
b.close()
print("pipe metadata, dup, fork sharing, I/O and independent socket ownership passed")
