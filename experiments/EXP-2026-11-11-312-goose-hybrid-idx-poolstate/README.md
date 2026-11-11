# EXP312: Hybrid k-pool state marking in Goose

`llama_memory_hybrid_idx_context::kpool_build_state` — which pools an ubatch must
re-pool (P5-05).

## Verdict

**GATED, NOT INTEGRATED** — the marking rules are exact; the source tree is
read-only. No performance claim.

## Scope transferred

A pool is marked once per representative row: sequences that share cells share
their pools and read one pooled row, so scatter rows stay unique. Marks are
generation stamps, so the state is reused without clearing arrays each step.

Two passes mark pools: every pool ending after a stale position (a sequence edit
regroups from there on), then every pool the ubatch's own tokens touch. In order
mode a token's cell gives its rank and the rank its pool, because positions repeat
for images; otherwise the pool is found by position and the token must still lie
inside it.

The graph size is `max(n_new, 1, min(bound, pad-1, n_pool_max))` with
`bound = n_tokens/kpool + n_seqs_unq` — padded so the graph width does not move
with the batch.

## Gate

| check | result |
|---|---|
| 3 000 batches over both modes, stale and clean, reusing one generation counter | 23 587 checks / 0 failures |

Marks are compared as membership in the current generation, not as raw values,
because the arrays are only cleared on a generation wrap.

## Mutations

| mutation | failures |
|---|---:|
| clean sentinel becomes a negative position | 10 443 |
| representative-row uniqueness is skipped | 38 |
| stale boundary includes an earlier pool | 147 |
| position lookup skips an equal first cell | 1 594 |
| token outside the pool still marks it | 399 |
| order mode maps rank to the wrong pool | 549 |
| graph bound ignores the unused padding slot | 477 |
| graph bound ignores the sequence count | 1 942 |

All eight are caught.

Four defects of mine, found in this order, all of them in the harness or the
interface rather than the rules:

1. `POS_CLEAN` is the maximum position and I used `-1` as the "clean" sentinel, so
   the stale pass ran on every step — in the port *and* in the transcription, which
   is why it first looked right.
2. `kpool_rank` searches the *cell* list, and I passed the pool count as its length.
3. The pool-offset table and the pool array were one pointer, so the module read
   offsets where it expected pool starts.
4. A vector sized with `assign(n, 0)` and then filled with `push_back` keeps the
   zeros at the front — the cell counts read as zero.

The bound mutations were invisible until the generator produced batches where the
bound actually binds: with small batches `n_tokens/kpool + n_seqs` never exceeds
the other terms, so nothing could distinguish the formula.

## Files

- `source/host.goose` — the marking rules plus a one-entry dispatcher.
- `tests/fixture.cpp` — comparison driver.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
