# EXP182: CUDA Q4_K dequantization in Goose

Hypothesis: implement `dequantize_q4_K` for the F32 destination in actual Goose. No
production or compiler-source changes. Current state: byte-exact against native, but
**not exercised by the Apex profile**, so no performance claim is made and the
active runtime is unchanged.

## Scope transferred

`dequantize_q4_K<dst_t=float>` and its helper `get_scale_min_k4`, reached through
`ggml_get_to_fp32_cuda(GGML_TYPE_Q4_K)`. Goose owns the 8x4 thread decomposition
(`il = tid/8`, `ir = tid%8`, `is = 2*il`, `n = 4`), the row base, the packed half2
scale/min unpack, both 6-bit scale/min pairs from the interleaved `scales` array, the
nibble split into element `l` and `l+32`, and the affine `d*q - m` reconstruction.

Layout used: `block_q4_K` is 144 bytes — half `d` at 0, half `dmin` at 2,
`uint8 scales[12]` at 4, `uint8 qs[128]` at 16; `QK_K = 256`.

## Why Apex does not exercise it

The only caller of `ggml_get_to_fp32_cuda` on this profile is the cuBLAS
output-conversion path in `ggml-cuda.cu`, which converts the GEMM result back to F32
when the compute type differs. The Apex MoE profile dispatches MMQ, not cuBLAS, so
the dequantizer never runs. The model witness confirms it: the Apex log contains
`FG181M`, `FG180Q` and `FG179W` but no `FG182D` line.

This is recorded rather than glossed over. The transfer is still required: it is the
prerequisite for the MMQ family (P3-08/P3-09) and for the quantized `get_rows`
paths, and its correctness is established independently of the model.

## Local gate

| check | result |
|---|---|
| `gpu-fixture` Goose | 12 checks / 0 failures / 4 cases |
| `gpu-fixture` native rollback | 12 / 0 / 4 |
| byte-for-byte F32 output | 4 of 4 (1, 4, 64 and 256 blocks) |
| compute-sanitizer memcheck | 0 errors |
| export surface vs EXP181 | exactly the two diagnostics |

Cases use raw random block bytes, which exercises every scale/min bit pattern
including the interleaved high bits used for indices >= 4.

## Defects in my own work, found and fixed

1. The destination base omitted the per-block offset. Native calls
   `dequantize_q4_K(vx, i, yy + i*QK_K, tid)`, so the block offset is in floats and
   must be added inside the body; without it every block wrote block 0, which showed
   as block 0 matching and everything else differing.
2. The fixture declared a constant named `QK_K`, which is a preprocessor macro from
   `ggml-common.h`; the declaration expanded into `static const int64_t 256 = 256;`.
   Renamed the local constant.

## Apex result

One run on the fixed profile, exact response:

| metric | value |
|---|---:|
| prefill tok/s | 2580.59 |
| decode tok/s | 26.67 |
| tokens / text / top-5 | exact, sha256 `e68df05b` |

No comparison is made: the dequantizer is not on the executed path, so a
single-arm reading would be meaningless.

## Rollback and reproduce

`GGML_GOOSE_CUDA_DEQUANT_Q4K=0` restores the private-renamed native selector. The
immutable EXP181 runtime remains the previous checkpoint. Reproduce with
`python3 emit_sources.py`, `python3 build_runtime.py`, `python3 test_local.py`;
`FG182D_DUMP=<dir>` writes the F32 output per case for an external comparison.

## Remaining scope

Q4_K/F16 and Q4_K/BF16 destinations, Q5_K, Q6_K and the IQ family stay native, as do
the MMQ and MUL_MAT families themselves.
