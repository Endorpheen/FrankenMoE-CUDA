#!/usr/bin/env python3
"""P10-05 mutation run: 12 mutations per slice, two halves of six.

    python3 tests/mutate.py 1     # pkg 6 + env 6 + art 6
    python3 tests/mutate.py 2     # pkg 6 + env 6 + art 6

Each mutation rewrites one decision in one Goose module, regenerates that module
at -O0, relinks its fixture and reruns the gate. A surviving mutation is a hole
in the gate: every half must be 6/6 for every slice. Builds are serial on
purpose.
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
    "pkg": B / "source" / "release-plan.goose",
    "env": B / "source" / "build-env.goose",
    "art": B / "source" / "generated-artifact-policy.goose",
}
SUM = {"pkg": "FG645_PKG", "env": "FG645_ENV", "art": "FG645_ART"}

# ── half 1 ────────────────────────────────────────────────────────────────
M1 = [
    ("pkg", "tag prefix accepts w",
     "    if s.len < 1 || s[0] != 118 {", "    if s.len < 1 || s[0] != 119 {"),
    ("pkg", "prerelease marker renamed",
     '    if rlp_find(s, 0, "-rc.") >= 0 {', '    if rlp_find(s, 0, "-rc") >= 0 {'),
    ("pkg", "rc suffix off by one",
     "    p = rlp_digits(s, p + 4);", "    p = rlp_digits(s, p + 3);"),
    ("pkg", "asset limit is inclusive",
     "    if size > RLP_ASSET_LIMIT {", "    if size >= RLP_ASSET_LIMIT {"),
    ("pkg", "tree limit is ten MiB short",
     "const RLP_TREE_LIMIT: i64 = 100000000;", "const RLP_TREE_LIMIT: i64 = 10000000;"),
    ("pkg", "keep-old flag ignored",
     "    if has_runtime == 0 || keep_old != 0 || tag_present == 0 || same_version != 0 {",
     "    if has_runtime == 0 || tag_present == 0 || same_version != 0 {"),

    ("env", "empty override is not the default",
     "    if set == 0 || val.len == 0 {\n        return 1;\n    }",
     "    if set == 0 {\n        return 1;\n    }"),
    ("env", "build directory renamed",
     'return ev_put(out, at, "build/franken-cuda");',
     'return ev_put(out, at, "build/franken-cuda-os");'),
    ("env", "parallel default is four",
     '        return ev_put(out, 0, "8");', '        return ev_put(out, 0, "4");'),
    ("env", "c compiler default moved back",
     'at = ev_put(out, at, "gcc-13");', 'at = ev_put(out, 0, "gcc-12");'),
    ("env", "multi-config default leaks",
     "    if (set == 0 || val.len == 0) && multi == 0 {",
     "    if (set == 0 || val.len == 0) && multi == 1 {"),
    ("env", "tests subdirectory bit folded into cli",
     "    if tests != 0 && llama != 0 {\n        m = m + 4;\n    }",
     "    if tests != 0 && llama != 0 {\n        m = m + 2;\n    }"),
]

# ── half 2 ────────────────────────────────────────────────────────────────
M2 = [
    ("pkg", "asset selector drops the prefix test",
     "    if rlp_selected(name) == 0 {\n        return 1;\n    }",
     "    if name.len == 0 {\n        return 1;\n    }"),
    ("pkg", "sums separator is one space",
     "        at = rlp_putc(out, at, RLP_SPACE);\n        at = rlp_putc(out, at, RLP_SPACE);",
     "        at = rlp_putc(out, at, RLP_SPACE);"),
    ("pkg", "manifest key renamed",
     '    at = rlp_put(out, at, ",\\n  \\"object_bytes\\": ");',
     '    at = rlp_put(out, at, ",\\n  \\"object_bytes_x\\": ");'),
    ("pkg", "leading whitespace not required",
     "    if lead == 0 {\n        return 0;\n    }", "    if lead < 0 {\n        return 0;\n    }"),
    ("pkg", "dist header name changed",
     '    at = rlp_put(out, at, "main.h\\n");', '    at = rlp_put(out, at, "main.hpp\\n");'),
    ("pkg", "basename keeps the trailing slash",
     "    while end > 0 && s[end - 1] == RLP_SLASH {\n        end = end - 1;\n    }",
     "    while end > 0 && s[end - 1] == RLP_SLASH && end > 1 {\n        end = end - 1;\n    }"),

    ("env", "option default taken before the description",
     "                if last_q >= 0 {\n                    d0 = last_q + 1;\n                }",
     "                d0 = name_end;"),
    ("env", "pin comparison accepts a length shift",
     "    if expected.len != actual.len {\n        return 0;\n    }",
     "    if expected.len != actual.len {\n        return 1;\n    }"),
    ("env", "applied patch reported as preserved",
     "    if reverse_ok != 0 {\n        return 1;\n    }", "    if reverse_ok != 0 {\n        return 2;\n    }"),
    ("env", "link guard drops the symlink exception",
     "    if exists != 0 && is_symlink == 0 {", "    if exists != 0 {"),
    ("env", "marker gate accepts a missing marker",
     "        if found == 0 {\n            return 0;\n        }", "        if found == 2 {\n            return 0;\n        }"),
    ("env", "upstream table loses the branch column",
     "            if f == 4 {", "            if f == 3 {"),
]

# art mutations are appended to both halves below, six each.
A1 = [
    ("art", "generated banner needs no full stop",
     'gap_contains(head, "Do not edit.")', 'gap_contains(head, "Do not edit")'),
    ("art", "cuh suffix misspelled",
     '    if gap_ends(path, ".cuh") && gap_contains_lower(path, "generated") {',
     '    if gap_ends(path, ".cuhx") && gap_contains_lower(path, "generated") {'),
    ("art", "cuh path test is case sensitive",
     'gap_contains_lower(path, "generated")', 'gap_contains(path, "generated")'),
    ("art", "limit converts with 1024",
     "    return mb_milli * 1048576 / 1000;", "    return mb_milli * 1048576 / 1024;"),
    ("art", "size limit is exclusive",
     "    if all_flag != 0 || size >= limit {", "    if all_flag != 0 || size > limit {"),
    ("art", "gate always warns",
     "    if all_flag != 0 || size >= limit {\n        return 2;\n    }",
     "    if all_flag != 0 || size >= limit {\n        return 1;\n    }"),
]

A2 = [
    ("art", "comment marker is an exclamation mark",
     "    if start >= end || line[start] == GAP_HASH {", "    if start >= end || line[start] == 33 {"),
    ("art", "trailing whitespace kept in a required line",
     "    while end > start && gap_ws(line[end - 1]) {\n        end = end - 1;\n    }",
     "    while end > start && gap_ws(line[end - 1]) && end < 1 {\n        end = end - 1;\n    }"),
    ("art", "missing file reported as missing snippet",
     "    if gap_required_split(line, rel, snip) == 0 {\n        return 0;\n    }\n    if file_present == 0 {\n        return 1;\n    }",
     "    if gap_required_split(line, rel, snip) == 0 {\n        return 0;\n    }\n    if file_present == 0 {\n        return 2;\n    }"),
    ("art", "hash mismatch counted as a missing file",
     "    if hash_match == 0 {\n        m = m + 2;\n    }", "    if hash_match == 0 {\n        m = m + 1;\n    }"),
    ("art", "orphan file accepted",
     "    if in_manifest == 0 {\n        return 1;\n    }", "    if in_manifest == 0 {\n        return 0;\n    }"),
    ("art", "verdict ignores a single problem",
     "    if problems > 0 {\n        return 1;\n    }", "    if problems > 1 {\n        return 1;\n    }"),
]


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, check=False, **kw)


def gate_cmd(slice_name: str, exe: Path) -> list[str]:
    if slice_name == "pkg":
        dist = OUT / "pkgdist-mut"
        dist.mkdir(parents=True, exist_ok=True)
        return [str(exe), str(B / "tests/cases/pkg.tsv"), str(dist), str(B / "tests/oracle_pkg.py")]
    if slice_name == "env":
        return [str(exe), str(B / "tests/cases/env.tsv"), str(B / "tests/native"),
                str(B / "tests/oracle_env.py"), str(OUT)]
    return [str(exe), str(B / "tests/cases/art.tsv"), str(B / "tests/oracle_art.py"), str(OUT)]


def fixture(slice_name: str) -> Path:
    return B / "tests" / f"{slice_name}_fixture.cpp"


def main() -> int:
    half = sys.argv[1] if len(sys.argv) > 1 else ""
    if half == "1":
        table = M1 + A1
    elif half == "2":
        table = M2 + A2
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
        src = (SRC[name]).read_text()
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
        r = run([CXX, "-O2", "-std=c++17", f"-I{d}", str(fixture(name)), str(d / "runtime.o"), "-o", str(exe)])
        if r.returncode != 0:
            print(f"BAD MUTATION {name}: link failed: {why}")
            print(r.stderr[-400:])
            return 2
        q = run(gate_cmd(name, exe), cwd=str(OUT))
        killed = q.returncode != 0
        line = [l for l in (q.stdout + q.stderr).splitlines() if l.startswith(SUM[name])]
        print(f"{'CAUGHT  ' if killed else 'SURVIVED'} {name:4s} {why:44s} {line[-1] if line else ''}")
        if killed:
            caught += 1
    total = len(table)
    print(f"half {half}: {caught}/{total} mutations caught")
    return 0 if caught == total else 1


if __name__ == "__main__":
    raise SystemExit(main())
