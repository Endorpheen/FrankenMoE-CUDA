#!/usr/bin/env python3
"""Where the remaining C++ actually is, measured in function lines with no Goose call anywhere in the body.

tools/decision-coverage.py counts decision predicates; this counts mass. They disagree on purpose: a file can
have high decision coverage and still hold thousands of native lines of plumbing, and a file can have zero
decision coverage while its decisions live elsewhere (llama-model.cpp asks 336 questions through hparams).

A function counts as native when its body contains no fgNNN_/fg_xxx_ bridge call and no goose reference.
Reported per file, largest first. No build needed.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "runtime/reconstructed/src"

SIG = re.compile(r"^[A-Za-z_][\w:<>,\s\*&]*\b([a-z_][a-z0-9_]*)\s*\([^;]*\)\s*(?:const\s*)?\{\s*$")
GOOSE = re.compile(r"fg\d{3}[a-z]_|fg_?(?:[a-z]{2,4}s?)_|\bgoose")


def main():
    want = sys.argv[1] if len(sys.argv) > 1 else None
    rows = []
    for f in sorted(SRC.glob("*.cpp")):
        lines = f.read_text(errors="ignore").split("\n")
        i, native_lines, fns = 0, 0, 0
        detail = []
        while i < len(lines):
            if SIG.match(lines[i]) and not lines[i].startswith((" ", "\t")):
                j, depth = i + 1, 1
                while j < len(lines):
                    depth += lines[j].count("{") - lines[j].count("}")
                    if depth <= 0:
                        break
                    j += 1
                body = "\n".join(lines[i:j + 1])
                if not GOOSE.search(body):
                    native_lines += j + 1 - i
                    fns += 1
                    detail.append((j + 1 - i, i + 1, SIG.match(lines[i]).group(1)))
                i = j + 1
            else:
                i += 1
        if fns:
            rows.append((native_lines, fns, f.name, len(lines), detail))

    rows.sort(reverse=True)
    print(f"{'native lines':>12} {'fns':>5} {'file lines':>10}  file")
    for nl, fn, name, fl, _ in rows:
        print(f"{nl:>12} {fn:>5} {fl:>10}  {name}")
    print(f"\ntotal native function lines: {sum(r[0] for r in rows)}")

    if want:
        for nl, fn, name, fl, detail in rows:
            if name == want:
                print(f"\nlargest native functions in {name}:")
                for size, line, fn_name in sorted(detail, reverse=True)[:15]:
                    print(f"  {size:>5} lines  {fn_name:<40} line {line}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
