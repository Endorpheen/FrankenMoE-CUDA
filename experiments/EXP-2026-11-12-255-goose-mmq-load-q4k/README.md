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

## First harness run: compiled, but the kernel faults

`source/cuda-runtime.cu` now carries both sides: `native_load` (the MMA-branch body copied verbatim) and the Goose
call, into separately zeroed staging arrays, compared element by element.

Build and run:

    nvcc -c ... -> /tmp/fg255.o (160,440 bytes)
    FG255Q_AB mismatches=0 cuda=700

`cuda=700` is `cudaErrorIllegalAddress`: the kernel faults, so "0 mismatches" is not a result -- the comparison
either did not run to completion or read garbage after the fault. Recorded as a failed first run rather than a pass.

Most likely causes, in order: the `half2` staging arrays are not 4-byte aligned after my pointer arithmetic, or the
shared-memory request (2*64*32 + 2*64*16 ints) exceeds what a single block can reserve without opt-in.

Next: align the staging arrays explicitly, size the request conservatively, and only then read the mismatch count.

## Second run: the harness works, the port does not

The `cuda=700` was mine, not the kernel's: the harness passed a **host** `std::vector` into the kernel as if it were a
device pointer. With a `cudaMalloc` + `cudaMemcpy` buffer:

    FG255Q_AB mismatches=18432 cuda=0

So the comparison now runs end to end and the Goose loader disagrees with the native one on both the `qs` staging and
the `dm` rows. That is the honest state: the measuring instrument is sound, the thing it measures is wrong.

Next: print the first differing index per array, which separates "wrong slot arithmetic" (`sram_stride`, `16*(txi/8)`,
`txi%8`) from "wrong source bytes" (`qs@16`, `scales@4`, `dm@0`).

## DM fixed, QS still wrong

Two findings from the index diff:

1. **DM was missing the sign.** Native computes `dm = bxi->dm * make_half2(1.0f, -1.0f)` and only then multiplies by
   `(sc, m)`; my primitive multiplied the raw `dm`. Symptom: `native=bf004a40 goose=3f004a40` -- one sign bit.
   After the fix the DM rows match and the count dropped 18432 -> 16384.
2. **QS still differs at the first index**: `native=090f0903 goose=020e000f`. `get_int_b4(x, i)` is
   `((const int *)x)[i]`, i.e. 4 bytes at `qs + 4*i`, and `qs` is at byte 16 -- which is what the module does, so the
   source offset is not obviously wrong. The remaining suspects are the argument order actually received by the
   generated function, and the mask applied after the shift.

DM matching while QS does not is useful: the staging slot arithmetic (`sram_stride`, `16*(txi/8)`, `txi%8`) is
shared by both writes, so the slot formula is right and the disagreement is in the bytes read or the mask.
