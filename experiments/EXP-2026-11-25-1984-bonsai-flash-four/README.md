# EXP-2026-11-25-1984 — Four-query grouping for staged Bonsai Flash

**REJECTED on throughput.** The two-query staged FP32 baseline remains
unchanged. This experiment retains Q4_0 KV staging from EXP-1983 and the
128-key partition and FP32 arithmetic from EXP-1982. Each CTA shares a
staged fragment across four query tokens instead of two. Two warp pairs
own 16 rows each. Shared Q storage is recycled for probabilities, and
score storage is reused for the swizzled V fragment.

## Component gate

RTX 4070, same deterministic Q4_0 fixtures as EXP-1983. CUDA event
brackets cover staging, tile processing and merge. Each row is the
mean of two arms in an A–B–B–A block.

| KV rows / query tokens | Two-query control, ms | Four-query, ms |
| --- | ---: | ---: |
| 65 / 16 | 0.037 | 0.047 |
| 257 / 33 | 0.059 | 0.059 |
| 8192 / 128 | 6.788 | 6.769 |
| 32768 / 128 | 29.278 | 31.420 |
| 1024 / 1024 | 2.072 | 2.291 |

All output bytes match on all five fixtures. Relative L2 against
frozen Q4 tiles remains 0.0286% at 32768/128. The candidate takes
7.3% more time on that shape and 10.6% more time at 1024/1024.
Its compiled tile uses 96 registers and 41,728 B of shared memory,
versus 93 and 41,216 B for control, with no spills in either build.
The wider group doubles the number of per-warp value accumulator
fragments. The experiment does not isolate the effect of register
pressure, CTA scheduling or key staging cadence. A future test can
measure each separately.

## Short token gate

Bonsai 273/8 on the first 273 IDs of EXP-1977, context 36864, ubatch
128, Q4_0 K/V, greedy with EOS masked, MTP disabled. One execution
in each mode. All eight token IDs and selected logits match byte
for byte through a partial final microbatch. Both modes allocate
1,112,077,568 B of device workspace. The single request timing is
recorded for diagnosis. The throughput verdict uses the balanced
component brackets. Apex and Inkling remain outside the candidate
path. No new long model benchmark was run.

The next Flash probe should attribute the partial-result write and
reload cost before changing the tile width again. Preserve bounded
scratch, ordered FP32 merge, the causal tail, and the same L2 and
short output gates.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1984-bonsai-flash-four
bash "$exp/source/build.sh" build/bonsai-flash-four-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-four.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-flash-four-repro/flash-four.cubin build/bonsai-flash-four-repro/frozen-goose.ptx
```

Use the EXP-1979 invocation with `bonsai-flash-four`, `flash-four.cubin`,
`BONSAI_FLASH_F32=1 BONSAI_FLASH_REUSE=1 BONSAI_UBATCH=128`,
and the first 273 input IDs from EXP-1977. Set `BONSAI_FLASH_FOUR=1`
only in the diagnostic arm.

[Component data](component.json). [Short token gate](results.json).
