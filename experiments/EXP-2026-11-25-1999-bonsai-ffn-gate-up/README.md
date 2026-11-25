# EXP-2026-11-25-1999 — Exact Bonsai FFN preparation fusion

**PROVEN for fused SwiGLU, signed FWHT and FP16 packing.** The experiment
also retains the rejected gate/up MMQ fusion variants. The accepted
path changes only Bonsai prefill batches of at least 32 tokens.
Separate gate and up projections remain in use. Q8 is not involved.

## Rejected joint gate/up projection

Both combined kernels reuse the same activation fragment and emit the
SwiGLU output directly. 32x64 stages both weight planes, while 64x64
reruns the stage with a shared weight buffer. Their outputs are all
byte-identical to two separate FP16 MMQs and the SwiGLU kernel.
The full stage medians on actual layer-0 weights at 1024 tokens were
11.430 ms separate, 14.713 ms fused 32x64 and 17.693 ms fused 64x64.
At layer 42, 1024 tokens gave 10.221, 13.622 and 16.161 ms. The
larger fused tile uses more registers (64 versus 40 in the normal
64x64 projection) and its stage reuses shared storage sequentially.
The experiment does not isolate register pressure from scheduling.
Both fusion variants were rejected at the component gate.

## Accepted tail fusion

After separate projections, one CUDA block computes SwiGLU, applies
the signed Hadamard transform in shared memory, and emits the same
packed halfword pairs for down MMQ. Arithmetic and butterfly order
are unchanged. The graph's only consumer of the rotated output is
the down projection, and the host marks its packed data ready. The
separate path is retained for short tails and single-token decode.

RTX 4070, real PQ2 gate/up projection outputs on a fixed finite
activation fixture, and actual model signs. All packed bytes match.

| Layer / tokens | Separate three stages, ms | Fused tail, ms |
| --- | ---: | ---: |
| 0 / 33 | 0.0251 | 0.0177 |
| 0 / 257 | 0.1998 | 0.1100 |
| 0 / 1024 | 1.0128 | 0.4060 |
| 42 / 1023 | 1.0173 | 0.4041 |
| 42 / 1024 | 1.0243 | 0.4055 |

## Short end-to-end evidence

4097/16, context 36864, ubatch 1024, Q4_0 K/V, first 4097 IDs
from EXP-1977, greedy, EOG masked, MTP disabled. A–B–B–A with
the EXP-1998 decay path and existing FP16 MMQ, GDN and Flash.
Both pairs save prefill time by 125.584 and 148.368 ms.
All 16 IDs and selected logits match byte for byte across the
four arms. All 16 IDs match the saved PrismML 4097/16 request.
Decode remains approximately 0.974 s. Both modes allocate
1,720,400,640 bytes GPU workspace. System and process memory peaks
were not sampled. No long model run was made here.

Default selection matches the wide control fixture output after
rebuilding. The independent 4097/16 diagnostic profile places
101.884 ms in the fused tail (256 launches). On the same EXP-1998
profile shape, the replaced per-microbatch SwiGLU, 17408-wide
FWHT and activation pack totaled 258.884 ms. Other FWHT and
pack calls are still present. MMQ is still 5.474 s of the
6.476 s GPU prefill span. The short gap to 5000 is in results.json.
Both intermediate F32 buffers remain allocated for fallback.
A future experiment can shrink their byte capacity to the largest
fallback batch and verify the small-tail path. Apex and Inkling
code and images remain unchanged.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-1999-bonsai-ffn-gate-up
bash "$exp/source/build.sh" build/bonsai-ffn-tail-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-fusion.py" \
    build/bonsai-integration build/bonsai-integration/inventory.json \
    build/bonsai-ffn-tail-repro/ffn-fused.cubin
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-tail.py" \
    build/bonsai-integration build/bonsai-integration/inventory.json \
    build/bonsai-ffn-tail-repro/ffn-fused.cubin
```

Use EXP-1998 runtime settings with `bonsai-ffn-tail` and
`ffn-fused.cubin`. Set `BONSAI_FFN_TAIL=0` for the control.
The fused tail runs by default in this experimental Bonsai build.
Raw profile: `build/bonsai-ffn-tail-1999-default/profile.nsys-rep`.

[Rejected joint projection data](gate-up-component.json).
[Accepted tail data](tail-component.json).
[Short A/B](results.json). [GPU profile](profile.json).
