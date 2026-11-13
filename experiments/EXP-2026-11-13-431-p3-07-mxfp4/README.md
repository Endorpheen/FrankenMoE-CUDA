# EXP431: MXFP4 MMQ activation quantizer in Goose

`quantize_mmq_mxfp4<scatter>` was the last supported activation quantizer on
this build. This experiment ports it and gates direct and scatter byte for byte
against the private-renamed native kernel.

## What was ported

`source/mxfp4.goose`:

- warp decomposition: `warp_start = (blockIdx.y * blockDim.y + threadIdx.y) * 64`,
  `k_block`, `quad_idx`, group/lane and the two 32-value sub-blocks;
- warp max with the native 16/8/4/2/1 shuffle ladder;
- E8M0 scale: `log2f`, `__float2int_rn`, bias `-2`, clamp 0..254;
- E8M0 to float with the native bit path (`0x00400000` for zero, else `e << 23`);
- FP4 E2M1 quantization: sign bit, strict-less search over
  `{0, 0.5, 1, 1.5, 2, 3, 4, 6}`;
- lane shuffles for the four values of each packed char2;
- nibble packing and `block_fp4_mmq` writes: `d4[quad] = (scale1 << 8) | scale0`,
  `qs` char2 at `quad*16 + 0*8 + group` and `quad*16 + 1*8 + group`;
- direct and scatter block indices.

Host plan (`source/mxfp4-host.goose`): direct grid `(ne1, ceil(ne0/512),
ne2*ne3)`, scatter grid `(n_tokens, ceil(ne0/512), 1)`, block `(32,8,1)`, with
the `ne0 % 64 == 0` guard.

The only extern primitives are loads/stores, shuffles, libm `log2f`, `rint`,
`__frcp_rn` and the E8M0 bitcast. Every index, comparison, LUT step and pack is
Goose.

## Gate

| case | result |
|---|---|
| direct, ne0 512 / 1024 / 2048 | byte-identical to native |
| scatter, 4 tokens x 8 experts, 32 rows | byte-identical to native |
| total checks | **11, 0 failures** |
| memcheck / racecheck / initcheck / synccheck | 0 / 0 / 0 / 0 |

Mutations, all caught:

| mutation | failures |
|---|---:|
| warp max offset 16 -> 8 | 1 |
| E8M0 bias -2 -> -1 | 4 |
| FP4 LUT 3.0 -> 2.5 | 4 |
| nibble pack shift 4 -> 5 | 4 |
| D4 scale high byte shifted | 4 |

## NVFP4

NVFP4 is not a porting gap on this host: `quantize_mmq_nvfp4` is compiled only
under `BLACKWELL_MMA_AVAILABLE` and is `NO_DEVICE_CODE` on sm_89. Its consumer is
a Blackwell FP4 tensor-core path. It is recorded as **hardware-unavailable on
sm_89**, not silently skipped. A future universal runtime can add a software
NVFP4 quantizer plus a BF16 dequant/GEMM fallback under the cross-platform
compatibility task (P3-18/P10-10); it cannot be gated against a native oracle on
this machine because that oracle does not exist here.

## Reproduce

    bash experiments/EXP-2026-11-13-431-p3-07-mxfp4/build_test.sh
    runtime/exp431/build/gpu-fixture
    python3 experiments/EXP-2026-11-13-431-p3-07-mxfp4/mutate.py
