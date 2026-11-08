# Apex Goose functional migration plan

Active plan: complete the actual Apex execution path before optimizing; preserve the universal [160-requirement backlog](GOOSE_MIGRATION_PLAN.md).

**32 target-scoped requirements: 12 inherited DONE; 20 remaining; 3 large delivery batches.**

Target-scoped work units,not original160task closures or code/effort percent. DONE inherited units retain bounded historical evidence; combined artifact qualification remains open.

## Actual model and execution path

Read-only audit: [metadata,source hashes and saved trace anchors](docs/goose-migration/apex-path-audit.json). Model architecture `qwen35moe`;40main layers (30recurrent/GDN,10attention),256experts/top8. Storage Q4_K/Q6_K/F32; internal F16/F32/Q8 and Q4KV are separate scope. Excluding Qwen model development does not exclude the GDN path used by Apex.

Startup/config/chat -> GGUF/mmap/tensor bindings -> tokenizer -> batch/context/hybrid memory -> graph builder -> CPU expert quantization/routed/tiled math + CUDA attention/dense/recurrent math -> scheduler/buffers -> logits/sampling/detokenization -> HTTP/SSE. Every owned implementation on this path must become Goose; ABI accessors are interim glue,not final native-free completion.

The fixed diagnostic profile uses context262144,input32768/output128,CPU MoE,CUDA dense/attention,FAon,Q4_0KV,b4096/ub4096,t8/tb16,specnone,EHSoff,mmap,one slot and32context checkpoints. Optional GPU expert cache is disabled by size0. Baseline argv and raw results remain in the existing history; no new speed claim is made here.

## Remaining sequence

Batch A (APX-013..019):CPU/GGML/memory/model runtime. Batch B (APX-020..025):CUDA. Batch C (APX-026..032):loader,text/server,canonical integration and complete closure. CPU/GPU interfaces may be implemented in a shared pass; critically dependent ownership gates stay explicit. Three batches are a target; no guarantee of compiler capability or completion date.

1. **APX-013 — Complete active CPU numerical/operator providers** (IN_PROGRESS;batch A). Embedding/GetRows,GLU and every CPU numerical fallback reached by Apex use Goose; precision,alias/stride and provider dispatch checked. Other formats/ISAs remain universal backlog. Original scope: P1-06, P1-07, P1-08, P1-09, P1-12, P2-01, P2-02, P2-03, P2-04, P2-05, P2-06, P2-07, P2-08, P2-09, P2-10, P2-11, P2-12, P2-13, P2-16, P2-17, P2-18.
2. **APX-014 — GGML contexts,tensor objects,shape,strides and views** (IN_PROGRESS;batch A). Own context/object/tensor creation,iteration,reset/free,shape/type/stride/view helpers and graph node construction in Goose; checked overflow and view bounds; no native algorithm hidden in field adapters. Original scope: P4-03, P4-04, P4-05.
3. **APX-015 — Physical tensor,buffer and graph allocator** (IN_PROGRESS;batch A). Goose owns reservations,allocation/free/reuse and ownership,including partial initialization/OOM; native OS allocation only remains as primitive. Original scope: P4-06, P4-09, P4-10, P4-11.
4. **APX-016 — CPU/GPU graph scheduler** (IN_PROGRESS;batch A). Goose owns placement,splits,copies,events,buffer reuse,execution and cancellation; cross-device lifetime and failure checks. Original scope: P4-12, P4-13, P4-14, P4-15, P4-16.
5. **APX-017 — Hybrid KV and recurrent state** (IN_PROGRESS;batch A). Attention KV plus conv/GDN state,lazy reservation,position management,prefix/checkpoint restore and cleanup execute Goose for Apex. Original scope: P5-01, P5-02, P5-04, P5-05, P5-06, P5-07, P5-08, P5-09, P5-10.
6. **APX-018 — Complete Apex model graph builder** (IN_PROGRESS;batch A). Metadata/tensor bindings and all40main-layer branches:10attention,30recurrent,routed/shared FFN,embedding/output head; no MTP needed with specnone. Original scope: P6-01, P6-02, P6-03, P6-04, P6-07.
7. **APX-019 — Batch,ubatch,prefill/decode and output orchestration** (IN_PROGRESS;batch A). Goose owns context lifecycle,batching,graph execution,logits/output extraction,abort/error propagation for normal variable Apex inputs. Original scope: P6-08, P6-09, P6-10, P6-12.
8. **APX-020 — Apex CUDA elementwise,norm,activation,reduction and RoPE** (TODO;batch B). Every active dtype/shape/stride of binary/GLU/RMS/softmax/topK/RoPE and related numerical operations executes Goose-generated device code; native leaves cannot masquerade as completed module. Original scope: P3-01, P3-02, P3-03, P3-06, P3-07, P3-08.
9. **APX-021 — CUDA movement,indexing,conversions and cache writes** (TODO;batch B). Copy/contiguous/views/GetRows/SetRows/gather/scatter and actual internal formats checked for bounds,alias and async lifetime. Original scope: P3-04, P3-05, P3-14.
10. **APX-022 — CUDA matrix paths for prefill and decode** (TODO;batch B). Apex dense Q4_K/Q6_K and actual internal activation formats,GEMV/GEMM/quantization and vendor-dispatch decisions owned by Goose; vendor primitives explicitly declared. Original scope: P3-09, P3-10, P3-11.
11. **APX-023 — Apex Flash Attention** (TODO;batch B). Actual head geometry,Q4_0KV and required sequence sizes have Goose FA implementation with mask/position/numerical checks. Original scope: P3-12.
12. **APX-024 — CUDA conv,GDN and recurrent updates** (TODO;batch B). SSM-conv,gated-delta and recurrent prefill/decode operations execute Goose; persistent state agrees with native multi-step oracle. Original scope: P3-13.
13. **APX-025 — CUDA backend and resource ownership** (TODO;batch B). Device/backend,streams/events,pinned/VMM/pools,transfers and cleanup owned by Goose; asynchronous/error-path evidence and rollback. Original scope: P3-16, P3-17, P3-19, P3-20, P4-07, P4-08.
14. **APX-026 — GGUF parser and complete Apex model loader** (TODO;batch C). Goose parsing,validation,mmap,weight/type/tensor binding,resident expert registration and cleanup; malformed/truncated input checks. Original scope: P7-01, P7-02, P7-03, P7-04, P7-05.
15. **APX-027 — Apex tokenizer,Unicode and detokenization** (TODO;batch C). GPT2/qwen35 BPE and required Unicode/special-token handling agree with native token corpus including adversarial text. Original scope: P7-06, P7-07, P7-08.
16. **APX-028 — Sampling,RNG and chat-template processing** (TODO;batch C). Sampling/token acceptance,Jinja/chat/reasoning and required text parsing owned by Goose; deterministic profile parity and malformed input handling. Original scope: P7-09, P7-10, P7-11, P7-12.
17. **APX-029 — Normal text server and startup path** (TODO;batch C). CLI/config,startup,HTTP completion/chat,SSE,request queue/slot,errors/cancel/shutdown all needed by Apex implemented in Goose; optional unrelated tools/UI/modes deferred explicitly. Original scope: P8-01, P8-02, P8-03, P8-04, P8-05, P8-06, P8-07, P8-08, P8-09, P8-10, P8-11, P8-12, P9-01, P9-02.
18. **APX-030 — Canonical Goose integration and native replacement** (TODO;batch C). Reproducible one-runtime build/link/source ownership; remove owned replaced native algorithms and C++ storage/containers in active path; imports limited to declared external primitives. Original scope: P10-01, P10-02, P10-05, P10-06, P10-07, P10-08, P10-09, P10-11.
19. **APX-031 — Active source/import/liveness closure** (TODO;batch C). Inventory every reachable owned implementation and actual Goose witnesses; no native fallback silently counted; source/build/model/compiler hashes,dependency report and reproducible rollback. Original scope: P0-04, P0-05, P10-12, P11-01, P11-02, P11-03, P11-04, P11-05, P11-06.
20. **APX-032 — Full Apex functional closure and performance-phase handover** (TODO;batch C). All prior targets complete in one artifact; differential local corpus plus one shared Apex32K/128 integration gate with IDs/top5,text,state,actual execution and RAM/VRAM. Cover variable requests and error contracts locally; no canned graph/output. Then start fresh-C++ paired full-system optimization. Original scope: P11-07, P11-08.

## Inherited bounded work

| Target | Existing scope | Evidence |
|---|---|---|
|APX-001|Immutable native controls and rollback (P0-01)|EXP141/results/reference-integrity-exp141.json; EXP136..141|
|APX-002|Effective-source provenance inventory (P0-02)|docs/goose-migration/source-audit.json|
|APX-003|Qualified self-hosted producer (P0-03)|EXP141/compiler/producer-receipt.json; revalidate at every subsequent stage|
|APX-004|Experimental isolation and disable switches (P0-06)|EXP141 snapshot rebuild:8objects,104graphs per backend; previous snapshots retained|
|APX-005|CPU activation Q8_K/Q8_0 quantization (P1-01)|EXP137 Q8_K; EXP139 common Q8_0/Q8_K exact and model traces; Q8_0 performance deferred EXP148 routes shared MoE Q8_K/Q8_0 through the same checked encoder module;new default-on flag has explicit disable rollback,841retained activation cases pass.|
|APX-006|CPU Q4_K/Q6_K raw and routed expert arithmetic (P1-02)|EXP137..138 CPU MoE and Apex traces|
|APX-007|CPU Apex Q4_K/Q6_K tiled-prefill computation (P1-04)|EXP141:104graphs/backend; actual Apex new-path execution; Qwen profile selects raw path|
|APX-008|Expert grouping and checked workspace policy (P1-05)|EXP140: Apex114/Qwen412graphs; exact actual models; native OS/compute services remain|
|APX-009|CPU backend/device/buffer/capability policy (P2-14)|EXP150 common CPU backend/device/buffer/capability/registry functional ownership closed:inherited real Goose API/buffer/traits plus complete registry/device initialization/properties/brand sequencing and HBM allocation/error/wrapper cleanup.210registry/OS/HBM-policy cases and5compiled HBM API/stub cases;30296cache/budget cases,actual326/66/960/174GGML corpus,component disable rollback and scoped ASan/UBSan/LSan pass. Fixed Apex32K/128 exact text/allIDs/top5;registry0-9 and127680workspace calls witnessed;RSS21312.090MiB/VRAM3348MiB. HBM actual hardware and other OS execution unqualified;architecture/provider math/ABI storage/native removal remain explicitly assigned P1-07/P2-03/P2-13/P2-16/P2-18/P10-10. Early copied-library alias gate invalidated and corrected before model.2656.285pp/28.079tg diagnostic vs saved C++2918.900/29.589;not paired speed/no-regression acceptance.|
|APX-010|CPU graph/job/threadpool execution control (P2-15)|EXP149 original P2-15 acceptance closed:all101dispatcher entries/96callbacks checked with independent unchanged-native-switch oracle;960task/workspace cases exact;Goose owns graph-wide planning/tiled sizing,executor reuse/abort,OMP+pthreads job lifecycle,physical pool init/cleanup,masks,barrier/chunk/readiness/poll/pause/kickoff/NUMA/priority policy.326/66/174actual GGML/common cases pass both implementations;78OMP/81pthread real lifecycle/graph failure cases,494policy boundary cases,release+ASan/UBSan/LSan,and scoped new-control pthread TSan pass. Exactly one Apex32768/128 per mode,text/allIDs/top5 exact vs C++;new pool/job paths witnessed. Complete source boundary audit maps numerical/provider/ABI/topology adapters to other open requirements;not fullCPU/native-free/safety-superiority/speed acceptance. P1-09/P2-14 and other stage1requirements remain open. C++2918.900/29.589;OMP2646.558/28.462;pthread2669.811/26.424tok/s are preliminary single samples,not stable performance parity.|
|APX-011|Allocation reuse/coalescing metadata policy only (P4-01)|EXP136 native service boundary retained; this does NOT mean physical allocator complete|
|APX-012|Tensor dependency/view lifetime policy only (P4-02)|EXP136 graph/lifetime evidence; full tensor ownership/scheduler still native|

## Execution and completion rules

Owner explicitly authorizes one paired Apex/native-C++ benchmark at the end of each fully completed delivery batch. No repeat approval is needed. Before the gate announce purpose,expected duration and the two arms. Use identical fixed profile,input and sampling;record prefill/decode,RAM/VRAM,correctness and actual Goose liveness. This does not authorize intermediate model runs or repeated performance series.

Finish the entire active batch before reporting its completion or advancing. The owner superseded separate-batch acceptance barriers: integrate all existing prepared Apex blocks into one common candidate and then run the shared Apex gate. Preserve per-component correctness,dependency and rollback checks;do not restart generation or require separate Batch A closure before integrating CUDA/loader/server. No partial completion counters or partial-list reports.

- Production and compiler source remain untouched. All candidates stay in `/home/end0/AI/LlamaServer/`;feature-disable and immutable-native rollback retained. Owner authorizes sixteen saved OpenCode child workers on `opencode-go/deepseek-v4.1-flash`; no new Codex subagents. Necessary project source/header snapshots are authorized for future tasks; exclude secrets,weights and private instructions. Review every returned artifact before integration.
- Reuse qualified objects/modules. During a batch use necessary changed-module differential,bounds/ownership/failure checks;share generation/build and a single fixed-profile real-model gate at batch end. No microbenchmarks or routine full sanitizer campaigns.
- Record known failures and performance regressions in ignored local `COMPILER_FEEDBACK.md`. Never treat a source wrapper,native service algorithm or table-driven canned answer as a Goose implementation.
- DONE requires supported self-hosted producer,real Goose computation/control,correctness and actual integration evidence. Whole functional closure additionally rejects hidden owned native code in source/import audit. Generated C/CUDA and explicitly justified OS/CUDA/vendor primitives remain declared dependencies.
- Do not erase unfinished universal requirements or mark them done from this narrower projection. Non-Apex modes remain backlog. Unseen required Apex operations append scope explicitly.
- After APX-032,start end-to-end optimization immediately. Fresh native C++ and paired repeat measurements must establish stable >3000prefill and >30decode without correctness/memory regressions;functional completion alone does not prove performance parity.

## Current checkpoint

EXP153 keeps all seven Batch A requirements IN_PROGRESS. No whole requirement or delivery batch is closed by partial local evidence.

- One isolated runtime now combines the Goose GGML context/tensor/shape core,operation constructors,physical/graph allocation,scheduler and Apex model graph/binding module. The merged graph differential passes against the native reference. Integration exposed a null second-operand dereference in unary shape projection;fixed by the common typed ABI and unary-aware projection. No compiler change was needed.
- Goose hybrid batch/context/sequence/checkpoint orchestration is compiled by the same qualified self-hosted producer and linked into `libllama-apex-hybrid-goose.so.0.7.0`. Real small CPU attention/recurrent buffers,sequence changes,state roundtrip and failed-read cleanup agree with the C++ disable arm. Evidence: `/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/results/hybrid-memory-summary.json`. This is module correctness,not Apex inference or performance evidence.
- Hybrid graph-input construction,host state-index filling and reuse policy now execute Goose in the shared candidate. Actual CPU input-buffer bytes,metadata and changed-input rejection match the native arm;`GGML_GOOSE_HYBRID_INPUT=0` restores native. Evidence: EXP153/results/hybrid-inputs-summary.json. Child KV numerical input APIs remain an explicit dependency.
- Generic/multi-buffer creation,clear,free,reset,usage propagation and base-pointer control now execute Goose. One combined existing allocation/failure/retry corpus agrees with C++,and every new entry was witnessed. Evidence: EXP153/results/buffer-lifecycle-summary.json. Native POD header storage/provider/OS primitives remain explicit.
- Child KV/recurrent storage/lifecycle,state IO,graph traversal/hash and batch/context execution remain under review/integration. Three received recurrent Goose modules emit successfully with the frozen producer;missing runtime hooks and an optional inactive oracle were explicitly returned for correction. Unreviewed helper output is never counted as closed work.
- Eight saved OpenCode sessions now use `opencode-go/deepseek-v4.1-flash`,confirmed by actual assistant provider/model records. Finished slots receive distinct next work. Production and compiler sources remain untouched.
- Historical EXP150 exact128tokens/top5 is retained;its post-ABI whole-candidate requalification and all new components await the one shared end-of-batch Apex gate. EXP149 remains immutable model-qualified rollback. No new Apex run,microbenchmark or performance claim was made during this checkpoint.
- Sources,generated C,commands,local results and replay scripts are saved under EXP153. Reproduction scripts for the newly linked model/hybrid cohort are under `source/integration/reproduce/`;these affect only the isolated candidate. CUDA numeric migration stays Batch B,and loader/text/server/native-free closure stays Batch C.

Update `docs/goose-migration/apex-path-tasks.json` and this plan at coherent checkpoints. Keep full-system measurements in `docs/goose-migration/apex-history.json`.

## Current delegation policy

Eight existing OpenCode child sessions execute only `opencode-go/deepseek-v4.1-flash`. Scope/file ownership:1graph/hash+active backend-control,2attention KV,3recurrent memory,4context execution/resources,5batch allocator,6CPU providers,7graph-result/input ownership,8physical/graph allocator closure. Each has a separate worker directory and access to isolated local compiler/CPU fixture checks. No Codex subagents.

The owner authorizes8→12→16→20and further+4 after two normal completed cycles per level,subject to actual quality and provider stability. There is no task-per-hour metric or fixed5/12 ceiling. A pending generated-code queue is not backpressure. A normal cycle needs real returned large blocks,verified local checks,no substantial unresolved defects and actual parallelism;source-only proposals or partial steps are insufficient. Provider overload reduces concurrency automatically while preserving healthy requests,saved sessions and files. No normal8-worker cycle is claimed complete at this checkpoint.

### Acceptance ownership update (2026-11-08)

When the ready queue grows,Codex switches from own implementation to acceptance. At16active OpenCode workers this mode becomes permanent:review,minor fixes,integration and concrete return-to-worker feedback only;all new subsystem implementation is delegated. Expansion to20and24 additionally requires integration to keep up,provider stability and acceptable correctness. The16-worker threshold was reached and saved actual session evidence exists;Codex remains permanently in acceptance mode. Production and the Apex GOAL remain unchanged.

### Shared cohort checkpoint after RAM recovery (2026-11-08)

The isolated EXP153 shared runtime now links qualified graph storage/traversal/hash/backend control,allocator size closure,recurrent constructor/context/state,common status/string/buffer IO,batch routing/public reserve and CPU numerical/provider updates. Actual normal API/failure/rollback corpora passed;the CPU provider uses actual changed source objects and matches the native corpus without preload. Graph result/input ownership is linked with a runtime disable switch. The separately linked context candidate matches the real vocabulary-only constructor/cparams/host-state probe;it is not active while device IO/full context gates remain pending. No Apex model was loaded for these local gates. All seven Batch A requirements remain IN_PROGRESS. Exact library hashes,receipts and remaining integration seams are in `/home/end0/AI/LlamaServer/franken-goose-exp153-apex-core-20261107/results/current-cohort-checkpoint.json`.

Sixteen DeepSeek sessions are saved and isolated. After the reported RAM/Gnome shutdown,the stopped server was restored under a dedicated user service and unfinished sessions resumed. Compilation checks have separate 4/6GiB high/max memory cgroups;the server has 20/24GiB high/max. This protects the desktop without changing system swap or other workloads. Codex remains in acceptance mode.

### Composed storage and CPU acceptance checkpoint (2026-11-08)

Actual shared CPU source integration now routes Q4_K/Q6_K type-trait dot and NUMA enumeration through Goose. The combined normal-backend corpus matches the original numerical arm exactly,with active Goose witnesses and independent disable switches. Existing CPU provider/control/executor modules are retained. Active Q4_Kx8 AVX2 repack remains assigned and open;this is not APX-013 completion. Evidence: EXP153/results/accepted-cpu-kdot-numa.json.

MODEL and CONTEXT/BATCH/UBATCH now pass a composed boundary corpus using the real Goose record/vector/map/set implementation instead of local C/C++ storage providers. Integration exposed incompatible free/data/map-out declarations,leaked UBATCH child vectors and mismatched published kind/key IDs. Distinct typed ABI adapters and Goose destructor/schema fixes resolve these findings;foundation live allocation/byte counters end at zero. This static composed candidate is not yet the active canonical runtime. Evidence: EXP153/results/canonical-storage-composed.json.

Device IO now passes real component cases after correcting a tensor/count field mixup,double-free/stale-context ownership and aligned buffer-capacity reuse. A native alignment abort for changed writer chunk count is preserved as negative evidence rather than disguised as a Goose failure. Device IO/context integration and the full Apex batch gate remain open. Evidence: EXP153/results/device-io-fixed-summary.json.

No additional model load,full benchmark or performance claim occurred. All Batch A requirements stay IN_PROGRESS. The agent-factory runbook and renamed local skill opencode-subagents describe actual direct dispatch,quotas,permissions,recovery and the observer automation boundary;they do not claim automatic jobpool consumption.

### Canonical IO acceptance addendum (2026-11-08)

Canonical handle IO now executes against the actual GGML backend and real Goose record/vector/set storage. The parent corrected leaked writer entries/reader scratch vectors,incompatible storage ABI,group-record-as-buffer misuse,old-buffer replacement ownership,pending versus committed geometry and read arena sizing. The real repeated/split/changed-writer-geometry corpus passes with zero owned foundation allocations/bytes. No mock result is used to claim this boundary. Evidence: EXP153/results/canonical-io-real.json. Normal context/state adapters and complete error/OOM closure remain open;no APX requirement or batch is closed.

### Active Q4_Kx8 repack integration (2026-11-08)

The shared CPU candidate now source-links Goose Q4_Kx8 weight repacking,Q8_Kx4 activation packing,GEMV and GEMM. Actual CPU_REPACK allocations and normal backend graphs match the native disable arm bit-for-bit,with all four generated paths witnessed. No preload or backend-control disable is used;existing dot/NUMA/provider/control remain linked. The parent made the provider gate strong and cached,checked actual native block layouts,and retained explicit rollback. Evidence: EXP153/results/accepted-cpu-repack.json. Provider/traits/workspace control is still delegated;APX-013 is not closed.

Before replacing the KV object,the audit identified active EXP101 lazy-VMM behavior and a vmm_lazy class field absent from the generic worker snapshot. The actual fork source/header were supplied to the KV owner;canonical memory/CUDA integration must retain them rather than silently regress memory behavior or ABI. No new Apex run or performance claim.

### Unified integration checkpoint (2026-11-08)

The active experimental shared cohort now combines CPU provider/Q4_K/Q6_K/repack/control,GGML/graph/allocator/scheduler,model graph,hybrid/KV/recurrent,state and ordinary context execution with canonical host/device IO adapters. The actual context constructor/host-state comparison passes;context-keyed device IO handles survive repeat/rebind/geometry changes and finish with zero tracked foundation allocations. Root integration repaired the exact ON_DEVICE read-size contract and checked registry ownership publication.

The CUDA numerical packages are now one linked DSO. A real RTX4070 joined graph exposed and then verified repairs for three integration defects:SSM bridge stride signature,Q4 scalar-versus-block byte addressing,and bytewise raw scalar CUDA effects that corrupted F16. The final tested SSM-conv/RMS/fused-GLU/F16/Q4 chain is bit-exact against the preserved native CUDA DSO. Initial Nsight kernel activity independently establishes execution of Goose SSM/RMS/movement paths. This is functional GPU evidence only,not a throughput result or full Apex qualification. The shared runtime aliases select this corrected numerical DSO;the original lazy-VMM CUDA control remains until its separate ABI/ownership repair passes.

Remaining concrete integration repairs are assigned to existing workers14(CUDA context ABI/resource ownership),15(persistent loader ownership;corrected tiny loader/vocab fixture already returned),16(normal llama_vocab facade binding). Ordinary server/canonical storage/source closure and the one full Apex gate also remain. No target is marked DONE from these partial fixtures. Production and compiler sources remain untouched. Exact binaries,hashes,commands and failures are retained in EXP153/results/current-cohort-checkpoint.json and build/cuda-unified/.
