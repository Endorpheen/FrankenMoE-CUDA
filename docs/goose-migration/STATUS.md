# FrankenMoE migration status

Updated: 2026-11-20. This file is the current state; update it in the same change
as every accepted slice.

## C++ removal checkout

The full `deletecplusplus` checkout reached zero C++/CUDA sources, C++-bearing headers,
mixed-language sources and detected C++ source templates at `5ee89b4b`. The committed
baseline contained 1,062,464 C++/CUDA lines including source templates. The worktree
uses a complete checkout and the counter refuses a sparse index on this branch.

`tools/build-standalone-runtime.sh` now links `frankenmoe-tokenize` and
`frankenmoe-serve` from one composed Goose unit and the C system adapters.
Both binaries depend only on `libc` and `libm`. The tokenizer read the Apex GGUF and
returned `[760, 6511, 314, 9338, 369]` for `The capital of France is` with BOS disabled.
The standalone listener returned HTTP 200 with body `ok` on `GET /health`.

`tools/build-goose-tensor-core.sh` builds the C tensor and quantization archive.
Only the C declarations and tables from the former mixed header were retained.
A Q4_K/F32 tensor fixture passed, and the existing Goose CPU arithmetic executed
an ADD followed by MUL_MAT with the exact result `[5, 11, 10, 22]`. The numerical
fixture linked only `libc` and `libm`. The standalone inference host uses the
Goose GGUF reader and model owner for tensor binding, KV state and RoPE section
storage. A synthetic two-layer GGUF exercised attention and recurrent tensor
binding, state reset and a BPE round trip through the host. The Goose greedy
sampler is available through a public ABI. End-to-end graph execution,
generation and the Apex/Qwen/Maxwell/Edge0 performance gates remain open.

EXP-2026-11-20-1494 builds `libfranken-inference.a` from the composed runtime,
the Goose CPU operators, and the C tensor substrate through
`tools/build-goose-inference.sh`. The standalone host's `hose_step` calls the
Goose graph dispatcher. Independent checks covered GDN, masked attention on
Q4 KV rows, and Q4_K matrix multiplication against decoded weights. The
generated two-layer fixture passed two steps with reference logit 5.655950 and
generated tokens `2,2` (`abab`). The test executable linked only `libc` and
`libm`. The single approved Apex CPU smoke test loaded 40 layers and a
248,320-token vocabulary and timed out at the four-minute limit before
producing a token. The CPU reference executor is serial and currently reserves
storage for every graph tensor without lifetime reuse. Apex generation,
performance and memory comparisons remain unverified.

EXP-2026-11-20-1495 adds `frankenmoe-infer MODEL.gguf PROMPT [MAX_TOKENS]`
to the inference build. The same executable linked only `libc` and `libm`
and produced `abab` on the two-layer synthetic model, ` Paris` in a one-token
Apex Q4_K_M smoke test, and ` Paris, a` in a three-token Apex smoke test.
The latter completed one prefill and two decode graphs. These are functional
checks without a reference-model comparison or a performance measurement.
The serial CPU path decodes each weight row once per group of input vectors,
including repeated routes to the same expert. The broadcast Q4_K and indexed
Q6_K regressions match the decoded-weight reference.

EXP-2026-11-20-1496 adds `frankenmoe-infer MODEL.gguf --serve PORT [MAX_CONNECTIONS]`.
The loopback listener shares the model host with the CLI and exposes
`/health`, `/props` and `/completion`. The completion route accepts raw prompt
bytes or JSON with a string `prompt` and optional positive `n_predict`.
It uses greedy sampling and returns token IDs, decoded text and count.
The ten-request synthetic check covered fragmented input, invalid framing,
JSON validation and exact generation. The real Apex HTTP smoke test returned
`{"ids":[11751,11],"text":" Paris,","count":2}` after prefill and one decode
graph. The executable linked only `libc` and `libm`. OpenAI routes, streaming,
GPU execution, performance and peak memory remain open.

EXP-2026-11-20-1497 routes Q4_K and Q6_K matrix multiplication in the shared
CLI and HTTP graph to CUDA for supported F32 layouts. It reuses the Goose
decoders and plans active expert routes in Goose. OpenCL C binds device
intrinsics, and the C host adapter uses the CUDA Driver API. Device buffers
are reused. The host uploads one active weight plane at a time, and all other
operators continue on the existing CPU path. `GOOSE_CUDA=0` disables offload.

On the RTX 4070, the broadcast Q4_K and indexed Q6_K fixtures matched the
dequantized reference within the existing absolute tolerance of 0.00001 on
both CPU and CUDA. Apex Q4_K_M returned ` Paris,` through the CLI, with 431
CUDA matrices in prefill and 431 in the following decode graph. The same
binary returned HTTP 200 and token IDs `[11751,11]` with the same two CUDA
counts. The executable has only `libc` and `libm` as recorded ELF dependencies
and loads the system CUDA driver at runtime. The full GPU path, longer
generation, other models, performance and peak memory are still open.

EXP-2026-11-20-1498 reduces Apex recurrent-state overhead. The Goose executor
copies physically contiguous scalar tensors in a single byte move. It checks
the layout before dispatching the move, and retains strided copies. The GDN
kernel computes on F32 slices and evaluates decay once per head and token while
preserving the dot-product reduction order. Tests cover scalar and column
gates, strided queries, multiple sequences, and intermediate snapshots.

The CPU and CUDA fixtures and the ten-request HTTP gate passed. The Apex
Q4_K_M smoke test produced ` Paris, a` and recorded 431 CUDA matrices in each
of its three graphs. This records functional evidence. The 32,768-input,
512-output Apex performance and peak-memory gates remain open.

EXP-2026-11-20-1499 moves generation control into Goose for both CLI and
HTTP. Goose admits token IDs against the active session capacity, processes
prefill in chunks of at most 128 tokens, requests logits only for the last
prompt position, and controls decode and EOS. The model owner sizes the
persistent arena for the requested context. Use `--ctx-size 262144` before the
model path to select the Apex context used by the previous paired workload.
`GET /props` reports the actual context capacity, and `POST /completion`
accepts a flat array of token IDs as well as a string.

The 137-token nonzero synthetic fixture matched next-step logits for single
and chunked prefill to an absolute tolerance of 0.000001. The CPU and CUDA
operator checks and all 14 HTTP requests passed. The Apex Q4_K_M server
returned HTTP 200 with 248,320 vocabulary entries, 40 active layers, and
262,144 context slots. At readiness it reported 1,577,428 KiB RSS and zero
process swap. This was a readiness check without generation. The saved
32,768-input, 512-output request also requires seeded top-k/top-p sampling and
probability output. The current endpoint uses greedy sampling, and Q4 cache
attention still runs on CPU. The full Apex workload and paired gates are open.

EXP-2026-11-20-1500 connects the existing Goose MT19937, top-k, top-p, min-p
and temperature sampler to the shared generation implementation. The HTTP
route accepts the saved Apex request options, including explicit seed and
cache disabled. `ignore_eos` bans end-of-generation candidates when choosing
a token. Requested log probabilities use the full vocabulary before sampling,
and the response includes the token IDs and timings needed for comparison.
Bare completion requests retain their compact response shape.

All 23 HTTP checks passed. They cover distinct and repeatable seeded draws,
EOG masking, full-vocabulary log probabilities within 0.000001 of an
independent calculation, and parsing a 32,768-element token array before
rejecting its intentionally invalid last token. The CPU/CUDA operator
fixtures and the 137-token chunked-generation differential also passed.
This verifies the request and response path on fixtures. Full real-model
generation and throughput at 32,768 input and 512 output tokens remain open.

EXP-2026-11-20-1501 connects host residency and persistent CUDA weights to
the model owner. Goose prefaults the original read-only mapping and checks
each page with `mincore` before readiness. Inference admission checks the
same mapping and rewarms it if needed. This is a verified-residency policy
at admission. It does not lock the full file against Linux page reclamation.

Goose builds an immutable CUDA manifest for eligible Q4_K/Q6_K dense
tensors, reserves space for the model state and a streamed output matrix,
and stages the selected weights in one device allocation. CUDA matmul
consumes those device addresses directly. Other planes keep using the
shared host mapping. `GET /props` exposes exact mapped/resident host bytes
and pages, and the persistent dense weight bytes on CUDA. Static Apex
GGUF inspection found 311 eligible matrices totaling 1,277,337,600 bytes.
The expert-bank cache, GPU attention and recurrent compute, and the
full-system validation remain open. This checkpoint is for composition and
linkage. It has no new model run or performance claim.

EXP-2026-11-20-1502 connects the existing Goose SLRU to the same CUDA
matrix path. Layers are grouped by expert count and the byte sizes of the
up, gate and down planes. One device slot holds all three planes. The
indexed matrix dispatcher uses its selected expert IDs to prepare the
slots, then passes their device offsets to CUDA. A fill reads the three
immutable planes from the model's RAM mapping, and hits reuse those
already uploaded bytes. If a fill fails, the adapter invalidates the
cache metadata before a later request can reuse a slot.

The cache budget is the smaller of 4,992 MiB and available device memory
after dense placement and the existing scratch reserve. At the full
budget, static Apex geometry yields 1,283 slots for the Q6_K-down group
and 1,479 slots for the Q4_K-down group. These are planned capacities.
`GET /props` reports actual allocated cache bytes. The clean build linked
successfully. The host fixture `integration/goose-standalone/tests/expert-cache.c`
links the same archive with `-Wl,--wrap=fg_cuda_expert_upload`. It verified
three-plane fills, hits without copies, cross-layer eviction, independent
groups and recovery after an interrupted fill. Model execution, resident
hit rates and the performance comparison remain open.

EXP-2026-11-20-1503 integrates CUDA attention for strided Q4_0 K/V and
F32 queries, with an optional shared F16 mask. The standalone kernel reuses
the existing Goose Q4_0 decoder. It computes one partial softmax state per
query, head and KV segment, then merges those states and writes the existing
output layout. It handles both prompt chunks and single-token decode. This
path covers single-sequence layouts with no ALiBi or logit soft cap. The
other supported layouts and CPU-only builds use the existing Goose CPU
operation. K/V updates and primary storage still use the host path.

On the RTX 4070 the graph executor logged both Q4 attention fixtures on CUDA.
The 517-key grouped-query case with strided K/V and empty mask segments had
maximum absolute error of 0.000000755 against an independent F64 result.
The CPU run's maximum error was 0.000000761. Matrix and recurrent graph
fixtures also passed. No Apex model run or throughput measurement was made.

EXP-2026-11-20-1504 wires the existing Goose scalar GDN kernel to the
same CUDA graph executor. The runtime preserves the packed output, including
both per-token scores and requested recurrent snapshots. It admits the
existing kernel's widths 16, 32, 64 and 128 with compatible F32 layouts.
Cases outside that layout retain the CPU GDN implementation. The backend
uses the same transfer buffers as Q4 attention where the operations have
disjoint lifetimes. The host remains the owner of the recurrent state.

The RTX 4070 trace confirmed CUDA execution at width 16 and at the Apex
width 128. The width-128 scalar and per-column gate fixtures, with strided
queries and two sequences, matched the independent scores and snapshots
with a maximum absolute error of 0.0000000373. The CPU fallback, matrix and
attention fixtures also passed. No Apex model run or throughput measurement
was made.

EXP-2026-11-20-1505 attaches persistent CUDA state to the standalone model
owner. For registered Q4 KV tensors the row update keeps the host mirror
current and uploads the changed byte interval. Attention consumes the
device cache directly. The annotated Qwen3.5 GDN path with one snapshot
updates its registered recurrence matrix in place and downloads the token
scores. Its state-copy node consumes that publication. A graph with an
unconsumed publication fails, and the existing model-step failure path
resets the session. The Goose reset routine clears the host tensors and
the device arena. A CPU recurrent step can read the device matrix and
publish its update for the next CUDA step.

On a generated two-layer fixture the RTX 4070 trace confirmed three
resident KV and three resident GDN nodes across prefill and continuation.
The four-step CUDA/CPU/CUDA sequence matched the full CPU replay after
reset with a maximum absolute logit error of 0.0000001193. The CPU-only
chunked fixture and the operator checks also passed. `/props` exposes
the allocated `cuda_model_state_bytes`. Actual Apex placement and
performance remain unmeasured.

EXP-2026-11-20-1506 uses the existing Goose Q4_0 quantizer to write
contiguous SET_ROWS updates directly into the resident CUDA KV cache.
The dispatcher validates all row addresses before choosing a backend.
Sparse or repeated row indices retain ordered CPU updates. They retrieve
the containing device interval before writing so untouched rows remain
intact. CPU attention also reads the authoritative device interval when
switching from CUDA. The current graph uploads the F32 source rows for a
CUDA update.

The RTX 4070 fixture matched CPU-packed bytes exactly for strided source
rows, signed maximum ties, zero and subnormal inputs, and a sparse update
with duplicate indices. The mixed CUDA/CPU graph executed six resident Q4
write nodes and preserved the prior logit tolerance (maximum absolute error
0.0000001193). The CPU chunked fixture also passed. Apex throughput and peak
memory remain unmeasured.

EXP-2026-11-20-1507 routes F32 matrix multiplication through the same
Goose CUDA plane planner as Q4_K and Q6_K. A warp follows the CPU dot
product's accumulator grouping and final pairwise reduction, with an
ordered scalar tail. The dense manifest includes the F32 projections
actually used by the model graph. Static Apex metadata adds 80 tensors
and 84,213,760 bytes, for 1,361,551,360 bytes of eligible dense weights
in total. State placement also leaves the streamed vocabulary weight
span free when deciding whether the CUDA state arena fits.

On RTX 4070 the width-99 broadcast and width-17 indexed F32 fixtures
matched the CPU result bytes exactly, including strided inputs and the
remainder columns. The synthetic mixed graph staged F32 model weights
and executed 26 F32 matrix nodes in each CUDA step. It preserved the
0.0000001193 maximum logit difference on CPU/CUDA continuation. Chunked
CPU generation and resident KV write tests passed. Activations still
cross the host between graph nodes. Apex throughput and peak memory
remain unmeasured.

EXP-2026-11-20-1508 adds a bounded CUDA activation arena to the shared
graph. Matrices, Q4 attention and GDN can publish their outputs directly
into it. CUDA consumers resolve the same host span or a contained view to
the device address, including Q4 row updates. CPU reads materialize the
required data. Writes through aliases materialize and invalidate the
affected records. When space or record slots run out, the graph publishes
the old contents before reusing the arena. Allocation pressure retains
the normal staging path.

The public graph API still publishes all results. The model asks for its
selected logits, or for no host output on a prefill-only step. On the
RTX 4070 a 262-matrix synthetic graph reused 259 inputs on each pass and
verified view offsets, CPU overwrite, arena rollover and both output
contracts. The mixed model fixture also reused one activation per CUDA
step and kept the 0.0000001193 maximum logit difference. CPU chunked
generation passed. The arena is capped at 64 MiB. Apex throughput,
actual transfer volume and peak memory remain unmeasured.

EXP-2026-11-20-1509 moves the F32 broadcast arithmetic, scale, clamp,
sigmoid, SiLU, softplus, packed and split SwiGLU, RMS normalization and L2
normalization used by the model into the same CUDA graph. The device code
shares the Goose scalar activation expressions with the CPU. Normalization
keeps F32 squares and F64 accumulation, with a warp reduction. Inputs and
outputs use the existing activation arena. Existing CPU paths cover
layouts, data types and variants outside the admitted device operations.

The RTX 4070 trace confirmed all 14 strided vector fixtures on CUDA, with
maximum absolute error of 0.000000477 against the CPU. Each CUDA step of
the mixed synthetic model executed 49 vector nodes and reused 89 device
inputs. The CPU/CUDA continuation maximum logit difference was
0.0000000112. The alias-overwrite and selective-output graph also passed.
The build reused the compiled common model unit and rebuilt the
standalone operator and PTX sources. Apex performance and peak memory
remain unmeasured.

## Historical requirements audit

`docs/goose-migration/tasks.json`: **DONE 1**, **PARTIAL 144**, **TODO 7** of 152.

See [the production C++ audit](PRODUCTION-CPP-AUDIT-20261120.md) for the
original verdicts. The entries below are retained for traceability. The
tested standalone runtime capabilities are recorded above.

### PARTIAL

| requirement | what is open |
|---|---|
| P6-10 | grammar reject port: unit gate green (tests/grammar/reject-alternates 7 cases, first-set 6 cases) and the defect fixed... |
| P6-11 | MTP/draft-model routes for synth and rejection acceptance (needs a draft GGUF matching the target vocabulary); eleven ... |
| P7-06 | acceptance run on the real vocabulary against the shipped artefact |
| P7-07 | acceptance run on the real vocabulary against the shipped artefact |
| P7-12 | 1) delete the replaced native GGUF/reader/tokenizer bodies - blocked by rule 15 pending acceptance (the wiring itself ... |
| P8-06 | switch the reload sets and the connect/source choice, then exercise the lifecycle switches on a running model (load, s... |
| P8-09 | web_serve: static assets, MIME and ETag are still served by cpp-httplib set_mount_point - replacing that is the real w... |
| P8-12 | remove the remaining native server bodies after each switch is accepted: the Origin three-compare arm and the find_too... |
| P9-03 | replace the 25 call sites in the expert cache, dense placement and row streaming with the Goose decisions (commit-befo... |
| P9-04 | rewrite on_eval to the rh_topk_plan/rh_ask_plan contract and replace the overlap/row-source call sites |
| P9-05 | extract expert_bytes_per_slot(meta), keep rts_config as a drift pin, and wire rts_n_batch/rts_ubatch plus the MTP head... |
| P9-07 | wire the CSV, decode-trace and route-trace sinks and DeterministicWorkload to the Goose rows (integration step) |
| P9-08 | wire the closure decisions into session.cpp and cli/main.cpp |
| P10-06 | build-time CUDA architecture and flag decisions: CMake picks CMAKE_CUDA_ARCHITECTURES itself (vendor/flint/ggml/src/gg... |
| P11-01 | standalone CLI and HTTP run Apex with partial CUDA offload. Remaining work includes GPU graph coverage, server feature parity and the fixed-profile Apex gates. |
| P11-04 | D1: F2 gguf_write_to_buf, F3 gguf_get_meta_size and F4 gguf_get_meta_data (all native in vendor/flint/ggml/src/gguf.cp... |
| C1-02 | T_NEXTN_HC_HEAD ids exist only in the integration tree, not the pinned upstream |

### TODO

| requirement | what is open |
|---|---|
| P11-07 | 131 other APEX_COMMON tasks remain open; see `tools/apex-accounting.py`. |
| C1-04 | shared loader, tokenizer and memory interfaces needed by Qwen |
| C1-05 | complete Qwen inference qualification in the common artifact |
| C1-06 | Apex protection while Qwen support is extended |
| F1-01 | All workstreams implemented; zero required executable C++ across upstream, vendor, kernels, bridges, wrappers, fallbacks and infrastructure; historical ... |
| F1-02 | All required functional tasks have evidence; real Apex/Qwen and representative other family fixtures; one common artif... |
| F1-04 | Every functional task DONE; zero untracked residual native paths; source build reproducible and working runtime unchan... |

## Current baseline and candidate

- The Apex slot fixture passed through `run-jsondefault` and the isolated Path B libllama `a6a6b6280b95`. Both directions of serialized-state exchange passed. The two files were byte-identical, each 134,102,272 bytes with SHA-256 `dc07e51d15e7155368b04bf0a3abafd776ab87a13c8def932f7676a19f01950a`. Six warm and replayed continuations had identical 24 output tokens, content and saved probabilities, with 410 reused and 10 evaluated input tokens. This qualifies the tested slot file and Apex profile. The KV metadata codec still executes in C++. Raw runs are `results/apex-runs/pathb-slot-state-{control,candidate,reverse}-20261120-*`.

- `builds/flint-acc/run-jsondefault` is the current control. The historical P10-07 JSON acceptance is retained as evidence. The vendor execution scope remains open. The 21-text strict JSON gate and same-response chat check are recorded in [P10-07-CLOSED-20261120.md](P10-07-CLOSED-20261120.md).
- The inspected path B diagnostic set reused objects from the failed build for unchanged inputs. It passed `verify-candidate`, including relocation checks. It carries all 1257 distinct exports in the composed source. The earlier 1253/1256 count used names before composition, which missed three renamed exports and counted two source exports with the same original name as one.
- The reconstructed DRY path had been falling back when its C++ caller described the flat breaker array as a slice of triples. With corrected slice lengths, the public API fixture produced identical output in 200 cases and observed 200 Goose calls with the switch on, zero with it off.
- The new `tests/integration/path-b-abi` fixture verifies the GGUF comparison and observes the two DRY execution paths through the public sampler API. The strict JSON gate also exposed a missing pre-model initialization. After the library constructor began calling the existing M1 once guard, all 21 texts passed in strict mode.
- The fresh path B set with libllama SHA-256 `0668b36773d6eb5ff42bde96438883cc1b69fcef2769b0f7247c5c152d0bd8a1` passed the candidate, ABI and JSON gates with the exact support binaries from `run-jsondefault`.
- Its Apex diagnostic matched the native GGUF view (52 keys, 753 tensors, zero disagreements) and then faulted in `llama_vocab::n_tokens()`. The prebuilt Qwen35MoE object accessed `llama_model::vocab` at offset `0x8d78`. The reconstructed carrier and accepted control use `0x8d68`. The four added fields in the external Flint `llama_hparams` account for the displacement. See `results/apex-runs/pathb-diag-0668-20261120-033752/server.log`.
- Path B now stages the tracked vendor files and reconstructed carriers together. Its 201 C++ sources build with one header set and no prebuilt Flint model objects. The unified set `44e46f0f2e4c4ddd` loaded Apex, matched all 512 output tokens and passed the GGUF differential with zero disagreements.
- The first unprofiled A–B–B–A at 512 tokens matched all 12 responses. Decode fell from 61.86 to 56.43 tokens/s and prefill from 2883.87 to 2872.08 tokens/s. Post-request medians were 11147 to 11140.5 MiB of GPU occupancy and 21362.16 to 21366.11 MiB of server RSS. This set was rejected for promotion.
- Checking the first slot with the existing Goose predicate removed the full three-array KV copy from normal single-token decode. In the next unprofiled B–A–A–B, decode was 62.183 to 61.516 tokens/s, prefill 2848.56 to 2838.02 tokens/s, GPU occupancy 11162 to 11153 MiB and server RSS 21362.10 to 21350.55 MiB. All 12 responses matched and neither process swap nor system swap-out increased. The remaining 0.67 tokens/s gap kept the control in place.
- The post-fix profile across 511 decode steps showed scheduler time of 6925.7 to 6807.0 ms and residual time inside `llama_process` of 76.1 to 285.4 ms. A second correction makes KV rollback gather only the touched cells, preserves the extension fields and sequence bounds, and allocates the full three arrays required by the Goose bounds ABI. The whole unified build, ABI fixtures and strict 21-text JSON gate passed again.
- **512-token development checkpoint (EXP-2026-11-20-1492).** The `franken-pathb-safe-20261120.7dnWLq/run` set has libllama SHA-256 `3ab50925bd04686dac0381f38fe88cd155a5eff013ed33ef5d4ddc6dc35e2d15`, 1257 composed exports and 20/20 accepted components. The server and support libraries are byte-identical to `run-jsondefault`.
- The unprofiled B–A–A–B series gave decode **62.096 → 63.243 tokens/s**, prefill **2888.03 → 2857.06 tokens/s**, GPU occupancy **11182.5 → 11161.5 MiB**, server RSS **21362.12 → 21365.38 MiB**, and client duration **19.605 → 19.582 s**, all medians. All 12 runs produced the same 512 output tokens and response SHA-prefix `c2ff42cc1219`. Process swap and system swap-out increments were zero. Raw runs are `results/apex-runs/pathb-safe512-*`, with per-response records in `benchmarks/experiment-history.jsonl`. The accepted release stays on `run-jsondefault` while the 128- and 256-token checks are pending. The production ledger has 151 open requirements.
- The next KV ownership step is to give the canonical `goose/kv/goose-kv-seqpos-incr.goose` module the bounds allocation and the full cache lifecycle. The address-keyed C++ owner table stays until the Goose owner passes create, reset, move, restore and release checks on the same integration path.

## Historical measurements (2026-11-20 01:40)

| measurement | value |
|---|---|
| baseline artefact | `builds/flint-acc/run-syncfix` - 200 doors, 20/20 components |
| speed, paired | decode **61.53**, prefill **2915.3**, VRAM 9465 MiB, hash `c2ff42cc1219` |
| with/without adapter | 61.53 / 59.57 - the sampler-sync fix recovered 3-5% |
| JSON path | accepted: door + adapter + swap 7/7 in strict mode |
| suite | 10 run, 0 failures, 6 precondition skips |
| grammar | Goose path witnessed 6x on a live constrained request |
| OpenWing | `export fn` works; three blockers reproduced (float-literal adaptation, `from` as a name - fixed on our side, stdlib path) |
