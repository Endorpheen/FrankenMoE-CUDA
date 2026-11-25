# EXP-2026-11-25-1992 — Wide Bonsai PQ2 MMQ tiles

**PROVEN for short prefill and exact output.** Widen the EXP-1990 async
MMQ tile from 32x32 to 64x64, each CTA owning 64 matrix rows and
64 token columns. The CTA uses 256 threads. Each warp accumulates four
8-column fragments with the same ordered FP32 MMA operations. The
resident weight format, half activation packing and per-element
operation order stay unchanged. The wider tile reuses weight blocks
across twice as many tokens and activations across twice as many rows.
No global memory is added.

## Component check

RTX 4070, real GGUF tensors, fixed synthetic input. CUDA events measure
the kernel with identical prepacked activations, four timings per arm.
All four modes produced identical output bytes on all six shapes.

| Matrix / tokens | 32x32, ms | 32x64, ms | 64x32, ms | 64x64, ms |
| --- | ---: | ---: | ---: | ---: |
| QKV tail / 33 | 0.292 | 0.216 | 0.284 | 0.234 |
| QKV / 257 | 1.144 | 1.065 | 1.053 | 1.043 |
| QKV / 1024 | 3.883 | 3.208 | 3.558 | 3.171 |
| Attention output / 1024 | 2.369 | 1.985 | 2.170 | 1.943 |
| FFN up / 1024 | 6.567 | 5.419 | 6.010 | 5.375 |
| FFN down / 1024 | 6.636 | 5.525 | 6.079 | 5.444 |

64x64 uses 40 registers and 34,816 B shared memory per CTA, no spills.
The runtime enables it from 128 tokens onward on the async MMQ path.
Smaller batches retain the existing 32x32 shape. This conservative
small-batch choice avoids claiming robust sizing from one tail fixture.

## Short model gate

4097/16, context 36864, ubatch 1024, Q4_0 K/V, first 4097 IDs from
EXP-1977, greedy with EOG suppressed, MTP disabled. Both modes
use EXP-1991 GDN and EXP-1988 async Flash. A–B–B–A on one executable.

| Runtime | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| Native 32x32 | 486.98 | 16.429 |
| Native 64x64 | 590.25 | 16.424 |
| Saved matched PrismML 4097/16 | 1263.84 | 53.03 |

Both pairs improve prefill, by 1.460 and 1.484 s. All 16 IDs and
selected logits are identical across native arms. All IDs match
the saved matched PrismML output. Peak process VRAM is 8932 MiB in
all four runs. Peak RSS (VmHWM) varies from 7,418,452 to 7,418,912 KiB.
Process swap and system swap-out increments are zero. Decode stays
within 1.7 ms between paired runs. A separate short profile verified
output bytes when wide selection is left at its default.

The 4097/16 default profile has 6.886 s prefill GPU span and
5.462 s in PQ2 MMQ, versus 7.116 s in the EXP-1991 profile.
F32 decay/beta projections take 0.346 s, Flash tiles 0.187 s,
GDN 0.169 s, and transform 0.150 s. MMQ remains the dominant cost.
A next comparison should include a once-per-matrix GPU expansion
into bounded reusable scratch, including the preparation cost,
so redundant unpack can be separated from matrix throughput.
No new 32K/512 model request was run. Shared Apex and Inkling
paths and device images are unchanged.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1992-bonsai-mmq-wide
bash "$exp/source/build.sh" build/bonsai-mmq-wide-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-wide.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-mmq-wide-repro/pq2-mmq-wide.cubin
```

Use EXP-1991 settings with `bonsai-mmq-wide` and `pq2-mmq-wide.cubin`.
Set `BONSAI_PQ2_MMQ_WIDE=0` for the async 32x32 control. Wide is
the experimental Bonsai build default for batches of at least 128.
Raw profile: `build/bonsai-mmq-wide-1992-default/profile.nsys-rep`.

[Component data](component.json). [Short model A/B](results.json).
[Updated GPU loss map](profile.json).
