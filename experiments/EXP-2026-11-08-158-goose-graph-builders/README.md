# EXP158: shared graph-operation builders in Goose

Continue solo from accepted EXP157.65existing native function bodies route to36actual Goose policies covering arithmetic,unary,normalization,GLU,MoE/matrix construction,reductions,repeats,concat,row gather/scatter,copy/cast/layout,softmax,RoPE,Flash Attention,SSM convolution,gated delta net,sorting/top-k,acc/set,diagonal masks and precision/sink metadata. Public convenience wrappers retain their ABI and call these transferred policies. No worker code or production/compiler changes.

## Source and effect boundary

`source/builders.goose` performs validation,result dimension/type selection,inplace alias selection,parameter slot/bit placement and source-edge assignment. Enum values are mechanically derived from the pinned header and recorded in `abi-values.json`;they are not a native algorithm provider. `recipes.json` packs existing C signatures into the Goose argument ABI. `builders.h` imports only POD projections,exact scalar bit casts,raw parameter/edge stores,formatting and previously accepted Goose geometry/tensor constructors. Copy-name selection is in Goose;C formats the selected string. No imported helper calls an original graph-operation implementation. Numerical CPU/CUDA kernels remain unchanged.

New outputs are immutable object-hash-named libraries. Relinking the unchanged EXP157 object list must reproduce its prior SHA before replacing just the core object. Generated runtime names are mechanically qualified to compose with EXP155/156/157;no compiler source edits. Cached default-on `GGML_GOOSE_GRAPH_BUILDERS=0` selects the retained original operation bodies. The rollback and every inherited checkpoint remain available. Current producer remains completed self-hosted EXP301 Stage3;EXP302 is an unfinished guard observer,not a replacement full compiler.

Checked dimension addition/products and positive-divisor checks guard newly computed result sizes/broadcast tests. Invalid input contracts are rejected before result publication. Gated-delta snapshot count additionally rejects values beyond the encoded int32slot. Parameter bit stores preserve signed zero and all unmodified parameter bytes. Cast self-reference,SET_ROWS legacy source order,view aliasing,RoPE sections/float slots,Flash precision/max-KV/sink slots and NULL sink clearing match the existing ABI. This is scoped constructor safety,not a whole-runtime memory-safety claim.

## Local correctness

Independent EXP157 loaded into a separate dynamic-loader namespace,not a self-referential fallback.16allocating/no-allocation case groups create558paired tensors and perform10408checks across all36Goose modes. Compare every tensor in each context,including intermediates:arena size/offset,type/op/flags,all shape/stride/parameter/name bytes,all10source edges,view roots/offsets,data offsets and default backend fields. Cover all22unary and7GLU choices,sevenmatrix weight types,repeat/broadcast,reductions,copy/cast/self-reference,named/unnamed destinations,multi-RoPE sections,attention sinks/precision,scalar/vector gates and recurrent snapshots. Eight native/candidate malformed-input pairs reject before arena mutation. Explicit rollback passes with zero new witness bits.

Inherited EXP156 constructor corpus passes35formats/2800paired tensors/429444checks,borrowed-context aliases,ten native/candidate fatal transactions and four overflow guards. Inherited EXP157 registry/lifecycle corpus passes4500checks with zero live contexts. No full rebuild,sanitizer suite or microbenchmark.

## Real Apex32K/128 and regression follow-up

The first fixed-profile candidate request gave2639.982prefill/26.596decode versus previous saved EXP1572638.849/27.574. Because decode was3.55percent lower,one sequential EXP157->EXP158 pair was run as a concrete regression follow-up,without code changes or parallel builds. All three raw results remain local and all summaries are saved;the first result is not discarded.

| Metric | EXP157 paired control | EXP158 paired candidate |
|---|---:|---:|
| Prefill tok/s |2612.899|2634.815|
| Decode tok/s |27.668|28.055|
| RSS peak MiB |21315.875|21315.906|
| Sampled VRAM MiB |3246|3246|
| Swap KiB |0|0|

Pair differences +0.839%prefill/+1.397%decode:the initial decrease did not reproduce. This is an incremental diagnostic against the previous Goose stage,not stable acceleration or a fresh C++ performance control. The archived same-profile original C++2867.846/27.978 remains historical;its prefill advantage is not hidden. Exactly three full requests total:one initial candidate and the one exceptional sequential pair. No further model runs or tuning.

Every request exactly matches the original C++ text,all128token IDs and entire saved probability/top5 arrays;127graph reuses. Candidate graph-builder witness `0x83f3e5f1b` confirms actual Goose execution,with all prior modules active. Context heap ownership count returns to zero at shutdown. Process RAM/VRAM and swap are recorded in `apex-summary.json`;raw prompt,response,server logs and process receipts stay local under `runtime/exp158/results/` and are not published.

## Reproduce and remaining scope

`build.py` emits/qualifies the checked-in Goose and rebuilds only the core object against immutable inherited objects. `check.py` runs the independent native/rollback corpus. `check_inherited.py` reuses the pinned earlier fixtures. `run_apex.py` performs the usual single gate and refuses existing receipts. `diagnose_pair.py` records the exact exceptional comparison and similarly refuses existing receipts. Do not automatically run the pair after an ordinary passing gate.

This closes the bounded common node-builder block,not P4-03 or100percent Apex. Remaining native core scope includes graph/hash/traversal/lifecycle APIs,additional model-used operation wrappers/parameter helpers and final source closure. The numerical backends,scheduler/memory/KV,model construction/loader/tokenizer/server and other open Apex requirements retain their existing plan status. Next:graph/hash/traversal and public graph lifecycle with the same per-significant-change correctness and Apex gate. Qwen stays deferred,no agents.
