# EXP199: KV cell ownership in Goose — gate GREEN

Real implementation, not a survey. Eight cell-ownership operations from `llama-kv-cells.h` are
Goose; the host holds the arena, Goose holds every decision.

## Result

    FG199K_FIXTURE checks=66729 failures=0
    EXP199_LOCAL_GATES_PASS

The gate mirrors 40 trials x 60 random operations into the Goose module and into a transcription
of `llama_kv_cells`, comparing after every step: positions, sequence words, used flags, shift,
the used range, `is_empty`, and the return values of `seq_rm`/`seq_keep`. Three fork-isolated trap
cases cover `rm` on an empty cell, `rm` out of range and `seq_rm` on an absent sequence.

## Operations ported

| Goose | native | behaviour |
|---|---|---|
| `is_empty` | `is_empty` | `pos == -1` |
| `rm` | `rm` | clear position, sequence words, used flag, shift |
| `seq_rm` | `seq_rm` | drop one sequence bit, return 1 when the cell empties, zero the words |
| `seq_keep` | `seq_keep` | keep only one bit, return 1 when it empties |
| `seq_add` | `seq_add` | set one bit only (does not mark used) |
| `pos_set` | `pos_set` | write position, mark used unless -1 |
| `seq_count` | `seq_count` | popcount over the words |
| `used_max_p1` | `used_max_p1` | one past the highest used index |

Arena: `pos[n] | seq[n*W] | used[n] | shift[n]`, int64 slots, `W = 8` words (512 sequence bits).

## Defects the gate found, in order

Four in the port:

1. **Arena offsets were constants, not derived from `n`** — every region after `pos` addressed
   the wrong memory.
2. **`seq_rm` left stale sequence words** when a cell emptied, so a later `seq_add` saw a bit
   already set. Native resets them.
3. **`seq_add` marked the cell used**; native does not.
4. **`const W` read as zero.** The generated code takes consts from `gg_globals`, which the
   emitter initialises inside `gg_program` — and the bridge never runs that. Replaced with a
   function returning a literal. This one was only visible by dumping arena slots directly; the
   generated C looked correct.

Three in the gate itself: `seq_add`'s precondition had to mirror native's assert, the gate's
arena was sized for a different layout, and empty cells had to start at `-1` rather than `0`.

## Point worth carrying to the next module

The `const` defect is a trap for every future Goose module that reads a compile-time constant at
runtime: the value is fine inside `main` and zero everywhere else. Any module whose bridge calls a
generated function directly must inline such constants as literals or pass them in.

## Next

`apply_ubatch` and `prepare` on top of these eight operations — the probe showed 276 and 138
calls per pass. The cell primitives they need are now Goose and gated.
