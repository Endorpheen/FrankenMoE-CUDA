# P10-12 — complete source/entrypoint inventory: residual native logic

Date: 2026-11-15. Base commit: `aa9109b5` (`tools: add http-url policy, the server
parser copy and the route table`). Work done outside the repository, in
`/tmp/opencode/sub-648/`; the shared tree, the production runtime and other
agents' files are untouched. No model, GPU, server or benchmark was started.

This report answers one question for P10-12: **is there any owned
implementation path or public entrypoint whose logic is still native and not
accounted for by an accepted Goose slice or an audited primitive?** It is an
audit first; the three slices in `README.md` are the part of the answer that
could be closed immediately without a model.

## 1. Method and scope

Three sources were read and cross-checked, not one:

1. **The ledger** — `docs/goose-migration/tasks.json`: 152 requirements,
   112 `DONE`, 10 `PARTIAL`, 30 `TODO`; `work_front` says 127/127 slices done.
2. **The canon** — `goose/MANIFEST.json`: 4226 entries = 891 canonical modules
   (797 slice, 94 ported) from 425 experiments + 3335 archived sources. Each
   ported entry names its experiment, native target, gate and SHA-256.
3. **The bytes** — every file under the owned roots, walked and matched against
   the MANIFEST native targets: `runtime/` (94 files, 40 478 lines), `tools/`
   (30, 3 846), `scripts/` (24, 2 649), `benchmarks/` (56, 288 218 — almost all
   recorded measurements), `editors/` (11, 1 836), `patches/` (22, 18 505).
   237 files total.

The native tree the migration ports from (`/home/end0/AI/LlamaServer/llama.cpp-flint-cuda`)
was used only as the oracle for this experiment's gates; nothing in it was
modified.

A file is counted as **covered** only when a canonical module names it (or its
directory and decision) as the native target and states a gate. A file that
merely has a module with a similar name is not covered.

## 2. Inventory by category

| category | count | content | verdict |
|---|---:|---|---|
| **A. Already Goose** | 891 modules | `goose/` outside `_archive/`: jinja, json, grammar, server, cpu, cuda, kv, model, sampling, metrics, chat, cli, misc | the ported canon |
| **B. Native, decisions ported by an accepted slice** | 15 of 19 carrier `.cpp` | `config`, `chat_parse`, `ngram_draft`, `runtime`, `thinking_control`, three metric sinks, `arch_registry`, `dense_weights`, `deterministic_workload`, `expert_stream_source`, `router_hook`, `row_stream`, `gguf_offsets` (naming) | host glue only |
| **C. Native, uncovered before this experiment** | 3 carrier `.cpp` | `io/file_reader.cpp`, `io/platform_io.cpp`, `moe/gguf_offsets.cpp` | **sliced here** (`fr_`, `pr_`, `sn_`) |
| **D. Native, uncovered after this experiment** | 2 carrier files, 7 regions | `engine/session.cpp` (5 regions), `cli/main.cpp` (2 regions) | see §4, gaps G1-G7 |
| **E. Substrate by nature** | 3 carrier units | syscalls (`open/pread/fstat/mincore/mmap/madvise/posix_memalign`), the gguf container walk, the llama.cpp graph/sampler API calls | audited primitive, stays native |
| **F. Owned code that is not production runtime** | 98 files | `runtime/tests` 8, `runtime/reconstructed` 33, `tools/` 30, `scripts/` 24, `benchmarks/` 3 C/py fixtures | P10-05/P10-09, not a runtime path |
| **G. Declarative or recorded artefacts** | 91 files | `editors/` grammar 11, `patches/` 22, `benchmarks/` data 53, `runtime/` data 5 | no logic to port |

Header files (11 under `runtime/core/include`, 11 under `runtime/core/src`)
carry declarations and `constexpr` bounds; P10-08 allows them as declaration
wrappers. The constants that a port needs are repeated inside the module and
pinned by building the gate against the real header (`config-validate.goose`
already does this).

## 3. Entrypoint audit

| # | entrypoint | where | ported part | residual |
|---|---|---|---|---|
| E1 | `main()` | `runtime/cli/main.cpp:590` | flags/env/defaults (`cc_`, EXP620), session line protocol (`cli_`, EXP572), `BMOE_*` emitters and the reader (`cli_`) | `print_predict_report` (pure, uncovered), the bench driver and `--ppl` path, the reader thread/queue plumbing, usage text, Windows console detection |
| E2 | engine init | `Session::open` | admission, mparams/cparams projection, the 20-stage init order (`lga_`, EXP636), lifecycle and teardown (`sl_`, EXP632) | the `llama_*` calls themselves — substrate |
| E3 | engine run | `Session::generate`, `run()` | budget/interrupt (`cls_`), KV turn state (`kvp_`), draft source (`ds_`), spec-decode decisions (`sd_`), runtime facade (`rts_`) | `GenTally`/`PrefillTally`, the prefill chunk plan, trace framing order, MTP warm-up, sampler-chain construction |
| E4 | server startup | native `tools/server/main.cpp` + `server.cpp` | startup, shutdown, prerouting, route table, request errors, required fields, multipart, stream policy/manager, SSE framing, GCP props, bind params, response, limits (EXP445-471) | not a file in this repository; the server binary is built from the native tree |
| E5 | tool mains | native `tools/*` | llama-bench, tokenize, fit-params (EXP638), gguf-split (EXP609), server-side decisions | completion, perplexity, export-lora, cvector-generator, tuning, quantize, imatrix, mtmd — all need a model; recorded in P10-02 `remaining` |
| E6 | test mains | `runtime/tests/*_test.cpp` (8) | — | P10-09: diagnostics, not a production path |
| E7 | script/CLI entrypoints | `tools/*.py` (6), `scripts/*.py` (10), `scripts/*.sh` (8) | — | P10-05: build, verification and orchestration; the composer/sync/check pair is the build system, not runtime logic |

No entrypoint contains an unaccounted second copy of a ported decision: the
portable parts of E1, E2, E3, E5 are named in the MANIFEST, and the rest is
either substrate or listed as a gap below.

## 4. Residual native code, with paths and an honest verdict

| id | path | lines | verdict |
|---|---|---:|---|
| R1 | `runtime/core/src/engine/session.cpp` — `GenTally::record`, `PrefillTally` (157-285) | ~130 | **clean logic**: pure arithmetic over counters and deltas; the metric columns are already `mt_`'s |
| R2 | `session.cpp` — prefill chunk plan (1565-1591) | ~27 | **clean logic**: index arithmetic and batch selection |
| R3 | `session.cpp` — trace framing order (1480-1530) | ~50 | ordered side effects; portable as a plan, needs the sinks |
| R4 | `session.cpp` — MTP capture warm-up (988-1150) | ~160 | ordered side effects over the vocabulary; rules already owned by `rh_` |
| R5 | `session.cpp` — sampler chain (937-951) | ~15 | **needs the llama API**: four `llama_sampler_chain_add` calls |
| R6 | `runtime/cli/main.cpp` — `print_predict_report` (540-588) | ~49 | **clean logic**: a fixed-format report, no model |
| R7 | `runtime/cli/main.cpp` — driver loop, bench driver, `--ppl` path | ~200 | mixed: the queue/threads are substrate, the `--ppl` argument projection is portable |
| R8 | `runtime/core/src/io/file_reader.cpp` | 195 | **sliced here** (`fr_`); the preads stay native |
| R9 | `runtime/core/src/io/platform_io.cpp` | 461 | **sliced here** (`pr_`); the syscalls stay native |
| R10 | `runtime/core/src/moe/gguf_offsets.cpp` | 167 | **naming sliced here** (`sn_`); the gguf walk stays native |
| R11 | `runtime/reconstructed/src/*` | ~16 000 | copies of native source kept as the reconstruction oracle, not built into the runtime |
| R12 | `runtime/tests/*` | 1 994 | original C++ diagnostics; P10-09 allows them outside the artifact |

R1-R4 and R6 are the only residual regions that are simultaneously native,
uncovered and model-free. That is the whole of the finding: the carrier's
remaining logic is either those seven regions or a syscall/API primitive.

## 5. Gaps that need modules

| gap | content | owner |
|---|---|---|
| G1 | `GenTally`/`PrefillTally` accounting | P9-08 remainder 1 |
| G2 | prefill chunk plan | P9-08 remainder 2 |
| G3 | trace framing order | P9-08 remainder 3 |
| G4 | MTP capture warm-up order | P9-08 remainder 4 |
| G5 | sampler chain construction (API) | P9-08 remainder 5 — substrate |
| G6 | `moe-predict` report | P10-12 / E1 |
| G7 | CLI driver loop beyond the line protocol | P9-08 remainder 6 |
| G8 | gguf container walk in `gguf_offsets.cpp` | audited primitive (container library) |
| G9 | test mains and fixtures | P10-09 (TODO) |
| G10 | build/verify tooling in `tools/` | P10-05 (TODO) |
| G11 | orchestration scripts in `scripts/`, including `make-tiny-moe.py`'s per-architecture tensor layout for fixtures (the container itself comes from the external `gguf` package) | P10-05 / P10-09 / P10-07 |
| G12 | reconstruction harness | proof artefact, not on the runtime path |
| G13 | model-bound tool mains | P10-02 `remaining` |
| G14 | vendored json/jinja/http helpers | P10-07 (TODO) |
| G15 | byte-identical `libggml-base` reconstruction | P10-06/P10-11 `remaining` |

Closed by this experiment: the three uncovered carrier files (C). G8, G12, G15
are deliberately not modules; the rest are owned by their requirement.

## 6. What the three slices close

`README.md` in this directory documents the gates:

```
FG648_FR cases=105 values=486 failures=0     file_reader.cpp decisions
FG648_PR cases=132 values=417 failures=0     platform_io.cpp pseudo-file readers
FG648_SN cases=  97 values=145 failures=0     gguf_offsets.cpp shard naming
```

322 cases / 1048 compared values, identical at `-O1` and `-O0`; 36/36 mutations
caught in two halves of 18. The native arm is the base-commit translation unit
itself (linked for `fr`, included for `pr` and `sn`), so the comparison is
against the real code, not a retelling — except the four sites that have no
callable observer (the verify branch, the bounce test, the shard tail grammar,
the chunking), each marked as a transcription at the site.

After the slices, `file_reader.cpp` keeps its preads and O_DIRECT probe,
`platform_io.cpp` keeps its syscalls, and `gguf_offsets.cpp` keeps its gguf
walk: what moved to Goose is the decision of *where* to read, *what* the
pseudo-file text means, and *which* shard files to open.

## 7. Recommendation: the next two or three slices

1. **`mt_`-adjacent tallies — `GenTally::record` + `PrefillTally` (G1).**
   The largest clean block left in the carrier: pure arithmetic, feeds columns
   that `metrics-traces.goose` already formats, and the failure mode (a wrong
   delta) is visible in one number. Gate: per-token and per-prefill counter
   tables against the native struct; 12 mutations are easy (order of
   accumulate/reset, division-by-zero guard, the compute residual).
2. **`moe-predict` report (G6).** A pure formatting decision at a public
   entrypoint, three tables plus the `-` rule for a layer with no routings.
   Gate: composed lines against the native `printf` sequence; the one risk is
   `%.1f`, which needs the same fixed-point helper `cli_fixed` uses or an
   extern shim like `lb_fmt2`.
3. **Prefill chunk plan + trace framing order (G2, G3).** Small, ordered, and
   they finish the prefill half of `session.cpp`; the framing gate is an
   ordered event list, which the existing trace fixtures already model.

G4 (MTP warm-up) is larger and touches the vocabulary; it should follow the
sampler decision, and G5 is an API call that belongs in substrate.

## 8. Limitations

- Coverage is asserted from the MANIFEST plus a read of each carrier file;
  it is not a per-function proof. A name match is not execution: the module
  may implement the decision and the host may still take another branch. This
  is the same limitation `final-coverage.md` records for kernels.
- Slice coverage means "the decisions of this file are ported", not "the file
  is dead". Every carrier `.cpp` in category B still compiles into the
  production build until the integration step replaces its body with a call
  into the composed Goose unit.
- `runtime/reconstructed/` was classified, not audited line by line: it is a
  reconstruction harness whose output is compared against the deployed
  library, and it is not compiled into the runtime.
- The 237-file count covers the owned roots named in this task. `upstream/`
  and `work/` are not owned sources (vendored and user trees), and
  `experiments/` is evidence.
