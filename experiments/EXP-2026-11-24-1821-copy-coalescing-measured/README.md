# EXP-2026-11-24-1821 — Destination-contiguity coalescing of expert copies: measured, ratio 1.00

Status: **MEASURED, lever inapplicable.** Baseline `37ac0f1d` unchanged
(prefill 3122.4, decode 78.225).

## What was asked

Merge queued expert copies whose destinations are contiguous in the SLRU bank,
to cut the transfer count below the measured 80 548 per request (the driver's
per-copy work is 88.3 % of request processor samples, EXP-1818).

A single `cuMemcpyHtoD` needs *both* ends contiguous, so the merge criterion used
here is the only sound one: merge while `dst[i] + bytes[i] == dst[i+1]` **and**
`src[i] + bytes[i] == src[i+1]`. Merging on destination alone cannot be expressed
as one transfer.

## Result

Implemented in `submit_expert_stage_range` with a counter (`GOOSE_COPY_TRACE=1`),
built from the baseline source, and run on the acceptance profile with a
32768-token prompt:

```
Goose copy totals: jobs 198274 -> runs 198274 over 698 calls
```

**198 274 jobs, 198 274 runs: ratio 1.00.** Not one neighbouring pair in the whole
request has both a contiguous destination and a contiguous source, so no two
copies can travel as one. The SLRU assigns slots by its replacement policy, not in
expert order, and the plane layout interleaves sources, so destination contiguity
never coincides with source contiguity.

Timing confirms the change is inert: `prompt_ms` 12140.7 (2699.0 tok/s) in the
diagnostic protocol, against 12208-12312 (2661-2684) for the baseline diagnostics
— the same spread seen without the change, because the code path is unchanged
when the ratio is 1.00.

## Consequence

The transfer count cannot be reduced by coalescing at all in this layout. The only
remaining way to fewer transfers is fewer misses (Belady headroom 2.3 %, EXP-1816)
or a layout in which the SLRU fills slots in expert order, which would change the
eviction policy and its hit pattern — a redesign whose upper bound is the same
2.3 %.

## Artefacts

`runs/coal3-021909` (the counter, the totals line, the timing), `runs/coal-021717`
and `runs/coal2-021812` (the same run before the counter was wired to props).
