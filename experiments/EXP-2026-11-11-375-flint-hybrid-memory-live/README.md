# EXP375: hybrid memory decisions -- integrated, witness unreachable on this profile

## Verdict

**GATED, INTEGRATED, NOT VALIDATED.** No live witness; the point is not counted as closed.

## Transferred

| op | rule |
|---|---|
| 0 | `seq_pos_min` = max of the two halves' minima |
| 1 | `seq_pos_max` = min of the two halves' maxima |
| 2 | a failed recurrent read clears the attention half |

## Gate

| check | result |
|---|---|
| 76 draws over all argument combinations | 76 checks / 0 failures |

## Why no witness

`mem->seq_pos_max` / `seq_pos_min` are reached from `llama_batch_allocr::init` only inside the M-RoPE
branch (`seq_pos[s]` non-empty). The Apex 32K/128 profile is not M-RoPE, so the combinators are never
called, and the failed-read branch needs a restore that fails in the recurrent half specifically.

EXP378 measured the second possibility directly: truncating the slot file between 360 MB and 387.9 MB
still restores successfully, and truncating to 1 MB fails inside the **attention** half, so the
recurrent section carries no payload to cut on this profile. The branch is unreachable by truncation.

So the port is present and correct under the gate, but this profile cannot exercise it. Closing this
point needs an M-RoPE request or a scenario that fails only the recurrent read; neither exists in the
agreed profile and neither was invented.

## Run

One server, save + successful restore, with the decisions enabled:

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both |
| probabilities | equal |
| `FG375S_HYBRID_MEMORY` | **0 occurrences** |
| `FG373S` / `FG345W` / `FG241U` | live |

The zero witnesses with an unchanged answer is the expected outcome, and it is also why nothing is
claimed for this experiment.


## Correction: the zero was a build defect, not reachability

This experiment was recorded as NOT VALIDATED because `FG375S` never fired, and I explained that with
the M-RoPE branch and `n_ubatch`. Both explanations were wrong for the same reason: `llama-memory-hybrid.cpp`
was **not in the list of files this tree compiles** — it was linked from the flint-line object, so the
decision code was never in the binary at all.

Once the file was added to the build, `FG375S` fires on the ordinary profile. The route is **LIVE**;
the M-RoPE and ubatch notes describe branches that exist but are not what blocked the witness.
