# EXP161: release tensor/graph allocator in Goose

Solo continuation from EXP160.42nontrivial release allocator bodies now route to actual self-hosted Goose:aligned/tallocr allocation,free-block/chunk selection and coalescing,dynamic allocator lifecycle,virtual backend buffers,gallocr creation/ownership/growth/reservation,views/inplace lifetime reuse,graph assignment/reallocation and context tensor collection/allocation utilities. Thin convenience ABI wrappers remain mechanical. Numerical kernels,compiler and production are unchanged.

## Authoritative baseline and reuse of prior work

Active EXP160 `base-ggmlalloc.o` has no historical Goose allocator symbols and reproduces bit-for-bit from the pinned native1230line source:SHA9246ec1bc84e9f0e5d65ba52f7f3b3c7e2af5c013ac19b258a02ac9da67a3149. Archived EXP136reuse/coalescing/lifetime policies were not active in the restored EXP150..160 lineage. Their sources are preserved under `source/prior-design/`;the new scalar/POD implementation retains their algorithms and extends them to the complete release module. Do not count legacy env flags alone as allocator liveness evidence.

## Logic/effect boundary and ownership

`allocator.goose` owns checked sizes/alignment,all free-list/chunk and graph-state transitions,buffer ownership/deduplication,growth fallback,allocation/lifetime/view decisions,metadata reservation/reuse and context collection loops. `allocator.h` provides typed POD/array access,memcpy pointer representation transfers,raw heap effects and existing backend API calls. Accepted Goose159/160hashing and earlier Goose tensor/context components are reused. No original allocator policy is called by the active default path.

Exact native per-chunk selector tie order,tail-block signed reuse calculation,zero-size allocation semantics,shared buffer-type aliases,output/view restrictions,logical addresses,buffer growth/reset and physical callback order are retained. New arithmetic/ID/overlap guards reject invalid operations before unsafe state writes. Unpublished gallocr construction cleans partially allocated arrays/dynamic allocators on OOM. Failed metadata growth preserves owned state until the caller closes the handle. Backend allocation failure returns false and cleans new physical buffers;growth failure retries exact size once. Existing backend tensor-init status handling is preserved;broader backend error-contract changes are separate scope.

A native-valid zero-size corner was caught before any model run:an empty free-list marker at1888was inside the released1776..2224span. Overlap checks now exclude empty intervals,while nonzero double release remains rejected. `development-findings.json` preserves trigger/correction. This was integration code,not a compiler defect.

`GGML_GOOSE_ALLOCATOR=0` selects the exact retained native bodies. `GGML_ALLOCATOR_DEBUG` stays native/unqualified;the target is the actual Apex release build. Previous EXP160 runtime remains immutable. Only the allocator object is replaced and the unchanged complete base relink SHA is checked first. Production exports only two read-only witness counters;failure/probe hooks live in separate non-active test DSOs.

## Correctness and safety checks

Independent native and candidate base-object compositions use separate loader namespaces and the original object lists;hidden GGML hash helpers stay inside the correct DSO. Native namespace has no new allocator witness.10800deterministic allocation/release operations across three alignments and three chunk limits compare every logical chunk/offset,free block,high-water mark and reset state.12graph scenarios cover single/multiple/shared buffer types,views,inplace reuse,measurement/reserve/allocation/repeated allocation,growth limits and deduplicated destruction.495physical backend alloc/free/reset/init event records match exactly.17576assertions pass in both Goose and explicit native rollback;rollback has zero new witness bits.

Additional public tallocr/context-bulk tests cover misaligned initial buffer bases,zero tensors,17tensor sequences,padding and utility sizing/cleanup. Failure tests cover all7unpublished creation allocation sites,6fatal reserve sites,1recoverable reserve failure and double-free/overflow rejection.8threads run128independent graph-allocator lifecycles and256graph allocations;backend/context/allocator heap counters return to zero. No full sanitizer suite or microbenchmark. These are scoped ownership/error checks,not a claim of whole-runtime safety superiority.

## One fixed Apex32K/128 gate

| Metric | EXP160 | EXP161 |
|---|---:|---:|
| Prefill tok/s |2636.702|2633.888|
| Decode tok/s |27.185|27.425|
| RSS peak MiB |21314.910|21315.945|
| Sampled VRAM MiB |3246|3246|
| Swap KiB |0|0|

-0.107%prefill/+0.881%decode are small uncontrolled single-sample changes,not stable speed claims or fresh paired C++ parity. Saved original C++2867.846/27.978remains historical. Exactly one real model request after all implementation fixes;no repeated series or tuning.

Text,all128token IDs and complete saved probability/top5 arrays exactly match original C++;127graph reuses. Actual allocator witness `0xb18070000c`;allocator,graph and context outstanding heap counters are zero at shutdown. The expected new base library is mapped and its SHA matches the manifest. Raw prompts,responses,server/process/memory logs stay local under `runtime/exp161/results/apex-32k128/`,not published.

## Reproduction and remaining boundary

`build.py` pins the completed full self-hosted producer,emits/qualifies Goose,checks source/unchanged-link provenance and replaces one object. `check.py` builds independent original/candidate allocator probes and runs positive/native-rollback corpora. `check_faults.py` builds isolated failure/thread fixtures. `run_apex.py` runs the unchanged canonical profile once and refuses an existing receipt. Production and compiler source are untouched;no helper agents.

This closes the bounded release `ggml-alloc.c` module. It does NOT close the entire physical allocation path:backend `alloc_buffer_n/get_alloc_size_n` still contains bulk tensor grouping/splitting and provider policy. Debug diagnostics,strict native-body/source removal and broader P4-05/P4-06/P4-07contracts remain open. Next:the actual backend bulk buffer planning/ownership boundary,then remaining scheduler/KV/model/loading/server/numerical scope. Qwen remains deferred.
