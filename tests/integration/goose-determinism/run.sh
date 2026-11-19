#!/usr/bin/env bash
# Permanent check: the Goose compiler emits byte-identical C for byte-identical input.
#
# P10-06 lists "reproducibility of the generated C" as its own item. A compiler that is
# deterministic in practice is not the same as one that is checked, so this generates the same
# composition twice and compares the two files.
#
# It also composes twice, so a nondeterministic module order in the composer is caught as well: the
# two compositions are compared, then each is generated and the C is compared.
#
# Tier: integration - composes four small modules and generates C twice, no model, no GPU. Under a
# minute; the whole unit is not used on purpose, so this stays cheap enough to run often.
#
# Usage: bash tests/integration/goose-determinism/run.sh
set -uo pipefail
D="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$D/../../.." && pwd)"
G=${FG_GOOSE:-/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose}
OUT=$D/build
LOG=$OUT/run.log
mkdir -p "$OUT/a" "$OUT/b"
MODS=("$ROOT/goose/model/qwen4exp.goose" "$ROOT/goose/model/q4m-metadata.goose"
      "$ROOT/goose/model/q4t-tensor-map.goose" "$ROOT/goose/model/q4e-expert-layout.goose")

if [ ! -x "$G" ]; then
    printf 'skip: no Goose compiler at %s\n' "$G" | tee "$LOG"
    exit 77
fi

fail=0
{
    printf 'compiler: %s\n' "$G"
    printf 'sha256:   %s\n' "$(sha256sum "$G" | cut -d' ' -f1)"
    printf 'source:   %s\n' "$(git -C "$ROOT" rev-parse --short HEAD 2>/dev/null || echo unknown)"

    printf '\n== compose twice\n'
    python3 "$ROOT/tools/compose-goose.py" "$OUT/a/unit.goose" "${MODS[@]}" >/dev/null || exit 1
    python3 "$ROOT/tools/compose-goose.py" "$OUT/b/unit.goose" "${MODS[@]}" >/dev/null || exit 1
    if cmp -s "$OUT/a/unit.goose" "$OUT/b/unit.goose"; then
        printf 'ok: the two compositions are byte-identical\n'
    else
        printf 'FAIL: the composer is not deterministic\n'; fail=1
    fi

    printf '\n== generate C twice from the same input\n'
    # The same composed file, twice: the compiler embeds the source path, so two different
    # compositions would differ for a reason that says nothing about determinism.
    "$G" --standalone -o "$OUT/a/unit.c" --header "$OUT/a/unit.h" "$OUT/a/unit.goose" >/dev/null || exit 1
    "$G" --standalone -o "$OUT/b/unit.c" --header "$OUT/b/unit.h" "$OUT/a/unit.goose" >/dev/null || exit 1
    for pair in unit.c unit.h; do
        if cmp -s "$OUT/a/$pair" "$OUT/b/$pair"; then
            printf 'ok: %s is byte-identical across two runs (%s bytes, sha256 %s)\n' \
                "$pair" "$(stat -c %s "$OUT/a/$pair")" "$(sha256sum "$OUT/a/$pair" | cut -c1-16)"
        else
            printf 'FAIL: %s differs between two runs\n' "$pair"; fail=1
            diff <(head -4000 "$OUT/a/$pair") <(head -4000 "$OUT/b/$pair") | head -6 >&2 || true
        fi
    done
    printf '\n[exit %d]\n' "$fail"
    exit "$fail"
} 2>&1 | tee "$LOG"
