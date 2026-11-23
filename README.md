# FrankenMoE-CUDA

FrankenMoE runs Mixture-of-Experts models that exceed a single GPU's memory.
The standalone runtime uses Goose for model execution and C adapters for the
operating system and CUDA driver. The reference model is Apex-35B-V1.0 Q4_K_M
on a Ryzen 9 5950X with 64 GB RAM and an RTX 4070 12 GB.

Learn about the language in the [original Goose repository](https://github.com/aardappel/goose).

## Run the Linux chat server

In a Linux chat package built from this tree, run the server from the extracted
directory. Python 3 enables the **Goose on Fire** terminal dashboard by default
in an interactive terminal. The launcher discovers the bundled web assets.

```bash
./frankenmoe-server --ctx-size 262144 --batch-size 4096 --ubatch-size 4096 \
  --cache-type-k q4_0 --cache-type-v q4_0 --expert-cache-mib 6290 \
  --threads 8 --threads-batch 16 --cpu-mask 0f0f --cpu-mask-batch ffff \
  --cpu-strict 1 --cpu-strict-batch 1 /path/to/Apex-35B-V1.0-Q4_K_M.gguf --serve 9101
```

Open the web UI at `http://127.0.0.1:9101/` or set the OpenCode provider's
OpenAI-compatible base URL to `http://127.0.0.1:9101/v1`. Use
`FRANKENMOE_CONSOLE=0` for plain server output and `FRANKENMOE_LOG=/path/to/log`
to capture raw logs from the dashboard. Adjust CPU masks and cache capacity to
fit your machine.

[v0.12.1-chat-alpha](https://github.com/Endorpheen/FrankenMoE-CUDA/releases/tag/v0.12.1-chat-alpha)
packages the OpenCode chat continuation fix for Linux x86-64 and Windows x64.
Linux uses the cumulative EXP-1808 host. On Windows, use `start.bat` from
the ZIP; the CUDA host follows the previous EXP-1805 Windows release.

![Goose on Fire terminal dashboard with Apex startup logs](docs/assets/goose-on-fire.png)

## Apex checkpoint

The cumulative Linux build adds EXP-1806's early decode submission and
[EXP-1808's asynchronous prefill job packs](experiments/EXP-2026-11-23-1808-prefill-jobpack-async/README.md)
to [EXP-1805's expert route prefetch](experiments/EXP-2026-11-23-1805-route-prefetch/README.md).
The saved standalone checkpoint `37ac0f1d…` recorded **3122.4 tok/s prefill**
and **78.225 tok/s decode** on the Apex 32K/512 workload with a 262144-token
context and 6290 MiB expert cache. The separately measured **81.292 tok/s**
result belongs to the [EXP-1806 candidate](experiments/EXP-2026-11-23-1806-decode-region-split/README.md).
These are single-response results from the archived binaries. See the
[build inputs and provenance](integration/goose-standalone/apex-checkpoint/README.md).

## Build

See the [build and packaging guide](docs/BUILD.md) for the pinned Goose toolchain,
checkpoint inputs and Linux and Windows packaging paths. The [current state](docs/CURRENT_STATE.md)
tracks the published release, cumulative Linux sources and platform coverage.

## Checks

The [model acceptance registry](docs/goose-migration/model-architectures.json)
tracks which architectures have been validated against a reference. The
[Goose tree check](tools/check-goose-tree.py) verifies source provenance.
`bash tools/check.sh fast` composes and compiles the Goose unit without model
weights. Historical tests and experiments retain the context of their original
runtime and may refer to upstream sources from older revisions.

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
| [`integration/`](integration/README.md) | standalone host and C adapters, with older integration fixtures indexed separately |
| `runtime/` | compiler records and historical runtime checkpoints |
| `tools/` | build, gate, audit and measurement entry points |
| `patches/`, `benchmarks/`, `results/` | integration patches and measurement records |

## Attribution and license

FrankenMoE is licensed under Apache-2.0. Upstream components and model weights
retain their respective licenses. See [`LICENSE`](LICENSE).

- [Roadmap](ROADMAP.md) · [Goose sources](goose/README.md) ·
  [Experiment journal](experiments/EXPERIMENTS.md) · [Migration archive](docs/goose-migration/INDEX.md)
