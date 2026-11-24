# EXP-2026-11-24-1882 — Share Q8_1 loads in the routed up/gate/SwiGLU kernel

**REJECTED at the kernel stage.** The frozen EXP-1753 control cubin
is byte-for-byte equal to the fused image in EXP-1880. EXP-1870
measured about 479 ms in 20440 launches of its
`goose_mmvq_up_gate_glu` entry. This experiment factors out the
duplicate Q8_1 reads and the two integer activation sums shared by
the up and gate projections. It uses the exact eight-block case
of the existing Apex dispatch. FP32 dot accumulation, cross-warp
reduction and reference SwiGLU remain per projection and per row.

The first variant computes one row per CTA as before. The second
computes two rows, shares their Q8_1 input and uses one CTA barrier.
Both variants reproduce all 122880 FP32 values bit-for-bit in every
probe. All three cubins have zero stack frame and no spills.
Registers are 40 for control, 37 for the single-row candidate
and 40 for the two-row candidate. Per-CTA shared storage is
768, 768 and 1536 bytes respectively.

| Repeated GPU probe | Control median µs/layer | Candidate median µs/layer |
| --- | ---: | ---: |
| One row, shared activation loads | 23.423 | 23.635 |
| Two rows, one barrier | 21.922 | 23.096 |

These comparisons use independent control readings from each
block. Times cover ten distinct synthetic layers in a CUDA Graph.
Each row summarizes four A-B-B-A comparisons with eight warm-ups
and 64 measured graph replays before each timed sample.
The initial single-row observation looked slightly positive
but did not reproduce. No driver integration or full Apex run
was made. EXP-1880 remains the accepted research checkpoint.

## Reproduce

Use the exact EXP-1753's CUDA glue and cubin and the frozen EXP-1786
device tree. Apply the matching [source patches](source), including
the named return-struct mapping in the fragment tool. Re-emit the
Q4 fragment using KeelWing, compile through Clang 18 and assemble
only `goose_mmvq_up_gate_glu` with CUDA 12.6 ptxas for SM89.
Verify [SHA256SUMS](SHA256SUMS) before comparing.

```sh
gcc-13 -O2 -Wall -Wextra -Werror -I"$cuda/include" probe.c -Wl,-l:libcuda.so.1 -o probe
./probe "$exp1786/device/goose-cuda-mmq.ptx" control.cubin candidate.cubin
sed 's/arm ? fused : original,hidden,routes/arm ? fused : original,arm ? hidden\/2 : hidden,routes/' probe.c > probe-rows2.c
gcc-13 -O2 -Wall -Wextra -Werror -I"$cuda/include" probe-rows2.c -Wl,-l:libcuda.so.1 -o probe-rows2
./probe-rows2 "$exp1786/device/goose-cuda-mmq.ptx" control.cubin rows2/rows2.cubin
```

[Structured output](results.json), [one-row measurements](probe-repeat.log)
and [two-row measurements](rows2-probe.log) include all repeated samples.
