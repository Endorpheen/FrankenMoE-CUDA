# EXP-2026-11-25-2011 — Bonsai Flash tile-local Q4 and query reuse

**Accepted in the Bonsai experimental HTTP line for Q reuse only.**
EXP-2010 measured an 11.4 s prefill Flash gap to the exact PrismML
reference. Its named loader motivates a controlled direct Q4 trial.
The existing Goose tile is staged FP16 with F32 accumulation, compact
6:1 packing, 128-key tiles and an ordered merge. Both trials preserve
that arithmetic and scratch layout. Shared CUDA paths for Apex and
Inkling are unchanged.

## Component gate

Direct K, direct V and direct K+V load packed Q4 into shared memory
using aligned 16-byte transactions. Each original 32-key subtile
is then decoded in place. The half rounding and mapping match the
global staging pass. Direct V writes the same transposed half
layout. A separate pair of kernels retain global staging and run
two or four adjacent 128-key tiles per CTA with one Q conversion.
Every original partial retains its position in scratch. The merge
kernel and the number of waves stay fixed. The grouped kernels
use 96 registers and 41,216 static shared bytes, with no spills.

Measure the entire Flash chain on six shapes, including staging
where applicable. Q4 K/V comes from the existing CUDA writer. All
variants produce identical merged outputs and identical last-wave
scratch bytes, including the tested causal and size tails.
Medians in ms from the combined component block:

| KV / query tokens | Staged | Direct K | Direct V | Direct K+V | Group 2 | Group 4 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 257/33 | 0.038 | 0.051 | 0.055 | 0.063 | 0.037 | 0.042 |
| 4096/1024 | 6.058 | 9.328 | 10.582 | 13.840 | 5.862 | 5.913 |
| 8192/1024 | 12.329 | 18.134 | 20.072 | 26.043 | 11.939 | 12.103 |
| 16384/1024 | 27.211 | 37.589 | 41.800 | 53.166 | 25.930 | 25.928 |
| 32768/1024 | 61.053 | 82.598 | 90.411 | 112.175 | 59.095 | 58.182 |
| 32767/127 | 8.262 | 10.999 | 12.002 | 14.330 | 7.976 | 7.847 |

Reject direct unpacking in this topology. It replaces globally reused
FP16 staging with repeated Q4 work inside each small query CTA. The
results do not disqualify the different Stream-K scheduling in the
PrismML reference. Selecting group 2 below 16384 keys and group 4
thereafter is supported by these component observations, not claimed
as a globally optimal crossover.

## End-to-end HTTP gate

RTX 4070, same binary and CUDA images in both arms. Control disables
`BONSAI_FLASH_TILE_REUSE`. Context 36864, ubatch 1024, Q4_0 K/V,
greedy with EOG ignored, MTP off. EXP-1977 input IDs. All four
4097/64 responses in A–B–B–A match exactly. Both B prefill times
are lower than their paired controls, with only 10.11 ms mean savings.

| 32768/512, one matched pair | Control | Q reuse |
| --- | ---: | ---: |
| Prefill, tok/s | 613.63 | 619.26 |
| Prefill, ms | 53,399.86 | 52,914.64 |
| Decode, tok/s | 16.653 | 16.639 |
| Client time, s | 84.107 | 83.646 |
| Peak process VRAM, MiB | 8932 | 8932 |
| Peak process RSS, KiB | 7,457,180 | 7,456,992 |
| Process and system swap-out | 0 | 0 |

All 512 output IDs and the text hash in both runs match PrismML.
Long prefill throughput grows 0.92%. Decode throughput changes
by -0.08% in that pair. No claim about repeatability of the exact
long gain is made from one pair. The total request saves 461 ms.
The new grouping is enabled in this Bonsai experimental build;
other paths retain their prior dispatch. Shared candidate acceptance
still needs the Apex and Inkling gates. The large MMQ/MMVQ gap
to PrismML is outside this optimization.

## Reproduction

```bash
ex=experiments/EXP-2026-11-25-2011-bonsai-flash-direct-q4
out=build/bonsai-q-reuse-repro
base=build/bonsai-row-reuse-2006-http
frozen=build/bonsai-mmq-k64-2000-default/frozen-goose.ptx
bash "$ex/source/build-runtime.sh" "$out"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$frozen" "$base/rows.cubin" "$out/direct.cubin"
```

Use the EXP-2005 HTTP launcher with `"$out/frankenmoe-infer"` and
`BONSAI_CUDA_IMAGE="$out/direct.cubin"`, and retain all Flash
flags and model configuration. Set `BONSAI_FLASH_TILE_REUSE=0`
for A. Post the exact EXP-1977 request for the long test;
take 4097 prompt IDs and request 64 tokens for the short test.
See [results](data/results.json) and [hashes](data/SHA256SUMS).
