# EXP349: Ubatch assembly in Goose

`llama_batch_allocr::ubatch_add`, the decision half (P6-07).

## Verdict

**GATED, NOT INTEGRATED** — flags, out-id order and sequence uniqueness are exact; the source
tree is read-only.

## Scope transferred

A list of batch indices becomes one ubatch. Three flags decide its shape:

- **mixed**: an embedding-marked row set that is neither empty nor everything — the only case
  where the ubatch carries a per-token type array;
- **use_token**: the batch has tokens, unless every row is embedding-marked;
- **use_embd**: the batch has embeddings, unless no row is embedding-marked.

A state payload is used whenever the batch has one — independent of `mixed`.

Two outputs are easy to get subtly wrong:

- `out_ids` receives the batch index of every row whose logits flag is set, **in row order**; the
  output reorder consumes it later, so the order is part of the contract;
- `seq_id_unq` is filled by scanning sequences **in increasing id**, not in the order they appear
  in the rows, and `seq_idx` maps a sequence to its position in that list.

Payload copies (token, embedding, state, positions) stay native; only their sizes are decided
here.

## Gate

| check | result |
|---|---|
| 5 000 draws over 1–8 rows, 1–5 sequences, all four flag combinations | 122 968 checks / 0 failures |

## Mutations

| mutation | failures |
|---|---:|
| a fully embedding-marked ubatch is not mixed | 661 |
| tokens are used for an all-embedding batch | 325 |
| embeddings are used for a token-only batch | 686 |
| the state payload is sized by the token count only | 2 541 |
| out ids are recorded for every row | 52 737 |
| unique sequences are listed in row order | 6 415 |
| the sequence index is not assigned | 10 472 |

All seven are caught. The two flag mutations at 325 and 686 are the ones a reader would consider
"equivalent enough": a fully embedding-marked batch must not read tokens, and a token-only batch
must not read embeddings.

## Files

- `source/host.goose` — the decisions plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
