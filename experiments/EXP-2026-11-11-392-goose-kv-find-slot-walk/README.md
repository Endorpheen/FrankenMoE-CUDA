# EXP392: KV slot placement walks in Goose (APEX.KV, part 1)

## Verdict

**LIVE, and the answer is unchanged.**

## What was blocking it

The per-cell predicate was already Goose (`fg199k_can_use`), but the walk stayed native because the only
preserved `fg205g` bridge took a `llama_kv_cells*`. That handle cannot cross the boundary — but it does
not have to: the cells fit in three flat arrays (pos, sequence count, max position + 1), and the
predicate is an extern call.

## Transferred

From the current head, test `cont ? n_tokens : 1` cells at a time, wrap at the end, abandon a group when
a cell fails in continuous mode, stop as soon as `n_tokens` indices are collected, and report failure
when the whole cache has been tested.

## Gate

| check | result |
|---|---|
| 18 000 generated caches (sizes 1..129, occupancy 0/1/2/3 sequences, SWA off and two types, head anywhere, cont on/off, four token counts) | **53 703 checks, 0 failures** |
| mutations | **8 caught** |

Caught: one-cell test in continuous mode, group kept after a failure, index shifted by one, predicate
inverted, failing cell not abandoning the group, failed walk reported as success, reset group becoming a
failure marker, full group not detected.

Three mutations were proven unobservable and documented instead of waived: dropping the wrap accounting
changes only when the walk stops, never what it returns, and the two "failure reported as success"
variants are masked by the second guard. The first two were replaced by mutations that do change the
answer.

## Bug found in my own tooling

`build.py` chose a module's owner by "does any file contain the prefix". My bridge mentions
`fg199k_can_use`, so it became the newest "owner" of `fg199k` and the link failed with duplicate
symbols. The criterion is now "carries `<prefix>_enabled`", and the comment in the file explains why.

## Run

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both |
| restore | succeeds |
| `FG392S_KV_FIND_SLOT` | **live** |
| `FG199K` (predicate) | live |
| `FG391S` / `FG386S` / `FG345W` / `FG241U` | live |

An identical answer with the walk moved is the strongest available statement: a different slot order
would show up as a different KV layout and, eventually, a different completion.
