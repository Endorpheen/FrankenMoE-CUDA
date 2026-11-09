# EXP184: CUDA mul_mat_vec_f in Goose

Hypothesis: implement `mul_mat_vec_f` for the shape the Apex profile executes
(T=float, one destination column, no fusion) in actual Goose and enable it by
default. It is a large share of the per-token path, so the paired measurement is
meaningful. Verdict: **ACCEPTED**, no regression.

## Scope transferred

`mul_mat_vec_f<float, float, ncols_dst=1, has_fusion=false>`, reached through
`ggml_cuda_mul_mat_vec_f`, plus the block-size search in the host module. Goose owns
the channel/sample/row address arithmetic including the ids path, the float2-strided
dot product, the FMA accumulation, the warp `shfl_xor` reduction, the cross-warp pass
through shared staging, and the store.

Left native and refused by the parent: half, bfloat16 and quantized sources,
`ncols_dst > 1`, any fusion (gate/bias), and `is_multi_token_id`.

## Apex pair, same DSO, only this op switched

Order native -> goose -> goose -> native:

| arm | prefill tok/s | decode tok/s |
|---|---:|---:|
| native-rollback a | 2565.63 | 26.07 |
| goose a | 2572.93 | **27.15** |
| goose b | 2577.23 | 26.04 |
| native-rollback b | 2564.00 | **26.24** |
| native median | 2564.82 | 26.16 |
| goose median | 2575.08 | 26.60 |
| delta | **+0.40%** | **+1.69%** |

Content sha256 identical in all four runs (`e68df05b`). The witness
`FG184M_CUDA_MMVF ... paths=0x1` appears only in the Goose arms.

## Local gate

| check | result |
|---|---|
| `gpu-fixture` Goose | 10 checks / 0 failures / 5 cases |
| `gpu-fixture` native rollback | 10 / 0 / 5 |
| byte-for-byte output | **5 of 5** |
| compute-sanitizer memcheck | 0 errors |
| export surface vs EXP183 | exactly the two diagnostics |

Cases cover ncols 256, 512, 2048, 4096 and 8192, which span the dispatcher's
block-size branches, plus a four-channel case that exercises the y channel stride.
A host oracle in double precision was added to the fixture so that a disagreement
can be attributed rather than guessed at; both arms sit within 1e-6..1e-5 of it,
which is the normal float spread, and Goose is not the worse of the two.

## Defects in my own work, found and fixed

1. **Warp reduction used the wrong shuffle.** I wrote a `shfl_down` ladder; native
   uses `__shfl_xor_sync` with offsets width/2..1, which leaves the full sum in
   *every* lane. With down-shuffles only lane 0 holds it, so the cross-warp pass
   read garbage. Fixed.
2. **Column stride was not halved.** Native passes `stride_col_y/2` because the kernel
   indexes `float2` pairs. Fixed.
3. **Bases were not in pair units.** Native advances the x and y pointers in float
   element units and only then casts to `float2`, so the pair-indexed base is that
   offset halved. Fixed; this also removed the out-of-bounds reads memcheck reported.
4. **Missing channel ratio.** Without ids native derives the source channel by
   dividing by `channel_ratio = nchannels_dst/nchannels_x` and wraps the y channel
   modulo `nchannels_y`. I had used `channel_dst` directly, which made the
   four-channel case wrong by a relative 24.9. Fixed.
5. **Missing zero fill in the cross-warp staging.** Native zeroes `buf_iw[tid]` for
   `tid < warp_size` before the per-warp writes, so slots above the warp count read
   zero instead of stale data. Restored.
6. **The block-size search ran on the wrong quantity.** This was the last 1-ULP
   difference and the root cause of it: native searches block sizes on the full
   `ncols`, not on the pair count `ncols2`. Using `ncols2` picked 64 instead of 128
   threads for ncols=256 and 128 instead of 256 for ncols=512, which changes the
   accumulation order. Fixed in the host module; the kernel divides by two itself.
   This is what took the fixture from 3/5 to 5/5.

Contraction was also probed explicitly: with FMA forbidden the fixture drops to 0/5,
so native does contract and the FMA form is the correct one to match.

## Rollback and reproduce

`GGML_GOOSE_CUDA_MMVF=0` restores the private-renamed native
`ggml_cuda_mul_mat_vec_f`; any shape the Goose body does not cover also falls back
inside the candidate. Reproduce with `python3 emit_sources.py`,
`python3 build_runtime.py`, `python3 test_local.py`; `FG184M_DUMP=<dir>` writes the
output per case and `FG184M_ORACLE=1` prints the deviation from the double-precision
oracle.

## Remaining scope

Half, bfloat16 and quantized sources, `ncols_dst > 1`, the fusion path, the
multi-token ids path, and the whole `mul_mat_vec_q` and `mul_mat_q` families stay
native.
