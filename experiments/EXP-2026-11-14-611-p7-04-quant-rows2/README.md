# EXP-2026-11-14-611 — K/IQ/MX/NV/TQ quantizers (P7-04 slice 3b)

Native counterpart: `ggml/src/ggml-quants.c`, the remaining unweighted row
quantizers. Base commit: `d43a48d5`.

## What this slice covers

`source/quant-rows2.goose` ports the block image for sixteen types:

- K: q2_K, q3_K, q4_K, q5_K, q6_K and the q8_K row quantizer;
- TQ: tq1_0, tq2_0;
- MX/NV: mxfp4, nvfp4;
- IQ: iq4_nl, iq4_xs, iq3_xxs, iq3_s, iq2_s, iq1_m.

The table-driven IQ grids are built by the native constructors from
`ggml-quants.c` (extracted into `tests/make-tables.py` output and linked into
the gate), so the port carries the packing logic, not a hand-copied table.

## Gate

`tests/build.sh` builds the module and runs `tests/fixture.cpp`; the oracle is
`ggml_quantize_chunk(type, src, dst, start, nrows, n_per_row, nullptr)` from
`libggml-base` (and `quantize_row_q8_K_ref` for q8_K). The whole destination
buffer is compared, including the region before `start_row`.

```
cases 3764, byte/value comparisons 1897854, failures 0
```

The same numbers hold at `-O1` and `-O0`. The corpus is 16 types x block-multiple
lengths {32..1024} x 17 value patterns (zeros, ±max, subnormals, grids, exact
ties, spikes, 60-decade mixes, random) x 1-4 rows x start offsets.

Two IQ families abort natively on specific extreme patterns (IQ3 on the two
huge-value patterns, IQ2_S/IQ1_M on the 60-decade mix); those pairs are excluded
from the corpus because the native call does not return, and the exclusion is
visible in `pattern_supported`.

## Mutations

`tests/mutate.py --half 1|2` holds 12 semantic mutations in two halves:

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

## Recorded limits

- The fp16 rounding is the native primitive; the port owns which value is
  converted.
- NaN/±inf inputs are outside the corpus (native float-to-int casts are UB).
- The imatrix branches are S4; dequant closure is S5b; q1_0/q2_0/q4_0..q8_0 are
  S3a (EXP603).
- An earlier draft run of this fixture reported 5292 cases; the final corpus is
  3764 because the excluded abort pairs were narrowed honestly rather than
  counted as skips.
