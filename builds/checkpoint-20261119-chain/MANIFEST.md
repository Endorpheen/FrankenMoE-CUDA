# Checkpoint 2026-11-19: sampler chain accepted

Preserved byte copies of the set that produced the acceptance measurement. Rule 22: an accepted set is
immutable, and this is a copy, not a symlink.

## Provenance

| item | value |
|---|---|
| source revision | see `git-rev.txt` |
| build tree | `builds/flint-acc` (cmake, `LLAMA_BUILD_SERVER=ON`) |
| Goose compiler | `goose-2026.3-string` (canonical), used with `--standalone --header` for the chain |
| chain build | `tools/build-sampler-chain.sh`, entry points by exported name |
| model | Apex-35B-V1.0-Q4_K_M |
| profile | ctx 262144, `-b 4096 -ub 4096 -ngl all -fit off --cpu-moe --moe-cache-mib 4992` |

## What it carries

- every previously accepted component: Q4_K tiled kernel (source-level integration), allocator,
  and the nine sampler stages;
- `goose/speculative/spec-decisions.goose` and `goose/model/tokenizer-decisions.goose`, composed into the
  accumulated unit (1184 symbols, 0 duplicates);
- the sampler chain addressed by exported names, no generated indexes anywhere.

## Evidence

- differential: typical 1800/1800, top-p 900/900 and 45/45, XTC 720/720, DRY 200/200, adaptive-p 33211
  logits, mirostat 500/500, RNG 1280/1280, grammar decision 0 of 1736513;
- acceptance: six official `apex-test.sh` runs, all with hash `c2ff42cc1219` (the baseline control hash),
  decode -0.18% and prefill -0.19% (EXP-2026-11-18-1225);
- runtime: `build-main-runtime.sh` green, 1184 symbols, 0 duplicates (EXP-2026-11-18-1226).

## Restore

```
export LD_LIBRARY_PATH=<this dir>:<this dir>
<this dir>/llama-server -m <model> <profile from above>
```

## Restore verified

The set was started **from this directory alone** (`APEX_SERVER_DIR=<here>`, nothing from the build tree) and
measured through the official entry point:

```
decode [45.35, 45.47, 45.28]   prefill [1969.0, 2265.7, 2256.9]   sha {'c2ff42cc1219'}
```

Same hash as the acceptance run, same speed range. So the copies are complete: a checkpoint that cannot be
started is not a checkpoint, and this one can.
