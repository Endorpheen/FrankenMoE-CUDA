# EXP190: CPU Q8_K activation quantization in Goose

Hypothesis: the CPU MoE expert path contains work that is genuinely scalar, so it can be
transferred to Goose without needing SIMD support the language does not have.

That hypothesis was tested before any porting, and it holds for this function:

```
objdump -d build-cuda/bin/libggml-cpu.so | awk '/<quantize_row_q8_K>:/,/^$/' | grep -c 'ymm|zmm'
0
```

Zero SIMD instructions. The adjacent dot products do not pass this test —
`ggml_vec_dot_q4_K_q8_K` has 58 and `ggml_vec_dot_q6_K_q8_K` has 91 — which is why they
stay native: Goose has no SIMD, and the compiler documentation places SIMD outside the
current scope. `quantize_row_q8_K` is also registered in the CPU type table at
`ggml-cpu.c:393` (`.from_float`), so it is on the executed path with `--cpu-moe`.

## Scope transferred

`quantize_row_q8_K_ref` in full: the per-super-block abs-max scan, the all-zero early
out, the `-127/max` inverse scale, the rounding and clamp to int8, the sixteen per-group
sums, and the `1/iscale` delta. Everything is Goose; the externs are only a float load,
an int8/int16 store, an int8 load, and the rounding and abs primitives.

## Gate

| check | result |
|---|---|
| byte-for-byte `block_q8_K` | **8 checks, 0 failures** |
| sizes | 256, 512, 2560, 4096 elements |
| zero super-block | included, exercises the early-out path |

The comparison is against `quantize_row_q8_K_ref` from the real `libggml-base.so`, not a
hand-written reference, so the whole block struct — `d`, the 256 int8 quants and the 16
int16 group sums — must match exactly.

## Two defects in my own work, found by measurement

1. **Wrong float stride.** I wrote `src + so + j`, but `x` is a float array so element
   `j` is at byte `4*j`. The symptom was telling: `qs` came out as
   `-127 0 0 0 -85 0` — every fourth byte. Fixed.
2. **`nearest_int` is not `roundf`.** Native adds `12582912.f` (1.5*2^23) and masks the
   mantissa, which is round-half-to-even through the FP32 representation. `roundf` is
   round-half-away-from-zero and diverges on exact `.5`. Replicated exactly.

The fixture caught the second only because the byte comparison covers the quant bytes
and not just the block delta.

## What this does and does not claim

This is a real transfer of executed CPU code, verified byte-exactly. It makes no
performance claim: the function is a small part of CPU MoE node time next to the AVX2
dot products, and no Apex pair was run for it. The dominant CPU cost —
38,226 Q4_K dot calls at 176.79 us mean CPU-node time per EXP133 — is behind the SIMD
barrier and remains native.

## Remaining CPU scope

The quantized dot-product families (`q4_K`, `q6_K` and the rest), which are SIMD on this
host, and the non-OpenMP expert work distribution. Both need either SIMD in Goose or a
different execution strategy, and neither is available from this side.
