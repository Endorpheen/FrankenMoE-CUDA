# EXP-2026-11-25-2007 — Bonsai ordered Flash merge and scratch waves

**Component tests passed for correctness. No runtime variant is promoted.**
EXP-2006 remains the reference. The profile captured before its decode
change assigns 12.566 s to Flash tile kernels and 2.796 s to ordered
merge on 32768 prefill tokens. The profile records 53,456 tile launches
and 53,344 ordered merge launches. The tests below isolate the merge
loop and the tile/merge wave schedule separately.

## Merge arithmetic

The control shares coefficients across four warps at each 32-tile boundary.
The competing kernels recompute the prefix maximum within each warp and
broadcast per-tile coefficients and weighted mass through shuffles.
The order of the FP32 recurrence is preserved. A second trial keeps
the shared coefficients and issues independent vector loads for 2, 4,
or 8 consecutive tiles before application, still in original order.
Every output matches bitwise on all six tested shapes, using finite
synthetic partials, positive softmax mass and the same causal limits.

At 32768 visible rows and 1024 queries, median times for one scratch
wave were 28.67 us for the control and 75.25/45.20/41.98 us for
1/2/4 warp-local variants. Removing CTA barriers therefore hurts this
shape. Prefetching 2/4/8 tiles measured 28.53/28.42/28.50 us on the same
shape. On a separate 33279/127 tail the best prefetch variants took
27.65 us vs 28.64 us. At 2048 keys they lost time. The maximum observed
mid-to-long gain does not justify projecting a measurable whole-request
improvement. Register allocations and raw samples are in `data/`.

## Wave scheduling

The 32 MiB arena divides one 32768/1024 layer into 220 tile/merge waves.
Increasing the budget to 128 MiB reduces that count to 50, but proves
slower on all five tested shapes. The existing per-layer gate and up
buffers are not used until after Attention and could back such a
temporary arena without increasing workspace. No such alias is wired.
Cache pressure is a hypothesis for the observed slowdown, not a
measured hardware counter.

The table gives median milliseconds for a complete layer's tile and merge
launches with synthetic resident FP16 K/V in the staged layout. All
budget variants produce exactly the same output bytes. The 32 MiB
control was repeated in each block.

| KV rows / queries | 16 MiB | 24 MiB | 32 MiB, repeated | 40 MiB | 64 MiB | 96 MiB | 128 MiB |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 4096/1024 | 5.994 | 5.900 | 5.892 / 6.178 | 6.214 | 7.169 | 7.352 | 7.275 |
| 8192/1024 | 12.016 | 11.617 | 12.184 / 12.250 | 13.033 | 14.150 | 14.456 | 14.152 |
| 16384/1024 | 25.048 | 25.124 | 27.054 / 27.042 | 28.944 | 30.419 | 30.084 | 29.399 |
| 32768/1024 | 64.730 | 60.743 | 60.731 / 60.908 | 61.261 | 61.969 | 62.457 | 61.380 |
| 32767/1023 | 64.872 | 60.852 | 60.860 / 60.863 | 61.400 | 61.990 | 62.443 | 61.221 |

The 24 MiB budget improves the measured middle shapes only. Preserve
that observation for a later adaptive scheduler. It does not show a
speed gain on a complete 32K request. No model benchmark or peak
model memory comparison was performed in this experiment.

## Reproduction

RTX 4070, CUDA sm89. From the repository root:

```bash
ex=experiments/EXP-2026-11-25-2007-bonsai-flash-warp-merge
base=build/bonsai-row-reuse-2006-http
out=build/bonsai-flash-warp-merge-repro
bash "$ex/source/build.sh" "$out" "$base"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/rows.cubin" "$out/merge.cubin"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check-waves.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/rows.cubin"
OPENBLAS_NUM_THREADS=1 python3 "$ex/source/check-waves.py" build/bonsai-integration \
    build/bonsai-integration/inventory.json "$base/rows.cubin" 16 24 32 40
```

The dominant prefill opportunity remains PQ2 MMQ at 31.981 s in the
same profiled request. Prior full FP16 weight expansion and ordinary
INT8 were already measured in EXP-1993 and EXP-1994. Recheck the real
400-matrix mix and packing costs before selecting a new kernel path.
Any arithmetic change requires the full fixed-history logits,
token selection, long-generation and complex-task quality gate.
