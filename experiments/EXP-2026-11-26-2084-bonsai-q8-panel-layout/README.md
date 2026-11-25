# EXP-2026-11-26-2084 — Panel-major Bonsai Q8 gate/up activations

**Retained in the cumulative experimental checkpoint.** The working
EXP-2049 runtime is unaffected. Combined quality acceptance remains
open, and the small decode difference changes sign across the pairs.

## Mechanism and component probe

Keep EXP-2081's M64 × N128 tile and paired PQ2 weight loads. Arrange
Q8 activation words and scales by 128-token panel, then by K block,
then by token. Adjacent tokens within the K block now occupy adjacent
128-byte rows instead of starting 5120 B apart. The matching writer
reads the original activations and performs the same quantization
and scale calculation. The consumer reads the same bytes and
performs the same integer products and ordered FP32 accumulation.
The dispatcher switches writer and consumer together only for full
5120-to-17408 paired-load FFN tiles. Existing buffers, KV, tails,
decode and MTP are unchanged. The new MMQ uses 94 registers, 28,416 B
shared memory, and zero spills. The existing 92 CUDA code sections
match byte-for-byte.

At 1024 tokens, four real PQ2_0 gate/up tensors produce bitwise
identical output. Balanced medians of eight observations per arm
include a fresh activation pack on each call:

| Tensor | EXP-2081, ms | Panel-major, ms | Time saved |
| --- | ---: | ---: | ---: |
| Layer 0 gate | 2.17146 | 2.06029 | 5.12% |
| Layer 0 up | 2.17240 | 2.06048 | 5.15% |
| Layer 62 gate | 2.18061 | 2.05563 | 5.73% |
| Layer 62 up | 2.17478 | 2.06179 | 5.20% |

All four clear the 5% component threshold.

## RTX 4070, HTTP 32K/512

All four valid runs are listed. Same GGUF, request, executable,
CUBIN, frozen PTX, argv and relevant process environment except
`BONSAI_FFN_Q8_PANEL`. EXP-1977 raw input IDs, context 36864,
batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123, empty prompt
cache and MTP off. GPU temperature and clocks before launch were
observed, not locked. The first pair was inconclusive on full time,
so a second pair was run in reverse order.

| Run | Prefill, tok/s | Decode, tok/s | Prefill, s | Client, s | VRAM after request, MiB | VmHWM, KiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| A | 980.38 | 32.71 | 33.424 | 49.068 | 8932 | 7457668 |
| B | 981.21 | 32.53 | 33.395 | 49.125 | 8932 | 7457460 |
| B2 | 991.94 | 32.73 | 33.034 | 48.671 | 8932 | 7458044 |
| A2 | 977.99 | 32.70 | 33.506 | 49.154 | 8932 | 7457576 |
| Mean control | 979.19 | 32.71 | 33.465 | 49.111 | 8932 | — |
| Mean candidate | 986.57 | 32.63 | 33.215 | 48.898 | 8932 | — |
| Saved PrismML | 1076.60 | 47.63 | 30.436 | 41.174 | 8288 | 7651032 |

Prefill saves 28 and 471 ms in the two pairs. The full request
takes 57 ms longer in the first pair and 483 ms less in the reverse
pair. Mean savings are 250 ms in prefill and 213 ms per request,
with +0.75% prefill and −0.23% decode throughput. All four responses
match all 512 PrismML IDs and text. Process swap is zero. EXP-2081's
earlier 976.17 / 32.65 tok/s was a separate run block and is not
the control for this comparison. The measured mean prefill gap to
PrismML is 2.778 s. Quality evidence for this exact change consists
of bitwise components and the fixed full-workload IDs/text.
Inherited numerical changes still require combined quality review.

## Executed graph

One separate Nsight Systems capture on B, using EXP-2066's capture
method and analyzer. The analyzed 32K/512 response matches PrismML.
It reports 993.79 / 32.37 tok/s and 48.782 s client time under
profiling, outside the scored pairs. Post-request VRAM is 8,944 MiB
and VmHWM is 8,001,748 KiB under the tracer, with zero swap.

The [complete kernel inventory](data/kernel-dispatch.json) and
[phase attribution](data/attribution.json) compare against the
previous [EXP-2082 full control profile](../EXP-2026-11-26-2082-bonsai-full-pipeline-after-pairload/README.md).
The candidate uses the new pack 1,984 times and new MMQ 3,968 times.
The remaining 64 pack calls use the original layout. After mapping
the two replaced kernel names, launch counts and the SHA-256 of
the ordered kernel names and launch geometries match the control.
The full ordered timelines are retained locally at the paths below.

| Measured GPU time | EXP-2082 control | EXP-2084 panel-major |
| --- | ---: | ---: |
| Gate/up full MMQ | 8.482 s | 7.991 s |
| Gate/up Q8 pack, including fallback | 49.508 ms | 51.784 ms |
| All prefill PQ2 and pack | 21.707 s | 21.165 s |
| Prefill attention | 5.863 s | 5.861 s |
| Traced prefill GPU gaps | 80.945 ms | 77.139 ms |

The traces show 104 H2D copies and one D2H copy in prefill, with the
same byte totals. Both captures show no CUDA graph launches or
in-request CUDA allocations. These are separate diagnostic captures.
The 488 ms sum for the two changed stages describes the traces.
The balanced unprofiled pair result above is the request comparison.

## Reproduce

Use the EXP-2081 build directory, including its `component.cl`,
and EXP-2081's staged profile. Clang 18 generates PTX and CUDA ptxas
targets SM89. The host build uses GCC 13 and the prior checkpoints.

```bash
exp=experiments/EXP-2026-11-26-2084-bonsai-q8-panel-layout
out=build/bonsai-q8-panel-layout-2084
base=build/bonsai-q8-paired-weight-load-2081
bash "$exp/source/build.sh" "$out" "$base"
python3 "$exp/source/verify-code.py" "$base/pipeline.cubin" "$out/pipeline.cubin"
bash "$exp/source/build-http.sh" "$out" "$base"
```

Set `FRANKENMOE_CHECKPOINTS` to the artifact tree for the host
build. Call EXP-2081's `source/check.py CORE_DIRECTORY INVENTORY
CUBIN paired panel`. Use the same binary and image for both arms
with `BONSAI_FFN_Q8_PANEL=0` and `=1`. Serve one
[raw request](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/request.json)
per arm with all EXP-2081 overrides. The raw traces are retained at
`build/bonsai-full-pipeline-profile-2082/trace.sqlite` and
`build/bonsai-q8-panel-layout-2084/trace/trace.sqlite`.

[Component evidence](data/component.json), [CUDA identity](data/device-code.json),
and [full A/B, effective settings and hashes](data/results.json).
