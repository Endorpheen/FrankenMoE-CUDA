# P4-03: constructor patch applied, gate pending a relinked base

## Applied

`experiments/EXP-2026-11-08-156-goose-tensor-constructors/source/constructors.goose`
line 52: the extra `ensure(n0 % bs == 0, 15)` is removed, with the reason recorded
in place. Native `ggml_new_tensor_impl` divides by the block size without requiring
`ne0` to be a multiple of it, so `ggml_view_3d` over a Q4_K tensor with `ne0 = 128`
must be accepted.

The divergence and its reproducer are in
`experiments/EXP-2026-11-12-338-p4-03-constructor-probe/`: flint accepts that call
and reports `ne=[128,4,2,1] nb=[144,144,1152,2304] nbytes=1656 contig=0`, while our
base aborted with `Goose tensor constructor validation failed: code=15`.

## Why the gate is not run yet

The constructors unit is linked into `libggml-base`. To exercise the patched module
the base has to be relinked, and the recorded recipe is incomplete:

- `EXP156/build.py` expects `runtime/exp155/build/commands.json`, which no longer
  exists, so its link step cannot run;
- `EXP156/build/commands.json` records only the emit step, not the base link argv;
- the deployed base `e213fa19` is preserved but contains the unpatched unit.

The sweep that builds the base from source is the reproducibility work the
coordinator moved to P10, but the *gate for this patch* still needs one relinked
base variant. It is a diagnostic build; the accepted runtime is not replaced.

## Next step

Reconstruct the base link from the flint CMake build (`build-cuda` compile
commands) plus the EXP155 geometry unit and the patched EXP156 constructors unit,
link into `runtime/exp156/run/`, then:

1. run the EXP338 probe on flint and on the patched base — it must complete on
   both lines with identical `ne/nb/nbytes/contig`;
2. run the malformed-input corpus (codes 10-17) and confirm only the removed check
   changed behaviour;
3. run Apex 32K/128 and compare content hash, 128 ids and probabilities against
   the recorded baseline, then close P4-03.

## Scope note

Byte-identical rebuild of `libggml-base` is P10-06/P10-11 per the coordinator and is
not investigated further here. P4-03's criterion is the constructor/view/alias
surface and the preserved ABI.
