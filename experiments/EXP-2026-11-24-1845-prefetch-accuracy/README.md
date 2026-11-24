# EXP-2026-11-24-1845 — Prefetch accuracy: 43 % of demanded planes can be staged ahead

Status: **MEASUREMENT.** Baseline `37ac0f1d` unchanged (prefill 3122.4, decode 78.225).

## The counter

A new counter records, for every layer the predicted-route prefetch stages, which
expert ids it copied, and then compares them with the ids the layer actually demands
when it runs (`pf_score`, installed as a hook from `prepare_expert_offsets`, printed
as `Goose CUDA: prefetch accuracy ...`).

## The measurement

Acceptance request, warm-up plus one measured, prefetch enabled for prefill
(`GOOSE_EXPERT_PREFETCH_PREFILL=1`), `runs/pfacc-084823`:

```
Goose CUDA: prefetch accuracy 15000 layers: 153399 predicted hits, 206601 misses (42.6% of demands were staged)
```

| quantity | measured |
| --- | ---: |
| predicted planes that survived to the demand | 42.6-44.8 % |
| demands that still had to be fetched on demand | ~57 % |
| prefill / decode | 3128.0 / 78.226 |
| text | `f8664331bd30` |
| fills / evictions / uploaded | 95 076 / 91 641 / 181.4 GB (same as the baseline block) |

## What it settles

Partial prefetching — copying only the experts that are missing, ahead of the layer —
can cover at most **43 % of the demand**. The copies that could be hidden are
therefore at most 0.43 × 2.232 s ≈ **0.96 s**, which is below the 1.13 s the 3500
target needs, before counting the cost of the reserve that must hold them.

Combined with EXP-1843 (a static one-layer reserve costs +19.1 % traffic and −9.2 %
decode) and EXP-1840 (enabling the prediction in prefill moved nothing), the partial
prefetch path cannot close the target: its ceiling is smaller than the requirement and
its price is measured in decode speed.

## Artefacts

- `runs/pfacc-084823` — server log with the accuracy line, requests, responses, props.
