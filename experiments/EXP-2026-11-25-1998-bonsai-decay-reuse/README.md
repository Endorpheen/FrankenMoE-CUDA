# EXP-2026-11-25-1998 — Exact decay/beta weight reuse for Bonsai prefill

**PROVEN for short prefill and byte-identical output.** Keep the FP16
PQ2 MMQ control from EXP-1992 and the V32 GDN path. Each warp now
feeds multiple token rows from the same loads of alpha and beta
weights. Four warps share one CTA. Each output still uses the same
lane-local multiply/add sequence, warp-reduction order and F32
softplus/sigmoid epilogue. No global workspace is added.

## Component

RTX 4070, real F32 tensors from recurrent layers 0 and 42.
A fixed synthetic activation fixture exercises whole batches and
tails. All five variants return byte-identical gate and beta outputs.
CUDA-event medians, times in microseconds.

| Layer / tokens | Original | Warp pack | Reuse 2 | Reuse 4 | Reuse 8 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0 / 1 | 27.04 | 13.31 | 13.26 | 18.56 | 46.98 |
| 0 / 33 | 68.18 | 50.03 | 33.15 | 25.95 | 51.20 |
| 0 / 273 | 456.48 | 413.70 | 217.92 | 121.86 | 80.38 |
| 0 / 1024 | 1725.90 | 1543.52 | 861.04 | 444.22 | 301.02 |
| 42 / 1023 | 1718.86 | 1543.10 | 860.67 | 445.71 | 303.98 |
| 42 / 1024 | 1722.19 | 1545.73 | 860.82 | 445.34 | 302.26 |

The model uses reuse-8 for batches of at least 256 and reuse-4 for
32–255. Shorter tails and single-token decode keep the original
kernel. The experiment does not promote any decode route.

## Short model A/B

4097/16, context 36864, ubatch 1024, Q4_0 K/V, exact first 4097
input IDs from EXP-1977, greedy, EOG masked, MTP disabled. Both
modes use the prior FP16 MMQ, V32 GDN and async Flash.
A–B–B–A on one binary, changing only decay/beta batch scheduling.
Prefill saves 243.702 and 258.984 ms in the two pairs. All 16 IDs
and selected logits are byte-identical across all four native
runs. All 16 IDs match the saved PrismML result. Decode remains
around 0.974 s. Both modes allocate 1,720,400,640 bytes of GPU
workspace. Peak RAM and VRAM were not sampled in this block.

The default-mode diagnostic trace has 6.628 s prefill GPU span.
Decay/beta takes 79.380 ms, versus 345.863 ms in EXP-1992's trace.
PQ2 MMQ remains 5.467 s. The default diagnostic reproduces the
candidate outputs byte for byte. No 32K/512 model run was made.
The 5000 tok/s target remains open. The short budget gap is recorded
in the result JSON. The next precision-preserving lever should
measure FFN gate/up plus SwiGLU fusion and its intermediate traffic.
Shared Apex and Inkling code and images remain untouched.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1998-bonsai-decay-reuse
bash "$exp/source/build.sh" build/bonsai-decay-reuse-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-decay.py" \
  build/bonsai-integration build/bonsai-integration/inventory.json \
  build/bonsai-decay-reuse-repro/decay-reuse.cubin
```

Use the EXP-1992 settings with `bonsai-decay-reuse` and
`decay-reuse.cubin`. Batch reuse is the default in this experimental
Bonsai build. Set `BONSAI_DECAY_REUSE=0` for the control.
The raw trace is stored locally under
`build/bonsai-decay-reuse-1998-default/profile.nsys-rep`.

[Component data](component.json). [Short A/B](results.json).
[GPU profile](profile.json).
