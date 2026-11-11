# Small Apex path: what remains, and the boundary of each item

Written after APEX.CORE, OUTPUT, TOKENIZER, LOAD, KV, RECURRENT_MEMORY and BATCH were closed and
self-accepted. Every remaining item is blocked by a named boundary, not by missing work inside libllama.

| item | remainder | boundary | what unblocks it |
|---|---|---|---|
| APEX.CPU_MATH | SIMD dot products and the expert matrices | the language has scalar `f32`/`f64` only, no vector types or intrinsics (compiler-limitations 3/3a); the profile runs `--cpu-moe`, so these are the used path | vector types plus AVX2 intrinsics, or a supported way to emit vectorised C. Request sent in `to-compiler/runtime-cpu-math-blocker.json` |
| APEX.CPU_CONTROL | expert lifecycle/ownership, native-removal boundary | its mandatory routes (FG143/FG147/FG149/FG150) live in `libggml-cpu` (280 Goose symbols), an artefact this DSO does not build | extend the build to produce the Goose `libggml-cpu`, then finish the lifecycle decisions there |
| APEX.CUDA_CONTROL | host/backend lifecycle, transfer/async ownership, source closure | mandatory routes FG195V/FG196P live in `libggml-cuda` (63 Goose symbols), same boundary; the in-`libllama` transfer policy is not on this request's path (EXP406) | extend the build to produce the Goose `libggml-cuda` |
| APEX.CUDA_MATVEC | declined Q6_K and remaining shapes | the kernel itself is in `libggml-cuda`; Q6_K and some shapes are still native there | same `libggml-cuda` build, then the Q6_K and shape paths |
| APEX.CUDA_MMQ | the reachable MMA/TensorCore path and `quantize_mmq_q8_1` | kernel work in `libggml-cuda`; only dispatch (FG203D) is live today | same build; then the quantizer and MMA path |
| APEX.MODEL_GRAPH | graph-builder and decode orchestration | `build_moe_ffn` alone is 504 lines with 104 `ggml_*` calls creating and connecting tensors; the bridge carries scalars and flat arrays, so a tensor handle cannot cross | a tensor API on the Goose side, or accepting node creation as a primitive (a criteria change, not mine to make) |
| APEX.REQUEST / APEX.SESSION | HTTP server, queue and slots | they live in `tools/server`, outside `libllama`; the bridge has no HTTP or socket surface | a server-side Goose component, which is a new artefact |
| APEX.SOURCE_CLOSURE / APEX.FINAL | end states | depend on the rows above | follow from them |

## The one dependency that unblocks four items

Three items (CPU_CONTROL, CUDA_CONTROL and the two CUDA kernel items) need the same thing: the ability to
build the Goose variants of `libggml-cpu` and `libggml-cuda` as part of the candidate, instead of linking
the inherited ones. The Goose sources exist in `experiments/` and the modules are already live in the
inherited libraries; what is missing is the build step in this tree.

That is an infrastructure dependency of the small plan, not a new direction: without it, four items
cannot be completed no matter how much work goes into `libllama`.
