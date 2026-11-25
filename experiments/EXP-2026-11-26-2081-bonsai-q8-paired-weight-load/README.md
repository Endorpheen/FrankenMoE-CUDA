# EXP-2026-11-26-2081 — Paired weight loads in Bonsai INT8 gate/up

**Retained in the cumulative experimental checkpoint.** Apply only to
Bonsai full 5120-to-17408 N128 gate/up tiles on EXP-2078. The stable
EXP-2049 checkpoint is unaffected and cumulative numerical quality
remains subject to the existing promotion gate.

## Mechanism

EXP-2079 attributes 9.015 s of prefill GPU time to
`bonsai_pq2_mmq_int8_full_n128`. Replace separate PQ2 code-byte loads
with aligned 16-bit pair loads and two-word shared stores. Each block
has a 34-byte stride and a 2-byte FP16 scale before its 32 code bytes.
The code offsets are even, and each destination pair starts at an
8-byte boundary in the 36-word aligned shared rows. The OpenCL probe
emits `ld.global.u16` and `st.shared.v2.u32`. The two codes still use
the same byte expansion, and activation packing, MMA and ordered FP32
accumulation are identical. Partial tiles retain their existing path.
No additional device allocation. SM89 uses 92 versus 93 registers,
28,416 B shared memory, and zero spills. All 91 original kernel text
sections match byte-for-byte.

## RTX 4070 component probe

Medians of eight measurements per arm in balanced order, including
Q8 activation pack. All output bytes match the control on four actual
PQ2_0 tensors at 1024 tokens. The zero row is exact and all values
are finite. Each case exceeds the 5% selection threshold.

| Tensor | Scalar, ms | Paired, ms | Time saved |
| --- | ---: | ---: | ---: |
| Layer 0 gate | 2.3145 | 2.1728 | 6.12% |
| Layer 0 up | 2.3337 | 2.1741 | 6.84% |
| Layer 62 gate | 2.3556 | 2.1794 | 7.48% |
| Layer 62 up | 2.3106 | 2.1744 | 5.90% |

Applying the measured component range to the prior kernel duration
suggested 0.53–0.67 s of kernel savings before end-to-end effects.

## RTX 4070 HTTP 32K/512

EXP-1977 exact IDs, context 36864, batch/ubatch 4096/1024, Q4_0 K/V,
greedy, seed 123, empty prompt cache and MTP off. Same binary and
CUBIN across all arms. The first pair showed a small decode decrease,
so a reverse pair was run. Chronological order A, B, B2, A2. Both
pairs improve prefill and full request time. The sign of the decode
difference reverses.

| Run | Prefill, tok/s | Decode, tok/s | Prefill, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 960.60 | 32.71 | 34.112 | 49.765 | 8932 | 7457940 |
| B | 970.15 | 32.53 | 33.776 | 49.508 | 8932 | 7457788 |
| B2 | 982.19 | 32.76 | 33.362 | 48.980 | 8932 | 7457464 |
| A2 | 954.50 | 32.53 | 34.330 | 50.062 | 8932 | 7457960 |
| Mean control | 957.55 | 32.62 | 34.221 | 49.914 | 8932 | — |
| Mean candidate | 976.17 | 32.65 | 33.569 | 49.244 | 8932 | — |
| Saved PrismML | 1076.60 | 47.63 | 30.436 | 41.174 | 8288 | 7651032 |

Mean prefill saves 652 ms (+1.94% throughput), and mean request time
saves 670 ms. Mean decode throughput is +0.09%. All four responses
match all 512 PrismML IDs and text. Process swap is zero. No new
quality tasks were run for this exact component change. Prior quality
risks for EXP-2078's numerical changes carry forward. The mean prefill
gap to PrismML is 3.133 s. EXP-2080 remains a separate branch of the
experiment record and was outside this pair.

## Reproduce

Use the EXP-2076 build inputs, EXP-2073 OpenCL source and EXP-2078
profile. Build with Clang 18 and CUDA ptxas for SM89 in isolation:

```bash
exp=experiments/EXP-2026-11-26-2081-bonsai-q8-paired-weight-load
out=build/bonsai-q8-paired-weight-load-2081
base=build/bonsai-combined-fp8-pv-2076
bash "$exp/source/build.sh" "$out" "$base" build/bonsai-q8-full-tiles-2073/pipeline.cl
python3 "$exp/source/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin"
bash "$exp/source/build-http.sh" "$out" "$base"
```

Set `FRANKENMOE_CHECKPOINTS` to the artifact tree for the host build.
Run `source/check.py CORE_DIRECTORY INVENTORY CUBIN` using EXP-1973's
`core.so` and the Bonsai GGUF inventory. With all EXP-2078 overrides,
set `BONSAI_FFN_Q8_PAIRLOAD=0` for A and `=1` for B. Run the exact
[EXP-1977 request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json).
The [checkpoint receipt](../bonsai-checkpoints.json) lists the full
profile and artifact hashes.

[Component timings](data/component.json), [CUDA identity](data/device-code.json),
and [full A/B evidence](data/results.json).
