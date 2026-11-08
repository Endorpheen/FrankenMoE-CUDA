# FrankenMoE -> Fried Goose: complete migration plan

Updated: 2026-11-08. Active experimental lineage: restored EXP150 plus verified EXP155..168;current checkpoint recorded in Git and runtime/active.json.

**One shared universal runtime. First finish the complete functional Apex implementation, then extend the same Goose modules to Qwen and other architectures. Production is unchanged.**

**Functional tasks: 160 total; 17 DONE; 3 PARTIAL; 24 IN_PROGRESS; 0 BLOCKED; 116 TODO. Remaining: 143.**

**Current Apex/common milestone: 136 tasks; 15 DONE; 121 remaining (including 3 partial).**

Task count is not code coverage, effort percentage, speed or a completion-date estimate. A bounded completed module does not complete its parent subsystem. Archived history is not proof that all modules run together.

## Current13-stage execution campaign

This supersedes the legacy B0-B8 grouping as the current remaining-work schedule. All original160requirements and acceptance contracts remain unchanged. Earlier12DONE are retained outside the148requirements assigned exactly once below. Stages have unequal sizes because lifecycle/deletion gates must follow their real dependencies. A new experiment number or implemented function is not a completed stage.

| Stage | Remaining-scope group | Requirements | DONE | Left | Gate |
|---|---|---:|---:|---:|---|
|1/13|CPU execution control,expert orchestration and buffer ownership|5|2|3|IN_PROGRESS|
|2/13|Complete CPU math,quantized/repacked MoE and native CPU removal|19|0|19|QUEUED / retain existing partial work|
|3/13|Complete CUDA compute operators|15|1|14|QUEUED / retain existing partial work|
|4/13|CUDA host ownership,device abstraction and CUDA native removal|6|0|6|QUEUED / retain existing partial work|
|5/13|GGML tensor,graph,allocator and unified scheduler|12|2|10|QUEUED / retain existing partial work|
|6/13|KV,recurrent/session memory and complete shared Apex model runtime|18|0|18|QUEUED / retain existing partial work|
|7/13|GGUF,model loader,tokenizer and common text infrastructure|12|0|12|QUEUED / retain existing partial work|
|8/13|Complete server,API,queues and application lifecycle|12|0|12|QUEUED / retain existing partial work|
|9/13|Standalone engine,tools,build,FFI and maintained infrastructure|16|0|16|QUEUED / retain existing partial work|
|10/13|Full native-free Apex integration,inventory and rollback closure|11|0|11|QUEUED / retain existing partial work|
|11/13|Qwen extension in the same universal Goose runtime|6|0|6|QUEUED / retain existing partial work|
|12/13|Remaining models,tokenizers,platforms and device compatibility|12|0|12|QUEUED / retain existing partial work|
|13/13|Final universal closure and genuine100percent completion|4|0|4|QUEUED / retain existing partial work|

Current stage: **1/13**. Completed groups: **0/13**. Original requirements: **17/160 DONE; 143remaining; 10.62%** by requirement count,not code or effort.

Per-component source ownership,correctness,memory safety,liveness and rollback remain mandatory. Share code generation/build/integration and full-system model samples across coherent groups. Keep critical synchronization/allocator/deletion subgates separate internally. Record objective blockers rather than relabeling partial work as full completion. Qwen waits until stage10fullApex. Final100%requires all contracts,source/import closure and no-worse-than-C++ real-system performance evidence.

1. CPU execution control,expert orchestration and buffer ownership: `P1-08, P1-09, P1-10, P2-14, P2-15`. Current pool/job work belongs here. Keep critical synchronization/OOM gates separate internally;math remains an explicit callable provider until stage2. Do not bundle deletion/CPU closure before math.
2. Complete CPU math,quantized/repacked MoE and native CPU removal: `P1-06, P1-07, P1-12, P2-01, P2-02, P2-03, P2-04, P2-05, P2-06, P2-07, P2-08, P2-09, P2-10, P2-11, P2-12, P2-13, P2-16, P2-17, P2-18`. One large coherent compute closure,including all operator/type/precision/ISA source inventory and CPU-only Apex. Final removal depends on all compute and prior control gates;not nineteen independent benchmark cycles.
3. Complete CUDA compute operators: `P3-01, P3-02, P3-03, P3-04, P3-05, P3-06, P3-07, P3-08, P3-09, P3-10, P3-11, P3-12, P3-13, P3-14, P3-15`. Elementwise,quantization,GEMV/GEMM,attention,recurrent/image/training share codegen and one integrated GPU gate. Preserve per-component memory/error guards.
4. CUDA host ownership,device abstraction and CUDA native removal: `P3-16, P3-17, P3-19, P3-20, P4-07, P4-08`. GPU lifetime/transfers/registry form one host subsystem. Close CUDA only after stage3 and actual async/failure validation.
5. GGML tensor,graph,allocator and unified scheduler: `P4-03, P4-04, P4-05, P4-06, P4-09, P4-10, P4-11, P4-12, P4-13, P4-14, P4-15, P4-16`. Tensor/graph invariants,allocator reuse and cross-device schedule have critical internal dependency gates;one common lifecycle integration,not independent architecture rewrites.
6. KV,recurrent/session memory and complete shared Apex model runtime: `P5-01, P5-02, P5-04, P5-05, P5-06, P5-07, P5-08, P5-09, P5-10, P6-01, P6-02, P6-03, P6-04, P6-07, P6-08, P6-09, P6-10, P6-12`. Memory/context/graph-builder/batching/sampling share the same model lifecycle. Preserve source coverage of each component and complete compatibility later.
7. GGUF,model loader,tokenizer and common text infrastructure: `P7-01, P7-02, P7-03, P7-04, P7-05, P7-06, P7-07, P7-08, P7-09, P7-10, P7-11, P7-12`. Use one parser/loader/tokenizer ownership contract and combined malformed-input/text/model gate.
8. Complete server,API,queues and application lifecycle: `P8-01, P8-02, P8-03, P8-04, P8-05, P8-06, P8-07, P8-08, P8-09, P8-10, P8-11, P8-12`. HTTP/SSE/slots/model management/concurrency share one server integration. Keep parsing and long-session safety subgates;remove native server only after all pass.
9. Standalone engine,tools,build,FFI and maintained infrastructure: `P9-01, P9-02, P9-03, P9-04, P9-05, P9-06, P9-07, P9-08, P10-01, P10-02, P10-05, P10-06, P10-07, P10-08, P10-09, P10-11`. Standalone streaming/cache and build/tool infrastructure use one canonical shared runtime;no per-tool model reload. Final residual inventory is stage10.
10. Full native-free Apex integration,inventory and rollback closure: `P0-04, P0-05, P10-12, P11-01, P11-02, P11-03, P11-04, P11-05, P11-06, P11-07, P11-08`. This is the actual full-Apex milestone:all prior group contracts,owned source removal,clean reproducible build,failure recovery and explicit external dependency report. No Qwen work before this gate.
11. Qwen extension in the same universal Goose runtime: `P6-11, C1-02, C1-03, C1-04, C1-05, C1-06`. Only after full functional Apex. Reuse shared components,include deferred MTP,protect Apex;no separate runtime branch.
12. Remaining models,tokenizers,platforms and device compatibility: `P1-11, P3-18, P5-03, P6-05, P6-06, P10-03, P10-04, P10-10, C2-01, C2-02, C2-03, C2-04`. Complete every retained architecture/tool/backend/ISA/multi-GPU contract. Hardware/compiler limitations remain explicit,not silently dropped.
13. Final universal closure and genuine100percent completion: `F1-01, F1-02, F1-03, F1-04`. Final source/import/ABI/model/runtime audit and unavoidable-dependency report only after every prior requirement is proved. Final whole-system performance must be no worse than comparable C++ controls;otherwise completion remains unproven.

## Large delivery cycles over the unchanged13logical stages

Owner requested larger delivery batches;these group all existing13logical stages and unchanged original contracts. They are not new goals,40-50automatic completions,or a promise to suppress necessary critical internal gates. Share expensive integration/model gates where safe;separate ownership/concurrency/hardware dependencies when required.

Current owner rule:after each significant change,perform scoped correctness checks and one fixed Apex32K/128 system gate before continuing integration. During implementation run necessary local correctness/ownership checks on changed code. Reuse unchanged qualified objects/evidence. No repeated full sanitizer/build/model suites without a concrete new correctness or safety reason. Final performance/safety requirements remain unchanged;do not claim stability from this diagnostic policy. Additional full suites/model runs require a concrete correctness/safety failure or material performance concern;routine reassurance is insufficient.

| Cycle | Existing stages | Requirement scope | DONE | Left |
|---|---|---:|---:|---:|
|1/3|1,2,3,4|45|3|42|
|2/3|5,6,7,8|54|2|52|
|3/3|9,10,11,12,13|49|0|49|

Target3large delivery cycles,with a fourth only for a real dependency. Internal critical correctness/ownership/hardware gates remain separate where required;they are not per-function experiments. Expensive builds/model reloads are shared where safe. A partial checkpoint does not complete a cycle or count its unfinished requirements.

## Active isolated candidate

- experiment: `EXP167`,solo continuation from verified EXP166/restored EXP150 lineage.
- root: `/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108`
- branch: `codex/apex-exp150-restored-20261108`
- source: `experiments/EXP-2026-11-08-167-goose-cuda-broadcast/source`
- run: `/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp167/run`
- status:Whole active CUDA binary/broadcast/repeat/fused module and planning are Goose. Native/typed logical GPU oracles,correctness fixes,rollback/preflight and exact Apex pass. Single decode decrease2.22percent is retained,not stable speed parity evidence. Full Apex/CPU/CUDA/model/source closure remains incomplete.
- next_batch:Actual Apex normalization,row routing/reductions,then matrix/attention/recurrent and model/KV residuals.
- previous_runtime_root: `/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/exp166/run`
- rollback: `GGML_GOOSE_CUDA_BROADCAST=0` or immutable EXP166 runtime. EXP166activation/RMS profile retained. Production/compiler untouched,no agents;EXP153 quarantined.

## Authoritative execution policy

1. Follow the current13-stage campaign and its dependency gates;P0-P11/B0-B8remain source/history classifications. Finish full shared/Apex closure before Qwen/MTP(stage11),other compatibility(stage12),and final universal closure(stage13). No separate Apex/Qwen runtimes or parallel projects.
2. The only current real-model development,debug and measurement target is Apex. Qwen source differences are a read-only future reference; do not build,develop,test,benchmark or tune Qwen now. Historical Qwen evidence remains explicitly historical.
3. Transfer whole coherent modules. For every step require source/producer provenance,correctness,actual Goose liveness and rollback. Calling original native computation from a Goose wrapper does not count. Native arithmetic/policy hidden inside ABI glue does not count.
4. After each significant change record one full-system fixed-profile Apex sample: prefill,decode,RAM/VRAM and correctness,compared with previous measured stage and frozen original C++. No microbenchmarks,no per-kernel tuning,no repeated optimization series. Record noise and regressions in local ignored COMPILER_FEEDBACK.md,then continue functional work.
5. Preserve production/default executable,libraries,profiles and compiler source. Use isolated candidate directories. Select latest completed self-hosted Goose producer with receipts at each new stage; do not modify the compiler or interrupt its developer.
6. Keep one canonical common Goose implementation. New architecture-specific code requires a concrete differing semantic contract; prefer metadata/adapters,not copied backends. Preserve Apex correctness and the fixed-profile history during later compatibility extension.
7. Update tasks.json,evidence and this plan at meaningful checkpoints;close delivery cycles with the shared integration/model evidence. Stable task IDs; append discovered missing scope and explain count changes; never erase unfinished work to improve the percentage.
8. Source deletion applies only to the isolated candidate after a verified Goose replacement. Historical native controls/test oracles stay outside candidate links. Production promotion requires separate owner agreement.

## Audit and actual source boundaries

The public FrankenMoE repository is a patch/research-runtime carrier; it has no complete server src/ checkout at its root. Audit therefore covers effective source trees and existing experimental modules,not only carrier Git. All source paths below are relative to the appropriate audited lineage; carrier/ is this repository.

| Audited tree | Files | Lines, including interfaces/vendor/tools | Active translation units |
|---|---:|---:|---:|
|apex|2546|1107530|378|
|qwen|2563|1112365|367|
|carrier|113|38050|0|

Roots and exact hashes: [source-audit.json](docs/goose-migration/source-audit.json). Apex base `1b44e30775dcb2f93952116b05f89e5bd797fb00` plus preserved patches; Qwen source provenance base `839a572a4cdecdb80d211297b8e7d7323a6e99d2`. Copied Qwen tree has no Git metadata; its recorded provenance and per-file hashes are used.

Active translation-unit counts reflect the source prefixes present in recorded compile databases,not every file in a build. Header/tool/vendor line counts are not owned production executable coverage. No file was semantically proven safe merely by inventorying it.

- apex: 101 actual GGML enum operations including NONE (COUNT excluded),35 active type enum names (removed/commented names excluded),166 model .cpp files. Full names recorded in audit.
- qwen: 102 actual GGML enum operations including NONE (COUNT excluded),35 active type enum names (removed/commented names excluded),164 model .cpp files. Full names recorded in audit.

Cross-lineage inventory: 2454 same-path identical files; 89 changed shared paths; 3 Apex-only and 20 Qwen-only paths. These are source differences,not a second development stream.

Source category assignment covers every inventoried file. Per-task source patterns partition behavior and may overlap a large file; a phase is not closed until its residual inventory task proves every relevant branch/entrypoint replaced. Optional upstream backends/architectures stay visible in later compatibility tasks; they are not silently counted as unavoidable dependencies.

## Already migrated, and boundaries still open

| Existing component | Evidence | Remaining native boundary |
|---|---|---|
|Allocation reuse/coalescing; dependency/view lifetime and graph policy|EXP136 historical controls;absent in active EXP160allocator object. EXP161now active full release allocator with exact native/event/Apex proof|Backend bulk-plan/provider,debug/source closure and broader scheduler remain|
|CPU Q8_K/Q8_0; Q4_K/Q6_K raw MoE|EXP137-139 actual inference and exact corpora|Other raw math/repack/threads remain|
|Five additional quantized math families|Archived EXP139 Qwen evidence|Shared integration/precision extension later; speed regression remains|
|Elementwise F32/vector module|EXP138 Apex and scoped checks|Other precisions,GLU/ops and strict NaN payload compatibility open|
|Common MoE controller|EXP140 actual Apex/control corpus|OS services and remaining math/native policy remain|
|Complete13-format AVX2 tiled computation|EXP141 Apex liveness;104graphs per native backend|Other ISA/native sizing/OS services remain; historical Qwen profile did not execute new tiled|
|CUDA RMS row plus SiLU/scale leaves|EXP136-137 actual Apex|Host launch,most CUDA ops,memory/streams/backend still native|

The tiled prefill -20.96% came from a dedicated CPU2048/16 comparison,not the fixed full-system profile below. It is preserved as legacy evidence and in private compiler feedback; it is not mixed into the cumulative series. Native IQ3_XXS fallback UBSan alignment failures and new scoped sanitizer passes are retained without claiming full memory-safety superiority.

## Legacy implementation batches: history and requirement classification

The160tasks are a requirements ledger,not160development cycles. The following former batches remain historical classification;execute the13-stage campaign above. Each batch shares one integration,one consolidated correctness/safety gate and one full-system Apex sample. No per-item research/build/benchmark/report. Internal compiler/build fixes do not create independent experiments. Close dozens of requirements together where technically possible; never mark unimplemented scope DONE merely to reduce iteration count.

| Batch | Requirements | DONE | Remaining | Integrated scope |
|---|---:|---:|---:|---|
|B0|4|4|0|Source audit,immutable reference and fixed Apex history initialization|
|B1|28|6|22|Complete shared CPU MoE and GGML CPU backend|
|B2|19|0|19|Complete CUDA computation,host backend and GPU ownership|
|B3|34|2|32|Unified GGML core,scheduler,memory,KV and Apex model runtime|
|B4|24|0|24|GGUF,loading,text/tokenizer,common layer and full server/API|
|B5|27|0|27|Standalone streaming,infrastructure,ABI and full Apex source/link closure|
|B6|8|2|6|Qwen extension of the same universal runtime after full Apex|
|B7|12|0|12|Remaining architecture/platform compatibility in the same runtime|
|B8|4|0|4|Universal functional closure and unavoidable dependency report|

B0 is audit/reference initialization. B1-B5 are the5large Apex/common implementation batches; B6-B8 extend/close the same universal runtime. Only a genuine dependency or failed correctness/safety gate justifies splitting a batch; preserve every requirement and explain the split. Speed regression is not a reason for a new optimization iteration.

## Stage order and counters

| Phase | Total | DONE | PARTIAL | Remaining | Scope |
|---|---:|---:|---:|---:|---|
|P0|6|4|2|2|Inventory, provenance and migration contracts|
|P1|12|5|0|7|Complete CPU MoE subsystem|
|P2|18|2|1|16|Complete GGML CPU backend|
|P3|20|0|1|20|Complete CUDA backend|
|P4|16|2|0|14|GGML core,allocator,ownership and scheduler|
|P5|10|0|0|10|KV-cache,recurrent memory and sessions|
|P6|12|0|0|12|Shared model runtime and complete Apex architecture|
|P7|12|0|0|12|Shared GGUF/loading/text infrastructure and Apex tokenizer|
|P8|12|0|0|12|Server,HTTP/API,queues and application lifecycle|
|P9|8|0|0|8|Standalone streaming runtime and CLI|
|P10|12|0|0|12|Tools,platforms,build system and dependency closure|
|P11|8|0|0|8|Full Apex functional integration and native-path removal|
|C1|6|1|0|5|Qwen extension of the same Goose runtime, only after full Apex|
|C2|4|0|0|4|Other architectures and platforms using the same runtime|
|F1|4|0|0|4|Universal source/import closure and final functional handoff|

Legacy source family: **B1**;current execution follows stage1control/ownership then stage2CPUmath/closure in the13-stage campaign. P1-06 is a starting source anchor,not an independent experiment. Use Apex native contracts and one combined synthetic/operator corpus; no Qwen implementation or execution. Then B2-B5; compatibility tasks wait until full Apex.

## Fixed Apex end-to-end profile and cumulative history

Authoritative profile/request hashes,argv,library hashes and raw records: [apex-history.json](docs/goose-migration/apex-history.json). One request/sample per stage; no averaging or speed acceptance from a noisy sample. One immutable model,context,placement,batches,thread affinity,KV types,input token IDs,sampling and cache/reset policy. A necessary profile change starts a new series and never silently joins incompatible rows.

Model Apex-35B-V1.0 Q4_K_M; context262144; actual input32768/output128; CPU MoE with GPU attention/dense; nglall,fitoff,FAon; Q4_0K/V; b4096/ub4096;t8/tb16; masks0f0f/ffff strict1,poll0; np1,mmap; cacheRAM0,32checkpoints,specnone; localhost8081; EHSoff. Fresh server/context and cache_prompt=false per arm; normal startup warmup identical; no warmup/functional requests counted in the single measured request. Original and candidate Goose code selections differ,but workload/settings are fixed.

| Stage | Prefill tok/s | Decode tok/s | Delta vs original pp/decode | Delta vs previous pp/decode | RSS peak MiB | Anonymous MiB | VRAM MiB | Swap KiB | Correctness / status |
|---|---:|---:|---|---|---:|---:|---:|---:|---|
|Original C++|2867.85|27.98|+0.00%/+0.00%|pending|21311.26|697.56|3348.00|0.00|exact text/IDs/top5; preliminary single pair; uncontrolled external load|
|EXP141 combined Goose baseline|2892.34|27.53|+0.85%/-1.62%|+0.85%/-1.62%|21318.07|699.55|3348.00|0.00|exact text/IDs/top5; preliminary single pair; uncontrolled external load|
|B1 intermediate diagnostic (254-case candidate)|2620.94|26.74|-8.61%/-4.43%|-9.38%/-2.86%|21313.55|695.59|3348.00|0.00|NOT equivalent: first token divergence at index5; text/IDs/top5 differ; pre-divergence top5 logprobs already differ; Owner-requested one-off diagnostic,not B1 acceptance; historical C++ control,uncontrolled external load; no retry/tuning|
|B1 post-GLU diagnostic (EXP144)|2638.15|27.08|-8.01%/-3.22%|+0.66%/+1.27%|21313.89|696.12|3348.00|0.00|Exact text,all128token IDs and all saved top5/logprobs vs original; sixth-token divergence absent; Owner-requested one-off post-GLU diagnostic,not B1 acceptance; historical controls,uncontrolled external load; no retry/tuning|
|B1 backend API (EXP145)|2608.12|26.46|-9.06%/-5.42%|-1.14%/-2.28%|21313.45|695.57|3348.00|0.00|Exact text,all128tokens and saved top5 vs original and previous B1; new backend API witnessed; One fixed-profile major-module check; historical controls/uncontrolled load; not full B1 or speed acceptance|
|EXP146 B1 backend buffer/traits|2149.11|18.48|-25.06%/-33.95%|-17.60%/-30.16%|21318.63|700.61|3348.00|0.00|Saved response independently equals original text/all128tokens/top5; assert-only buffer hooks are not migration; Historical high-load EXP146; canonical metrics corrected by EXP147 audit; buffer migration and DSO sanitizer claims rejected|
|EXP147 audit: fresh C++|2918.90|29.59|+1.78%/+5.76%|+35.82%/+60.11%|21309.15|693.52|3348.00|0.00|Exact text/all128tokens/saved top5 across all three fresh arms; One sequential fixed-profile request per arm; uncontrolled load; speed parity unproven; no tuning/repeat series|
|EXP147 audit: preserved EXP146|2648.65|28.61|-7.64%/+2.25%|-9.26%/-3.32%|21311.75|695.60|3348.00|0.00|Exact text/all128tokens/saved top5 across all three fresh arms; One sequential fixed-profile request per arm; uncontrolled load; speed parity unproven; no tuning/repeat series|
|EXP147 real Goose buffer policy|2665.51|27.74|-7.06%/-0.86%|+0.64%/-3.04%|21311.89|695.61|3348.00|0.00|Exact text/all128tokens/saved top5 across all three fresh arms; One sequential fixed-profile request per arm; uncontrolled load; speed parity unproven; no tuning/repeat series|
|EXP148 common CPU quantization/type traits batch|2680.11|27.88|-6.55%/-0.36%|+0.55%/+0.50%|21312.62|696.17|3348.00|0.00|Exact text/all128tokens/top5 vs fresh C++ and EXP147;172640new-module Q8_K blocks witnessed; One request after combined three-component batch;uncontrolled load,no speed parity claim or tuning|
|EXP149 CPU job/pool omp|2646.56|28.46|-7.72%/+1.73%|-1.25%/+2.10%|21311.80|695.55|3348.00|0.00|Exact text,all128IDs and saved top5 vs original C++;Goose pool/job/executor traced; One request per distinct OMP/pthread path,uncontrolled external load;not paired stable speed acceptance|
|EXP149 CPU job/pool pthread|2669.81|26.42|-6.91%/-5.55%|+0.88%/-7.16%|21313.31|697.43|3348.00|0.00|Exact text,all128IDs and saved top5 vs original C++;Goose pool/job/executor traced; One request per distinct OMP/pthread path,uncontrolled external load;not paired stable speed acceptance|
|EXP150 CPU registry/shared expert policy|2656.29|28.08|-7.38%/+0.36%|-0.51%/+6.26%|21312.09|696.29|3348.00|0.00|Exact text/all128IDs/top5 vs original;new registry modes0-9 and127680scratch queries traced. Cache/HBM absent from this profile and checked separately.; One diagnostic under external load,saved C++ comparison,not fresh repeated paired/stable no-regression acceptance|

RAM columns separate total process RSS,file-backed model pages and anonymous allocation where raw samples permit; RSS is not a pure ownership/allocation metric. VRAM is process memory,not total desktop GPU usage. Record process swap/system swapout,external load,cold/warm conditions and library/preload hashes. Liveness counters identify actual executed Goose paths; no claim that every implementation is exercised by Apex.

## Detailed measurable task ledger

Status: TODO=not started; IN_PROGRESS=implementation active; PARTIAL=bounded evidence exists but whole task criteria not met; BLOCKED=explicit dependency prevents progress; DONE=functional criteria plus evidence satisfied. Performance acceptance is separate. Every major Apex/common completion adds one whole-system row above; operation correctness fixtures do not become microbenchmarks.

### First: shared runtime and complete Apex

#### P0 — Inventory, provenance and migration contracts

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P0-01 / **DONE**|Preserve immutable native controls and production rollback|reference runtime manifests; EXP136..141 provenance|Frozen executable/library/model hashes and commands; production hashes unchanged. Evidence: EXP141/results/reference-integrity-exp141.json; EXP136..141|
|P0-02 / **DONE**|Audit all effective source trees and enumerate remaining scope|apex/**; qwen/**; carrier/runtime/**; carrier/patches/**|Hash/line inventory for both lineages, active translation units, operation/type/model registries and every file assigned to a subsystem. Evidence: docs/goose-migration/source-audit.json|
|P0-03 / **DONE**|Qualify current self-hosted producer per new stage|compiler receipts; source/*/generation-command.json|Pin latest completed self-hosted producer/receipt at stage entry; generated-C provenance recorded; compiler source untouched. Evidence: EXP141/compiler/producer-receipt.json; revalidate at every subsequent stage|
|P0-04 / **PARTIAL**|Replace manual experimental ABI scaffolding with audited module interfaces|source/*/bridge.h; source/*/unit.c|All shared types have size/offset/import contracts; no implicit declarations; primitive imports separately inventoried. Evidence: Per-experiment static ABI assertions exist; unified import ledger not yet complete|
|P0-05 / **PARTIAL**|Maintain complete differential and liveness coverage registry|tests/**; tools/server/tests/**; source/*/*fixture*|Every supported operation/type/mode has an executable check and witness; Apex main, Qwen at milestones; unsupported coverage explicit. Evidence: Existing exact graph/model corpora cover selected modules only|
|P0-06 / **DONE**|Preserve isolated builds, feature switches and source rollback|variants/**; experiments/EXP-2026-11-07-*/tools/reproduce_local.py|All migrated modules can be disabled and replayed from pinned sources without replacing production. Evidence: EXP141 snapshot rebuild:8objects,104graphs per backend; previous snapshots retained|

#### P1 — Complete CPU MoE subsystem

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P1-01 / **DONE**|Port common Q8_K/Q8_0 activation quantization|ggml/src/ggml-cpu/quants.c; ggml/src/ggml-cpu/arch/*/quants.c|Whole row quantizers match native bytes and are witnessed in actual Apex/Qwen inference. Evidence: EXP137 Q8_K; EXP139 common Q8_0/Q8_K exact and model traces; Q8_0 performance deferred EXP148 routes shared MoE Q8_K/Q8_0 through the same checked encoder module;new default-on flag has explicit disable rollback,841retained activation cases pass.|
|P1-02 / **DONE**|Port raw Q4_K/Q6_K matrix and routed-expert arithmetic|ggml/src/ggml-cpu/arch/x86/quants.c; source/cpu-moe/**|Dot/matrix/routed outputs match preserved precision contracts; actual Apex executes Goose. Evidence: EXP137..138 CPU MoE and Apex traces|
|P1-04 / **DONE**|Port entire13-format AVX2 tiled-prefill computation|ggml/src/ggml-cpu/tiled/**; source/cpu-tiled-all/**|All13unpack/gather/micro/dense/MoE stages exact, Apex execution witnessed, Qwen integration checked, original fallback retained. Evidence: EXP141:104graphs/backend; actual Apex new-path execution; Qwen profile selects raw path|
|P1-05 / **DONE**|Port common expert controller and checked workspace policy|ggml/src/ggml-cpu/ggml-cpu.c; ggml/src/ggml-cpu/ggml-cpu-mul-mat-id-cold.c; source/moe-control/**|Grouping,strided IDs,cold masks,bank maps,quant partitions,fused windows and tile/chunk policy exact in both lineages. Evidence: EXP140: Apex114/Qwen412graphs; exact actual models; native OS/compute services remain|
|P1-06 / **IN_PROGRESS**|Replace every remaining raw CPU quantized MoE math family|ggml/src/ggml-cpu/quants.c; ggml/src/ggml-cpu/arch/*/quants.c; ggml/src/ggml-quants.c|Every active common CPU dot/type-trait family absent from P1-02/03 has Goose dot/dequant/routed tests; zero calls to replaced native arithmetic; cover K/IQ/TQ/FP4/legacy types using Apex native precision contracts. Other architecture-specific precision differences stay in later compatibility adapters. Evidence: EXP143 B1: common23-format public dequantization matches actual GGML CPY fixtures; remaining raw dot/routed families and type/layout closure incomplete.|
|P1-07 / **TODO**|Replace repacked expert matrices and all packing layouts|ggml/src/ggml-cpu/repack.cpp; ggml/src/ggml-cpu/arch/*/repack.cpp; ggml/src/ggml-cpu/traits.cpp|Each registered pack/trait layout round-trips and matmul matches native; raw/repack/fused dispatch liveness and tails covered.|
|P1-08 / **TODO**|Replace fused and cold expert execution orchestration|ggml/src/ggml-cpu/ggml-cpu-mul-mat-id-cold.c; ggml/src/ggml-cpu/ggml-cpu.c|Complete stock/cold/fused paths execute Goose; masks,bank remapping,shared experts,ready hooks and fallback tested; no hidden original executor.|
|P1-09 / **IN_PROGRESS**|Move expert work distribution and synchronization ownership to Goose|ggml/src/ggml-cpu/ggml-cpu.c; ggml/src/ggml-threading.*|Goose owns partitioning,job queues,claims,barrier state,cancellation; only primitive OS atomics/thread waits imported;1/2/8/16thread deterministic outputs and termination. Evidence: EXP143 Goose active OpenMP graph-job lifecycle and worker publication/barrier/result policy;326graph/status cases include1/2/8/16thread persistent pool,cancellation/recovery. Physical allocation/init,OS placement,expert chunk claims and non-OpenMP implementation remain native/incomplete. EXP144 actual fixed Apex request witnesses OpenMP job/controller execution with exact original response; no full non-OpenMP/expert-work-queue closure claim.|
|P1-10 / **IN_PROGRESS**|Move MoE buffers, residency and output assembly ownership to Goose|ggml/src/ggml-cpu/ggml-cpu.c; src/llama-expert-*.cpp; src/llama-moe-cache.cpp|Expert scratch/resident/staging lifecycles and scatter owned by Goose; allocation failure,resize,empty/overlapping routes and cancellations checked. Evidence: EXP145 moves backend graph scratch growth/reuse/failure policy into Goose,with real API/Apex and allocation-policy tests. This does not yet replace complete expert residency/staging/bank ownership. EXP150 moves all local expert tiled scratch queries to shared Goose policy,complete SLRU/eligibility/read-span/upload-grouping and bank budget/slot planning to Goose.30296native differential cases and scoped sanitizers pass. Cache constructor/tensor allocation/full upload-copy/resource lifecycle and remaining expert staging ownership are not complete;no full closure. Fixed Apex has no model-cache allocations,so only workspace execution is claimed at real inference.|
|P1-12 / **TODO**|Close CPU MoE subsystem with native compute disabled|ggml/src/ggml-cpu/**; src/llama-expert-*|All supported MoE formats/modes pass graph corpus and real Apex milestone; replaced native symbols unavailable in candidate link; rollback preserved externally. Depends: P1-06,P1-07,P1-08,P1-09,P1-10.|

#### P2 — Complete GGML CPU backend

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P2-01 / **PARTIAL**|Finish complete binary,unary and GLU compute module|ggml/src/ggml-cpu/binary-ops.cpp; ggml/src/ggml-cpu/unary-ops.cpp; ggml/src/ggml-cpu/ops.cpp|Every binary/unary/GLU variant,type,stride and special-value contract implemented; NaN payload discrepancy resolved or explicit compatibility contract agreed. Evidence: EXP138 plus EXP143 selected scalar/binary/unary/GLU. Original AVX2 SWIGLU math now Goose,9/9wide cases byte-exact;signed-zero clamp/OAI selection fixed. EXP144 one fixed Apex32768/128 request exactly matches original text/IDs/all saved top5. Full type/stride/NaN payload/ISA closure remains incomplete.|
|P2-02 / **IN_PROGRESS**|Port tensor copy,conversion and set/contiguous operations|ggml/src/ggml-cpu/ops.cpp; ggml/src/ggml-cpu/vec.cpp|DUP,CPY,CONT,SET,ACC,F32/F16/BF16/integer/quantized copy paths exact for strided,padded,aliasing and empty tensors. Evidence: EXP143 actual GGML scalar copy/CPY plus23 supported public quantized dequant formats pass current composite subset and scoped sanitizers; full conversion/set/type contracts incomplete.|
|P2-03 / **IN_PROGRESS**|Port scalar/vector primitives and numerical conversions|ggml/src/ggml-cpu/vec.cpp; ggml/src/ggml-cpu/simd-mappings.h; ggml/src/ggml-cpu/simd-gemm.h|Every exported CPU vector/conversion symbol mapped to Goose; scalar/SIMD lane reduction,rounding and exceptional-value cases checked. Evidence: EXP143 checked tensor/byte storage,half/BF16/bit conversion and native precision-shaped vector primitives integrated; full primitive/type/special-value closure incomplete.|
|P2-04 / **IN_PROGRESS**|Port dense matmul,out-product and batched GEMM|ggml/src/ggml-cpu/ggml-cpu.c; ggml/src/ggml-cpu/llamafile/sgemm.cpp; ggml/src/ggml-blas/**|F32/F16/BF16/quantized,broadcast,4D,tails and precision modes match; native owned GEMM logic removed, optional BLAS explicit imported primitive. Evidence: EXP143 F32 dense graph subset passes; other float/quantized dense/out-product paths and all precision/layout contracts incomplete.|
|P2-05 / **IN_PROGRESS**|Port reductions,scans,comparison and sorting|ggml/src/ggml-cpu/ops.cpp|SUM,SUM_ROWS,CUMSUM,MEAN,ARGMAX,COUNT_EQUAL,ARGSORT,TOP_K match including stable tie/NaN/empty behavior. Evidence: EXP143 reductions,scans,comparison/sort source integrated; tested reduction subset passes. Full type/stride/special-value closure incomplete.|
|P2-06 / **IN_PROGRESS**|Port normalization and normalization gradients|ggml/src/ggml-cpu/ops.cpp|NORM,RMS_NORM,GROUP_NORM,L2_NORM and registered backward variants match axes,epsilon,strides and exceptional inputs. Evidence: EXP143 normalization subset plus RMS+MUL fused executor cases pass; complete normalization/gradient/type closure incomplete.|
|P2-07 / **IN_PROGRESS**|Port softmax,masks and RoPE subsystem|ggml/src/ggml-cpu/ops.cpp|SOFT_MAX/back,diag masks,ROPE/back and all registered position/scaling modes match both graph corpus and model attention outputs. Evidence: EXP143 basic softmax plus normal/NeoX F32 RoPE graph cases pass. Other masks/types/modes/backward and full subsystem gate incomplete.|
|P2-08 / **IN_PROGRESS**|Port gather,scatter,repetition and tensor transforms|ggml/src/ggml-cpu/ops.cpp|GET/SET_ROWS,backward,REPEAT/back,CONCAT,DIAG,PAD/reflect,ROLL,ARANGE,FILL,TRI,embedding and layout operations covered. Evidence: EXP143 gather/repeat/concat/pad/roll actual GGML subset passes; complete scatter/index/stride/type contracts incomplete.|
|P2-09 / **TODO**|Port complete CPU Flash Attention subsystem|ggml/src/ggml-cpu/ops.cpp; ggml/src/ggml-cpu/common.h|All supported cache dtypes,masks,head/group counts,quantized KV,tile/tail and forward/back modes execute Goose; Apex attention trace.|
|P2-10 / **IN_PROGRESS**|Port recurrent,SSM and gated attention computations|ggml/src/ggml-cpu/ops.cpp|SSM_CONV/SCAN,RWKV6/7,GLA,GATED_DELTA_NET,solve-tri,lightning-indexer and DSV4HC supported variants match stateful multi-token corpus. Evidence: EXP143 common GDN/KDA,SSM scan,RWKV6/7 and gated linear attention integrated:52 recurrent cases inside278-case composite and scoped sanitizer suite pass. Whole Apex equivalence remains open; no stateful Goose model witness in diagnostic GPU profile; other recurrent scope incomplete.|
|P2-11 / **TODO**|Port convolution,image and relative-position operations|ggml/src/ggml-cpu/ops.cpp|All registered1D/2D/3Dconv/im2col/col2im,pooling/upscale,window partition and relative-position modes have differential fixtures.|
|P2-12 / **TODO**|Port training,custom and miscellaneous CPU operators|ggml/src/ggml-cpu/ops.cpp; ggml/src/ggml-opt.cpp|Every residual GGML op enum has Goose support/identical unsupported error; loss/backward/AdamW/SGD/custom-callback contracts checked.|
|P2-13 / **IN_PROGRESS**|Port remaining quantization/serialization math and type traits|ggml/src/ggml-quants.c; ggml/src/ggml-cpu/quants.c; ggml/src/ggml-cpu/traits.cpp|Every non-removed type registry entry has Goose quant/dequant/traits where native supports it; IQ calibration/tables and Q8_1 included; exact size/layout preserved. Evidence: EXP143 actual GGML CPY covers23 native-supported public dequant types. Q8_1/Q8_K have no native public to_float trait; no fabricated oracle. Remaining quantizers/calibration/traits incomplete. EXP148 combines nine CPU encoders and actual from_float trait dispatch:Q1_0,Q2_0,Q4_0,Q4_1,Q5_0,Q5_1,Q8_1 plus existing Goose Q8_0/Q8_K.893native-byte/canary/concurrency+rollback cases and841Q8special contracts pass release and scoped ASan/UBSan/LSan;all prior326/66/960/174gates preserved;Apex32K/128 exact with172640Q8_K blocks.Compact nonfinite fallback,full float-domain/other formats/ISA/serialization remain unclosed.|
|P2-14 / **DONE**|Port CPU backend device/buffer/capability registry|ggml/src/ggml-cpu/ggml-cpu.cpp; ggml/src/ggml-cpu/hbm.cpp; ggml/src/ggml-cpu/traits.cpp|Backend API,buffer alignment/copy,capability predicates and device initialization owned by Goose; OS/HBM primitive imports audited. Evidence: EXP150 common CPU backend/device/buffer/capability/registry functional ownership closed:inherited real Goose API/buffer/traits plus complete registry/device initialization/properties/brand sequencing and HBM allocation/error/wrapper cleanup.210registry/OS/HBM-policy cases and5compiled HBM API/stub cases;30296cache/budget cases,actual326/66/960/174GGML corpus,component disable rollback and scoped ASan/UBSan/LSan pass. Fixed Apex32K/128 exact text/allIDs/top5;registry0-9 and127680workspace calls witnessed;RSS21312.090MiB/VRAM3348MiB. HBM actual hardware and other OS execution unqualified;architecture/provider math/ABI storage/native removal remain explicitly assigned P1-07/P2-03/P2-13/P2-16/P2-18/P10-10. Early copied-library alias gate invalidated and corrected before model.2656.285pp/28.079tg diagnostic vs saved C++2918.900/29.589;not paired speed/no-regression acceptance.|
|P2-15 / **DONE**|Port CPU graph executor,task planning and abort paths|ggml/src/ggml-cpu/ggml-cpu.c|Entire operation dispatch,n_tasks/workspace planning,graph reuse,threadpool job lifecycle and abort status execute Goose; all operator entries traced. Evidence: EXP149 original P2-15 acceptance closed:all101dispatcher entries/96callbacks checked with independent unchanged-native-switch oracle;960task/workspace cases exact;Goose owns graph-wide planning/tiled sizing,executor reuse/abort,OMP+pthreads job lifecycle,physical pool init/cleanup,masks,barrier/chunk/readiness/poll/pause/kickoff/NUMA/priority policy.326/66/174actual GGML/common cases pass both implementations;78OMP/81pthread real lifecycle/graph failure cases,494policy boundary cases,release+ASan/UBSan/LSan,and scoped new-control pthread TSan pass. Exactly one Apex32768/128 per mode,text/allIDs/top5 exact vs C++;new pool/job paths witnessed. Complete source boundary audit maps numerical/provider/ABI/topology adapters to other open requirements;not fullCPU/native-free/safety-superiority/speed acceptance. P1-09/P2-14 and other stage1requirements remain open. C++2918.900/29.589;OMP2646.558/28.462;pthread2669.811/26.424tok/s are preliminary single samples,not stable performance parity.|
|P2-16 / **TODO**|Close CPU architecture support and residual source inventory|ggml/src/ggml-cpu/**; ggml/src/ggml-blas/**|All CPU audit rows assigned and zero owned native executable logic remains in candidate; hardware-unavailable variants stay explicitly incomplete.|
|P2-17 / **TODO**|Run CPU-only functional milestone across model families|tests/test-backend-ops.cpp; src/models/**|Full supported operator/type matrix plus Apex and tiny representative dense,recurrent,vision graph fixtures pass; witnesses prove Goose, not fallback.|
|P2-18 / **TODO**|Remove replaced CPU implementation from candidate production link|ggml/src/ggml-cpu/**|Candidate link map contains only Goose-emitted CPU logic plus audited primitives; frozen original remains outside candidate for rollback. Depends: P2-01,P2-02,P2-03,P2-04,P2-05,P2-06,P2-07,P2-08,P2-09,P2-10,P2-11,P2-12,P2-13,P2-14,P2-15,P2-16,P2-17.|

#### P3 — Complete CUDA backend

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P3-01 / **DONE**|Finish complete CUDA normalization/elementwise module|ggml/src/ggml-cuda/norm.cu; ggml/src/ggml-cuda/unary.cu; ggml/src/ggml-cuda/scale.cu; source/rms-block/**|All norm/unary/scale variants and launch policy execute Goose. Joint EXP166/168:40activation/GLU/scale/clamp and8normalization paths;native/reference/rollback/preflight and exact Apex gate pass. Final native-source removal and stable fresh-C++ performance are separate open requirements.|
|P3-02 / **IN_PROGRESS**|Port CUDA primitive math,reductions and conversions|ggml/src/ggml-cuda/common.cuh; ggml/src/ggml-cuda/convert.cu; ggml/src/ggml-cuda/reduce_rows.cuh; ggml/src/ggml-cuda/mma.cuh|Goose-owned device math/conversion/warp/block helpers; generated device code provenance; rounding,half,bfloat,warp boundaries checked.|
|P3-03 / **TODO**|Port CUDA copy,gather,scatter and tensor transformations|ggml/src/ggml-cuda/cpy*; ggml/src/ggml-cuda/getrows*; ggml/src/ggml-cuda/set*; ggml/src/ggml-cuda/pad*; ggml/src/ggml-cuda/concat*; ggml/src/ggml-cuda/roll*|All dtype/stride/tail variants match native and Compute Sanitizer bounds checks; asynchronous copies preserve order.|
|P3-04 / **IN_PROGRESS**|Port CUDA binary,broadcast and activation families|ggml/src/ggml-cuda/binbcast*; ggml/src/ggml-cuda/unary*; ggml/src/ggml-cuda/clamp*; ggml/src/ggml-cuda/snake*; ggml/src/ggml-cuda/softcap*; ggml/src/ggml-cuda/acc*; ggml/src/ggml-cuda/add-id*|Complete registered binary/unary/GLU type/mode matrix; all actual kernels generated from Goose, no original arithmetic callback.|
|P3-05 / **TODO**|Port CUDA reductions,sort,top-k and expert routing|ggml/src/ggml-cuda/sum*; ggml/src/ggml-cuda/mean*; ggml/src/ggml-cuda/arg*; ggml/src/ggml-cuda/top*; ggml/src/ggml-cuda/cumsum*; ggml/src/ggml-cuda/count*|Every reduction/sort/topk MoE mode,tie/index rule and partial block checked; routing witnesses on real model.|
|P3-06 / **TODO**|Port CUDA softmax,RoPE,masks and position encodings|ggml/src/ggml-cuda/softmax*; ggml/src/ggml-cuda/rope*; ggml/src/ggml-cuda/diag*; ggml/src/ggml-cuda/tsembd*; ggml/src/ggml-cuda/arange*|All position/mask/scaling/attention-shape variants differential; valid finite tolerance fixed before candidate test.|
|P3-07 / **TODO**|Port CUDA dequantization and activation quantization|ggml/src/ggml-cuda/dequantize.cuh; ggml/src/ggml-cuda/quantize*; ggml/src/ggml-cuda/vecdotq.cuh|Every supported type and packed block path mapped to Goose with byte/ULP fixtures and malformed-size rejection.|
|P3-08 / **TODO**|Port full CUDA quantized matrix-vector decode|ggml/src/ggml-cuda/mmvq*; ggml/src/ggml-cuda/vecdotq.cuh; ggml/src/ggml-cuda/template-instances/**|Every registered MMVQ format/instance,expert routing,split K,tail and batch mode executes Goose; Apex decode witnesses.|
|P3-09 / **TODO**|Port full CUDA quantized matrix-matrix prefill|ggml/src/ggml-cuda/mmq*; ggml/src/ggml-cuda/template-instances/**|All MMQ tile loads,accumulation,quant formats,configurations and tails generated from Goose; no native math fallback in candidate.|
|P3-10 / **TODO**|Port dense CUDA GEMM/vector and cuBLAS dispatch policy|ggml/src/ggml-cuda/mmf*; ggml/src/ggml-cuda/mmvf*; ggml/src/ggml-cuda/out-prod*|Dense kernels and shape/precision/dispatch policy Goose; cuBLAS allowed only audited external primitive with all owned orchestration ported.|
|P3-11 / **TODO**|Port CUDA MoE expert matrix,weighted reduction and fused path|ggml/src/ggml-cuda/mmid*; ggml/src/ggml-cuda/moe-weighted-reduction*; ggml/src/ggml-cuda/topk-moe*|All expert grouping,weighted gather/scatter/fusion and ready/stream ordering owned by Goose; masks and cold experts tested.|
|P3-12 / **TODO**|Port complete CUDA Flash Attention|ggml/src/ggml-cuda/fattn*; ggml/src/ggml-cuda/template-instances/**|Tile/vector/MMA paths,all supported cache types,mask/alibi/heads/tails and dispatch covered; actual Apex attention witnesses.|
|P3-13 / **TODO**|Port CUDA recurrent,SSM,gated-delta and sparse indexing|ggml/src/ggml-cuda/ssm*; ggml/src/ggml-cuda/gated*; ggml/src/ggml-cuda/gla*; ggml/src/ggml-cuda/wkv*; ggml/src/ggml-cuda/solve*; ggml/src/ggml-cuda/lightning*; ggml/src/ggml-cuda/dsv4*|Multi-step stateful outputs match for each registered path; state ownership and concurrent stream ordering checked.|
|P3-14 / **TODO**|Port CUDA convolution,image,pooling and remaining math|ggml/src/ggml-cuda/conv*; ggml/src/ggml-cuda/im2col*; ggml/src/ggml-cuda/col2im*; ggml/src/ggml-cuda/pool*; ggml/src/ggml-cuda/upscale*; ggml/src/ggml-cuda/fwht*|Every remaining inference op has Goose device implementation and shape/type corpus; inventory has no unassigned kernel.|
|P3-15 / **TODO**|Port CUDA backward,optimizer and residual operators|ggml/src/ggml-cuda/cross*; ggml/src/ggml-cuda/opt*; ggml/src/ggml-cuda/fill*; ggml/src/ggml-cuda/tri*; ggml/src/ggml-cuda/**|All residual registered enum/dispatch branches implemented or same native unsupported error; no silent deletion of supported capability.|
|P3-16 / **IN_PROGRESS**|Port CUDA host backend,devices and capability predicates|ggml/src/ggml-cuda/ggml-cuda.cu|Goose owns device selection,op dispatch,events,streams,capability checks,error propagation and graph capture/replay; CUDA SDK calls primitive imports only. EXP160 shared Goose fusion/params/hash policy integrated in core,scheduler,meta,CPU core and exact active CUDA-host lineage.714native predicate comparisons/1655checks,two rollback modes and15malformed-input cases pass. One Apex2636.702/27.185,exact text/all128IDs/full probabilities,127graph reuses,core/CPU/CUDA/scheduler witnesses,meta not exercised. Decode-2.44percent single sample vs EXP159;stable performance unproven. Other leaf header copies,numerical kernels,device/launch and full subsystem/source closure remain open.|
|P3-17 / **TODO**|Port GPU/pinned/VMM pools and transfer/residency policy|ggml/src/ggml-cuda/ggml-cuda.cu; src/llama-expert-*.cpp|All allocation/pool/pinned ring/VMM/residency/copy decisions Goose; OOM,resize,reuse,cancel,device reset and asynchronous lifetimes checked.|
|P3-19 / **TODO**|Run whole CUDA backend functional/sanitizer milestone|tests/test-backend-ops.cpp; ggml/src/ggml-cuda/**|Supported matrix passes differential checks and Compute Sanitizer memcheck/racecheck/initcheck/synccheck where applicable; Apex CPU/GPU mixed paths execute Goose.|
|P3-20 / **TODO**|Remove replaced CUDA source logic from candidate link|ggml/src/ggml-cuda/**|Zero owned hand-written CUDA/C++ computation/orchestration in selected link; generated CUDA/C from Goose allowed and provenance checked. Depends: P3-01,P3-02,P3-03,P3-04,P3-05,P3-06,P3-07,P3-08,P3-09,P3-10,P3-11,P3-12,P3-13,P3-14,P3-15,P3-16,P3-17,P3-19.|

#### P4 — GGML core,allocator,ownership and scheduler

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P4-01 / **DONE**|Port allocation reuse/free-list/coalescing policy|ggml/src/ggml-alloc.c; source/allocator-reuse/**; source/allocator-coalesce/**|Bounded selected metadata policy exactly matches reference and executes in real inference. Evidence: EXP136 native service boundary retained; this does NOT mean physical allocator complete|
|P4-02 / **DONE**|Port tensor dependency/view lifetime and allocation-graph policy|ggml/src/ggml-alloc.c; source/tensor-lifetime/**; source/graph-plan/**|Selected dependency/view/refcount/release traversal state transitions exact and real inference witness. Evidence: EXP136 graph/lifetime evidence; full tensor ownership/scheduler still native|
|P4-03 / **IN_PROGRESS**|Port GGML tensor/context/object/type core|ggml/src/ggml.c; ggml/include/ggml.h; ggml/src/ggml.cpp|All public tensor/context constructors,shape/stride/view/alias invariants,type/operation tables and errors handled by Goose; ABI preserved. Evidence:EXP155 geometry and EXP156 arena/tensor/view/reshape/permute/traversal execute actual Goose. Independent native/rollback35-format corpus,canaries,cross-context borrowed payloads and failure transactions pass. Fixed Apex2639.870/27.247,exact text/all128IDs/all saved top5,127graph reuses. EXP157 adds actual Goose context init/reset/free,owned/borrowed allocation/error policy,active type/op/status registries and core flags/names/accessors.4500native/rollback checks,256threaded context cycles,allocation failure cleanup and inherited429444constructor checks pass. One Apex2638.849/27.574,exact text/all128IDs/all saved probabilities,127graph reuses;live context heaps0. EXP158 transfers65common arithmetic/MoE/attention/recurrent graph-builder/settings bodies with10408independent checks/all36modes,rollback and exact real Apex. Sequential EXP157->EXP158 follow-up gives2612.899/27.668->2634.815/28.055;initial candidate2639.982/26.596retained. Remaining graph/hash/traversal/lifecycle,additional parameter helpers and source closure stay open. EXP160 shared Goose fusion/params/hash policy integrated in core,scheduler,meta,CPU core and exact active CUDA-host lineage.714native predicate comparisons/1655checks,two rollback modes and15malformed-input cases pass. One Apex2636.702/27.185,exact text/all128IDs/full probabilities,127graph reuses,core/CPU/CUDA/scheduler witnesses,meta not exercised. Decode-2.44percent single sample vs EXP159;stable performance unproven. Other leaf header copies,numerical kernels,device/launch and full subsystem/source closure remain open.|
|P4-04 / **IN_PROGRESS**|Port graph construction,hashing,traversal and differentiation|ggml/src/ggml.c; ggml/src/ggml-opt.cpp|All graph expansion/copy/export/gradient/optimizer entrypoints mapped; cycles,shared views,empty graphs and malformed dimensions tested. Evidence:EXP159 transfers core hash/bitset policy,checked graph layout,iterative forward traversal/order/select,copy/dup/borrowed views/clear and lookups.93393candidate/92907rollback checks,12000depth,9error cleanup and256threaded independent graph lifecycles pass. Final Apex2635.438/27.865,exact text/all128IDs/full probabilities,127graph reuses,Goose witness and0graph heaps. Final C bridge uses memcpy after pre-commit aliasing correction. Other-TU inlines,core fusion,training/autodiff/optimizer/export and final source closure remain unclosed. EXP160 shared Goose fusion/params/hash policy integrated in core,scheduler,meta,CPU core and exact active CUDA-host lineage.714native predicate comparisons/1655checks,two rollback modes and15malformed-input cases pass. One Apex2636.702/27.185,exact text/all128IDs/full probabilities,127graph reuses,core/CPU/CUDA/scheduler witnesses,meta not exercised. Decode-2.44percent single sample vs EXP159;stable performance unproven. Other leaf header copies,numerical kernels,device/launch and full subsystem/source closure remain open.|
|P4-05 / **IN_PROGRESS**|Port full tensor allocator and physical buffer ownership|ggml/src/ggml-alloc.c|Goose owns tallocr/dyn_tallocr/free blocks/chunks and object lifecycle; only malloc/mmap/device alloc primitives imported; failure/overflow/double-free guards. Evidence:EXP161 full release ggml-alloc.c policy now Goose across42bodies:chunk/free-list/coalescing,tallocr/vbuffer,gallocr reserve/reuse/views/lifetime/ownership and context collection.10800dynamic operations,12graphs,495physical backend events/17576checks plus rollback,7creation OOM/6reserve fatal/1recoverable failure and128threaded graph lifecycles pass. One Apex2633.888/27.425,exact text/all128IDs/full probabilities,127graph reuses,allocator witness0xb18070000c and0outstanding heaps. Backend bulk-plan/provider internals,debug diagnostics and strict source closure remain separate/open.|
|P4-06 / **IN_PROGRESS**|Port graph allocator reservation,reuse and realloc|ggml/src/ggml-alloc.c|Goose owns gallocr reservation,multi-buffer planning,views,inplace reuse and growth; native allocation state unavailable in candidate; exact event/order corpus. Evidence:EXP161 full release ggml-alloc.c policy now Goose across42bodies:chunk/free-list/coalescing,tallocr/vbuffer,gallocr reserve/reuse/views/lifetime/ownership and context collection.10800dynamic operations,12graphs,495physical backend events/17576checks plus rollback,7creation OOM/6reserve fatal/1recoverable failure and128threaded graph lifecycles pass. One Apex2633.888/27.425,exact text/all128IDs/full probabilities,127graph reuses,allocator witness0xb18070000c and0outstanding heaps. Backend bulk-plan/provider internals,debug diagnostics and strict source closure remain separate/open.|
|P4-07 / **IN_PROGRESS**|Port backend buffer,device,event and copy abstraction|ggml/src/ggml-backend.cpp; ggml/src/ggml-backend-impl.h|Every backend public/vtable lifecycle,view,multi-buffer,async copy/event/sync contract implemented; typed ownership explicit. Evidence:EXP162 transfers30common backend buffer/type/default bulk/multi/view/tensor-binding bodies to Goose with an explicit C/CPP ABI. Native-success/event parity,243callback contracts,12bulk+3multi OOM sites,2bad_alloc callbacks and128threaded bulk lifecycles pass. Corrects independently reproduced native stale metadata in5of6provider failures via snapshots/rollback. One Apex2639.393/27.387,exact text/all128IDs/all probabilities,127graph reuses,actual internal bulk-plan witness/zero buffer heaps. Provider-specific/meta and broader copy/event/device/scheduler/source closure remain open. Evidence:EXP163 transfers48generic backend/stream/I-O/copy/event/device/registration-interface bodies to Goose,with typed C/CPP ABI and unchanged provider/kernel objects.37native case groups/264ordered callback events/1091checks cover all48entry modes and rollback;11bounds/staging/callback-error cases and128threaded transfer/event lifecycles pass. One Apex2631.942/26.956,exact text/all128IDs/all probabilities,127graph reuses,actual shared-control witness and0staging/buffer/context heaps. Decode-1.57percent single sample;no stable speed claim. Actual scheduler/global registry/provider implementations and full source closure remain open.|
|P4-08 / **IN_PROGRESS**|Port backend registration,dynamic loading and feature discovery|ggml/src/ggml-backend-reg.cpp; ggml/src/ggml-backend-dl.cpp; ggml/src/ggml-feats.h|Registry/device enumeration,plugin ABI,error behavior and capability queries Goose; dlopen/OS imports only. Evidence:EXP163 transfers48generic backend/stream/I-O/copy/event/device/registration-interface bodies to Goose,with typed C/CPP ABI and unchanged provider/kernel objects.37native case groups/264ordered callback events/1091checks cover all48entry modes and rollback;11bounds/staging/callback-error cases and128threaded transfer/event lifecycles pass. One Apex2631.942/26.956,exact text/all128IDs/all probabilities,127graph reuses,actual shared-control witness and0staging/buffer/context heaps. Decode-1.57percent single sample;no stable speed claim. Actual scheduler/global registry/provider implementations and full source closure remain open.|
|P4-09 / **DONE**|Port graph backend placement and splitting|ggml/src/ggml-backend.cpp|Goose owns node/leaf assignments,split boundaries,buffer compatibility,offload and fallback policy; placement traces match source contracts.|
|P4-10 / **IN_PROGRESS**|Port cross-device copy planning and asynchronous dependencies|ggml/src/ggml-backend.cpp|Goose owns split inputs,copy slots,events,waits and lifetime edges; multi-stream/multi-sequence cancellation/failure fixtures pass.|
|P4-11 / **DONE**|Port unified CPU/GPU scheduler execution and graph reuse|ggml/src/ggml-backend.cpp|Reserve/compute/recompute/synchronize/reset/eval callback/reallocation paths execute Goose; graph reuse and cold/warm/abort traces checked.|
|P4-12 / **TODO**|Port meta backend,backend optimization and threading services|ggml/src/ggml-backend-meta.cpp; ggml/src/ggml-opt.cpp; ggml/src/ggml-threading.cpp|All meta/planner/optimizer helpers Goose; OS primitive boundary minimal; callback reentrancy and thread lifetimes checked.|
|P4-13 / **TODO**|Unify allocation and scheduler interfaces across both lineages|ggml/src/ggml-alloc.c; ggml/src/ggml-backend.cpp; source/*/bridge.h|Single Goose ownership model serves Apex; no duplicated hidden native policy; complete borrowed/owned lifetime table.|
|P4-14 / **TODO**|Check full graph/allocator lifecycle under failures|tests/test-backend-ops.cpp; tests/test-alloc.cpp; ggml/src/ggml-alloc.c|Allocation failure injection,invalid-size/view/copy,abort/reset/reuse and concurrent independent contexts pass scoped sanitizers; no new leaks.|
|P4-15 / **IN_PROGRESS**|Run unified scheduler actual model milestone|src/llama-context.cpp; ggml/src/ggml-backend.cpp|Apex and Qwen real prefill/decode,prefix reuse,branch/restore/cancel match reference with Goose scheduler and allocator witnesses.|
|P4-16 / **TODO**|Remove owned GGML core/native scheduler implementation|ggml/src/ggml*.c; ggml/src/ggml*.cpp|Candidate GGML core link contains only Goose-generated logic plus declared primitives; all API symbols and original rollback retained externally.|

#### P5 — KV-cache,recurrent memory and sessions

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P5-01 / **TODO**|Port KV cell allocation,sequence ownership and position maps|src/llama-kv-cells.h; src/llama-kv-cache.cpp|Every cell find/copy/remove/keep/shift/div/sequence operation checked for interleaving,multiple sequences and full-capacity failures.|
|P5-02 / **TODO**|Port KV tensors,type conversion and physical cache layout|src/llama-kv-cache.cpp|F32/F16/BF16/Q8/Q4 supported K/V layouts and CPU/GPU allocation/data movement match; no reduced context/precision for pass.|
|P5-04 / **TODO**|Port recurrent-state memory subsystem|src/llama-memory-recurrent.cpp|State create/update/copy/remove/rollback and stream ordering match multi-step recurrent models; actual Apex recurrent witnesses.|
|P5-05 / **TODO**|Port hybrid and indexed hybrid memory|src/llama-memory-hybrid*.cpp; src/llama-memory.cpp|Hybrid full/sliding/recurrent memory composition and sequence synchronization match reset/branch/interleave fixtures.|
|P5-06 / **TODO**|Port lazy KV/VMM reservation and context growth policy|src/llama-kv-cache*.cpp; ggml/src/ggml-cuda/ggml-cuda.cu; patches/kv-lazy-vmm.patch|Preserve lazy mapping/reservation,bounded committed pages,OOM/reallocation and original large-context correctness; CUDA driver primitive import only.|
|P5-07 / **TODO**|Port checkpoint,prefix reuse,branch and restore|src/llama-context.cpp; src/llama-memory*.cpp; tools/server/server-context.cpp|Exact saved state and final outputs on reuse/rewind/early branch/near-capacity; no comparing KV alone as answer correctness.|
|P5-08 / **TODO**|Port state/session serialization and compatibility|src/llama-context.cpp; src/llama-io.cpp|Native<->Goose session/state read/write where format permits; version/endian/truncation/error corpus and deterministic continuation.|
|P5-09 / **TODO**|Port KV defrag,maintenance and multi-sequence lifecycle|src/llama-kv-cache*.cpp; src/llama-context.cpp|All supported defrag/shift/sequence cleanup/context reuse paths Goose; no dangling caches after cancellation/model unload.|
|P5-10 / **TODO**|Close memory/KV subsystem on actual models|src/llama-memory*.cpp; src/llama-kv-cache*.cpp|Apex memory and session milestone including configured context/multi-sequence lifecycle; zero owned native KV/policy symbols in selected link.|

#### P6 — Shared model runtime and complete Apex architecture

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P6-01 / **TODO**|Port architecture,hparams,cparams and tensor-name registries|src/llama-arch.cpp; src/llama-hparams.cpp; src/llama-cparams.cpp|Every audited architecture/tensor/config entry represented; unknown/invalid metadata errors preserved; no Apex-only hardcoding.|
|P6-02 / **TODO**|Port model object and tensor binding lifecycle|src/llama-model.cpp; src/llama-model.h|All model tensor ownership,backend attachment,metadata queries and unload/error cleanup implemented with ABI parity.|
|P6-03 / **TODO**|Port complete shared graph-builder DSL|src/llama-graph.cpp; src/llama-graph.h|All shared dense,MoE,attention,rope,norm,recurrent and output graph helpers Goose; node/shape/config traces differential.|
|P6-04 / **TODO**|Port complete Apex architecture graph and shared MoE graph interface|src/models/qwen35moe.cpp; src/models/qwen35.cpp; src/llama-graph.cpp|Apex graph construction and real prefill/decode/state lifecycle execute Goose using general shared interfaces; no model-specific native graph callback.|
|P6-07 / **TODO**|Port batching,ubatching,sequence and output mapping|src/llama-batch.cpp; src/llama-context.cpp|Batch splitting,positions,logit/embedding selection,multi-sequence and output permutations match; invalid batch rejection and aborts.|
|P6-08 / **TODO**|Port context decode/encode and graph execution orchestration|src/llama-context.cpp; src/llama.cpp|Goose owns context construction,warmup,evaluation,graph reuse,output/embedding/logit APIs,performance counters and error propagation.|
|P6-09 / **TODO**|Port adapters,LoRA,control vectors and model saving|src/llama-adapter.cpp; src/llama-model-saver.cpp|All adapter apply/remove/scale and save paths Goose; tensor/value/format roundtrips and error cleanup checked.|
|P6-10 / **TODO**|Port sampling,grammar and constrained generation|src/llama-sampler.cpp; src/llama-grammar.cpp; common/sampling.cpp|Every sampler/RNG/chain/grammar mode differential with fixed seeds; token filtering and reproducible text/API semantics preserved.|
|P6-12 / **TODO**|Close public inference API and complete Apex model lifecycle|include/llama.h; include/llama-cpp.h; src/**|Every shared public C API implemented in Goose; full Apex load/graph/decode/state/unload works; remaining architecture builders explicitly assigned to compatibility tasks.|

#### P7 — Shared GGUF/loading/text infrastructure and Apex tokenizer

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P7-01 / **TODO**|Port complete GGUF parser and metadata accessors|ggml/src/gguf.cpp; ggml/include/gguf.h|Every GGUF version/value type,alignment/endian/tensor descriptor and accessor roundtrip; truncated,overflow,malformed and multi-shard corpus rejected safely.|
|P7-02 / **TODO**|Port GGUF writing,splitting and tensor serialization|ggml/src/gguf.cpp; tools/gguf-split/**|Native<->Goose byte/semantic compatible serialization,shard naming/offsets/split-merge and nonseekable/error cases checked.|
|P7-03 / **TODO**|Port full model loader,mmap and file I/O ownership|src/llama-model-loader.cpp; src/llama-mmap.cpp; src/llama-io.cpp|All load modes,multi-shard,lazy/mmap/mlock,overrides,tensor validation,progress/cancel and error cleanup implemented; Apex load witnesses.|
|P7-04 / **TODO**|Port model quantization,importance and conversion policy|src/llama-quant.cpp; common/imatrix-loader.cpp; tools/quantize/**; tools/imatrix/**|Every supported quantizer/type policy and calibration path Goose; deterministic small tensor/model corpora; no adaptive quantization research added now.|
|P7-05 / **TODO**|Port Unicode tables,normalization and character utilities|src/unicode*.cpp; common/unicode.cpp|All audited tables and Unicode normalization/category/byte conversion behavior differential across valid/invalid UTF8; no locale-dependent drift.|
|P7-06 / **TODO**|Port vocabulary loader and Apex tokenizer through a general tokenizer interface|src/llama-vocab.cpp|Apex tokenizer/pretokenizer/special-token policy exact on complete native differential corpus; interface supports future tokenizer implementations without a separate runtime.|
|P7-07 / **TODO**|Port detokenization,token metadata and incremental text decoding|src/llama-vocab.cpp; tools/tokenize/**|All token-to-piece/special/byte fallback/stream boundary behavior exact; arbitrary byte and Unicode roundtrips; token IDs unchanged.|
|P7-08 / **TODO**|Port chat templates,Jinja and reasoning parsing|src/llama-chat.cpp; common/chat*.cpp; common/jinja/**; common/parsers/**|All shared Jinja/template/filter/parser/tool/reasoning logic Goose; Apex golden prompt bytes and structured output exact; no minja arithmetic/parsing wrapper left.|
|P7-09 / **TODO**|Port JSON,schema,PEG and grammar conversion infrastructure|common/json*.cpp; common/peg-parser.cpp; common/json-schema-to-grammar.cpp; common/trie.cpp|JSON/schema/PEG/grammar parsing/rendering and validation owned by Goose; malformed/deep/large input tests and native golden compatibility.|
|P7-10 / **TODO**|Port common configuration,args,presets,fit and startup|common/arg.cpp; common/preset.cpp; common/common.cpp; common/fit.cpp|All maintained flags/env/defaults/config precedence and memory-fit policy ported; --help/invalid args and model profile compatibility checked.|
|P7-11 / **TODO**|Port download,HF cache,HTTP,logging and process helpers|common/download.cpp; common/hf-cache.cpp; common/http.h; common/log.cpp; common/console.cpp; common/subproc.cpp; common/debug.cpp|Owned network/cache/resume/logging/console/subprocess decisions Goose; OS/TLS/libcurl primitives explicitly imported; no downloads or destructive cache work during migration tests.|
|P7-12 / **TODO**|Close GGUF,loader,tokenizer and common native dependencies|ggml/src/gguf.cpp; src/llama-vocab.cpp; common/**|All shared GGUF/loader/common symbols and Apex tokenizer implementation ported; native format/token/API compatibility corpus passes; other tokenizers tracked in compatibility tasks.|

#### P8 — Server,HTTP/API,queues and application lifecycle

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P8-01 / **TODO**|Port server executable,startup and configuration lifecycle|tools/server/main.cpp; tools/server/server.cpp; tools/server/server-common.cpp|Startup/shutdown/config/model selection/errors/signals handled by Goose; health and lifecycle endpoints compatible.|
|P8-02 / **TODO**|Port HTTP transport,routing,CORS and request parsing|tools/server/server-http.cpp; tools/server/server-cors-proxy.h; common/http.h|Owned HTTP/router/limits/CORS/auth middleware Goose; malformed/oversize/timeout/cancel/connection-close requests checked; external socket/TLS primitives only.|
|P8-03 / **TODO**|Port server queues,tasks,slots and scheduling|tools/server/server-queue.cpp; tools/server/server-task.cpp; tools/server/server-context.cpp|Task lifecycle,slot allocation,batch/coalescing,cancel,timeout and backpressure owned by Goose; concurrent request outputs and no lost tasks.|
|P8-04 / **TODO**|Port completion,chat,response APIs and SSE streaming|tools/server/server-chat.cpp; tools/server/server-stream.cpp; tools/server/server-context.cpp|Native API compatibility suite for stream/nonstream completion/chat/responses,token usage,finish reasons,errors and cancellation.|
|P8-05 / **TODO**|Port embedding,reranking,tokenization and utility endpoints|tools/server/server-context.cpp; tools/server/server-schema.cpp|Every registered endpoint has schema/status/body differential test; embeddings/rerank numerical contract and tokenizer bytes checked.|
|P8-06 / **TODO**|Port model management,connectors and hot reload|tools/server/server-models.cpp; tools/server/server-connect.cpp|Load/unload/swap/list/proxy lifecycle Goose; pending/active requests and resource cleanup checked without changing working runtime.|
|P8-07 / **TODO**|Port tools,MCP,structured output and decision helpers|tools/server/server-tools.cpp; tools/server/server-mcp.cpp; tools/server/server-decision.cpp; tools/server/server-schema.cpp|All maintained tool/MCP/schema/decision paths Goose with fixtures; no external messages or real tool side effects during tests.|
|P8-08 / **TODO**|Port metrics,watchdog,diagnostics and operational controls|tools/server/server-swapwatchdog.cpp; tools/server/server-common.cpp|Metrics/counters/watchdog/status and administrative controls match; simulated pressure/failure corpus, no stopping other workloads.|
|P8-09 / **TODO**|Port web/UI serving and application integration|tools/ui/**; tools/server/**|Goose serves maintained UI assets and all owned native UI/API plumbing; existing HTML/CSS/browser assets explicitly inventoried, no hand-written native server logic left.|
|P8-10 / **TODO**|Run complete server/API functional acceptance corpus|tools/server/tests/**; tools/server/bench/**|All audited route/method/schema combinations have tests; Apex real end-to-end request,stream,cancel,reuse; Qwen major milestone.|
|P8-11 / **TODO**|Exercise concurrent clients,error handling and long-lived sessions|tools/server/tests/**; tools/server/server-context.cpp|Concurrent independent/branched sessions,disconnects,invalid inputs and repeated model lifecycles complete correctly; leaks/races scoped checked.|
|P8-12 / **TODO**|Remove native server implementation from candidate executable|tools/server/**; common/**|Goose entrypoint owns HTTP/API lifecycle; link/source/import audit shows no owned original C++ server/parser/task logic remains.|

#### P9 — Standalone streaming runtime and CLI

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P9-01 / **TODO**|Port standalone config,recipe and architecture registry|carrier/runtime/core/src/config.cpp; carrier/runtime/core/src/moe/arch_registry.cpp|All config/recipe defaults,validation and architecture gates match existing tests; CLI profiles compatible.|
|P9-02 / **TODO**|Port platform/file I/O and GGUF offset indexing|carrier/runtime/core/src/io/**; carrier/runtime/core/src/moe/gguf_offsets.cpp|Bounded reads,mmap/pinned allocation,shard offsets,endian/error/overflow cases Goose; only primitive platform imports retained.|
|P9-03 / **TODO**|Port dense weights and expert streaming/LRU sources|carrier/runtime/core/src/moe/dense_weights.cpp; carrier/runtime/core/src/moe/expert_stream_source.cpp|Goose owns bounded RAM LRU,eviction,prefetch,dense residency and source lifetimes; deterministic trace/data checks.|
|P9-04 / **TODO**|Port row streaming,router hooks and I/O-compute overlap|carrier/runtime/core/src/moe/row_stream.cpp; carrier/runtime/core/src/moe/router_hook.cpp; carrier/runtime/core/src/moe/stall_union.h|Exact route/readiness/row-slice outputs,overlap ordering,cancel/failure and bounded RAM/VRAM behavior; native policy callbacks removed.|
|P9-05 / **TODO**|Port standalone sessions,runtime and ngram draft|carrier/runtime/core/src/engine/runtime.cpp; carrier/runtime/core/src/engine/session.cpp; carrier/runtime/core/src/engine/ngram_draft.cpp|Complete decode/prefill/session/draft lifecycle Goose; repeated-state and deterministic workload fixtures plus real model request.|
|P9-06 / **TODO**|Port chat/thinking controls and standalone CLI|carrier/runtime/core/src/engine/chat_parse.cpp; carrier/runtime/core/src/engine/thinking_control.cpp; carrier/runtime/cli/main.cpp|CLI,chat rendering,thinking markers,budget and interrupt behavior match existing corpus and stdin/stdout contract.|
|P9-07 / **TODO**|Port metrics/traces and deterministic workload infrastructure|carrier/runtime/core/src/metrics/**; carrier/runtime/core/src/moe/deterministic_workload.cpp|CSV/schema,route/decode traces,stall union and deterministic workloads match golden records; no secret/private data published.|
|P9-08 / **TODO**|Close standalone engine native implementation|carrier/runtime/**|All standalone public API,CLI and enabled optional paths Goose; existing eight test modules ported/retained as independent oracle; source/link closure checked.|

#### P10 — Tools,platforms,build system and dependency closure

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P10-01 / **TODO**|Consolidate experimental modules into canonical Goose source tree|experiments/**/source/**; src/**; ggml/src/**|One canonical selected module per subsystem under appropriate src paths; frozen experiment history preserved; no duplicate runtime implementations.|
|P10-02 / **TODO**|Port maintained command-line tools and utilities|tools/cli/**; tools/completion/**; tools/llama-bench/**; tools/perplexity/**; tools/export-lora/**; tools/cvector-generator/**; tools/connect/**; tools/tuning/**; tools/fit-params/**; tools/batched-bench/**|Every audited tool native entrypoint has Goose equivalent,CLI compatibility and tiny fixture; no tool directory silently excluded.|
|P10-05 / **TODO**|Port owned executable infrastructure and generators|scripts/**; cmake/**; CMakeLists.txt; common/build-info.cpp.in|Owned runtime/generator/protocol logic moved to Goose tools; build declarations/shell launch wrappers can remain explicitly declarative with no hidden original computation.|
|P10-06 / **TODO**|Integrate deterministic Goose build,generated C/CUDA and packaging|CMakeLists.txt; ggml/CMakeLists.txt; src/CMakeLists.txt; tools/server/CMakeLists.txt; carrier/runtime/CMakeLists.txt|Clean isolated build uses pinned self-hosted Goose for all owned logic; generated outputs reproducible; CPU/CUDA compile/link manifest machine-audited.|
|P10-07 / **TODO**|Replace or justify vendored executable dependencies|vendor/**; ggml/src/ggml-cuda/vendors/**; common/jinja/**|Each JSON/Jinja/HTTP/regex/image/audio helper classified; replace owned/replaceable logic or explicitly justify true external primitive. Vendored C++ is not automatically unavoidable.|
|P10-08 / **TODO**|Preserve public headers,FFI and binary interoperability|include/**; ggml/include/**; carrier/runtime/core/include/**|C ABI symbols/layout/statuses compatible; header-only declaration wrappers allowed, executable C++ compatibility logic ported or remains unfinished.|
|P10-09 / **TODO**|Port test runners,fixtures and diagnostics as needed for native-free builds|tests/**; tools/server/tests/**; carrier/runtime/tests/**; benchmarks/**|Candidate can be functionally tested without linking owned original implementation; original C++ tests/oracles allowed only outside production artifact.|
|P10-11 / **TODO**|Preserve licensing,attribution and source reproducibility|LICENSE; UPSTREAMS.md; vendor/**; runtime/LICENSE.BigMoeOnEdge|All derived source attribution/licenses retained; producer/source/generated hashes and reproducible external dependency manifest included.|
|P10-12 / **TODO**|Verify complete source/entrypoint inventory has no residual native logic|docs/goose-migration/source-audit.json; all audited roots|Every Apex/shared implementation path and public entrypoint maps to completed Goose task or audited primitive; remaining architecture/platform sources are explicit compatibility tasks, never hidden exemptions.|

#### P11 — Full Apex functional integration and native-path removal

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P11-01 / **TODO**|Assemble one fully migrated Apex-capable common runtime|all selected Goose modules|One isolated artifact combines CPU,CUDA,GGML,memory,KV,model,GGUF,tokenizer,server and owned infrastructure; every Apex-path owned component Goose, not a collection of separate partial variants.|
|P11-02 / **TODO**|Run complete Apex end-to-end functional milestone|model/profile manifests; full candidate|Fixed Apex model/profile/input; text,tokens,probabilities,stream,state,reuse/cancel checked and every migrated subsystem witnessed; full-system prefill/decode,RAM/VRAM recorded once.|
|P11-03 / **TODO**|Complete remaining supported feature/format/API corpus|operation/type/architecture/endpoint inventories|All shared and Apex-supported operations/types/formats/API/platform fixtures pass; Qwen/other architecture-specific cases are explicitly deferred to compatibility workstreams.|
|P11-04 / **TODO**|Remove every replaced owned C/C++/CUDA implementation|all candidate source/build/link manifests|Zero owned hand-written native executable logic in candidate; no renamed original symbols/callbacks/embedded C arithmetic; old experiment/oracle code retained outside selected link.|
|P11-05 / **TODO**|Validate clean build,rollback and failure recovery|build/package/reproduction manifests|Rebuild full artifact from checked-in Goose and pinned producer; test rollback to frozen runtime and bad-input/OOM/cancel/shutdown recovery without touching production.|
|P11-06 / **TODO**|Publish unavoidable dependency and remaining-limit report|dependency/import/link audit|List exact OS,C runtime,CUDA driver/toolchain/vendor primitive dependencies with reasons/ABI/licenses; replaceable owned code not renamed external; unresolved native dependency blocks full completion.|
|P11-07 / **TODO**|Verify full Apex functional completion and remaining compatibility count|GOOSE_MIGRATION_PLAN.md; docs/goose-migration/tasks.json|All APEX_COMMON tasks DONE with evidence and zero native executable logic on Apex paths; compatibility tasks counted separately as remaining universal GOAL work.|
|P11-08 / **TODO**|Hand off full Apex runtime and begin shared-runtime compatibility extension|full functional report; COMPILER_FEEDBACK.md local ignored|Record full Apex functionality,dependency/import report,rollback and cumulative speed/memory history; continue Qwen/other architecture compatibility in same runtime; no production promotion.|

### Existing shared work retained from historical experiments

#### P1 — Complete CPU MoE subsystem

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P1-03 / **DONE**|Port five Qwen raw quantized expert math families|source/moe-quantized-byte/**; ggml/src/ggml-quants.c|Q2_0,Q5_0,IQ4_NL,IQ2_XXS,IQ2_S dot/matrix/routed/dequant/fused scope exact; real Qwen witnesses. Evidence: EXP139 source and exact/sanitizer/model evidence; speed regression recorded|

### After full Apex: Qwen compatibility in the same runtime

#### P6 — Shared model runtime and complete Apex architecture

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P6-11 / **TODO**|Port MTP,speculative decoding and draft state orchestration|common/speculative.cpp; common/ngram*.cpp; src/llama-context.cpp; patches/mtp-sidecar.patch|Shared/separate draft,MTP borrowing,accept/reject/rewind and no-spec mode checked; actual Apex MTP milestone after standard path.|

#### C1 — Qwen extension of the same Goose runtime, only after full Apex

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|C1-01 / **DONE**|Record Qwen-specific source differences without active development|qwen/src/**; qwen/ggml/src/**|Hash and classify all Apex/Qwen source differences; identify Qwen4Exp,IQ2 precision/cold-bank/pinned/MTP and cache differences; no Qwen build/test now. Depends: P11-07. Evidence: source-audit.json cross-lineage differences and architecture ledger; historical EXP139..141 evidence only|
|C1-02 / **TODO**|Extend general model graph/metadata support to Qwen4Exp|src/models/qwen4exp.cpp; src/llama-arch.cpp; src/llama-graph.cpp|Use existing shared Goose graph/runtime; architecture adapter only where source semantics differ; no cloned backend/runtime. Depends: P11-07.|
|C1-03 / **TODO**|Integrate Qwen quantized precision,cold-bank and expert hook differences|ggml/src/ggml-cpu/arch/x86/quants.c; ggml/src/ggml-cpu/ggml-cpu-mul-mat-id-cold.c; src/llama-expert-*.cpp|Reuse common quant/math/controller modules; preserve Qwen precision and explicit fused verification windows; no fork without demonstrated differing contract. Depends: P11-07.|
|C1-04 / **TODO**|Extend shared loader,tokenizer and memory interfaces for Qwen|src/llama-model-loader.cpp; src/llama-vocab.cpp; src/llama-memory*.cpp; src/llama-kv-cache*.cpp|Only required architecture/tokenizer/state adapters; GGUF/parser/allocator/scheduler remain shared; sharded IQ2 model load and state fixtures. Depends: P11-07.|
|C1-05 / **TODO**|Qualify complete Qwen inference in the common artifact|full common Goose runtime; Qwen model/profile manifests|After full Apex port only: real Qwen text/token/probability and state/API compatibility plus full-system performance/memory; no microbench campaign. Depends: P11-07.|
|C1-06 / **TODO**|Protect Apex while extending Qwen support|fixed Apex profile; compatibility change history|Each major compatibility change passes same Apex correctness and one full-system measurement; prior baseline preserved; regression recorded and no unverified production promotion. Depends: P11-07.|

### Then: other architectures/platforms in the same runtime

#### P1 — Complete CPU MoE subsystem

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P1-11 / **TODO**|Cover portable scalar and alternate CPU ISA MoE paths|ggml/src/ggml-cpu/arch/**; ggml/src/ggml-cpu/amx/**; ggml/src/ggml-cpu/kleidiai/**; ggml/src/ggml-cpu/spacemit/**|Every source/backend registered in audit has Goose implementation or explicit hardware-unverified status; no silent native fallback called complete; portable reference tested.|

#### P3 — Complete CUDA backend

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P3-18 / **TODO**|Cover CUDA hardware configurations and multi-GPU|ggml/src/ggml-cuda/mmq-config*; ggml/src/ggml-cuda/vendors/**; ggml/src/ggml-cuda/allreduce*|sm89 executed; alternate hardware configurations have compile/emulation evidence and explicit hardware-unverified ledger; vendor services are not owned-code exemptions.|

#### P5 — KV-cache,recurrent memory and sessions

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P5-03 / **TODO**|Port sliding-window/interleaved/special attention caches|src/llama-kv-cache-*.cpp|All ISWA/DSA/MSA/DSV4 combinations and attention masks/position updates mapped; native-supported modes not silently dropped.|

#### P6 — Shared model runtime and complete Apex architecture

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P6-05 / **TODO**|Port every remaining text model architecture builder|src/models/*.cpp|Every remaining audited text builder assigned and ported; tiny per-architecture graph construction fixture and native-supported API behavior.|
|P6-06 / **TODO**|Port recurrent,multimodal,audio and special model builders|src/models/*.cpp; tools/mtmd/**; tools/tts/**|Every remaining special builder/encoder/projector mapped; image/audio/recurrent tiny fixtures; unavailable model assets explicit unverified, not DONE.|

#### P10 — Tools,platforms,build system and dependency closure

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P10-03 / **TODO**|Port multimodal,audio and RPC tool infrastructure|tools/mtmd/**; tools/tts/**; tools/rpc/**; ggml/src/ggml-rpc/**|Owned projector/audio/codec orchestration and RPC serialization/server/client Goose; protocol/tiny media fixtures; hardware/asset gaps explicit.|
|P10-04 / **TODO**|Port maintained non-CPU/CUDA backend implementations|ggml/src/ggml-*|Every additional audited BLAS/CANN/ET/Hexagon/HIP/Metal/MUSA/OpenCL/OpenVINO/RPC/SYCL/VirtGPU/Vulkan/WebGPU/ZDNN/ZenDNN source assigned and ported or remains explicitly unfinished; not labeled inevitable dependency.|
|P10-10 / **TODO**|Preserve cross-platform and alternate CPU/device support|cmake/**; ggml/src/ggml-cpu/arch/**; ggml/src/ggml-cuda/vendors/**|Every original maintained platform/build option represented in capability ledger; compile-only/emulated/unavailable hardware scope explicit; no unsupported path claimed verified.|

#### C2 — Other architectures and platforms using the same runtime

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|C2-01 / **TODO**|Port all remaining tokenizer/vocabulary families|src/llama-vocab.cpp; src/unicode*.cpp|Every audited tokenizer family has Goose implementation and native golden corpus; shared interface from Apex retained. Depends: P11-07.|
|C2-02 / **TODO**|Port all remaining dense/MoE/recurrent architecture adapters|src/models/*.cpp|Each audited architecture not handled by Apex/Qwen assigned once to shared graph-based adapter and a tiny shape/operation fixture; no separate runtime. Depends: P11-07.|
|C2-03 / **TODO**|Close optional model/tool/platform compatibility matrix|all optional backend/model/tool source inventory|No maintained feature silently dropped; unavailable hardware/assets have explicit evidence gaps and prevent claims beyond tested scope. Depends: P11-07.|
|C2-04 / **TODO**|Protect common runtime/Apex across remaining architecture extensions|common runtime; fixed Apex profile|One common implementation and same Apex correctness/performance history throughout; architecture branches only with recorded semantic reason. Depends: P11-07.|

### Finally: full universal functional closure

#### F1 — Universal source/import closure and final functional handoff

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|F1-01 / **TODO**|Close all owned implementation/import/source paths|complete source-audit.json; candidate build/link/import manifests|All workstreams implemented; no owned handwritten C/C++/CUDA execution hidden as bridge/vendor dependency; historical controls excluded from candidate link. Depends: P11-07.|
|F1-02 / **TODO**|Qualify final universal API,format,model and runtime compatibility|all shared capability matrices|All required functional tasks have evidence; real Apex/Qwen and representative other family fixtures; one common artifact and reproducible rollback. Depends: P11-07.|
|F1-03 / **TODO**|Publish final unavoidable external dependency report|OS/C-runtime/CUDA/vendor primitive ABI manifest|Every remaining dependency justified with source/symbol/version/license; replaceable owned parser,kernel,scheduler or server is not called unavoidable. Depends: P11-07.|
|F1-04 / **TODO**|Verify all functional task counters and deliver final source/runtime|GOOSE_MIGRATION_PLAN.md; tasks.json; full build manifests|Every functional task DONE; zero untracked residual native paths; source build reproducible and working runtime unchanged. Then comprehensive audits/optimization. Depends: P11-07.|

## Per-stage execution and completion protocol

1. Select the next large batch and pin producer/source/control once; record its requirement IDs/dependencies. Keep shared interfaces; do not spawn another development stream.
2. Implement the complete coherent Goose module with typed ownership and explicit primitive imports. Generate C/CUDA through the qualified self-hosted compiler. No compiler edits,no native-computation wrapper counted as migration.
3. Compile isolated candidate. Run suitable differential correctness/error fixtures and scoped sanitizer checks. Preserve failed sources/logs; distinguish native-control defects from new defects.
4. At the large delivery-cycle end,run one combined real Apex gate using the fixed profile. Earlier model runs require a concrete correctness boundary that cannot be checked locally. Check text/IDs/probabilities or a predeclared numerical contract and actual Goose path witnesses. Record one full-system prefill/decode/RAM/VRAM row,plus exact configuration/library hashes. Do not run Qwen now.
5. Compare new row to original and previous stage,record any regression in private ignored COMPILER_FEEDBACK.md,and continue. Do not tune local loops or repeat benchmarks to rescue a number.
6. Check disable/rollback path and unchanged working runtime. Save source/emitted code/tests/receipts/results as one coherent stage. Update task status/evidence/history and regenerate this file. Never mark a whole parent subsystem DONE from one leaf.
7. Remove replaced native code only from candidate once functional closure is proven. The final link/import/source audit must reject hidden owned native services. Generated C/CUDA and OS/vendor primitive calls remain explicitly declared.

## Dependencies,compiler limits and end condition

Candidate unavoidable dependencies: OS kernel/threads/virtual memory/file/socket primitives; C ABI/runtime/math support; CUDA driver/device runtime/toolchain and specifically justified vendor primitives; build tools. This is a provisional list,not an approval to keep arbitrary C++ libraries. Owned GGML/llama arithmetic,allocator,scheduler,GGUF/tokenizer/JSON/Jinja/HTTP/server logic is migration scope,not an unavoidable external dependency.

Compiler limitations stay in local ignored COMPILER_FEEDBACK.md with reproducer,producer and observed behavior. Do not publish that private file,edit the compiler or manufacture a native fallback completion. If a language feature is missing,use a supported Goose formulation; if impossible,keep the task explicitly incomplete and continue independent tasks.

Full Apex milestone means every APEX_COMMON task DONE with one integrated artifact; it does not complete the universal GOAL. Full functional migration means all current functional tasks DONE,complete candidate source/import closure,API/model compatibility evidence,rollback and final unavoidable-dependency report. If tasks are added,the denominator and final condition update together. Working/default deployment is a separate approval.

## After full functional migration: comprehensive checks and optimization

- POST-01 — Systematic compiler/runtime performance audit and optimization: Only after functional migration; matched CPU/CUDA operator,prefill,decode,RAM/VRAM studies; feedback-driven compiler changes require separate compiler-owner workflow.
- POST-02 — Independent comparative memory-safety audit against contemporary llama.cpp: Pinned equal models/settings; sanitizers,fuzzing,thread races,CPU/CUDA,ownership; preserve reproducers and limitations; no superiority without evidence.
- POST-03 — Regular matched upstream comparison and useful change review: Pin current official upstream at comparison time; source/behavior/perf differences separated; no autonomous production change.
- POST-04 — Adaptive joint model/kernel quantization research: Roadmap idea only; no change to migration goal/priorities; future equal-size quality/speed/memory comparison.

These are not substitutes for functional migration and are excluded from its functional-task counter. Historical speed/safety claims remain scoped; no new superiority claim before independent evidence.

## Progress update commands

Edit `docs/goose-migration/tasks.json` status/evidence/history and append fixed-profile results to `docs/goose-migration/apex-history.json`. Then run:

```bash
python3 scripts/update_goose_migration_plan.py
```

The generator verifies stable unique IDs,valid statuses,completion evidence and dependency references,and derives all counters/tables from the ledger. Source inventory is [source-audit.json](docs/goose-migration/source-audit.json); future scope additions require an explicit audit/history entry.

## Change history

- fe9d550cca21d5d64244b1f7f85f92681f84f479: Initial source-audited universal migration ledger; EXP136..141 preserved. Owner updated priority to full Apex first, one fixed Apex end-to-end profile, Qwen deferred.
- fe9d550cca21d5d64244b1f7f85f92681f84f479: Token-budget strategy:160requirements grouped into9large ordered batches (B0 reference,5Apex implementation batches,B6Qwen,B7other,B8closure). One integration/combined gate/system sample per batch; no per-item experiments or detailed reports.
- 71b539406937b7d8628e64ca91880f9378238c30: B1implementation started: latest completed self-hosted EXP300 frame-default pinned; common tensor/storage/conversion,reduction,normalization and transform Goose source written. No candidate build/model acceptance claimed.
- B1/EXP143 uncommitted intermediate diagnostic: 254/254 GGML differential cases and scoped ASan/UBSan/leak checks pass; 218 byte-exact,36 within original tolerance. Owner-requested single Apex32768/128 request completes: pp2620.936,tg26.738,RSS21313.555MiB,VRAM3348MiB. Goose graph executor/GET_ROWS/GLU witnessed. Response diverges at token index5 with earlier logprob differences,so model equivalence and B1 acceptance remain open. No repeated series,microbench,optimization,Qwen or production change; continue coherent CPU modules.
- EXP143 B1 in-progress source snapshot: Expanded common recurrent block to RWKV6/7 and GLA:278/278 composite cases,234 exact,scoped ASan/UBSan/leak checks pass. Functional investigation of Apex drift:66/66 operator-contract cases pass;147 real embedding rows byte-exact on1/8/16threads; wide GLU within unchanged tolerance. Root cause remains unestablished; not B1 acceptance. Source/emitted C/tests/provenance are reviewable in experiments/EXP-2026-11-07-143-goose-cpu-b1/; no repeat full inference or speed optimization.
- EXP143 CPU control/job and numerical-contract checkpoint: Goose owns shared CPU plan/dispatch and active OpenMP graph-job policy.326/326graph/status,960/960planning/rollback and66/66Apex operator contracts pass with scoped sanitizers. SWIGLU9/9wide configurations now byte-exact after matching AVX2 math; GLU signed-zero difference repaired. Full Apex response equivalence remains open,no repeated model request/performance tuning. B1 is incomplete; residual source remains visible.
- EXP144 post-GLU Apex diagnostic on9c0a698: Exactly one owner-requested32768/128 request completes:pp2638.151,tg27.078,RSS21313.887MiB,VRAM3348MiB. All128tokens,text and all saved top5/logprobs exactly equal original C++; prior divergence at token index5 absent. Goose control/job/executor/GLU/GET_ROWS witnessed. Historical-control speed deltas remain preliminary,not B1 acceptance. No repeated series,optimization,Qwen or production change; continue B1 backend API/registry ownership.
- EXP145 Goose CPU backend API: 19backend API paths route policy to Goose,not original C++ methods.167API records/28280capability queries and rollback exact;9allocation failure/recovery policy cases;326/66/960common cases preserved;scoped ASan/UBSan/leak checks pass. One fixed Apex32768/128 gives2608.115pp/26.461tg,RSS21313.453MiB,VRAM3348MiB,text/IDs/top5 exact;5346backend graph calls witnessed. Previous-stage pp-1.139%,tg-2.280%preliminary;no tuning or repeat series. B1 remains incomplete; refresh private manual-handoff kit next.
- EXP147 independent EXP146 audit and real Goose buffer policy: Preserved original EXP146;fixed missing 64-bit fg146 C prototypes,assert-only allocation/free/copy hooks,invalid tensor-range fixture,fixture-only sanitizer coverage and wrong canonical EXP146 metrics. Full new base/CPU units instrumented;326/66/960/174 plus148/9memory contracts pass. Fresh C++2918.900/29.589,EXP1462648.645/28.608,EXP1472665.514/27.738tok/s;all128tokens/text/top5 exact,RSS21311.895MiB/VRAM3348for fixed candidate. Preliminary external-load samples,no parity claim/tuning;B1 remains open. Next coherent work:P1-06/P2-13 remaining raw quantized CPU math/type traits.
- EXP148 common CPU quantization/type traits batch: Three coherent components integrated once:nine-format encoder module,shared Q8 activation reuse and real CPU trait/MoE adapters. Latest completed self-hosted EXP301 Stage3 pinned;five inherited generated C units and four previous common output corpora byte-identical.893trait/canary/concurrent+rollback cases,841Q8special and326/66/960/174common gates pass release/sanitized. One Apex32768/128:pp2680.113,tg27.876,RSS21312.617MiB,VRAM3348MiB;all128tokens/text/top5 exact,172640Q8_K blocks. No requirements closed;P2-13 float-domain/format/ISA closure and P1-06 raw arithmetic remain open. Adopt owner instruction:target10-15related requirements per shared cycle where safe,no agents,no scope reduction.
- Owner13-stage campaign regroup: Regroup all148remaining original requirements exactly once into13dependency-checked groups. Prior12DONE retained;denominator160unchanged. Stage1CPU control/ownership5requirements;stage2fullCPU math/removal19;subsequentCUDA,GGML,KV/model,GGUF,server,infrastructure,Apexclosure,Qwen,othercompatibility and final universal audit. Group sizes differ because critical closure dependencies cannot be moved earlier. Current EXP149 targets entire P2-15 within stage1;an experiment/module alone is not a completed stage. Final100percent remains unproven until all original contracts and actual system performance/safety gates pass.
- EXP149 entire CPU graph/job/pool control contract: Close original P2-15 after complete default-path source audit,all101dispatcher entries versus independent native oracle,960planning cases,OMP+pthreads real lifecycle and injected failures,494mask/NUMA/priority/wrap cases,shared actual GGML corpus,ASan/UBSan/LSan and scoped pthread TSan. Two fixed Apex requests:OMP2646.558/28.462,pthread2669.811/26.424tok/s,text/all128IDs/top5 exact;memory/trace preserved. Counts13DONE/160,147left,stage1of13has1/5closed. Other kernel/provider/ABI/memory/architecture requirements remain open. Record owner intermediate >3000pp/>30tg target with repeated paired fresh-C++ confirmation;not reached and no stability/parity claim.
- EXP150 common expert policy and CPU registry: Close P2-14 functional CPU registry/device/buffer contract and expand P1-10 with SLRU/cache grouping/budget and shared expert scratch.30296new cache/budget cases,210registry policy and5compiled HBM-stub API cases;corrected actual326/66/960/174GGML corpus,rollback,scoped sanitizers and one exact128token/top5Apex. Model2656.285pp/28.079tg,RSS21312.090/VRAM3348MiB;single sample,stable C++ parity unproven.14/160DONE,146left,8.75percent;stage1/13has2/5done,0fullstages. Group unchanged13stages into3large delivery cycles (45/54/49requirements at regroup),allow fourth only for real dependencies;cycle1has2/45closed. No false cache/HBM model-execution,hardware,speed or full-expert closure. Final typed fill-ABI correction checked locally;recorded earlier model keeps its original llama hash. Planned extra model request not launched;next exact whole-candidate model gate deferred to large-cycle end per owner.


## Owner-directed restart from verified EXP150 (2026-11-08)

EXP153 is quarantined;stop auditing/fixing that candidate under the superseded investigation request. Continue the existing full Apex migration solo from exact commit87c7974 on branch `codex/apex-exp150-restored-20261108`. EXP154 restores the historical core binary SHA exactly and one real Apex32K/128 request confirms exact text,all128IDs,all saved top5/probabilities and127graph reuses. Restored2631.170prefill/27.492decode versus historical2656.285/28.079;RSS21315.605MiB,VRAM3246MiB,zero swap. Both are uncontrolled single samples.

Latest owner testing rule supersedes the former large-batch-only model gate:after every significant change,perform scoped correctness checks and one fixed whole-system Apex32K/128 correctness/performance gate before further integration. Small timing noise is acceptable;large slowdowns and correctness mismatches must be fixed before advancing. Keep working production untouched and preserve rollback at every accepted stage. No auxiliary agents or delegation. Reuse the verified EXP001..150 work;do not import EXP153 worker modules. The full migration goal and deferred Qwen strategy are unchanged.

Evidence: `experiments/EXP-2026-11-08-154-apex-exp150-restore/`. No additional requirement is marked done by restoration.


### EXP155: solo shared tensor geometry (2026-11-08)

Real Goose shape/stride/span/layout/broadcast module integrated on the exact restored EXP150 baseline. Original numerical kernels retained. Independent native/rollback corpus and one fixed Apex gate pass with exact text,all128IDs/all saved top5 and127graph reuses. Prefill2632.402/decode27.762 versus EXP1542631.170/27.492;small single-sample variation,no stable speed claim. See `experiments/EXP-2026-11-08-155-goose-tensor-geometry/`. P4-03 moves TODO->IN_PROGRESS;no full requirement is newly marked done. Next:tensor/view constructors and context/object/type ownership,same mandatory per-significant-change Apex gate,no helper agents.


### EXP156: solo context-arena and tensor/view constructors (2026-11-08)

Goose now owns arena object placement/capacity/publication,tensor validation/default metadata/strides,nested view flattening,reshape/layout transformation and object traversal. Typed C imports are raw POD/storage/formatting effects and accepted Goose geometry.35formats,2800paired tensors,canary/borrowed-context/failure transaction corpus and explicit rollback pass. One actual Apex32K/128 request yields2639.870prefill/27.247decode,exact128IDs/text/all saved top5 and127graph reuses,with new constructor witnesses. +0.28%/-1.86% vs EXP155 is small uncontrolled single-sample variation. P4-03 stays IN_PROGRESS;physical context lifecycle,type/op tables and residual APIs remain. Next:context lifecycle/core metadata. No helper agents,production untouched.


### EXP157: solo context lifecycle and core metadata (2026-11-08)

Goose owns context creation/reset/free,owned/borrowed Linux allocation/error cleanup,the active43-slot type registry and101operation/22unary/7GLU/status labels,ftype mapping and common tensor names/flags/accessors.37existing entrypoints redirected;raw OS/storage primitives and unresolved numerical callback addresses are explicit.4500independent native/rollback checks,256independent threaded lifecycle cycles,8allocation-failure abort cases and inherited429444constructor checks pass. One fixed Apex2638.849prefill/27.574decode,exact text/all128IDs/all saved top5/probabilities,127graph reuses,new Goose witness and0live contexts;RSS21315.961MiB,VRAM3246MiB,swap0. -0.039%/+1.203% vs EXP156 is small uncontrolled single-sample variation,not stable C++ parity. See `experiments/EXP-2026-11-08-157-goose-context-core/`. P4-03 remains IN_PROGRESS;no full requirement is newly DONE. Remaining public core/graph builders and source closure next;allocator/scheduler/numerical callbacks remain separate. Solo,production and compiler untouched.


### EXP158: solo common graph-operation construction (2026-11-08)

65native bodies route to36Goose policies for common arithmetic/unary/norm/GLU,MoE/matrix,reduction/repeat/concat,row routing,copy/cast/layout,softmax,RoPE,Flash Attention,SSM-conv/gated-delta,sorting/tri/acc/set and precision/sink metadata. Actual result shapes/validation/params/edges owned by Goose;only typed effects and accepted Goose geometry/constructors imported.10408independent checks/558paired nodes/all36modes plus rollback,8native error pairs and inherited429444/4500gates pass. First Apex2639.982/26.596was3.55percent below the previous decode sample;one justified sequential EXP157->EXP158 follow-up without edits gives2612.899/27.668->2634.815/28.055. All3responses exactly match original text/all128IDs/all saved probabilities and127graph reuses;new Goose mask0x83f3e5f1b,live context heaps0. Initial decrease did not reproduce;no stable acceleration/fresh-C++ parity claim. See `experiments/EXP-2026-11-08-158-goose-graph-builders/`. P4-03 stays IN_PROGRESS,no full requirement newly DONE. Graph/hash/traversal/lifecycle and residual operation helpers remain;full numerical/runtime scope is unchanged. Solo,production/compiler untouched.


### EXP159: solo hash,iterative traversal and graph lifecycle (2026-11-08)

22core bodies/9core-header helpers route to31Goose policies for hash/bitset,checked graph layout,forward order/select,copy/rehash/dup/view/clear and metadata access. Explicit iterative DFS preserves native ordering,compute upgrades,shared/self-referencing nodes and use counts without recursive generated-C stack growth.93393candidate/92907rollback checks,12000depth,9error cleanup cases and256independent threaded graph lifecycles pass. Pre-commit C bridge aliasing risk corrected using memcpy;final corpus and model rerun on corrected artifact. Final Apex2635.438/27.865,exact text/all128IDs/full probabilities,127graph reuses,Goose mask0x50dc22e and0graph/context heaps. +0.024%/-0.678% vs latest EXP158;small uncontrolled variation,no stable acceleration/fresh-C++ parity claim. Two model requests total:early pre-fix diagnostic and final gate after the concrete fix,both retained. See `experiments/EXP-2026-11-08-159-goose-graph-lifecycle/`. Synchronize tasks.json P4-03 with verified EXP155..158 and move P4-04 toIN_PROGRESS;no new full requirement DONE. Other-TU inlines,fusion,autodiff/training/export and remaining runtime scope stay open. Production/compiler untouched,no agents.


### EXP160: shared fusion/parameter/hash service across CPU/CUDA host (2026-11-08)

One Goose service replaces selected fusion and parameter/header copies in core,scheduler,meta,CPU core and exact active EXP107 CUDA-host unit. All three baseline DSOs relink to the original SHA before replacement;all unselected objects/kernels preserved.714native decisions/1655checks,bit-exact float/int slots,composition metadata,two rollback modes and15bad-input/cycle cases pass. One Apex2636.702/27.185,exact text/all128IDs/all probabilities,127graph reuses,RSS21314.910MiB/VRAM3246MiB,swap0. Actual witnesses core0x11410,CPU0x400,CUDA0xc28,scheduler0x100000000000;meta andCPU fusion not exercised by this profile. Decode-2.438percent vs final EXP159 retained as preliminary;no stable speed-parity claim. See `experiments/EXP-2026-11-08-160-goose-shared-fusion/`. P3-16 becomesIN_PROGRESS for partial host-control ownership;P4-03/P4-04 stayIN_PROGRESS,no full requirement newlyDONE. Other leaf inlines/numerical/device/model/source closure remain open. Next:Apex allocator/buffer ownership from existing accepted work. No agents,compiler/production untouched.


### EXP161: actual release allocator module (2026-11-08)

Active EXP160allocator object reproduces SHA9246ec1bc84e9f0e5d65ba52f7f3b3c7e2af5c013ac19b258a02ac9da67a3149from clean native source;historical Goose136selector/coalescing/lifetime code was not active. EXP161uses those verified designs and transfers42release bodies to current self-hosted Goose,including chunk/free-list,physical vbuffer ownership,gallocr reserve/reuse/alias/lifetime and collection.10800dynamic operations,12graphs,495backend event records/17576checks,rollback,7creationOOM/6reserve fatal/1recoverable failure and128threaded graph lifecycles pass. Native-valid zero-length marker overlap false-positive found/fixed before model. One Apex2633.888/27.425,RSS21315.945MiB/VRAM3246MiB,swap0,exact text/all128IDs/all probabilities,127graph reuses,allocator mask0xb18070000c/zero outstanding heaps. -0.107%/+0.881%vs EXP160,single-sample,no stable speed-parity claim. See `experiments/EXP-2026-11-08-161-goose-allocator/`. P4-05/P4-06 nowIN_PROGRESS;backend bulk planning/provider/debug/source closure remain,no new full universal requirementDONE. Next:P4-07actual backend bulk grouping/ownership. Production/compiler untouched,solo.


### EXP162: backend buffer/bulk ownership and failed-init rollback (2026-11-08)

30common buffer/type/default bulk/multi/view/tensor-binding bodies now actual Goose in one C unit with typed C++ effects. Native-success/event/allocator parity remains exact.243callback contracts,12bulk+3multi OOM sites,2bad_alloc callbacks and128threaded bulk lifecycles pass. Native5of6provider failures leave stale tensor metadata;new code restores complete snapshots after cleanup and preserves borrowed/caller-owned resources. Independent parity uses separate loader namespaces;C++ exception callback tests use primary like real server. One Apex2639.393/27.387,RSS21315.066MiB/VRAM3246MiB,swap0,exact text/all128IDs/all probabilities,127graph reuses,internal plan bit40and zero buffer/allocator/context heaps. +0.209%/-0.140%vs EXP161,single-sample,no stable acceleration claim. See `experiments/EXP-2026-11-08-162-goose-backend-buffers/`. P4-07nowIN_PROGRESS;common bulk import gap from EXP161closed,provider/meta/broader event/device/scheduler and strict source contracts remain. No new full universal requirementDONE. Next:generic stream/tensor-copy/event/device control. Solo,no production/compiler changes.


### EXP163: generic stream/I-O/copy/event/device control (2026-11-08)

48generic bodies now Goose:required/optional interface validation,2D fallback loops,copy routes/staging,both-backend sync,event ownership,graph status/sync and device/reg forwarding.37native case groups/264callback events/1091checks and rollback pass,including host->unbound-view.11bounds/overflow/staging/callback failure cases reject safely;128threaded staged-copy/event lifecycles are exact with zero heaps. One Apex2631.942/26.956,RSS21314.930MiB/VRAM3246MiB,swap0,exact text/all128IDs/all probabilities,127graph reuses,actual stream mask0xf3ffc4fa233e. Staging markers50/51were local-only,not claimed exercised by this Apex profile. -0.282%/-1.573%vs EXP162,single sample,no stable speed-parity claim. See `experiments/EXP-2026-11-08-163-goose-stream-control/`. P4-07remainsIN_PROGRESS;P4-08interface subset becomesIN_PROGRESS,global registry/loader native. No new full universal requirementDONE. Next:actual scheduler placement/split/owned state,not another interface-only layer. Solo,production/compiler untouched.


### EXP164: actual shared scheduler (2026-11-08)

EXP164 transfers all37normal scheduler/layout entry bodies to actual self-hosted Goose:placement/compatibility/offload,splits/parallel copies,ordered allocator deps,reserve/reuse,compute/eval/event/expert orchestration and owned lifecycle.44native placement/reuse cases,2wide72input graphs,compute failure,10expert phase routes,12constructor OOM sites and32threaded independent lifecycles pass with rollback. One Apex2626.103prefill/27.267decode,RSS21315.086MiB/VRAM3246MiB,exact text/all128IDs/full saved probabilities,127graph reuses,new scheduler witness/zero heaps. Small single-sample speed variation;stable C++ parity unproven. P4-09/P4-11 close their functional placement and reserve/compute/reuse contracts. P4-10/P4-15 remain partial for real asynchronous cancellation and branch/restore/future compatibility. This does not close a campaign stage or delivery cycle. Default-off fallback remains original native scheduler,strict native-source removal remains P4-16. See `experiments/EXP-2026-11-08-164-goose-scheduler/`. Latest owner per-significant-change correctness/system-speed gate retained. Next:global registry/dynamic loading and residual common backend helpers. Production/compiler untouched,no agents.


### EXP165: actual Linux registry,device discovery and loader (2026-11-08)

EXP165 transfers16public Linux registry/loader APIs plus registration/device tables,dedup/unload,name/type/init selection,score/init/API checks,search/path/variant/fallback and load-all order to self-hosted Goose.48native registration scenarios,8real synthetic plugins,best/fallback/default search/load-all,345callback events/684checks and rollback pass;4threads128registrations and6bounds/NULL/OOM cases verify releasable partial ownership. Reentrant count callbacks required publishing the backend first and querying count each native loop iteration;fixed before model. One Apex2633.578prefill/27.399decode,RSS21316.629MiB/VRAM3246MiB,exact text/all128IDs/all saved probabilities,127graph reuses,new registry witness andzero owned metadata/handles. Dynamic paths are local-plugin tested,not executed by this directly linked Apex profile. Other-platform feature detection/provider internals/native-source removal remain separate. Small single-sample speed changes are not stable acceleration or fresh-C++ parity evidence. One model request. P4-08 remains IN_PROGRESS for explicit non-Linux/architecture-feature scope;no universal requirement/campaign/cycle falsely closed. See `experiments/EXP-2026-11-08-165-goose-registry-loader/`. Next:active Apex provider/math and model/KV residuals. Production/compiler untouched,solo.


### EXP166: actual CUDA activation/GLU/scale/clamp compute (2026-11-08)

EXP166 transfers40CUDA activation/GLU/scale/clamp public paths,including device arithmetic/indexing and host validation/pointer/launch planning,to self-hosted Goose.896real GPU cases/1059072values/58252checks and native rollback are bit-exact across types/strides/packed-swapped/fused/inplace/specials;8host rejection cases stop before stream/kernel effects. xIELU1ULP from opposite NVCC FMA contraction fixed in Goose before model;device exit witness moved before CUDA teardown. Initial Apex2642.061/26.286was4.06percent below saved EXP165 decode;one unchanged sequential EXP1652638.737/26.943->EXP1662631.509/27.171pair did not reproduce the drop. All3responses exact text/all128IDs/full probabilities/127graph reuses. Actual host/device witness,cuda_status0;only superseded SiLU/scale preloads removed,RMS retained. Stable speed parity unproven;other CUDA/inline/provider/model/KV/source scope remains. P3-01/P3-02 remainIN_PROGRESS for broader CUDA coverage. Raw model logs/responses stay local. See `experiments/EXP-2026-11-08-166-goose-cuda-activations/`. No production/compiler changes,auxiliary agents,Qwen,microbenchmark or performance-tuning series.


### EXP167: full active CUDA broadcast/repeat module (2026-11-08)

EXP167 transfers the whole active CUDA binbcast module:add/sub/mul/div/repeat/repeat-back,fused add/mul2..8,host collapse/type/stride/grid planning,Goose fast-div metadata and real device arithmetic/indexing.1692GPUcases/34282156values/111498checks include1080native-exact,396repaired-type and216padded-native-defect cases;1074native-compatible rollback cases pass. Independent typed GPU reference confirms corrected F16 pointer selection/padded destinations.1792full-uint32fast-div cases match CPU division and native host coefficients;245native overflow/shift discrepancies repaired.12host preflight cases reject before stream/kernel effects. One Apex2635.581prefill/26.567decode,RSS21328.164MiB/VRAM3246MiB,exact text/all128IDs/full saved probabilities,127graph reuses,actual add/mul/fused-add device witness,cuda_status0. Decode-2.223percent single uncontrolled sample vs latest EXP166 retained;stable speed parity unproven. Other CUDA/provider/model/KV/source scope remains. P3-04 nowIN_PROGRESS;broader registered families remain. All repaired-case distinctions are explicit;no claim of bit parity with incorrect native behavior or whole-runtime safety. One model request,no repeats/microbenchmark. See `experiments/EXP-2026-11-08-167-goose-cuda-broadcast/`. Production/compiler untouched,solo.


### EXP168: intermediate normalization source checkpoint (2026-11-08)

The eight-entry host/device normalization drafts,typed POD plan/host FFI,native oracle and accepted Goose RMS reference are pinned before the authorized two-worker GLM-5.3 Flash trial. Current self-hosted code generation passes for both modules;host generated C passes strict FFI syntax checking;emitted compute bodies make no VM allocation calls. Missing fused multiply/add operands fail explicitly. This is SOURCE_CHECKPOINT_ONLY: no CUDA build,GPU correctness gate,Apex measurement,requirement closure or runtime promotion. Verified EXP167 remains the active rollback runtime. Workers receive sanitized source-only snapshots with disjoint host-plan and GPU-fixture ownership;Codex owns integration/review and real acceptance gates. See `experiments/EXP-2026-11-08-168-goose-cuda-normalization/`. No production/compiler changes.

### EXP168: complete active CUDA normalization and retained GLM work (2026-11-08)

EXP168 transfers all8normalization public entrypoints,host validation/launch policy,outer indexing,warp/block reductions and device arithmetic to current self-hosted Goose;EXP166 already owns all40unary/GLU/scale/clamp entries.99GPUcases/95native-exact/12independent-reference/2NaN-policy cases pass;87native rollback and93legacy-RMS rollback cases pass;24preflight cases reject before stream/kernel effects and15additional malformed cases reject. Recovered GLM host/test code retained with Codex fixes,including fixture stream-order race. One Apex2648.029prefill/27.539decode,RSS21328.535MiB/VRAM3246MiB,swap0,exact text/all128IDs/full saved probabilities,127graph reuses. Actual host/device mask0x18,vector2,fusions5,cuda_status0;all superseded preloads removed only from new candidate. Single preliminary sample +0.472percent prefill/+3.658percent decode vs EXP167;stable speed parity with fresh C++ remains unproven. P3-01 functional normalization/unary/scale contract is DONE using joint EXP166/168 evidence;P3-02/P3-04 and broader CUDA/native-removal requirements remain open. No campaign/cycle or full-Apex closure. Production/compiler untouched;exactly2GLM workers now prepare independent reductions/argmax modules. See `experiments/EXP-2026-11-08-168-goose-cuda-normalization/`.

### EXP169 intermediate compatibility checkpoint, not Apex acceptance (2026-11-08)

Local CUDA row-sum/mean/argmax/global-sum and raw helpers now use Goose compute/control with explicit external NVIDIA CUB.326native GPU cases,5028independent checks,8CUDA graph cases,rollback and24before-stream/pool rejection cases pass. Corrected GLM byte/element row-base bugs before GPU integration. One fixed Apex2656.553prefill/28.438decode response remains exact128tokens/full probabilities,but no FG169 execution witness:the new standalone operators are bypassed in this profile. This proves compatibility only,does not advance actual Apex coverage or close a requirement. Accepted runtime remains EXP168. Preserve useful code;next integrate actual fused topk-MoE without disabling/decomposing fusion,then a new executable-change gate. Static Apex path audit and true topk-API fixture assigned to exactly2GLM workers;production/compiler untouched. See `experiments/EXP-2026-11-08-169-goose-cuda-reductions/`.


### EXP169: actual fused Apex MoE routing and shared reductions (2026-11-08)

Four CUDA reduction source units/two raw helpers and fused topk-MoE now use current self-hosted Goose arithmetic/control.326reduction GPU cases/5028independent checks/8capture cases and1436routing cases/2496independent oracle rows pass with original rollback.24reduction+30routing invalid-input cases reject before stream/kernel/pool effects.16small-expert NaN cases repair68original invalid-ID rows,explicitly distinguished from native parity. NVIDIA CUB is an explicit external primitive for large/device-wide sums;not all computation is claimed Goose.

The first compatibility-only request remains archived;the subsequent single gate after actual routing integration gives2645.921prefill/28.038decode,exact text/all128IDs/full saved probabilities,127graph reuses,RSS21329.273MiB/VRAM3246MiB,swap0 and zero owned heaps. Real FG169T host3/E256mask0x100/softmax+norm flags9/status0 proves new numerical code executes inside Apex;standalone reductions remain local-GPU-only coverage. Changes vs EXP168:-0.080percent/+1.812percent,one preliminary sample;C++ archive2918.900/29.589 is not a fresh paired comparison. No stable speed-parity claim.

EXP168 internal FFI visibility receipt was found inaccurate and corrected without modifying its immutable binary. New combined CUDA export map hides those internals and private native oracles,confirmed by nm. P3-05 advances TODO->IN_PROGRESS;sort/cumsum/count and other CUDA/provider/model/KV/loading/server/source closure remain. No complete Apex batch/campaign or universal requirement newly DONE. Source checkpoint7959318 remains rollback history;whole-stage rollback is immutable EXP168;per-module default-on fallbacks are GGML_GOOSE_CUDA_REDUCTIONS=0 and GGML_GOOSE_CUDA_TOPK_MOE=0. Exactly2authorized GLM workers;production/compiler unchanged. See `experiments/EXP-2026-11-08-169-goose-cuda-reductions/`.
