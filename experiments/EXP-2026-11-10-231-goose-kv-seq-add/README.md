# EXP231: `llama_memory_recurrent::seq_add` in Goose — ported, gated, not exercised here

## Gate

    40033 checks, 0 failures

11 named cases chosen for the decision edges, plus 20000 random ones:

- `shift == 0` returns immediately;
- a **negative `p1` means "no upper bound"**, not an empty range — the case that distinguishes
  `p1 = INT64_MAX` from any small substitute;
- `p0 == p1` after clamping is an empty range and must not write;
- `pos == p1` is **exclusive** upper bound (`pos < p1`), `pos == p0` is inclusive;
- a cell whose tail exists but belongs to a **different** sequence is not shifted.

Mutations, all caught:

| mutation | failures |
|---|---:|
| negative `p1` clamped to 0 instead of INT64_MAX | 694 |
| `pos < p1` relaxed to `pos <= p1` | 16 |
| the `tail_has_seq` guard dropped | 906 |

## Not exercised in this profile

    prefill 2547.6  decode 25.14
    tokens True  probabilities True  sha e68df05b
    FG231A_KV_SEQ_ADD calls=0

Zero calls: `seq_add` only runs when a sequence position range is shifted, which the fixed 32K/128
profile never does. Same category as EXP218 and EXP230 — correct, gated, unreachable here. It is shipped
because the switch defaults to enabled and costs nothing when unreachable.

Witness superset against `exp230`: no missing routes, and both `fg210k_` and `fg231a_` verified present
in `llama-memory-recurrent.o` with `nm -u` before linking.

## Integration note

The Goose side receives the already-normalised bounds and returns `(should_write, new_pos)`, so the cell
stays owned by C++. My first bridge had the wrong signature for the store helper — the generated code
declares `void(uint64_t, int64_t)` where I had written `void(uint64_t, int64_t, int64_t)`. The compiler
caught it; that is the second time in this stretch that a bridge signature mismatch surfaced at build
time rather than silently.
