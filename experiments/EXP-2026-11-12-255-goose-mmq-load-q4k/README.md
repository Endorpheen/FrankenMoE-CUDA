# EXP255: Q4_K tile loader for the MMA layout, in Goose (started)

The probe in EXP-2026-11-12-253 proved the tensor-core route executes: making
`ggml_cuda_mmq_load_tiles_q4_K` return immediately changed the completion to `a85b7375`. So the loader is live code
and this is the first piece of CUDA_MMQ that has to become Goose.

## What the native loader does (MMA branch)

`block_q4_K` is `{ ggml_half2 dm; uint8_t scales[12]; uint8_t qs[128]; }` = 144 bytes: **dm @0, scales @4, qs @16**.

    qs0 = get_int_b4(bxi->qs, txi)
    x_qs[i*sram_stride + 16*(txi/8) + txi%8 + 0] = (qs0 >> 0) & 0x0F0F0F0F
    x_qs[i*sram_stride + 16*(txi/8) + txi%8 + 8] = (qs0 >> 4) & 0x0F0F0F0F
    dm  = bxi->dm * make_half2(1.0f, -1.0f)
    x_dm[i*sram_stride + 4*ksc + l] = dm * make_half2(sc8[l], m8[l]),  l = 0..3

where `sc32/m32 = unpack_scales_q45_K(scales, ksc + 0 / +2)` and `sc8/m8` are its four bytes.

## State

`source/device.goose` carries the arithmetic with the correct offsets (an earlier draft guessed
`qs@4/scales@138/dm@130`, which is wrong for this structure). Primitives that stay in the bridge: `ld_i32`, `st_i32`,
`unpack_scales_q45_K` (it lives in `mmq-load-tiles.cuh`), and the half2 pack/multiply.

Not gated yet. Next: the A/B harness at kernel level (one thread pair computing native and Goose staging for the same
block), then substitution and the accepted-profile check.
