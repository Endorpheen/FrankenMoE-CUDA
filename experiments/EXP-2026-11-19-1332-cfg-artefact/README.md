# EXP-2026-11-19-1332: config validation checked against the shipped library, with the two fields the old gate lacked

## What was blocked, and why it was not a relink

EXP-2026-11-19-1331 established that the old gate could not be pointed at the shipped library: not because of its
`gg_function_1` call, but because the current module validates `moe.drop_cold_frac` and `moe.substitute_lambda` at
`f[3]` and `f[4]`, which a 37-parameter gate cannot set. Relinking alone would have produced numbers whose
disagreements could not be attributed.

## What was done

The gate was copied into this experiment and extended in three places, each named rather than guessed:

1. the two fields are randomised across both sides of their rules - `0`, `.4`, `.8`, `1.2`, `1.0`, and a uniform
   draw in `[0.1, 0.9]` - so the range rules and the cache-requirement rules are both reachable;
2. `native_rule` maps the four new native messages to rules **45-48**, the numbers the module assigns them;
3. `wrap_now.c` maps the gate's flat arguments onto the current `cfg_validate(i[], f[])` by **native field name**,
   using the index map the module documents. Nothing is inferred from argument position.

The native side is still `bmoe::validate` - real native code - and the port side is `cfg_validate` from
`builds/flint-acc/bin/libllama.so`.

## Result

```
config validate: cases=4000 fails=0 uncovered=0
```

Four thousand randomised configurations, no disagreement, and **no rule left uncovered** - which is the part that
matters here: the four new rules were exercised and agreed, not merely present.

Wired into the sweep, which is now **20 passed, 0 failed, 1 stale**.

## What this closes

The last of the twelve composed modules now has a check against the artefact everyone runs (EXP-2026-11-19-1330 listed
eleven of twelve). The distinction the report keeps drawing still holds: the rules are proven in the shipped library,
and the server does not yet take its validation decisions from it.
