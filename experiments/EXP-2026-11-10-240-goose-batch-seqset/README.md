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

# 256-bit version: gated at 300,008 checks, but still not entered — cause NOT established

Finished the four-word version (`source/ss4.goose`, `source/bridge4.c`), because `LLAMA_MAX_SEQ` is 256:

    FG240Q_FIXTURE checks=300008 failures=0

8 named cases pinning every word boundary (63, 64, 127, 128, 192, 255) plus 20000 random with `nmax` up to
256. Mutations:

| mutation | failures |
|---|---:|
| word index `sid/64` → `sid/128` | 58293 |
| bit mask `sid&63` → `sid&31` | 33514 |

Integrated with the guard widened to `LLAMA_MAX_SEQ <= 256`, object rebuilt, and verified by `nm`:

    DSO defines   fg240q_build_sets4  fg240q_enabled  fg240q_generated_main  fg240q_init_globals
    object undef  fg240q_build_sets4  (alongside fg237s/fg238q/fg239b)

Then the run:

    FG240Q calls=0        prefill 2463.3  decode 24.29  tokens True  probs True  sha e68df05b
    witness superset vs exp239: verdict OK

**The route is linked and never entered.** I did not establish why, and I am not going to guess. The
candidates I would check first, in this order:

1. whether `llama_batch_allocr::init` is the path the server actually takes for this profile — `split_equal`
   does fire (146 calls) from the same class, which is why I expected `init` to run, but "same class" is not
   evidence that this function runs;
2. whether `batch.n_tokens` at that point exceeds the 65536 guard;
3. whether `flat` is empty because `batch.n_seq_id[i]` is zero for this batch shape.

A one-line unconditional counter inside the guard would answer all three in a single run, and that is the
next step rather than a fourth theory.

## Honest status of EXP240

Two gated implementations, 390,055 combined checks, zero failures, and **neither is exercised**. The first
was refused by its own guard (correctly); the second is linked and does not run for a reason not yet known.
Recording it as `#deferred` is the accurate label.
