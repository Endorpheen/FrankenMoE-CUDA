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

## Ledger

`docs/goose-migration/tasks.json`: **DONE 1**, **PARTIAL 144**, **TODO 7** of 152.

See [the production C++ audit](PRODUCTION-CPP-AUDIT-20261120.md) for all 152 dispositions. The narrower tables below retain earlier detailed remainders. Runtime rows require a proven production switch and removal of C++ and fallbacks.

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
| P11-01 | standalone runtime has no CUDA, tokenizer engine or HTTP (the working Apex path is the INTEGRATED stage-run, not the s... |
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
