# EXP-2026-11-15-622 — P9-01: standalone config, recipe and architecture registry

Verdict: **ACCEPTED** (model-free, differential). Base commit: `df3ce73a`
(`docs: record the staged runtime integration plan`; the runtime tree is
identical at `78134de4`, verified with `git diff df3ce73a..HEAD -- runtime/`).

Work was done outside the repository, in
`/tmp/opencode/sub-622/EXP-2026-11-15-622-p9-01-config-recipe-arch/`, so the
shared tree, the production runtime and other agents' files are untouched.
There is no model and no tok/s number here by construction: P9-01 is the
policy layer of the runtime.

## Slices

Three independent decision sets, one experiment. Each slice has its own
`.goose` file, its own native oracle and its own gate.

| slice | Goose source | prefix | native counterpart |
|---|---|---|---|
| S1 config validation | `source/config-validate.goose` | `cfg_` | `runtime/core/src/config.cpp` — `bmoe::validate`, lines 7-263 |
| S2 recipe + arch registry | `source/arch-registry.goose` | `ar_` | `runtime/core/src/moe/arch_registry.cpp` (whole file) and `router_hook.cpp:25-39` (`match_expert`) |
| S3 CLI resolution | `source/config-resolve.goose` | `cc_` | `runtime/cli/main.cpp` — `env_int` 42-45, `--cache-mb` 686-691, `--dense-weights` 704-724, `--mtp/--ngram` 645-655, env block 792-802, `cache_auto` 808-809 |

### S1 — config validation (`cfg_`)

One export, `cfg_validate(i: const i64[:], f: const f64[:]) -> i64`, returns
`0` for a valid config or the number of the first failing rule in the native
order (48 rules). The flat layout is documented at the top of the file and
mirrors every field `validate()` reads. Numbers replace the English texts:
the texts name values and remedies and stay in the carrier.

Why the whole function and not one rule: the native contract is "first
failure wins", so rule *order* is as load-bearing as the bounds. Passing a
single function keeps that order observable to the gate.

### S2 — recipe and architecture registry (`ar_`)

| export | native |
|---|---|
| `ar_count()` | `n_moe_recipes()` |
| `ar_find(arch)` | `find_moe_recipe()` — exact `strcmp`, no aliases |
| `ar_arch(i, out)` | `moe_recipe_at(i)->arch` |
| `ar_suffix(i, slot, out)` | `moe_recipe_at(i)->exps_suffix[slot]`, `-1` for the native `nullptr` |
| `ar_exps(i)` | the non-null suffix count the streamer iterates (`session.cpp:1019-1026`) |
| `ar_match(name, row, lay)` | `match_expert`: `blk.<il>.<suffix>.weight`, first matching slot, exact `.weight` tail |

Nine rows are carried in the native table order, including the fused gemma4
layout (`ffn_gate_up_exps`, `ffn_down_exps`, unused third slot). Lookup is
exact: no case folding, no prefix match, no alias — that is a decision, not an
omission, and the gate pins it with 39 refused variants.

### S3 — CLI flag/env/default resolution (`cc_`)

| export | native |
|---|---|
| `cc_env_name` / `cc_flag_name` / `cc_env_default` | the 11-row table at 792-802 |
| `cc_env_int` / `cc_env_bool` | `env_int` and `!= 0` |
| `cc_cache_auto` | the `--moe-stream` auto-sizing default at 808-809 |
| `cc_cache_mb_kind` | the literal `"auto"` check at 686-691 |
| `cc_dense_mode` | `mmap\|warm\|anon\|ahwb` at 704-724 |
| `cc_draft_pick` | the `--mtp`/`--ngram` contradiction at 645-655 |
| `cc_atoi` | `std::atoi` prefix parse |

The precedence rule is the slice: a typed flag wins even when it repeats its
default, an empty environment variable is absent, and `getenv`/`atoi` are the
only native calls left.

## Interfaces the port relies on (canon and native)

- `bmoe::validate(const RunConfig &) -> ValidationResult` and the structs
  `RunConfig`, `SamplingConfig`, `MoeStreamConfig`, `SpecConfig`, `DraftSource`
  in `runtime/core/include/bmoe/config.h`; constants `cache_min_mb = 1500`,
  `io_threads_max = 8`, `prefetch_layers_max = 8`, `route_ahead_max = 8`,
  `draft_max_limit = 8`, `ngram_match_limit = 64` — repeated in `cfg_` because
  the module must build standalone.
- `bmoe::MoeRecipe` (`max_exps = 3`, `arch`, `exps_suffix[3]`),
  `find_moe_recipe`, `n_moe_recipes`, `moe_recipe_at` in
  `runtime/core/include/bmoe/recipe.h` and `runtime/core/src/moe/arch_registry.cpp`.
- `match_expert(const char *, const MoeRecipe &, int &)` in
  `runtime/core/src/moe/router_hook.cpp:25-39` (static, no ABI).
- `env_int`, the flag branches and the env table in `runtime/cli/main.cpp`;
  `std::getenv`, `std::atoi`, `std::strcmp`.
- Imported Goose modules: none. All three files are self-contained and declare
  no `import`, so `goose --standalone` is the whole build.

## Gate

`tests/build.sh [cfg|ar|cc|all] [final|mutate]`. The final gate compiles the
Goose source with `goose --standalone`, compiles it at `-O1`, and links it with
its fixture. The config and registry fixtures link the **real native objects**
(`config.cpp`, `arch_registry.cpp`) compiled from `FG_CORE`, so those two arms
are differential against the actual functions, not against copies. The CLI
fixture cannot link `main.cpp` without the engine, so its oracle is a
line-quoted transcription with a pinned table on top.

```
FG622_S1 cases=66 fuzz=40000 values=40000 failures=0 covered_rules=47/48 ok=17177 fail=22823
FG622_S2 rows=9 variants=39 match_names=20632 fuzz=20000 fuzz_hits=478 failures=0
FG622_S3 fields=11 atoi=22 matrix=936 draft=9 fuzz=20000 passes=20000 failures=0
```

- S1: 66 named cases cover all 49 outcomes (0 + 48 rules), including six
  rule-order pins and NaN boundaries; 40 000 fuzz configs compare native
  against port with no pins. The fuzz alone reaches 47 of 48 rules (rule 33 is
  reachable only through the order pin, since the band check fires first).
- S2: all nine rows checked against the native accessors and against a pinned
  table; 39 refused lookup variants; 20 632 tensor-name comparisons against the
  native transcription (9 rows x 3 slots x 6 layer numbers, 686 row/slot-specific
  hostile names, 15 number forms, cross-layout names) plus 20 000 composed-name
  fuzz, 478 of which are real matches.
- S3: the 11-row field table, 22 `atoi` values against `std::atoi`, a 936-cell
  flag/env/default/raw matrix, all 16 `cache_auto` combinations, 9 draft-source
  pairs, 11 dense names and 20 000 random precedence cases.

All three gates also pass unchanged at `-O0` (`results/gate-o0.log`).

## Mutations

`tests/mutate.py 1|2` — 12 mutations in two halves of six. Each rewrites one
decision, regenerates the slice at `-O0`, links its own fixture and reruns it.

Half 1 (config bounds and order; registry table and matcher): `n_predict` zero
accepted; `n_ubatch == n_ctx` rejected; `cache_mb = 0` treated as the
pathological band; NaN passing the drop range; gemma4 losing its down
projection; the `.weight` tail accepting a trailing space.

Half 2 (registry identity and lookup; CLI precedence): the first registry row
skipped by lookup; gemma4 renamed in the table; a typed flag losing to the
environment; an empty variable read as 0; `BMOE_CACHE_MB` no longer suppressing
auto-sizing; `ahwb` mapped to `anon`.

```
python3 tests/mutate.py 1   # 6/6 caught
python3 tests/mutate.py 2   # 6/6 caught
```

## Integration step (exact)

1. Copy the three accepted sources into the canon, keeping the experiment as
   the proof:

   | from | to |
   |---|---|
   | `source/config-validate.goose` | `goose/cli/config-validate.goose` |
   | `source/arch-registry.goose` | `goose/model/arch-registry.goose` |
   | `source/config-resolve.goose` | `goose/cli/config-resolve.goose` |

2. Add three rows to `CANONICAL_SOURCES` in `tools/sync-goose-tree.py`
   (target, experiment id, experiment-relative source, native description,
   gate result). Then run `python3 tools/sync-goose-tree.py` so
   `goose/MANIFEST.json` carries the new SHA-256 values, and
   `python3 tools/check-goose-tree.py` to verify hashes, experiment presence
   and export-name duplicates.
3. In the carrier, in this order — nothing else moves:
   - `runtime/core/src/config.cpp`: keep `validate()` as the exported symbol and
     its English messages; replace the body with (a) the `int64_t iv[32]` /
     `double fv[5]` fill from `RunConfig`, (b) `cfg_validate(iv, fv)`, (c) a
     `switch` from the returned rule number to the existing `fail(...)` text.
     Floats must be narrowed to `float` first and then widened
     (`(double) cfg.moe.drop_cold_frac`), because the native comparison is
     single-precision.
   - `runtime/core/src/moe/arch_registry.cpp`: keep the `MoeRecipe` ABI and
     materialise the table once (function-local `static` or `std::once`):
     `n = ar_count()`, then per row `ar_arch(i, buf)` and
     `ar_suffix(i, slot, buf)` into stable storage; `find_moe_recipe` =
     `ar_find` then `&rows[i]`; `moe_recipe_at` = range check then `&rows[i]`;
     `n_moe_recipes` = `ar_count()`. Pointers must stay stable for the process:
     `RouterHook` keeps `const MoeRecipe &`.
   - `runtime/cli/main.cpp`: after the flag loop and before `validate`, replace
     the 11 env lines with `cc_env_int`/`cc_env_bool` calls (`has_env` =
     `std::getenv(...) != nullptr`, `raw` = the variable's bytes), then
     `cc_cache_auto(...)` for the `--moe-stream` default; replace `v == "auto"`
     with `cc_cache_mb_kind`, the dense if-chain with `cc_dense_mode`, and the
     `--mtp`/`--ngram` check with `cc_draft_pick`.
4. Re-run the three gates from the rebased canon and record the result in the
   P9-01 ledger entry (`slices_total`, `slices_done`, `remaining`,
   `coverage_doc`) in the same change.

## Compatibility checks run

- `goose check` on each source: all three `typechecked ok`.
- Gate at `-O1` and `-O0`: identical summaries, 0 failures.
- Export names: `cfg_`, `ar_` and `cc_` do not occur in `goose/` (0 matches
  before integration), so no duplicate export is introduced. `cli_` was
  already taken by `goose/cli/cli-protocol.goose`, which is why the CLI slice
  uses `cc_`.
- `python3 tools/check-goose-tree.py` in the repository: `4131 file(s), hashes
  and experiments match`; the only warnings are pre-existing shared export
  names (`s3_*`, `value_*`) unrelated to these slices.
- No absolute scratch path in `tests/build.sh` or `tests/mutate.py`; both
  derive the experiment root from their own location.
- The repository was not modified: `git status` shows no path from this
  experiment, and `runtime/`, `goose/` and `tools/` carry only changes that
  pre-date this run (other agents' work).

## Assumptions and possible conflicts

- **Single-precision narrowing** (S1): the native fields are `float`; the port
  receives `f64`. The carrier must narrow to `float` before widening, or a
  value just outside `[0,1]` can disagree at the boundary. The gate's fixtures
  do exactly this.
- **Overflow is out of scope**: `std::atoi` and `sscanf("%d")` overflow are
  undefined in the native code. The port accumulates in `i64`; the gate stays
  inside the range where both agree, and no engine-generated name or env value
  approaches it.
- **Materialised table lifetime** (S2): the adapter's `MoeRecipe` rows must
  outlive every `RouterHook`. A function-local static array of string buffers
  satisfies this; a per-call temporary does not.
- **Repeated flags**: `main.cpp` keeps the last occurrence of a flag; the port
  models one resolved value per field. The carrier keeps last-wins and calls
  the port with the final value. This is not a loss of coverage of the
  resolution rule itself.
- **Strings matter only as emptiness** (S1): the port takes
  `model_path`/capture/replay as presence bits; a whitespace-only path stays
  "present", as in the native `empty()` check.
- **Matcher scope** (S2): `ar_match` reproduces `sscanf("blk.%d.%n")`, including
  leading whitespace, `+`, and `-0`. Embedded NUL bytes, names longer than the
  caller's buffers and `%d` overflow are out of scope.
- **No consumer loop** (S2): `session.cpp:1019` and `expert_stream_source.cpp`
  iterate `max_exps` and skip null slots; `ar_exps` answers the same question
  but the loops themselves stay native until their own P9 slices.
- **Concurrent module ownership**: `goose/cli/cli-protocol.goose` owns `cli_`;
  `goose/model/*.goose` graph builders know layer composition but contain no
  expert-suffix table (checked), so `ar_` does not duplicate an existing canon
  decision.
- **Ledger mirroring is not done here**: `docs/goose-migration/tasks.json` and
  its P9-01 entry are repository state and belong to the integration change,
  not to this isolated run.

## Recorded limits

- The graph, tokenizer, sampler, decode, KV, model loading and the JSON/CLI
  protocol are native and untested here. These slices decide policy only.
- S1 checks consistency, not defaults: the defaults it assumes (`n_ctx = 2048`,
  `cache_floor_mb = 1536`, ...) are the caller's `RunConfig`, and a changed
  default is still a valid config.
- S3's oracle is a transcription. It is quoted line by line and backed by a
  pinned table, but the file it transcribes is `main.cpp`, which this gate does
  not compile.
- The recipes are the table as of `df3ce73a`; adding a model family is a table
  row in the Goose file plus the existing byte-identity gate, as before.
- `cfg_` returns rule numbers; callers that need the native text keep it. A
  divergence in wording is therefore invisible to this gate, by design.

## Artifacts

```
source/config-validate.goose     S1
source/arch-registry.goose       S2
source/config-resolve.goose      S3
tests/config_fixture.cpp         S1 gate (native validate linked)
tests/arch_fixture.cpp           S2 gate (native registry linked + match transcription)
tests/config_resolve_fixture.cpp S3 gate (main.cpp transcription)
tests/build.sh                   three gates, -O1 final / -O0 mutate
tests/mutate.py                  12 mutations, two halves
results/gate-final.log           final run, -O1
results/gate-o0.log              same gates, -O0
results/mutate-1.log, mutate-2.log
results/hashes.txt               SHA-256 of every source, test and gate binary
```

Build artefacts (`build/gen-*.c`, `build/*.o`, `build/gate-*`) are reproducible
from the Goose sources plus the scripts and are not part of the evidence.
