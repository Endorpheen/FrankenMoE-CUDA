# EXP-2026-11-19-1408: the bench-report module is composed and answers on hand-checkable cases

## What was and was not there

`goose/cli/llama-bench-report.goose` existed with its doors, and none of them was in the shipped library - the same shape
as the fit-params module one card earlier: a file, not a capability. One line in the module list composes it:

```
nm -D libllama.so.0 | grep -c 'T lb_'   -> 23
```

## The probe, on values that need no benchmark

A door that links is not a door that works, so three means and two CSV escapes were checked by hand:

```
avg {2,4}=3   {1,2,3}=2   {7}=7
escape(abc)   -> "abc"
escape(a"b)   -> "a""b"
bench doors: cases=5 failures=0
```

The integer mean truncates, which is what an average of counts should do here, and the escape doubles an inner quote -
the CSV rule - and quotes every field, which is what the native report writer does for its columns.

## What this does and does not establish

Establishes: the module is inside the cumulative unit, twenty-three of its doors are reachable, and five cases whose
answers are known agree.

Does not establish: that the report a `llama-bench` run would produce matches. No `llama-bench` run is compared here; the
report formatting is verified at the level of its arithmetic and escaping, not end to end.

## Shipped path

Additive; no call site uses these doors yet. Acceptance afterwards: decode 62.85 / 62.87 / 63.34, prefill 2700.2 / 2957.2 / 2951.7, hash `c2ff42cc1219`.
