# EXP155: shared tensor geometry executes Goose

Solo implementation from restored EXP150/EXP154. No EXP153 worker source imported. This stage moves shared GGML shape,stride,byte-span and broadcast algorithms into `source/geometry.goose`. It preserves the public C ABI and the existing fast CPU/SIMD/CUDA kernels.

## Implementation and boundaries

Twenty-four directly replaced functions/policies include element/row counts,byte spans,padding,row size,scalar/vector/matrix/dimension checks,contiguity with skipped dimensions,layout/permutation/empty predicates,shape/stride equality,broadcast and matrix/out-product compatibility. Seven existing public contiguous wrappers reach the transferred common predicate.

Native imports are typed read-only projections of live `ggml_tensor.ne/nb`,block/type byte metadata,alignment and invariant failure. All geometry arithmetic/loops/decisions execute emitted Goose C. Generated logic and projections compile in one TU,so ordinary C optimization can inline field reads. The source snapshot retains unchanged GGML functions and explicit rollback bodies;this is not complete GGML/native-source removal.

A cached default-on switch `GGML_GOOSE_TENSOR_GEOMETRY=0` restores native bodies. Per-thread mode bits record the first use of each Goose policy with one atomic OR,avoiding an atomic counter on every query. ABI field types are64-bit signed extents/unsigned strides on this host. No tensor layout or allocator ownership changed.

An unchanged relink of the inherited EXP149/EXP150 objects must match the verified EXP154 base DSO SHA exactly before substituting one `ggml.c` object. This prevents accidentally dropping already accepted backend/quantization work.

## Validation

The independent native image is loaded with `dlmopen(LM_ID_NEWLM)`;comparison never falls back to the candidate.35registered formats,13440tensor cases and362880comparisons cover empty,singleton,quant-block,padded,permuted,strided,broadcast and unsigned span behavior. Normal GGML constructors exercise the internal row/matmul/out-product checks. All24Goose modes are observed. The same corpus passes with explicit native rollback and zero Goose mode bits. These are correctness checks,not microbenchmarks.

One unchanged Apex32768/128 request matches original C++ text,all128token IDs and the complete saved probability/top5 arrays. Graph reuse remains127. Actual inference Goose mode mask: `0x5fbdd7`.

| Metric | Verified EXP154 | EXP155 |
|---|---:|---:|
| Prefill tok/s |2631.170|2632.402|
| Decode tok/s |27.492|27.762|
| RSS peak MiB |21315.605|21315.699|
| Sampled VRAM MiB |3246|3246|
| Process swap KiB |0|0|

Speed differences +0.05%/+0.98% are small uncontrolled single-sample variation,not a stable acceleration claim. No repeated series,sanitizer run,Qwen run or production change.

## Reproduce

Initial fresh build: `python3 build.py`,then `python3 check.py`,from this directory;paths are pinned in the scripts. The builder intentionally refuses an existing immutable output DSO;replay the saved argv into a fresh build/run directory under `/home/end0/AI/LlamaServer/` instead of overwriting an active library. `run_apex.py` runs the already-authorized fixed full-model gate once and refuses to overwrite an existing receipt. It preserves the original EXP150 Goose preload/flags. Its raw prompt/response/memory logs remain local under `runtime/exp155/results/apex-32k128/`,not published.

P4-03 is IN_PROGRESS:context/object/type ownership,tensor constructors,view creation and remaining public core APIs are still native. This stage does not close the whole requirement. The next coherent transfer is tensor/view constructors with the same native contracts and one subsequent Apex gate.
