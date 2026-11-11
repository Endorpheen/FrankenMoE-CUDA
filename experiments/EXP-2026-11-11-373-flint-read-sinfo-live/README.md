# EXP373: KV read-orchestration guards live on the reconstructed tree

The three guards that run before and inside `state_read_sinfo`, wired into the tree that reproduces the
accepted runtime.

## Verdict

**LIVE, EXACT on the reconstructed tree.**

## Guards transferred

| op | rule |
|---|---|
| 0 | a mirrored layout must have exactly one entry per stream |
| 1 | the section's stream count must match the cache |
| 2 | a zero-cell stream requires an empty mirrored entry |

The reads, the clear and the throw stay in llama; only the decisions moved.

## Gate

| check | result |
|---|---|
| 41 draws over all op/argument combinations, codes compared | 41 checks / 0 failures |

## Run on the reconstructed tree (one server)

| check | result |
|---|---|
| completion before save | `e68df05b…` |
| save | 387 989 920-byte file |
| restore (successful) | `n_restored` |
| completion after restore | `e68df05b…`, all 128 probabilities equal |
| `FG373S_KV_READ_SINFO` | **live, 1 call** (the successful restore) |
| `FG345W_KV_STATE_WRITE` | live, 1 call (the save) |
| `FG241U` (accepted route) | live |

So the orchestration guard now runs on the same tree as the accepted routes, on the success path —
complementary to EXP367, which witnessed the clear on the failure path.

## Not transferred

The per-stream body of `state_read_sinfo` (meta/data call order and the clear-before-throw) still lives
in llama: the section interleaves header and payload per stream, so a single decision call cannot
replace it without changing the read order — the same constraint recorded in EXP366.
