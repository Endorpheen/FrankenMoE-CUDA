# P10-10: platforms and alternate backends - three decisions, no call sites, and what can be verified here

Checked 2026-11-19 on `main`, text level, no build.

## The three Goose decisions the requirement names

All three exist and are composed:

| module | doors | content |
|---|---|---|
| `goose/misc/cpu-features.goose` | 7 - `feat_arch64_linux`, `feat_arch64_apple`, `feat_arch64_windows`, `feat_arch64_score`, `feat_x86_score`, `feat_kleidiai_mask`, `feat_kleidiai_name` | architecture scoring and the KleidiAI mask/name |
| `goose/misc/backend-select.goose` | 10 - `dev_by_name`, `dev_reg_by_name`, `dev_by_type`, `dev_init_best`, `dev_best_score`, `dev_backend_filename`, `dev_match_candidate`, `dev_exe_prefix`, `dev_search_paths`, `dev_registered_ids` | backend and device registry decisions |
| `goose/misc/platform-paths.goose` | 8 - `plat_sep`, `plat_cache_dir`, `plat_config_dir`, `plat_cache_file`, `plat_set_env`, `plat_priority_noop`, `plat_priority`, `plat_tty_colors` | separators, cache/config paths, priorities, tty colours |

**None of them has a call site.** Sampling `feat_x86_score`, `dev_best_score`,
`plat_sep` and `plat_cache_dir` across `vendor/flint`, `runtime/` and `integration/` returns
nothing.

## What the platform variants actually are

The Apple and Windows branches are in the upstream C++, not in Goose: 22 files under
`vendor/flint/src` and `vendor/flint/ggml/src` carry `__APPLE__` or `_WIN32`. This host is Linux
x86_64, so those branches cannot be executed here at all.

Their honest status, stated so it cannot be mistaken for verification:

| variant | status |
|---|---|
| Linux x86_64, AVX2 | the only variant this project has ever executed; the accepted measurements are on it |
| Linux x86_64, AVX512 / AVX-VNNI / AMX | **compile-only**: the trees build, nothing was run on that hardware |
| Apple silicon | **hardware-unavailable**: the branches are upstream C++ and this host cannot run them |
| Windows | **hardware-unavailable**, same reason |
| KleidiAI | a mask and a name in `cpu-features.goose`, **not built or run** |

The requirement's own rule applies verbatim: a transcription of an unsupported platform is not a
test of it, and this document does not present one as such.

## What closing this needs

1. wire the three decisions where the runtime asks the same questions today. The CPU feature check
   and backend selection happen in ggml's C++ (`ggml-cpu` feature detection and the backend
   registry), so the call sites exist on that side and are the place to switch;
2. for `cpu-features`, gate the scoring against the native result on this host, where AVX2 is the
   only variant that can be compared;
3. update the capability ledger with the table above, which is the accurate statement of what is
   verified, what is compile-only and what cannot be run here.
