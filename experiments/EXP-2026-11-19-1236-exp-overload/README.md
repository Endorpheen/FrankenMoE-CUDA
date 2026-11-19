# EXP-2026-11-19-1236: the `exp` overload clash, resolved by naming the symbol apart

## What was happening

Two builds, two opposite failures, one cause:

| context | `extern fn exp(x: f64) -> f64;` present | absent |
|---|---|---|
| chain build alone | works | `error: unknown function: exp` |
| composed unit | `error: ambiguous call to exp: multiple overloads match equally well` | works |

The standard library provides `exp` for `f32` and `f64` in the unit; the chain build alone does not pull it in.
A local `extern fn exp` gives the unit a **second exact match** for an `f64` argument, and both builds then
fail in opposite directions. My earlier "fix" - removing the declaration - worked for the unit and broke the
chain, which is why the error kept coming back.

## The resolution

Bind the C symbol under a Goose-only name:

```
extern "gs_exp" fn fg_exp(x: f64) -> f64;
```

and call `fg_exp`. No overload set is extended in either context, so both builds are green:

```
chain build:        built into build/sampler-chain
main runtime:       typechecked ok: 4422 specialization(s)
                    main runtime ok: 1192 global symbols, 0 duplicates, 148 extern
```

## And the acceptance hash still holds

```
decode [45.46, 45.87, 46.11]   prefill [1956.6, 2257.9, 2256.1]   sha {'c2ff42cc1219'}
```

## Why this took three attempts

The build step that should have caught it was swallowing its own failure (`| tail` returned 0), so the first
two attempts looked successful. With that fixed, the step reported `typecheck FAILED` and the two contexts
could be compared directly.
