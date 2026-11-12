# EXP298: quantized K/V -- native serves, our line dies with Goose assertions

P5-02's criterion names F32/F16/BF16/Q8/Q4 K/V layouts. The profile uses the default F16 cache, so the quantized layouts
were never exercised live. This pass ran both lines on the same request with `--cache-type-k/--cache-type-v` set.

| cache types | native | ours |
|---|---|---|
| q8_0 / q8_0 | served, `1da0f499...`, "over the lazy dog." | **exit 1**, never became healthy |
| q4_0 / q4_0 | served, `1da0f499...` -- identical to q8_0 and to F16 | **exit 1**, never became healthy |

Native produces the same continuation for all three cache types, which is expected for a deterministic greedy decode on
this prompt.

Our log shows sixteen `goose runtime error: assertion failed` lines immediately after `FG376S_HYBRID_CONTEXT`, before
`FG183F_CUDA_FWHT`, then the process exits 1 without serving.

## Where to look next

The rotation path is the only piece that is enabled *specifically by quantization*: `attn_rot_k` and `attn_rot_v` require
`ggml_is_quantized(type_k)` / `ggml_is_quantized(type_v)` and an embedding width divisible by 64. With an F16 cache the
Hadamard tensors are never built, which is why the accepted profile never touches that code; with q8_0 or q4_0 they are
built and fed through the FWHT module.

So the failure is most likely in the Hadamard/FWHT area (EXP320 generator, EXP333 matrix store, FG183F_CUDA_FWHT) rather
than in the layout arithmetic, and the immediate next step is to identify which module raises the assertion rather than to
guess. A selective switch bisect is the tool: the FWHT and rotation switches can be turned off one at a time, which the
earlier "all switches off" experiment could not do because the server refuses to start with everything disabled.
