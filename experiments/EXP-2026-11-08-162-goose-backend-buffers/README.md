# EXP162: backend bulk buffer planning and ownership in Goose

Solo continuation from EXP161.30shared backend buffer/type/bulk/multi/view-binding bodies now route to one actual self-hosted Goose service. Native provider callbacks,accepted allocator/core/CPU/CUDA code and the scheduler remain unchanged except the selected backend-control object. Goose is compiled as C;explicit matching C/C++ foreign declarations are shared with the typed C++ effect bridge. No compiler/production changes or helper agents.

## Transferred control and effect boundary

`buffers.goose` owns required/optional callback selection/defaults,buffer descriptor fields and lifetime,alignment/max/allocation-size contracts,default tensor grouping/splitting/size calculation,bulk allocation/init/cleanup,multi-buffer ownership/usage/clear,view and tensor binding,and allocation-size expansion classification. `buffers-bridge.h` only projects typed fields,performs typed new/delete/memcpy effects,executes selected provider callbacks and reuses accepted Goose161 tallocr/Goose155 size logic. No original C++ vector-based plan/default-bulk algorithm is called by the default active path. Internal bit40is emitted from inside Goose build_plan,not just an outer wrapper.

Default-on `GGML_GOOSE_BACKEND_BUFFERS=0` retains original bodies. The older complete EXP161 runtime is immutable rollback. Baseline base DSO relink must reproduce its original SHA before replacing one control object and adding the generated C unit. CPU/CUDA/libllama hashes remain unchanged. Foreign effect signatures are declared before both compilation units,so width/type mismatches fail compilation. Only two read-only witness counters are exported;mutating hooks are confined to separate non-active test DSOs.

Typed allocations start C++ POD object lifetimes correctly;new/delete kinds match. Packet workspace is call-local. Pointer representations and complete tensor snapshots use memcpy,not incompatible pointer/integer lvalues. Zero-sized dummy buffers,missing optional callbacks,view-copy destination selection and nested multi usage/clear/free preserve valid native behavior. Counts/alignment/products/address ranges are checked before unsafe access/allocation;direct duplicate multi children are rejected before ownership transfer.

## Corrected failure behavior

Native EXP161independently leaves stale tensor buffer/data metadata in5of6injected provider failure cases after releasing the corresponding buffers. The Goose default bulk path snapshots every input tensor before allocation,retains successful metadata on success,and restores the complete snapshot after failed allocation/init/wrapper creation and resource cleanup. Borrowed external tensors/buffers survive. This is an intentional safety correction,not a native failure-state equivalence claim.

All12bulk metadata/descriptor OOM sites returnNULL with restored metadata and no owned heaps. All3multi-wrapper allocation failures leave child ownership with the caller. A failed generic descriptor constructor releases its provider context through the supplied free callback and returnsNULL;opaque context without a free callback remains caller-managed. std::bad_alloc from single-buffer alloc/init callbacks is translated toNULL/ALLOC_FAILED before crossing the C boundary,allowing Goose cleanup. Other provider-specific behavior is not silently claimed ported.

## Local validation

Two independently linked base object compositions in separate loader namespaces:control EXP161 has no FG162witness;candidate includes the exact new C/CPP unit. The accepted allocator corpus remains exact:10800dynamic operations,12graph cases,495physical backend event records/17578checks;native backend rollback passes.12bulk grouping scenarios and6provider failures compare successful layouts/statuses and491physical event records. Native stale metadata is reproduced,Goose rollback confirmed,borrowed data retained,nested multi lifetime and duplicate ownership rejection checked.

243callback/default checks cover required/optional/custom providers,single and view-copy ABI,dummy buffers and all101size-expansion opcodes. OOM probes cover12bulk and3multi sites,descriptor/provider cleanup and2bad_alloc callbacks.8threads run128independent bulk lifecycles with borrowed views;heap/backend counts return to zero. Exception callbacks are tested in the primary namespace matching the real server:thrower/catcher in separate dlmopen C++ runtimes aborted the harness,while the identical candidate passes in primary. Native-success parity stays namespace-isolated;this test adaptation is recorded,not reported as a compiler/runtime defect. No full sanitizer suite or microbenchmark.

## One fixed real Apex32K/128 gate

| Metric | EXP161 | EXP162 |
|---|---:|---:|
| Prefill tok/s |2633.888|2639.393|
| Decode tok/s |27.425|27.387|
| RSS peak MiB |21315.945|21315.066|
| Sampled VRAM MiB |3246|3246|
| Swap KiB |0|0|

+0.209%prefill/-0.140%decode are small uncontrolled single-sample changes,not stable acceleration or fresh C++ parity. Saved original same-profile C++2867.846/27.978is historical. Exactly one model request after final implementation/local gates;no performance series or tuning.

Text,all128token IDs and complete saved probability/top5 arrays exactly match original C++;127graph reuses. The new library is mapped with its manifest SHA. Actual witness `0x100180dfbef` includes internal Goose default-plan bit40. Multi-buffer behavior is locally verified but this profile did not create a multi aggregate;do not claim runtime bit41. Buffer,allocator,graph and context heap counters are zero at process exit. Raw requests/responses/logs/memory/process receipts remain local under `runtime/exp162/results/apex-32k128/`,not published.

## Reproduce and residual scope

`build.py` pins the completed full self-hosted producer,checks the unchanged link,compiles a generated C unit and typed C++ bridge,and replaces only backend control. `check.py` runs allocator parity,rollback,bulk ownership and callback contracts. `check_faults.py` covers metadata OOM,bad_alloc and independent threads. `run_apex.py` uses the unchanged fixed profile once and refuses an existing receipt.

The common imported bulk-planning boundary from EXP161is now Goose. Provider-specific alloc_buffer_n/get_alloc_size_n callbacks and meta-specific classification/usage remain typed opaque provider boundaries. Generic stream/tensor I/O/copy/event/device/registry/scheduler control and remaining numerical/KV/model/loading/server/source removal stay open. P4-07is IN_PROGRESS,not complete;full Apex100percent is unproven. Next:one coherent generic stream/tensor-copy/event/device control block. Working production and compiler untouched.
