# EXP272: server slot-selection decisions in Goose

Two decisions of `get_available_slot()` / `get_free_slots()` in `tools/server/server-context.cpp`, ported and gated.

## The decisions

| decision | native rule |
|---|---|
| `pick_similar` | among eligible slots, the strictly largest prompt-LCP fraction that is also strictly above `slot_prompt_similarity`; ties keep the earlier slot. The fraction is computed in f32 (`lcp / n_tokens`) exactly as native, so rounding matches |
| `pick_lru` | the first free slot preferring empty prompts, then the smallest `t_last_used`, ties keeping the earlier slot -- the order `std::stable_sort` produces |

Both return an index or -1; slots, their state and the prompt cache stay in the server.

## Gate

    similar best / below thr / tie keeps first / none eligible   OK
    lru prefers empty / oldest / tie keeps first / none idle      OK
    checks: 8, failures: 0

Fixture compares against transcriptions with the same float arithmetic. The gate was shown to fail: weakening both
strict comparisons to `>=` breaks two tie-break cases (8/2).

## Still to do

Integration into `get_available_slot()` behind a default-on switch, archive replacement and relink of
`libllama-server-impl.so`, then the concurrency A/B that already exercises slot choice (two clients on one slot plus a
cancel), comparing both clients' outputs and the witness.
