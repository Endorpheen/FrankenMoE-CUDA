# EXP209: incremental per-sequence min/max (replaces the rejected EXP207 scan)

EXP207 answered `seq_pos_min/max` with a linear scan and cost 59% of decode (26.46 -> 10.76),
because those queries sit on the hot path: `can_use_cell` calls them once per tested cell inside
the slot search. This module keeps the answer instead of recomputing it.

## Result — ACCEPTED

    FG209I_KV_SEQPOS ins=65814 rem=8 bound=8
    2547.67 / 26.39, tokens and all 128 probabilities equal, sha e68df05b

| criterion | value | requirement |
|---|---:|---|
| prefill | 2547.67 | >= 2500 |
| decode | 26.39 | >= 25 |
| prefill vs EXP206 | -0.25% | > -2% |
| decode vs EXP206 | +1.18% | > -2% |

Gate: 40,877 checks / 0 failures over 400 randomised insert/remove sequences against a
`std::set<(pos,cell)>` transcription.

## Design

Three int64 slots per sequence — `mn | mx | cnt` — updated from `seq_pos_inc` / `seq_pos_dec`,
queried in O(1) by `seq_pos_min` / `seq_pos_max`. A removal that hits a bound asks the host to
rescan that one sequence, so the rescan is O(cells) but happens only when a bound actually moved:
the live witness shows 65,814 inserts against 8 rescans.

The arena lives **outside** the object (`static inline int64_t * goose_bounds`), allocated in
`resize()`. Adding members would change `sizeof(llama_kv_cells)` for every TU built against the
unmodified header, which is exactly what produced an HTTP 500 in EXP206.

## Two defects the gate found

1. **The count is not optional.** Native keys the set on `(pos, cell)`, so one position can appear
   more than once for a sequence. Min/max alone cannot distinguish one entry at `p` from two, so
   removing one of a duplicate pair wrongly reported the set empty. Caught at min == max == 5 with
   two entries.
2. **A gate defect of mine**: I first generated duplicate `(pos, cell)` pairs, which `std::set`
   silently refuses to grow. Fixed by giving each insert a unique cell index.

Also a bridge defect: after adding `cnt_at`/`cnt_set` the emitted `gg_function_N` numbering
shifted, and the bridge still pointed at the old ids. Fixed by probing each function with
distinguishing arguments rather than trusting the previous mapping.

## Switch

`GGML_GOOSE_KV_SEQPOS_INCR=0` keeps the native `std::set` answers.

## Not modelled

`seq_pos_get` + `upper_bound` (`llama-memory-hybrid-idx.cpp:414`): hybrid-idx memory is not
instantiated in this profile.
