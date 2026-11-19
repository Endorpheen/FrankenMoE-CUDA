# EXP-2026-11-18-1220: live speculative numbers, with the call ABI finally correct

Both slice doors are now called with struct arguments copied from the generated header, which is the
only reason any of these numbers exist.

```
FG_SP_NMAX    goose calls=1 disagreements=0      (configuration, four spec types tried earlier)
FG_SP_PREFIX  goose calls=5 disagreements=0      (128-token speculative generation, temp 0.8)
```

Five live comparisons is a small number, and it is the honest one: the accept route that computes a
prefix is entered once per verify step, and this run took few steps with an ngram drafter.

## What was wrong until now, in one line

The earlier `FG_SP_NMAX goose calls=1 disagreements=0` was produced by an **invalid call** - flattened
pointer/length arguments instead of structs - so its agreement was luck, not evidence. It has been
re-measured with the correct ABI and still reports zero, which is the first time that number means
something.

That is worth stating plainly: a green counter produced by a malformed call is worse than a red one,
because it looks like progress.

## State of the decision set

| decision | gate | live |
|---|---|---|
| draft compatibility | 10 cases | - |
| draft width (`n_max`) | 10 cases | 1 call, 0 disagreements |
| prefix agreement | 7 cases | 5 calls, 0 disagreements |
| synthetic acceptance | 2006 cases | hook in place, route not taken |
| rejection acceptance | 5006 cases | hook in place, route not taken |
| eleven others | gated separately | not wired |
