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

## Localization: the asserting module is fg143 in libggml-cpu

A first attempt under gdb appeared to succeed, but it was false evidence: without `LD_LIBRARY_PATH` the binary's RPATH
resolved to an older variant directory (`franken-goose-exp150-expert-registry-20261107/variants/cpu-backend/run`), a
different build entirely. With `LD_LIBRARY_PATH` pinned to the stage, the failure reproduces and the backtrace is:

    gg_fail                              libggml-cpu.so.0
    gg_function_11.constprop             libggml-cpu.so.0
    gg_function_1.constprop              libggml-cpu.so.0
    fg143_compute                        libggml-cpu.so.0
    fg143_try.part                       libggml-cpu.so.0

Sixteen identical failures appear because sixteen worker threads reach the assertion, and each `gg_fail` calls `exit(1)`
-- which is also why the server dies without printing a bind message.

## Which side triggers it

| cache K / V | assertions | listening |
|---|---|---|
| q8_0 / q8_0 | 16 | no |
| q8_0 / f16 | 16 | no |

Quantized **K** alone is enough. That points at the K rotation path, which is built only when K is quantized
(`attn_rot_k` requires `ggml_is_quantized(type_k)`), rather than at the V layout.

The next step is to identify the assertion inside fg143's compute for that op -- either by rebuilding the CPU module with
symbols and mapping the failing program counter to its generated function, or by dumping the op name that fg143 dispatches
when the rotation tensor first appears.
