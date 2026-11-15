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

Proven in `experiments/EXP-2026-11-15-627-s0-composed-artifact`: each accepted
set composes, generates C and builds a shared object; exported symbols are
scoped per set (`goose_init` -> `goose_init_<set>`, the duplicate `json_parse`
localised), the sets link into one `libgoose_unified.so`, and a smoke host loads
every set both by link and by `dlopen` (5/5, rc=0, no unresolved symbols). The
two duplicate groups before scoping were `goose_init` (5 sets) and `json_parse`
(server and P9-06); both are explained and resolved. The generated C embeds the
output path, so the artifact hash is reproducible at a fixed `OUT`.

The growing main build is `tools/build-main-runtime.sh`. It composes every
accepted module that shares one unit, typechecks, generates C, compiles an
object and fails on any duplicate global symbol. Current state: 47 modules,
2585 specializations, 16.7 MB of C, 3 s to typecheck and 13 s to compile,
518 global symbols, 0 duplicates, 97 extern helper entry points at -O1.

The build holds the model graph/loader/file-io/arch-registry, the six quantizer
modules plus weighted, the four GGUF modules, expert LRU, dense placement,
row-stream planning, vocab dispatch, special-token partition, common strings,
the Jinja runtime, the schema converter, config validate/resolve, CLI protocol,
metrics, spec-decode decisions, the six server modules (including the MCP stdio
integration) and the two chat modules. Per-architecture
model builders stay out by design: they are alternatives, not one unit.

It still cannot link because the native helper entry points are extern symbols
supplied by the per-slice test fixtures; wiring them to the runtime is S1 work.
This step touches no production file.

Determinism, measured on 2026-11-15: the composer gives byte-identical output
for the same module list, the C generator gives byte-identical output for the
same composed file, and two consecutive builds into the same output directory
give identical `main.c` and `main.o` hashes. Only the output path changes the
hash, because the generated C embeds it in `gs_file0`; a fixed `OUT` is therefore
required for byte-level comparison.

### Compatibility rule

The build grows only with modules that share one unit. A subsystem whose
canonical files are alternative ports of the same functions is not added as a
set; the build keeps one representative per such family. The composer enforces
this by refusing a set where two modules carry different bodies under one name,
and by failing the export-set check when a dedup would drop a public entry.

Checked on 2026-11-15, all against the current main build:

| candidate group | result |
|---|---|
| `goose/kv/*` (85) | 508 body conflicts (`put`, `get`, `fail`, ...) - alternative KV ports, not one unit |
| `goose/cuda/*` (34) | 168 body conflicts - alternative CUDA ports |
| `goose/cpu/*` (16) | 78 body conflicts (`ld_u8`, `st_i64`, ...) - alternative CPU ports |
| `goose/misc/*` (30) | 125 body conflicts (`put`, `get`, `fail`, ...) - alternative live-run ports |
| `goose/sampling/*` (5) | 5 body conflicts (`fail`, `run`, ...) - alternative sampler ports |
| `goose/json/*` (31) | alternative schema ports: `ac` vs `ac-grammar`, `format-grammar` vs `schema` vs `pattern`; one representative (`format-grammar`) is in the build |
| `goose/model/goose-*.goose` (13) | 55 body conflicts (`put`, `get`, `fail`, ...) - alternative batch/ubatch/loader live ports |
| `goose/server/*` (40) | 124 body conflicts (`put`, `get`, `fail`, `st_i64`, ...) - alternative server and live-run ports |
| `goose/jinja/tool-builders.goose` | alternative runtime copy (its own `K_*`, `bases`, `esc`; 14 body conflicts) - the tool builders are already inside the canonical runtime from EXP574 |

Integrating an alternative is a merge decision (pick a body, delete the other),
not a composition step; it is out of scope until a stage needs that subsystem.

### S1.2-S2.1 — in flight (isolated staged builds)

Four staged builds run in parallel on the same pattern as S1.1, each behind its
own build flag, each in its own copy of the CPU-only tree:

| stream | flag | module | gate |
|---|---|---|---|
| GGUF write/plan | `FG_GOOSE_GGUF` | gguf-metadata/writer/tensors/split | byte parity on synthetic inputs |
| quantizers | `FG_GOOSE_QUANT` | quant-types/allow/rows/rows2/dequant/dequant2/weighted | byte parity of quant/dequant blocks |
| config validation (done, EXP630) | `FG_GOOSE_CONFIG` | config-validate (+ arch-registry, config-resolve) | 48 rules and 6 order pins: 72/72 both paths, 12/12 mutations |
| metrics/traces (done, EXP628) | `FG_GOOSE_METRICS` | metrics-traces | byte parity of CSV fields and trace rows: 53/53 both paths, 4/4 mutations |

Each must show: OFF build unchanged, ON build dispatches into Goose
(`goose_calls > 0`), exactly one definition of the Goose symbol in ON and none
in OFF, and no model run.

### S1.1 — proven on the schema converter (EXP626, isolated)

The staged build is reproduced in
`experiments/EXP-2026-11-15-626-s1-1-apex-staging`: a CPU-only copy of the
runtime tree, the Goose schema converter generated from
`goose/json/format-grammar.goose`, and a build flag `FG_GOOSE_JSON_SCHEMA` that
selects the Goose path while the native one stays compiled.

| check | result |
|---|---|
| base build (pristine llama.cpp, CPU only) | built, 74 s |
| staged build, flag OFF | built, 57 s |
| staged build, flag ON | built, 57 s |
| smoke gate, 30 schemas / 182 rules | OFF 30/30, ON 30/30, 0 failures |
| Goose actually dispatched | `goose_calls=30` on ON, `0` on OFF |
| grammar text base vs OFF vs ON | identical (242 lines) |
| `json_schema_convert` definitions | exactly 1 in the ON library, 0 in OFF |

The switch is therefore a build flag, not a runtime branch; the off path is the
production one. The remaining seam is the host bridge that walks a JSON document
into the flat node table the converter takes (`$ref`, `additionalProperties`,
exclusive bounds); it is still native and is the next S1.1 step. No model was
run and no production file was touched.

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

## Accepted module streams (2026-11-15)

| stream | requirement | stage | experiment | state |
|---|---|---|---|---|
| config/recipe/registry | P9-01 | S2 | EXP620 | accepted, 3/3 gates; `validate()` wired in EXP630 |
| dense/expert streaming | P9-03 | S1 | EXP621 | accepted, 74/198, call sites native |
| metrics/traces/workload | P9-07 | S1 | EXP618 | accepted, 106/203; field/row encoders wired in EXP628 |
| meta backend/threading | P4-12 | S1 | EXP617 | accepted, 67/67 |
| ggml-opt optimizer | P4-12 | S1 | EXP624 | accepted, 132/132; P4-12 DONE 4/4 |
| MTP/speculative draft | P6-11 | S2 | EXP619 | accepted, 9828/0, engine wiring open |
| loader/tokenizer/common | P7-12 | S1 | EXP622 | accepted, 3/3 gates, wiring open |
| MCP stdio integration | P8-07 S5b | S2 | EXP623 | accepted, 26/243; P8-07 DONE 6/6 |
| hoisted native index | P7-08 | S3 | EXP625 | accepted, gbnf 13/13, parse 9/9; P7-08 DONE 9/9 |
| router hooks/overlap | P9-04 | S1 | EXP629 | accepted, 1007 checks, on_eval wiring open |
| model manager/reload/connectors | P8-06 | S3 | EXP631 | accepted, 49 cases / 521 values, wiring open |
| sessions/runtime/draft source | P9-05 | S3 | EXP632 | accepted, 332 checks, wiring open |
| signed-zero dequantizer fix | P7-04 | S1 | EXP633 | accepted, bit-exact 300/300, 12/12 + 3/3 |
| GGUF write/plan | S1.2 | S1 | in flight | OFF/ON build proven, mutation set pending |
| quantizers | S1.3 | S1 | in flight | OFF/ON build proven, mutation set pending |
| CLI tools | P10-02 | S4 | in flight | inventory-first |
| standalone engine | P9-08 | S3 | in flight | inventory-first |

The composed artifact (S0, EXP627), the S1.1 schema switch (EXP626), the S1.4
metrics switch (EXP628) and the S2.1 config switch (EXP630) are accepted. The
main runtime build composes every accepted module that shares one unit.

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

1. S0 is accepted (EXP627): the sets link into one artifact and every one loads.
2. S1.1 is accepted (EXP626): the schema converter runs behind a build flag with
   identical grammar text and a native fallback.
3. S1.4 is accepted (EXP628): metrics encoders behind a flag with byte parity.
4. In flight: GGUF (S1.2), quantizers (S1.3), config validation (S2.1), model
   management (P8-06) and router/overlap (P9-04).
5. Ask Igor for the first model A/B with the exact plan (goal, duration, number
   of runs) before any server is started.

## Remaining work map

The slice front is complete (112/112). What is left is integration and the
requirements that carry no slices yet:

| requirement | stage | what it needs |
|---|---|---|
| P6-11 | S2 | wire `ngram_draft.cpp` and `session.cpp` to the sdp_ decisions |
| P7-12 | S1 | wire vocab dispatch, special partition and common strings |
| P9-01 | S2 | wire `validate()`, `arch_registry.cpp` and `main.cpp` |
| P9-03 | S1 | replace the 25 streaming call sites |
| P9-07 | S2 | wire the three sinks and `DeterministicWorkload` |
| P8-06 | S3 | model management, connectors and hot reload (accepted, EXP631) |
| P9-04 | S1 | router hooks and I/O-compute overlap (in flight) |
| P8-09 | S3 | web/UI serving and application integration |
| P9-05 | S3 | standalone sessions/runtime/ngram draft (accepted, EXP632) |
| P9-08 | S3 | standalone engine closure |
| P4-13, P1-12 | S1 | allocation/scheduler unification and CPU MoE closure |
| P10-* | S4 | build integration, packaging, headers/FFI, tools, licensing |
| P11-*, C1-*, C2-*, F1-* | S4+ | Apex integration, Qwen extension and final qualification |

P10-01 (consolidation) is already true in practice: every accepted slice is in
`goose/`, `MANIFEST.json` covers all 811 files outside `_archive`, and
`check-goose-tree.py` is green; it needs a criterion decision, not more work.
