# Runtime integration plan — composed Goose modules into the GPU runtime

Updated: 2026-11-15. Direction from Igor: merge the accepted canonical Goose
modules into the full GPU runtime step by step, keep the working small Apex path
(correctness and performance) as the reference, verify on real models with the
GPU, and never block or rewrite the migration.

## Invariants

1. The working Apex binary stays the reference and the fallback until a stage is
   accepted: `runtime/reconstructed/build/stage-run/llama-server`, Apex
   35B-A3B Q4_K_M, `--cpu-moe -ngl all -fa on`, KV `q4_0`, `-c 262144`,
   `-b 4096 -ub 4096`, `-t 8 -tb 16`, `--moe-cache-mib 3584`.
2. Baseline to preserve (2026-11-13): prefill 2228.76 tok/s, decode 41.62 tok/s,
   128 predicted tokens, content sha `9d90a010…`, exit 0. Without the GPU expert
   cache the same binary decodes at 29–33 tok/s.
3. A stage is accepted only with: its unit/differential gate green, identical
   model output for the same request on the same flags, and prefill/decode not
   worse than the baseline beyond measurement noise; memory (VRAM/RAM) recorded.
4. Every stage is behind a build-time switch. The previous artifact and the
   switch-off path stay available for rollback.
5. Each stage is one commit; no stage is promoted to the working runtime
   automatically.
6. Model runs follow the standing rule: state goal, duration and number of runs,
   and start only after Igor's explicit yes.

## Stages

### S0 — composed artifact (safe, no runtime change)

Compose the accepted sets with `tools/compose-goose.py`, build each into a
shared object at `-O2 -fPIC`, link a smoke host, and prove there are no duplicate
symbols and that every set loads. Sets today: server (6 modules), quantizer (6),
GGUF (4), P9-06 (3), runtime+schema (2), app (9) and core (5).

The full unit (core + app, 14 modules) is the first end-to-end check: 1678
specializations, 35 164 lines, 14.1 MB of generated C, compiles at -O0 in 13 s
into a 6.8 MB object with 201 global symbols and no duplicate definitions. It
still cannot link because the native helper entry points are extern symbols
supplied by the per-slice test fixtures; wiring them to the runtime is S1 work.
This step touches no production file.

### S1 — leaf modules with a native oracle

Swap one leaf at a time into a copy of the runtime build, starting where the
Goose module already has a differential gate and a small surface:

1. JSON escaping and the schema-to-GBNF converter (chat JSON validity; already
   fixed and gated in EXP615, converter in EXP570).
2. GGUF metadata/writer/tensor/split (EXP571/596/601/609) — used by load/save
   paths, but not on the decode hot path.
3. Quantization type policy and allow-cuts (EXP581/576); row quantizers and
   dequant (EXP603/611/607/613) follow once the type table is shared.

Each swap keeps the C++ fallback compiled and selects the Goose symbol through a
build flag; the unit gates run unchanged.

### S2 — server and CLI decisions

MCP core/stdio, request schema, decision core, CLI line protocol and the session
loop (EXP572/573/575/595/605/610/612). These are decision layers, so a change is
visible in request handling before it reaches the decode loop.

### S3 — chat and Jinja

`chat apply` and the Jinja runtime remain the largest block: template selection,
per-format handlers, caps branches and the `common_chat_params` assembly. The
runtime/value copies are layout-specific variants, so this stage is a merge, not
a rename. It starts only after the composition of those copies is designed.

### S4 — full runtime

All of the above selected by default, the C++ implementations retained one
release as the fallback, and the Apex baseline re-measured on the same flags.

## In-flight module streams (2026-11-15)

| stream | requirement | expected stage | what it feeds |
|---|---|---|---|
| MCP stdio integration | P8-07 S5b | S2 | server request path with a real process transport |
| hoisted native index | P7-08 | S3 | chat grammar text parity (not a runtime link) |
| composed artifact | S0 | S0 | duplicate-symbol and load proof for every set |
| config/recipe/registry | P9-01 | S2 | startup, flags and architecture selection |
| dense/expert streaming | P9-03 | S1 | data path: LRU, placement, prefetch planning |
| metrics/traces/workload | P9-07 | S1 | instrumentation and replay, no hot-path change |
| meta backend/threading | P4-12 | S1 | device and thread policy decisions |
| MTP/speculative draft | P6-11 | S2 | decode speculation and rollback decisions |
| loader/tokenizer/common | P7-12 | S1 | tokenizer registry, special tokens, common helpers |

Each stream produces standalone modules with their own gates and an explicit
integration step; the lead accepts them into the canon before any stage uses
them.

## Verification protocol per stage

- Correctness: same request (`request.json`), same flags, identical output hash
  or a documented intentional difference; unit/differential gates green.
- Performance: 1 warmup + 5 measured runs per arm, alternating A/B (baseline vs
  stage), same model, same GPU (RTX 4070 12 GB), same flags; report medians and
  ranges for prefill and decode, plus VRAM/RAM.
- Acceptance: no regression above 2% beyond the observed spread; any regression
  larger than the spread is investigated before the stage is kept.
- Rollback: the stage switch off returns to the previous artifact; the Apex
  binary is never overwritten in place.

## Immediate next actions

1. Accept the composed artifact from S0 (no model run).
2. Prepare the S1.1 build switch (JSON escaper + schema converter) and its unit
   gates.
3. Ask Igor for the first model A/B with the exact plan (goal, duration, number
   of runs) before any server is started.
