# EXP397: hybrid delegation order and rollback are Goose-owned (APEX.RECURRENT_MEMORY)

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | the delegation plan: step 0 is the attention half, step 1 the recurrent half | `llama_memory_hybrid::state_write` / `state_read` |
| 1 | after a recurrent read failure the attention half must be cleared | `state_read` catch branch |

The plan drives the calls: each step asks Goose which half to visit, instead of executing a hard-coded
pair of statements.

## Gate

| check | result |
|---|---|
| 33 checks over steps −5..9 and failure flags −5..9, plus the two-step plan | 0 failures |
| mutations | **4/4 caught** |

## Run

`FG397S_HYBRID_DELEGATION` live; `FG375S` and `FG376S` live in the same run; completion `e68df05b`;
restore exact.

## Why this is now testable at all

The hybrid file was not in the build's changed list, so no hybrid decision code was in the binary. That
defect also produced the earlier false zeros for EXP375/EXP376; it is fixed in the same change that makes
this experiment live.
