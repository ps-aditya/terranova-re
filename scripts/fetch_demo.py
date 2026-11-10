#!/usr/bin/env python3
"""Download the freely distributed Terra Nova demo into data/demo/.

The demo was published by Looking Glass as promotional shareware. This script
only downloads it; the archive is not part of this repository.
"""

from __future__ import annotations

import hashlib
import os
import sys
import urllib.request
import zipfile

URL = "https://archive.org/download/terra_nova-demo1/TN1.zip"
SHA256 = "d1affdffb9faac9487614618c450655b506713dafdb8daf76b8c1934a75ed3e1"
DEST = os.path.join(os.path.dirname(__file__), "..", "data", "demo")


def main() -> int:
    os.makedirs(DEST, exist_ok=True)
    archive = os.path.join(DEST, "TN1.zip")

    if not os.path.exists(archive):
        print(f"downloading {URL}")
        urllib.request.urlretrieve(URL, archive)

    digest = hashlib.sha256(open(archive, "rb").read()).hexdigest()
    print(f"sha256 {digest}")
    if SHA256 and digest != SHA256:
        print("checksum mismatch", file=sys.stderr)
        return 1

    with zipfile.ZipFile(archive) as z:
        z.extractall(DEST)
    print(f"extracted to {os.path.abspath(DEST)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
