#!/usr/bin/env python3
"""P10-07 mutation run: 12 mutations, two halves of six, two per slice.

    python3 tests/mutate.py 1     # lic 2 + vnd 2 + sys 2
    python3 tests/mutate.py 2     # lic 2 + vnd 2 + sys 2

Each mutation rewrites one decision in one Goose module, regenerates that module
at -O0, relinks its fixture and reruns the gate. A surviving mutation is a hole
in the gate: every half must be 6/6. Builds are serial on purpose.
"""
from pathlib import Path
import shutil
import subprocess
import sys

B = Path(__file__).resolve().parent.parent
OUT = B / "build"
WORK = OUT / "mut"
GOOSE = "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose"
CC = "gcc-13"
CXX = "g++-13"

SRC = {
    "lic": B / "source" / "vendor-licence-payload.goose",
    "vnd": B / "source" / "vendor-provenance.goose",
    "sys": B / "source" / "vendor-selection.goose",
}
SUM = {"lic": "FG649_LIC", "vnd": "FG649_VND", "sys": "FG649_SYS"}

# ── half 1 ────────────────────────────────────────────────────────────────
M1 = [
    ("lic", "underline loses the last byte",
     "    var i: i64 = 0;\n    while i < title.len {\n        at = lic_putc(out, at, 61);\n        i = i + 1;\n    }\n    return at;\n}",
     "    var i: i64 = 0;\n    while i + 1 < title.len {\n        at = lic_putc(out, at, 61);\n        i = i + 1;\n    }\n    return at;\n}"),
    ("lic", "crlf pair becomes two line feeds",
     "            at = lic_putc(out, at, LIC_LF);\n            i = i + 2;",
     "            at = lic_putc(out, at, LIC_LF);\n            i = i + 1;"),

    ("vnd", "branch pin read as a tag",
     '    if vnd_contains(url, "/refs/heads/") {\n        return VND_BRANCH;\n    }',
     '    if vnd_contains(url, "/refs/heads/") {\n        return VND_TAG;\n    }'),
    ("vnd", "counting allows overlapping hits",
     "        if vnd_at(text, p, pat) {\n            n = n + 1;\n            p = p + pat.len;",
     "        if vnd_at(text, p, pat) {\n            n = n + 1;\n            p = p + 1;"),

    ("sys", "httplib added without the common layer",
     "    if build_common != 0 {\n        m = m + SYS_HTTPLIB;\n    }",
     "    m = m + SYS_HTTPLIB;"),
    ("sys", "system ggml reported as the in-tree branch",
     "    if use_system != 0 {\n        return 2;\n    }",
     "    if use_system != 0 {\n        return 1;\n    }"),
]

# ── half 2 ────────────────────────────────────────────────────────────────
M2 = [
    ("lic", "entry drops the title separator",
     '    var at: i64 = lic_put(out, 0, "R\\"=L=(");\n    at = lic_put(out, at, "License for ");',
     '    var at: i64 = lic_put(out, 0, "R\\"=L=(");\n    at = lic_put(out, at, "License for");'),
    ("lic", "body forgets the nullptr terminator newline",
     '    at = lic_put(out, at, "nullptr\\n");',
     '    at = lic_put(out, at, "nullptr");'),

    ("vnd", "only commits count as reproducible",
     "    if kind == VND_TAG || kind == VND_COMMIT {",
     "    if kind == VND_COMMIT {"),
    ("vnd", "missing file reported as undeclared",
     "    if declared == 0 {\n        return 1;\n    }\n    if present == 0 {\n        return 2;\n    }",
     "    if present == 0 {\n        return 1;\n    }\n    if declared == 0 {\n        return 1;\n    }"),

    ("sys", "mtmd forgets the image decoder",
     "        return SYS_HASH + SYS_MINIAUDIO + SYS_STB + SYS_SHEREDOM;",
     "        return SYS_HASH + SYS_MINIAUDIO + SYS_SHEREDOM;"),
    ("sys", "mismatch mask ignores the requested set",
     "        if sys_has(a, bit) && !sys_has(b, bit) {\n            r = r + bit;\n        }\n        bit = bit * 2;",
     "        if sys_has(a, bit) || sys_has(b, bit) {\n            r = r + bit;\n        }\n        bit = bit * 2;"),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, check=False, **kw)


def gate_cmd(name: str, exe: Path) -> list[str]:
    if name == "lic":
        return [str(exe), str(B / "tests/cases/lic.tsv"), str(B / "tests/oracle_lic.py"),
                str(B / "tests/native/license.cmake"), str(B / "tests/fixtures"), str(OUT)]
    if name == "vnd":
        return [str(exe), str(B / "tests/cases/vnd.tsv"), str(B / "tests/oracle_vnd.py"),
                str(B / "tests/native/sync_vendor.py"), str(B / "tests/fixtures"),
                "/home/end0/AI/FrankenMoE-CUDA/upstream/llama.cpp", str(OUT)]
    return [str(exe), str(B / "tests/cases/sys.tsv"), str(B / "tests/oracle_sys.py"),
            str(B / "tests/native"), str(OUT)]


def main() -> int:
    half = sys.argv[1] if len(sys.argv) > 1 else ""
    if half == "1":
        table = M1
    elif half == "2":
        table = M2
    else:
        print("usage: mutate.py 1|2", file=sys.stderr)
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    caught = 0
    for i, (name, why, old, new) in enumerate(table):
        d = WORK / f"{name}-{half}-{i}"
        if d.exists():
            shutil.rmtree(d)
        d.mkdir(parents=True)
        src = SRC[name].read_text()
        if src.count(old) != 1:
            print(f"BAD MUTATION {name}: pattern count {src.count(old)}: {why}")
            return 2
        mutated = d / "runtime.goose"
        mutated.write_text(src.replace(old, new))
        r = run([GOOSE, "--standalone", "-o", str(d / f"{name}.c"), "--header", str(d / f"{name}.h"), str(mutated)])
        if r.returncode != 0:
            print(f"BAD MUTATION {name}: does not compile: {why}")
            print(r.stdout[-400:], r.stderr[-400:])
            return 2
        r = run([CC, "-O0", "-std=c11", "-w", "-c", str(d / f"{name}.c"), "-o", str(d / "runtime.o")])
        if r.returncode != 0:
            print(f"BAD MUTATION {name}: C does not compile: {why}")
            print(r.stderr[-400:])
            return 2
        exe = d / "gate"
        r = run([CXX, "-O2", "-std=c++17", f"-I{d}", str(B / "tests" / f"{name}_fixture.cpp"),
                 str(d / "runtime.o"), "-o", str(exe)])
        if r.returncode != 0:
            print(f"BAD MUTATION {name}: link failed: {why}")
            print(r.stderr[-600:])
            return 2
        q = run(gate_cmd(name, exe), cwd=str(OUT))
        killed = q.returncode != 0
        line = [l for l in (q.stdout + q.stderr).splitlines() if l.startswith(SUM[name])]
        print(f"{'CAUGHT  ' if killed else 'SURVIVED'} {name} {why:46s} {line[-1] if line else ''}")
        if killed:
            caught += 1
    total = len(table)
    print(f"half {half}: {caught}/{total} mutations caught")
    return 0 if caught == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
