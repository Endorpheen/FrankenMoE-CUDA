# Apex reference binary (restored)

Status: **restored and verified**, not started by this work. Model and GPU runs
still require the project owner's explicit approval.

## Source

| item | value |
|---|---|
| release | GitHub `Endorpheen/FrankenMoE-CUDA` tag `v0.1.0` |
| asset | `goose-runtime-v0.1.0-linux-x86_64.tar.zst` |
| asset size | 458 545 569 bytes |
| asset SHA-256 | `50cd22339489ee31ca3f78e066a12eb86d7c7fc1d81b495f4eb04735c916f9f2` (matches the published digest) |
| restored path | `/home/end0/AI/LlamaServer/apex-reference-v0.1.0/` (outside the working build, 1.6 GB, 104 files) |
| `llama-server` SHA-256 | `badbfc5cf63895a9381ae267d77e99e11ac5038e8886da864a37ad9d19f10d26` (15 960 bytes) |
| bundle manifest | `MANIFEST.sha256`, 8 of 8 libraries verified, 0 mismatches |

The binary hash matches the anchor named in `P11-04-REMOVAL.md` and in the M1
report, so the rollback anchor is no longer missing (risk R-2 closed).

## What the bundle is

A copy of `runtime/reconstructed/build/stage-run` from 2026-11-13: `llama-server`
0.7.0-dev (build 1, commit `1b44e307`) with the Goose modules linked in, plus the
exact shared libraries selected by `ldd`. `LAUNCH.txt` requires
`LD_LIBRARY_PATH` to point at the bundle directory, otherwise the system selects
a different build.

## Reference numbers to preserve

| item | value |
|---|---|
| prefill | 2228.76 tok/s |
| decode | 41.62 tok/s, best observed 43.04 tok/s |
| GPU | RTX 4070 12 GB |
| flags | `--cpu-moe -ngl all -fa on`, KV `q4_0`, context 262144, batch/ubatch 4096, threads 8/16, MoE cache 3584 MiB |

## Usage rule

The bundle is the A/B control. The working build and the installed profile are
not replaced by it, and the bundle itself is not started for real tests without
approval. The run command for a future approved A/B is in `APEX-TEST-PLAN.md`;
`LAUNCH.txt` inside the bundle carries the library-path requirement.

## Verification performed here

- asset SHA-256 compared with the release digest: equal;
- archive extracted; `llama-server` and 8 libraries hashed;
- `MANIFEST.sha256` re-checked by basename: 8/8;
- `llama-server` hash compared with the documented anchor: equal.

No model was loaded and no server process was started.
