# EXP232: recurrent state-write range grouping in Goose — ACCEPTED, and it is live

The grouping loop at the top of `llama_memory_recurrent::state_write` (llama-memory-recurrent.cpp:789-844)
builds three things in one pass: the matching cell count, `cell_ranges` (contiguous index runs) and
`cell_ranges_data` (contiguous logical-row runs).

## Why this one mattered

I had the serialization family recorded as "liveness unverified". It is **live**: the server log shows
prefix checkpoints being saved —

    RAM-only prefix checkpoints (recurrent): enabled, budget 2048.0 MiB
    RAM recurrent prefix saved: tokens = 28672, size = 62.813 MiB, retained = 2

and the witness confirms it in the run: `FG232R_RECR_STATE_RANGES calls=9`. Nine calls per request is
small, but this is the first route added since EXP211 that is genuinely on the measured path *and* does
real work.

## The rule worth gating

Per matching cell the logical row is

    cell_id = rs_idx_cur * size + (src >= 0 ? src : i)

and a new data range starts **only** when `cell_id` is not exactly the previous range's end. So two
adjacent cells with a `src` jump give ONE index range but TWO data ranges. A naive implementation that
appends a data range per contiguous index run gets the index ranges right and the data ranges wrong —
which is exactly the mutation that fails 5897 checks.

## Gate

    205000 checks, 0 failures

9 named cases — including `src-jump` (adjacent cells, discontinuous rows), `src-consecutive`,
`src-duplicate`, `holes`, `tail-and-hole`, `rs-offset` — plus 20000 random arrays. Asserts the cell count,
both range vectors element by element, both counts, and that nothing is written past them.

Mutations:

| mutation | failures |
|---|---:|
| discontinuity test replaced by "always start a new range" | 5897 |
| `src >= 0` branch removed (always use `i`) | 28991 |

## Live

    FG232R_RECR_STATE_RANGES calls=9
    prefill 2549.6  decode 25.10
    tokens True  probabilities True  sha e68df05b

Witness superset vs `exp231`: no missing, `new FG232R`. All three recurrent routes verified present in
`llama-memory-recurrent.o` before linking (`fg210k_ fg231a_ fg232r_`).

## Two of my own defects

1. **Segfault on first run**: the bridge passed `rs` as a scalar while the Goose module reads it as a
   per-cell array. The fixture caught it immediately, but only because it runs; a route that had been
   shipped without a gate would have crashed the server.
2. **The first integration did not skip the native loop** — Goose filled the vectors and then the native
   loop overwrote them, so the port would have been a no-op that still passed every gate. Fixed with a
   `fg232r_done` flag guarding the loop condition. Worth stating plainly: that failure mode produces
   correct output from the wrong code path, and only the witness count would have revealed it.
