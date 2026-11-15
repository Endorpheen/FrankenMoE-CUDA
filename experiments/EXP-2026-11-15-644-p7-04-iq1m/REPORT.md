# EXP-2026-11-15-642 — the iq1_m split search picks the wrong level set (P7-04)

Date: 2026-11-15
Base commit: `1de2effc`
Experiment root: `/tmp/opencode/sub-645/experiments/EXP-2026-11-15-642-p7-04-iq1m`
Native oracle: `../../tree/llama.cpp` (CPU-only, same commit)

## 1. Verdict

ACCEPTED as a fix for the second `iq1_m` defect: the port's exhaustive split
search evaluates the four level combinations from prefix sums, while the native
code accumulates them in a different order. The two are mathematically
equivalent but not bit-identical, and when two combinations tie (which happens
when every element of a 16-value group falls into one split part) the chosen
`k` differs — and `k` decides how the levels are read back, so the block bytes
differ.

| state | gate result |
|---|---|
| canon `1de2effc` + accepted EXP641 qgrid patch | 12 of 24 cases differ |
| the same + `patches/canon-iq1m-split.diff` | **24/24 cases, 0 mismatches** |
| mutations of the fixed search | **6/6 + 6/6** |

The fix replaces the prefix-sum formulation with a literal transcription of the
native accumulation. No constants, no thresholds, no rounding modes change.

## 2. Where the two implementations part

Native, `ggml-quants.c:4692` `quantize_row_iq1_m_impl`:

```
4767   qsort(pairs, block_size, 2*sizeof(float), iq1_sort_helper);
4772   for (int i1 = 0; i1 <= block_size; ++i1)
4774       for (int i2 = i1; i2 <= block_size; ++i2) {
4776           memset(sumqx, 0, 4*sizeof(float)); memset(sumq2, 0, 4*sizeof(float));
4777           for (int j = 0; j < i1; ...)      // part 0, updates all four k
         ...
4786               sumqx[0] += weight[i]*x_p[0]*xb[i];   // one product at a time,
         ...                                          // in ascending j
4845           if (sumq2[k] > 0 && sumqx[k]*sumqx[k] > best_score*sumq2[k]) {
4846               scale = sumqx[k]/sumq2[k]; best_score = scale*sumqx[k];
```

The port, `goose/model/quant-rows2.goose:2415` `qz2_row_iq1_m`, precomputed
prefix sums and then combined them algebraically:

```
2482   sort16(pairs, 16);
2488   st_f32(px, 0 as f32);  ...            // two prefix arrays,
2512   let p2 = 1.125; ...                    // px[h][j] = sum_{t<j} w*x
2530   let qxp = tx * p2 - sx;                // then qx = x_p[2]*Sx - Sx1 - Sx2
2531   ...
2563   best_score = scale * sumqx;
```

`x_p = (-0.875, 0.125, 1.125)` and `x_m = (-1.125, -0.125, 0.875)` are both
unit-step sequences, so the algebraic rewrite is exact in real arithmetic:
`qx = x_2*Sx - Sx1 - Sx2` and `q2 = W*x_2² + W1*(x_0²-x_1²) + W2*(x_1²-x_2²)`.
It is not exact in binary32, and the difference is enough to flip ties.

### 2.1 Why a tie decides the output

When `(i1, i2) = (0, 0)` or `(16, 16)`, every element of the group lands in
one part, so all four combinations collapse pairwise:

```
score(k=0) = (1.125 Sx)² / (1.265625 W) = Sx² / W
score(k=3) = (0.875 Sx)² / (0.765625 W) = Sx² / W
```

Equal in exact arithmetic. The native accumulation and the prefix form differ
in the last bits, and one of them lands `k=3` while the other lands `k=0`.
`k` then selects `x_p` or `x_m` when the levels are read back, so the whole
block changes: the `qs` bytes, the `qh` shift bits and the block scale.

Measured, first block of the ramp pattern (`results/trace-decisions.txt`),
columns `besti1*1000+besti2`, `bestk`, `scale` bits:

```
native  100 0        200 0   300 3223700853
port    100 0        200 0   300 3223700853      ib=0 agrees
native  101 16      201 3   301 1101953639
port    101 16      201 0   301 3249437327      ib=1: k 3 vs 0 (sign-flipped tie)
native  102 0       202 3   302 3225005201
port    102 0       202 0   302 3222301125      ib=2: different tie, different scale
native  103 16      203 3   303 1100379138
port    103 16      203 3   303 1100379146      ib=3: same k, scale differs by 8 ulp
```

The fixed module reproduces all twelve native decisions bit for bit.

## 3. The fix

`patches/canon-iq1m-split.diff` (rows2 and weighted together):

* removes the prefix arrays `px`/`pw`, their constant block
  (`p2, m2, cp1, cp2, cm1, cm2`) and the four algebraic combinations;
* puts in their place the native three-part loop over `j` that updates the four
  `sumqx[k]`/`sumq2[k]` accumulators in ascending `j`, with the same
   `weight[i]*x_p[c]*xb[i]` product order and the same `h = (i < 8)` split;
* leaves the selection rule, the `L` assignment, the `all_on_grid` refinement,
  the final `d` and the `1.1125` fudge factor untouched.

The block is O(block³) = 4096 evaluations per group, exactly like the native
code; `block_size` is 16, so nothing measurable changes.

## 4. Gate

`tests/fixture.cpp`: twelve patterns × two lengths = 24 `iq1_m` cases, each
quantized by the module and by the native library and compared byte for byte.

```
results/gate-before.txt
-- before (canon + EXP641 qgrid), iq1_m corpus
FAIL iq1_m pat=0 rows=1 byte=8 native=00 port=05
...
EXP642 type=iq1_m cases=24 bytes=4032 mismatches=12 offgrid_lookups=166 nb_calls=12835

results/gate-after.txt
-- fixed (canon + EXP641 qgrid + iq1m split), iq1_m corpus
EXP642 type=iq1_m cases=24 bytes=4032 mismatches=0 offgrid_lookups=166 nb_calls=12835
```

The corpus is deliberately not off-grid-driven: `offgrid_lookups=166` against
12,835 neighbour reads shows the defect fires on ordinary, mostly on-grid input
— which is why the accepted EXP611/616 gates, whose corpora were on-grid by
construction, passed.

## 5. Mutations

Twelve one-site edits of the fixed search, Goose object at `-O0`:

| id | mechanism | result |
|---|---|---|
| A1 | h=0 branch: group 1 uses the negative level set | caught (22) |
| A2 | h=1 branch: group 2 swaps the level sign | caught (22) |
| A3 | half-block boundary 8 → 7 | caught (14) |
| A4 | group 2 level constants swapped | caught (16) |
| A5 | squared level becomes linear in one sum | caught (24) |
| A6 | middle part starts at 0 instead of i1 | caught (16) |
| B1 | scale estimate halved | caught (18) |
| B2 | score comparison scaled by 0.5 | caught (18) |
| B3 | best score built from the wrong factor | caught (18) |
| B4 | shift mask 0x80 → 0x40 | caught (6) |
| B5 | final fudge factor 1.1125 → 1.125 | caught (24) |
| B6 | super-scale denominator 15 → 16 | caught (12) |

```
results/mutations.txt
half A: caught 6/6
half B: caught 6/6
after restore: EXP642 type=iq1_m cases=24 bytes=4032 mismatches=0
```

## 6. Patches

Two forms, both verified to reproduce `results/work/quant-rows2.fixed.goose`
and `quant-weighted.fixed.goose` exactly (SHA-256 equal):

| file | applies to | contents |
|---|---|---|
| `patches/canon-full-on-1de2effc.diff` | canon `1de2effc` | EXP641 qgrid + this fix, both files |
| `patches/canon-iq1m-split.diff` | canon + EXP641 qgrid | this fix only, both files |

**Note on the base.** The brief says the canon already carries EXP641. It does
not: at `1de2effc` `goose/model/quant-rows2.goose` and `quant-weighted.goose`
still contain zero `qgrid` references — EXP641 was committed as an experiment
with `patches/canon-iq-qgrid.diff`, and that patch was never applied to
`goose/model`. This experiment therefore takes "canon + EXP641 canon patch" as
its `before` state (EXP641 was accepted and is a prerequisite: on the raw canon
`iq1_m` cannot be compared meaningfully because `iq2_s` crashes the same gate).
The full patch above is provided so the lead can apply both at once.

Affected canon files:

```
goose/model/quant-rows2.goose
goose/model/quant-weighted.goose
```

`quant-weighted.goose` receives the same fix for consistency (`qz_row_iq1_m`
has the same prefix-sum search); it is not gated here — see §7.

## 7. Boundaries

1. **Weighted (imatrix) arms.** `qz_row_iq1_m` in `quant-weighted.goose` is
   patched but not gated: this experiment has no imatrix corpus. The unweighted
   path of the same function is what the gate covers.
2. **fp16.** The block scale is still stored through `ggml_fp32_to_fp16`; the
   `1.1125` fudge factor and the `max_scale/15` super-scale are unchanged, and
   mutation B5/B6 confirm the gate notices if either moves.
3. **The other IQ types** (`iq2_s`, `iq3_xxs`, `iq2_xxs`, `iq2_xs`, `iq1_s`,
   `iq3_s`) are untouched here; their off-grid defect was closed by EXP641.
4. **`iq1_m` neighbour search** (`iq1_best_nb2`) is unchanged: with the split
   decisions now matching native, the neighbour path is entered on the same
   inputs as native and produces the same bytes on this corpus (12,835
   neighbour reads exercised).
5. **No model, no server, no GPU.** Only the row quantizer entry point is
   compared.

## 8. SHA-256

```
58fb646f889567eaa0515138d9ff89c5f1c86ed878798173e3c02355e2940e85  source/quant-rows2.goose   (canon + EXP641 qgrid)
112ee8e81ad8d46a58da712bbebacbdc284c5958d71bd622245b277ab1daf431  patches/canon-iq1m-split.diff
8cbea680b0436034c29eaab462ae02cfaecc51c5c0b2379edc2784ee1c9a9077  patches/canon-full-on-1de2effc.diff
c1b4acc0d3c67343bf856588547cac47298afbc51a34186ffcfed71984388fdc  patches/iq1m-split.diff
aa9e52daa81686a0c8a56b8d633fe58e1ccbfff21442d0fe091c8b9be9f036e5  results/work/quant-rows2.fixed.goose
fa697aa233d74d02f91fef2c3e01a79cc425badf22dfbff6a32e1903ac0cac02  results/work/quant-weighted.fixed.goose
9cd66f63ab11fda1a48e8f951c663970f5032228e2213576714be7303c3c8353  tests/fixture.cpp
c5064a8e2853c8f221215f0b1633c8fe81886b44647daad938d3127b0e029360  tests/mutate.py
4ec7e670520c03033929dd476e467d24342ce7cccc80d40739b69eb7ec4a60c5  results/gate-after.txt
1a220a07b34a769b16ba5dc10acb5f7a85ae47c76fef63f499310f8da5cdddb6  results/mutations.txt
```

`results/hashes.txt` (written by `tests/hashes.sh`) is the authoritative list.

## 9. Raw evidence

```
results/trace-decisions.txt   native / before / fixed split decisions, ib=0..3
results/gate-before.txt       12 of 24 differ
results/gate-after.txt        0 of 24 differ
results/mutations.txt         twelve mutations, halves, restore
results/native-tables.c       native init tables used by the fixture
results/gen/                  generated C for before, fixed and every mutant
results/work/                 materialised module states
```
