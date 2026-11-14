#!/usr/bin/env python3
"""Mutations of the runtime esc() body (P7-08 chat-JSON escaping)."""
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
SRC = os.path.join(ROOT, "source", "runtime.goose")
G = os.environ.get("GOOSE", "/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose")

M = [
    ("\\b becomes \\B", '"\\\\b"', '"\\\\B"'),
    ("\\f becomes \\F", '"\\\\f"', '"\\\\F"'),
    ("\\n becomes \\N", '"\\\\n"', '"\\\\N"'),
    ("\\r becomes \\R", '"\\\\r"', '"\\\\R"'),
    ("\\t becomes \\T", '"\\\\t"', '"\\\\T"'),
    ("\\u00 becomes \\u0", '"\\\\u00"', '"\\\\u0"'),
    ("hex high nibble divides by 8", "d[((c as i64) / 16) % 16]", "d[((c as i64) / 8) % 16]"),
    ("hex low nibble masks 8", "d[(c as i64) % 16]", "d[(c as i64) % 8]"),
    ("control cutoff is 0x1f", "if c < 0x20 {", "if c < 0x1f {"),
    ("quote escaped as apostrophe", "p = appc(out, p, '\"');", "p = appc(out, p, 39);"),
    ("backslash branch tests 93", "if c == 92 {", "if c == 93 {"),
    ("raw byte replaced by '?'", "p = appc(out, p, c);", "p = appc(out, p, 63);"),
]


def esc_span(text):
    start = text.index("fn esc(out: u8[:], at: i64, s: const u8[:]) -> i64 {")
    end = text.index("\n}\n", start) + 3
    return text[:start], text[start:end], text[end:]


def main():
    lo = int(sys.argv[1]) if len(sys.argv) > 1 else 0
    hi = int(sys.argv[2]) if len(sys.argv) > 2 else len(M)
    base = open(SRC, encoding="utf-8").read()
    head, body, tail = esc_span(base)
    caught = 0
    for i in range(lo, hi):
        label, old, new = M[i]
        if body.count(old) != 1:
            print(f"[skip] {i} {label}: {body.count(old)}")
            continue
        open(SRC, "w", encoding="utf-8").write(head + body.replace(old, new, 1) + tail)
        try:
            r = subprocess.run(["bash", os.path.join(HERE, "build.sh")], capture_output=True, text=True, timeout=900)
            ok = r.returncode != 0
            tail_line = (r.stdout.strip().splitlines() or ["no output"])[-1]
            print(f"[{'caught' if ok else 'MISSED'}] {i} {label}: {tail_line}")
            caught += ok
        finally:
            open(SRC, "w", encoding="utf-8").write(base)
    print(f"mutations {caught}/{hi - lo} caught (range {lo}:{hi})")
    return 0 if caught == hi - lo else 1


if __name__ == "__main__":
    sys.exit(main())
