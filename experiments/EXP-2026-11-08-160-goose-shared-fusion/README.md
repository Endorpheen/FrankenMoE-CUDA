# EXP160: shared Goose fusion/parameter/hash control across CPU/CUDA host

Continue solo from EXP159. A single Goose service now owns use-count/elision,linear/sequential/subgraph fusion and edge predicates;operation-parameter bounds/bit packing;RoPE offsets,GLU parameter composition,argsort-top-k views and padding composition. Accepted EXP159 Goose hash/bitset logic is reused across selected inlined backend copies. No compiler/production changes or external workers.

## Five selected translation units

- GGML core: shared policy implementation,core fusion API and remaining composition helpers.
- Scheduler/backend buffer unit: original accepted backend code retained;shared hashing replaces active header copies.
- Meta backend: shared helpers compiled and linked;this Apex profile did NOT execute the meta caller.
- CPU core: accepted EXP150 MoE/executor/quantization code retained;shared parameter/header calls.
- CUDA host: exact active EXP107 lineage retained;shared fusion/parameter/header calls.

All three unchanged baseline base/CPU/CUDA link recipes reproduce the active SHA before replacement. Only the selected five objects are replaced;all other CPU numerical objects,CUDA leaf objects,earlier Goose modules and compiler are unchanged. Link with strict undefined-symbol checks. Immutable object-hash outputs and exact manifests remain. After one core-only zero-byte copy guard refinement,the unchanged four caller objects were reused only with verified command/source/header/object hashes;relinking unchanged CPU/CUDA outputs was also SHA-checked. Cold reproduction without a cache recompiles the same selected objects.

## Actual ownership and ABI

`service.goose` owns all predicate loops,validation,shape comparisons,use-count decisions,constant-view recognition,cycle-safe view-chain checks,parameter limits/slots and node result dimensions. Imports are typed projections,memcpy bit effects,buffer usage accessors and already accepted Goose159 hashing/Goose158 GLU/sort/Goose156 constructors. Native operation algorithms are not called by default active policies. Padding signed adds are checked before allocation;parameter rejections precede mutation. Zero-byte NULL copy avoids invoking memcpy on a NULL source. Scalar parameter access uses memcpy and preserves all32bits,including NaN payloads and signed zero.

`transport.h` includes the unchanged original internal header under private native names,then defines ABI wrappers. This avoids pragma-once header duplication and retains exact native fallbacks. It is used in the selected caller units;the core uses its existing qualified header plus the shared service. Env `GGML_GOOSE_FUSION=0` disables fusion/parameter/composition replacement. `GGML_GOOSE_GRAPH_LIFECYCLE=0` disables transported hash/bitset replacement. Previous EXP159 runtime remains a complete rollback. New exported service and two initialization-only flags are trusted native ABI,not a network API. No persistent queues or buffers are introduced.

## Local correctness and memory contracts

Native EXP159 and candidate probes are in separate loader namespaces. Each probe has DT_RPATH pinned before LD_LIBRARY_PATH;the fixture explicitly asserts no EXP160 query symbol in the native namespace and a query symbol in the candidate. This prevents a self-referential oracle.

714native predicate comparisons and1655checks cover use counts,outputs/compute/param flags,views,shape mismatches,source order/edges,invalid sequence ends,linear/subgraph windows,external immutable weight buffers,parameter copy tails/all16slots,integer extremes,float NaN/sNaN/infinity/signed-zero bit patterns,and complete composition-node metadata/aliases/arena placement. Fusion-only and full fusion/hash native rollback pass with zero low service-witness bits.

15malformed-input cases include10controlled pre-mutation rejections:parameter index/size/NULL-source,invalid RoPE offset/op,and padding negative extent/signed overflow. Additional checks cover negative fusion/edge indices,full absent-key hash,zero-byte NULL copy,and a valid64long borrowed immutable-view chain followed by rejection of its cycle. No full sanitizer suite or microbenchmark;this is scoped safety evidence.

## One real Apex32K/128 gate

| Metric | EXP159 final | EXP160 |
|---|---:|---:|
| Prefill tok/s |2635.438|2636.702|
| Decode tok/s |27.865|27.185|
| RSS peak MiB |21316.418|21314.910|
| Sampled VRAM MiB |3246|3246|
| Swap KiB |0|0|

+0.048%prefill/-2.438%decode vs last EXP159. This is an uncontrolled single sample;do not label it stable speed parity or acceleration. The observed decrease is retained for the final controlled performance audit. Archived same-profile original C++2867.846/27.978 remains historical,not a fresh paired control. Exactly one model request,no repeated series or optimization.

Text,all128token IDs and every saved probability/top5 array exactly match original C++;127graph reuses. All three expected new libraries are actually mapped with manifest SHA. Actual service witness masks:core0x11410,CPU0x400,CUDA0xc28,scheduler0x100000000000. CPU exercised integer parameter reads;its fusion branch was not exercised by this profile. CUDA exercised sequential/subgraph fusion and integer/float parameter reads. Scheduler exercised Goose159 find-or-insert. Meta was compiled/local-helper checked but has no Apex runtime witness;do not claim otherwise. Graph/context outstanding heap counters are zero. Raw requests/responses/logs/memory/process data stay local under `runtime/exp160/results/apex-32k128/`,not published.

## Reproduction and residual scope

`build.py` verifies baseline relinks,pins the completed self-hosted producer,emits the shared service and compiles/links selected units. `check.py` runs independent native and two rollback paths. `check_errors.py` checks bounds/rejection/cycle contracts. `run_apex.py` runs the unchanged fixed profile once and refuses an existing receipt.

Other leaf/operator translation units still contain native parameter/helper inlines until those units migrate. Numerical kernels,CUDA device management/capability/launch ownership,complete allocator/scheduler/KV/model/loading/tokenizer/server and final source/import removal remain open. This is a coherent shared control/parameter block,not full P3-16/P4-03/P4-04 or100percent Apex. Continue the actual Apex allocator/buffer ownership path using the preserved allocator/coalescing/lifetime work;Qwen remains deferred,no auxiliary agents.
