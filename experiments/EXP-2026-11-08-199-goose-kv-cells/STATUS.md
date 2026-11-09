# EXP199 status: KV cell ownership ported to Goose, gate not yet green

Real implementation, not a survey. The module exists and compiles; the differential gate is
failing on one condition and the cause is not yet isolated.

## What is implemented

`source/cells.goose` -- eight cell-ownership operations from `llama-kv-cells.h`, with the arena
held by the host and the policy in Goose:

| Goose function | native | what it does |
|---|---|---|
| `is_empty` | `is_empty` | `pos == -1` |
| `rm` | `rm` | clear position, sequence words, used flag, shift |
| `seq_rm` | `seq_rm` | drop one sequence bit, return 1 when the cell empties |
| `seq_keep` | `seq_keep` | keep only one sequence bit, return 1 when it empties |
| `seq_add` | `seq_add` | set one sequence bit (does not touch the used set) |
| `pos_set` | `pos_set` | write the position, mark used |
| `seq_count` | `seq_count` | popcount over the sequence words |
| `used_max_p1` | `used_max_p1` | one past the highest used index, 0 if none |

Layout is documented in the source: `pos[n] | seq[n*W] | used[n] | shift[n]`, int64 slots,
`W = 8` words per cell for 512 sequence bits. The C++ containers stay as storage; what moved is
every decision that was inside the class, which is the requirement.

## What the gate caught so far

`tests/fixture.cpp` mirrors a random operation sequence into both the Goose module and a
transcription of `llama_kv_cells`, comparing pos, sequence words, used flags, shift, the used
range and the return values of the two remove operations after every step.

Three real defects in the port were found and fixed:

1. Arena offsets were constants instead of being derived from `n` -- every region after `pos`
   addressed the wrong memory. The gate trapped immediately.
2. `seq_rm` left stale sequence words behind when the cell emptied, so a later `seq_add` saw a
   bit already set. Native resets the bitset in that path.
3. `seq_add` set the used flag; native does not (the position index is derived from `pos[]`).

Two more were defects in the gate, not the module: `seq_add`'s precondition must mirror native's
assert, and the gate's own arena buffer was sized for a different layout.

## Where it stands

The gate still fails with validation code 42 (`seq_add` precondition: the bit is already set).
The operation sequence that reaches it has not been reduced to a minimal case yet.

## Next step, precisely

Add a trace of the last ten operations before the trap so the failing sequence can be read
directly, then decide whether the module or the gate's mirror is wrong. The gate is the more
likely party: the module's `seq_rm` and `seq_keep` now match native line by line, while the
gate's random generator can place a sequence on a cell it had already given that sequence
through a path the mirror does not reproduce.
