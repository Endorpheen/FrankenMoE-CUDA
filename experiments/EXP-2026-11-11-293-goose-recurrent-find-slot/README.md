# EXP293: Recurrent-state slot placement in Goose

`llama_memory_recurrent::find_slot` — the recurrent half of the memory subsystem
(P5-04). Unlike the KV cache, a recurrent cache gives a whole sequence one cell,
so a slot is always contiguous and the function has to move cells around to make
the used range start at the lowest tail.

## Verdict

**GATED, NOT INTEGRATED** — exact against a transcription of the class; the
source tree is read-only, so nothing is wired in. No performance claim.

## Scope transferred

Every phase of the native body:

1. the head reset when `head > used + 2*n_seqs`,
2. the shared-sequence sweep: a sequence that appears as a second id loses its
   tail, and a cell that becomes empty is released and stops counting as used,
3. the ring search for the next empty cell, from the head,
4. slot assignment per sequence: take the existing cell when the sequence is its
   sole owner, otherwise copy the old tail's position and source into the next
   empty cell and move the sequence there, then advance to the next empty cell,
5. the re-order pass that swaps two cells' position, source and sequence set and
   retargets every tail that pointed at either of them, so the range becomes
   contiguous from `min`,
6. the final position and sequence-set write, with each sequence's tail pointing
   at its new cell,
7. the source-reference staging: the first cell in the range with no incoming
   reference becomes the zero-state source, `src` is replaced by the cell's own
   index, and `src0` keeps the staged value,
8. `head = min`, `n = max - min + 1`, `used` recounted, and the `n >= n_seqs`
   sanity result.

A cell is empty exactly when its sequence set is empty (`mem_cell::is_empty`),
not when its position is −1 — that is the one place this differs from the KV
cache, and the port follows the recurrent definition.

The source-reference staging is written without a scratch vector: the reference
count is the scan itself. Using `src0` as scratch first was wrong and the gate
said so — it clobbered the cells outside the range, which native leaves at −1.

## Gate

The oracle is a transcription over a model with the native `std::set` of sequence
ids. States are drawn so that each sequence owns at most one cell and cells may
carry several sequences — the invariant the native asserts rely on — and the
first `seq_id` of each batch group is distinct, which is the other native
precondition.

| check | result |
|---|---|
| 800 random states and batches, whole cell array compared | 131 200 checks / 0 failures |
| re-order pass actually exercised | 1 233 swaps |

Compared per trial: every cell's position, `src`, `src0`, `tail` and sequence
set, plus `head`, `used`, `n` and the returned bool.

## Mutations

| mutation | detection |
|---|---:|
| head reset tolerance is wider | 2 866 |
| shared sequences keep their tail | 18 668 |
| next-empty scan skips every second cell | 1 204 |
| ownership test accepts a shared cell | 2 825 |
| swap does not move the sequence words to the source cell | 1 165 |
| final pass keeps the previous position | 2 050 |
| zero-state scan starts above the range | abort (no unreferenced cell is found) |
| range check rejects an exact fit | 296 |

All eight are caught.

Three candidate mutations were rejected because the final pass rewrites what
they break: swapping the position and the `src` of the two cells is invisible
once the destination gets `last_pos` and every cell in the range gets `src = i`.
Only the sequence words that land in the *source* cell survive the pass, and the
mutation that drops them is the one that fails 1 165 values. That is the same
class of mistake as the wrap accounting in EXP290.

## Files

- `source/host.goose` — the port plus a one-entry dispatcher.
- `tests/fixture.cpp` — native transcription, random driver, comparison.
- `emit_sources.py`, `test_local.py`, `mutate.py` — build, gate, mutation driver.
