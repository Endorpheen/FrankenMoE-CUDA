# EXP257: Q6_K MMA tile loader (qs part) in Goose

`block_q6_K` = `{ ql[128]; qh[64]; int8_t scales[16]; ggml_half d; }` = 210 bytes: ql@0, qh@128, scales@192, d@208
(the same layout EXP412 pinned down for the CPU kernel).

Native MMA-branch qs write, per (i, txi): unpack `ql` with 4-bit masks, unpack `qh` with the `(txi & 8) >> 2` shift and
`0x30303030` mask, form `kq0/kq1`, then `__vsub4(x, 0x20202020)` -- four independent byte subtractions.

## Gate

    FG257D_AB mismatches=0 cuda=0

Eight blocks, whole staging array, native vs Goose. The `vsub4` is ported as real per-byte arithmetic, not left as an
intrinsic.

## Live and defect-tested

Called from the native qs loop of `ggml_cuda_mmq_load_tiles_q6_K`. Accepted profile:

    first_sha = e68df05b..., probs_equal, restore_ok

Defect test: subtracting `0x20202021` instead of `0x20202020` changed the completion to `22ea97d3...`; restoring
returned `e68df05b`.

## Still native in this loader

The `df` row (`x_df[i*sram_stride] = bxi->d`) and the `sc` rows (`get_int_b2(bxi->scales, ...)`). Both are a load and a
store with no arithmetic to speak of; they are the next candidates.
