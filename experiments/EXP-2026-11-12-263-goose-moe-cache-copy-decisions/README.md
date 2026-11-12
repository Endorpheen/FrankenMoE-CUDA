# EXP263: MoE cache upload-copy decisions in Goose

Continues EXP-262 in P1-10's lifetime layer. The SLRU plan is Goose (EXP-150), admission/grouping is Goose (EXP-262).
Between the plan and the ggml copy primitives there are four decisions left in native code; this experiment moves them.

## The four decisions

From `llama-moe-cache.cpp`:

| decision | native site | rule |
|---|---|---|
| read-path run length | `copy_experts` | a negative slot copies nothing; otherwise extend while `slots[e+n] == slots[e]+n` |
| write-path run length | `copy` | extend while the next fill is `(expert+n, slot+n)` |
| stats bucket | `copy` | small up to `8*n_expert_used` ids, large above |
| bytes per expert tensor | `copy` | sum of `n * expert_size` over the runs |

A run ends at the range end, at a negative slot, or at the first pair that is not consecutive in both fields. The
copies themselves stay ggml primitives.

## Module and gate

`source/copy_decisions.goose` exposes `run_from_slot`, `run_from_fills`, `stats_class` and `fill_bytes`; the emitter
derives all four wrappers from the emission, so no caller depends on function numbering.

`tests/copy_fixture.c` compares each entry against a direct transcription of the native loops, and also asserts the
expected value independently:

    checks: 27, failures: 0

Cases: single element, run of three, stop at a gap, negative slot, run ending exactly at the last element, the same
four shapes for fills, the byte sum over several runs, and the stats boundary at exactly `8*n_expert_used` (small) and
one above (large).

## The gate was shown to fail

Weakening the slot run condition to `slots[e+n] == slots[e]` fails three cases (`run of three`, `stops at gap`,
`run to last`). Restored, 27/0 again.

## Scope

Dormant on the accepted profile (`cache_n = 0`); live in the 3072 MiB cache configuration. Recorded as ported and
gated, not as an Apex route.

## What remains in the lifetime layer

Tensor and bank construction, the view setup, the copy call sequence itself (one call per run, in tensor order), and
teardown order (implicit in member order natively). Each needs an integration point that the cache configuration can
exercise.
