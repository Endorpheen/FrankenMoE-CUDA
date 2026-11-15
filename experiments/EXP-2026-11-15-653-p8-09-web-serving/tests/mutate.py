#!/usr/bin/env python3
"""P8-09 mutation run: 12 mutations, two halves of six, two per slice per half.

    python3 tests/build.sh all        # once, so build/gen and build/ui.o exist
    python3 tests/mutate.py 1         # 2 web + 2 mime + 2 etag
    python3 tests/mutate.py 2         # 2 web + 2 mime + 2 etag

Each mutation rewrites one decision of one Goose module, regenerates that
module, compiles the generated C at -O0, relinks the slice fixture against the
already built oracle assets and reruns its gate. A surviving mutation is a hole
in the gate; every mutation must be caught. Builds are serial on purpose: the
gate builds are manual, one at a time.
"""
from __future__ import annotations

import shutil
import subprocess
import sys
from pathlib import Path

B = Path(__file__).resolve().parent.parent
OUT = B / "build"
WORK = OUT / "mut"
GOOSE = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
REPO = Path("/home/end0/AI/FrankenMoE-CUDA")
UI = REPO / "upstream/llama.cpp/tools/ui"
CC = "gcc-13"
CXX = "g++-13"

SRC = {
    "web": B / "source" / "web-assets.goose",
    "mime": B / "source" / "mime-table.goose",
    "etag": B / "source" / "asset-etag.goose",
}
FIX = {
    "web": B / "tests" / "fixture_web.cpp",
    "mime": B / "tests" / "fixture_mime.cpp",
    "etag": B / "tests" / "fixture_etag.cpp",
}
HDR = {"web": "web.h", "mime": "mime.h", "etag": "etag.h"}
SUM = {"web": "FGWEB_WEB", "mime": "FGWEB_MIME", "etag": "FGWEB_ETAG"}

# ── half 1 ──────────────────────────────────────────────────────────────────
M1 = [
    ("web", "build.json loses its no-cache registration",
     '        || web_name_is(names, off, idx, "build.json") {\n'
     '        return WEB_POLICY_NOCACHE;',
     '        || web_name_is(names, off, idx, "build.json.bak") {\n'
     '        return WEB_POLICY_NOCACHE;'),
    ("web", "every asset receives the cross-origin isolation headers",
     '    if web_name_is(names, off, idx, "index.html") {\n'
     '        out[2] = out[2] | WEB_H_ISOLATION;',
     '    if idx >= 0 {\n'
     '        out[2] = out[2] | WEB_H_ISOLATION;'),

    ("mime", "jpeg is looked up under a misspelled extension",
     '    "jpg",\n    "jpeg",',
     '    "jpg",\n    "jpej",'),
    ("mime", "jpg resolves to the png id",
     "const MIME_EXT_ID: i64[12] = [0, 1, 2, 3, 4, 5, 6, 7, 7, 8, 9, 10];",
     "const MIME_EXT_ID: i64[12] = [0, 1, 2, 3, 4, 5, 6, 6, 7, 8, 9, 10];"),

    ("etag", "the FNV prime loses its last digit",
     "const ET_PRIME: u64 = 0x100000001b3;",
     "const ET_PRIME: u64 = 0x100000001b5;"),
    ("etag", "the hex digits are produced in upper case",
     "    return (97 + (d - 10)) as u8;",
     "    return (65 + (d - 10)) as u8;"),
]

# ── half 2 ──────────────────────────────────────────────────────────────────
M2 = [
    ("web", "the gzip token is matched case-sensitively as upper case",
     "        if accept[i] == 103 && accept[i + 1] == 122 && accept[i + 2] == 105 && accept[i + 3] == 112 {",
     "        if accept[i] == 71 && accept[i + 1] == 122 && accept[i + 2] == 105 && accept[i + 3] == 112 {"),
    ("web", "the Content-Encoding flag is never reported",
     "        out[2] = WEB_H_ENCODING;\n        out[3] = 1;",
     "        out[2] = WEB_H_ENCODING;\n        out[3] = 0;"),

    ("mime", "a bundle script sets the stylesheet label instead",
     '    if mime_starts(base, "bundle") && mime_ends(base, ".js") {\n        mask = mask | 32;',
     '    if mime_starts(base, "bundle") && mime_ends(base, ".js") {\n        mask = mask | 64;'),
    ("mime", "the required-label text for the first label drifts",
     '    if bit == 0 {\n        return mime_put(out, 0, "index.html");',
     '    if bit == 0 {\n        return mime_put(out, 0, "index.htm");'),

    ("etag", "the offset basis loses its last digit",
     "const ET_OFFSET: u64 = 0xcbf29ce484222325;",
     "const ET_OFFSET: u64 = 0xcbf29ce484222324;"),
    ("etag", "the closing quote becomes an apostrophe",
     "    out[p] = 34;\n    p = p + 1;\n    return p;",
     "    out[p] = 39;\n    p = p + 1;\n    return p;"),
]

MUT = {1: M1, 2: M2}


def run(cmd: str, cwd: Path | None = None) -> subprocess.CompletedProcess:
    return subprocess.run(cmd, shell=True, capture_output=True, text=True, timeout=900, cwd=cwd)


def main() -> int:
    half = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    if half not in MUT:
        print("usage: mutate.py 1|2")
        return 2
    if not (OUT / "gen" / "ui.cpp").exists() or not (OUT / "ui.o").exists():
        print("build/gen and build/ui.o are missing: run `bash tests/build.sh all` first")
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    tot = {s: {"caught": 0, "survived": 0, "broken": 0} for s in SRC}
    for i, (sl, name, old, new) in enumerate(MUT[half], 1):
        tag = f"h{half}-{i:02d}"
        d = WORK / tag
        shutil.rmtree(d, ignore_errors=True)
        d.mkdir(parents=True)
        base = SRC[sl].read_text()
        n = base.count(old)
        if n != 1:
            print(f"MUT {tag} SETUP-ERROR occurrences={n} :: {sl} :: {name}")
            tot[sl]["broken"] += 1
            continue
        (d / "p.goose").write_text(base.replace(old, new, 1))
        g = run(f"timeout 300 {GOOSE} --standalone -o p.c --header {HDR[sl]} p.goose", cwd=d)
        if g.returncode != 0:
            print(f"MUT {tag} BUILD-FAIL (goose) :: {name}\n{(g.stderr or g.stdout).strip()[:200]}")
            tot[sl]["broken"] += 1
            continue
        c = run(f"timeout 300 {CC} -O0 -std=c11 -w -c p.c -o p.o", cwd=d)
        if c.returncode != 0:
            print(f"MUT {tag} BUILD-FAIL (gcc) :: {name}")
            tot[sl]["broken"] += 1
            continue
        link = (
            f"timeout 300 {CXX} -O2 -std=c++17 -I{d} -I{OUT} -I{OUT / 'gen'} -I{UI} "
            f"{FIX[sl]} p.o {OUT / 'ui.o'} -o pfix"
        )
        link_res = run(link, cwd=d)
        if link_res.returncode != 0:
            print(f"MUT {tag} BUILD-FAIL (fixture) :: {name}\n{link_res.stderr.strip()[:300]}")
            tot[sl]["broken"] += 1
        else:
            r = run("timeout 300 ./pfix", cwd=d)
            line = "no summary (stopped before it)"
            for ln in r.stdout.splitlines():
                if ln.startswith(SUM[sl]):
                    line = ln
            if r.returncode != 0:
                tot[sl]["caught"] += 1
                print(f"MUT {tag} CAUGHT :: {sl} :: {name} :: {line}")
            else:
                tot[sl]["survived"] += 1
                print(f"MUT {tag} SURVIVED :: {sl} :: {name} :: {line}")
        shutil.rmtree(d, ignore_errors=True)
    shutil.rmtree(WORK, ignore_errors=True)
    bad = 0
    for sl in SRC:
        t = tot[sl]
        print(
            f'HALF {half} slice={sl} total={t["caught"] + t["survived"] + t["broken"]} '
            f'caught={t["caught"]} survived={t["survived"]} broken={t["broken"]}'
        )
        bad += t["survived"] + t["broken"]
    print(
        f'HALF {half} overall total={len(MUT[half])} '
        f'caught={sum(t["caught"] for t in tot.values())} '
        f'survived={sum(t["survived"] for t in tot.values())} '
        f'broken={sum(t["broken"] for t in tot.values())}'
    )
    return 0 if bad == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
