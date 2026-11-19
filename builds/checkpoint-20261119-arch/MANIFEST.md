# Checkpoint 2026-11-19-arch: the MoE recipe registry wired and verified live

Byte copies of the set that produced this stage's measurement. Rule 22: accepted sets are immutable; a new
stage gets a new directory, which is why this exists beside `checkpoint-20261119-chain`.

## Provenance

| item | value |
|---|---|
| source revision | `git-rev.txt` |
| build tree | `builds/flint-acc` (cmake, `LLAMA_BUILD_SERVER=ON`) |
| Goose compiler | `goose-2026.3-string`, `--standalone --header` for the chain unit |
| chain build | `tools/build-sampler-chain.sh`, entry points by exported name |
| model | Apex-35B-V1.0-Q4_K_M |
| profile | ctx 262144, `-b 4096 -ub 4096 -ngl all -fit off --cpu-moe --moe-cache-mib 4992` |

## What changed since the previous checkpoint

- `llama-arch.cpp` now calls the ported MoE recipe registry at every model load and checks **89** things
  against a transcription of `runtime/core/src/moe/arch_registry.cpp`: nine row names, `ar_exps` per row,
  27 suffix slots, and 53 matcher cases (including the `.scale` rejection and gemma4's fused row);
- the check runs once per load and the sweep is what caught two errors in my own verification, both recorded.

## Evidence

```
FG_ARCH_SWEEP recipes=89 disagreements=0
sha c2ff42cc1219   decode [44.93, 46.1, 46.29]   prefill [1967.9, 2273.3, 2279.7]
main runtime ok: 1192 global symbols, 0 duplicates
```

The acceptance hash is unchanged from the previous checkpoint, so the stage adds a verified call path without
altering behaviour or speed.

## Restore

```
LD_LIBRARY_PATH=<this dir>:<this dir>  <this dir>/llama-server -m <model> <profile>
```
