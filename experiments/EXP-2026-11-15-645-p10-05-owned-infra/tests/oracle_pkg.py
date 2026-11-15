#!/usr/bin/env python3
"""Native oracle for P10-05 S1 (release and package planning).

The native text is not re-typed: every rule is extracted from the byte-identical
copies in tests/native/ and executed by the same interpreter the script uses.

    version        make-release.sh:40 regex, evaluated by bash [[ =~ ]]
    prerelease     make-release.sh:98 glob, evaluated by bash case
    basename       GNU basename
    runtime_asset  make-release.sh:118 case pattern, bash case on the basename
    asset_selected the jq startswith("goose-runtime-") of the retention loop
    asset_limit    make-release.sh:82 [ "$size" -gt ... ], bash test
    tree_limit     make-release.sh:86 awk program, GNU awk on a git ls-tree line
    keep           the retention guards around the native loop, in bash
    module_count   package-main-runtime.sh:31 grep -cE pattern, GNU grep
    sha256sums     package-main-runtime.sh:36-49 block, run as the script runs it
    manifest       the same block: json.dumps(manifest, indent=2) + "\\n"

Usage: oracle_pkg.py CASES DIST MODULES DEFINED EXTERN VERSION

Each result goes to stdout as "<row>\\t<value>": decimal for integers, hex of the
raw bytes for text. Rows are numbered from 1 over the non-comment case lines.
"""

from __future__ import annotations

import json
import os
import re
import subprocess
import sys

sys.dont_write_bytecode = True
import tempfile
from pathlib import Path

HERE = Path(__file__).resolve().parent
NATIVE = HERE / "native"


def run(cmd: list[str], stdin: str | None = None) -> str:
    p = subprocess.run(cmd, input=stdin, capture_output=True, text=True, check=False)
    return p.stdout.strip()


def run_native_block(dist: Path, modules: str, defined: str, extern: str, version: str) -> None:
    """Execute the packaging python block exactly as package-main-runtime.sh does."""
    text = (NATIVE / "package-main-runtime.sh").read_text()
    m = re.search(r"<<'PY'\n(.*?)\nPY\n", text, re.S)
    if not m:
        raise SystemExit("native packaging block not found")
    script = m.group(1)
    p = subprocess.run(
        [sys.executable, "-c", script, str(dist), modules, defined, extern, version, str(dist)],
        capture_output=True, text=True, check=False,
    )
    if p.returncode != 0:
        raise SystemExit(f"native packaging block failed: {p.stderr}")


def decode(field: str) -> str:
    return field.encode().decode("unicode_escape") if "\\" in field else field


def main() -> int:
    cases_path = Path(sys.argv[1])
    dist = Path(sys.argv[2])
    modules, defined, extern, version = sys.argv[3:7]

    make_release = (NATIVE / "make-release.sh").read_text().splitlines()
    packaging = (NATIVE / "package-main-runtime.sh").read_text().splitlines()

    version_line = next(l for l in make_release if "=~" in l)
    version_re = re.search(r"=~\s+(\S+)\s+\]\]", version_line).group(1)
    pre_line = next(l for l in make_release if "== *-rc.*" in l)
    pre_glob = re.search(r"==\s+(\S+)\s+\]\]", pre_line).group(1)
    asset_line = next(l for l in make_release if "goose-runtime-*" in l)
    asset_glob = re.search(r"\bin\s+(\S+?)\)", asset_line).group(1)
    limit_line = next(l for l in make_release if "2147483648" in l)
    asset_limit = re.search(r"-gt\s+(\d+)", limit_line).group(1)
    tree_line = next(l for l in make_release if "100000000" in l)
    tree_awk = re.search(r"awk\s+'(.*?)'", tree_line).group(1)
    count_line = next(l for l in packaging if "grep -cE" in l)
    count_pat = re.search(r"-cE\s+'(.*?)'", count_line).group(1)

    run_native_block(dist, modules, defined, extern, version)

    rows: list[str] = []
    for raw in cases_path.read_text().splitlines():
        if not raw or raw.startswith("#"):
            continue
        rows.append(raw)

    out: list[str] = []
    made_dist = False
    for idx, raw in enumerate(rows, 1):
        f = raw.split("\t")
        kind = f[0]
        if kind == "version":
            v = decode(f[1])
            r = run(["bash", "-c", 'if [[ "$1" =~ $2 ]]; then echo 1; else echo 0; fi', "_", v, version_re])
            out.append(f"{idx}\t{r}")
        elif kind == "prerelease":
            v = decode(f[1])
            r = run(["bash", "-c", 'case "$1" in $2) echo 1;; *) echo 0;; esac', "_", v, pre_glob])
            out.append(f"{idx}\t{r}")
        elif kind == "basename":
            r = run(["basename", "--", decode(f[1])])
            out.append(f"{idx}\t{r.encode().hex()}")
        elif kind == "runtime_asset":
            name = decode(f[1])
            r = run(["bash", "-c", 'case "$(basename -- "$1")" in $2) echo 1;; *) echo 0;; esac',
                     "_", name, asset_glob])
            out.append(f"{idx}\t{r}")
        elif kind == "asset_selected":
            name = decode(f[1])
            r = run(["bash", "-c", 'case "$1" in goose-runtime-*) echo 1;; *) echo 0;; esac', "_", name])
            out.append(f"{idx}\t{r}")
        elif kind == "asset_limit":
            r = run(["bash", "-c", '[ "$1" -gt "$2" ] && echo 1 || echo 0', "_", f[1], asset_limit])
            out.append(f"{idx}\t{r}")
        elif kind == "tree_limit":
            listing = f"100644 blob 0123456789abcdef {f[1]}\tmodel.gguf\n"
            r = run(["bash", "-c", f'printf "%s" "$1" | awk \'{tree_awk}\' | wc -l', "_", listing])
            out.append(f"{idx}\t{1 if int(r.strip()) > 0 else 0}")
        elif kind == "keep":
            has_runtime, keep_old, tag, new_version, name = decode(f[1]), f[2], decode(f[3]), decode(f[4]), decode(f[5])
            script = (
                'if [ "$1" -eq 1 ] && [ "$2" -eq 0 ]; then\n'
                '    old="$3"; new="$4"; name="$5"\n'
                '    if [ -n "$old" ] && [ "$old" != "$new" ]; then\n'
                '        case "$name" in goose-runtime-*) echo 0;; *) echo 1;; esac\n'
                '    else echo 1; fi\n'
                'else echo 1; fi\n'
            )
            out.append(f"{idx}\t{run(['bash', '-c', script, '_', has_runtime, keep_old, tag, new_version, name])}")
        elif kind == "module_count":
            text = decode(f[1])
            r = run(["bash", "-c", 'printf "%s\\n" "$1" | grep -cE "$2" || true', "_", text, count_pat])
            out.append(f"{idx}\t{r.splitlines()[0] if r else '0'}")
        elif kind == "version_line":
            text = decode(f[1])
            # No strip: tail keeps the line's own trailing spaces.
            p = subprocess.run(["bash", "-c", 'printf "%s" "$1" | tail -1', "_", text],
                               capture_output=True, text=True, check=False)
            out.append(f"{idx}\t{p.stdout.rstrip(chr(10)).encode().hex()}")
        elif kind == "dist_files":
            names = ["main.goose", "main.c", "main.h", "main.o", "SHA256SUMS", "manifest.json"]
            out.append(f"{idx}\t{(''.join(n + chr(10) for n in names)).encode().hex()}")
        elif kind == "sha256sums":
            sums = (dist / "SHA256SUMS").read_bytes()
            out.append(f"{idx}\t{sums.hex()}")
        elif kind == "manifest":
            out.append(f"{idx}\t{(dist / 'manifest.json').read_bytes().hex()}")
        else:
            raise SystemExit(f"unknown case kind: {kind}")
    print("\n".join(out))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
