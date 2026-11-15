#!/bin/bash
# EXP-2026-11-15-642: SHA-256 of every input, patch and artefact.
set -euo pipefail
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
E="$(cd "$S/.." && pwd)"
cd "$E"
{
    sha256sum source/quant-rows2.goose
    sha256sum patches/iq1m-split.diff patches/canon-iq1m-split.diff \
              patches/canon-full-on-1de2effc.diff patches/exp641-qgrid.diff
    sha256sum tests/fixture.cpp tests/make-tables.py tests/mutate.py \
              tests/build.sh tests/run-mutations.sh
    sha256sum results/native-tables.c
    sha256sum results/work/quant-rows2.fixed.goose results/work/quant-weighted.fixed.goose
    sha256sum results/gen/fixed.c results/gen/before.c
    sha256sum results/bin/fixed results/bin/before
    sha256sum results/gate-before.txt results/gate-after.txt \
              results/trace-decisions.txt results/mutations.txt
    sha256sum README.md REPORT.md
} > results/hashes.txt
cat results/hashes.txt
