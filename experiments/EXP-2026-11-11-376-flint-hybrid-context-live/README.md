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
