# EXP383: ubatch container composition is Goose-owned (APEX.BATCH remainder)

## Verdict

**LIVE on the reconstructed tree.**

## Transferred

| op | rule | native site |
|---|---|---|
| 0 | the per-token `type` array exists only for a mixed batch | `llama_batch_allocr::ubatch_add` |
| 1 | the decision-order array exists only when an order was given | same |
| 2 | the sequence-id pool is reserved for the token count | same |

The containers themselves stay inside llama — the bridge carries scalars and flat arrays, so a container
cannot cross the boundary by design. What moved is their **composition**: which arrays exist at all and
how large each is, which is exactly the part the remainder named ("container ownership").

## Gate

| check | result |
|---|---|
| 37 draws over mixed 0..1 and token counts 0..5, three plans each | 37 checks / 0 failures |
| mutations | **5/5 caught** |

Mutations: type cap always full, type cap never, order cap inverted, order cap always, seq-id cap off
by one.

## Run

| check | result |
|---|---|
| completion before / after restore | `e68df05b…` both, probabilities equal |
| `FG383S_BATCH_CONTAINER_PLAN` | **live** |
| `FG382S` ×2 / `FG381S` / `FG373S` / `FG345W` / `FG232R` / `FG241U` | live |

## What is not claimed

The dormant splitter branches (`n_keep_tail = 0`, non-unified attention cache on this profile) still
have no witness, and no witness can be produced without changing the profile. They remain recorded as a
profile fact, not as work in progress.
