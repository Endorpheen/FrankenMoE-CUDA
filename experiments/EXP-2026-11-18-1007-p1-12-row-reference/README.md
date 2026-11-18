# EXP-2026-11-18-1007 — P1-12: the delivered row object does not implement the kernel the Apex runtime executes

Base: `cad70a30`. No production change, no model run, no GPU. Measurement and verification only.

## 1. Question

The compiler side delivered row-level Goose entries for multi-block Q4_K and Q6_K
(`Fried-Goose/research/p1-12-simd/ROWS.md`, `ROW_ACCEPTANCE.json`):

```
fg_cpu_simd_row_q4_k_q8_k(fg_cpu_simd_bytes a, fg_cpu_simd_bytes b, int32_t nb, float * value);
fg_cpu_simd_row_q6_k_q8_k(fg_cpu_simd_bytes a, fg_cpu_simd_bytes b, int32_t nb, float * value);
```

The contract takes contiguous raw quantization blocks: `144*nb` bytes of weights,
`292*nb` bytes of activations, and it reproduces one of two named FP orders
(`--row-order generic` or `--row-order avx2`). P1-12 needs these entries on the
path that the Apex runtime actually executes. The question is therefore not
whether the object is internally correct (it is), but which native kernel the
live runtime calls and whether either delivered order reproduces it.

## 2. What the live runtime executes

Chain, from the shipped `libggml-cpu.so.0.27.0` (`sha256 63ad4984...`) and the sources it was built from:

1. `ggml_compute_forward` / `GGML_OP_MUL_MAT_ID` enters `fg143_try`, then
   `fg140_dispatch`, then `fg140_previous_mul_mat_id`
   (`ggml_compute_forward.part.0+0x2b851/0x2b867/0x2b881` in the binary).
2. `fg143_try` declines every quantized `MUL_MAT_ID`: `entry.goose:42-45`
   (`!scalar_type(p.x) || !scalar_type(p.d)`) and `matrix.goose:65-77`
   (`p.x.kind` restricted to F32/F16/BF16). The "FG143_CPU op=100" witness in
   the ledger is `GGML_OP_GLU`, not the expert matmul; `op=40` is `GET_ROWS`.
3. The loader places the Q4_K expert tensors in `CPU_REPACK`:
   `weight_buft_supported` / `select_weight_buft`
   (`runtime/reconstructed/src/llama-model-loader.cpp:1129-1149`) probes the
   candidate list, and `ggml_backend_dev_supports_op` answers true for
   `GGML_OP_MUL_MAT_ID` with a 3-D Q4_K tensor; the Goose placement rule FG404
   is the native one, first supported candidate wins
   (`goose/model/goose-weight-placement.goose`).
4. Repack's `forward_mul_mat_id` executes **one row at a time** and calls
   `gemv<block_q4_K, 8, 8, GGML_TYPE_Q8_K>`
   (`ggml/src/ggml-cpu/repack.cpp`, the `gemv<BLOC_TYPE, INTER_SIZE, NB_COLS,
   PARAM_TYPE>(...)` call in `forward_mul_mat_id`), i.e.
   `ggml_gemv_q4_K_8x8_q8_K` (`arch/x86/repack.cpp:1464`) over the interleaved
   `block_q4_Kx8` layout. It never calls `ggml_vec_dot_q4_K_q8_K`.

Independent runtime corroboration: EXP411 instrumented all quantized `vec_dot`
entries on the accepted profile and measured `q4_K=0 q6_K=0 q8_0=0 q2_K=0 q3_K=0 q5_K=0`
(`experiments/EXP-2026-11-11-411-goose-cpu-q4k-dot-avx2/README.md`). The accepted
reference was produced without the `vec_dot` kernels.

Q6_K has no x86 repack branch (`repack.cpp:5061-5072`, NEON only), so its expert
matmul does not go through repack; the tiled path (`EXP141`) is the configured
route there.

## 3. Measurement

`tests/gate_rows.cpp` compares three things on identical data, all from the
shipped library, no re-implementation:

- `ggml_vec_dot_q4_K_q8_K` / `ggml_vec_dot_q6_K_q8_K` — the dispatched kernels;
- `ggml_gemv_q4_K_8x8_q8_K` — the kernel the live repack path calls, fed with
  weights repacked by the library's own `CPU_REPACK` buffer;
- the delivered row object, both delivered orders.

Weights and activations are quantized with the library's own
`quantize_row_q4_K` / `quantize_row_q6_K` / `quantize_row_q8_K`.

| size (nb) | Q4_K gemv vs delivered avx2 | Q4_K gemv vs delivered generic | delivered avx2 vs vec_dot | Q6_K avx2 vs vec_dot |
|---|---:|---:|---:|---:|
| n=512 (nb=2)   | 188 / 256 differ | 217 / 256 differ | 0 / 256 differ | 0 / 256 differ |
| n=2048 (nb=8)  | 207 / 256 differ | 217 / 256 differ | 0 / 256 differ | 0 / 256 differ |
| n=8192 (nb=32) | 227 / 256 differ | 223 / 256 differ | 0 / 256 differ | 0 / 256 differ |

Difference between the live gemv and either delivered order: up to
`1.196e-04` relative and 1536 ULP at n=2048, `2.624e-04` and 3584 ULP at n=8192.
The delivered object is bit-exact with the dispatched `vec_dot` (256/256 for
both types at every size) and with neither the generic order nor the live gemv.

## 4. Conclusion

The delivered row object is correct against the `vec_dot` reference and is
irrelevant to the live Q4_K path:

- the live Q4_K expert dot is `ggml_gemv_q4_K_8x8_q8_K` over `block_q4_Kx8`;
- the delivered ABI accepts only raw `block_q4_K` rows, so it cannot even read
  the buffer the runtime holds;
- its association reproduces the AVX2 `vec_dot`, which differs from the live
  gemv in 74-89% of the measured calls at every Apex row length.

Wiring it in therefore changes the arithmetic of the dominant CPU-MoE operation
and cannot reproduce the accepted reference output. The same is true in the
other direction: dropping `CPU_REPACK` so the row object becomes reachable also
changes the executed kernel and the output bits.

This is a numerical-contract and ABI gap in the delivery, not a compiler
capability gap and not a defect in the delivered code.

## 5. What the delivery must add to close P1-12

One entry point that reproduces the association of the kernel the runtime
executes, over the layout the runtime holds:

```c
// 8 interleaved columns per super-block, exactly block_q4_Kx8
//   d[8] (16) + dmin[8] (16) + scales[96] + qs[1024] = 1152 bytes
int32_t fg_cpu_simd_row_q4_k_q8_k_x8(
        fg_cpu_simd_bytes weights,   // nb * 1152 bytes, block_q4_Kx8[nb]
        fg_cpu_simd_bytes act,       // nb * 292 bytes,  block_q8_K[nb]
        int32_t nb, int32_t nc,      // nc multiple of 8
        float * out);                // nc floats, one integer accumulator and one FMA per output
```

Numerical contract: bit-identical to `ggml_gemv_q4_K_8x8_q8_K` at `nr = 1`, one
FP32 accumulator per output column, the same `d`, `dmin` and bsums handling and
the same final reduction. The two existing row orders do not satisfy it.

Until such an entry exists, P1-12's Q4_K part cannot become Goose without
changing the accepted output, and that decision belongs to Igor.

## 6. Reproduce

```
cd experiments/EXP-2026-11-18-1007-p1-12-row-reference/tests
D=/home/end0/AI/LlamaServer/franken-goose-exp150-restored-20261108/runtime/reconstructed/build/stage-run
R=/home/end0/AI/Fried-Goose/build/qualified/cpu-simd-rows-8b637cdc7a8c/cpu/avx2/blocks.o
g++-13 -O2 -std=c++17 -w \
  -I/home/end0/AI/FrankenMoE-CUDA/vendor/flint/ggml/include \
  -I/home/end0/AI/FrankenMoE-CUDA/vendor/flint/ggml/src \
  gate_rows.cpp "$R" -L"$D" -lggml-cpu -lggml -lggml-base -Wl,-rpath,"$D" -lm -pthread -o gate_rows
./gate_rows 2048 32          # differential gate
./gate_rows decide           # loader decision for a Q4_K expert tensor
```

`gate_rows decide` prints `supports_op(repack, q4_K gate/up expert) = true`; the
q6_K case aborts in the shipped Goose guard `fg156` on `ggml_new_tensor_3d` in a
synthetic context, which is a limitation of the harness, not a result.

## 7. Not done

No production file was modified, the reference binary was not touched, no model
or GPU run was made: the blocker is decided before any run can change it.
