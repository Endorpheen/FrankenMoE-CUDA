# EXP243: compat position decision in Goose — ported, gated, not exercised

`llama_batch_compat::init`, the position block (`llama-batch.cpp:1441-1458`).

## What it preserves

    if (pos) { if (has_token) pos[0] = pos[i];                       // one per token
               else for j: pos[j] = pos[j*n_tokens + i]; }           // embedding: section-major
    else       pos[0] = pos_next[sids[0]]++;                          // auto from the FIRST seq id

Three traps, none of which changes a token value:

- the embedding layout is **section-major**, not row-major; row-major reads plausible numbers and silently
  transposes an M-RoPE batch. The token branch's single `pos[i]` is what makes "unifying" them tempting;
- auto-generation uses **`sids[0]`**, the first sequence id of the entry, and advances *that* counter;
- the counter is per sequence and shared across the batch, so entries of one sequence get consecutive
  positions in iteration order. Resetting per entry gives duplicate positions that look fine until attention
  reads them.

The `j >= 1` slots of a token batch are written as 0 because native leaves them untouched in a freshly
constructed `llama_batch_ext::token`, whose defaults are 0. That equivalence depends on the struct being
fresh per entry, and is stated here rather than assumed silently.

## Gate

    261197 checks, 0 failures

8 named cases plus 20000 random with `n_pos_per_embd` 1..4, both position modes and 1..6 sequences.

Mutations:

| mutation | failures |
|---|---:|
| embedding layout row-major instead of section-major | 25063 |
| auto-generation takes the LAST seq id instead of the first | 30855 |
| counter not advanced | 30446 |

### The mutation that first passed, and why that mattered

The `sids[0]`-vs-last mutation initially produced **0 failures**. The fixture gave every token exactly one
seq id, so first and last were the same element — the gate was blind to the trap it was meant to protect. The
fixture was widened to 1..3 seq ids per entry, which raised the same mutation to 30855 failures. A gate that
catches a bug only through one hand-written case is one refactor away from catching nothing; this is the
second time in this block that widening a generator turned a blind spot into a real check.

## Not exercised

    FG243C_COMPAT_POSITIONS calls=0
    FG242O_UBATCH_OUTIDS     calls=292
    prefill 2477.34  decode 25.337
    tokens True  probabilities True  sha e68df05b
    witness superset vs exp242: verdict OK

`llama_batch_compat` is the **legacy** `llama_batch` entry point: it is constructed only in
`llama_context`'s old-API `llama_decode`/encode paths (`llama-context.cpp:3838, 4602, 4607`). The server for
this profile uses the `llama_batch_ext` API directly, so the conversion — and this block with it — never runs.

That is a fact about the call graph, not a guess: the witness count is 0 and the constructor call sites are
the old-API ones. Same category as EXP218, EXP230, EXP231, EXP238.

## Performance

No pair was run: the route is not taken, so it cannot affect the profile's timings. The figure above is a
single run and is not a measurement of anything.
