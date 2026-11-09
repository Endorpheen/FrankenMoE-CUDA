# EXP188: CUDA Q4_0 dequantization in Goose

Hypothesis: implement `dequantize_block_q4_0<__half>` in actual Goose. The Apex KV
cache is q4_0 on both K and V and the census lists
`dequantize_block_q4_0<__half>(const void *, T1 *, int)`, so this looked like it was
on the attention path.

Result: the transfer is **byte-exact**, but the Apex profile does not execute it, so
no performance claim is made and the active runtime is unchanged.

## Verdict

**OFF_PATH** — byte-exact Q4_0 dequantization to F16, but the profile reaches ggml_get_to_fp16_nc_cuda rather than the row selector this module replaces.

## Scope transferred

`dequantize_block_q4_0<dst_t=half>` and its helper `dequantize_q4_0`, reached through
`ggml_get_to_fp16_cuda(GGML_TYPE_Q4_0)`. Goose owns the 8-blocks-per-CTA
decomposition (`il = tid/8`, `ir = tid%8`, `ib = 8*i + ir`), the out-of-range guard,
the destination base `256*i + 32*ir + 4*il`, the nibble split, and the affine
`d*q - 8d` reconstruction.

Layout used: `block_q4_0` is 18 bytes — half `d` at 0, `uint8 qs[16]` at 2; QK4_0 = 32,
so one CTA covers 8 blocks = 256 elements.

## Local gate

| check | result |
|---|---|
| `gpu-fixture` Goose | 15 checks / 0 failures / 5 cases |
| `gpu-fixture` native rollback | 15 / 0 / 5 |
| byte-for-byte F16 output | **5 of 5** |
| compute-sanitizer memcheck | 0 errors |
| export surface vs EXP185 | exactly the two diagnostics |

A defect in my own export gate was caught by re-running it and fixed: the visibility
map still carried EXP185's `local:` substitutions, so `fg188h_*`, the privatised
native selector and the Goose device prefixes were all exported instead of hidden.
Without the fix the gate reported nine added symbols; with it, exactly the two
diagnostic readers, and the byte comparison stays at 5 of 5.

Byte equality is the right criterion: the transform is `d*(q&0xF)+dm` and
`d*(q>>4)+dm` with one rounding to half, so a tolerance would hide a wrong nibble
order or a wrong `-8d` bias. Sizes 256/512/768/1024/4096 include a tail CTA that is
not a whole 8-block window.

## Why Apex does not exercise it

The Apex run completes with identical output (`e68df05b`) but the model log contains
no `FG188D` witness in either arm, so the Goose row dequantizer never ran. The
inference path reaches `ggml_get_to_fp16_nc_cuda`, which for Q4_0 returns
`dequantize_block_cuda<QK4_0, QR4_0, dequantize_q4_0>` — a different template from the
`dequantize_row_q4_0_cuda` wrapper that `ggml_get_to_fp16_cuda` serves. The census
entry `dequantize_block_q4_0<__half>(const void *, T1 *, int)` is the row form, so it
must come from a path this profile does not take.

Recorded rather than glossed over. Reaching the NC variant needs a separate seam
(`ggml_get_to_fp16_nc_cuda` and its `to_t_nc_cuda_t` signature with per-dimension
strides), which is the natural follow-up work.

## Rollback and reproduce

`GGML_GOOSE_CUDA_DEQUANT_Q4_0=0` restores the private-renamed native selector.
Reproduce with `python3 emit_sources.py`, `python3 build_runtime.py`,
`python3 test_local.py`; `FG188D_DUMP=<dir>` writes the F16 output per case.

## Remaining scope

The NC (non-contiguous) dequantization seam, every other quantized type, and the
`mul_mat_q` family stay native.
