# EXP-2026-11-14-616 — dequant closure (P7-04 slice 5b)

Native counterparts: the remaining `dequantize_row_*` functions in
`ggml/src/ggml-quants.c` and the different-type copy paths of
`ggml/src/ggml-cpu/ops.cpp`. Base commit: `b68cffed`.

## What this slice covers

`source/quant-dequant2.goose` decodes the packed block image into one f32 per
element for the types left open after S5a:

- K: q2_K, q3_K, q4_K, q5_K, q6_K, plus the q8_K row decoder;
- TQ: tq1_0 (base-3 planes, 48-byte `qs` + 4-byte `qh`), tq2_0;
- MX/NV: mxfp4 (E8M0 block scale), nvfp4 (four UE4M3 sub-block scales);
- IQ: iq4_nl, iq4_xs, iq3_xxs, iq3_s, iq2_s, iq1_m;
- and the two S3a types whose dequant was still open: q1_0, q2_0.

That closes every ggml type whose unweighted row quantizer is accepted in S3a
or S3b. The five types of S5a (q4_0, q4_1, q5_0, q5_1, q8_0) stay in
`experiments/EXP-2026-11-14-607-p7-04-dequant/`; a runtime calls that module
first and this one for the rest. The module prefix is `qd_`.

The different-type copy is the ggml-cpu sequence: `to_float` for a quantized
source (`ggml_compute_forward_dup_from_q`), the f16/bf16 primitives of
`ggml_compute_forward_dup_flt`, and `from_float` for a quantized destination
(`ggml_compute_forward_dup_to_q`). The port exposes:

- `qd_copy_to_f32(type, src, n, out)` — quantized or f16/bf16 -> f32;
- `qd_copy_from_f32(type, src, n, dst)` — f32 -> f16, bf16 or q8_0.

The f16/bf16 encoders copy the native rounding: the fp32-to-fp16 bit trick of
`ggml_compute_fp32_to_fp16` (round to nearest even, subnormals, overflow to
infinity, NaN to 0x7E00) and the round-to-nearest-even of
`ggml_compute_fp32_to_bf16`.

## Gate

`tests/build.sh` builds the module and runs `tests/fixture.cpp`, which:

1. builds valid blocks with `ggml_quantize_chunk(type, src, dst, 0, nrows, n,
   nullptr)` from `libggml-base` (q8_K has no chunk case in this tree, so that
   type uses `quantize_row_q8_K_ref` directly);
2. dequantizes with `ggml_get_type_traits(type)->to_float` — the function that
   `dup_from_q` calls (q8_K registers no `to_float` trait here, so the native
   `dequantize_row_q8_K` is called directly);
3. runs `qd_dequant` on the same bytes and compares every float bit for bit.

Corpus: 18 types x lengths {256, 512, 768, 1024} x 6 value patterns (zeros,
ramp, deterministic random, single spike, 40-decade mix, fp16-subnormal
values) x 1-2 rows. The three IQ families that abort natively on the 40-decade
pattern are excluded from that one pattern and the exclusion is visible in
`pattern_supported`, not counted as a skip.

```
FG616 cases=1391 value/byte comparisons=1131616 failures=0
```

The same numbers hold at `-O1` and `-O0`.

Copy gate, compared against the native functions the dup switch calls:

| pair | native oracle | cases |
|---|---|---|
| q6_K, iq4_xs, tq1_0, iq2_s -> f32 | `to_float` per row, as `dup_from_q` | 92 |
| f16 -> f32 | `ggml_fp16_to_fp32` per element | 2 |
| bf16 -> f32 | `ggml_bf16_to_fp32` per element | 2 |
| f32 -> f16 | `ggml_fp32_to_fp16` per element (bytes) | 2 |
| f32 -> bf16 | `ggml_fp32_to_bf16` per element (bytes) | 2 |
| f32 -> q8_0 | `quantize_row_q8_0` from `libggml-cpu` (bytes) | 4 |

The f32 -> f16/bf16 corpus includes exact f16 ties, subnormal f16 values, the
exponent band where the native bias clamp applies, 65504, 65520 (rounds to
infinity) and 1e38. The q8_0 corpus includes a block with `amax == 127`, so
`id` is exactly 1.0 and the 0.5 grid hits the round-to-nearest-even ties.

## Mutations

`tests/mutate.py 0 6` and `tests/mutate.py 6 12` hold 12 semantic mutations in
two halves; each is rebuilt at `-O0` and the gate must fail.

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

## Recorded limits

- Goose does not carry the sign of zero: native `-8 * 0` is `-0` in places, the
  port gives `+0`. The gate folds `-0` to `+0`; every non-zero value is
  compared bit-exactly. NaN bit patterns produced by an overflowing block
  scale are compared exactly and do match.
- The fp16 encode is the native software bit trick, which is what this tree
  builds (`GGML_F16C=OFF`). A hardware `vcvtps2ph` path rounds identically but
  may differ in NaN payloads.
- The q8_0 from_float follows the AVX2 path that `libggml-cpu` actually
  dispatches here: `id = 127/amax` and round-to-nearest-even. The scalar
  reference uses `1/d` and `roundf` (ties away from zero) and would differ.
- q8_K registers no `to_float` trait in this tree; it is gated through
  `dequantize_row_q8_K`. The bytes are still produced by the native quantizer.
- iq1_s, iq2_xxs and iq2_xs have no unweighted native quantizer (they assert on
  `imatrix`), so no valid-block corpus exists for them; their dequantizers stay
  open with the weighted quantizers in S4.
- The copy gate compares the row primitives, not a full `GGML_OP_DUP` graph
  execution: the switch only chooses between exactly these functions and adds
  the row offsets.
- Only the f32 destination of a quantized source is implemented natively;
  quantized -> f16/bf16 aborts with `GGML_ABORT("fatal error")` in `dup`, so no
  such pair is claimed.
- Host primitives: float bit reinterpretation (`qd_bits`/`qd_of_bits`) and the
  four IQ grid tables (`iq3xxs_grid`, `iq3s_grid`, `iq2s_grid`, `iq1s_grid`),
  extracted from the native `ggml-common.h` by `tests/make-tables.py`. The
  port owns the decode; it does not hand-copy grid data.

## Files

```
source/quant-dequant2.goose   the port
tests/build.sh                build and run (-O1 default, OPT=-O0 supported)
tests/fixture.cpp             differential gate
tests/make-tables.py          IQ grid extraction
tests/mutate.py               12 mutations, two halves
build/native-tables.c         generated grid tables (not committed upstream)
```
