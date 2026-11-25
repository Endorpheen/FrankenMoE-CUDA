# EXP-2026-11-26-2095 — Exact packed half2 Q4 decode unpack

**Retained in the cumulative experimental checkpoint.** Apply only
to the long single-query direct Q4 MMA path of EXP-2094. The stable
runtime and the inherited numerical quality gate remain as before.

Replace scalar Q4 dequantization with aligned reads of two code
bytes and two half2 operations. Subtract 1032 from two biased
half values to recover the exact integer coefficients, then multiply
by the original half scale. For a finite FP16 scale, the product
by an integer from −8 to 7 is representable exactly in FP32, so the
original FP32 multiply followed by round-to-nearest FP16 has the
same rounding boundary. The MMA, score and probability arithmetic,
partial layout, and EXP-2094 ordered merge are unchanged. The new
entry uses 72 registers, 47,872 B shared memory and no spills, same
as its control. All 97 retained CUDA text sections are byte-identical.
There is no new workspace.

## RTX 4070 component probe

Same CUBIN and seeded inputs, with Q4_0 K/V encoded by the frozen
Goose image. Ten balanced event observations per arm, including
the existing parallel merge. All partials and final outputs match
bit-for-bit at all three lengths.

| Visible keys | Scalar unpack, ms | Half2 unpack, ms | Time saved |
| ---: | ---: | ---: | ---: |
| 32768 | 0.32768 | 0.29278 | 10.65% |
| 32769 | 0.32666 | 0.29318 | 10.25% |
| 33279 | 0.32870 | 0.29472 | 10.34% |

All cases clear the 5% component threshold.

## RTX 4070 HTTP 32K/512

All four valid runs are listed. Same exact GGUF, raw request,
binary, image, frozen PTX, effective argv and affinity. Context
36864, batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123, EOS masked
and MTP off. Only `BONSAI_DECODE_DIRECT_HALF2` changes. All four
arms pass the cold gate, with state reset, 32,768 evaluated input
tokens and no reused prompt tokens. Prelaunch clocks and temperature
were observed and left unlocked. The first pair slowed on client
time, so the second pair reversed the order.

| Run | Prefill, tok/s | Decode, tok/s | Decode, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 1020.32 | 37.24 | 13.724 | 45.860 | 8932 | 7458536 |
| B | 1010.86 | 37.66 | 13.568 | 46.004 | 8932 | 7458316 |
| B2 | 1026.83 | 38.02 | 13.439 | 45.374 | 8932 | 7456936 |
| A2 | 1024.71 | 37.28 | 13.708 | 45.708 | 8932 | 7458116 |
| Mean control | 1022.52 | 37.26 | 13.716 | 45.784 | 8932 | — |
| Mean candidate | 1018.85 | 37.84 | 13.503 | 45.689 | 8932 | — |
| Saved PrismML | 1076.60 | 47.63 | 10.728 | 41.174 | 8288 | 7651032 |

Decode accelerates in both pairs, saving 156 and 269 ms,
respectively. Mean decode throughput increases 1.58% and its
elapsed time decreases 213 ms. Mean prefill takes 117 ms longer,
with different signs across pairs. Client time is 144 ms slower
in the first pair and 333 ms faster in the second. Its mean drops
95 ms. The latter is not a repeatable full-request gain by itself.

All responses match 512 PrismML IDs and text. Process swap is zero,
VRAM stays at 8,932 MiB, and peak RAM is comparable. No new quality
tasks were run for the exact component change. The EXP-2093
numerical candidate still needs expanded validation. Apex and
Inkling packaged artifacts match the stable package byte-for-byte.
No new cross-architecture model runs were made.

## Execution evidence

The [EXP-2094 full graph](../EXP-2026-11-26-2094-bonsai-decode-ordered-merge/data/attribution.json)
is the traced control. It routes the direct MMA tile 16 times at
final prefill and 8,176 times in decode, followed by ordered merge.
The host variant selects the new unpack entry with the same
geometry, arguments and scratch contracts. The component executes
both entries and the unchanged merge. The A/B logs record the exact
flag and image. No separate full candidate trace was captured for
this experiment. Per-kernel timing of the change in the full
request remains unmeasured.

## Reproduce

Use the EXP-2094 build and profile. Set `FRANKENMOE_CHECKPOINTS`
to the artifact tree. Build with Clang 18, CUDA ptxas for SM89
and GCC 13.

```bash
exp=experiments/EXP-2026-11-26-2095-bonsai-decode-q4-half2-unpack
out=build/bonsai-decode-q4-half2-unpack-2095
base=build/bonsai-decode-ordered-merge-2094
bash "$exp/source/build.sh" "$out" "$base"
python3 "$exp/source/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin"
FRANKENMOE_BENCH_COLD_LOG=1 bash "$exp/source/build-http.sh" "$out" "$base"
```

Use EXP-2005's `source/check.py CORE_DIRECTORY INVENTORY FROZEN_PTX
CUBIN --decode-mma-half2` for component checks. For HTTP load the
EXP-2094 profile and set `BONSAI_DECODE_DIRECT_HALF2=0` for A and
`=1` for B. Send the
[exact EXP-1977 raw request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
and use the cold-prefill validator before and after each arm.

[Component evidence](data/component.json), [CUDA identity](data/device-code.json),
and [full pairs and cold receipts](data/results.json).
