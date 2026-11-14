# EXP-2026-11-14-615 — Weighted (imatrix) quantizers (P7-04 slice 4)

Native counterpart: `ggml/src/ggml-quants.c` of llama.cpp-flint-cuda at
`1b44e307` (the same tree the S3b gate used). Base commit: `b68cffed`.

## What this slice covers

`source/quant-weighted.goose` ports the weighted (imatrix) arm of
`ggml_quantize_chunk` for eighteen types:

- K: q2_K, q3_K, q4_K, q5_K, q6_K — `quantize_row_q*_K_impl` with
  `quant_weights`, including `make_qkx3_quants` and `make_qp_quants`;
- q4 family: q4_0, q4_1, q5_0, q5_1 — the weighted `_impl` arms;
- IQ: iq2_xxs, iq2_xs, iq3_xxs, iq3_s, iq2_s, iq1_s, iq1_m and the shared
  `quantize_row_iq4_nl_impl` for iq4_nl and iq4_xs.

The S3b unweighted row set is carried verbatim inside the same file because
the chunk entry still needs it for `imatrix == NULL`: the weighted and
unweighted arms of each type are different native functions, and the dispatch
picks one or the other exactly as the native `quantize_*` wrappers do.

The three weight shapes are kept distinct, because they are not
interchangeable:

| family | weight handed to the search |
|---|---|
| q2_K | `qw * sqrt(sum(x*x)/QK_K + x*x)` |
| q3_K, q4_K, q5_K, iq3_xxs/s, iq4_nl/xs, iq2_s, iq1_s, iq1_m | `qw * sqrt(2*sum(x*x)/QK_K + x*x)` |
| iq2_xxs, iq2_xs | `qw * sqrt(sum(x*x)/QK_K + x*x)` |
| q6_K | `qw` straight into `make_qx_quants`, no sqrt factor |

`make_qp_quants` (the K super-scale from the group scales and mins) is a new
port: it is never called without weights, and its `slx*slx*suml2 >
sumlx*sumlx*sl2` acceptance rule and `MIN(nmax, ...)` clamps are reproduced as
written. `make_qkx3_quants` is a separate port from `make_qkx2_quants`: it
uses `max <= min` (not `max == min`), `v > 0 ? nearest_int(MIN(v,nmax)) : 0`,
and its `best_mad` is `sum(w*diff)` rather than the qkx2 error form.

The native chunk entry also passes the imatrix to tq1_0, tq2_0, mxfp4, nvfp4
and q8_0, whose quantizers ignore it; those types stay on the S3b path. q8_K
is still not a chunk type in this tree and is refused with `-3`.

## Gate

`tests/build.sh` builds the module and runs `tests/fixture.cpp`. The oracle is
`ggml_quantize_chunk(type, src, dst, start, nrows, n_per_row, imatrix)` from
`libggml-base`, with the same imatrix buffer the port receives. The whole
destination buffer is compared, including the region before `start_row`, and
the returned byte count.

The imatrix corpus is four deterministic positive patterns: constant 1.0, a
ramp (0.25 .. 8.125), a 21-step power of two (2^-10 .. 2^10), and a sparse
pattern (1.0 with 1000.0 every 97th column and 0.001 every 13th). The value
corpus is the S3b one: 17 patterns (zeros, ±max, subnormals, grids, exact
ties, spikes, 60-decade mixes, random) over block-multiple lengths
32..1024, 1-4 rows and start offsets.

```
cases 28308, byte/value comparisons 13053520, failures 0     (-O1)
cases 28308, byte/value comparisons 13053520, failures 0     (-O0)
```

The NULL arm of the eleven types that have one (q2_K..q6_K, iq4_nl, iq4_xs,
iq3_xxs, iq3_s, iq2_s, iq1_m) is compared against the same oracle with
`imatrix == nullptr` inside the same run.

## S3b NULL-arm regression

`tests/null-regress.cpp` runs the S3b corpus (16 types, 9 lengths, 17
patterns, the S3b geometry set, 5168 cases, 2468058 destination bytes) through
`qz_quantize_chunk(..., NULL)` of two separately compiled ports: this one and
`tests/s3b/quant-rows2.goose`, a verbatim snapshot of the accepted EXP611
source at `b68cffed`. Both print a SHA-256 over every destination byte and
byte count:

```
dd6a1fce1b4319618448d74ab83d986291f0040b698463d27fa0cf5720d34705  new
dd6a1fce1b4319618448d74ab83d986291f0040b698463d27fa0cf5720d34705  s3b
```

The snapshot is frozen on purpose: the live EXP611 directory is another
agent's workspace and may be mid-edit, so the gate must not read it.

## Mutations

`tests/mutate.py --half 1|2` holds 12 semantic mutations in two halves.
Every mutant passes `goose check` and must then fail the fixture (`-O0`):

```
python3 tests/mutate.py --half 1   # 6/6 caught
python3 tests/mutate.py --half 2   # 6/6 caught
```

Half 1 runs the K arms: q2_K group level count, q3_K per-element weights
replaced by the group sums, q4_K level count, q5_K scale clamp, q6_K dropping
the imatrix pointer, and the `make_qp` super-scale search step. Half 2 runs
q4_0/q4_1 level counts, the iq4_nl sigma2 factor, iq4_xs neighbour trials, the
iq2_xxs level count, and the iq1_m final super-scale weight.

## Recorded limits

- The imatrix corpus is positive and finite. Negative weights would make
  `sqrt(sigma2 + x*x)` and `sqrt(weight)` produce NaN; the native code has no
  guard for that and the port was not gated on it.
- NaN and ±inf rows are outside the corpus: the native float-to-int casts are
  undefined outside the target range. Blocks whose scale overflows to inf are
  still compared byte for byte; the port reproduces the x86 truncation.
- The IQ grid quantizers abort natively when every neighbour distance is inf:
  pm_fltmax and decade_mix for iq3_xxs, iq3_s, iq2_xxs, iq2_xs and iq1_s;
  decade_mix only for iq2_s and iq1_m. Those pairs are excluded visibly in
  `pattern_supported`, not counted as skips.
- `iq1_find_best_neighbour2` has an upstream out-of-bounds read in its
  fallback loop (`xval[i]` with the grid index as `i`). The port reproduces
  the same read shape; the gated corpus never reaches that fallback because it
  excludes the patterns that make every distance infinite.
- q4_0/q4_1/q5_0/q5_1 keep their unweighted refs in S3a (EXP603). This slice
  ports only their weighted arms, and the chunk entry answers `-4` for their
  NULL arm. IQ2_XXS/IQ2_XS/IQ1_S answer `-5` for the NULL arm because the
  native impls assert on it; `ggml_quantize_chunk` also asserts for those
  types through `ggml_quantize_requires_imatrix`.
- The fp16 rounding is the native primitive; the port owns which value is
  converted. The IQ grids, code->grid maps and neighbour lists are host
  primitives built by the native constructors (`tests/make-tables.py`), the
  same boundary S3b used.
- Mutation runs use `-O0` and the gate itself stays at `-O1`; both were run
  here and report identical counts.

## Layout

```
source/quant-weighted.goose   the port
tests/build.sh                gate build (-O1), NULL-arm regression, -O0 via OPT
tests/fixture.cpp             weighted corpus and refusals
tests/null-regress.cpp        S3b NULL-arm digest comparison
tests/make-tables.py          native IQ table extractor (from S3b)
tests/mutate.py               12 mutations in two halves
tests/s3b/quant-rows2.goose   frozen EXP611 source for the regression
results/gate-o1.txt           recorded gate output at -O1
results/gate-o0.txt           recorded gate output at -O0
```
