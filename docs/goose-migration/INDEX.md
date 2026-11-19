# docs/goose-migration: where to look

292 markdown files hold this migration's record. This index lists the ones that describe the
**current state** first, because a plan read behind a stale note is how the state gets misread.

## Start here

| file | title |
|---|---|
| `HANDOFF-20261119.md` | ПЕРЕДАЧА РАБОТЫ — FrankenMoE-CUDA → Codex |
| `STATUS.md` | FrankenMoE migration status |
| `REPRODUCTION-VERDICT-20261119.md` | The accepted runtime reproduces from neither path in the tree |
| `BUILD-PROVENANCE-GAP.md` | Two build paths, and the release does not say which one made it |
| `BASELINE-DEPENDS-ON-UNTRACKED-ARTEFACT.md` | The accepted checkpoint cannot load without an untracked experiment artefact |
| `RELEASE-INTEGRITY-20261119.md` | The published release is the file this repository records |
| `ARTEFACT-MATRIX.md` | Which artefact answers which door |
| `PATH-B-REPRODUCTION.md` | Reproducing the accepted runtime (path B), from tracked sources |
| `EXTRA-OBJECTS-PROVENANCE.md` | extra-objects: 16 of 18 map to a tracked source, 2 are build products |
| `SERVER-DOORS-OUTSIDE-UNIT.md` | 29 server doors exist and are in no shipped library |
| `ROLLBACK-AUDIT.md` | Rollback: a field on four records, a sentence on the rest |
| `LEDGER-REFERENCES-AUDIT.md` | Every document the ledger cites exists; 36 of its build outputs do not |
| `F1-03-DEPENDENCIES.md` | F1-03 - every remaining dependency, justified |
| `C1-03-ARCH-AUDIT.md` | C1-03 audit: what Qwen reuses, what it decides, and one part that does not exist |
| `C2-04-ARCH-BRANCH-AUDIT.md` | C2-04 audit: one common implementation, every architecture branch with a recorded reason |
| `D3-JINJA-SUPERSEDED-PROOF.md` | D3: the eight Jinja slices are superseded, and here is the proof |
| `P10-10-PLATFORMS-STATE.md` | P10-10: platforms and alternate backends - three decisions, no call sites, and what can be verif |

## The rest, oldest-first by name

| file | title |
|---|---|
| `ABI-STATE.md` | ABI state, and the two things P10-08 asked for |
| `APEX-ACCEPTANCE.md` | Where the Apex acceptance stands, generated from the registry |
| `APEX-API-SCHED-CLOSURE.md` | P4-15 and P6-12 closure: Apex scope, Qwen assigned to stage 11 |
| `APEX-BASELINE-20261113.md` | Apex baseline, 2026-11-13 |
| `APEX-BASELINE.md` | Apex performance baseline |
| `APEX-FINAL-20261112.md` | Apex small plan: CLOSED (2026-11-12) |
| `APEX-HANDOVER.md` | Apex handover — checkpoint EXP-2026-11-19-1463 |
| `APEX-LOAD-20261116.md` | The standalone runtime already loads the full Apex model. The status said otherwise. |
| `APEX-REFERENCE.md` | Apex reference binary (restored) |
| `APEX-REMAINING-SCOPE.md` | Remaining Apex scope: CPU flash attention, CPU-only milestone, fused/cold experts |
| `APEX-STATE-20261111.md` | Apex port: state and where it is blocked |
| `APEX-TEST-PLAN.md` | Apex test plan — staged Goose runtime against the working Apex baseline |
| `ARCHITECTURE-MAP.md` | Architecture dependency map (AGENTS.md rule 17) |
| `ARCHITECTURE-PHILOSOPHY.md` | FrankenMoE architecture: modular source, one optimised compute path |
| `BIG-PLAN-AUDIT-20261112.md` | Big-plan audit, 2026-11-12 |
| `BLOCKER-20261112-sched-cuda-split.md` | Blocker: our line aborts on a long prompt inside the scheduler, and the base library is not repr |
| `BLOCKERS.md` | The remaining items, by the one thing that blocks each |
| `BRIDGE-BUILD-GAP-20261115.md` | The MoE-cache Goose bridges are built outside every build script |
| `BUILD-CACHE.md` | Build cache: measurements and options (research only) |
| `BUILD-REGISTRY.md` | Build registry (short, canonical) |
| `BUILD-SET.md` | Build set: the composed unit's module list |
| `C1-02-COVERAGE.md` | C1-02 — Qwen4Exp model graph and metadata |
| `C1-02-QWEN4EXP-STATE.md` | C1-02: Qwen4Exp graph and metadata - the slice exists, most of it is not in the unit |
| `C1-03-C1-04-QWEN-STATE.md` | C1-03 and C1-04: what Qwen-specific work exists, and what the native tree does not have |
| `C1-06-APEX-PROTECTION.md` | C1-06: protecting Apex while extending Qwen - what exists, and the one gate that is owed |
| `C2-04-EXTENSION-PATTERN.md` | C2-04: extending the runtime with another architecture, as actually done for Qwen4Exp |
| `CACHE-OBSERVABILITY-20261116.md` | The MoE cache had no working observability; it does now, and the first numbers are a finding |
| `CACHE-PATHS-20261116.md` | The two expert-cache paths, and a correction |
| `CANDIDATE-20261119-REJECTED.md` | Candidate 20261119 is rejected: a fresh build does not reproduce the accepted set |
| `CANDIDATE-VINTAGE.md` | What the deployed candidate actually contains, and where the rest lives |
| `CARRIER-DIVERGENCE-20261119.md` | Carrier divergence: the two copies of a source are not interchangeable |
| `CI-BLOCKERS.md` | CI blockers found before writing the workflow |
| `CODEX-HANDOFF.md` | Codex handoff: blockers the local agent could not close |
| `COMPILER-2026-3-STRING.md` | Compiler switch: goose-2026.3-string (runtime 1e714e6146bc) |
| `COMPILER-BLOCKERS.md` | Blockers for the Goose compiler, derived from nine real ports |
| `COMPILER-PREVIEW-ADOPTION-20261119.md` | Adoption of Fried Goose v0.1.9-preview "TrueWing": blocked by the export surface |
| `COMPILER-V61-QUALIFICATION.md` | Compiler v61: runtime qualification on the used path |
| `COMPILER-VERSIONS.md` | Compiler versions |
| `COMPLETION-ORDER.md` | The completion order — mandatory, not advisory |
| `CPU-MATH-APEX-CLOSURE.md` | CPU math closure: Apex execution scope |
| `CUDA-BLOCKER.md` | CUDA backend in the standalone — the exact blocker, measured |
| `CUDA-CONTROL-PLAN.md` | CUDA_CONTROL: integration plan and first step done |
| `CUDA-MMQ-PLAN.md` | CUDA_MMQ: state and remaining MMA work |
| `DECODE-REMAINING.md` | The decode gap: what is closed, what is open |
| `DECODE-REOPENED-CLOSED.md` | The FG148 question, answered at the source |
| `DEPENDENCY-MAP.md` | Which remaining work is gated on which decision |
| `EDGE0-120B-ADAPTATION.md` | Edge0-120B Q3_K adaptation (out-of-ledger work item) |
| `EXTERNAL-DEPENDENCIES-20261119.md` | External dependencies of the shipped set, and a RUNPATH defect found while checking them |
| `EXTERNAL-DEPENDENCIES.md` | External dependencies of FrankenMoE-CUDA: map and transfer plan |
| `EXTERNAL-INVENTORY.md` | External artefact inventory (stage 1 of the dependency transfer) |
| `FIX-LIST-20261112.md` | Consolidated fix list from the verification work (2026-11-12) |
| `GATE-EVIDENCE.md` | Gate evidence: what was proved, when, and against what |
| `GGUF-READ-INVENTORY.md` | What still reads GGUF natively, and what of it is verified |
| `HANDOFF-20261119-1377.md` | HANDOFF-20261119-1377 |
| `HANDOFF.md` | Handoff — batch/output block, EXP237–244 |
| `HOST-SIDE-INVENTORY.md` | Which remaining items have a host side in this tree |
| `INCIDENT-20261112-STAGE-OVERWRITE.md` | Incident: I overwrote the accepted runtime's server library through a symlink |
| `INFERENCE-SCOPE-CLOSURE.md` | Inference scope closure: P4-04 and P6-06 |
| `LIBLLAMA-ABI.md` | Why the reference libllama cannot be used |
| `LINEAGES-20261116.md` | Three lineages, and the 22 PARTIAL tasks all point at the one nothing builds |
| `LIVE-GOOSE-SHARE.md` | How much Goose actually executes in the working Apex path |
| `MAXWELL-CPP-LEDGER.md` | Maxwell C++ ledger and bridge-removal plan |
| `MBSA-HANDOFF.md` | MBSA handoff — Maxwell block-sparse attention |
| `MEASUREMENT_BLOCKER.md` | Resolved: the paired Apex measurement for EXP239–242 |
| `MERGE-MAP.md` | Merge map: which patched copies are cumulative and which are forks |
| `MODULO_SEMANTICS.md` | Division and modulo semantics: Goose is Euclidean, C truncates |
| `MOE-BLOCK.md` | The MoE block — requirements, read from the native code |
| `MTP-LAYER-COUNT-20261117.md` | RESOLVED: the MTP layer count, and a harness that was wrong twice |
| `NAMING-POLICY.md` | Naming policy, and the current state of every component |
| `NVIDIA-ARCH-POLICY.md` | NVIDIA architecture policy after 2026-11-13 |
| `OWNER-DECISIONS-20261112-CLOSURE.md` | Owner closure decisions, 2026-11-12 |
| `OWNER-DECISIONS-20261112.md` | Owner decisions blocking closure, 2026-11-12 |
| `P1-06-INVENTORY.md` | P1-06 inventory: raw quantized MoE math families |
| `P1-07-CLOSURE.md` | P1-07 closure: pack/trait round trips against the artifact, and what "matmul matches native" mea |
| `P1-09-INVENTORY.md` | P1-09 inventory: expert work distribution and synchronisation ownership |
| `P1-09-RESTATEMENT.md` | P1-09 restatement for the substrate that actually runs |
| `P1-10-INVENTORY.md` | P1-10 inventory: expert scratch, residency, staging and scatter |
| `P1-10-RESTATEMENT.md` | P1-10 restatement for the substrate that actually runs |
| `P1-11-INVENTORY.md` | P1-11 inventory: registered sources and backends vs what the line actually links |
| `P1-12-DECISION.md` | P1-12 — CPU MoE closure decision note |
| `P10-02-COVERAGE.md` | P10-02 — maintained command-line tools and utilities |
| `P10-02-TOOLS-INVENTORY.md` | P10-02: which command-line tools this tree actually has |
| `P10-05-COVERAGE.md` | P10-05 — owned executable infrastructure and generators |
| `P10-06-BUILD-AND-PACKAGING.md` | P10-06: what the build and packaging already do, and what is still missing |
| `P10-06-BUILD.md` | P10-06 — deterministic Goose build, generated C/CUDA and packaging |
| `P10-07-COVERAGE.md` | P10-07 — replace or justify vendored executable dependencies |
| `P10-07-JSON-ADAPTER-STATE.md` | P10-07: the JSON adapter - what exists and what the swap needs |
| `P10-08-COVERAGE.md` | P10-08 — public headers, FFI and binary interoperability |
| `P10-09-COVERAGE.md` | P10-09 — test runners, fixtures and diagnostics |
| `P10-09-TEST-AND-DIAGNOSTICS.md` | P10-09: test runners, fixtures and diagnostics - what exists and what is accounted for |
| `P10-10-COVERAGE.md` | P10-10 — cross-platform and alternate CPU/device support |
| `P10-10-WIRING-PLAN.md` | P10-10 wiring: the exact call sites |
| `P10-11-LICENSING.md` | P10-11 — licensing, attribution and source reproducibility |
| `P10-12-COVERAGE.md` | P10-12 — complete source/entrypoint inventory and residual native logic |
| `P11-04-D1-STATE-20261119.md` | P11-04 D1: state of the six named native bodies |
| `P11-04-D2-STATE-20261119.md` | P11-04 D2: configuration and speculation - what is wired, what is composed only |
| `P11-04-D3-STATE-20261119.md` | P11-04 D3: partially ported subsystems - model/session, metrics, stream policy, quantization, Ji |
| `P11-04-D4-QUANT-COVERAGE.md` | P11-04 D4: quantisation types - the number in the ledger is not a verified current fact |
| `P11-04-REMOVAL.md` | P11-04 — removal plan for replaced owned implementations |
| `P11-05-VALIDATION.md` | P11-05 — clean build, rollback and failure recovery |
| `P11-06-DEPENDENCIES.md` | P11-06 — unavoidable dependencies and remaining limits |
| `P2-01-INVENTORY.md` | P2-01 inventory: binary, unary and GLU |
| `P2-13-INVENTORY.md` | P2-13 inventory: type registry coverage |
| `P2-16-INVENTORY.md` | P2-16 inventory: CPU audit rows against existing evidence |
| `P2-18-LINK-MAP.md` | P2-18: the candidate CPU link map, object by object |
| `P3-03-CLOSURE.md` | P3-03 closure: CUDA copy, gather, scatter and tensor transforms |
| `P3-07-AUDIT.md` | P3-07 audit: dequantization complete, MMQ activation layouts partial |
| `P3-07-CLOSURE.md` | P3-07 closure: dequantization and activation quantization |
| `P3-08-CLOSURE.md` | P3-08 closure: MMVQ decode on the Apex scope |
| `P3-09-CLOSURE.md` | P3-09 closure: MMQ prefill on the Apex scope |
| `P3-16-CLOSURE.md` | P3-16 closure: CUDA host backend, devices and capability predicates |
| `P3-17-INVENTORY.md` | P3-17 inventory: GPU/pinned/VMM pools and transfer/residency policy |
| `P3-17-RESTATEMENT.md` | P3-17 restatement for the substrate that actually runs |
| `P3-20-CLOSURE.md` | P3-20 closure: selected CUDA link and the restored generated object |
| `P3-20-CUDA-LINK.md` | P3-20 census: the candidate CUDA link, and the definition it depends on |
| `P3-CUDA-SCOPE-CLOSURE.md` | P3-11, P3-12, P3-13 closure: Apex CUDA scope |
| `P4-03-ABI-PARITY.md` | P4-03: public symbol parity of the built line against flint |
| `P4-03-PATCH-STATUS.md` | P4-03: constructor patch applied, gate pending a relinked base |
| `P4-03-REPRODUCIBILITY.md` | P4-03: base-library reproducibility — findings and route |
| `P4-03-STATUS.md` | P4-03 status: verified surface, two open gaps, both behind the base-library blocker |
| `P4-10-CLOSURE.md` | P4-10 closure: cross-device copy planning on the Apex scope |
| `P4-13-CONTRACT.md` | P4-13 — unified allocation and scheduler interface (contract) |
| `P4-13-DECISION.md` | P4-13 — allocation and scheduler interface unification (decision note) |
| `P4-14-CLOSURE.md` | P4-14 closure: failure injection, invalid inputs, abort/reset/reuse and concurrent contexts |
| `P4-16-CORE-LINK.md` | P4-16: the candidate GGML core link, object by object |
| `P4-ALLOC-BACKEND-CLOSURE.md` | P4 allocator and backend-interface closure audit, 2026-11-12 |
| `P5-01-INVENTORY.md` | P5-01 inventory: KV cell allocation, sequence ownership and position maps |
| `P5-07-INVENTORY.md` | P5-07 inventory: checkpoint, prefix reuse, branch and restore |
| `P5-09-CLOSURE.md` | P5-09 closure: defrag retired, shift/cleanup OFF_PATH, lifecycle measured |
| `P5-09-INVENTORY.md` | P5-09 inventory: defrag, shift, cleanup, reuse |
| `P5-10-CLOSURE.md` | P5-10 closure: memory/KV subsystem on the real Apex model |
| `P6-02-INVENTORY.md` | P6-02 inventory: model tensor ownership, backend attachment, metadata queries, unload cleanup |
| `P6-02-INVESTIGATION.md` | P6-02 investigation: CUDA-split abort on a long prompt |
| `P6-03-INVENTORY.md` | P6-03 inventory: graph helpers and the trace that must prove them |
| `P6-04-EVIDENCE.md` | P6-04: Apex graph construction, prefill/decode and state lifecycle |
| `P6-05-COVERAGE.md` | P6-05 coverage — modern text model architecture builders |
| `P6-06-INVENTORY.md` | P6-06 inventory: recurrent and hybrid text builders |
| `P6-08-INVENTORY.md` | P6-08 inventory: context construction, evaluation, graph reuse and output APIs |
| `P6-09-COVERAGE.md` | P6-09 coverage — adapters, LoRA, control vectors and model saving |
| `P6-10-CLOSURE.md` | P6-10 closure audit: eleven items, their evidence, and what stays open |
| `P6-10-INVENTORY.md` | P6-10 — sampling and grammars: live inventory (2026-11-18) |
| `P6-10-SAMPLER-REACH.md` | P6-10: the sampler chain is not reached by the shipped build, and that was not recorded |
| `P6-11-COVERAGE.md` | P6-11 — MTP, speculative decoding and draft state |
| `P6-11-SCOPE.md` | P6-11 scope: MTP, speculative decoding and draft state |
| `P6-11-SPECULATIVE-STATE.md` | P6-11: speculative decoding - the doors that exist and the four that are called |
| `P7-01-CLOSURE.md` | P7-01 closure: GGUF reader on the Apex scope |
| `P7-02-COVERAGE.md` | P7-02 coverage — GGUF writing, splitting and tensor serialization |
| `P7-03-CLOSURE.md` | P7-03 closure record: model loader, mmap and file I/O ownership |
| `P7-04-COVERAGE.md` | P7-04 coverage — quantization, importance and conversion policy |
| `P7-06-COVERAGE.md` | P7-06 / P7-07: tokenizer and detokenizer coverage |
| `P7-08-COVERAGE.md` | P7-08 coverage: chat templates, Jinja and reasoning parsing |
| `P7-08-MAPPER-NOTES.md` | P7-08 output mapper notes |
| `P7-09-CLOSURE.md` | P7-09 closure: JSON, JSON-schema, schema-to-GBNF, PEG and grammar |
| `P7-09-COVERAGE.md` | P7-09 coverage: JSON, schema, PEG and grammar |
| `P7-09-INVENTORY.md` | P7-09 inventory: JSON, schema, PEG and grammar parsing are native |
| `P7-10-CLOSURE.md` | P7-10 closure: configuration surface, with fit scoped as host startup utility |
| `P7-11-COVERAGE.md` | P7-11 coverage — download, HF cache, HTTP, logging and process helpers |
| `P7-TOKENIZER-GGUF-STATE-20261119.md` | P7-06, P7-07, P7-12: the tokenizer and GGUF doors - what the shipped copies call |
| `P8-01-CLOSURE.md` | P8-01 closure: server startup, configuration and lifecycle |
| `P8-02-CLOSURE.md` | P8-02 closure: HTTP transport, routing, CORS and request parsing |
| `P8-03-REMAINDER.md` | P8-03 remainder: what "timeout and backpressure" means here |
| `P8-05-INVENTORY.md` | P8-05 inventory: embedding, reranking, tokenization and utility endpoints |
| `P8-06-COVERAGE.md` | P8-06 — model management, connectors and hot reload |
| `P8-06-MODEL-MANAGER-STATE.md` | P8-06: model management - two doors served, eleven composed only, and a source/binary drift |
| `P8-07-COVERAGE.md` | P8-07 coverage — tools, MCP, structured output and decision helpers |
| `P8-08-INVENTORY.md` | P8-08 inventory: metrics, watchdog and diagnostics |
| `P8-09-COVERAGE.md` | P8-09 — web/UI serving and application integration |
| `P8-09-WEB-STATE-20261119.md` | P8-09: web and UI serving - nineteen doors composed, zero call sites |
| `P8-10-CLOSURE.md` | P8-10 closure: Apex server/API acceptance corpus |
| `P8-12-P4-13-ANALYSIS.md` | P8-12 / P4-13 — server removal and lineage unification (analysis) |
| `P8-12-SERVER-NATIVE-STATE.md` | P8-12: the native server implementation, checked by name |
| `P9-01-COVERAGE.md` | P9-01 — standalone config, recipe and architecture registry |
| `P9-02-CLOSURE.md` | P9-02 closure: platform/file I/O and GGUF offset indexing |
| `P9-03-COVERAGE.md` | P9-03 — dense weights and expert streaming/LRU sources |
| `P9-03-EXPERT-CACHE-STATE.md` | P9-03: expert cache, dense placement and row streaming |
| `P9-04-COVERAGE.md` | P9-04 — row streaming, router hooks and I/O-compute overlap |
| `P9-04-ROW-STREAMING-STATE.md` | P9-04: row streaming, router hooks and I/O-compute overlap - what exists |
| `P9-05-COVERAGE.md` | P9-05 — standalone sessions, runtime and ngram draft |
| `P9-05-P9-08-STANDALONE-STATE.md` | P9-05 and P9-08: the standalone engine - decisions in Goose, consumers only in the m1 harness |
| `P9-06-COVERAGE.md` | P9-06 coverage — chat/thinking controls and the standalone CLI |
| `P9-07-COVERAGE.md` | P9-07 — metrics, traces and deterministic workload |
| `P9-07-METRICS-STATE-20261119.md` | P9-07: metrics, traces and deterministic workloads - what is composed and who calls it |
| `P9-08-COVERAGE.md` | P9-08 — standalone engine native implementation |
| `P9-DEPENDENCY-BOUNDARY.md` | Where the rest of the P9 family can be verified, and where it cannot |
| `PASS-COMPOSITION-20261117.md` | What "compose the 41 layers into one pass" actually needs |
| `PERF-INVENTORY.md` | Inventory of the performance modules — what runs on Goose, what does not, and why |
| `PORT-BUFFER-CONTRACT.md` | What a door may write, and what the host must check |
| `PREFILL-DECODE.md` | Prefill and decode — what is established, and where I stopped |
| `REAL-CHAIN-20261117.md` | Real values chained through the Goose builder |
| `REAL-VALUES-GAP-20261117.md` | Correction: 20/20 passes on real SHAPES, not on real WEIGHT VALUES |
| `REAL-VALUES-PASS-20261117.md` | The value path works: a real q4_K Apex tensor through a kernel |
| `RELEASE-NOTES-TEMPLATE.md` | <version> — <short title> |
| `REMAINING-PRIMITIVES.md` | The remaining native primitives, each with why it is one |
| `REQUEST-SESSION-PLAN.md` | APEX.REQUEST / APEX.SESSION: scope and first target |
| `RUNTIME-INTEGRATION.md` | Runtime integration plan — composed Goose modules into the GPU runtime |
| `RUNTIME-MAP-20261117.md` | Accepted Apex runtime: source, ownership and ABI map |
| `RUN_AUTHORIZATION.md` | Standing run authorization (this session) |
| `S15-GATE-20261116.md` | The S1.5 tokenizer gate: what it proves now, and one stale claim in it |
| `SAMPLER-UNWRITTEN-ENTRY.md` | The invalid token is an unwritten entry, not a wrong id |
| `SPEC-BLOCKER.md` | Blocker resolved: the server is built from a merged copy, not from the vendor source |
| `SPEC-OPEN.md` | Open question: a print that must fire does not, in the same block as one that does |
| `SPEC-WIRING.md` | Wiring the speculative decisions into the host: what works and what does not |
| `STANDALONE-DEPS.md` | What the standalone runtime actually depends on |
| `STANDALONE-HARNESSES-20261116.md` | 23 standalone harnesses, two of them run, one failing |
| `STATE-BUFFERS.md` | State buffers — the recurrent layer's KV cache |
| `STATE.md` | State between calls — broken into closed units |
| `STRATEGY-ADDITIONS.md` | Long-term strategy additions (recorded 2026-11-09, not yet active) |
| `SYNC-GUARD.md` | sync-goose-tree.py — the rewrite guard (rule 26) |
| `TECH-DEBT.md` | Technical debt ledger |
| `TEST-COVERAGE.md` | Test coverage of every module in the shipped unit, and what is not covered |
| `TEST-PLAN.md` | Test coverage plan: the 24 modules of the shipped unit, progress and remainder |
| `TESTING-CENTRAL.md` | Centralised tests |
| `TOKENIZER-PATH-20261116.md` | The tokenizer decisions are live in the hot path, not merely linked |
| `TRANSFER-BUDGET-20261116.md` | Where the decode time actually goes: bandwidth 21%, synchronisation 3.5% |
| `TREE-DRIFT-20261115.md` | Goose tree drift — found 2026-11-15 |
| `UNIT-CARRY-20261119.md` | How much of the composed unit an artefact actually carries |
| `UNTRACKED-SOURCES-20261116.md` | Seven files the runtime is built from are not in the repository |
| `VERIFICATION-SET.md` | The cheap verification set |
| `VERIFIED.md` | Verified against compiled native code |
| `VRAM-BUDGET.md` | The VRAM budget, measured rather than assumed |
| `WHAT-IS-GOOSE.md` | What in the candidate is actually Goose |
| `address-typing-feedback.md` | Address typing: the measured cause of the matmul gap |
| `apex-cpu-math-inventory.md` | APEX CPU_MATH inventory for the agreed Apex path |
| `apex-deferred-optimization.md` | Deferred plan: one optimization item after the small Apex path |
| `apex-remaining-boundaries.md` | Small Apex path: what remains, and the boundary of each item |
| `architecture-scope.md` | Architecture scope: what we keep and what we drop |
| `audit-report.md` | Технический аудит архитектуры FrankenMoE — отчёт о чтении кода (строго чтение) |
| `baseline-vs-candidate.md` | Baseline vs candidate: the conditions, checked flag by flag |
| `census-full.md` | Full kernel census from the archived trace |
| `census-split.md` | Where the GPU time actually goes, split by who runs it |
| `census-status.md` | Apex census — transfer status |
| `compiler-limitations.md` | Fried Goose compiler: confirmed limitations blocking the FrankenMoE port |
| `compiler-revalidation-2026-11-10.md` | Compiler revalidation, 2026-11-10 |
| `correctness-evidence.md` | Correctness evidence of the active runtime |
| `cpu-control-remainder.md` | APEX.CPU_CONTROL: the exact remainder after the build was made reproducible |
| `cpu-math-unblocked.md` | APEX.CPU_MATH: the vector blocker is gone, with the evidence |
| `cpu-moe-structure.md` | CPU MoE: Goose owns the control, native owns the dot products |
| `cpy-transposed-regression.md` | Perf check found a real regression the Apex profile cannot see |
| `cuda-build-unblocked.md` | Building the Goose `libggml-cuda` here: unblocked |
| `cuda-chain-rebuild.md` | Rebuilding the CUDA Goose chain: what works, what is missing, in what order |
| `cuda-codegen-gap.md` | The CUDA items share one cause: a codegen gap, measured twice |
| `cuda-matvec-remainder.md` | APEX.CUDA_MATVEC: the exact remaining blocker, measured |
| `final-coverage.md` | Coverage of the kernels Apex actually executes |
| `find-slot-scope.md` | Why `llama_memory_recurrent::find_slot` is not ported |
| `gate-sweep-2026-11-11.md` | Gate sweep after the closed items (2026-11-11) |
| `gates-2026-11-10-final.md` | Full gate run, end of session — 25 fixtures, 1,248,346 checks, 0 failures |
| `gates-2026-11-10.md` | Full gate run against the shipped artifacts, 2026-11-10 |
| `ggml-cpu-build-status.md` | Building the Goose `libggml-cpu` in this tree: state after the first attempt |
| `grad-mode-open.md` | test-backend-ops `grad` mode: four failures, not yet reproduced in isolation |
| `integration-readiness-2026-11-11.md` | Integration readiness for the small Apex path (2026-11-11) |
| `link-closure-measurement.md` | What is in the link line (P3-20 measurement) |
| `mailbox-protocol.md` | Mailbox protocol — read it at every checkpoint, not when convenient |
| `measurement-env-stripped.md` | My A/B Apex measurements were all the same configuration |
| `mm-ids-width-scope.md` | `mm_ids_helper` width scope — the "other widths" item is moot for Apex |
| `mmq-chain-variant.md` | The chain ships the rejected MMQ variant |
| `mmq-dormant-optin.md` | The MMQ route ships in the runtime, opted out — two defects, one correction to my own records |
| `mmq-mma-not-portable.md` | Why the MMA path of `mul_mat_q` is not portable with this module format |
| `mmq-reopen.md` | MMQ Q8_1: the rejection rested on a 5-point-spread measurement |
| `named-result-defect-exposure.md` | Exposure to the confirmed named-result codegen defect (2026-11-10) |
| `named-result-fix-verified.md` | Named-result fix, subset-7ef08c2d0b65dffe — verified |
| `p5-entry-plan.md` | P5 entry: what the KV subsystem looks like from here |
| `p6-03-scope.md` | P6-03 scope: why the builder DSL is not more of EXP200 |
| `perf-mode-check.md` | `test-backend-ops perf` on the active runtime |
| `profiling-unavailable-2026-11-10.md` | Both GPU profilers are unavailable on this machine (2026-11-10) |
| `provenance-audit-2026-11-10.md` | Provenance audit: every linked object has a committed source (2026-11-10) |
| `ratio-audit.md` | Audit for the sample_ratio class of defect |
| `reconstruct-accepted-tree.md` | Reconstructing the tree the accepted runtime was built from |
| `regression-sweep-20261108.md` | Regression sweep after the defect fixes |
| `remainder-audit.md` | Audit of the native remainder that is not blocked by codegen |
| `route-audit-2026-11-10.md` | Route audit of the active runtime, 2026-11-10 |
| `sampler-not-a-target.md` | The sampler is not a port target, and the reason is the working-set size |
| `switch-hygiene.md` | Module switch audit — defaults are correct, one name is misleading |
| `test-backend-ops-failures.md` | test-backend-ops against the active runtime: current failure map |
| `upstream-comparison.md` | Full Goose path vs current official llama.cpp |
| `where-the-work-is.md` | What is left, and why each remaining candidate is not a port |

## How to tell a current document from a historical one

A document whose first line names a date later than the fact it states is not automatically current:
today several documents written this morning were found to describe a build that no longer existed.
The rule used here is the one the ledger uses - a claim is current when a tool command in this
repository reproduces it, and `tests/REGISTRY.md` names the tools that can.
