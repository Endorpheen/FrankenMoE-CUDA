# EXP-2026-11-25-1982 — Bonsai 6:1 Flash with FP32 accumulation

**PROVEN on the component accuracy gate and the short greedy fixture.**
The isolated candidate uses two tokens and six real query heads per
token for each KV head. Each CTA handles 128 consecutive keys. Both
Tensor Core multiplications accumulate in F32. Q/K/V and the probability
multiplicand use F16. Softmax sums, value partials, and increasing-order
tile merge are F32. Q4_0 remains the resident cache format; the CTA
expands only its working set in shared memory.

The runtime reuses the existing 32 MiB attention scratch in waves.
A bound check falls back to the previous kernel if one group cannot fit.
The experiment starts from the unchanged EXP-1980 control. The
EXP-1981 padded F16-accumulator candidate is absent from the composition.

## Component gate

RTX 4070, same seeded finite query and Q4_0 fixture as EXP-1981.
The A–B–B–A CUDA event bracket includes the full tile and merge launches.
Unchanged acceptance limits: relative L2 < 0.003, max absolute error < 0.002.

| KV / queries | Q4 tiles, ms | FP32 Flash, ms | Relative L2 | Max absolute |
| --- | ---: | ---: | ---: | ---: |
| 65 / 16 | 0.139 | 0.050 | 0.0284% | 1.839e-4 |
| 257 / 33 | 0.471 | 0.141 | 0.0295% | 6.605e-5 |
| 8192 / 128 | 47.028 | 14.678 | 0.0289% | 1.221e-5 |
| 32768 / 128 | 220.944 | 66.286 | 0.0286% | 5.923e-6 |

All cases pass. The long component is 3.33x faster than the Q4 control.
EXP-1981 reached about 61x but exceeded the error limit and changed
the short greedy trajectory. The FP32 version reduces the long-case
relative L2 error from 0.933% to 0.0286%. This experiment changes
the tile structure as well as precision, so it does not isolate
the exact contribution of each arithmetic change.

## Full short fixture

Both modes use the same executable and matrices, context 36864,
ubatch 1024, Q4_0 K/V, no MTP and greedy with EOS suppressed.
The prompt is the first 1025 IDs from EXP-1977 with an output budget
of 32. Runs were A–B–B–A, one new native process each.

| Mode | Prefill, tok/s | Decode, tok/s |
| --- | ---: | ---: |
| Previous Q4 | 240.63 | 16.041 |
| FP32 Flash | 254.51 | 16.030 |

All 32 IDs matched the current native control. Maximum selected-logit
difference was 0.418. Both arms reproduced their own exact outputs on
repeat. The first pair saved 455.6 ms and the second saved 7.3 ms.
The mean short prefill improvement was 5.77%, with the second pair
close to timing noise. Both allocated exactly 1,569,405,696 bytes of
device workspace. Peak RAM was not measured in this block.

The integrated candidate is opt-in while additional short fixtures and
the eventual 32K/512 gate are pending. Long-context component rows were
generated in a standalone GPU fixture. No new long model request was
made. The short match is with FrankenMoE's control. The previous
PrismML trajectory discrepancy remains open.

## Next measurement

Measure K/V staging and transpose overhead by staging once per layer
for the same sequence of tile waves. Contrast a packed shared-memory
layout with bank skews, and increase reuse across queries. Compare
L2, absolute error, GPU time, and complete short greedy IDs for each
candidate. Reuse the accepted EXP-1980 control for model-level
comparisons. Preserve and report any memory cost of persistent staging.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1982-bonsai-flash-fp32
bash "$exp/source/build.sh" build/bonsai-flash-fp32-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-flash.py" \
    build/bonsai-integration build/bonsai-integration/inventory.json \
    build/bonsai-flash-fp32-repro/flash-fp32.cubin build/bonsai-flash-fp32-repro/frozen-goose.ptx
```

Use EXP-1980's 1025/32 request with `bonsai-flash-fp32` and
`flash-fp32.cubin`. Set `BONSAI_FLASH_F32=1` for the candidate or
`BONSAI_FLASH_F32=0` for the control. Use `BONSAI_UBATCH=1024`.

[Component data](component.json). [Full short request data](results.json).
