# EXP-2026-11-18-986: the live sampler gates join the re-run

Status: 8 gates, **38 168 checks, 0 failures** on the current unit.

## Why

EXP984 re-ran five gates that need only the composed unit. The three live sampler
comparisons - the ones that compare against the sampling the accepted `libllama`
actually performs, and that found the DRY bug in EXP968 - were left out because they
need that library as their oracle. A suite that omits its strongest checks is the
shape this series keeps warning about, so the omission had to be closed or named.

## What was done

`tools/verify-unit-gates.sh` gains a `llama` need: the accepted `libllama.so.0` and
its ggml dependencies, plus the `llama.h` include paths. Three gates join the table:
`typical-live`, `tns-live` and `dry-live`.

Two mechanical facts had to be handled, each found by a failed run:

1. **Link order.** The libraries were placed before the objects, and a left-to-right
   linker discards libraries whose symbols are not yet needed. Include flags and
   library flags are now separate arrays, libraries last.
2. **The initialiser name.** The gates call `fg950_goose_init`, because the shipping
   DSO renames its initialiser to avoid colliding with per-module inits. The
   standalone unit defines `goose_init`, so the suite compiles a two-line shim rather
   than keeping two copies of every gate.

## Result

| gate | checks | result |
| --- | --- | --- |
| typical | 2 535 | PASS |
| top-n-sigma | 17 112 | PASS |
| dry | 2 440 | PASS |
| http-request | 566 | PASS |
| http-listen | 97 | PASS |
| typical-live | 202 | PASS |
| tns-live | 12 515 | PASS |
| dry-live | 2 701 | PASS |
| **total** | **38 168** | **0 failures** |

Unit: 142 modules, 1158 globals, 0 duplicates.

Still skipped, each with its reason: the reader adapter, the vocabulary wire, the BPE
engine and the detokenizer (they need a candidate `libllama` build from their own
experiment directory plus a generated fixture), and the wired-sampler test (it loads
two `libllama` builds with `dlopen` side by side). The script prints that list, so the
report cannot imply coverage it does not have.

## Not claimed

The suite re-runs gates; it does not create new evidence about model behaviour, and no
model was started. The four skipped gates are genuinely not covered here - their
fixtures and candidate builds are outside the repository.

## Reproduction

    bash tools/verify-unit-gates.sh
    sed -n '/^gate/,/^result/p' <log>
