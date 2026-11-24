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

## What it settles, and what it does not

The counter measures **demands**, that is how many of the expert ids a layer asked for
had already been staged: 42.6-44.8 %.

**Correction.** Demand coverage is not byte coverage and not time coverage. The three
planes of a layer are not equal (`ffn_down_exps` is 210 MiB against 144 MiB for each of
the gate and up planes), and the ids are counted per demand rather than weighted by
plane bytes or by transfer time. Converting 43 % of demands into "0.43 × 2.232 s" was
therefore not a measurement, and the statement that the prefetch "cannot close the
target" is withdrawn as unsupported. The correct next step is to weight the same
counter by plane bytes so that coverage is expressed in transfer time.

Combined with EXP-1843 (a static one-layer reserve costs +19.1 % traffic and −9.2 %
decode) and EXP-1840 (enabling the prediction in prefill moved nothing), the partial
prefetch path cannot close the target: its ceiling is smaller than the requirement and
its price is measured in decode speed.

## Artefacts

- `runs/pfacc-084823` — server log with the accuracy line, requests, responses, props.
