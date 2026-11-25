# EXP-2026-11-25-2037 — Compressed shared PQ2 fragments for prefill

**Rejected at the component gate.** EXP-2035 attributes
7.315 s of 32K prefill to FFN down. Test whether storing
the existing PQ2 blocks in shared memory and decoding
the FP16 A operands in registers reduces that cost.
The control is EXP-2019 cached 64×128 K64, as shipped in
the EXP-2036 experimental CUDA image. No runtime dispatch
or production artifact is changed.

The candidate stages each complete 34-byte PQ2 block once
per CTA, with two padding bytes for alignment. It keeps
the scales in their original format. Each warp decodes just
its FP16 MMA fragment. B remains in the normal FP16 shared
layout and still uses ldmatrix. The two initial kernels
differ in activation stage depth (K64 or K128). Four
follow-up variants bound the inner K loop, and two of
those also reduce the number of accumulator fragments.
Every candidate preserves the original
`mma.sync.aligned.m16n8k16.row.col.f32.f16.f16.f32` order.

## RTX 4070, SM89 evidence

Seven real GGUF weight fixtures, common FP16 activation
packing, one process, balanced microbenchmark order.
All six variants match the same 51,898,369 FP32 output
values byte for byte. This includes the 1023-token and
5119-row joint tail. CUDA event medians cover the projection
kernel after the common input pack.

| Projection | Tokens | Cached K64, ms | Best candidate, ms | Time increase |
| --- | ---: | ---: | ---: | ---: |
| Layer 0 FFN down | 1024 | 3.659 | 4.054 | 10.79% |
| Layer 0 FFN down, tail | 1023 | 3.628 | 3.924 | 8.14% |
| Layer 42 FFN down | 1024 | 3.251 | 3.688 | 13.43% |
| Layer 63 FFN down | 1024 | 3.367 | 3.597 | 6.84% |
| Layer 0 FFN up | 1024 | 3.459 | 3.582 | 3.54% |
| Layer 0 QKV | 1024 | 1.904 | 2.069 | 8.67% |
| Layer 3 attention out | 512 | 0.586 | 0.659 | 12.36% |

Initial K64/K128 both use 128 registers. Disabling full
unroll and adjusting the shape reduces that to 56–80, but
still loses on all tested matrices. K64 reduces shared
memory from 27,648 to 20,736 B. K128's larger activation
stage instead uses 37,120 B at 64×128. There are no spills.
The packed variant replaces one reusable CTA-level FP16
expansion with per-warp unpacking. The two warp groups
covering the same output rows decode the same weight
fragments independently. Measured component time rules out
this tradeoff as an improvement. The test did not collect
hardware occupancy or instruction-stall counters.

No full-model prefill/decode, VRAM or RAM is measured.
Variants allocate no additional global memory in the test.
Keep the EXP-2019 prefill dispatch. Avoid repeating this
layout with a larger unroll. The remaining large prefill
opportunity needs a different treatment of work or
arithmetic. EXP-2008 documented the minimum for the dense
FP16 arithmetic count, and EXP-1994/1996/2023 record
quality failures for previously tried Q8 routes. An
alternative numeric path requires the fixed-history logits
and multi-request quality gates before promotion. There
is no established speed reuse for other architectures.

## Reproduction

Run from the root with the EXP-2036 build tree present.

```bash
exp=experiments/EXP-2026-11-25-2037-bonsai-mmq-packed-fragment
out=build/bonsai-mmq-fragments-repro
bash "$exp/source/build-image.sh" "$out"
env -i PATH=/usr/bin:/bin HOME=/home/end0 LANG=C CUDA_VISIBLE_DEVICES=0 OPENBLAS_NUM_THREADS=1 taskset 0f0f /usr/bin/python3 "$exp/source/check.py" build/bonsai-integration build/bonsai-integration/inventory.json "$out/fragment.cubin"
```

[Measured samples, register counts and hashes](data/)
