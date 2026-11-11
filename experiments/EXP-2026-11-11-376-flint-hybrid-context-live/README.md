# EXP376: hybrid context decisions -- gated, integrated, unreachable on this profile

## Verdict

**GATED, INTEGRATED, NOT VALIDATED.** No live witness; the point is not counted as closed.

## Transferred

| op | rule |
|---|---|
| 0 | `apply()` result is the conjunction of both halves, evaluated without short-circuit |
| 1 | `next()` ends when the advanced cursor reaches the ubatch count |

## Gate

| check | result |
|---|---|
| 35 draws over all argument combinations | 35 checks / 0 failures |

## Diagnosis of the zero

The run does build hybrid memory: `recurrent` appears in the startup log. But neither
`llama_memory_hybrid_context::apply` nor `::next` is called on the Apex 32K/128 request: the batch
reaches the memory through the single-ubatch path, so `next` is never iterated and `apply` is taken from
the half-specific context.

Closing this point needs a request that actually splits into several ubatches (`n_ubatch < n_tokens`).
The agreed profile does not produce one, and inventing a threshold to force it would be a new
acceptance rule, so nothing is claimed.

## Run

One server, save + successful restore, decisions enabled:

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both |
| probabilities | equal |
| `FG376S_HYBRID_CONTEXT` | **0 occurrences** |
| `FG373S` / `FG345W` / `FG241U` | live |


## Correction: the zero was a build defect, not reachability

This experiment was recorded as NOT VALIDATED because `FG376S` never fired, and I explained that with
the M-RoPE branch and `n_ubatch`. Both explanations were wrong for the same reason: `llama-memory-hybrid.cpp`
was **not in the list of files this tree compiles** — it was linked from the flint-line object, so the
decision code was never in the binary at all.

Once the file was added to the build, `FG376S` fires on the ordinary profile. The route is **LIVE**;
the M-RoPE and ubatch notes describe branches that exist but are not what blocked the witness.
