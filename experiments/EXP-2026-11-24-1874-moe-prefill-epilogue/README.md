# EXP-2026-11-24-1874 — Test the batched MoE output path

**INCONCLUSIVE for the full model.** The four-value weighted reduction
is neutral and rejected. The batched shared-expert epilogue is faster
in isolation and ready for a guarded dispatch study. No model profile
was changed or measured.

## Weighted reduction

The full-window Apex prefill trace records about 226 ms across 5160
`goose_moe_weighted` launches. Specialize eight routes and 2048
columns and give each thread a `float4`, keeping separate multiply
and add rounding. The probe streams across 16 distinct tensors.
It matched every FP32 word for 4, 252 and 256 tokens. At 256 tokens
the median was 43.034 → 42.886 µs (-0.34%). This scale of gain does
not justify a full-profile variant.

## Shared expert epilogue

The built-in decode path already fuses the shared expert gate and
the two residual additions (EXP-1717). The batched prefill graphs
contain corresponding nodes. The new entry broadcasts one prepared
gate per token, evaluates the same multiply and two additions in
order, and writes only the final tensor. The routed reduction and
gate-producing operation stay separate. The control runs archived
`goose_vector_ops` for the broadcast and `goose_vector_flat4`
for both additions. All FP32 output words matched, including inputs
with signed zero. The candidate compiled to 22 registers with no spill.

| Tokens | Control ms | Fused ms | Matched FP32 words |
| ---: | ---: | ---: | ---: |
| 4 | 0.00800 | 0.00280 | 8192 |
| 4092 | 1.21918 | 0.29354 | 8380416 |
| 4096 | 1.12692 | 0.29352 | 8388608 |

These are medians of six alternating blocks of 32 evaluations on
RTX 4070. The large fixture saves about 0.83 ms. Coverage and
overlap in the existing graph remain unmeasured. A full-profile
saving cannot be calculated by multiplying the fixture delta by
the layer count. No full-model correctness, RAM, VRAM, prefill or
decode claim follows.

## Integration gate

The likely dispatch site is `fg143_graph_run` in
`goose/cpu/goose-cpu-backend-api/graph-dispatch.goose`.
The archived runtime keeps this symbol in `cpu.o`. First verify
that the broadcast multiply and the two ordered adds are consecutive
on Apex. Match exact tensor sources, 2048-wide contiguous FP32
layouts and the scalar-per-row gate. Require both intermediate
tensors to have no external users, prove lifetime and pointer
safety for every input and the destination, and leave observed
graphs and unmatched shapes on their original path. Dispatch the
fused CUDA entry without reading its inputs back to the host.
Retain the original weights, states, IDs and text as correctness gates.

## Reproduce

Use the EXP-1786 frozen device tree and archived MMQ PTX with CUDA
12.6 ptxas and Clang 18, using the commands in EXP-1859. Build the
weighted4 entry by applying [source/weighted4.patch](source/weighted4.patch).
Extract `goose_moe_weighted` directly from archived PTX as
`control.cubin`. Build [epilogue.cl](epilogue.cl) separately with
the same Clang NVPTX flags and frozen device-prelude include and
assemble it as `epilogue.cubin` for SM89. Check [SHA256SUMS](SHA256SUMS).

```sh
python3 probe.py
python3 probe-epilogue.py "$exp1786/device/goose-cuda-mmq.ptx"
```

[Raw reduction timings](probe.log), [raw epilogue timings](epilogue.log)
and [structured samples](results.json) record both tests.
