# DEFECT: RoPE aborts on inputs outside the Apex profile

Status: open, reproduced, isolated, not yet fixed.
Found: 2026-11-08, while running `test-backend-ops` against `runtime/exp196/run`.

## What happens

    test-backend-ops test -o ROPE -b CUDA0

with `GGML_GOOSE_CUDA_ROPE` unset aborts immediately, before printing a single case result.
The backtrace ends in `fg171r_fail` ← `fg171rh_gg_function_1` ← `fg171r_enter`, i.e. the Goose
module traps on its own validation rather than declining the case and letting the native path
run.

## It is our port, not the baseline

Same binary, one variable:

| run | result |
|---|---|
| `GGML_GOOSE_CUDA_ROPE=0` | 2/2 backends passed |
| switch unset (Goose on) | aborts before the first case completes |

## Why the Apex profile never showed it

The live witness reads `FG171R_CUDA_ROPE host=0x1,cuda_status=0,modes=0x8,variants=0x50`, so
the module does execute during inference — for exactly the one RoPE configuration Apex uses.
The test suite enumerates the whole matrix (both rotation types, several `n_dims`, inplace and
not, `f32`/`f16`, and the `ROPE_SET_ROWS` variants), and the module traps on the first case it
does not model.

## Relation to the GET_ROWS defect

Same class, same root: a module that models one configuration and traps on the rest instead of
falling back. `DEFECT-quantized-get-rows.md` in EXP176 records the sibling case, where the
trap was a misaligned read rather than a validation code. Both were invisible because the gate
for each stage compared only the shape the Apex profile exercises.

The general fix is the same shape in both places: bound the Goose path by a predicate that
matches what the body actually models, and send everything else to the native branch. For RoPE
that predicate has to cover the rotation type, the dimension, the inplace flag and the type
pair, so it is a slightly larger check than GET_ROWS' single type comparison.

## Not fixed here

Fixing this needs the same chain procedure as GET_ROWS: change the module, rebuild the object
and relink the stages that inherit it, rerun `test-backend-ops -o ROPE`, then the paired Apex
protocol. It is the next piece of work after the GET_ROWS gate, which is already rebuilt and
verified.

## Note on scope of the earlier verification

Every per-stage gate in this chain was written against the Apex profile, and so was every
"covered" row in the census. That was deliberate — port by fact, not by guess — but it means
"covered" describes the profile, not the operator. `test-backend-ops` is the instrument that
shows the difference, and running it across the whole matrix is what P3-19 is for.
