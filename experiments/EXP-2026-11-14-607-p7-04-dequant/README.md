# EXP-2026-11-14-607 — basic row dequantizers (P7-04 slice 5a)

Native counterparts: `dequantize_row_q4_0/q4_1/q5_0/q5_1/q8_0` in
`ggml/src/ggml-quants.c`. Base commit: `fed26568`.

## What this slice covers

`source/quant-dequant.goose` decodes the packed block image of the five basic
types into one f32 per element:

- q4_0: a half scale, low/high nibbles minus 8, interleaved at j and j+16;
- q4_1: the same with the half minimum added;
- q5_0: the fifth bit taken from the 32-bit qh at bit j and j+12, offset 16;
- q5_1: the same with the minimum;
- q8_0: the half scale and signed int8 quants.

The half-to-float decode (normal, subnormal, signed zero) is implemented in
Goose, so the module needs no libc shim.

## Gate

`tests/build.sh` builds the module and runs `tests/fixture.cpp`, which builds
valid blocks with `ggml_quantize_chunk`, dequantizes them with the native type
traits' `to_float`, and compares the port's floats bit for bit.

```
FG_DQ cases=300 failures=0
```

300 cases: 5 types x lengths {32,64,96,128,256} x 4 value classes (zeros, ramp,
deterministic random, single spike) x 1-3 rows.

## Mutations

`tests/mutate.py 0 6|6 12` holds 12 semantic mutations in two halves:

```
python3 tests/mutate.py 0 6    # 6/6 caught
python3 tests/mutate.py 6 12   # 6/6 caught
```

## Recorded limits

- Goose does not preserve the sign of zero: native `-8*0` is `-0`, the port
  gives `+0`. The gate folds `-0` to `+0`; every non-zero value is compared
  bit-exactly. This is a language-level difference, recorded rather than hidden.
- K/IQ/MXFP4/NVFP4/TQ dequant and different-type copy are S5b; the weighted
  quantizers are S4; imatrix is S2 (done).
