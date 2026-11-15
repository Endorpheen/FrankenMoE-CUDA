#!/bin/bash
# EXP-2026-11-15-636: SHA-256 of every input, patch and artefact.
set -euo pipefail
S="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
E="$(cd "$S/.." && pwd)"
cd "$E"
{
    sha256sum source/quant-rows2.goose source/quant-weighted.goose
    sha256sum patches/quant-rows2-iq-qgrid.diff patches/quant-weighted-iq-qgrid.diff \
              patches/canon-iq-qgrid.diff
    sha256sum tests/fixture.cpp tests/make-tables.py tests/mutate.py \
              tests/build.sh tests/run-mutations.sh
    sha256sum results/native-tables.c
    sha256sum results/gen/fixed.c results/gen/canon.c
    sha256sum results/bin/fixed results/bin/canon
    sha256sum results/tables.txt results/gate-before.txt results/gate-after.txt \
              results/gate-iq1_m.txt results/mutations.txt
} > results/hashes.txt
cat results/hashes.txt
