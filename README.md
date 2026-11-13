# FrankenMoE-CUDA

FrankenMoE-CUDA runs Mixture-of-Experts models that do not fit in GPU memory on
one consumer GPU. The runtime is derived from `llama.cpp` and is being migrated,
module by module, to **Goose**, the project's own language. The reference model
is **Apex-35B-V1.0 Q4_K_M** on a Ryzen 9 5950X, 64 GB RAM and an RTX 4070 12 GB.

The current state and the open work are in
[`docs/goose-migration/STATUS.md`](docs/goose-migration/STATUS.md).

## Apex today — 2026-11-13

Canonical configuration, fixed profile, one request:

| item | value |
|---|---|
| binary | `runtime/reconstructed/build/stage-run/llama-server` |
| model | `Apex-35B-V1.0-Q4_K_M.gguf` |
| placement | `--cpu-moe`, `-ngl all`, flash attention on |
| GPU expert cache | `--moe-cache-mib 3584` |
| KV cache | `q4_0`, context `262144` |
| batch | `-b 4096 -ub 4096`, `-np 1` |
| threads | `-t 8 -tb 16` |
| workload | fixed 32768-token prompt |

Measured on that profile:

| metric | value | source |
|---|---:|---|
| prefill | **2228.76 tok/s** | `docs/goose-migration/APEX-BASELINE-20261113.md` |
| decode, 128 tokens | **41.62 tok/s** | same |
| decode, 512 tokens (unified logging build) | **43.04 tok/s** | same |
| prefill, 512 tokens (unified logging build) | 2244.80 tok/s | same |
| without the GPU expert cache | 29–33 tok/s | same |

The 128-token run produced content hash
`9d90a010775f2e72fbea3fec7e0d34b8e57b52aab9dd16e90eb9cd5151ba7560` with server
exit 0. The GPU cache selects a different execution path than the historical
`e68df05b...` reference, so those two hashes are not comparable; the answer
remains coherent, which is why the cache run has its own baseline.

These are single-request decode numbers. Aggregate throughput across slots is a
separate metric and is not claimed here.

## Goose migration

All 4061 Goose sources are in one tree, grouped by subsystem:

| directory | files | subsystem |
|---|---:|---|
| `goose/jinja/` | 8 | Jinja lexer, parser, value model, JSON output, built-ins, runtime |
| `goose/json/` | 33 | JSON parsing, rendering, JSON-schema |
| `goose/grammar/` | 1 | GBNF grammar parsing |
| `goose/server/` | 44 | HTTP routing, limits, SSE, providers, lifecycle |
| `goose/cpu/` | 239 | CPU backends, quantized kernels, MoE, allocator |
| `goose/cuda/` | 233 | CUDA operators |
| `goose/kv/` | 90 | KV cache, memory, batch, context |
| `goose/model/` | 29 | model loading, GGUF, tokenizer |
| `goose/sampling/` | 5 | sampler decisions |
| `goose/chat/` | 14 | chat templates and tool calls |
| `goose/misc/` | 33 | slices with no named native file |

A complete module keeps the name of the native file it replaces
(`goose/jinja/lexer.goose` for `common/jinja/lexer.cpp`); a partial slice is
named after the decision it implements (`goose/server/route-table.goose`).
`goose/MANIFEST.json` records the experiment, native path, gate result and
SHA256 of every file. `tools/sync-goose-tree.py` rebuilds the tree and
`tools/check-goose-tree.py` verifies it (4061 entries, green).

The C++ transport is still present: the Goose modules are compiled to `.so` and
linked into the llama.cpp-derived runtime. That is a hybrid by design until the
remaining requirements close.

## Repository map

| path | content |
|---|---|
| `goose/` | the Goose sources (start here) |
| `docs/goose-migration/` | migration ledger, status, per-requirement coverage and closures |
| `experiments/` | one directory per slice: source, differential fixture, build, mutations |
| `runtime/` | local runtime artefacts and baselines |
| `scripts/` | launch, fit and maintenance scripts |
| `editors/goose-vscode/` | VS Code extension, TextMate grammar and the Linguist package |
| `patches/`, `benchmarks/`, `results/` | integration patches and measurement records |

## Measurement rules

1. Performance is measured only on the canonical Apex profile above; a different
   binary, model or flag set is a different experiment.
2. Correctness comes first: tokens and probabilities must match the accepted
   reference, and every claim states the content hash.
3. Single-response decode is never presented as aggregate throughput.
4. A paired comparison below 2% is not claimed as a change without repeated
   pairs and the raw data.
5. Rejected experiments stay in the record with their numbers.

## Run the canonical profile

The binary is built from the pinned upstream revision
([`UPSTREAMS.md`](UPSTREAMS.md)); the exact argv is preserved in
`runtime/exp244/results-apex-or244-or-goose/argv.json` plus
`--moe-cache-mib 3584`. The server listens on the loopback interface by default.

## Status and next step

The migration ledger stands at **DONE 102, PARTIAL 1 (P7-08), TODO 49** of 152
functional requirements, with **34 of 38 slices** done. P7-08 has four of eight
slices: lexer, parser, value model and JSON output are accepted; built-ins and
the runtime are partially ported; `for`/`break`/`continue` and the loop object
are the slice in progress (EXP509), followed by macro/call, filters and tests,
string/caps and the chat layer.

The next functional goal is full Edge0-120B pack support in this runtime
(`docs/goose-migration/EDGE0-120B-ADAPTATION.md`, POST-08); it is not implemented
yet and no load or performance claim is made.

## Attribution and license

The runtime derives from `llama.cpp` and keeps upstream attribution and licenses.
The Apex, Qwen and Flint models are distributed under their own licenses. This
repository's own work is Apache-2.0; see [`LICENSE`](LICENSE).

- [Current status](docs/goose-migration/STATUS.md)
- [Migration ledger](docs/goose-migration/tasks.json)
- [Goose sources](goose/README.md)
- [Pinned upstreams](UPSTREAMS.md)
- [Experiment journal](experiments/EXPERIMENTS.md)
