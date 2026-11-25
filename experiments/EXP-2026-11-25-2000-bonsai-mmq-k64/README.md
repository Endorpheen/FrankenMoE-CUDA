# EXP-2026-11-25-2000 — Exact K64 staging for Bonsai PQ2 MMQ

**PROVEN for short prefill and exact output.** Each K64 shared stage
processes half a PQ2 weight block and preserves the original order
of FP32 Tensor Core updates. Inputs remain FP16, weights remain
resident PQ2, and the prior GDN, Flash and FFN tail stay unchanged.
Test K64 with 64x64, 64x128, 128x64, and 128x128 output tiles.
The baseline is EXP-1992 K128 64x64. No new global memory is added.

## Component gate

RTX 4070, real model matrices and seeded finite activations. Four
CUDA event samples per mode with common prepacked operands.
All outputs match byte for byte on all eleven shapes. Medians in ms:

| Matrix / tokens | K128 64x64 | K64 64x64 | K64 64x128 | K64 128x64 | K64 128x128 |
| --- | ---: | ---: | ---: | ---: | ---: |
| blk.0.attn_qkv / 33 | 0.235 | 0.203 | 0.375 | 0.224 | 0.335 |
| blk.0.attn_qkv / 128 | 0.450 | 0.331 | 0.371 | 0.431 | 0.333 |
| blk.0.attn_qkv / 511 | 1.615 | 1.181 | 1.153 | 1.258 | 1.292 |
| blk.0.attn_qkv / 512 | 1.615 | 1.181 | 1.157 | 1.259 | 1.289 |
| blk.0.attn_qkv / 768 | 2.398 | 1.753 | 1.713 | 1.861 | 1.927 |
| blk.0.attn_qkv / 257 | 1.042 | 0.801 | 0.958 | 0.843 | 0.991 |
| blk.0.attn_qkv / 1024 | 3.191 | 2.320 | 2.269 | 2.459 | 2.293 |
| blk.3.attn_output / 1024 | 1.935 | 1.415 | 1.386 | 1.507 | 1.549 |
| blk.0.ffn_up / 1024 | 5.384 | 3.904 | 3.875 | 4.145 | 4.246 |
| blk.0.ffn_down / 1024 | 4.955 | 3.601 | 3.605 | 4.010 | 4.635 |
| blk.0.ffn_down / 1023 | 4.824 | 3.538 | 3.513 | 3.912 | 4.556 |

The 64x64 K64 setup reduces shared allocation from 34,816 to
18,432 B per CTA at the same 40 registers. The 64x128 candidate uses
27,648 B and 56 registers, with no spills. On all large tested
projections the K64 candidates beat the K128 baseline. The 128-row
candidates were slower than the selected shapes. These resource
figures explain why more CTAs may fit on an SM. No hardware
occupancy counter was used for this attribution.

The runtime selects 64x64 K64 at 128–511 tokens and 64x128 K64 from
512 onward when the existing wide async mode is active. The
transition was checked on QKV shapes 128, 257, 511, 512, 768, and
1024, plus other matrices at 1023 and 1024. Smaller batches retain
their earlier path and single-token decode is unchanged.

## Short model gate

4097/16, context 36864, ubatch 1024, Q4_0 K/V, first 4097 EXP-1977
input IDs, greedy, EOG masked and MTP disabled. A–B–B–A with one
runtime binary changes only MMQ tile staging. The two pairs save
1476.584 and 1482.086 ms prefill. All ID and selected logit bytes
match across native arms, and all 16 IDs match the saved same-input
PrismML result. Both modes allocate 1,720,400,640 B GPU workspace.
Decode remains about 0.974 s in both modes.

| Arm | Prefill tok/s | Decode tok/s |
| --- | ---: | ---: |
| K128 control | 627.34 | 16.435 |
| K64 candidate | 811.06 | 16.419 |
| Saved PrismML at 4097/16 | 1263.84 | 53.03 |

A separate 273/8 pair with ubatch 128 exercises the 64x64 K64
dispatch and the short fallback. Prefill saves 122.377 ms and
all eight output IDs and selected logits match byte for byte.
The default-selection 4097/16 profile reproduces the candidate
output exactly after rebuilding. MMQ accounts for 3.984 s of the
5.005 s GPU prefill span, versus 5.474 s in EXP-1999's trace.
With a target of 819.4 ms for 4097 inputs at 5000 tok/s, the
scored A/B mean still needs to recover 4.232 s.
This figure is short-workload evidence. No 32K benchmark or
host/device peak memory sampling was performed here. Apex and
Inkling paths and shared CUDA images remain unchanged.

## Reproduction

```sh
exp=experiments/EXP-2026-11-25-2000-bonsai-mmq-k64
bash "$exp/source/build.sh" build/bonsai-mmq-k64-repro
OPENBLAS_NUM_THREADS=1 python3 "$exp/source/check-k64.py" \
    build/bonsai-integration build/bonsai-integration/inventory.json \
    build/bonsai-mmq-k64-repro/mmq-k64.cubin
```

Use EXP-1999 settings with `bonsai-mmq-k64` and `mmq-k64.cubin`.
Set `BONSAI_PQ2_MMQ_K64=0` for the control. K64 is selected by
default in this experimental Bonsai build. The raw diagnostic
profile is local at
`build/bonsai-mmq-k64-2000-default/profile.nsys-rep`.

[Component data](component.json). [Short A/B](results.json).
[Updated prefill profile](profile.json).
