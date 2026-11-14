#!/usr/bin/env python3
"""Mutation check for the FG572 parser port.

Twelve mutations of the d_* parser block, split into two halves of six.
Each half is expected to kill 6/6: the differential gate must report at
least one mismatching value (or reject the port JSON) after the mutation.

Every mutation runs in its own scratch directory: runtime.goose is copied
there, mutated, emitted by `goose --standalone` and linked against the
unchanged fixture. One build at a time, sequential, as the experiment rules
require. The canonical source is never modified.
"""
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent      # experiment directory
SRC = ROOT / "source" / "runtime.goose"
BUILD = ROOT / "tests" / "build.sh"
FIXTURE = ROOT / "tests" / "fixture.cpp"
WORK = ROOT / "build" / "mut"

# (label, exact text in the fixed source, mutated text)
HALF1 = [
    ("muse analysis skip is one byte short",
     '            reas = rest[body_at .. end];\n            rest = rest[end + 7 .. rest.len];',
     '            reas = rest[body_at .. end];\n            rest = rest[end + 6 .. rest.len];'),
    ("muse commentary tail skip is one byte short",
     '                clen = app(cbuf, clen, body, 0, n);\n'
     '                cur = if e < 0 { body[0 .. 0] } else { body[e + 7 .. body.len] };',
     '                clen = app(cbuf, clen, body, 0, n);\n'
     '                cur = if e < 0 { body[0 .. 0] } else { body[e + 6 .. body.len] };'),
    ("muse start marker strip swallows the space",
     '        if d_prefix(cur, "<|start|>assistant") {\n            cur = cur[18 .. cur.len];\n        }',
     '        if d_prefix(cur, "<|start|>assistant") {\n            cur = cur[19 .. cur.len];\n        }'),
    ("muse final recipient matches the wrong name",
     '            } else if d_eq_bytes(recipient, "user") {',
     '            } else if d_eq_bytes(recipient, "assistant") {'),
    ("muse ATEM parameter open tag loses its attribute",
     '    let open = "<atem:parameter name=\\"";',
     '    let open = "<atem:parameter id=\\"";'),
    ("muse typed argument is escaped as a string",
     '    } else if ty == D_JSON {\n        let t = d_trim(value);\n        p = app(abuf, p, t, 0, t.len);',
     '    } else if ty == D_JSON {\n        p = esc(abuf, p, value);'),
]

HALF2 = [
    ("k2 xml call loses its opening brace",
     '                    let call_start = q;\n                    let start = d_params_begin(abuf, call_start);',
     '                    let call_start = q;\n                    let start = call_start;'),
    ("k2 xml arguments are sliced past the opening brace",
     '                    aoff[ncall] = call_start;\n                    alen[ncall] = e - call_start;\n'
     '                    q = e;\n                    ncall = ncall + 1;',
     '                    aoff[ncall] = start;\n                    alen[ncall] = e - start;\n'
     '                    q = e;\n                    ncall = ncall + 1;'),
    ("k2 arg key open tag typo",
     '    let open = "<ifm|arg_key>";',
     '    let open = "<ifm|arg_keys>";'),
    ("k2 call name is split on CR",
     '                    let nl = d_find(body, 0, "\\n");',
     '                    let nl = d_find(body, 0, "\\r");'),
    ("message JSON renames the reasoning field",
     '        q = apps(dst, q, ",\\"reasoning_content\\":");',
     '        q = apps(dst, q, ",\\"reasoning\\":");'),
    ("parameter separator fires on the first parameter",
     '    if first == 0 {\n        p = appc(abuf, p, 44);\n    }',
     '    if first != 0 {\n        p = appc(abuf, p, 44);\n    }'),
]


def run_one(index, label, old, new, canary):
    """Apply one mutation in its own directory and read the gate verdict."""
    d = WORK / f"m{index:02d}"
    if d.exists():
        shutil.rmtree(d)
    (d / "source").mkdir(parents=True)
    (d / "tests").mkdir(parents=True)
    shutil.copy2(BUILD, d / "tests" / "build.sh")
    shutil.copy2(FIXTURE, d / "tests" / "fixture.cpp")
    text = canary.replace(old, new, 1)
    if text == canary:
        return None, "pattern not found"
    (d / "source" / "runtime.goose").write_text(text)

    try:
        r = subprocess.run(["bash", str(d / "tests" / "build.sh")], text=True,
                           capture_output=True, timeout=900, errors="replace")
    except subprocess.TimeoutExpired:
        return False, "timeout"
    lines = [x for x in r.stdout.splitlines() if x.startswith("FG572_PARSERS")]
    if not lines:
        tail = (r.stderr.strip().splitlines() or ["no stderr"])[-1]
        return False, "build failed: " + tail[:120]
    line = lines[-1]
    fails = 0
    for token in line.split():
        if token.startswith("failures="):
            fails = int(token.split("=", 1)[1])
    return fails > 0, line


def half(name, items, first_index, canary):
    caught = 0
    print(f"--- {name} ---")
    for i, (label, old, new) in enumerate(items):
        ok, info = run_one(first_index + i, label, old, new, canary)
        if ok is None:
            print(f"[SKIP  ] {label}: {info}")
            continue
        caught += 1 if ok else 0
        print(f"[{'caught' if ok else 'MISSED'}] {label}: {info}")
    print(f"{name}: {caught}/{len(items)} caught\n")
    return caught, len(items)


def main():
    canary = SRC.read_text()
    missing = [lab for lab, old, _ in HALF1 + HALF2 if old not in canary]
    if missing:
        print("source does not contain the expected text (already mutated?):")
        for lab in missing:
            print("  ", lab)
        return 2
    WORK.mkdir(parents=True, exist_ok=True)
    a = half("half 1 (muse/ATEM)", HALF1, 1, canary)
    b = half("half 2 (k2/shared)", HALF2, 7, canary)
    print(f"mutations {a[0] + b[0]}/{a[1] + b[1]} caught; halves {a[0]}/{a[1]} and {b[0]}/{b[1]}")
    return 0 if a[0] == a[1] and b[0] == b[1] else 1


if __name__ == "__main__":
    sys.exit(main())
