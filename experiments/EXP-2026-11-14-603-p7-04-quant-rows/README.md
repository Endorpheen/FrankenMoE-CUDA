# EXP-2026-11-14-607 — P7-04 S3a: unweighted row quantizers

Hypothesis: the seven unweighted row quantizers of `ggml/src/ggml-quants.c` can
be expressed in Goose so that the produced blocks are byte-identical to the
native `ggml_quantize_chunk(..., imatrix = nullptr)` path, including f16 scales,
the min field, nibble order, the 5th-bit plane and the 2-bit packing order.

Base commit: `b41d2546` (FrankenMoE-CUDA). Native tree:
`/home/end0/AI/LlamaServer/llama.cpp-flint-cuda` (`1b44e307`).

## Scope

| slice | native function | block | bytes/block |
|---|---|---|---|
| q4_0 | `quantize_row_q4_0_ref` | 32 | 18 |
| q4_1 | `quantize_row_q4_1_ref` | 32 | 20 |
| q5_0 | `quantize_row_q5_0_ref` | 32 | 22 |
| q5_1 | `quantize_row_q5_1_ref` | 32 | 24 |
| q8_0 | `quantize_row_q8_0_ref` | 32 | 34 |
| q1_0 | `quantize_row_q1_0_ref` | 128 | 18 |
| q2_0 | `quantize_row_q2_0_ref` | 64 | 18 |

Plus the public entry: `ggml_quantize_chunk` itself, restricted to the
`imatrix == nullptr` row path, its `start` / `n_per_row` geometry rules and the
`start_row * row_size` destination offset.

## Port

`source/quant-rows.goose` (prefix `qz_`), intended as canon
`goose/model/quant-rows.goose`:

```
qz_blck(t)                       ggml_blck_size
                 qz_type_size(t)          ggml_type_size
                 qz_row_size(t, ne)       ggml_row_size
                 qz_quantize_row(t, x, y, k)     quantize_qX_0 with nullptr
                 qz_quantize_chunk(t, src, dst, start, nrows, n_per_row)
```

Return values: bytes written (native returns the same `nrows * row_size`),
`-1` for a type outside S3a, `-2` for a geometry the native version asserts on.

The port owns every decision: the scale expression and its divisor, the
first-maximum rule of q4_0/q5_0 (`strict <` keeps the sign of the first value
that reached the maximum), the min/max scan of q4_1/q5_1, the truncating
`(int8_t)`/`(uint8_t)` casts with their clamps, the `roundf` of q8_0 and q2_0,
the nibble pairing `j` / `j + qk/2`, the `qh` bit positions `j` and `j + 16`, the
q1_0 sign test `>= 0`, and the q2_0 2-bit order `j/4`, `(j%4)*2`.

Six primitives stay native, as externs the host must provide (the same style as
`goose/kv/iq4xs-encode.goose` and `goose/model/read-path.goose`):

| extern | meaning |
|---|---|
| `qz_ld_f32(u64) -> f32` | load one row element |
| `qz_ld_u8(u64) -> i64` | read back a byte already written (q1_0, q2_0) |
| `qz_st_f16(u64, f32)` | `GGML_FP32_TO_FP16` store (fp16 rounding is a hardware/runtime primitive in the reference too) |
| `qz_st_u8(u64, i64)` | store one byte |
| `qz_st_u32(u64, i64)` | store `qh` as four little-endian bytes (native `memcpy` of `uint32_t`) |
| `qz_abs(f32) -> f32` | `fabsf` |

## Gate

`tests/fixture.cpp` links the port against the real library entry:
`ggml_quantize_chunk` from `libggml-base` (`libggml-cpu` supplies
`ggml_quantize_init`). Both sides get identical source rows and a destination
buffer pre-filled with `0xA5`; the whole buffer and the returned byte count are
compared.

Row patterns (17, deterministic splitmix64, no std random): `+0`, `-0`,
`±FLT_MAX`, minimum subnormals, random `[-1,1]`, random `[-10,10]`, exact 1/8
grid, exact 0.5 ties, a block whose first maximum is positive and whose opposite
extreme is negative (the only way the reference nibble clamp is reached), a
single peak, a constant block, a ramp, a ±30-decade mix, a q8_0 tie grid with
`amax = 254` so that `x*id` lands on `k+0.5` exactly, random subnormals, a
negative-only block, and random exponents `2^-30..2^30`.

Shapes: row lengths 32, 64, 96, 128, 160, 256, 384, 512, 1024; one to four rows;
`start` of zero, one or two rows. Row lengths that are not a multiple of the
type block are refused instead of compared, because the native asserts.

Also compared: `ggml_blck_size`, `ggml_type_size`, `ggml_row_size` for every
gated type and nine row lengths; and the eleven refusals (unknown type,
non-block row length, misaligned start, start that is not a row multiple, zero
rows, zero columns, negative start, non-block `k` on the row entry).

Result: **1798 cases, 441 986 byte comparisons, 0 failures** at `-O1` and at
`-O0`.

## Mutations

`tests/mutate.py`, twelve textual mutations in two halves of six. Each mutant
passes `goose check`, is built at `-O0` and must make the gate fail.

Half 1: q4_0 divisor `-8 -> -16`; q4_0 nibble clamp `15 -> 14`; q4_0/q5_0
first maximum `< -> <=`; q4_1 divisor `15 -> 16`; q5_0 5th-bit position
`j -> j+1`; q5_0 clamp `31 -> 30`.
Half 2: q5_1 divisor `31 -> 30`; q5_1 5th-bit position `j+16 -> j`; q8_0 divisor
`127 -> 128`; q8_0 `roundf -> trunc`; q1_0 sign test `>= -> >`; q2_0 pack shift
`(j%4)*2 -> j%4`.

Result: **12/12 caught**.

## Boundaries

- K, IQ, TQ, MXFP4 and NVFP4 quantizers are S3b. The imatrix branches
  (`quant_weights != nullptr`) of q4_0/q4_1/q5_0/q5_1 are S4. This slice only
  covers `imatrix == nullptr`, which is what `ggml_quantize_chunk(..., nullptr)`
  selects.
- q1_0 is a 1-bit sign quantizer with an f16 mean-`|x|` scale, not a ternary
  type; the ternary types are TQ1_0/TQ2_0 and stay in S3b. q2_0 is the 2-bit
  type with four levels `{-d, 0, +d, +2d}`.
- The fp16 rounding itself is a native primitive (`qz_st_f16`). The port decides
  which float is converted; it does not re-implement `GGML_FP32_TO_FP16`.
- The native `(int8_t)`/`(uint8_t)` cast of a float is undefined for NaN, ±inf
  and out-of-range values. The port reproduces what the reference build emits
  (x86-64 `cvttss2si`, the indefinite `0x80000000`, low byte zero), which makes
  subnormal blocks byte-identical too, where `1/d` overflows to inf. NaN/±inf
  rows are still excluded from the corpus and no claim is made for them.
- Byte output is little-endian, as the GGUF format and the native `memcpy` of
  `qh` require.
- The port validates the geometry the native asserts on; a caller that feeds it
  invalid shapes gets `-2` instead of an abort.

## Reproduction

```
bash tests/build.sh              # gate at -O1
python3 tests/mutate.py --half 1
python3 tests/mutate.py --half 2
```

`build/` holds generated C, objects and the gate binary; it is reproducible and
must not be committed. Integration is a copy of `source/quant-rows.goose` to
`goose/model/quant-rows.goose` plus the six externs in the runtime that hosts it.
