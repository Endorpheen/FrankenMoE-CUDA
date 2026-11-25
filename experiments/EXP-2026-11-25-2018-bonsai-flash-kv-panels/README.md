# EXP-2026-11-25-2018 — Share Bonsai Flash K/V between query panels

**Accepted in the Bonsai experimental HTTP line.** Two adjacent
16-row query panels occupying the same KV head share one staged K/V
load in a 256-thread CUDA block. Each panel keeps its existing query
layout, FP32 accumulation order, 128-key partials and ordered merge.
The compact variant reuses score storage for packed probabilities
after the warp completes its reads, with an explicit warp memory
barrier. It uses a 16-word V stride and 50,176 bytes of dynamic
shared memory, versus 61,952 bytes for the initial two-panel
variant. No global memory allocation is added. Neither variant
changes the shared Apex or Inkling model paths.

## Component and dispatch

All merged outputs and aligned-wave final partials are byte-identical
to the existing grouped tiles on six shapes, including causal and
query tails. The component includes K/V staging, tile launches and
merge. The chosen compact two-tile path uses 78 registers and an
8-byte stack frame with no spills. CUDA event medians in ms:

| KV / queries | Active grouping control | Initial panels best | Compact panels, 2 KV tiles |
| --- | ---: | ---: | ---: |
| 257/31 | 0.036 | 0.043 | 0.043 |
| 4096/1024 | 5.606 | 6.604 | 5.759 |
| 8192/1024 | 11.972 | 12.432 | 10.813 |
| 16384/1024 | 27.193 | 30.544 | 25.644 |
| 32768/1024 | 60.690 | 67.764 | 57.460 |
| 32767/127 | 8.328 | 9.434 | 7.652 |

The initial geometry loses time over all tested shapes. The compact
geometry is selected at 8192 or more keys when the staged compact
async path is active and both panels can share a KV head. Wave
capacity is rounded down to an even count only in that case. The
global scratch allocation is unchanged. For odd panels, insufficient
wave capacity or unsupported dynamic shared memory, keep the known
Flash path. The table contains isolated layer times, not model
throughput.

## End-to-end results

RTX 4070, identical executable and CUDA images in the four arms.
Context 36864, batch/ubatch 4096/1024, Q4_0 K/V, affinity 0f0f,
greedy, EOG masked, MTP and prompt reuse off. Same exact request as
EXP-1977. A–B–B–A, with the candidate enabled by default.

| 32768/512 | Prefill A → B, tok/s | Decode A → B, tok/s |
| --- | ---: | ---: |
| First pair | 606.00 → 618.36 | 32.678 → 32.664 |
| Reverse pair | 605.69 → 618.53 | 32.696 → 32.670 |
| Aggregate phase times | 605.85 → 618.45 | 32.687 → 32.667 |

Average prefill saves **1.102 s**, or **+2.08% throughput**. Average
decode changes -0.06%. Both full requests are shorter by 1.073 s
and 1.110 s respectively. All four outputs match all 512 PrismML
IDs and UTF-8 text SHA-256. Peak VRAM is 8932 MiB in every arm.
VmHWM is 7,457,288–7,457,316 KiB in A and 7,457,328–7,457,464 KiB
in B. Swap-out is zero. This confirms Bonsai-only acceptance.
Shared Apex and Inkling release regression gates remain open.

An additional CUDA-only 32768/16 trace returns the same 16 IDs and
records 53,184 launches of the compact panel kernel. Panel tiles
take 11.209 s, remaining ordinary tiles 0.577 s, and the original
stage and ordered merge take 0.140 and 2.739 s. PQ2 MMQ remains the
largest prefill group (29.734 s FP16 plus 2.407 s integer). This
profile is diagnostic and separate from the scored pairs.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2018-bonsai-flash-kv-panels
dev=build/bonsai-flash-kv-panels-repro
out=build/bonsai-flash-kv-panels-http-repro
frozen=build/bonsai-mmq-k64-2000-default/frozen-goose.ptx
bash "$ex/source/build.sh" "$dev"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$frozen" build/bonsai-decode-decay-2017/decay.cubin "$dev/panels.cubin"
bash "$ex/source/build-runtime.sh" "$out" "$dev"
```

Use the EXP-2017 server options with this executable and
`BONSAI_CUDA_IMAGE="$out/panels.cubin"`. Set
`BONSAI_FLASH_KV_PANEL=0` for A and leave it unset for B.
Post the exact EXP-1977 request. The separate trace request sets
`n_predict=16`. See [results](data/results.json), [CUDA profile](data/profile-summary.json)
and [hashes](data/SHA256SUMS).
