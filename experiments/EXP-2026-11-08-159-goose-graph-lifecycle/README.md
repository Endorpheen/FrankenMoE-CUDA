# EXP159: inference graph/hash/traversal/lifecycle in Goose

Solo continuation from EXP158.22core bodies and9core-header helpers route to31real self-hosted Goose policies. They cover prime hash sizing,bitsets,linear probing/find/insert/contains,hash allocation/reset/free,graph byte layout/overhead/arena creation,forward traversal/order/selection,graph copy/dup/borrowed views/clear,and node/name/gradient metadata access. Production and compiler sources are untouched;no delegated workers or numerical kernel changes.

## Ownership and traversal

`source/graph.goose` owns all sizing,overflow checks,hashing/probes,allocation/error cleanup,graph layout,DFS frames/order,COMPUTE flags,use counts,classification/naming decisions,copy/rehash and borrow metadata. `graph.h` projects typed fields and performs raw malloc/calloc/free,memset,memcpy,formatting and accepted EXP156 arena allocation. No original native graph algorithm is called from an active Goose policy. Explicit cached `GGML_GOOSE_GRAPH_LIFECYCLE=0` selects original bodies.

The exact default layout,zero/default metadata,hash/gradient arrays,leaf-first name lookup,negative node indices,uid preservation and borrowed views match the reference. Iterative DFS uses a checked temporary four-word frame per possible hash slot,freed on success and bounded traversal failures;there is no generated-C recursion proportional to graph depth. It preserves native source ordering,already-visited compute upgrades,self-referencing cast nodes,PARAM leaves and edge-use counts. Copy into a different hash modulus remaps use counts and gradient references. Repeated destination copy safely reuses existing slots instead of indexing the native already-exists sentinel. Invalid capacity/index/slice/size/full-table input and malformed endless leaf cycles are rejected with bounded storage.

Only the core translation unit uses `ggml-impl-graph.h`. Identical header inlines embedded in other previously compiled CPU/CUDA/backend units remain native until those units are migrated. Differentiation,training/optimizer reset,graph print/export/DOT and core fusion predicates are NOT claimed ported. Atomic graph UID allocation remains an unchanged primitive. Thus neither P4-04 nor full Apex is declared complete.

## C bridge correction before acceptance

The first diagnostic inference was exact,but pre-commit review identified incompatible integer/pointer-array lvalue access in a generic storage helper. The final bridge uses memcpy for object-representation loads/stores,with a64-bit ABI assertion. `source/aliasing-fix.patch` records the actual correction;no compiler change or disabling of strict-aliasing optimization. All local tests and one final real model gate were rerun on the corrected immutable artifact. The earlier diagnostic remains saved and is explicitly not the accepted source variant.

## Local checks

An unchanged EXP158 private-algorithm oracle and the Goose candidate have separate test-only DSO wrappers;the production image exports only two read-only witness counters,no mutating test/failure hooks. Compare180native/rollback graph pairs,plus12candidate repeated-copy checks:93393candidate assertions,92907rollback assertions,387collision-heavy insertions and all31Goose modes. Cases include prime-size and size_t edges,bit boundaries/canaries,all graph array offsets,empty/default/grads graphs,left/right/unknown evaluation order,shared sources,cast self-links,compute upgrades,selection,leaf-first name lookup,borrowed views,copy to a larger modulus,gradient remapping,clear/reuse and a12000deep dependency chain. Explicit rollback has zero new witness bits.

Nine controlled error cases cover key/bitset OOM with cleanup,DFS scratch OOM,capacity/index/slice failures,malformed leaf cycle,size overflow and full hash table. Eight threads perform256independent graph lifecycles and512expand/clear/reuse operations,with zero outstanding graph allocations. These are scoped correctness/ownership checks,not a whole-runtime sanitizer or race-safety claim. No microbenchmarks or broad rebuild/sanitizer suite.

## Apex32K/128

| Metric | EXP158 latest | EXP159 final |
|---|---:|---:|
| Prefill tok/s |2634.815|2635.438|
| Decode tok/s |28.055|27.865|
| RSS peak MiB |21315.906|21316.418|
| Sampled VRAM MiB |3246|3246|
| Swap KiB |0|0|

+0.024%prefill/-0.678%decode against the last EXP158 sample;small uncontrolled single-sample variation,not stable acceleration or fresh paired C++ parity. EXP158 decode samples span26.596..28.055;final EXP159 is inside that observed range. Original saved same-profile C++2867.846prefill/27.978decode remains historical and its prefill advantage is not hidden.

Final text,all128token IDs and complete saved probability/top5 arrays exactly match original C++;127graph reuses. Actual Goose lifecycle/traversal mask `0x50dc22e`,prior graph builders active,zero graph/context heaps at shutdown. Exactly two model requests:one early diagnostic before the identified bridge fix and one final gate after it. No repeated performance series or tuning. Raw request/response/log/process/memory receipts stay local under `runtime/exp159/results/`,not published.

## Reproduce and next scope

`build.py` emits the pinned completed self-hosted EXP301 Stage3,qualifies helper names,verifies unchanged EXP158 relink SHA and replaces only the isolated core object. `check.py` builds separate oracle/probe objects and runs native parity,rollback,failure cleanup and independent-thread checks. `run_apex.py` uses the fixed canonical request once,refusing existing final receipts. Immutable prior runtimes and rollback remain available.

Continue core fusion predicates and residual model-used operation parameter helpers/source closure,then remaining allocator/scheduler/backend/model/runtime work under the unchanged goal. Qwen remains deferred. A complete parent requirement is closed only when its full scope is proven;function/policy counts are not full-runtime completion.
