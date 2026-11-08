# EXP173: actual Apex Q4 FlashAttention on self-hosted Goose

**The standard Apex QF32/KVQ4_0 execution path is accepted in the isolated experimental chain. Production is unchanged.** This is not full Apex/runtime migration or complete attention support for every cache/type/architecture. EXP172 and `GGML_GOOSE_CUDA_FA=0` remain rollback paths.

Goose owns vector decode,Q8 query codec/Q4 K/V arithmetic,Q4-F16 staging,async loader indexing,TensorCore prefill,online softmax,stream-K distribution,masks/fixups/combine and host validation/launch/scratch policy. The final coherent prefill keeps scalar-field Q/V/max/sum state inside one Goose process block. It uses no legacy C++ per-thread state buffers and does not pass/return the large state across a function ABI. CUDA SDK,raw ISA,typed tensor access,pool/mutex/atomic effects remain explicit.

All9modules are emitted by the complete selfhosted Stage3 f849 and mechanically qualified;compute bodies match the real compiler emission. Original numerical FA kernel includes and the temporary original-kernel occupancy query are absent from the replacement object. Own Goose kernels supply SDK occupancy. Original units are retained separately for explicit rollback and unsupported configurations;their arithmetic is never called by the enabled in-scope path. Source closure for the whole CUDA backend remains open.

## Evidence and performance limits

27hostcases/17406checks pass. Full ordinary API8default+8originalrollback and long nonperiodic/non-dyadic KV8default+8rollback pass bitwise,with one graph capture per mode,canaries and readonly checks. Earlier vector,loader,staging and iteration corpora remain archived. A concrete shared-address defect previously justified one scoped memcheck;no broad sanitizer/concurrency/multidevice certification is claimed. Final actual Apex32768/128 matches every token ID,text and saved top5 probability,with127graph reuses,FA host/vector/prefill witnesses,status0 and7zero owned heaps.

Two opposite-order pairs use the **same CUDA DSO** and change only the FA original/Goose switch:

| Mean of2responses per arm | C++ FA rollback | Goose FA |
|---|---:|---:|
| Prefill tok/s |2583.778|2577.509|
| Decode tok/s |25.826|26.195|
| Sampled VRAM MiB |3246|3246|
| Sampled RSS MiB |21332.115|21310.756|

Differences are-0.243percent prefill/+1.427percent decode. All4responses are exact128/text/top5 and swap0. These small uncontrolled samples support preliminary **component parity**,not sustained3000/30 or parity with a fresh wholly-C++ runtime:other experimental modules are identical in both arms. Board telemetry and full commands are retained locally. See paired-component-results.json.

After deleting original occupancy/template dependencies,all4actual target-kernel PTX bodies remain identical to the paired artifact,normalizing only basic-block ordinal prefixes/source-location lines. SDK/API/longKV/rollback checks pass again. One final unpaired Apex response is2533.002pp/25.522tg,RSS21309.699MiB/VRAM3246MiB,swap0,exact128/text/top5. It is retained as a separate observation,not relabeled as one of the pairs. See apex-summary.json,owned-occupancy-ptx-equivalence.json and qualification.json. A final link-only visibility correction hides the two owned packet destructor symbols;all numerical/host objects remain unchanged and the scoped API/longKV/rollback/host/export gates pass. The immutable model receipt explicitly retains its pre-visibility-fix library hash;the active library is identified separately. No model repetition is inferred. Absolute3000/30 and full migration remain unmet.

## Retained attempts and causal limits

The earlier accepted checkpoint107e17d specialized vector private-array indices;the next source step also expanded its32step KQ/VKQ loops. Full prefill loop expansion alone was exact but did not recover system prefill. An intermediate large-POD argument/return variant was GPU-exact but created actual ptxas spill traffic,and was withheld before another model request. Its source/C/resource report is preserved. The final inline state is exact and reaches paired component parity;its static640/672Bstack is not claimed optimal or spill-free.

The historical approximately18percent slowdown was not initially isolated in one library. Code changed before the final pairs,so it cannot be declared wholly false or wholly compiler-caused. Whole-model CUDA traces are explicitly diagnostic:tracing overhead affects throughput,and unchanged MMQ/GDN/concat kernels varied too. Static instruction counts are not operation timing. Earlier0spill/528Baddressed-state evidence belongs to its own artifact;later actual-spill evidence belongs to the large-POD variant. All failures and request roles are retained in apex-request-history.json and the old summaries.

## Reproduction

On this machine,the qualified baseline build,compiler snapshots and fixture objects are retained under runtime/ and the pinned llama.cpp-flint-cuda build. `python3 verify_generated.py` freshly emits all9Goose modules and checks mechanical compute bodies. `python3 build_runtime.py` verifies them,builds one isolated CUDA/host unit,reproduces the unchanged EXP172 link SHA,then links the replacement with a privately renamed original host object. It never edits production. Existing qualified fixture object prerequisites and their exact compile argv are retained in the component/full-local result receipts;owned-occupancy-check.py relinks/runs the full and longKV corpora. Full fixture source is tracked.

Tracked `.goose` is the canonical source for a rebuild. The source-specialization and value-state scripts preserve how it was derived;they are not required on each build. Historical diagnostics refuse to overwrite their model receipt. Do not rerun old worker-preparation scripts over qualified source. Raw model responses/traces,binaries and private compiler feedback remain local.

## Compiler readiness and remaining work

New complete EXP303Stage3 snapshot871aec9cc92d has verified bootstrap fixed-C provenance. Its output for all9current modules equals qualifiedf849C byte for byte,and a formerly rejected contextualf32literal-branch probe passes. It is selected for subsequent work with f849rollback retained. Existing EXP173binaries were produced by f849;no extraApex or speedup claim follows from identical emittedC. Float-to-integer support is not part of EXP303. See compiler-exp303-qualification.json. Compiler sources were not modified here.

P3-12 staysIN_PROGRESS for broader cache/type/architecture contracts. Next is actual Apex SET_ROWS cache/state integration,then remaining quantized CUDA paths. Two external DeepSeek workers supply isolated drafts/reviews;all accepted logic receives parent review,actual compiler/ABI/CUDA gates and system checks at significant milestones. No universal requirement,Apex delivery batch or complete runtime is falsely markedDONE.
