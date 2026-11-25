# EXP-2026-11-25-1990 — Async activation and warp fragment loads for Bonsai PQ2 MMQ

**PROVEN on component and short prefill.** Compile the inherited
32x32 PQ2 MMQ tile with aligned shared operands and warp matrix loads.
The accepted kernel also fills the packed activation tile with 16-byte
CUDA async copies while decoding the weight tile. Both tile operands
and the ordered FP32 MMA sequence are unchanged. The resident PQ2
format remains exact. The experimental half-scale lookup variant uses
byte permutes to assemble the same halfword values. It is excluded
from the runtime selection because it has no repeatable advantage.

## Component

Real PQ2 tensors, finite fixed synthetic inputs, RTX 4070. Medians
of four CUDA-event timings per mode. All output bytes matched for all
five shapes and all variants. Input packing is identical and outside
this kernel bracket.

| Matrix / tokens | Original, ms | ldmatrix, ms | Async, ms | Async + lookup, ms |
| --- | ---: | ---: | ---: | ---: |
| GDN QKV tail / 33 | 0.351 | 0.330 | 0.288 | 0.288 |
| GDN QKV / 1024 | 5.695 | 5.553 | 3.876 | 3.875 |
| Attention output / 1024 | 3.567 | 3.532 | 2.352 | 2.350 |
| FFN up / 1024 | 9.501 | 9.319 | 6.563 | 6.567 |
| FFN down / 1024 | 9.206 | 9.085 | 5.920 | 5.913 |

## Full short A/B

4097/16, first 4097 raw IDs from EXP-1977, context 36864, ubatch
1024, Q4_0 K/V, greedy, EOG masked, MTP disabled. Both arms retain
EXP-1988 single-buffer async Flash and EXP-1980 ordered decode.
Native A–B–B–A on one binary. Prefill saved 2.716 s and 2.934 s in
the two pairs. Decode stayed at about 0.974 s. All four native
outputs for IDs and selected F32 logits matched byte for byte. All
16 token IDs also matched the saved same-input PrismML reference
from EXP-1985. Exact CUDA workspace allocation was identical
at 1,720,400,640 B. Peak system or process memory was not sampled.
A separate 1025/16 smoke verified the accepted mode without an env
override and matched the previous short control's bytes.

The candidate profile records 6.843 s of PQ2 MMQ, down from 9.898 s
in the EXP-1989 profile. GDN still consumes 4.456 s, and the two
together account for about 90% of the current 12.578 s GPU span.
The next major prefill test should qualify an accelerator for GDN
on the real 16-key/48-value-head geometry. Validate per-layer state,
causal tails, and token sequences. Then reprofile prior to further MMQ
work. Nothing in the shared Apex and Inkling execution paths changed.
No new 32K/512 model request was run. The matched short PrismML prefill
of 1263.84 tok/s remains well ahead of this candidate.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1990-bonsai-pq2-mmq-loads
bash "$exp/source/build.sh" build/bonsai-mmq-loads-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-loads.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-mmq-loads-repro/pq2-mmq-loads.cubin
```

Use the EXP-1988 4097/16 invocation with `bonsai-mmq-loads` and
`pq2-mmq-loads.cubin`. The async MMQ is the default in this experimental
Bonsai build. Set `BONSAI_PQ2_MMQ_ASYNC=0` for the scalar-load control.
Use the profiler commands from EXP-1989 with the new executable and
module. The raw profile is local at
`build/bonsai-mmq-loads-1990-runtime/profile.nsys-rep`.

[Component data](component.json). [Model A/B](results.json).
[Updated GPU budget](profile.json).
