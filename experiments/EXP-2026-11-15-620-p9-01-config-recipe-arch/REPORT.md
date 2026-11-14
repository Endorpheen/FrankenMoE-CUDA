# REPORT — EXP-2026-11-15-622 — P9-01 config, recipe and architecture registry

Date: 2026-11-15
Base commit: `df3ce73a` (runtime tree identical at `78134de4`)
Kind: `correctness` (model-free differential gate)
Verdict: **ACCEPTED**
Experiment directory: `/tmp/opencode/sub-622/EXP-2026-11-15-622-p9-01-config-recipe-arch/`
Repository changes: none (no production file, ledger file or canon file touched)

## Outcome in plain words

The three policy modules of P9-01 now exist as standalone Goose source and are
proven against the native code they are meant to replace:

1. the config-consistency gate (`validate()`), all 48 rules in their native
   order;
2. the MoE architecture registry and the tensor-name matcher that turns a gguf
   tensor name into a recipe slot (`find_moe_recipe`, `moe_recipe_at`,
   `n_moe_recipes`, `match_expert`);
3. the CLI flag/environment/default resolution in front of that config.

The first two are compared against the **linked native functions**, not against
a re-implementation. The third is compared against a transcription of
`main.cpp`, because that file cannot be built without the engine; the
transcription is quoted line by line and a pinned table backs it.

## Correctness

```
S1 cfg  cases=66   fuzz=40000  values=40000  failures=0  rules covered 49/49 (0 + 48)
S2 ar   rows=9     variants=39 match_names=20632 fuzz=20000 fuzz_hits=478 failures=0
S3 cc   fields=11  atoi=22     matrix=936  draft=9     fuzz=20000 passes=20000 failures=0
```

- S1: 66 named cases (one per rule plus boundary and order pins) and 40 000
  deterministic fuzz configs. Native and port agree on every config; the fuzz
  alone reaches 47 of 48 rules. NaN boundaries are exercised for both float
  ranges, and a short flat layout is rejected as a caller error.
- S2: every row is checked against `moe_recipe_at`, every suffix against the
  native pointer (including gemma4's null third slot), and every lookup against
  `find_moe_recipe`. 39 near-miss strings confirm exact matching. 20 632
  tensor names are compared with the native `match_expert` transcription,
  including companion tensors (`.scale`), wrong tails, cross-layout names,
  whitespace and sign forms, `-0`, and oversized layer numbers. 20 000
  composed-name fuzz cases add 478 real matches.
- S3: 936-cell matrix over (flag seen x env present x raw value x flag value x
  default), all 16 `cache_auto` combinations, 22 `atoi` values against
  `std::atoi`, 9 draft-source pairs, 11 dense-mode names and 20 000 random
  precedence cases.

The same three gates pass unchanged with the Goose module compiled at `-O0`.

## Mutations

12 mutations, two halves of six, 12/12 caught (`tests/mutate.py 1|2`):

| half | slice | mutation | gate reaction |
|---|---|---|---|
| 1 | cfg | `n_predict` zero accepted | 11 failures |
| 1 | cfg | `n_ubatch == n_ctx` rejected | 11 failures |
| 1 | cfg | `cache_mb = 0` falls into the band | 17 failures |
| 1 | cfg | NaN passes the drop range | 11 failures |
| 1 | ar | gemma4 loses `ffn_down_exps` | 33 failures |
| 1 | ar | `.weight` accepts a trailing space | 1 180 failures |
| 2 | ar | first registry row skipped | 1 failure |
| 2 | ar | gemma4 renamed in the table | 3 failures |
| 2 | cc | typed flag loses to the environment | 287 failures |
| 2 | cc | empty variable read as 0 | 13 failures |
| 2 | cc | `BMOE_CACHE_MB` stops suppressing auto | 1 254 failures |
| 2 | cc | `ahwb` mapped to `anon` | 2 failures |

## Memory impact

None measurable and none claimed: these modules are pure decisions, no model is
loaded, no allocation path changes. The only runtime memory the integration
adds is the materialised recipe table (9 rows, 3 suffix pointers, a few dozen
bytes of string storage) inside the carrier adapter — a fixed cost already
present in the native table.

## Performance impact

None claimed. The integrated functions run once per process start
(`validate`, registry materialisation) or once per flag/env field, not in the
per-token path. `match_expert` is a startup-time tensor-name scan. No tok/s
measurement was made or is meaningful for this experiment.

## Files

```
source/config-validate.goose
source/arch-registry.goose
source/config-resolve.goose
tests/config_fixture.cpp
tests/arch_fixture.cpp
tests/config_resolve_fixture.cpp
tests/build.sh
tests/mutate.py
README.md
REPORT.md
results/gate-final.log
results/gate-o0.log
results/mutate-1.log
results/mutate-2.log
results/hashes.txt
```

SHA-256 for every file and gate binary: `results/hashes.txt`.

## Integration and boundaries

Exact steps, call order and the native interfaces are in `README.md`,
sections "Integration step (exact)" and "Interfaces the port relies on".
In one line: copy the three sources to `goose/cli/config-validate.goose`,
`goose/model/arch-registry.goose`, `goose/cli/config-resolve.goose`; add the
three `CANONICAL_SOURCES` rows; make `validate()`, the three registry
accessors and the CLI resolution block thin adapters over `cfg_`, `ar_` and
`cc_`; keep the English messages in the carrier.

Stays native: the graph, tokenizer, sampler, decode, KV, model loading and
gguf I/O, the JSON/CLI line protocol, the streamer's iteration over recipe
slots, and the message text of every validation failure.

## Risks

- The registry adapter must materialise the table once with stable pointers:
  `RouterHook` stores `const MoeRecipe &`, so a per-call buffer would dangle.
- `validate()` must narrow its `float` fields to single precision before
  calling the port; a raw `double` can disagree at the `[0,1]` boundaries.
- `cc_env_int` expects `getenv`'s raw bytes and the "was the flag typed" bit;
  substituting the default value for the bit reintroduces the bug the `seen`
  set exists to prevent.
- The recipe table is data: a new architecture still needs its row and the
  existing byte-identity run, exactly as `docs/adding-a-model.md` says.

## What would falsify the acceptance

Any divergence between `cfg_validate` and `validate`, between the `ar_` table
and `k_recipes`, or between the CLI resolution and `main.cpp` on a config the
engine can reach. The fuzz covers value combinations, not the carrier glue;
the first integration build must rerun these three gates from the rebased
canon.
