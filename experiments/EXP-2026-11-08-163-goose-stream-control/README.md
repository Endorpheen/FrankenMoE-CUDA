# EXP163: generic stream/tensor I/O/copy/event/device control in Goose

Solo continuation from EXP162.48generic backend/interface bodies now execute actual self-hosted Goose:backend introspection/free/default type,1D/2D synchronous/asynchronous I/O,memset,synchronization,graph plan/compute routing,copy selection/staging/fallback,events,device/registration-interface dispatch and optional callback behavior. This is shared control,not a claim that the underlying CPU/CUDA providers or scheduler were ported.

## Ownership,ABI and ordering

`stream.goose` owns validation,callback presence/default selection,1D/2D fallback loops,layout comparisons,host/direct/staging copy routes,both-backend synchronization,graph error/sync ordering,device props initialization and event-owner dispatch. `stream-bridge.h` projects typed fields and invokes only chosen provider callbacks,raw staging allocation/free and accepted Goose155/162 component APIs. It never calls an original generic I/O/copy/event algorithm. Goose compiles as a separate C unit;one foreign header is included before generated C and C++ definitions,checking all64-bit arguments/returns at compile time. All callback calls use exact typed signatures.

Synchronous compute always synchronizes after a returned error,as before. Async copy tries the destination callback first;on decline it synchronizes source then destination before blocking copy. 2D fallback and zero-size callback behavior are deliberately different between sync/async paths and preserved. A host source may copy into a view that has only its owner buffer:the direct dst-buffer check is postponed until the route actually needs it. View-owner selection matches the original API.

Checked tensor/address/stride arithmetic validates entire nonzero 2D spans before any row transfer. Staging allocation failure rejects before I/O. Allocation failure exceptions from I/O callbacks become status inside the C++ effect boundary;Goose releases any staging storage before failing. Buffer/event/backend resources remain caller/provider owned. This is scoped safety/error handling,not a whole-runtime safety claim or a transactional payload promise for arbitrary throwing provider implementations.

Cached default-on `GGML_GOOSE_STREAM_CONTROL=0` selects exact original bodies. Complete prior EXP162runtime remains immutable rollback. Only the backend-control object and new generated C unit are changed;CPU/CUDA/libllama hashes and all numerical kernels remain unchanged. Baseline relink SHA is verified before replacement. Production exports only read-only witness/staging counters;mutating hooks exist only in a separate test image.

## Local checks

Native EXP162 and candidate base-object images use separate loader namespaces,with no new witness in the native control.37case groups/264cross-backend callback records and1091checks compare data,canaries,full48entry coverage,required/optional methods,sync/async 1D/2D,n=0/1/4 copies,zero/nonzero sizes and all host/direct/staging/copy-async routes. The matrix covers host->unbound-view,source/destination identity,device/register/procedure pointer ABI,event ownership,NULL/optional factories,graph plans,error+sync ordering and optional optimization callbacks. Explicit native rollback has zero new witness bits.

11negative cases reject out-of-bounds,overflow,NULL/unallocated payload and invalid 2D spans before payload mutation. Staging OOM and2bad_alloc callback cases verify staging is freed before failure. Source and destination contain distinct bytes so an early write cannot be hidden. Zero-size memset retains the original allowance for unbound tensors.8threads run128independent transfer/event lifecycles with128staged copies,data exact and no staging/buffer/context heaps. C++ exception cases use primary namespace matching actual server;ordinary native-success oracles remain namespace-isolated. No full sanitizer suite or microbenchmark.

## One fixed Apex32K/128 gate

| Metric | EXP162 | EXP163 |
|---|---:|---:|
| Prefill tok/s |2639.393|2631.942|
| Decode tok/s |27.387|26.956|
| RSS peak MiB |21315.066|21314.930|
| Sampled VRAM MiB |3246|3246|
| Swap KiB |0|0|

-0.282%prefill/-1.573%decode vs EXP162in one uncontrolled sample. The decrease is retained;stable C++ parity or acceleration is not proven. Archived same-profile original C++2867.846/27.978remains historical. Exactly one model request,no repeated performance series or tuning.

All128token IDs,text and every saved probability/top5 array exactly match original C++;127graph reuses. Actual generic-control mask `0xf3ffc4fa233e` is witnessed during model load/inference/shutdown;the exact entry list is saved in `apex-summary.json`. This profile did not execute the staging/async-fallback markers50/51;those paths are verified by local and concurrent fixtures,not falsely claimed as real-Apex paths. All staging,buffer,allocator,graph/context heap counters return to zero. Expected new base SHA is mapped. Raw requests,responses/logs/memory/process receipts remain local under `runtime/exp163/results/apex-32k128/`,not published.

## Reproduce and next scope

`build.py` emits the pinned completed full self-hosted producer and qualifies its C helpers,checks the unchanged link and replaces selected control. `check.py` checks native parity/all48entry modes and rollback. `check_faults.py` checks malformed requests,staging/callback failures and independent threads. `run_apex.py` performs the fixed canonical profile once and refuses existing receipts.

The remaining substantial common-backend code is actual graph placement/splitting,copy slots/input lifetime dependencies/reservation/compute scheduling,global dynamic registration/loading and graph-copy utilities. Provider-specific CPU/CUDA callbacks/devices/kernels and other model/KV/loader/tokenizer/server/source-removal scope remain. P4-07andP4-08stay partial/in-progress. Next:the real shared scheduler placement/split/owned-state path,using preserved earlier design work but authoritative active source. No production/compiler changes or auxiliary agents,Qwen deferred.
