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

## The purge branch is not reachable on this profile

Tried to make it run with a warm-up request before the measured one:

    APEX_WARMUP_DONE
    FG199K_KV_CELLS goose=65814 cleared=0 native=0 purge=0 head=292

`purge` stays zero, and the reason is the profile itself: the fixed Apex request sets
`cache_prompt=false`, so the cache is reset before the measured request and no cell being placed
is ever occupied. `cleared=0` and `purge=0` are the same fact seen twice.

That is not a defect and not a reason to change the profile to make a branch light up. What it
means is narrower and worth stating plainly:

- the **decision** `should_purge` is ported and covered by the differential gate, where occupied
  cells are generated directly;
- the **branch in the live runtime** is not exercised by this profile, so the in-situ evidence
  for it is absent rather than positive.

Per the standing rule, a path the profile does not take keeps its native fallback and gets
recorded rather than manufactured. `GGML_GOOSE_KV_CELLS=0` restores the whole native sequence, and
the two decisions that do run are proven by the `native=0` witness.

## find_slot decisions ported and live

Three more from `find_slot` / `llama-hparams.h`:

- `head_start(head_cur, used, n_tokens)` -- line 1026: restart the search at 0 when enough cells
  are free before the head. This decides which cells get reused.
- `is_masked_swa(n_swa, swa_type, p0, p1)` -- all four SWA types (NONE / STANDARD / CHUNKED /
  SYMMETRIC). One correction during the port: native divides by `n_swa` without a guard in the
  CHUNKED case, and my first version trapped on zero. The module now mirrors the arithmetic and
  returns 0 for `n_swa == 0` instead of asserting a condition native does not have.
- `can_use_cell(occupied, seq_count, pos_cell, seq_pos_max_1, n_swa, swa_type)` -- lines 1060-1082.

Apex after routing all three:

    FG199K_KV_CELLS goose=65814 cleared=0 native=0 purge=0 head=292
    FG199K_KV_SLOT  head_start=146 can_use=32907
    tokens and all 128 top-5 probabilities equal, 2542.02 / 26.62

`head_start=146` and `can_use=32907` are the live evidence that slot selection now runs through
Goose: 32907 cell-reuse decisions in one pass. Gate is 67929 checks / 0 failures, with the new
decisions compared against the native expressions over 400 randomised cases each.

## Next

`prepare` itself -- the trial-placement loop with its recovery stack -- and then the
`llama_kv_cache_context` seam.

## Cache-level seq_rm decisions ported and live

From `llama-kv-cache.cpp:404-467`:

- `pos_in(pos, p0, p1)` -- llama-kv-cells.h:421, `pos >= p0 && pos < p1`.
- `norm_range(p0, p1, out)` -- lines 416-422, the negative-bound normalisation. One native
  property had to be preserved rather than "improved": an inverted range is not an error there,
  it simply matches nothing, so the module does not assert `p0 <= p1`.
- `head_after_rm(freed, new_head, head)` -- lines 435-437.

Apex:

    FG199K_KV_CELLS goose=65814 cleared=0 native=0 purge=0 head=292
    FG199K_KV_SLOT  head_start=146 can_use=32907
    FG199K_KV_SEQRM pos_in=2621440 head_after_rm=0
    tokens and all 128 top-5 probabilities equal, 2547.10 / 25.15

**2.6 million `pos_in` decisions in one pass**, all Goose, `native=0`. `head_after_rm=0` because
nothing is freed in this profile -- the same fact as `cleared=0`, recorded rather than hidden.

Gate: 69529 checks / 0 failures.

## Running total for KV

| decision | Goose function | live calls per pass |
|---|---|---:|
| cell ownership (8 ops) | `rm`/`seq_rm`/`seq_keep`/`seq_add`/`pos_set`/`seq_count`/`is_empty`/`used_max_p1` | gate + 65798 placements |
| token placement | `place_cell` | 65798 |
| purge condition | `should_purge` | 0 (unreachable here) |
| head advance | `head_after_place` | 292 |
| slot search | `head_start`, `can_use_cell`, `is_masked_swa` | 146 / 32907 |
| range clear | `pos_in`, `norm_range`, `head_after_rm` | 2621440 |

## Next

`prepare`'s trial loop and the `llama_kv_cache_context` seam; then the same treatment for
`seq_cp` / `seq_keep` at cache level.

## seq_cp decisions ported; the path is not exercised here

From `llama-kv-cache.cpp:473-560`: `same_stream`, `seq_cp_self` (copy onto itself is a no-op) and
`seq_cp_is_full` (cross-stream copies are only supported for a full buffer, otherwise native
asserts). All three are gated against the native expressions over 300 randomised cases.

Apex:

    FG199K_KV_SEQCP cp_decisions=0

So `seq_cp` is **not called on this profile** -- a single-sequence run never copies KV between
sequences. The decisions are ported and covered by the differential gate; the live path is absent
rather than proven, and per the standing rule an unexercised path keeps its native fallback
(`GGML_GOOSE_KV_CELLS=0`) and is recorded here instead of being counted as a landed integration.

Everything else is unchanged and still exact:

    FG199K_KV_CELLS goose=65814 cleared=0 native=0
    FG199K_KV_SLOT  head_start=146 can_use=32907
    FG199K_KV_SEQRM pos_in=2621440 head_after_rm=0
    tokens and all 128 top-5 probabilities equal, 2537.45 / 26.57

Gate: 70429 checks / 0 failures.
