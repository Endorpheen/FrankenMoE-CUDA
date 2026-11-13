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

## Diagnostic base built and probed (2026-11-13)

The base was rebuilt from the flint CMake objects with `ggml.c.o` replaced by the
patched `ggml-constructors.c` (EXP155 geometry + EXP156 constructors):

    gcc-13 <flint ggml.c flags> -I EXP156/source -I EXP155/source \
        -o ggml-constructors.o -c EXP156/source/ggml-constructors.c
    g++-13 -shared -Wl,-Bsymbolic -Wl,-soname,libggml-base.so.0 \
        -o libggml-base-diag.so.0 <8 flint objects> ggml-constructors.o -lm -lgomp -lpthread

Probe results, compared with the flint base on the same call:

| probe | flint | patched base |
|---|---|---|
| `probe.c` (full constructor/view/permute/reshape surface, including the Q4_K `view_3d` with `ne0 = 128`) | completes | **completes, byte-identical output** (`diff` empty) |
| `probe-errors.c` case 7 (non-block-aligned view) | no abort | **no abort** |
| `probe-errors.c` cases 1,2,4,5,6 | abort | abort (decisions still match) |
| `probe-errors.c` case 3 (overflow) | abort | no abort — pre-existing difference in the EXP155/156 unit, present in the deployed base too, unrelated to this patch |

## Apex acceptance blocked by three symbols

The diagnostic base cannot serve the Apex run: our `libggml-cuda` and
`libggml-cpu` call Goose functions that the base provides.

    libggml-cuda.so.0: fg160_fusion_enabled, fg160_query
    libggml-cpu.so.0:  one more fg symbol

The rebuilt base was linked from flint objects plus the constructors unit only, so
those symbols are absent and the server stops with `symbol lookup error`. Running
Apex with this base therefore requires linking the EXP160 host unit (and the CPU
one) into it — i.e. a fuller base build.

An attempt to run the canonical profile against the diagnostic base was made and
correctly failed; an earlier run that appeared to succeed had silently used the
baseline stage, and that result is discarded, not reported.

## Deployed base inspected (2026-11-13)

The preserved base contains the constructor unit and the other Goose modules:

    fg156 symbols: 15 (constructor entry plus witnesses)
    fg159: 22, fg160: 17, fg163: 39, fg164: 65

The EXP338 probe linked directly against the preserved base reports **no abort**
for case 7 (non-block-aligned `view_3d`), matching flint, and the result does not
change with `GGML_GOOSE_TENSOR_CONSTRUCTORS=0` or `=1`. So the strict check the
patch removes is not firing in the accepted base: either that build already lacks
it, or the view path does not reach that constructor entry. The numeric error-code
text gap recorded in EXP338 is likewise not observable there.

## Why Apex cannot run on a rebuilt base yet

The diagnostic base built from flint objects needs only two symbols from EXP160
(`fg160_fusion_enabled`, `fg160_query`); the CPU library has none. EXP160's run
artifacts are gone (`runtime/exp160/run` is empty) and its build recipe depends on
`runtime/exp159/build/ggml-graph.o`, which is also missing. Reproducing that
composite unit from source is the base-rebuild task recorded under P10-06/P10-11.

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
