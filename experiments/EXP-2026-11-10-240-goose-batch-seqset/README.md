# EXP240: sequence-set construction in Goose — ported, gated, guard keeps it OFF

## What it preserves

Native (llama-batch.cpp:271-285) builds, per token, the **set** of sequence ids it belongs to, plus the
union over the batch:

    for i in tokens: for s in seq_id[i]: cur.set(seq_id[i][s]); unq.set(...)
                     seq_set.push_back(cur); seq_set_map[cur].push_back(i)

`seq_set_map` is keyed by that set, so two tokens with the same ids must produce identical bits. A wrong
packing does not fail any token-level check; it silently changes how tokens group when the batch splits.

## Gate

    90047 checks, 0 failures

7 named cases (empty batch, single id, multi-id, high bit 63, empty set) plus 20000 random.

The mutation history here is worth recording because it shows a gate being **strengthened**, not weakened:

| attempt | random range | failures for `1<<(sid&31)` |
|---|---:|---:|
| first | `nmax` 1..32 | only **2**, both from the named high-bit case |
| after widening to 1..64 | `nmax` 1..64 | **5344** |

The first fixture never generated a seq id above 31, so the 32-bit-shift bug was essentially invisible. I
widened the generator rather than accepting a mutation that "passed by 2". A gate that catches a bug only
through one hand-written case is one careless refactor away from catching nothing.

## Not exercised — and the guard is correct

    FG240S_BATCH_SEQSET calls=0
    prefill 1477.1  decode 23.96
    tokens True  probabilities True  sha e68df05b
    witness superset vs exp239: verdict OK

**`LLAMA_MAX_SEQ` is 256** (`src/llama-cparams.h:8`), not 64. The module was written for a single 64-bit
word and the call site guards on `LLAMA_MAX_SEQ <= 64`, so the route does not fire — correctly. Had the guard
been absent, sets with ids ≥ 64 would have been silently truncated and the batch split would have grouped
wrong tokens.

At 146-style call frequency this would be a real port; as written it is dormant. Finishing it needs
**four** 64-bit words per set (256 bits) through the module, which is a mechanical change to the bit loops and
the gate, not a redesign.

## What this stage is worth

Two things, both real: a gated implementation of the set construction, and a demonstration that the
conservative guard did its job. The second is the more useful of the two — it is the difference between a
dormant route and a silently wrong batch split.

## Performance

**Прогон выполнен при параллельной нагрузке. Показатели производительности могут быть искажены.** Prefill
1477.1 is far below the usual band; nothing is concluded from it, and the route is not even taken.

---

# 256-bit version: gated at 300,008 checks and LIVE — after finding three of my own bugs

`LLAMA_MAX_SEQ` is 256 (`src/llama-cparams.h:8`), so a set is four 64-bit words. Finished that version and it
now runs: `FG240Q_BATCH_SEQSET calls=146`, tokens and all 128 probabilities exact, witness superset OK.

    gate  300008 checks, 0 failures   (8 named word-boundary cases + 20000 random, nmax to 256)
    mut   word index sid/64 -> sid/128   58293 failures
    mut   bit mask  sid&63 -> sid&31     33514 failures

## What actually blocked it — three bugs of mine, found in order

1. **Wrong field.** The first integration read `batch.n_seq_id[i]` / `batch.seq_id[i]`, which belong to the
   `llama_batch` view that `split_equal` uses. `init` works from `batch_inp.tokens[i].seq_ids`, so `flat`
   was always empty and the route never fired. `seq_ids` is an `unordered_set<int>`, not a vector — the
   bitmask is order-independent, so iteration order does not matter, but the type does.
2. **Stale symbol.** After switching to the 256-bit module the declaration still named `fg240s_enabled`
   while only `fg240q_enabled` existed, so the server died at startup with
   `undefined symbol: fg240s_enabled`. `nm -u` on the object showed the mismatch in one second.
3. **The runner does not use the directory I built.** It loads `runtime/exp240/run`. I had linked into
   `run2`, `run3` and `run4` and watched three runs report zero calls against the **old** DSO. The
   `loaded-library-hashes.json` of the run names the exact path and sha — reading it would have saved all
   three.

## The probe that settled it

Guessing three times was the mistake. A temporary unconditional counter inside the guard answered it in one
run:

    FG240PROBE block=146 guard=146 flat=146

So the block runs 146 times, the guard passes 146 times, and `flat` is non-empty 146 times. The probe was
removed afterwards and the final build re-verified — `FG240PROBE` count is 0 in the shipped run.

## Why this took so long

Each failure looked like the previous one: a linked route that does not fire. I kept forming a new theory
instead of instrumenting. The lesson is the one this project keeps relearning from the other direction — one
counter beats three hypotheses, and the runner's own `loaded-library-hashes.json` already recorded which
binary it used.

## Live

    FG240Q_BATCH_SEQSET calls=146
    prefill 958.6  decode 20.18
    tokens True  probabilities True  sha e68df05b

Prefill 958.6 is far below the normal band; **Прогон выполнен при параллельной нагрузке. Показатели
производительности могут быть искажены.** Nothing is concluded from it.

## Performance

Not measured as a pair. 146 calls over `n_tok` tokens each, with a vector rebuild per token, so this one
*does* deserve a real pair when the host is quiet — unlike the 146-call single-scalar ports.
