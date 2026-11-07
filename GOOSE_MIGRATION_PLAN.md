# FrankenMoE -> Fried Goose: complete migration plan

Updated: 2026-11-07. Source/experiment checkpoint: `fe9d550cca21d5d64244b1f7f85f92681f84f479`.

**One shared universal runtime. First finish the complete functional Apex implementation, then extend the same Goose modules to Qwen and other architectures. Production is unchanged.**

**Functional tasks: 160 total; 12 DONE; 4 PARTIAL; 0 IN_PROGRESS; 0 BLOCKED; 144 TODO. Remaining: 148.**

**Current Apex/common milestone: 136 tasks; 10 DONE; 126 remaining (including 4 partial).**

Task count is not code coverage, effort percentage, speed or a completion-date estimate. A bounded completed module does not complete its parent subsystem. Archived history is not proof that all modules run together.

## Authoritative execution policy

1. Work in this order: shared/Apex P0-P11; then Qwen compatibility C1 plus its deferred MTP work; then other compatibility C2/deferred architecture tasks; then universal closure F1. Task workstream overrides phase grouping for deferred items. No separate Apex/Qwen runtime branches or parallel projects.
2. The only current real-model development,debug and measurement target is Apex. Qwen source differences are a read-only future reference; do not build,develop,test,benchmark or tune Qwen now. Historical Qwen evidence remains explicitly historical.
3. Transfer whole coherent modules. For every step require source/producer provenance,correctness,actual Goose liveness and rollback. Calling original native computation from a Goose wrapper does not count. Native arithmetic/policy hidden inside ABI glue does not count.
4. After each major subsystem record one full-system fixed-profile Apex sample: prefill,decode,RAM/VRAM and correctness,compared with previous measured stage and frozen original C++. No microbenchmarks,no per-kernel tuning,no repeated optimization series. Record noise and regressions in local ignored COMPILER_FEEDBACK.md,then continue functional work.
5. Preserve production/default executable,libraries,profiles and compiler source. Use isolated candidate directories. Select latest completed self-hosted Goose producer with receipts at each new stage; do not modify the compiler or interrupt its developer.
6. Keep one canonical common Goose implementation. New architecture-specific code requires a concrete differing semantic contract; prefer metadata/adapters,not copied backends. Preserve Apex correctness and the fixed-profile history during later compatibility extension.
7. Each completed stage updates tasks.json,evidence,history and this rendered plan before starting the next. Stable task IDs; append discovered missing scope and explain count changes; never erase unfinished work to improve the percentage.
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
|Allocation reuse/coalescing; dependency/view lifetime and graph policy|EXP136 integrated Apex controls|Physical ownership,full allocator,GGML core and scheduler remain|
|CPU Q8_K/Q8_0; Q4_K/Q6_K raw MoE|EXP137-139 actual inference and exact corpora|Other raw math/repack/threads remain|
|Five additional quantized math families|Archived EXP139 Qwen evidence|Shared integration/precision extension later; speed regression remains|
|Elementwise F32/vector module|EXP138 Apex and scoped checks|Other precisions,GLU/ops and strict NaN payload compatibility open|
|Common MoE controller|EXP140 actual Apex/control corpus|OS services and remaining math/native policy remain|
|Complete13-format AVX2 tiled computation|EXP141 Apex liveness;104graphs per native backend|Other ISA/native sizing/OS services remain; historical Qwen profile did not execute new tiled|
|CUDA RMS row plus SiLU/scale leaves|EXP136-137 actual Apex|Host launch,most CUDA ops,memory/streams/backend still native|

The tiled prefill -20.96% came from a dedicated CPU2048/16 comparison,not the fixed full-system profile below. It is preserved as legacy evidence and in private compiler feedback; it is not mixed into the cumulative series. Native IQ3_XXS fallback UBSan alignment failures and new scoped sanitizer passes are retained without claiming full memory-safety superiority.

## Large development batches: requirements are not iterations

The160tasks are a requirements ledger,not160development cycles. Execute the following large batches. Each batch shares one integration,one consolidated correctness/safety gate and one full-system Apex sample. No per-item research/build/benchmark/report. Internal compiler/build fixes do not create independent experiments. Close dozens of requirements together where technically possible; never mark unimplemented scope DONE merely to reduce iteration count.

| Batch | Requirements | DONE | Remaining | Integrated scope |
|---|---:|---:|---:|---|
|B0|4|4|0|Source audit,immutable reference and fixed Apex history initialization|
|B1|28|4|24|Complete shared CPU MoE and GGML CPU backend|
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
|P2|18|0|1|18|Complete GGML CPU backend|
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

Current implementation batch: **B1**, all remaining common CPU MoE and GGML CPU backend requirements together. P1-06 is a starting source anchor,not an independent experiment. Use Apex native contracts and one combined synthetic/operator corpus; no Qwen implementation or execution. Then B2-B5; compatibility tasks wait until full Apex.

## Fixed Apex end-to-end profile and cumulative history

Authoritative profile/request hashes,argv,library hashes and raw records: [apex-history.json](docs/goose-migration/apex-history.json). One request/sample per stage; no averaging or speed acceptance from a noisy sample. One immutable model,context,placement,batches,thread affinity,KV types,input token IDs,sampling and cache/reset policy. A necessary profile change starts a new series and never silently joins incompatible rows.

Model Apex-35B-V1.0 Q4_K_M; context262144; actual input32768/output128; CPU MoE with GPU attention/dense; nglall,fitoff,FAon; Q4_0K/V; b4096/ub4096;t8/tb16; masks0f0f/ffff strict1,poll0; np1,mmap; cacheRAM0,32checkpoints,specnone; localhost8081; EHSoff. Fresh server/context and cache_prompt=false per arm; normal startup warmup identical; no warmup/functional requests counted in the single measured request. Original and candidate Goose code selections differ,but workload/settings are fixed.

| Stage | Prefill tok/s | Decode tok/s | Delta vs original pp/decode | Delta vs previous pp/decode | RSS peak MiB | Anonymous MiB | VRAM MiB | Swap KiB | Correctness / status |
|---|---:|---:|---|---|---:|---:|---:|---:|---|
|Original C++|2867.85|27.98|+0.00%/+0.00%|pending|21311.26|697.56|3348.00|0.00|exact text/IDs/top5; preliminary single pair; uncontrolled external load|
|EXP141 combined Goose baseline|2892.34|27.53|+0.85%/-1.62%|+0.85%/-1.62%|21318.07|699.55|3348.00|0.00|exact text/IDs/top5; preliminary single pair; uncontrolled external load|

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
|P1-01 / **DONE**|Port common Q8_K/Q8_0 activation quantization|ggml/src/ggml-cpu/quants.c; ggml/src/ggml-cpu/arch/*/quants.c|Whole row quantizers match native bytes and are witnessed in actual Apex/Qwen inference. Evidence: EXP137 Q8_K; EXP139 common Q8_0/Q8_K exact and model traces; Q8_0 performance deferred|
|P1-02 / **DONE**|Port raw Q4_K/Q6_K matrix and routed-expert arithmetic|ggml/src/ggml-cpu/arch/x86/quants.c; source/cpu-moe/**|Dot/matrix/routed outputs match preserved precision contracts; actual Apex executes Goose. Evidence: EXP137..138 CPU MoE and Apex traces|
|P1-04 / **DONE**|Port entire13-format AVX2 tiled-prefill computation|ggml/src/ggml-cpu/tiled/**; source/cpu-tiled-all/**|All13unpack/gather/micro/dense/MoE stages exact, Apex execution witnessed, Qwen integration checked, original fallback retained. Evidence: EXP141:104graphs/backend; actual Apex new-path execution; Qwen profile selects raw path|
|P1-05 / **DONE**|Port common expert controller and checked workspace policy|ggml/src/ggml-cpu/ggml-cpu.c; ggml/src/ggml-cpu/ggml-cpu-mul-mat-id-cold.c; source/moe-control/**|Grouping,strided IDs,cold masks,bank maps,quant partitions,fused windows and tile/chunk policy exact in both lineages. Evidence: EXP140: Apex114/Qwen412graphs; exact actual models; native OS/compute services remain|
|P1-06 / **TODO**|Replace every remaining raw CPU quantized MoE math family|ggml/src/ggml-cpu/quants.c; ggml/src/ggml-cpu/arch/*/quants.c; ggml/src/ggml-quants.c|Every active common CPU dot/type-trait family absent from P1-02/03 has Goose dot/dequant/routed tests; zero calls to replaced native arithmetic; cover K/IQ/TQ/FP4/legacy types using Apex native precision contracts. Other architecture-specific precision differences stay in later compatibility adapters.|
|P1-07 / **TODO**|Replace repacked expert matrices and all packing layouts|ggml/src/ggml-cpu/repack.cpp; ggml/src/ggml-cpu/arch/*/repack.cpp; ggml/src/ggml-cpu/traits.cpp|Each registered pack/trait layout round-trips and matmul matches native; raw/repack/fused dispatch liveness and tails covered.|
|P1-08 / **TODO**|Replace fused and cold expert execution orchestration|ggml/src/ggml-cpu/ggml-cpu-mul-mat-id-cold.c; ggml/src/ggml-cpu/ggml-cpu.c|Complete stock/cold/fused paths execute Goose; masks,bank remapping,shared experts,ready hooks and fallback tested; no hidden original executor.|
|P1-09 / **TODO**|Move expert work distribution and synchronization ownership to Goose|ggml/src/ggml-cpu/ggml-cpu.c; ggml/src/ggml-threading.*|Goose owns partitioning,job queues,claims,barrier state,cancellation; only primitive OS atomics/thread waits imported;1/2/8/16thread deterministic outputs and termination.|
|P1-10 / **TODO**|Move MoE buffers, residency and output assembly ownership to Goose|ggml/src/ggml-cpu/ggml-cpu.c; src/llama-expert-*.cpp; src/llama-moe-cache.cpp|Expert scratch/resident/staging lifecycles and scatter owned by Goose; allocation failure,resize,empty/overlapping routes and cancellations checked.|
|P1-12 / **TODO**|Close CPU MoE subsystem with native compute disabled|ggml/src/ggml-cpu/**; src/llama-expert-*|All supported MoE formats/modes pass graph corpus and real Apex milestone; replaced native symbols unavailable in candidate link; rollback preserved externally. Depends: P1-06,P1-07,P1-08,P1-09,P1-10.|

#### P2 — Complete GGML CPU backend

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P2-01 / **PARTIAL**|Finish complete binary,unary and GLU compute module|ggml/src/ggml-cpu/binary-ops.cpp; ggml/src/ggml-cpu/unary-ops.cpp; ggml/src/ggml-cpu/ops.cpp|Every binary/unary/GLU variant,type,stride and special-value contract implemented; NaN payload discrepancy resolved or explicit compatibility contract agreed. Evidence: EXP138 F32 vectors/4binary/23unary qualified on valid Apex data; other types/NaN strict gate open|
|P2-02 / **TODO**|Port tensor copy,conversion and set/contiguous operations|ggml/src/ggml-cpu/ops.cpp; ggml/src/ggml-cpu/vec.cpp|DUP,CPY,CONT,SET,ACC,F32/F16/BF16/integer/quantized copy paths exact for strided,padded,aliasing and empty tensors.|
|P2-03 / **TODO**|Port scalar/vector primitives and numerical conversions|ggml/src/ggml-cpu/vec.cpp; ggml/src/ggml-cpu/simd-mappings.h; ggml/src/ggml-cpu/simd-gemm.h|Every exported CPU vector/conversion symbol mapped to Goose; scalar/SIMD lane reduction,rounding and exceptional-value cases checked.|
|P2-04 / **TODO**|Port dense matmul,out-product and batched GEMM|ggml/src/ggml-cpu/ggml-cpu.c; ggml/src/ggml-cpu/llamafile/sgemm.cpp; ggml/src/ggml-blas/**|F32/F16/BF16/quantized,broadcast,4D,tails and precision modes match; native owned GEMM logic removed, optional BLAS explicit imported primitive.|
|P2-05 / **TODO**|Port reductions,scans,comparison and sorting|ggml/src/ggml-cpu/ops.cpp|SUM,SUM_ROWS,CUMSUM,MEAN,ARGMAX,COUNT_EQUAL,ARGSORT,TOP_K match including stable tie/NaN/empty behavior.|
|P2-06 / **TODO**|Port normalization and normalization gradients|ggml/src/ggml-cpu/ops.cpp|NORM,RMS_NORM,GROUP_NORM,L2_NORM and registered backward variants match axes,epsilon,strides and exceptional inputs.|
|P2-07 / **TODO**|Port softmax,masks and RoPE subsystem|ggml/src/ggml-cpu/ops.cpp|SOFT_MAX/back,diag masks,ROPE/back and all registered position/scaling modes match both graph corpus and model attention outputs.|
|P2-08 / **TODO**|Port gather,scatter,repetition and tensor transforms|ggml/src/ggml-cpu/ops.cpp|GET/SET_ROWS,backward,REPEAT/back,CONCAT,DIAG,PAD/reflect,ROLL,ARANGE,FILL,TRI,embedding and layout operations covered.|
|P2-09 / **TODO**|Port complete CPU Flash Attention subsystem|ggml/src/ggml-cpu/ops.cpp; ggml/src/ggml-cpu/common.h|All supported cache dtypes,masks,head/group counts,quantized KV,tile/tail and forward/back modes execute Goose; Apex attention trace.|
|P2-10 / **TODO**|Port recurrent,SSM and gated attention computations|ggml/src/ggml-cpu/ops.cpp|SSM_CONV/SCAN,RWKV6/7,GLA,GATED_DELTA_NET,solve-tri,lightning-indexer and DSV4HC supported variants match stateful multi-token corpus.|
|P2-11 / **TODO**|Port convolution,image and relative-position operations|ggml/src/ggml-cpu/ops.cpp|All registered1D/2D/3Dconv/im2col/col2im,pooling/upscale,window partition and relative-position modes have differential fixtures.|
|P2-12 / **TODO**|Port training,custom and miscellaneous CPU operators|ggml/src/ggml-cpu/ops.cpp; ggml/src/ggml-opt.cpp|Every residual GGML op enum has Goose support/identical unsupported error; loss/backward/AdamW/SGD/custom-callback contracts checked.|
|P2-13 / **TODO**|Port remaining quantization/serialization math and type traits|ggml/src/ggml-quants.c; ggml/src/ggml-cpu/quants.c; ggml/src/ggml-cpu/traits.cpp|Every non-removed type registry entry has Goose quant/dequant/traits where native supports it; IQ calibration/tables and Q8_1 included; exact size/layout preserved.|
|P2-14 / **TODO**|Port CPU backend device/buffer/capability registry|ggml/src/ggml-cpu/ggml-cpu.cpp; ggml/src/ggml-cpu/hbm.cpp; ggml/src/ggml-cpu/traits.cpp|Backend API,buffer alignment/copy,capability predicates and device initialization owned by Goose; OS/HBM primitive imports audited.|
|P2-15 / **TODO**|Port CPU graph executor,task planning and abort paths|ggml/src/ggml-cpu/ggml-cpu.c|Entire operation dispatch,n_tasks/workspace planning,graph reuse,threadpool job lifecycle and abort status execute Goose; all operator entries traced.|
|P2-16 / **TODO**|Close CPU architecture support and residual source inventory|ggml/src/ggml-cpu/**; ggml/src/ggml-blas/**|All CPU audit rows assigned and zero owned native executable logic remains in candidate; hardware-unavailable variants stay explicitly incomplete.|
|P2-17 / **TODO**|Run CPU-only functional milestone across model families|tests/test-backend-ops.cpp; src/models/**|Full supported operator/type matrix plus Apex and tiny representative dense,recurrent,vision graph fixtures pass; witnesses prove Goose, not fallback.|
|P2-18 / **TODO**|Remove replaced CPU implementation from candidate production link|ggml/src/ggml-cpu/**|Candidate link map contains only Goose-emitted CPU logic plus audited primitives; frozen original remains outside candidate for rollback. Depends: P2-01,P2-02,P2-03,P2-04,P2-05,P2-06,P2-07,P2-08,P2-09,P2-10,P2-11,P2-12,P2-13,P2-14,P2-15,P2-16,P2-17.|

#### P3 — Complete CUDA backend

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P3-01 / **PARTIAL**|Finish complete CUDA normalization/elementwise module|ggml/src/ggml-cuda/norm.cu; ggml/src/ggml-cuda/unary.cu; ggml/src/ggml-cuda/scale.cu; source/rms-block/**|Allnorm/unary/scale variants and launch policy use Goose logic; shape,type,aliasing and tail cases exact/declared tolerance. Evidence: EXP136/137 actual SiLU/scale leaves and full RMS row; other operations/launch control native|
|P3-02 / **TODO**|Port CUDA primitive math,reductions and conversions|ggml/src/ggml-cuda/common.cuh; ggml/src/ggml-cuda/convert.cu; ggml/src/ggml-cuda/reduce_rows.cuh; ggml/src/ggml-cuda/mma.cuh|Goose-owned device math/conversion/warp/block helpers; generated device code provenance; rounding,half,bfloat,warp boundaries checked.|
|P3-03 / **TODO**|Port CUDA copy,gather,scatter and tensor transformations|ggml/src/ggml-cuda/cpy*; ggml/src/ggml-cuda/getrows*; ggml/src/ggml-cuda/set*; ggml/src/ggml-cuda/pad*; ggml/src/ggml-cuda/concat*; ggml/src/ggml-cuda/roll*|All dtype/stride/tail variants match native and Compute Sanitizer bounds checks; asynchronous copies preserve order.|
|P3-04 / **TODO**|Port CUDA binary,broadcast and activation families|ggml/src/ggml-cuda/binbcast*; ggml/src/ggml-cuda/unary*; ggml/src/ggml-cuda/clamp*; ggml/src/ggml-cuda/snake*; ggml/src/ggml-cuda/softcap*; ggml/src/ggml-cuda/acc*; ggml/src/ggml-cuda/add-id*|Complete registered binary/unary/GLU type/mode matrix; all actual kernels generated from Goose, no original arithmetic callback.|
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
|P3-16 / **TODO**|Port CUDA host backend,devices and capability predicates|ggml/src/ggml-cuda/ggml-cuda.cu|Goose owns device selection,op dispatch,events,streams,capability checks,error propagation and graph capture/replay; CUDA SDK calls primitive imports only.|
|P3-17 / **TODO**|Port GPU/pinned/VMM pools and transfer/residency policy|ggml/src/ggml-cuda/ggml-cuda.cu; src/llama-expert-*.cpp|All allocation/pool/pinned ring/VMM/residency/copy decisions Goose; OOM,resize,reuse,cancel,device reset and asynchronous lifetimes checked.|
|P3-19 / **TODO**|Run whole CUDA backend functional/sanitizer milestone|tests/test-backend-ops.cpp; ggml/src/ggml-cuda/**|Supported matrix passes differential checks and Compute Sanitizer memcheck/racecheck/initcheck/synccheck where applicable; Apex CPU/GPU mixed paths execute Goose.|
|P3-20 / **TODO**|Remove replaced CUDA source logic from candidate link|ggml/src/ggml-cuda/**|Zero owned hand-written CUDA/C++ computation/orchestration in selected link; generated CUDA/C from Goose allowed and provenance checked. Depends: P3-01,P3-02,P3-03,P3-04,P3-05,P3-06,P3-07,P3-08,P3-09,P3-10,P3-11,P3-12,P3-13,P3-14,P3-15,P3-16,P3-17,P3-19.|

#### P4 — GGML core,allocator,ownership and scheduler

| ID / status | Concrete transfer | Source anchors | Completion criterion and evidence |
|---|---|---|---|
|P4-01 / **DONE**|Port allocation reuse/free-list/coalescing policy|ggml/src/ggml-alloc.c; source/allocator-reuse/**; source/allocator-coalesce/**|Bounded selected metadata policy exactly matches reference and executes in real inference. Evidence: EXP136 native service boundary retained; this does NOT mean physical allocator complete|
|P4-02 / **DONE**|Port tensor dependency/view lifetime and allocation-graph policy|ggml/src/ggml-alloc.c; source/tensor-lifetime/**; source/graph-plan/**|Selected dependency/view/refcount/release traversal state transitions exact and real inference witness. Evidence: EXP136 graph/lifetime evidence; full tensor ownership/scheduler still native|
|P4-03 / **TODO**|Port GGML tensor/context/object/type core|ggml/src/ggml.c; ggml/include/ggml.h; ggml/src/ggml.cpp|All public tensor/context constructors,shape/stride/view/alias invariants,type/operation tables and errors handled by Goose; ABI preserved.|
|P4-04 / **TODO**|Port graph construction,hashing,traversal and differentiation|ggml/src/ggml.c; ggml/src/ggml-opt.cpp|All graph expansion/copy/export/gradient/optimizer entrypoints mapped; cycles,shared views,empty graphs and malformed dimensions tested.|
|P4-05 / **TODO**|Port full tensor allocator and physical buffer ownership|ggml/src/ggml-alloc.c|Goose owns tallocr/dyn_tallocr/free blocks/chunks and object lifecycle; only malloc/mmap/device alloc primitives imported; failure/overflow/double-free guards.|
|P4-06 / **TODO**|Port graph allocator reservation,reuse and realloc|ggml/src/ggml-alloc.c|Goose owns gallocr reservation,multi-buffer planning,views,inplace reuse and growth; native allocation state unavailable in candidate; exact event/order corpus.|
|P4-07 / **TODO**|Port backend buffer,device,event and copy abstraction|ggml/src/ggml-backend.cpp; ggml/src/ggml-backend-impl.h|Every backend public/vtable lifecycle,view,multi-buffer,async copy/event/sync contract implemented; typed ownership explicit.|
|P4-08 / **TODO**|Port backend registration,dynamic loading and feature discovery|ggml/src/ggml-backend-reg.cpp; ggml/src/ggml-backend-dl.cpp; ggml/src/ggml-feats.h|Registry/device enumeration,plugin ABI,error behavior and capability queries Goose; dlopen/OS imports only.|
|P4-09 / **TODO**|Port graph backend placement and splitting|ggml/src/ggml-backend.cpp|Goose owns node/leaf assignments,split boundaries,buffer compatibility,offload and fallback policy; placement traces match source contracts.|
|P4-10 / **TODO**|Port cross-device copy planning and asynchronous dependencies|ggml/src/ggml-backend.cpp|Goose owns split inputs,copy slots,events,waits and lifetime edges; multi-stream/multi-sequence cancellation/failure fixtures pass.|
|P4-11 / **TODO**|Port unified CPU/GPU scheduler execution and graph reuse|ggml/src/ggml-backend.cpp|Reserve/compute/recompute/synchronize/reset/eval callback/reallocation paths execute Goose; graph reuse and cold/warm/abort traces checked.|
|P4-12 / **TODO**|Port meta backend,backend optimization and threading services|ggml/src/ggml-backend-meta.cpp; ggml/src/ggml-opt.cpp; ggml/src/ggml-threading.cpp|All meta/planner/optimizer helpers Goose; OS primitive boundary minimal; callback reentrancy and thread lifetimes checked.|
|P4-13 / **TODO**|Unify allocation and scheduler interfaces across both lineages|ggml/src/ggml-alloc.c; ggml/src/ggml-backend.cpp; source/*/bridge.h|Single Goose ownership model serves Apex; no duplicated hidden native policy; complete borrowed/owned lifetime table.|
|P4-14 / **TODO**|Check full graph/allocator lifecycle under failures|tests/test-backend-ops.cpp; tests/test-alloc.cpp; ggml/src/ggml-alloc.c|Allocation failure injection,invalid-size/view/copy,abort/reset/reuse and concurrent independent contexts pass scoped sanitizers; no new leaks.|
|P4-15 / **TODO**|Run unified scheduler actual model milestone|src/llama-context.cpp; ggml/src/ggml-backend.cpp|Apex and Qwen real prefill/decode,prefix reuse,branch/restore/cancel match reference with Goose scheduler and allocator witnesses.|
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
4. Run real Apex using the one fixed profile. Check text/IDs/probabilities or a predeclared numerical contract and actual Goose path witnesses. Record one full-system prefill/decode/RAM/VRAM row,plus exact configuration/library hashes. Do not run Qwen now.
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
