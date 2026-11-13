# EXP430: D4 and D2S6 MMQ Q8_1 activation layouts in Goose

P3-07's activation half was DS4-only. The device body in EXP180 already had
D4/D2S6 branches, but the host refused those layouts and the D2S6 stores were
wrong. This experiment enables and fixes them, then gates all three layouts
against the native kernel.

## Changes

- Host (`host.goose`): accept layout 0 (D4), 1 (DS4), 2 (D2S6) instead of
  refusing anything but DS4.
- Launch (`cuda-runtime.cu`): the kernel is templated on `(layout, scatter)` and
  dispatches to the six Goose wrappers; the layout arrives from
  `mmq_get_q8_1_ds_layout(type_src0)`.
- D2S6 (`device.goose`): two real defects fixed.
  1. The sum was stored at union offset `4 + 2*(2 + iqs/16)`; native
     `d2s6[2 + iqs/16]` is at `2*(2 + iqs/16)`. The extra 4 bytes were removed.
  2. The sum reduction used the 8/4/2/1 shuffle ladder. D2S6 has
     `vals_per_sum = 16`, so the native ladder is 2/1. The max ladder stays
     8/4/2/1 because `vals_per_scale = 64`.
- D2S6 stores are `half` (`st_half`, `__float2half_rn`), not `float`; D4 stores
  `float`, DS4 stores `half2`.

## Gate

The candidate DSO carries the private-renamed native entry points and the Goose
entry points. The bridge reads `GGML_GOOSE_CUDA_MMQ_Q8_1` per call, so both arms
run in one process and the raw `block_q8_1_mmq` arrays are compared byte for
byte.

| type | layout | direct widths | scatter |
|---|---|---|---|
| Q4_0 | DS4 | 128, 512, 2048 | 4 tokens x 8 experts, 32 rows |
| Q8_0 | D4 | 128, 512, 2048 | same |
| Q2_K | D2S6 | 128, 512, 2048 | same |

Result: **24 checks, 0 failures**; memcheck/racecheck/initcheck/synccheck all 0;
5/5 mutations:

| mutation | result |
|---|---|
| D2S6 sum shuffle 2 -> 4 | 4 failures |
| D2S6 sum slot stride 2 -> 3 | process abort (out-of-bounds write traps) |
| D2S6 scale index 6 -> 7 | 4 failures |
| D2S6 sum window 96 -> 64 | 4 failures |
| D4 scale slot 5 -> 4 | 4 failures |

The aborting mutation is a detection, not a harness pass: it writes outside the
block and the process traps.

## Still open in P3-07

- MXFP4 activation quantizer (`quantize_mmq_mxfp4`, direct and scatter) is still
  native.
- NVFP4 (`quantize_mmq_nvfp4`) is gated by `BLACKWELL_MMA_AVAILABLE` and
  compiles to `NO_DEVICE_CODE` on this sm_89 build; it is a hardware-unavailable
  variant, not a silently skipped path.
- P3-07 therefore stays IN_PROGRESS until MXFP4 is ported and NVFP4 is recorded
  as hardware-unavailable.

## Reproduce

    bash experiments/EXP-2026-11-13-430-p3-07-d4-d2s6/rebuild_test.sh
    runtime/exp430/build/gpu-fixture
    python3 experiments/EXP-2026-11-13-430-p3-07-d4-d2s6/mutate.py
