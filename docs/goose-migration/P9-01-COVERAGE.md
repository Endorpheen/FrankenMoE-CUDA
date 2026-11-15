# P9-01 — standalone config, recipe and architecture registry

Status: **PARTIAL** — 3 of 3 slices accepted; the call sites are still native.

## Slices

| slice | content | exports |
|---|---|---|
| S1 config validation | the 48 ordered rules of `validate()` and their outcomes/order | `cfg_validate` |
| S2 recipe registry | the 9-row MoE recipe table, expert name matcher and suffix accessors | `ar_count`, `ar_find`, `ar_arch`, `ar_suffix`, `ar_exps`, `ar_match` |
| S3 CLI resolution | typed flag > env > default for 11 fields, auto cache band, dense mode, draft source | `cc_env_*`, `cc_cache_auto`, `cc_cache_mb_kind`, `cc_dense_mode`, `cc_draft_pick`, `cc_atoi` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-620-p9-01-config-recipe-arch`.
- Canon: `goose/cli/config-validate.goose`, `goose/model/arch-registry.goose`,
  `goose/cli/config-resolve.goose`.
- Gates: 66 named cases + 40000 fuzz configs against the linked native
  `validate()` (47/48 rules reached); 9 rows, 20632 tensor names and 20000 fuzz
  names against the native accessors and `match_expert`; a 936 matrix plus 20000
  fuzz for the resolver. All 0 differences, also at -O0.
- Mutations: 12/12 in two halves.

## Remaining

Wire `validate()`, `arch_registry.cpp` and `main.cpp` to the Goose decisions.
Host rules: narrow f64 to float before `validate()`, give the recipe table static
lifetime because `RouterHook` keeps references, keep atoi overflow and
NUL-in-name outside the corpus.

## Boundaries

Graph, tokenizer, sampler, decode, KV, load, gguf I/O and the validation error
texts stay native. The flat `iv[32]`/`fv[5]` layout is hand-maintained.
