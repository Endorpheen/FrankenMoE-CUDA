#!/usr/bin/env python3
"""Measure Goose coverage of DECISIONS, not of call sites.

Counting `fgNNN_` occurrences per file measures where the text is, not where a decision is made:
llama-model.cpp has 4095 lines and zero Goose calls yet asks 336 questions through hparams., which is
Goose-routed (EXP-884). So the unit here is a decision predicate: a function whose name starts with
is_/can_/select_/should_/need_/use_/has_ and whose body answers a question. A predicate counts as Goose
when its body contains at least one fgNNN_* call.

Reported per file and overall. No build needed. Derived from the repository, never typed by hand.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SRC = ROOT / "runtime/reconstructed/src"

SIG = re.compile(
    r"^\s*(?:static\s+|inline\s+)*"
    r"(?:const\s+)?(?:bool|int|int32_t|uint32_t|size_t|float|double|llama_\w+|ggml_\w+|enum\s+\w+)"
    r"\s+(?:[A-Za-z_][A-Za-z0-9_]*::)*([a-z_][a-z0-9_]*)\s*\([^;]*?\)\s*(?:const\s*)?\{\s*$"
)
DECISION = re.compile(r"^(is_|can_|select_|should_|need_|use_|has_)")
GOOSE = re.compile(r"fg\d{3}[a-z]_")


def body_of(lines, start):
    depth, out = 1, []
    for ln in lines[start + 1:start + 200]:
        depth += ln.count("{") - ln.count("}")
        out.append(ln)
        if depth <= 0:
            break
    return "\n".join(out)


# A one-line bitfield test is an accessor, not a decision. Counting those inflates the number the same way
# counting call sites did (EXP-884): is_normal()/is_eog() are `return id & FLAG`, while is_chinese_char() and
# can_reuse() branch on real conditions.
TRIVIAL = re.compile(r"^\s*return\s+[A-Za-z_][A-Za-z0-9_.()\[\]>-]*\s*&\s*[A-Za-z0-9_]+\s*;\s*$")


def is_trivial(body):
    kept = [ln for ln in body.split("\n") if ln.strip() and not ln.strip().startswith("//")]
    if len(kept) > 3:
        return False
    return any(TRIVIAL.match(ln) for ln in kept)


def main():
    per = {}
    total = goose = 0
    native = []
    for f in sorted(SRC.glob("*.cpp")):
        lines = f.read_text(errors="ignore").split("\n")
        for i, ln in enumerate(lines):
            m = SIG.match(ln)
            if not m or not DECISION.match(m.group(1)):
                continue
            name = m.group(1)
            b = body_of(lines, i)
            if is_trivial(b):
                continue
            g = bool(GOOSE.search(b))
            total += 1
            goose += g
            per.setdefault(f.name, [0, 0])
            per[f.name][0] += 1
            per[f.name][1] += g
            if not g:
                native.append(f"{f.name}:{i + 1} {name}")

    print(f"{'file':<28} {'decisions':>9} {'goose':>6} {'native':>7}")
    for name in sorted(per, key=lambda n: -(per[n][0] - per[n][1])):
        t, g = per[name]
        if t:
            print(f"{name:<28} {t:>9} {g:>6} {t - g:>7}")
    c = 100.0 * goose / total if total else 0.0
    print(f"{'TOTAL':<28} {total:>9} {goose:>6} {total - goose:>7}")
    print(f"\ndecision coverage: {goose}/{total} = {c:.1f}%")
    if "--list" in sys.argv:
        print("\nnative decision bodies:")
        for n in native:
            print("  ", n)
    return 0


if __name__ == "__main__":
    sys.exit(main())
