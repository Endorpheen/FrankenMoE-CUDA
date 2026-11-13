# EXP-2026-11-07-150: shared expert policies and CPU device registry

**Functional checkpoint: P2-14 complete; P1-10 advanced,not complete.** This is part of the first large delivery cycle,not a complete CPU/CUDA subsystem or40-50requirement closure. Production/compiler sources unchanged. The qualified latest completed self-hosted EXP301 Stage3 remains pinned at f849233b999c;compiler HEAD d030e533 was rechecked.

## Real Goose implementation

- CPU registry/device initialization,properties,brand acquisition/string sequencing,backend/buffer routing and HBM allocation/failure cleanup join inherited real Goose backend/buffer/capability/trait policies. Native code holds ABI records/containers or invokes explicit OS/vendor services. Old bodies are disable rollback.
- Every MoE-local tiled-workspace query in the integrated CPU core reuses the existing Goose graph policy;the old native helper is used only with EXP150 expert-policy disabled.
- Complete SLRU state-transition algorithm owns initialization,promotion,probation/protected links,victim selection,request protection,deduplication,wrap-safe generations,hits and fill ordering. Cache eligibility,read spans and contiguous-upload grouping are Goose.
- A shared Goose planner owns GPU budget distribution,zero-free-memory fallback,aligned expert bank sizing with the CUDA overread guard slot,slot capacity and tensor counts. The compiler rejects both f64-to-i64 casts;Goose decodes IEEE binary64 exponent/mantissa after range checks. The only native helper copies bits with memcpy;native numeric conversion is not called.

This does not complete cache tensor/resource construction or full copy/upload lifecycle;P1-10 remains open. Cold/fused Qwen execution is deferred. CPU feature/provider math,architecture qualification,remaining ABI storage and native removal are separate original requirements,not declared unavoidable final dependencies.

## Consolidated correctness and safety

30296cache/budget scenarios compare actual native policy and Goose,including all state,slots,fills,hits,list invariants,generation wrap,duplicates,overcapacity,device splits,zero-free fallback,alignment and budgets.210registry/OS/HBM-policy cases include failures and cleanup.5additional compiled HBM API cases use a substitute allocator with actual buffer clear/free/live-allocation checks;no HBM hardware claim. Release and scoped ASan/UBSan/LSan pass. New touched units/fixtures are instrumented;unchanged qualified EXP149 objects are reused;vendor/other inherited numerical objects are not all instrumented. This is not a whole-runtime safety superiority claim.

The actual corrected CPU DSO passes326common graphs,66GLU/embedding cases,960planning and174backend/buffer cases,including native comparisons and rollback. New registry and scratch disable flags separately reproduce enabled outputs exactly. The SLRU corpus compares both implementations per case.

An integration mistake was found during the compiled HBM gate:copied short CPU-library names still contained inherited bytes,and the new scratch helper was absent from the integrated core. Early common results were explicitly archived/invalidated;direct new object fixtures were unaffected. Aliases/integration were corrected and both common release/sanitized gates rerun before any Apex request. Loader relocations,aliases,actual loaded paths and model-library hashes are recorded. HBM also exposed a preexisting standalone source dependence on inaccessible static CPU helpers;the isolated module uses public CPU buffer APIs instead. No production changes.

## One fixed Apex32768/128 request

| Build | Prefill tok/s | Decode tok/s | RSS MiB | VRAM MiB |
|---|---:|---:|---:|---:|
| Saved original C++ |2918.900|29.589|21309.152|3348|
| Previous EXP149 OMP |2646.558|28.462|21311.797|3348|
| EXP150 OMP |2656.285|28.079|21312.090|3348|

Response text,all128token IDs and all saved top5 probabilities match original C++. Registry modes0-9 and127680Goose expert-workspace calls are traced. Fixed profile has no model-cache/HBM allocation,so their policies rely on explicit separate gates and are not claimed exercised in this Apex request. No process swap/system swapout. These are single uncontrolled-load diagnostics using a saved C++ measurement,not fresh repeated paired confirmation of speed parity or stable3000/30. Full performance acceptance remains open;no promotion.

## Same-host reproduction and rollback

Canonical isolated tree:/home/end0/AI/LlamaServer/franken-goose-exp150-expert-registry-20261107. `reproduce/build.py` rebuilds changed objects and relinks against the preserved qualified base;`--sanitize` builds the scoped instrumented variant. It requires the recorded toolchain/native object lineage;this is not yet a standalone native-free runtime. `tests.py` checks new policy modules,`common_tests.py` the shared corpus,`hbm_check.py` the compiled allocator-stub path,and`rollback.py` new disable switches. The one-shot model script has a consumed receipt;do not rerun it without a new justified gate.

`GGML_GOOSE_CPU_REGISTRY=0`, `GGML_GOOSE_EXPERT_POLICY=0`, and `GGML_GOOSE_EXPERT_CACHE=0` restore their own native policies. Other existing Goose flags remain independent. The original production and earlier EXP149 artifacts remain available. Source/source-boundary audit,producer,generated C,commands,failed-gate invalidation,raw model evidence and checkpoints are retained. Private COMPILER_FEEDBACK.md remains local ignored/untracked.

## Final typed-ABI correction and next model gate

After the recorded diagnostic,a final source audit replaced flat int32 pointer arithmetic across fill struct subobjects with typed vector/member access. Cache/budget release and scoped sanitizer gates passed again. The CPU DSO/Goose algorithms are unchanged. The model response remains valid for the library hashes in its original receipt;it is not relabeled as a run of the post-correction llama DSO. A prepared followup Apex was not launched:the owner requested minimum runs and one common model gate at the end of the large delivery cycle. P1-10 remains incomplete. Final-cycle integration must test the exact complete candidate. See post-fill-abi-correction.json and retained pre-correction source.
