#!/usr/bin/env python3
"""Guest checks for privileged chown mode changes in the metadata backend."""
import os
from pathlib import Path
import tempfile

assert os.geteuid() == 0
with tempfile.TemporaryDirectory(prefix="chown-mode-") as tmp:
    root = Path(tmp)
    for label, before, after, uid, gid in [
        ("setuid", 0o4755, 0o755, 1000, 1000),
        ("setgid-executable", 0o2755, 0o755, -1, 1000),
        ("both", 0o6755, 0o755, 1000, -1),
        ("setgid-non-executable", 0o2644, 0o2644, 1000, 1000),
        ("same-owner", 0o4755, 0o755, 0, 0),
        ("keep-ids", 0o6755, 0o755, -1, -1),
    ]:
        path = root / label
        path.write_text("mode check")
        path.chmod(before)
        os.chown(path, uid, gid)
        actual = path.stat().st_mode & 0o7777
        assert actual == after, (label, oct(actual), oct(after))
    path = root / "fd"
    path.write_text("fd")
    path.chmod(0o6755)
    with path.open("rb") as f:
        os.fchown(f.fileno(), 1000, 1000)
        assert os.fstat(f.fileno()).st_mode & 0o7777 == 0o755
    directory = root / "directory"
    directory.mkdir()
    directory.chmod(0o2775)
    os.chown(directory, 1000, 42)
    assert directory.stat().st_mode & 0o7777 == 0o2775
    path.chmod(0o4755)
    link = root / "symlink"
    link.symlink_to("fd")
    os.chown(link, 1000, 1000, follow_symlinks=False)
    assert path.stat().st_mode & 0o7777 == 0o4755
print("privileged chown mode clearing, directory and symlink exceptions passed")
