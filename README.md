# FrankenMoE-CUDA

Runs Mixture-of-Experts models that do not fit in GPU memory on one consumer GPU. Derived from
`llama.cpp` and being migrated, module by module, to the **Goose** language. Reference setup:
Apex-35B-V1.0 Q4_K_M, Ryzen 9 5950X, 64 GB RAM, RTX 4070 12 GB.

## Latest release

[v0.12.0-chat-alpha](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.12.0-chat-alpha)
(Latest) packages the standalone Pure Goose chat host for **Linux x86-64** and **Windows x64**
with the cumulative runtime from EXP-1722 through
[EXP-2026-11-23-1805](https://github.com/Endorpheen/FrankenMoE-CUDA/blob/claude/apex-100-prefetch/experiments/EXP-2026-11-23-1805-route-prefetch/README.md).
It serves a web chat and an OpenAI-compatible `/v1` API for OpenCode.

| | |
|---|---|
| decode, 32K prompt / 512 output | **79.85 tok/s**, against 76.91 for EXP-1786 in two A/B pairs, 512 token ids identical |
| prefill, 32K prompt | about **3075 tok/s** |
| decode, short chat (34 prompt / 384 output) | **87.19 tok/s**, against 35.47 for v0.11.2 on the same flags |
| RAM / VRAM | 21.5 GB RSS / 8770 MiB peak sampled (32K profile) |
| profile | ctx 262144, expert cache 6290 MiB, batch/ubatch 4096, KV q4_0, threads 8/16 |
| archives | `FrankenMoE-v0.12.0-chat-alpha-linux-x64.tar.gz`, `FrankenMoE-v0.12.0-chat-alpha-windows-x64.zip`, see `SHA256SUMS` |

All figures are single-response numbers on one RTX 4070, not aggregate throughput. The newest step,
EXP-1805, predicts the experts of the next layer from the current one and copies them while the
current layer computes. The model still uses its own routes, so outputs do not change. The Windows
build passed the HTTP transport gate and a CPU-only model start under Wine. GPU inference on native
Windows still needs community testing.

### Previous runtime record

**EXP-2026-11-19-1476**, the `llama-server` based cumulative build published as
[v0.10.0-exp1476](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.10.0-exp1476).

| | |
|---|---|
| decode | **62.86 tok/s** median (62.53 / 63.26 / 62.86) |
| prefill | **2940.9 tok/s** median (2690.8 / 2947.1 / 2940.9) |
| RAM / VRAM | 20.8 GB RSS / 9511 MiB |
| answer hash | `c2ff42cc1219` (the record hash of the accepted baseline) |
| profile | ctx 262144, `-b 4096 -ub 4096 -ngl all -fit off --cpu-moe --moe-cache-mib 4992`, KV q4_0 |
| source | `e7c9155a01938b1825189240e7088620c6c7cd85`, tag `v0.10.0-exp1476` |
| archive | `frankenmoe-apex-exp1476-linux-x86_64.tar.gz`, sha256 `23857b8834736c60e161a430d0e3de4d7c2dc50eb6e84414f74aa18f89a32833` |

All 20 accepted Goose components are in the shipped library
(`tools/check-accepted-components.py`), and `tools/check-cumulative-baseline.py` passes on the
release set. Earlier checkpoints and releases stay untouched: EXP906 remains the historical
control, EXP1298 the previous baseline.

## Goose migration

121 of 152 functional requirements are DONE; the remaining 31 are listed with their acceptance
criteria in the [migration ledger](docs/goose-migration/tasks.json). Goose sources live under
[`goose/`](goose/README.md), with provenance and hashes in
[`goose/MANIFEST.json`](goose/MANIFEST.json) enforced by `python3 tools/check-goose-tree.py`.

The migration is not complete, and the shipped runtime is not claimed to be C++-free.

## Tests

Permanent checks live in [`tests/`](tests/README.md): one directory per test with a `run.sh`, a
`tier` and a `README.md`, and [`tests/REGISTRY.md`](tests/REGISTRY.md) lists each with the
experiment and the requirement it covers.

```
bash tests/run.sh list          # every registered test
bash tests/run.sh fast          # no model, no GPU, then tools/check.sh fast
bash tests/run.sh grammar       # one area
bash tests/run.sh integration   # tools/check.sh integration plus integration-tier tests
```

`fast`, `integration` and `full` delegate to [`tools/check.sh`](tools/check.sh), which owns the
stamps and the build ladder. Anything that starts a server or loads a model goes through
[`tools/apex-test.sh`](tools/apex-test.sh), which holds the lock and owns the `127.0.0.1:9101`
endpoint; those runs need explicit permission and are never implied by a tier.

## Run

The chat host from the latest release (Linux; on Windows use `start.bat` from the ZIP):

```
tar -xzf FrankenMoE-v0.12.0-chat-alpha-linux-x64.tar.gz && cd FrankenMoE-v0.12.0-chat-alpha-linux-x64
FRANKENMOE_WEB_ROOT="$PWD/web" ./frankenmoe-server --ctx-size 262144 --batch-size 4096 \
  --ubatch-size 4096 --cache-type-k q4_0 --cache-type-v q4_0 --expert-cache-mib 6290 \
  --threads 8 --threads-batch 16 /path/to/Apex-35B-V1.0-Q4_K_M.gguf --serve 8080
```

Open http://127.0.0.1:8080/ or point OpenCode at http://127.0.0.1:8080/v1. The accepted runtime
switches are on by default; any `GOOSE_*` variable you set wins, `GOOSE_EXPERT_PREFETCH=0` turns off
the route prefetch and `GOOSE_RUNTIME_DEFAULTS=0` restores the bare defaults. Choose an expert
cache that fits your free VRAM.

The previous `llama-server` record build:

```
tar -xzf frankenmoe-apex-exp1476-linux-x86_64.tar.gz -C /opt/frankenmoe
cd /opt/frankenmoe && sha256sum -c SHA256SUMS
LD_LIBRARY_PATH=$PWD ./llama-server --model Apex-35B-V1.0-Q4_K_M.gguf \
  --host 127.0.0.1 --port 9101 -c 262144 -b 4096 -ub 4096 -ngl all -fit off \
  --cpu-moe --moe-cache-mib 4992 --cache-type-k q4_0 --cache-type-v q4_0
```

Expect roughly 62–63 tok/s decode and ~2900 tok/s prefill at 32K input / 512 output on an RTX 4070
with the model resident in page cache.

## Build

Prerequisites: Linux x86_64, GCC 13, CMake 3.24+, Ninja, CUDA toolkit, and the pinned Goose
compiler recorded in `runtime/compiler/current.json`.

```
scripts/prepare_upstreams.sh        # pinned upstream checkouts, see UPSTREAMS.md
bash tools/build-main-runtime.sh    # compose and typecheck the Goose unit
bash tools/check.sh fast            # the gate CI runs on every pull request
```

CI pins the public Goose `v2026.4` release; its SHA256 is in
[the workflow](.github/workflows/ci.yml).

## Measurement rules

1. Every result names the model, the runtime and the workload.
2. Correctness first: tokens and probabilities must match the accepted reference, and every claim
   states the answer hash.
3. Single-response decode is never presented as aggregate throughput.
4. A paired difference below 2% is not claimed as a change without repeated pairs and raw data.
5. Rejected experiments stay in the record with their numbers.
6. Apex measurements run only through `tools/apex-test.sh`, never two at a time.

## Repository map

| path | content |
|---|---|
| `goose/` | the Goose sources: modules, manifest, per-area notes |
| `tests/` | permanent tests, registry and the unified runner |
| `docs/goose-migration/` | ledger, status, coverage documents, compiler records |
| `experiments/` | one directory per slice: hypothesis, source, gate, results |
| `runtime/` | runtime build scripts and baselines |
| `tools/` | build, gate, audit and measurement entry points |
| `patches/`, `benchmarks/`, `results/` | integration patches and measurement records |

## Attribution and license

The runtime derives from `llama.cpp` and keeps upstream attribution and licenses. The Apex, Qwen
and Flint models are distributed under their own licenses. This repository's own work is
Apache-2.0; see [`LICENSE`](LICENSE).

- [Migration ledger](docs/goose-migration/tasks.json) · [Goose sources](goose/README.md) ·
  [Pinned upstreams](UPSTREAMS.md) · [Experiment journal](experiments/EXPERIMENTS.md)
