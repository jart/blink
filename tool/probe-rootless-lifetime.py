#!/usr/bin/env python3
"""Guest regression for the opt-in Blink filesystem ownership backend."""
import json
import os
from pathlib import Path
import sys

root = Path(sys.argv[2])
rows = []


def check(name, actual, expected):
    rows.append({"name": name, "actual": actual, "expected": expected})


def owner(path):
    st = path.lstat()
    return [st.st_uid, st.st_gid]


if sys.argv[1] == "create":
    os.setgid(1000)
    os.setuid(1000)
    (root / "file").write_text("original")
    os.link(root / "file", root / "link")
    (root / "file").rename(root / "renamed")
    os.chown(root / "renamed", 123, 456)
    # Opening an existing inode with O_CREAT must preserve explicit ownership.
    fd = os.open(root / "link", os.O_WRONLY | os.O_CREAT, 0o600)
    os.close(fd)
    check("hardlink-after-rename", owner(root / "link"), [123, 456])
    os.chown(root / "link", -1, 789)
    check("partial-chown", owner(root / "renamed"), [123, 789])
    (root / "symbolic").symlink_to("renamed")
    os.chown(root / "symbolic", 321, 654, follow_symlinks=False)
    check("symlink-owner", owner(root / "symbolic"), [321, 654])
    check("symlink-target-unmodified", owner(root / "renamed"), [123, 789])
    fd = os.open(root / "deleted", os.O_CREAT | os.O_RDWR, 0o600)
    os.unlink(root / "deleted")
    os.fchown(fd, 777, 888)
    st = os.fstat(fd)
    check("unlinked-open-inode", [st.st_uid, st.st_gid], [777, 888])
    os.close(fd)
    (root / "shared").mkdir()
    os.chown(root / "shared", 1000, 42)
    (root / "shared").chmod(0o2770)
    (root / "shared/file").write_text("inherited group")
    (root / "shared/directory").mkdir()
    check("sgid-file", owner(root / "shared/file"), [1000, 42])
    check("sgid-directory", owner(root / "shared/directory"), [1000, 42])
    check("sgid-directory-mode", bool((root / "shared/directory").stat().st_mode & 0o2000), True)
    (root / "restricted").write_text("still available to the build backend")
    (root / "restricted").chmod(0)
    check("restricted-mode", (root / "restricted").stat().st_mode & 0o7777, 0)
    fd = os.open(root / "created-restricted", os.O_CREAT | os.O_EXCL | os.O_RDWR, 0)
    os.write(fd, b"created with mode zero")
    check("created-restricted-mode", os.fstat(fd).st_mode & 0o7777, 0)
    os.close(fd)
    (root / "created-restricted-dir").mkdir(mode=0)
    check("created-directory-mode", (root / "created-restricted-dir").stat().st_mode & 0o7777, 0)
elif sys.argv[1] == "check":
    check("persisted-file", owner(root / "renamed"), [123, 789])
    check("persisted-hardlink", owner(root / "link"), [123, 789])
    check("persisted-symlink", owner(root / "symbolic"), [321, 654])
    check("persisted-sgid-group", owner(root / "shared/directory"), [1000, 42])
    check("persisted-sgid-mode", bool((root / "shared/directory").stat().st_mode & 0o2000), True)
    check("persisted-restricted-mode", (root / "restricted").stat().st_mode & 0o7777, 0)
else:
    raise ValueError("expected create or check")
print(json.dumps(rows, indent=2))
raise SystemExit(any(row["actual"] != row["expected"] for row in rows))
