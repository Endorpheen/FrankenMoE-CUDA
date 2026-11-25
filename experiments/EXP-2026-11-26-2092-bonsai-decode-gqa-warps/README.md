# EXP-2026-11-26-2092 — Pack independent decode KV tiles into CTAs

**Rejected at the component performance gate.** EXP-2086 stays
the cumulative checkpoint. No host routing or runtime changes.

## Mechanism

EXP-2091 recorded a 3.379 s decode attention and merge/fixup gap
to PrismML for Bonsai 32K/512. Test the narrow launch-geometry
hypothesis first: place two, four or eight original GQA3 warps
in one CTA. Each warp still covers a separate 256-row KV tile
and computes exactly the same three heads with the same FP32
operations. Keep the existing scratch layout and the original
ordered merge. The first and last partial tile indices are
unchanged. Out-of-range warps exit as a unit before shuffles.
There is no added shared or global workspace.

The new kernel is one SM89 entry launched with 64, 128 or 256
threads. It uses 95 registers rather than 80, without spills.
This probe preserves the original number of KV tiles and
does not reproduce PrismML's MMA dataflow.

## RTX 4070 component probe

Same CUBIN and seeded inputs, with Q4_0 K/V encoded by the
frozen Goose image. Ten balanced timings per geometry, including
the original merge. All partials and final outputs match GQA3
bit-for-bit at all three lengths.

| Visible keys | 1 warp, ms | 2 warps, ms | 4 warps, ms | 8 warps, ms |
| ---: | ---: | ---: | ---: | ---: |
| 32768 | 0.53146 | 0.58453 | 0.59595 | 0.60621 |
| 32769 | 0.53485 | 0.57699 | 0.59546 | 0.59608 |
| 33279 | 0.53914 | 0.59494 | 0.60363 | 0.60912 |

The fastest variant is 7.88–10.35% slower across lengths.
It misses the 10% time-reduction threshold. A full HTTP request
and process memory were not measured for the variant. The retained
EXP-2086 reference remains 1015.48 prefill / 32.59 decode tok/s and
8932 MiB process VRAM. Saved PrismML is 1076.60 / 47.63 tok/s.

## Follow-up

Focus on the work within each KV tile. The executed PrismML
decode path uses direct Q4 K/V reads and FP16 MMA, with stream-K
merge. Probe a single-query GQA variant that stages each Q4 tile
once across the six query heads and uses available FP16 MMA
fragment routines. Compare against the original GQA3 plus merge
on long contexts, including odd-tail lengths. Changing QK/PV
operand or reduction arithmetic requires a numerical probe and
targeted quality checks. Hold the full workload and profile fixed
for any subsequent 32K/512 comparison.

## Reproduce

Use the EXP-2086 combined PTX, Clang 18 and CUDA ptxas for SM89.
The harness takes the EXP-1973 bootstrap directory and GGUF
inventory, and the same frozen Goose attention image as EXP-2086.

```bash
exp=experiments/EXP-2026-11-26-2092-bonsai-decode-gqa-warps
out=build/bonsai-decode-gqa-warps-2092
check=experiments/EXP-2026-11-25-2005-bonsai-decode-gqa/source/check.py
bash "$exp/source/build.sh" "$out" build/bonsai-q8-paired-selector-2086
OPENBLAS_NUM_THREADS=1 python3 "$check" CORE_DIRECTORY INVENTORY FROZEN_PTX "$out/pipeline.cubin" --warp-tiles
```

[Measured components](data/component.json) and [build receipt](data/build.json).
