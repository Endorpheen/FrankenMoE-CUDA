# EXP381: KV whole-cache clear and meta-due decision are live

Closes the reachable part of APEX.KV's remainder.

## Verdict

**LIVE on the reconstructed tree.**

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | a whole-context restore (`seq_id == -1`) empties the cache exactly once, before the per-stream loop | `llama_kv_cache::state_read_sinfo` |
| 1 | metadata is written only for a stream that holds cells | `llama_kv_cache::state_write` |
| 2 | a negative sequence takes the whole-cache path in the hybrid drop | `llama_memory_hybrid_idx::state_drop` |

Op 0 and op 1 run on every save/restore; op 2 is integrated but this profile never reaches it (the hybrid
indexed memory is not built by Apex).

## Gate

| check | result |
|---|---|
| 22 draws over sequences -3..3 and counts -2..4 | 22 checks / 0 failures |
| mutations | **7/7 caught** |

Mutations: clear on every restore, never clear, clear on non-negative, meta always, meta when
non-negative, drop only positive, drop never.

## Run

One server, save + successful restore:

| check | result |
|---|---|
| completion before / after | `e68df05b…` both, probabilities equal |
| restore | succeeds |
| `FG381S_KV_WHOLE_CLEAR` | **live** |
| `FG373S` / `FG345W` / `FG241U` | live |

## What this closes, and what it does not

Closed: the `state_clear(seq_id = -1)` whole-cache branch — the item APEX.KV named first.
Recorded but not claimed: the hybrid drop is integrated yet unreachable here (hybrid indexed memory is
not built by Apex), and `state_read_data` remains out of scope at the current interleaved on-disk format
(EXP366).

## Batch note

Containers still belong to llama: the bridge carries scalars and flat arrays only, so a container that
lives inside llama cannot cross the boundary by design.
