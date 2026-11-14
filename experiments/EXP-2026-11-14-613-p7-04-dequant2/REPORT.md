# EXP-2026-11-14-616 — REPORT (P7-04 slice 5b, dequant closure)

## Verdict

ACCEPTED as a port slice: all 18 remaining dequantizers and the f16/bf16/q8_0
copy legs reproduce the native bits on every case of the gate, at `-O1` and
`-O0`, and 12/12 mutations are caught in two halves of 6.

```
FG616 cases=1391 value/byte comparisons=1131616 failures=0
```

This closes S5b for every ggml type whose unweighted quantizer is accepted in
S3a/S3b. `iq1_s`, `iq2_xxs` and `iq2_xs` remain open with the weighted
quantizers in S4: their native quantizer asserts on `imatrix`, so no valid
block corpus can be produced for them yet.

## Reproduction

```
cd experiments/EXP-2026-11-14-616-p7-04-dequant2
./tests/build.sh                      # -O1, 1391 cases, 0 failures
OPT=-O0 OUT_DIR=$PWD/build-o0 ./tests/build.sh
python3 tests/mutate.py 0 6           # 6/6
python3 tests/mutate.py 6 12          # 6/6
```

Host toolchain: goose-2026.3-string, gcc/g++-13, `llama.cpp-flint-cuda`
(`GGML_F16C=OFF`), `libggml-base` + `libggml-cpu`.

## Gate by type

Dequant cases are 18 types x {256, 512, 768, 1024} x 6 value patterns x
{1 row, 2 rows, 1 row again} = 72 per type; the four IQ families that abort
natively on the 40-decade pattern run 5 patterns = 60.

| type | cases | float values | oracle |
|---|---|---|---|
| q1_0 | 72 | 61440 | `to_float` |
| q2_0 | 72 | 61440 | `to_float` |
| q2_K | 72 | 61440 | `to_float` |
| q3_K | 72 | 61440 | `to_float` |
| q4_K | 72 | 61440 | `to_float` |
| q5_K | 72 | 61440 | `to_float` |
| q6_K | 72 | 61440 | `to_float` |
| q8_K | 72 | 61440 | `dequantize_row_q8_K` (no trait in this tree) |
| tq1_0 | 72 | 61440 | `to_float` |
| tq2_0 | 72 | 61440 | `to_float` |
| mxfp4 | 72 | 61440 | `to_float` |
| nvfp4 | 72 | 61440 | `to_float` |
| iq4_nl | 72 | 61440 | `to_float` |
| iq4_xs | 72 | 61440 | `to_float` |
| iq3_xxs | 60 | 51200 | `to_float` |
| iq3_s | 60 | 51200 | `to_float` |
| iq2_s | 60 | 51200 | `to_float` |
| iq1_m | 60 | 51200 | `to_float` |
| dequant total | 1248 | 1064960 | |
| copy quantized -> f32 (q6_K, iq4_xs, tq1_0, iq2_s) | 92 | 58880 | `to_float` per row |
| copy f16/bf16 -> f32 | 4 | 2048 | `ggml_fp16_to_fp32`, `ggml_bf16_to_fp32` |
| copy f32 -> f16/bf16 | 4 | 4096 bytes | `ggml_fp32_to_fp16`, `ggml_fp32_to_bf16` |
| copy f32 -> q8_0 | 4 | 1632 bytes | `quantize_row_q8_0` (AVX2) |
| geometry/refusal checks | 39 | | | 
| **total** | **1391** | **1131616** | |

Floats are compared bit for bit; only `-0` is folded to `+0` (Goose has no
signed zero). Bytes are compared byte for byte.

## Mutations

Rebuilt at `-O0`, the gate must fail. All 12 were caught.

| # | mutation | caught |
|---|---|---|
| 0 | q2_K reads the minimum nibble as the scale | yes |
| 1 | q4_K high nibble shifts by 3 | yes |
| 2 | q6_K q2 takes the third high bit | yes |
| 3 | q8_K treats int8 as unsigned | yes |
| 4 | tq1_0 tier decode uses 512 buckets | yes |
| 5 | mxfp4 e8m0 exponent bias off by one | yes |
| 6 | nvfp4 sub-block stride is 4 bytes | yes |
| 7 | iq4_xs scale high bits use the wrong pair | yes |
| 8 | iq3_xxs second grid takes the wrong sign bit | yes |
| 9 | iq2_s grid mask keeps 0x200 instead of 0x300 | yes |
| 10 | f16 encode clamps the subnormal bias one exponent early | yes |
| 11 | q8_0 rounding treats a tie as a round up | yes |

Two candidate mutations were replaced after measurement: `pow3[5] = 241` and
`e < 3` in the E8M0 branch were not caught because the quantizer never emits a
242 byte and because codes 2 and 3 expand to the same float; both are recorded
here as non-mutations, not as corpus gaps. `e < 1` is unreachable because the
all-zero block scale multiplies zero values.

## SHA-256

```
43a13e0bc34bca890875bed2da3b664c6866a335f23d03951e5cfff57f0eeb14  source/quant-dequant2.goose
1d1865056cf38fa075b3841091a84118faa165c970618689a51cf0d16b713ea9  tests/fixture.cpp
cc9cb8cc1b3ff5a7bc3842571b24b46554c68c5da540f39c92ce17a23d41f3ea  tests/build.sh
55815f184fe1710d2c6adec10774daf63d0000cd99b59c2118ba01cea8ec4b15  tests/mutate.py
f92ba733bd4cb106c5071bbc7c293fa4490df4676a614d540fd3172603f5cfad  tests/make-tables.py
ea4614b14b938e12a771e3986494e847bf3728ac1cc2464e663ec34d02089852  build/native-tables.c
c96710571430f36875b5fbf5d64ebfd38ed178f3dc99c166b6083ed12129afb2  build/gate.log
7827b16ee1b41db4897e7d4e8ed02cd1f27b1eca2813d2915340140e7b6c134a  build-o0/gate.log
```

## Memory and performance impact

None at runtime beyond the module: the dequantizers are table-free except for
the four IQ grids supplied by the host, and every loop writes only its own
output. The gate allocates the native block buffer plus two float buffers per
case and uses under 16 MiB peak (no change to any measured tok/s path).

## Boundaries carried into the report

- signed zero folded; NaN bit patterns match exactly;
- fp16 encode pins the software bit trick this tree builds (`GGML_F16C=OFF`);
- q8_0 from_float pins the AVX2 path (`id = 127/amax`, ties to even);
- q8_K has no `to_float` trait here and is gated through its row decoder;
- quantized -> f16/bf16 aborts natively and is not claimed;
- IQ grids are native data extracted mechanically, not hand-copied;
- `iq1_s`, `iq2_xxs`, `iq2_xs` stay open with S4.
