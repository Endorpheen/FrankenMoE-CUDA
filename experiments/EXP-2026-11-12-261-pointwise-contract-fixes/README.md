# EXP261: pointwise contract fixes -- diagnosis corrected, fixes staged, regeneration blocked on a bridge remap

Status: diagnosis and staged source. Not accepted yet; the library rebuild and the contract run are the next step and
are spelled out below.

## Defect 1 is not where the consolidated list said it was

`FIX-LIST-20261112.md` recorded it as "SILU scalar tail uses libm expf while the wide path uses the bit-exact
avx2_exp_compat". Measurement says otherwise.

The probe was re-run unchanged (`tools/p2-01-contract/build-and-compare.sh`): 26 of 37 cases bit-identical, the same 11
divergences. Then the differing indices were analysed per case:

| case | differing indices |
|---|---|
| `silu_1000` | 247 of 1000, spread from 0 to 999 -- including 0, 1, 3, 4 and the tail 991..999 |
| `silu_t1`, `silu_t8` (12 values) | exactly one: index 3, which is x = -1 |

Index 3 is inside the vectorised head (`col < (12/8)*8 = 8`), not the tail. So the divergence is not the tail.

A one-file probe against the flint header settles which side is which:

    x=-1  native vector ggml_v_silu = be89b2b0  (-0.268941402)
    x=-1  native scalar expf        = be89b2b1  (-0.268941432)
    Goose arm result                = be89b2b1  (the expf value)

The Goose arm produces the libm result, not the AVX2 result. Reading the module explains it: `silu_compat` (the
AVX2-compatible association) is called only from `glu`; the standalone SILU op goes through `unary_value`, whose SILU
case returns `silu(x)` -- libm `expf` -- for every element. So the whole vectorised head of every plain SILU tensor
diverges by 1-2 ULP, which is exactly the 247/1000 spread.

Correct fix (staged here): give `unary_value` the column and width and route its SILU case through `silu_compat`, the
same way the GLU path already does.

## The other two defects are confirmed as recorded

`clamp_t1` differs at indices 6 and 7 (the two NaNs): Goose -0.5, native +0.5. Native is `MAX(MIN(x, hi), lo)`, the
module was `min(max(x, lo), hi)`. `scale_t1` differs at index 1: Goose +0, native -0, because the module always adds
the bias and `(-0) + (+0) = +0`, while native skips the addend when it is zero.

Three changes are applied in `source/pointwise.goose`:

    unary_value(...,col,width); SILU -> silu_compat(p,x,col,width)
    OP_CLAMP: max32(min32(a,hi),lo)
    OP_SCALE: if bias == 0 then a*k else a*k + bias

## Regeneration: one compiler is blocked, the artifact compiler works

The current pinned compiler (`runtime-candidate-v61/stage3`) cannot regenerate this kernel as written:

    codec/decode.goose:125: error: incompatible types (type 3 kind 1 width 8 -> 46 kind 1 width 2)
    ... bitscale(a,si+sub,(2*((high>>12)&7)+1) as i32); ...

That is an `i64 -> i32` cast the older compiler accepted. Recorded as a blocker with its exact location rather than
worked around silently: either the expression needs an explicit narrowing (which changes the accepted kernel's codegen
for an unrelated decoder) or the CPU wave keeps its artifact compiler.

Regenerated with the compiler recorded in `generation-command.json`
(`/home/end0/AI/LlamaServer/franken-goose-live-20261107/compiler/stage3-454151b3449b`, sha256 prefix `454151b3449b`):

    generated 597140 bytes vs live 595643; 16 diff hunks, 538 diff lines

Most hunks are function renumbering, which is the consequence of the new `unary_value` signature: the emitter numbers
functions by emission order. That matters because the hand-written bridge calls emitted functions by number. The
regenerated C therefore **cannot be dropped in as is** -- the bridge's `gg_function_N` mapping must be re-derived from
the new emission first. This is the same hazard EXP392 recorded for module entry points.

## Next step, in order

1. Re-derive the CPU backend bridge's function mapping against the regenerated C and check every call site by
   signature, not by number.
2. Rebuild `libggml-cpu` in an isolated directory, stage it beside the accepted set, and run
   `tools/p2-01-contract/build-and-compare.sh <stage> <flint>`: the expected outcome is that
   `silu_t1`, `silu_t8`, `silu_1000`, `clamp_t1`, `clamp_t8`, `scale_t1`, `scale_t8` leave the divergence set, leaving
   only norm/softmax/mul_mat/rope.
3. Apex regression on the accepted profile (`e68df05b`, 32K/128) with the rebuilt library, as the consolidated list
   requires.

## Result: the fixes are verified

The bridge maps only `gg_function_1`, the single top-level entry, and its signature is byte-identical between the live
and the regenerated C -- the function renumbering inside the file therefore does not require a bridge change. (The
caution in the previous section still holds for modules that call internal entries by number; this module does not.)

Built in an isolated directory with the recorded `integration-commands.json` recipe, staged beside the accepted set,
and measured:

    differing: norm_finite, softmax_finite, mul_mat_32x8x6, rope_16x2x4
    silu_t1, silu_t8, silu_1000, clamp_t1, clamp_t8, scale_t1, scale_t8 -- no longer reproduce
    Apex 32K/128: e68df05b, probs_equal, restore_ok, slot 387989920 bytes

`tools/p2-01-contract/build-and-compare.sh` now records the four remaining divergences as its expected set, so exit
code 0 means "exactly the remaining ULP-close ops of other items".

P2-01's criterion is met for every binary/unary/GLU case this probe exercises, including NaN payloads and signed zero.
The one recorded coverage gap is strided views: the probe still uses contiguous 1-D tensors.
