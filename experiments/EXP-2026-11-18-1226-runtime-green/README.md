# EXP-2026-11-18-1226: the accumulated runtime typechecks and builds again

```
[2/5] typecheck  typechecked ok: 4412 specialization(s), 7 struct/0 enum instance(s)
[4/5] compile object at -O1
[5/5] symbol check
main runtime ok: 1184 global symbols, 0 duplicates, 148 extern, C 21152246 bytes, object 5839712 bytes
```

That is the first green `tools/build-main-runtime.sh` in this session, and it carries every module added
today: `goose/model/tokenizer-decisions.goose`, `goose/speculative/spec-decisions.goose`, the doors and
the chain.

## Two defects behind it, one of them worse than the error it hid

1. **A duplicate declaration made `exp` ambiguous.** The chain module carried its own
   `extern fn exp(x: f64) -> f64;` while the standard library provides `exp` for both `f32` and `f64`.
   Alone, the module resolved it; composed with the rest of the unit, both overloads matched and the
   typecheck failed at two call sites. Removing the local declaration fixes it - and reveals that my
   first "fix", casting the arguments, was unnecessary: the compiler then reported
   `redundant 'as f64': max_l is already f64`. The casts were reverted, so the source says what it means.

2. **The build step could not fail.** The typecheck was

   ```
   "$GOOSE" check "$UNIT" 2>&1 | grep -E "typechecked ok|error" | tail -2
   ```

   and a pipeline's exit status is the last command's, so a failing typecheck left `tail` returning 0 and
   the build reported success **without generating any C**. That is why the runtime "built" for as long as
   the two `exp` errors existed: they were printed and ignored.

   It now writes `typecheck.log`, prints the verdict and `exit 1` on any error. A build that cannot fail is
   worse than a red one: it hides exactly the class of mistake this project keeps paying for.

## What follows

The checkpoint this session was working towards can now be taken: the unit composes, typechecks,
compiles with no duplicate symbols, and the sampler chain inside it has the acceptance evidence from
EXP-2026-11-18-1225 (six official runs, identical hash).
