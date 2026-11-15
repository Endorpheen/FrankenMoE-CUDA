#!/usr/bin/env python3
"""Build the synthetic UI bundle the differential gates run against.

The bundle is deliberately not a copy of any shipped UI: every byte is derived
from the file name by a fixed LCG plus a name-dependent length, so the tree is
reproducible on any host and carries no third-party content. What it does carry
is one asset per content-type branch of embed.cpp plus the edge cases the table
is sensitive to (upper case, a dot inside a directory, an empty extension, no
extension, a ".txt" that must fall back to octet-stream).

The eight required-bundle labels of embed.cpp are all satisfied; embed.cpp
itself refuses the tree otherwise, which is the end-to-end check that the label
table in mime-table.goose still describes the same bundle.

    python3 tests/make_assets.py <out_dir>
"""
import os
import sys

# (asset name, content length). Lengths are fixed per name, not random.
ASSETS = [
    # the eight required labels
    ("index.html", 733),
    ("manifest.webmanifest", 211),
    ("sw.js", 1201),
    ("build.json", 89),
    ("version.json", 61),
    ("_app/immutable/bundle.a1b2c3.js", 4099),
    ("_app/immutable/bundle.a1b2c3.css", 1501),
    ("workbox.9f8e7d.js", 877),
    # one asset per content-type branch
    ("docs/page.html", 512),
    ("assets/site.css", 337),
    ("assets/plain.json", 47),
    ("assets/logo.svg", 220),
    ("assets/icon.png", 97),
    ("assets/photo.jpg", 129),
    ("assets/photo2.jpeg", 131),
    ("assets/favicon.ico", 45),
    ("assets/text.woff", 64),
    ("assets/text2.woff2", 68),
    # fallbacks and edge cases
    ("assets/notes.txt", 33),
    ("assets/noext", 19),
    ("assets/UPPER.HTML", 23),
    ("assets/trailing.", 7),
    ("dir.with.dot/file", 11),
    # covered by the no-cache registration list
    ("_app/version.json", 55),
]


def body(name: str, size: int) -> bytes:
    """Deterministic bytes: an LCG seeded from the name, truncated to size."""
    seed = 2166136261
    for ch in name.encode():
        seed = ((seed ^ ch) * 16777619) & 0xFFFFFFFF
    out = bytearray(size)
    x = seed
    for i in range(size):
        x = (1103515245 * x + 12345) & 0x7FFFFFFF
        out[i] = (x >> 16) & 0xFF
    return bytes(out)


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    root = sys.argv[1]
    for name, size in ASSETS:
        path = os.path.join(root, name)
        os.makedirs(os.path.dirname(path), exist_ok=True) if os.path.dirname(path) else None
        with open(path, "wb") as f:
            f.write(body(name, size))
    print(f"assets: {len(ASSETS)} files under {root}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
