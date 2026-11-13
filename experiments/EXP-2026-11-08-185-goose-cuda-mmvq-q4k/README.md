# EXP185: CUDA mul_mat_vec_q for Q4_K in Goose

Hypothesis: implement `mul_mat_vec_q<Q4_K>` for the shape the Apex profile executes
(one destination column, no fusion) in actual Goose and enable it by default. This is
the largest kernel family in the Apex census and Q4_K is the model's dominant weight
type, so it sits directly on the per-token path. Verdict: **ACCEPTED**.

## Scope transferred

`mul_mat_vec_q<GGML_TYPE_Q4_K, ncols_dst=1, has_fusion=false>`, `vec_dot_q4_K_q8_1`
and `vec_dot_q4_K_q8_1_impl_vmmq`, plus the launch-geometry decisions in the host
module. Goose owns the tid decomposition (`kbx = tid/16`, `kqs = 2*(tid%16)`,
`kby = kbx*8`), the `bq8_offset` and q4 pointer derivation, the 6-bit scale/min
unpack of the interleaved `scales` array including the branchless `hi` mask form,
the nibble-shifted dp4a dot and the q8 sum, the per-i accumulation, the cross-warp
staging, and the super-block scale/min combination.

The other 17 `mul_mat_vec_q` instantiations, `ncols_dst > 1`, fusion, and
`is_multi_token_id` stay native and are refused by the parent.

## Apex pair, same DSO, only this op switched

Order native → goose → goose → native:

| arm | prefill tok/s | decode tok/s |
|---|---:|---:|
| native-rollback a | 2576.93 | 26.78 |
| goose a | 2587.18 | 26.02 |
| goose b | 2588.96 | **27.08** |
| native-rollback b | 2588.24 | 27.00 |
| native median | 2582.59 | 26.89 |
| goose median | 2588.07 | 26.55 |
| delta (first pair) | **+0.21%** | **−1.28%** |

Both inside the 2% threshold. Content sha256 identical in all four runs
(`e68df05b`) and the witness `FG185Q_CUDA_MMVQ_Q4K ... paths=0x1` appears only in the
Goose arms.

## Re-measured over seven pairs

The pair above was the first one taken. Seven paired runs now exist for this stage across
several sessions and conditions, and the median is what should be quoted:

| statistic | goose | native | delta |
|---|---:|---:|---:|
| decode median (n=7) | 26.37 | 26.83 | **−1.70%** |
| decode range | 25.80–27.08 | 26.53–27.40 | |
| prefill median (n=7) | 2585.84 | 2587.64 | −0.07% |

Both are still inside the 2% threshold, so the verdict does not change, but the honest
figure is −1.70% decode, not the −0.45%..−1.28% that the early pairs suggested. Reporting a
single pair was optimistic by roughly a percentage point, and the spread within each arm
(1.3 tok/s for goose, 0.9 for native) is large enough that a two-run comparison can land
anywhere in that band.

The spread correlates with host load and CPU clock during the run, not with the library:
every run used the identical `libggml-cuda-mmvq4k-fa8dc98a39ae`, verified by hash in each
run's `loaded-library-hashes.json`. So the number to carry forward is the median, and any
future comparison should use at least four pairs under comparable load.

## Local gate

| check | result |
|---|---|
| `gpu-fixture` Goose | 8 checks / 0 failures / 4 cases |
| `gpu-fixture` native rollback | 8 / 0 / 4 |
| byte-for-byte output | **4 of 4** |
| compute-sanitizer memcheck | 0 errors |
| export surface vs EXP184 | exactly the two diagnostics |

Cases cover ncols 256, 1024, 4096 and a two-channel 2048. A single-block `probe`
target compares the two arms on exactly one 256-wide block, so a defect in the
scale/min unpack can be localised without any launch-geometry noise.

## Defects in my own work, found and fixed

1. **`sc` and `m` alias different words of `aux`.** In native, `sc` is
   `(uint8_t*)aux` and `m` is `sc + 2`, so *both* scales are the low bytes of
   `aux[0]` and *both* mins are the low bytes of `aux[1]`. I had taken the low byte of
   each word as a scale and the high byte as a min. This was the first real numeric
   error and the probe reproduced it immediately.
2. **`dmin` read at the wrong offset.** `ld_h2b` already selects the high half of the
   `half2` at the given address, so passing `xb+2` read the element at `xb+4`.
3. **Warp reduction order.** Warps with `ty>0` must publish their *raw* partial into
   slot `ty-1` and only warp 0 may reduce; I reduced first, which published the warp
   total four times over. The `__syncthreads()` also has to sit outside the `ty>0`
   branch.
4. **`channel_y` without ids.** Native takes it straight from `blockIdx.y` and only
   applies the modulo in the ids branch; I applied the modulo unconditionally.
5. **`src1` is F32 on this ABI.** `ggml_cuda_mul_mat_vec_q` quantizes it to q8_1 into
   a padded staging buffer itself. The fixture was feeding pre-quantized bytes through
   a tensor declared F32, which also tripped a native assert.
6. **The staging stride is the padded one.** `stride_col_y` is
   `PAD(ncols, MATRIX_ROW_PADDING)/QK8_1` and the y channel stride is
   `s12 = ne11*s11` **in q8_1 blocks**, not in elements.
7. **`ybase` was a block index added to a byte offset.** This was the last and
   most stubborn defect: `ybase` counts `block_q8_1` units, so it has to be scaled by
   the block size together with `kby` and `bq8`. Adding it unscaled made channel 1
   read byte 64 of block 0 instead of block 64, which produced NaN. It only showed up
   in the multi-channel case, which is why the single-channel cases were already
   passing.

A separate procedural lesson: the first several fixes appeared to have no effect
because `build_runtime.py` reused a stale `cuda-mmvq-q4k.o`. Removing the object file
before rebuilding made the edits take effect. The symptom was that changing a `printf`
inside the compiled primitive changed nothing at all.

## Rollback, reproduce, remaining scope

`GGML_GOOSE_CUDA_MMVQ_Q4K=0` restores the private-renamed native
`ggml_cuda_mul_mat_vec_q`; any type or shape the Goose body does not cover also falls
back inside the candidate. Reproduce with `python3 emit_sources.py`,
`python3 build_runtime.py`, `python3 test_local.py`.

Still native: the other 17 quantized types for this kernel, `ncols_dst > 1`, the
fusion path, the multi-token ids path, the `small_k` and `halve_iters` variants, and
the whole `mul_mat_q` (MMQ) family.
