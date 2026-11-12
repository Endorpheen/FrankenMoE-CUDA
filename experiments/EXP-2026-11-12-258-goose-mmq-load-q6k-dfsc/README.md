# EXP258: Q6_K MMA tile loader -- df and sc rows in Goose

Continues EXP255/EXP257. After EXP257 the `qs` staging of `ggml_cuda_mmq_load_tiles_q6_K` was Goose and live; the
two remaining native rows in the same function are the df row and the scale rows. This experiment closes both.

## What the native rows do (MMA branch)

    // third loop, one store per thread
    i = (i0 + threadIdx.y*warp_size + threadIdx.x) % I
    x_df[i*sram_stride] = bxi->d

    // fourth loop
    i   = (i0 + threadIdx.y*rows_per_warp + threadIdx.x/(MMQ_TILE_NE_K/8)) % I
    bxi = x + kbx0 + i*stride + (threadIdx.x % (MMQ_TILE_NE_K/8)) / 4
    x_sc[i*sram_stride + threadIdx.x%4] = get_int_b2(bxi->scales, threadIdx.x % (MMQ_TILE_NE_K/8))

`block_q6_K` is 210 bytes: `ql@0`, `qh@128`, `scales@192`, `d@208`. `x_df` is `float*`, so slot indices scale by
four bytes; `bxi->d` is `ggml_half` and the native assignment widens it to float. `get_int_b2` reads the same four
bytes a single 4-byte load would, so `ld_i32` is the right primitive.

Two details kept as written rather than "simplified": the destination slot of the scale row uses `threadIdx.x % 4`
while the source index uses `threadIdx.x % (MMQ_TILE_NE_K/8)` -- they coincide only because the ratio is four today;
and the row index takes `% I` in this loop but not in the first.

## Module layout

Three separate Goose files, one entry each: `qs.goose` (EXP257's module, kept so the gate covers the whole loader),
`df.goose`, `sc.goose`. The half-to-float widening and the 4-byte load/store are device primitives; every address is
Goose arithmetic.

`emit_sources.py` emits each module with its own prefix (`fg258qs_`, `fg258df_`, `fg258sc_`) and, unlike EXP257,
also writes a fixed-name entry wrapper (`fg258df_run`, `fg258sc_run`). The C backend numbers functions by emission
order and adds a trailing `base` argument to functions that call other Goose functions, so a caller should not have to
know either fact; the wrapper resolves both mechanically from the emitted source.

## Gate

    FG258_AB mismatches=0 cuda=0 shmem=40960

Eight blocks, 32 threads, sentinel-filled staging arrays for all three regions (`qs` 4096 ints, `df` 512 floats,
`sc` 512 ints), native and Goose arms compared element by element. Zero differences.

## Integration and defect tests

Integrated into the working copy of the flint line: services and the generated code in `mmq-load-tiles.cuh`, the two
MMA-branch statements replaced by calls. Accepted profile (32K/128, run under the standing authorization):

    first_sha = e68df05b..., probs_equal, restore_ok, slot_file_size = 387989920

Defect tests (A -> B -> A), each one constant in the generated device code:

| mutation | completion |
|---|---|
| `sc` source offset 192 -> 193 | `b18650ff...` |
| `df` half offset 208 -> 206 | `a85b7375...` |
| restored | `e68df05b...` |

Both rows are therefore on the live tensor-core path, not merely linked. One recorded slip: the first integration
passed `MMQ_TILE_NE_K/8` as the tile width (the module divides it by 8 itself), which produced a division-by-zero
trap and an aborted run; the argument is now `MMQ_TILE_NE_K`, matching the gate.

Not claimed: any speed effect. Single unpaired runs on this line reported prefill 1554.8 and 1557.8 tok/s, decode
28.31 and 28.87 tok/s; without a paired A/B these are context, not a verdict.

## What remains in this loader

Nothing. `ggml_cuda_mmq_load_tiles_q6_K` is now fully Goose on the MMA branch. The next candidates are the other
quantised loaders in the same header (`q5_K`, `q2_K`, `q3_K`, `iq*`), each with the same shape: staging arithmetic
is portable, the dots and `mma()` stay primitives.
