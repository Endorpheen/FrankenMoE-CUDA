# Small Apex path: what remains, and the boundary of each item

Written after APEX.CORE, OUTPUT, TOKENIZER, LOAD, KV, RECURRENT_MEMORY and BATCH were closed and
self-accepted. Every remaining item is blocked by a named boundary, not by missing work inside libllama.

| item | remainder | boundary | what unblocks it |
|---|---|---|---|
| APEX.CPU_MATH | SIMD dot products and the expert matrices | the Q4_K x Q8_K super-block dot is ported to Goose (EXP410): 160/160 bit-exact against a transcription of this tree's `quants.c`, 9/9 mutations caught, drop-in `libfs410s.so` enabled by default with `FG410S_ENABLED=0` as the off switch. The Stage3 compiler now supports SIMD (v58), so the earlier "no vector types" note no longer applies to this entry | remaining: the real-server run (completion + equal probabilities + KV restore) and the checkpoint; other CPU_MATH kernels not yet ported |
| APEX.CPU_CONTROL | expert lifecycle/ownership, native-removal boundary | the routes (FG143/FG147/FG149/FG150) live in `libggml-cpu`, and that library **now builds from source** in this environment: `cmake -S llama.cpp-flint-cuda -B /tmp/build-cpu-goose -DGGML_CUDA=OFF` then `ninja ggml-cpu` produces `bin/libggml-cpu.so.0.27.0` (1,439,136 bytes). The build is out-of-tree and does not touch the working library | relink that build with the Goose objects and the replaced symbols, then finish the lifecycle decisions there |
| APEX.CUDA_CONTROL | host/backend lifecycle, transfer/async ownership, source closure | mandatory routes FG195V/FG196P live in `libggml-cuda` (63 Goose symbols), same boundary; the in-`libllama` transfer policy is not on this request's path (EXP406) | extend the build to produce the Goose `libggml-cuda` |
| APEX.CUDA_MATVEC | declined Q6_K and remaining shapes | the Goose Q6_K matvec exists and is **byte-exact** (EXP186: 4/4 outputs identical, Apex sha e68df05b, memcheck clean) but was not active: the flag was opt-in and the symbol is absent from the active `libggml-cuda` (0 strings for GGML_GOOSE_CUDA_MMVQ_Q6K). Per the owner's rule that speed is not a veto and accepted work is on by default, the source flag was flipped to opt-out (=0 disables) | rebuild blocked: `build_runtime.py` needs `runtime/exp185/build/*.o` and `runtime/exp185/run/libggml-cuda.so`, and only `measured/` and `results/` survive, so the object chain must be reconstructed before the module can be linked into the active library |
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
