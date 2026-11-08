# EXP156: context-arena and tensor/view constructors in Goose

Solo continuation from EXP155. The compiler and production are untouched;no EXP153 worker code imported. Twenty-two existing functions are redirected to fifteen Goose entry policies covering arena object placement/publication,tensor creation and duplication,view/reshape,permutation/transpose,workspace allocation,tensor enumeration/named lookup,maximum tensor size and index decomposition. Existing1D..4D constructor wrappers remain mechanical ABI packing around the transferred constructor.

## Ownership and ABI

All size/layout/offset/validation/allocation decisions and traversal loops are in `source/constructors.goose`. The typed C layer projects POD fields,performs raw zeroing or index stores,formats names and calls accepted EXP155 Goose geometry. It does not call original constructor algorithms. Every64-bit import is declared before generated C. Generated runtime helper names are mechanically prefixed to compose two Goose modules in one TU;explicit `fg156_*` imports remain unchanged. Compiler sources are not modified.

Arena checks precede object publication. Tensor shape/byte/offset and user-stride overflow checks precede the corresponding tensor allocation. Valid views preserve original base-owner flattening,relative op-parameter offsets,absolute view offsets,data addresses,source links and names. Reshape is checked before publication. On valid metadata the byte layout and object order match the unchanged C implementation exactly. New checked arithmetic also rejects malformed negative extents,overflowing nested offsets,user strides and byte products that were not safely diagnosed by the historical implementation. This is a scoped error-contract improvement,not proof of whole-runtime memory safety.

Physical context initialization/reset/free and backing-buffer ownership still use the unchanged implementation. Borrowed view roots may belong to other contexts or external buffers. The new code never frees borrowed payloads. Original tensor layout and all fast CPU/CUDA kernels are unchanged. Cached default-on `GGML_GOOSE_TENSOR_CONSTRUCTORS=0` selects original bodies. Sparse per-thread witness bits avoid an atomic increment on every allocation.

## Tests

Independent unchanged EXP155 image via `dlmopen(LM_ID_NEWLM)`;no self-reference fallback.35registered formats,2800paired tensors,and429444checks cover allocating/no-allocation contexts,all24permutations,empty shapes,custom strides,nested views,reshape,metadata defaults,op params,naming,arena placement/order,object counts,raw workspaces,graph objects,index decomposition and canary guards. Explicit rollback passes429432checks with zero constructor-witness bits.

Two cross-context borrow modes exercise arena-owned and external payloads:three contexts,nested flattened views,reshape,data alias writes and freeing graph contexts while preserving the borrowed model payload. Ten native/candidate fatal-case pairs use the public abort callback to verify arena bytes,first object and object count are unchanged at rejection. Four added malformed-input overflow guards reject transactionally. No broad sanitizer run or microbenchmark.

An internal view helper initially needed a distinct default-stride entry instead of public custom-stride packing. Source review found and corrected this before Apex;the final local corpus witnesses all15modes.

## One unchanged Apex32K/128 gate

Exact original text,all128token IDs and complete saved probability/top5 arrays;127graph reuses. Actual constructor witness mask `0x53ff`,with inherited Goose geometry also active.

| Metric | EXP155 | EXP156 |
|---|---:|---:|
| Prefill tok/s |2632.402|2639.870|
| Decode tok/s |27.762|27.247|
| RSS peak MiB |21315.699|21315.863|
| Sampled VRAM MiB |3246|3246|
| Swap KiB |0|0|

Speed differences +0.28%/-1.86% are small uncontrolled single-sample changes,not stable acceleration or fresh-C++ parity proof. There was one full model request after final implementation;only additional unchanged-code borrowed-view tests followed it.

## Reproduction and remaining scope

`build.py` uses the pinned self-hosted producer,accepted EXP155 source,typed helpers,strict undefined-symbol link and unchanged-baseline relink SHA check. Outputs have immutable object-hash names. `check.py` builds/runs the independent native and explicit rollback corpus. `run_apex.py` performs the authorized fixed profile once and refuses an existing receipt. Raw prompts/responses/logs remain local in `runtime/exp156/results/apex-32k128/`;they are not published.

P4-03 remains IN_PROGRESS:context initialization/reset/free,type/op tables,metadata and other public core APIs remain. Dynamic graph/tensor backend allocators and scheduler are separate open requirements. Next coherent step:context lifecycle and remaining core metadata,with the same exact-model and small-regression gate. No auxiliary agents.
