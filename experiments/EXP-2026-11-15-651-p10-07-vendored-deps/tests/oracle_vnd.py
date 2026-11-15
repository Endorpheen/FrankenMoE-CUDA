#!/usr/bin/env python3
"""P10-07 S2 oracle: the vendor table, the patch anchors and the real tree.

Three independent readings of the native material, none of them a transcription
of the port:

  * the vendor table is read out of the byte-identical tests/native/sync_vendor.py
    by parsing its assignments and its dict literal (f-strings are expanded from
    the constants the file itself declares), so a changed pin or a new file
    appears in both arms on the next run;
  * the patch table is taken from the same file with ast.literal_eval, and the
    anchors are counted with Python's str.count() on the byte-identical fixture
    copies, which is the exact function the native script calls;
  * the tree facts (declared, present, reproducible, owned) are recomputed from
    the real vendor directory on every run; the case table carries the values the
    port was given, so staleness on either side is a gate failure.

    python3 oracle_vnd.py <cases.tsv> <sync_vendor.py> <fixtures-dir> <vendor-root>

Output: one `<row>\\t<value>` line per case, in file order.
"""
import ast
import re
import sys
from pathlib import Path

CASES = Path(sys.argv[1]).resolve()
NATIVE = Path(sys.argv[2]).resolve()
FIX = Path(sys.argv[3]).resolve()
ROOT = Path(sys.argv[4]).resolve()

SRC = NATIVE.read_text()

# ── constants and the vendor table ───────────────────────────────────────────
CONST = dict(re.findall(r'(?m)^([A-Z][A-Z0-9_]*)\s*=\s*"([^"]*)"\s*$', SRC))
if not CONST:
    sys.exit("oracle: no pin constants found in sync_vendor.py")

block = re.search(r"(?ms)^vendor\s*=\s*\{(.*?)^\}", SRC)
if not block:
    sys.exit("oracle: vendor table not found in sync_vendor.py")

VENDOR = {}
for raw_line in block.group(1).splitlines():
    line = raw_line.split("#")[0].strip()
    if not line or ":" not in line:
        continue
    m = re.match(r'^([fr]?)"([^"]*)"\s*:\s*"([^"]*)"\s*,\s*$', line)
    if not m:
        continue
    prefix, url, path = m.groups()
    if prefix == "f":
        url = url.format(**CONST)
    VENDOR[url] = path
if not VENDOR:
    sys.exit("oracle: vendor table parsed empty")

# ── the patch table ──────────────────────────────────────────────────────────
node = None
for n in ast.parse(SRC).body:
    if isinstance(n, ast.Assign) and any(getattr(t, "id", "") == "patches" for t in n.targets):
        node = n.value
if node is None:
    sys.exit("oracle: patch table not found in sync_vendor.py")
PATCHES = ast.literal_eval(node)

# ── the pin rule, from the table's own shapes ────────────────────────────────
HEX40 = re.compile(r"(?<=/)[0-9a-fA-F]{40}(?=/|$)")


def pin_kind(url: str) -> int:
    if "releases/latest/download" in url:
        return 4
    if "/refs/heads/" in url:
        return 3
    if "/refs/tags/" in url:
        return 1
    if HEX40.search(url):
        return 2
    return 0


PIN_NAME = {0: "none", 1: "tag", 2: "commit", 3: "branch", 4: "floating"}


def unescape(field: str) -> str:
    out, i = [], 0
    while i < len(field):
        if field.startswith("~512~", i):
            out.append("x" * 512)
            i += 5
        elif field[i] == "\\" and i + 1 < len(field):
            c = field[i + 1]
            if c == "x" and i + 3 < len(field):
                out.append(chr(int(field[i + 2:i + 4], 16)))
                i += 4
                continue
            out.append({"n": "\n", "t": "\t", "r": "\r", "\\": "\\"}.get(c, c))
            i += 2
        else:
            out.append(field[i])
            i += 1
    return "".join(out)


def read_fixture(name: str) -> str:
    return (FIX / name).read_bytes().decode("utf-8", "surrogateescape")


def count(text: str, pat: str) -> int:
    return text.count(pat)


# ── the tree facts ───────────────────────────────────────────────────────────
GENERATED = {
    "vendor/cpp-httplib/httplib.h",
    "vendor/cpp-httplib/httplib.cpp",
}


def tree_facts(rel: str):
    p = ROOT / rel
    declared = 1 if rel in VENDOR.values() else 0
    present = 1 if p.is_file() else 0
    url = next((u for u, v in VENDOR.items() if v == rel), None)
    reproducible = 1 if url is not None and pin_kind(url) in (1, 2) else 0
    owned = 1 if (rel in GENERATED or p.name == "CMakeLists.txt") else 0
    return declared, present, reproducible, owned


def file_state(declared: int, present: int, reproducible: int, owned: int) -> int:
    if owned:
        return 0
    if not declared:
        return 1
    if not present:
        return 2
    if not reproducible:
        return 3
    return 0


def value(kind: str, f: list[str]) -> str:
    if kind == "pin":
        return str(pin_kind(unescape(f[1])))
    if kind == "pin_name":
        return PIN_NAME[pin_kind(unescape(f[1]))]
    if kind == "repro":
        return "1" if int(f[1]) in (1, 2) else "0"
    if kind == "anchor":
        text = read_fixture(f[1])
        return str(count(text, unescape(f[2])))
    if kind == "anchor_state":
        return "1" if int(f[1]) == 1 else "0"
    if kind == "patch":
        text, old, new = (unescape(f[1]), unescape(f[2]), unescape(f[3]))
        if count(text, old) != 1:
            return "-1"
        return text.replace(old, new, 1).encode("utf-8", "surrogateescape").hex()
    if kind == "real_anchor":
        # the real patch table: file, anchor index, anchor text. The count is
        # taken from the real checked-in copy, which is the patched file.
        rel, idx, oldfield = f[1], int(f[2]), f[3]
        old, _new = PATCHES[rel][idx]
        if unescape(oldfield) != old:
            sys.exit(f"oracle: case table anchor drifted for {rel}[{idx}]")
        return str(count(read_fixture(Path(rel).name), old))
    if kind == "tree_state_synth":
        return str(file_state(*(int(x) for x in f[1:5])))
    if kind == "tree_state":
        rel = f[1]
        got = tree_facts(rel)
        given = tuple(int(x) for x in f[2:6])
        if got != given:
            sys.exit(f"oracle: case table is stale for {rel}: table {given} tree {got}")
        return str(file_state(*got))
    raise SystemExit(f"oracle: unknown case kind {kind!r}")


def main() -> int:
    out, row = [], 0
    for line in CASES.read_text().splitlines():
        if not line or line.startswith("#"):
            continue
        row += 1
        f = line.split("\t")
        out.append(f"{row}\t{value(f[0], f)}")
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
