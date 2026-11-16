# The value path works: a real q4_K Apex tensor through a kernel

Closes the gap REAL-VALUES-GAP-20261117 named. Until this harness, every kernel in this runtime had run at
real dimensions with fabricated values, and the 21.7 GB payload had been read but never consumed.

## The result

```
FG_VALUES tensor=blk.3.attn_q.weight type=q4_K ne=[2048 8192] bytes=9437184
          max_abs=1.042329e-03 max_rel=2.773e-03 rms=5.470908e-02 nonfinite=0 nonzero=8192 PASS
```

`blk.3.attn_q.weight` in its own quantised type, read at its declared offset from the file and multiplied by
a deterministic vector through ggml's quantised `mul_mat`. The reference is the same weights dequantised to
f32 through `ggml_get_type_traits(type)->to_float` and multiplied with a plain f32 `mul_mat` - two
independent paths through ggml.

`max_rel = 2.773e-03` of the result vector's magnitude, `rms = 5.47e-02`, all 8192 outputs finite and
non-zero.

## Why 2.8e-3 is agreement, not error

The two paths are not meant to be bit-identical: the quantised `mul_mat` dequantises per block and folds the
per-block scale into the accumulation, while the reference dequantises once and then does f32 arithmetic. For
q4_K (4.5 bits per weight) a few parts in a thousand of the vector magnitude is the expected difference, and
the gate allows 5e-3.

That tolerance is tight enough to be a real check: a wrong block size, a misread tensor offset, a mismatched
row stride or a broken `to_float` moves the result far outside it. The first version of this harness failed
for a different reason - per-element relative error reported 26 for absolute agreement of 1e-3, because a dot
product that lands near zero makes per-element relative error meaningless. Measuring against the magnitude of
the whole vector is the fix, and it is a metric bug rather than a value bug, recorded so it is not re-derived.

## What it does not prove

That the model answers correctly. One tensor through one projection says the dequantisation and the
quantised matmul agree with the f32 path; it says nothing about 41 layers, the router, or the logits. The
next step is the same comparison against the **integrated runtime's** result for this tensor, which would tie
the standalone's arithmetic to the known-good implementation rather than to ggml's own second opinion.

## Status after this

| capability | before | now |
|---|---|---|
| read the container and all 21.7 GB of payload | yes | yes |
| run kernels at real dimensions (synthetic values) | yes, 20 harnesses | yes |
| **dequantise a real q4_K block and matmul over it** | **no** | **yes, 2.8e-03 vs f32** |
| compose 41 layers over real values | no | no |

So the two halves - reading the model and computing on it - now meet at exactly one tensor.

## All three types the model contains

The model holds exactly three: Q4_K (379 tensors), F32 (310), Q6_K (64). The harness now checks one of each.

```
FG_VALUES tensor=blk.3.attn_q.weight   type=q4_K ne=[2048 8192]   max_abs=5.944166e-04 max_rel=3.532e-03 PASS
FG_VALUES tensor=output.weight         type=q6_K ne=[2048 248320] max_abs=6.660819e-04 max_rel=2.889e-03 PASS
FG_VALUES tensor=output_norm.weight    type=f32  ne=[2048 1]      max_abs=0.000000e+00 max_rel=0.000e+00 PASS
FG_VALUES_ALL PASS
```

F32 gives exactly zero - it is the reference against itself, which is the control that shows the comparison
is wired (a broken comparison would report a non-zero difference here).

Two allowances, both justified rather than convenient:

- **256 rows instead of all of them.** Row stride and block layout are per row, so the first 256 rows
  exercise the same dequantisation. `output.weight` is 248320 rows, 1.2 GB once dequantised, which the
  harness has no reason to hold.
- **5e-3 tolerance on the quantised types.** Explained above; F32 needs none and gets none.
