# DEFECT: RoPE aborts on inputs outside the Apex profile

Status: **FIXED** and verified against `test-backend-ops`.
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

## Exact trap

The abort carries a code, which `cuda-bridge.cuh:21` prints:

    Goose RoPE validation failed:code=27

`host.goose:88` is `ensure(n_ctx>0,27)`, and the variable is read at `host.goose:71` as
`ti(node,8,4)` — `op_params[4]`, which the native code calls `n_ctx_orig`.

The native kernel does not require it to be positive. `rope.cu:582` shows `n_ctx`
(`op_params[3]`) commented out entirely, and only `n_ctx_orig` is used, in
`ggml_rope_yarn_corr_dims`. Called with zero, that function yields a non-finite correction
bound and the native kernel simply carries it into `fmaxf(0, floor(...))` / `fminf(n_dims-1,
ceil(...))` without complaint.

So the Goose contract is stricter than the code it replaces. Removing the single `ensure` is
not enough: the same zero would then reach `floorh`, whose own `ensure(fin(x),49)` would trap
on the `-inf`, so the trap would move from code 27 to code 49 rather than disappear.

## Two candidate fixes

1. **Bound the Goose path by the contract it actually models.** Decline to the native branch
   when `n_ctx_orig <= 0`, exactly as the GET_ROWS fix declines on a type mismatch. Smallest
   change, no behavioural modelling of the non-finite case.
2. **Make validation failure a fallback instead of a trap.** `fg171r_fail` would call the
   native op rather than `GGML_ABORT`. This is the more robust design — any configuration the
   body does not model stops being fatal — but it changes what the module's own gate asserts
   about invalid inputs, so that gate has to be revisited rather than left as is.

Option 1 is the one to do first: it is the same shape as the GET_ROWS fix, it keeps the gate
honest, and it cannot silently change trap semantics.

## Fix applied

Option 1, in `source/cuda-bridge.cuh`: the Goose path now also declines when
`op_params[4] <= 0`,

    if (!fg171r_enabled() || op_params[2] == GGML_ROPE_TYPE_VISION || op_params[4] <= 0) {
        ...native...
    }

so the non-finite correction-dims case is never modelled. EXP171 was rebuilt and the active
runtime relinked; the link line references `runtime/exp171/build/{cuda-rope,host-goose}.o` by
path, so no downstream stage carries a copy.

    test-backend-ops test -o ROPE -b CUDA0   ->  2/2 backends passed

The full suite then completes instead of aborting: 18748 cases OK, 16 FAIL, no abort. The
remaining failures are mapped in `docs/goose-migration/test-backend-ops-failures.md`.

Note also that this is the second module found this way, which says the pattern is systematic:
each stage's gate was written against the Apex profile, so "covered" means "covered for the
profile". The next step after this fix is to run the full matrix and fix the modules it names,
rather than assuming RoPE is the last.
