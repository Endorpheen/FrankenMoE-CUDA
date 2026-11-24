# EXP-2026-11-24-1886 — Fold dense MMVQ warps

**Rejected at the standalone probe.** In [EXP-1870](../EXP-2026-11-24-1870-gdn-phase-trace/README.md),
dense MMVQ with 8192 output rows accounted for 605.2 ms across 20,440
launches in the profiled Apex decode. This experiment changes the work
distribution in `fg_mmvq_row` for eight quant blocks per row. Each thread
computes either two or four of the old per-thread partials. The lane-wise
sum, warp shuffle order and Q4_K/Q6_K dot product are preserved.
The 64-thread variant needs 256 bytes of shared memory per CTA (control:
384). The 32-thread variant needs no shared memory or CTA barrier.
There is no change to the weight format or amount of matrix data read.

## RTX 4070 kernel probe

Eight deterministic weight matrices and packed inputs per quantization,
8192 output rows and 2048 input elements. Each candidate matched every
FP32 word for each fixture (262,144 comparisons in all). Median time in
microseconds per CUDA Graph node from 12 alternating sample blocks, 96 nodes
per sample. One-matrix tests reuse the same weights; the rotating tests
cycle 72 MiB of Q4_K or 105 MiB of Q6_K weights.

| Shape | Weight reuse | 128 threads | 64 threads | 32 threads |
| --- | --- | ---: | ---: | ---: |
| Q4_K | One matrix | 13.173 | 12.032 | 13.296 |
| Q4_K | Eight matrices | 21.360 | 21.312 | 21.861 |
| Q6_K | One matrix | 13.168 | 12.853 | 11.776 |
| Q6_K | Eight matrices | 30.363 | 30.320 | 30.299 |

The strongest rotating-matrix results save 0.048 us (Q4_K) and 0.064 us
(Q6_K) per node. Even the faster of these, multiplied by all 20,440
historical launches, gives about 1.3 ms. The one-matrix result does not
generalize to the rotating working set. These are standalone timings,
not measures of actual Apex cache residency or end-to-end performance.
No full-model candidate was integrated or measured. Apex decode,
prefill, RAM and VRAM effects remain unmeasured. A similar folding scheme
may be worth revisiting for a verified cache-resident projection.

## Reproduce

Use a copy of the EXP-1786 frozen device tree. Apply `mmvq-goose.patch`
to the frozen `device/goose/mmvq.goose`, re-emit with self-hosted KeelWing
v0.3.0 preview and run the frozen `tools/frankenmoe-fragment.py` as in
[EXP-1859](../EXP-2026-11-24-1859-flash-interior/README.md), using fragment
name `mmvq`. Copy its device and types headers into `device/`. Apply
`cuda-mmvq.patch` to `device/glue/cuda-mmvq.cl`. Use the EXP-1859 Clang 18
command and assemble the four `goose_mmvq_fold_*` entries with CUDA 12.6
`ptxas -arch=sm_89` as `candidate.cubin`. Assemble only
`goose_mmvq_dense_matvec` from the archived MMQ PTX as `control.cubin`.

Place both cubins next to [probe.py](probe.py), install NumPy and run
`python3 -u probe.py`. The [run log](probe.log), [compiler reports](ptxas.log)
and [raw samples with cubin hashes](results.json) are included.
