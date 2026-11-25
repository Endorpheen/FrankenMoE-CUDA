# EXP-2026-11-25-1981 — Padded Bonsai Flash MMA

**REJECTED as a runtime default.** Padding each Bonsai group from six to
eight query heads let the 256-wide Apex Flash MMA kernel run unchanged.
The two dummy heads are isolated from the real heads. A GPU kernel extracts
the six real output heads per group. The already resident Q4_0 K/V rows
are converted into the frozen kernel's F16 staging format for each call.

## Attention component

RTX 4070, generated finite query and KV operands, the same causal mask and
the frozen Goose Q4_0 writer in both arms. A–B–B–A CUDA events included
query padding, both KV conversions and output unpadding. Predetermined
limits were relative L2 < 0.003 and max absolute error < 0.002.

| KV rows / queries | Q4 tiles, ms | Flash MMA, ms | Relative L2 | Gate |
| --- | ---: | ---: | ---: | --- |
| 65 / 16 | 0.137 | 0.050 | 0.054% | Pass |
| 257 / 33 | 0.470 | 0.058 | 0.091% | Pass |
| 8192 / 128 | 51.017 | 1.041 | 0.468% | Fail |
| 32768 / 128 | 221.800 | 3.632 | 0.933% | Fail |

The attention component speedup at 32768/128 was about 61x. This figure
only covers one attention invocation with already available KV and queries.
The maximum absolute error at 32768/128 was 4.69e-4.

## Full short request

EXP-1980 decode matvec retained in both arms. RTX 4070, context 36864,
ubatch 1024, greedy, EOS masked, Q4_0 K/V, MTP off. Two balanced pairs
on 1025/32 using the same IDs from EXP-1977.

| Mode | Mean prefill, tok/s | Mean decode, tok/s |
| --- | ---: | ---: |
| Two-pass Q4 attention | 240.14 | 15.93 |
| Padded Flash MMA | 260.76 | 15.95 |
| Matched PrismML reference from EXP-1980 | 1170.99 | 54.08 |

Both pairs favored Flash prefill time. The candidate changed the fourth
output ID and 23 of 32 IDs differed across the generated sequences.
The first three IDs were equal and their selected logits differed by up
to 0.461. Both arms reproduced their separate 32-token trajectories.
Flash added 218,104,064 bytes of CUDA scratch allocation. No new 32K/512
model run was made. The candidate is off by default in the preserved
diagnostic build. Shared Apex and Inkling code and their images remain
untouched.

The frozen V-probability product uses F16 MMA accumulators, while the tiled
control accumulates in F32. The observed relative error grows with attended
length. FP16 accumulation is a candidate contributor. The experiment does
not isolate it from Q/P conversion or online softmax ordering. A next
diagnostic should compare FP32 value accumulation or shorter KV partitions
with a FP32 merge against exactly the same masks, Q4 rows and output gate.
The long-workload target remains 1858.46 prefill and 81.55 decode tok/s,
with correctness and architecture regression checks.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1981-bonsai-flash-mma
bash "$exp/source/build.sh" build/bonsai-flash-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-attention.py" \
    build/bonsai-integration build/bonsai-integration/inventory.json \
    build/bonsai-flash-repro/flash.cubin build/bonsai-flash-repro/frozen-goose.ptx
```

Use EXP-1980's 1025/32 request with `bonsai-flash-infer` and `flash.cubin`.
Set `BONSAI_FLASH=0` for the tiled control or `BONSAI_FLASH=1` for the
diagnostic candidate. Single-token decode always uses the prior Q4 path.
[Component data](component.json). [Request data](results.json).
