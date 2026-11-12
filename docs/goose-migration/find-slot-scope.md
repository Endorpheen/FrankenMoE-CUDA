# Why `llama_memory_recurrent::find_slot` is not ported

This is the largest remaining live algorithm in the KV/recurrent area — 131 lines, called per prepared
ubatch — and I am leaving it native deliberately. The reason is structural, not effort.

## What it does that the boundary cannot carry

`find_slot` mutates cell metadata in place, in several passes over the same array:

1. clamps `head` against `used + 2*n_seqs`;
2. walks each sequence's `seq_id` **set**, erasing a seq_id from one cell and re-inserting it in another,
   asserting `!orig_cell.is_empty()` after the erase;
3. then a **second** pass over `n_seqs` reads `cells[ubatch.seq_id[i][0]].tail`, which pass 2 just wrote;
4. then swaps whole cells (pos, src, `seq_id` set) and repairs every `tail` pointer in the cache that
   referenced either cell;
5. then writes `pos` and rebuilds `seq_id` per sequence;
6. then computes refcounts of `src` and picks `src0`;
7. finally recomputes `head`, `n` and `used` by counting non-empty cells.

Steps 2-4 are the problem. The Goose module boundary carries flat scalar arrays; `seq_id` is a
`std::set` per cell with erase/insert semantics and the loop's correctness depends on those mutations
being visible to later passes. Flattening it would mean re-implementing the set operations across the
boundary on every call — the boundary would cost more than the function.

## Why not port it as a pure decision layer

The alternative I used for `apply_ubatch` (EXP228) was to compute a decision in Goose and apply it in C++.
That works when the decision is per-cell and independent. Here the passes are sequentially dependent:
pass 3's input is pass 2's output. Splitting them means four or five boundary crossings per ubatch with
freshly copied state each time, and every copy is a place to get a stale value.

## What that costs the migration

`find_slot` stays native. It is 131 lines in `llama-memory-recurrent.cpp`, on the recurrent path only,
and the Apex profile's GDN layers go through it. So the recurrent memory subsystem is **not** fully on
Goose, and `P5-02` should not be marked complete on the strength of the ten routed helpers.

## What would change the answer

If the cell metadata representation ever moves to flat arrays (the `[TAG_KV_CACHE_SHARE_CELLS]` /
`TAG_RECURRENT_ROLLBACK_SPLITS` refactors hinted at in the source comments), the passes become array
walks and porting becomes straightforward. That is a refactor of the native side, which is outside this
migration's remit.

Recorded so the next session does not spend an hour rediscovering it, and so the ledger's claim stays
honest: ten routes live, `find_slot` and the state read/write I/O paths native.
