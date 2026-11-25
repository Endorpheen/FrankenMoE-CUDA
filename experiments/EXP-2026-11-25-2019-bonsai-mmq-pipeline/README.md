# EXP-2026-11-25-2019 — Reuse PQ2 scales across Bonsai K64 half-blocks

**Accepted in the experimental Bonsai HTTP line for packed weight reuse.**
Alternating activation buffers are rejected at the component gate.
The accepted replacement reads both code slices and their FP16 scale
at the start of each original 128-weight block. Each thread carries
four packed 32-bit entries into the second K64 stage and unpacks the
same half operands in the original sequence. The FP32 MMA order,
global tensor layout, Q8 gate/up schedule, and DP4A decode stay fixed.

## Component gate

Real Bonsai PQ2_0 projections with identical packed FP16 activations.
All outputs are byte-identical across nine shapes, including
short tails. Alternating K32/K64 activation tiles add shared memory
or barriers and fail to improve the main shapes. In the first
block the sole gain is on the isolated 1023-token down fixture.
The accepted weight-cache approach uses the same shared allocation
as K64 (18,432 or 27,648 B), at 56 or 72 registers per thread, with
no spills. It adds no global scratch. Representative times in ms:

| Projection / tokens | Current K64 | Best double-buffered X in final block | Selected cached PQ2 |
| --- | ---: | ---: | ---: |
| Layer 0 QKV / 128 | 0.328 | 0.349 | 0.304 |
| Layer 0 QKV / 257 | 0.814 | 0.834 | 0.766 |
| Layer 0 QKV / 1024 | 2.492 | 2.455 | 2.125 |
| Layer 0 FFN gate / 1024 | 3.749 | 3.973 | 3.431 |
| Layer 0 FFN up / 1024 | 3.653 | 3.875 | 3.444 |
| Layer 0 FFN down / 1023 | 3.641 | 3.862 | 3.440 |
| Layer 42 FFN gate / 1024 | 3.714 | 3.866 | 3.275 |
| Layer 42 FFN down / 1024 | 3.732 | 3.954 | 3.375 |
| Layer 3 Attention output / 1024 | 1.266 | 1.324 | 1.147 |

The selected cache path follows the existing tile width rule: 64
tokens per tile below 512 and 128 tokens per tile at 512 or more.
It runs only when wide async K64 is selected. Small paths and
one-token projections keep their existing dispatch.

## RTX 4070 HTTP results

Same executable, CUDA image, model and request in all arms. Q4_0 KV,
context 36864, batch/ubatch 4096/1024, affinity 0f0f, greedy and EOG
masked, MTP and prompt reuse off. Four arms in A–B–B–A order.

| 32768/512 | Prefill A → B, tok/s | Decode A → B, tok/s |
| --- | ---: | ---: |
| First pair | 618.78 → 647.75 | 32.654 → 32.702 |
| Reverse pair | 617.48 → 649.83 | 32.691 → 32.705 |
| Aggregate phase times | 618.13 → 648.79 | 32.672 → 32.703 |

Average prefill saves **2.505 s** with **+4.96% throughput**.
Decode changes +0.09% in the aggregate. Full client wall saves
2.391 s and 2.648 s in the separate pairs. Every arm reproduces
512 PrismML IDs and the same completion text SHA-256. Peak process
VRAM is 8932 MiB throughout. VmHWM is 7,457,340–7,457,900 KiB for
A and 7,457,420–7,457,444 KiB for B. Process and system swap-out
stay zero. This change is scoped to the Bonsai adapter. Apex and
Inkling release gates remain open.

A separate CUDA-only 32768/16 trace reproduces the first 16 IDs and
observes 11,776 launches of cached wide FP16 K64, taking 27.133 s.
The corresponding EXP-2018 trace took 29.734 s with the same call
count. This is cross-trace attribution, separate from the scored
within-block A/B. The new trace also measures 2.394 s integer
MMQ, 11.203 s of shared Flash panels and 2.711 s of ordered merge.
Measured prefill remains below the 1093.21 tok/s PrismML reference
and above the 17.63 s prefill budget for the +70% target.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2019-bonsai-mmq-pipeline
dev=build/bonsai-mmq-pipeline-repro
out=build/bonsai-mmq-pipeline-http-repro
base=build/bonsai-flash-kv-panels-2018-lean-rebuild
bash "$ex/source/build.sh" "$dev" "$base"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/panels.cubin" "$dev/pipeline.cubin"
bash "$ex/source/build-runtime.sh" "$out" "$dev"
```

Use the EXP-2018 HTTP arguments with this executable and
`BONSAI_CUDA_IMAGE="$out/pipeline.cubin"`. Set
`BONSAI_MMQ_CACHED_PQ2=0` for A and leave it unset for B.
Post the exact EXP-1977 request. For the trace use `n_predict=16`.
[Paired data](data/results.json), [CUDA trace](data/profile-summary.json)
and [hashes](data/SHA256SUMS).
