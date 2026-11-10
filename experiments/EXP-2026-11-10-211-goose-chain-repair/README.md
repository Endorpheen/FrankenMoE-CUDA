# EXP211: restore EXP199 to the active chain — a regression I introduced

## What was wrong

EXP199's 66 `fg199k_*` call sites were **dropped from the active chain at EXP204** and stayed
missing through EXP205, EXP206 and EXP209. Every one of those runs reported "tokens exact" and
passed the acceptance criteria, because the cells class simply fell back to native. The only signal
was the witness line, and I had stopped checking it:

| run | FG199K witness lines |
|---|---:|
| EXP202 | 7 |
| EXP203 | 7 |
| **EXP204** | **0** |
| EXP205 | 0 |
| EXP206 | 0 |
| EXP209 | 0 |

Cause: each stage copies `llama-kv-cache.cpp` from the **previous stage's integration copy**, and
EXP204 started from a copy that predated EXP199 (taken while wiring the cell copy). Every later
stage inherited the loss. This is the same class of mistake as the earlier "EXP175 linked off
EXP173" note: composing from the wrong base silently reverts a module.

Two of my own reports are therefore overstated: EXP204..EXP209 claimed the KV chain was composed
when one component had silently reverted. The numbers in those rows are still real measurements of
those builds, but they measured a chain that was missing EXP199.

## Fix

Applied EXP199's integration diff onto the current stage's copy and verified call-site parity
mechanically rather than by eye:

    exp199 call sites 66   merged 66   missing 0   extra 0

## Result — ACCEPTED

    FG199K_KV_CELLS / _NKV / _SEQCP / _SEQRM / _SHIFT / _SLOT all present again
    2557.42 / 26.30, tokens and all 128 probabilities exact, sha e68df05b

    prefill 2557.42 >= 2500    decode 26.30 >= 25
    vs EXP209: prefill +0.38%, decode -0.33%, both within 2%

## Rule adopted from this

A stage is not composed until its witness list is compared against the previous stage's, not just
its tokens. The acceptance script checks speed and correctness; it does not know which modules were
supposed to run. Adding that check to the routine is the point of this experiment.
