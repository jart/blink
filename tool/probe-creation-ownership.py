#!/usr/bin/env python3
"""Guest-side regression: newly created inodes must use the creating identity."""
import json
import os
from pathlib import Path
import tempfile


def main():
    expected = [os.geteuid(), os.getegid()]
    rows = []
    with tempfile.TemporaryDirectory(prefix="creation-owner-") as directory:
        root = Path(directory)
        (root / "file").write_text("created by guest\n")
        (root / "directory").mkdir()
        (root / "symlink").symlink_to("file")
        assert (root / "symlink").lstat().st_mode & 0o7777 == 0o777
        os.link(root / "file", root / "hardlink")
        (root / "explicit-root").write_text("explicit ownership must survive\n")
        os.chown(root / "explicit-root", 0, 0)
        for name in ("file", "directory", "symlink", "hardlink", "explicit-root"):
            st = (root / name).lstat()
            wanted = [0, 0] if name == "explicit-root" else expected
            rows.append({"name": name, "actual": [st.st_uid, st.st_gid],
                         "expected": wanted})
    print(json.dumps({"identity": expected, "results": rows}, indent=2))
    return int(any(row["actual"] != row["expected"] for row in rows))


if __name__ == "__main__":
    raise SystemExit(main())
