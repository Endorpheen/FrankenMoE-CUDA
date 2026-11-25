# EXP-2026-11-26-2091 — Full Bonsai 32K/512 GPU comparison with PrismML

**Diagnostic.** Compare one complete PrismML capture with the saved
EXP-2086 capture. Same RTX 4070, PQ2_0-mtp GGUF, raw EXP-1977 input IDs,
context 36864, batch/ubatch 4096/1024, Q4_0 K/V, greedy, seed 123, EOS
masked and MTP off. Both cold-prefill gates pass and all 512 response
IDs and text match. The PrismML capture uses the prior executable and
source revision 910442b. An earlier capture produced a complete HTTP
response but retained only 484 reads of logits after the request
extended beyond its trace window. It is excluded from attribution.

## Full phase comparison

Use the first complete 248,320-logit readback to divide prefill and
decode. Both captures contain all 512 readbacks. GPU activity is
the union of kernel, copy and memset spans; kernel sums are separate.

| Trace measure, s | PrismML | Goose EXP-2086 | Goose minus PrismML |
| --- | ---: | ---: | ---: |
| Prefill window | 29.761 | 32.023 | 2.262 |
| Prefill GPU gaps | 0.221 | 0.079 | −0.143 |
| Prefill PQ2 and visible pack | 17.072 | 20.112 | 3.040 |
| Prefill attention | 4.2815 | 5.952 | 1.670 |
| Decode window | 10.744 | 15.783 | 5.039 |
| Decode GPU gaps | 0.333 | 1.024 | 0.691 |
| Decode projections and visible Q8 pack | 8.383 | 8.879 | 0.496 |
| Decode attention and merge/fixup | 0.950 | 4.329 | 3.379 |
| Decode vocabulary D2H service, 511 copies | 0.0202 | 0.0201 | −0.0001 |

The PrismML projection row counts the executed `mul_mat_q` and
`quantize_mmq_q8_1` in prefill and `mul_mat_vec_q` plus
`fwht_quantize_q8_1` in decode. Other packing and fusion remain
in the kernels that perform them. Sums are directional and are
not additive guaranteed savings. PrismML's recurrent
`gated_delta_net_cuda` alone takes 3.486 s in prefill, while the
Goose GDN, decay and convolution group takes 2.774 s. The groups
have different scope.

PrismML reports 1101.57 prefill / 47.57 decode tok/s and 40.500 s
client time in this diagnostic capture. Goose reports 1024.06 /
32.38 tok/s and 47.800 s. Post-request VRAM is 8,300 versus 8,944
MiB with tracing. VmHWM is 8,196,596 versus 8,001,004 KiB and both
have zero process swap. Keep the unprofiled comparison at
1076.60 / 47.63 tok/s for saved PrismML and 1015.48 / 32.59
for EXP-2086. The scored prefill deficit remains 1.833 s.

## Actual SM89 dispatch

- PrismML prefill calls `mul_mat_q<142,128,false,30,false>`
  12,704 times. The recorded dispatch uses 256 threads, 254 registers
  and 57,856 B of dynamic shared memory. Its paired activation pack
  is J128 Q8_1/D4, 12,704 calls. The executed entry is the generic
  MMQ, with the PQ2 loader in `ggml-cuda/mmq-load-tiles.cuh`.
  The dedicated J128 kernel in `mmq-pq2-j128.cuh` is gated to
  SM90+ in `launch_mul_mat_q` and does not appear in this trace.
  Goose uses the exact paired PQ2 unpack in its INT8 gate/up
  and the FP16 half2 path in FFN down. The arithmetic differs.
- In decode, PrismML runs
  `flash_attn_ext_f16<256,256,1,8,false,false,false,Q4_0>`
  with 184 CTAs of 64 threads, followed by 32 fixup CTAs. Goose
  runs `bonsai_decode_gqa3` over 8 × 129 or 8 × 130 one-warp CTAs,
  then `goose_q4_attention_merge` over 24 CTAs. Both run one tile
  and one merge/fixup launch per layer-step, 8176 times. The tile
  count and scheduling inside each launch differ considerably.
- PrismML traces 565,499 CUDA Graph kernel nodes in decode and
  none in prefill. Goose has zero in both phases. The decode
  gap delta is about 0.69 s. The trace does not isolate Graph
  gains from host work or scheduling. The 3.38 s attention
  difference remains the larger decode lead.

The priority for prefill remains PQ2, with D4 arithmetic and any
derived candidate subject to quality checks. EXP-2010's earlier
D4 probe did not reproduce the PrismML schedule. Use the actual
J128 launch geometry and K-partition behavior when selecting the
next component experiment. For decode, the strongest separate
target is Q4 attention. Test consolidating KV work per CTA while
preserving ordered FP32 merge boundaries. Use CPU `perf` to
resolve the smaller launch and scheduling gap independently.

## Artifacts and reproduction

[EXP-1977](../EXP-2026-11-25-1977-bonsai-prism-32k-reference/README.md)
contains the server arguments and frozen request. Use the
`validate-cold-32k.py` before/after gate. Capture with Nsight Systems,
CUDA Graph node collection and periodic CUDA flushing, starting the
single request soon after readiness within the 85-second capture.
Capture the request UTC nanosecond markers and process memory
as in EXP-2086. Export to `trace.sqlite` and run:

```bash
exp=experiments/EXP-2026-11-26-2091-bonsai-prism-full-comparison
python3 "$exp/source/compare.py" build/bonsai-prism-full-profile-2091-complete build/bonsai-q8-paired-selector-2086/trace
```

[Phase timings, all kernel counts and hot geometries](data/attribution.json)
and [build, launch and exclusion receipt](data/build.json).
