# EXP296: P5-05 closure evidence -- hybrid and indexed memory

Criterion: "Hybrid full/sliding/recurrent memory composition and sequence synchronization match reset/branch/interleave
fixtures."

## Live on the accepted profile

Apex builds hybrid memory, so this item is not dormant. One accepted run prints:

    FG375S_HYBRID_MEMORY first call calls=1
    FG376S_HYBRID_CONTEXT first call calls=1
    FG397S_HYBRID_DELEGATION first call calls=1
    FG382S_RECURRENT_IO_PLAN first call calls=1
    FG396S_RECURRENT_PAYLOAD_SPAN first call calls=1

and the loader reports `RAM-only prefix checkpoints (recurrent): enabled, budget 2048.0 MiB`, so the recurrent half is
composed rather than bypassed.

## Branch, measured rather than asserted

The state round trip from EXP293 runs against this same hybrid model: the server writes a 131 975 288-byte state that
contains both halves, and a fresh process restores it and continues with an identical continuation (`1da0f499`). That is
a real branch into a new context through the hybrid `state_write`/`state_read` path, not a fixture.

## Reset and interleave fixtures

The gates carry the cases live traffic does not reach:

| experiment | coverage | gate |
|---|---|---|
| EXP309 | pool staleness marking, regroup, seq_rm/add/cp/keep/div marking | 19 362 checks, 8/8 mutations |
| EXP310 | pool pad/rank and per-sequence layout update, both grouping modes | -- |
| EXP336 | hybrid-idx init_batch/init_update/clear, split rules, zero staleness after clear | 34 774 checks, 8/8 |
| EXP341 | hybrid-idx delegates: seq_rm refusal order, whole-sequence cp, marking, state suffix, state_read reuse | 44 122 checks, 9/9 |
| EXP342 | kpool layout update: resume vs rebuild, stream bound | 63 506 checks, 9/9 |
| EXP343 | k-pool sizing: full vs batch context | 28 482 checks, 8/8 |

Two of those gates were wrong when first written and are documented as such: EXP341 fed the port the post-operation model
so it compared the port with itself, and EXP342 had a resume length surviving a rebuild on both sides. Both were fixed and
re-run, which is why their mutation counts are quoted after the fix.

## Boundary

The live path proves composition, the recurrent payload span, delegation and a full branch through state. Interleaved
multi-sequence editing of the pool and the full-capacity sizing remain covered by the fixtures above rather than by live
traffic, because a single-slot server run does not interleave sequences.
