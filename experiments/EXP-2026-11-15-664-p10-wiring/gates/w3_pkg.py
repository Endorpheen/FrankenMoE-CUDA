#!/usr/bin/env python3
"""W3 gate: release and packaging decisions.

Arm A (corpus, 73 rows): the whole accepted P10-05 S1 case table — tag acceptance,
prerelease, basename, the two asset classes, both size limits, the retention
chain, the dist file list, SHA256SUMS bytes, manifest.json bytes, the module
count and the compiler line — evaluated by the executable oracle (bash, the
script's own python block) and through the host.

Arm B (end to end, 1 run): tools/package-main-runtime.sh in both modes, comparing
the dist directory name by name and the two documents byte for byte.

Usage: w3_pkg.py [--skip-e2e]
"""
from __future__ import annotations

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))
from host import CORPUS, ROOT, Host, Tally, cases, decode, field, hx, oracle  # noqa: E402

ORACLE = CORPUS / "oracle_pkg.py"
CASES = CORPUS / "cases" / "pkg.tsv"
SCRIPT = ROOT / "tools" / "package-main-runtime.sh"
UNIT = "goose-main"
MODULES, DEFINED, EXTERN = "7", "9", "11"
VERSION = "goose runtime 1e714e6146bc"


def text(value: str) -> str:
    """The protocol spells the empty string '-'; the oracle emits ''."""
    return "" if value == "-" else value


def num(row: list[str], i: int) -> int:
    return int(decode(field(row, i)))


def get(row: list[str], i: int) -> str:
    return decode(field(row, i))


def synthetic_dist(dist: Path) -> None:
    """The same fixtures the accepted gate uses, so the rows are comparable."""
    dist.mkdir(parents=True, exist_ok=True)
    (dist / "main.goose").write_bytes(b"unit\n")
    (dist / "main.c").write_bytes(b"c" * 123)
    (dist / "main.h").write_bytes(b"/* h */\n")
    (dist / "main.o").write_bytes(b"o" * 456)


def sums_pairs(dist: Path) -> tuple[list[str], list[str]]:
    hashes, names = [], []
    for line in (dist / "SHA256SUMS").read_text().splitlines():
        if len(line) < 66:
            continue
        hashes.append(line[:64])
        names.append(line[66:])
    return hashes, names


def arm_corpus(host: Host) -> Tally:
    t = Tally("FG663_PKG_CORPUS")
    rows = cases(CASES)
    with tempfile.TemporaryDirectory() as td:
        dist = Path(td) / "dist"
        synthetic_dist(dist)
        want = oracle(ORACLE, str(CASES), str(dist), MODULES, DEFINED, EXTERN, VERSION)
        hashes, names = sums_pairs(dist)
        sums_req = " ".join(f"{hx(h)} {hx(n)}" for h, n in zip(hashes, names))
        for i, row in enumerate(rows, 1):
            kind = row[0]
            t.rows += 1
            if kind == "version":
                t.eq(i, kind, want[i], host.ask(f"VERSION {hx(get(row, 1))}"))
            elif kind == "prerelease":
                t.eq(i, kind, want[i], host.ask(f"PRERELEASE {hx(get(row, 1))}"))
            elif kind == "basename":
                t.eq(i, kind, want[i], text(host.ask(f"BASENAME {hx(get(row, 1))}")))
            elif kind == "runtime_asset":
                t.eq(i, kind, want[i], host.ask(f"RUNTIME_ASSET {hx(get(row, 1))}"))
            elif kind == "asset_selected":
                t.eq(i, kind, want[i], host.ask(f"SELECTED {hx(get(row, 1))}"))
            elif kind == "asset_limit":
                t.eq(i, kind, want[i], host.ask(f"ASSET_LIMIT {num(row, 1)}"))
            elif kind == "tree_limit":
                t.eq(i, kind, want[i], host.ask(f"TREE_LIMIT {num(row, 1)}"))
            elif kind == "keep":
                tag, new, name = get(row, 3), get(row, 4), get(row, 5)
                same = 1 if tag == new else 0
                present = 0 if tag == "" else 1
                t.eq(i, kind, want[i],
                     host.ask(f"KEEP {num(row, 1)} {num(row, 2)} {present} {same} {hx(name)}"))
            elif kind == "module_count":
                t.eq(i, kind, want[i], host.ask(f"COUNT {hx(get(row, 1))}"))
            elif kind == "version_line":
                t.eq(i, kind, want[i], text(host.ask(f"VERSION_LINE {hx(get(row, 1))}")))
            elif kind == "dist_files":
                t.eq(i, kind, want[i], text(host.ask("DIST")))
            elif kind == "sha256sums":
                t.eq(i, kind, want[i], text(host.ask(f"SUMS {len(names)} {sums_req}")))
            elif kind == "manifest":
                t.eq(i, kind, want[i],
                     text(host.ask(f"MANIFEST {hx(UNIT)} {hx(get(row, 6))} {num(row, 1)} "
                                   f"{num(row, 2)} {num(row, 3)} {num(row, 4)} {num(row, 5)} "
                                   + " ".join(hx(h) for h in hashes))))
            else:
                raise SystemExit(f"gate: unknown case kind {kind}")
    return t


def arm_end_to_end() -> Tally:
    """Both modes of the packaging script must write the same dist bytes.

    Both runs use the same output directory: the composed unit records the path
    it was built at, so two independent builds differ in main.goose by that path
    alone. One directory, two modes, two dist snapshots keeps the comparison
    about the wiring.
    """
    t = Tally("FG663_PKG_E2E")
    with tempfile.TemporaryDirectory() as td:
        out = Path(td) / "out"
        outs = {}
        for mode in ("0", "1"):
            env = dict(os.environ)
            env["BMOE_GOOSE_PKG"] = mode
            p = subprocess.run(["bash", str(SCRIPT), str(out)], env=env, capture_output=True,
                               text=True, check=False)
            if p.returncode != 0:
                raise SystemExit(f"gate: packaging failed in mode {mode}:\n{p.stderr[-800:]}")
            snapshot = Path(td) / f"snapshot-{mode}"
            shutil.copytree(out / "dist", snapshot)
            outs[mode] = snapshot
        files_off = sorted(p.name for p in outs["0"].iterdir())
        files_on = sorted(p.name for p in outs["1"].iterdir())
        t.rows += 1
        t.eq(t.rows, "dist listing", files_off, files_on)
        for name in ("SHA256SUMS", "manifest.json"):
            t.rows += 1
            t.eq(t.rows, name, (outs["0"] / name).read_text(), (outs["1"] / name).read_text())
        for name in ("main.goose", "main.c", "main.h"):
            t.rows += 1
            t.eq(t.rows, name, (outs["0"] / name).read_bytes(), (outs["1"] / name).read_bytes())
    return t


def main() -> int:
    host = Host("pkg")
    try:
        a = arm_corpus(host)
    finally:
        host.close()
    b = Tally("FG663_PKG_E2E")
    if "--skip-e2e" not in sys.argv:
        b = arm_end_to_end()
    return a.done() | b.done()


if __name__ == "__main__":
    raise SystemExit(main())
