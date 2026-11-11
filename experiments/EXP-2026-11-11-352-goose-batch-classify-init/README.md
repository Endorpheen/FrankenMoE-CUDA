# EXP352: Batch entry classification in Goose

`llama_batch_allocr::init`, content-type section (P6-07).

## Verdict

**GATED, NOT INTEGRATED** — the rejection order, the mixed rule and the token/sequence bounds are
exact; the source tree is read-only.

## Scope transferred

Every entry carries a token id, an embedding, or both, and the batch must agree about the state
embedding. In native order:

1. an entry with neither a token id nor an embedding is refused;
2. the batch must be uniform about the state embedding, with **entry zero** defining the expected
   value;
3. entries carrying both cannot be mixed with the others — all or none;
4. a batch mixing token-only and embedding-only entries is refused when the model does not allow
   mixed batches;
5. `mixed` then decides whether a per-entry embedding flag array exists, and `has_token` /
   `has_embd` follow from which kinds are present.

Included from the same decision: a token id must lie in `[0, n_vocab)` — skipped for an
embedding-marked entry, which stores a placeholder — and a sequence id in `[0, n_seq_max)`.

## Gate

| check | result |
|---|---|
| 6 000 draws, including entries with neither kind, both kinds and non-uniform state | 27 839 checks / 0 failures |

The gate itself had the bug first: it compared the state embedding against an **external** notion
of the batch instead of entry zero, and disagreed with the port on a batch whose first entry was
the odd one. Native takes entry zero as the reference, and the transcription was corrected.

## Mutations

| mutation | failures |
|---|---:|
| an entry with neither token nor embedding is accepted | 2 374 |
| a non-uniform state batch is accepted | 3 370 |
| both-token-and-embedding entries may be mixed | 2 379 |
| a mixed batch is accepted where it is forbidden | 100 |
| a token id out of range is accepted | 724 |
| an embedding-marked entry reads its token id | 80 |
| a sequence id out of range is accepted | 684 |

All seven are caught. The narrowest (80) is the placeholder rule: an embedding-marked entry must
not have its token id read, which only shows when that id happens to be out of range.

## Files

- `source/host.goose` — classification and the two bounds checks.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
