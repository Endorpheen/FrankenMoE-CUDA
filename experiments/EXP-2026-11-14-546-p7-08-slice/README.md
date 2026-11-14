# EXP546: slice, ternary and comment nodes (P7-08 slice 5, part 36)

## Hypothesis

EXP545 left four encoder gaps and one skipped-by-runtime template. Slice,
ternary and comment are small, deterministic nodes: slice is `value.cpp slice()`
with direction-dependent defaults and negative bounds, ternary evaluates a
condition and one branch, a comment is a no-op. Implementing all three should
drop the skipped count to zero and bring four more real templates into the gate.

That held, with one correction: the slice defaults and clamping are not Python's
rules verbatim, and one mutation showed where a naive reading diverges.

## Rules captured

| node | rule |
|---|---|
| slice | `step` defaults to 1; a negative `step` defaults `start = len-1` and `stop = -1`; a negative bound counts from the end; forward `start` clamps to `len`, reverse `start` to `len-1`; reverse `stop` clamps to `-1`; the walk is `direction*i < direction*stop`, skipping out-of-range indices; `step == 0` raises `slice step cannot be zero` |
| slice types | arrays and tuples keep their kind (a tuple slice stays a tuple), strings slice by byte; any other type is an unknown-builtin error |
| ternary | evaluate the condition, then only the taken branch |
| comment | no-op |

## Gate

```
FG546_RUNTIME_SLICE cases=526 values=916 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=526 values=916 failures=0 skipped=0
```

Real templates now gated: `google-gemma-2-2b-it`, `llama-cpp-deepseek-r1`,
`LFM2-8B-A1B`, `google-gemma-4-31B-it`, `HuggingFaceTB-SmolLM3-3B`. Slice cases
cover forward, reverse, negative bounds, a step, a zero step, strings and a
tuple; ternary and comment are exercised through the templates themselves.

`Mistral-Small-3.2-24B-Instruct-2506` is excluded: its first line calls
`strftime_now`, which is deliberately not ported (wall-clock dependency).

## Two findings from the mutation round

1. The first "negative start" mutation looked equivalent because the nested
   `if sv < 0 { sv = 0; }` clamp restored the same value. The replacement
   (`sv = 0 - sv`) distinguishes the behaviour.
2. A tuple-slice mutation was invisible because the rendered text of a tuple and
   an array slice coincide in these templates, and the type-check mutation had no
   case that slices a non-container. Both were replaced by observable ones plus a
   new `v[:2]` case.

## Not ported

- `strftime_now` (wall clock), the typed-content direction, grammars/autoparser,
  string marking/caps, model parsers and the legacy `use_jinja=false` path.
