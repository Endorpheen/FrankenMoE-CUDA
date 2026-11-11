# EXP398: ubatch container layout is Goose-owned (APEX.BATCH)

## Verdict

**LIVE, real-request match.**

## Transferred

| op | layout decision |
|---|---|
| 0 | destination index inside a position row: `j * n_tokens + i` |
| 1 | source index inside the batch rows: `j * n_batch + idx` |
| 2 | pool growth: current size plus contributions |

The fill order of every flat container is therefore decided by Goose, on top of the composition rules
already moved in EXP383.

## Gate

| check | result |
|---|---|
| 251 checks over rows 0..3, widths 0..4, elements 0..4 and pool growth | 0 failures |
| mutations | **5/5 caught** |

Mutations: axes swapped, source drops the element, pool resets, destination is 1-based, source loses the
row.

## Run

| check | result |
|---|---|
| completion | `e68df05b…`, equal to the reference |
| restore | exact |
| `FG398S_BATCH_CONTAINER_LAYOUT` | **live, 527 report lines** |
| `FG397S` / `FG396S` / `FG383S` / `FG241U` | live |

## Boundary, stated rather than hidden

The storage itself (`std::vector` inside `llama_ubatch`) stays with llama, the same way the io primitive
stays with llama on the KV payload path. What Goose owns is the composition (EXP383), the fill order and
indices (here), and the mapping paths that drive them (FG237S/FG239B/FG240Q/FG242O/FG349S/FG350S/FG352S).
Moving the storage would mean replacing `llama_ubatch`'s containers with Goose-owned arrays, which changes
every reader for no behavioural difference; that is proposed as a decision, not slipped in.
