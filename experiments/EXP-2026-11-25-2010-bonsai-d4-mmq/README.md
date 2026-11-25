# EXP-2026-11-25-2010 — Attribute the Bonsai PrismML gap and test D4

## Direct comparison

PrismML is the **same** local llama-server, revision
`910442b0ce1128ef85d7730cbcae835cce60cdd4`, used by EXP-1977 on the
PQ2_0 weights. The second runtime is the accepted EXP-2006 Goose
HTTP executable. Each runs the exact same 32768 prompt IDs and
returns 16 output IDs, all equal. Both use RTX 4070, context 36864,
batch/ubatch 4096/1024, Q4_0 K/V, greedy and EOG masked, MTP off.
CUDA graph node collection is enabled. The two runs are diagnostic
rather than unprofiled acceptance measurements. Kernel bins exclude
Prism warm-up; the first vocabulary output separates prefill from
15 continuation evaluations. Exact trace-derived names and
template instances are retained in [data](data/trace-comparison.json).

| Region for 32K/16 | Goose, ms | PrismML, ms | Goose minus PrismML, ms |
| --- | ---: | ---: | ---: |
| Prefill wall | 53,448.68 | 28,437.25 | 25,011.43 |
| PQ2 MMQ, visible pack and MMQ fixup | 32,136.35 | 16,335.37 | 15,800.98 |
| Prefill Flash stage, tile, merge or fixup, and mask preparation | 15,485.09 | 4,081.49 | 11,403.60 |
| Main prefill GDN recurrence kernels alone | 1,397.85 | 3,340.69 | -1,942.84 |
| Decode wall, 15 continuation evaluations | 905.74 | 309.49 | 596.26 |
| Decode PQ2 projections and visible quantizing FWHT | 598.77 | 234.80 | 363.98 |
| Decode Attention tiles and merges | 120.68 | 27.66 | 93.02 |

The rows use scoped kernel sums. They are not estimates of the savings
from isolated changes. Any fused cost attributable to more than one
operation is grouped by the named kernel. Goose's FFN tail includes
additional packing inside its fused kernel, outside the visible pack
row. First-prefill GPU span is 53,446.90 ms vs 28,405.45 ms. Decode
span is 904.80 ms vs 305.40 ms. This phase-level evidence places the
largest gap on GPU execution, with no host scheduling or PCIe cause
assigned from these CUDA-only captures.

**Executed paths.** The trace records `mul_mat_q<142,128,...>` and
`quantize_mmq_q8_1<D4,...>` on PrismML, 12,704 launches each.
Goose runs `bonsai_pq2_mmq_64x128_k64` 12,800 times with half operands.
PrismML selects `flash_attn_ext_f16<256,256,32,2,...,Q4_0>` or the
one-query 256-wide Q4_0 instantiation, with stream-K fixups. Goose
materializes half K/V staging and uses (53,456) tile and (53,344)
ordered-merge launches in prefill. The 132 ms explicit staging pass
alone cannot account for the 11.4 s Attention difference. Both the
tile schedule and tile work need attention. Quantized one-column
`mul_mat_vec_q<142,1,...>` plus fused `fwht_quantize_q8_1` are active
in PrismML decode. Goose runs `bonsai_pq2_matvec_rows2` and ordered
FP32 matvec, with separate normalization and rotation. PrismML also
executes its `add_rms_norm_fwht_f32` fusion. GDN alone is already
faster here on Goose.

Sources at the tested revision:
[MMQ](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/mmq.cu),
[MMVQ](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/mmvq.cu),
[Flash dispatch](https://github.com/PrismML-Eng/llama.cpp/blob/910442b0ce1128ef85d7730cbcae835cce60cdd4/ggml/src/ggml-cuda/fattn.cu).

## D4 arithmetic pilot

**Rejected for promotion on the quality gate.** The independent CPU
oracle reproduces token-major D4 codes, scale bytes and sampled FP32
products exactly on seven real GGUF shapes, including row and token
tails. The pilot reproduces the per-32 scale contract and round-away
ties but retains its own token-major scratch arrangement and tile
order. It is not a binary-equivalent copy of PrismML's MMQ scheduler.

Pack plus matrix timings for the 64-token tile improve by about 17–20%
vs FP16 on the wide FFN fixtures. Layer-0 gate is 3.974 → 3.232 ms and
up is 3.981 → 3.236 ms. The prior one-scale-per-128 Q8 path is faster
at 2.419/2.418 ms but has more projection error on these fixtures.
D4 reduces relative L2 from roughly 0.66–0.75% to 0.54–0.57%.
The D4 128-token tile uses 142 registers and is slower than FP16
on most shapes. D4 64 uses 70 registers, 19,712 shared bytes, no spills.

The model probe applies D4 only to gate/up and retains FP16 elsewhere.
On the code-trace task the first output difference is position 60,
and independently graded cell accuracy goes from 18/63 to 16/63.
The scheduler task changes after 43 tokens. Ledger and incident
extraction sequences match. Fixed-history top-1 agrees at 38/39
sampled positions. No long runtime A/B is scored after this failure.

## Reproduction

Build the D4 cubin with `source/build.sh` and run `source/check.py`
with the same inventory, control cubin and D4 cubin ordering as the
script arguments. Build the diagnostic model executable with
`source/build-quality.sh OUTPUT DEVICE_BUILD`. Run `source/quality.py`
with the EXP-1994 quality harness arguments. The case suite and
scoring thresholds are unchanged.

The full trace reports are local under
`build/bonsai-prism-cuda-audit-2010` and
`build/bonsai-goose-cuda-audit-2010`. The recorded request is
`build/bonsai-exp2005-profile-2006/request.json`, the EXP-1977 workload
with `n_predict=16`. Start the respective existing HTTP binaries
with the EXP-1977 and EXP-2006 settings on separate free loopback
ports, one run at a time, under
`nsys profile --trace=cuda --cuda-graph-trace=node --sample=none --cpuctxsw=none`.
After processing the request and exporting each report to
`trace.sqlite`, run `source/compare-traces.py PRISM_TRACE_DIR GOOSE_TRACE_DIR`.

The measured Flash gap motivates a direct Q4 tile plus scheduling
experiment with matching component arithmetic. The measured MVQ gap
motivates a separate one-column DP4A prototype with fixed-history,
free-generation and independent-scoring gates. The accepted FP16
binary and shared Apex/Inkling paths remain unchanged.
