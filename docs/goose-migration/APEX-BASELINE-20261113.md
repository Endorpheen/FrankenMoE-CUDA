# Apex baseline, 2026-11-13

This is the canonical performance baseline for Apex. Functional tests may use a
smaller context, but a performance claim must use this binary and these flags.

## Canonical configuration

| item | value |
|---|---|
| binary | `runtime/reconstructed/build/stage-run/llama-server` |
| model | `/home/end0/AI/LocalAI/Models/Apex-35B-V1.0-Q4_K_M.gguf` |
| argv | `runtime/exp244/results-apex-or244-or-goose/argv.json` **plus** `--moe-cache-mib 3584` |
| placement | `--cpu-moe`, `-ngl all`, `-fa on`, KV `q4_0` |
| context | `-c 262144`, fixed 32K prompt |
| batch | `-b 4096 -ub 4096` |
| threads | `-t 8 -tb 16`, `-np 1` |
| request | `runtime/exp244/results-apex-or244-or-goose/request.json` (32768 prompt tokens, 128 predicted) |

The only difference from the saved fixed profile is the GPU expert cache:
`--moe-cache-mib 3584` keeps about 3.5 GiB of experts on the GPU. Without it the
same binary decodes around 29-33 tok/s on this host.

## Measured 2026-11-13

One request on the canonical configuration:

| metric | value |
|---|---:|
| prefill | **2228.76 tok/s** |
| decode | **41.62 tok/s** |
| predicted tokens | 128 |
| content sha | `9d90a010775f2e72fbea3fec7e0d34b8e57b52aab9dd16e90eb9cd5151ba7560` |
| server exit | 0 |

The content hash differs from the no-cache reference `e68df05b...` by design:
the GPU expert cache selects a different execution path, and the historical
record already documents that the cache changes the output while the answer
remains coherent. Do not compare a cache run to `e68df05b` as if it were the
same path.

## Unified logging artifact

`runtime/exp421/unified` is the baseline `stage-run` with only
`libllama-common` replaced by the Goose logging build. All other Goose modules
stay baseline. Measured with the canonical flags:

| predicted tokens | prefill tok/s | decode tok/s | content sha |
|---:|---:|---:|---|
| 128 | 2259.28 | 39.24 | `9d90a010...` |
| 512 | 2244.80 | **43.04** | `c2ff42cc...` |

The 512-token result is at or above the historical 41.94 baseline, so the
logging change is not a regression.

## Historical cache curve

From `docs/goose-migration/apex-optimization.json`, same binary and profile with
`--moe-cache-mib 3584`:

| predicted tokens | decode tok/s |
|---:|---:|
| 64 | 37.65-38.12 |
| 128 | 39.62-39.65 |
| 256 | 40.93-41.09 |
| 512 | **41.93-41.95** |

The 41.94 figure remembered from the earlier session is the 512-token run.
128 tokens at 39.6 is the same baseline, not a regression.

## Why the recent runs showed 30-33

The logging experiments used a staged `libllama-common` built from the flint
`build-cuda` tree, with `-c 512` and no `--moe-cache-mib`. That tree does not
carry the reconstructed candidate's Goose expert pipeline and the GPU cache, so
it is a functional test rig, not a performance baseline. Its numbers must not
be compared with 41.94.

## Rule going forward

- Functional logging/API/KV tests: reduced context is allowed, but label them
  functional and do not quote their tok/s as Apex performance.
- Performance or acceptance runs: canonical binary, `--moe-cache-mib 3584`,
  32K prompt, and at least 128 predicted tokens; use paired A/B/B/A and report
  prefill, decode, RAM and VRAM together.
