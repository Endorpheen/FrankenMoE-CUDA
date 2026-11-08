# EXP157: Goose context lifecycle and core metadata

Continue solo from verified EXP156,without EXP153 worker code. Thirty-seven existing API/helper entrypoints route to actual self-hosted Goose policies. Production,compiler source,the numerical kernels and default launch profiles are unchanged.

## Transferred behavior and boundaries

`source/context-policy.goose` owns context creation,owned-versus-borrowed arena selection,alignment/size checks,allocation failure cleanup,reset/free,used-memory and no-allocation state;Linux POSIX allocation/error policy;type/operation/unary/GLU/status lookup and validation;ftype mapping;name copying,flags/view propagation and typed tensor metadata access. `registry-data.goose` initializes the active registry and labels in Goose from independently reflected EXP156 ABI facts. The reproducible compiler input is `context.goose`.

Typed C imports perform POD reads/writes,raw allocation/free,zeroing,clock and mutex primitives. Existing numerical callback addresses are copied into Goose-initialized records;their callback bodies are explicitly NOT migrated by this experiment. The legacy registry/bodies remain available only for explicit rollback or unresolved callbacks. Generated C runtime helpers are mechanically qualified for composition with EXP155/156;the compiler is not modified.

Default-on cached `GGML_GOOSE_CONTEXT_CORE=0` selects the original implementation. The qualified host is Linux POSIX,x86_64 without CPU HBM;other platform/HBM allocation adapters remain native and unqualified. Public struct layout,callback ABI and names are unchanged. Owned buffers are padded to GGML alignment;borrowed buffers retain exact caller size and ownership. Overflow is rejected before allocation. Failure after descriptor creation releases the descriptor before aborting. Reset retains backing storage and payload. Sparse thread-local witness bits avoid per-call atomic increments.

## Correctness and memory checks

- Independent unchanged EXP156 DSO in a separate dynamic-loader namespace:4500 registry/lifecycle/metadata checks cover all43enum slots,101operations,22unary and7GLU labels,ftype success/rejection,statuses,owned/borrowed buffers,padding,reset/reuse,no_alloc,canaries,names and flags. Explicit native rollback passes the same corpus with zero new Goose bits.
- Eight independent context threads complete256create/reset/free cycles;the descriptor ownership counter returns to zero.
- The full inherited constructor corpus passes on EXP157:35formats,2800paired tensors,429444checks,cross-context borrowed payloads,ten native/candidate transactional fatal cases and four additional overflow guards.
- A separate non-active `FG157_TESTING` image injects failures only at OS allocation effects. Eight controlled abort cases verify descriptor cleanup and no borrowed free:context descriptor/buffer OOM,borrowed descriptor OOM,bad alignment,size overflow,private malloc/calloc OOM and calloc multiplication overflow. Three zero-size cases and aligned allocation refusal also pass. This is scoped safety evidence,not a whole-runtime sanitizer or race-safety claim.

## One fixed real Apex32K/128 gate

Text,all128token IDs and complete saved probability/top5 arrays exactly match the frozen same-profile C++ response;127graph reuses. New Goose mask `0x5703f35` is witnessed during real inference;all context descriptors are released at process exit. Inherited geometry and constructors remain active.

| Metric | Original saved C++ | EXP156 | EXP157 |
|---|---:|---:|---:|
| Prefill tok/s |2867.846|2639.870|2638.849|
| Decode tok/s |27.978|27.247|27.574|
| RSS peak MiB |historical control|21315.863|21315.961|
| Sampled VRAM MiB |historical control|3246|3246|
| Swap KiB |historical control|0|0|

EXP157 changes -0.039%/+1.203% versus EXP156 in one uncontrolled sample. This is no material observed incremental regression,not stable acceleration or fresh paired C++ parity. Historical C++ throughput remains higher on prefill;that existing difference is not hidden. Model requests:one after final implementation,no repeated series or microbenchmark.

## Reproduction and remaining work

`build.py` pins the completed self-hosted EXP301 Stage3,emits checked-in Goose C,verifies unchanged EXP156 relink SHA and replaces only its core object. All inherited objects/fast kernels stay unchanged. Outputs have immutable object-hash names. `check.py` runs the independent core/rollback/concurrency corpus;`check_faults.py` builds only the instrumented core object and isolated failure fixture. The inherited EXP156 constructor fixture is run against the new library using its recorded command in `build-commands.json`. `run_apex.py` uses the unchanged fixed request and refuses overwrite of an existing model receipt. Raw request/response/log files stay local under `runtime/exp157/results/apex-32k128/` and are not published.

P4-03 stays IN_PROGRESS:the real context/arena/type/metadata path is substantially transferred,but remaining public core APIs and graph-operation builders need migration/source closure. Numerical callback bodies,backend allocator and scheduler are separate open scope. No full requirement or full Apex migration is claimed complete. Next:remaining core/graph-construction behavior,with the same scoped correctness and one Apex gate after each significant change. No auxiliary agents.
