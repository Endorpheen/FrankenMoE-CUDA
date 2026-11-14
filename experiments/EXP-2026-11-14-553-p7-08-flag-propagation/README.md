# EXP553: input-flag propagation through filters (P7-08 slice 5, part 43)

## Hypothesis

EXP552 marked context strings and gated the BOS/EOS strip, but the flag stopped
at the first filter: every filter built a fresh string with the flag cleared, so
filtered user content was treated as template text and lost its BOS/EOS. The
native rules are written down in `common/jinja/string.h`, so each filter can be
classified and matched.

## What the native code actually does

Reading `string.cpp` and `value.cpp` instead of trusting the header comment:

| native branch | rule | example |
|---|---|---|
| `apply_transform` (upper/lower/capitalize, strip, title) | keeps the flag per part | `<s>x|upper` stays input |
| `replace`, `slice`, `indent` | `mark_input_based_on(input)` | kept |
| `string`/`safe` | returns the value unchanged | kept |
| `tojson` | builds a fresh string, no marking | dropped |
| `join` | builds a fresh string, no marking | dropped |
| `split`/`rsplit` | **only the remaining tail** is marked; cut tokens are not | `a|b` split: `b` input, `a` not |
| `format` | flag of the first non-empty rendered piece | placeholder first -> kept |

`split` is the surprising one: the header comment calls it one-to-many, but the
implementation marks only the trailing remainder, because the cut tokens are
created before the `mark_input_based_on` call.

## Port

`inherit_input(src, dst)` copies bit 16 of the usage mask. It is called by the
identity filters, `replace`, `slice`, `indent`, `string`/`safe`, by the split
tail, and conditionally by `format`, which records the flag of the first non-empty
piece (argument or format text, whichever is emitted first) while it builds the
output.

## Gate

```
FG553_RUNTIME_PROP cases=607 values=1076 failures=0 skipped=0
mutations 7/7 caught (range 0:7)
mutations 7/7 caught (range 7:14)
baseline after restore: rc=0 cases=607 values=1076 failures=0 skipped=0
```

Fourteen new `check_mark` cases on top of the six from EXP552: `strip`, `replace`,
`title`, `slice`, `indent`, a literal transform, `join` with and without a
literal element, two `format` shapes (placeholder first, literal prefix), a
literal `format` argument, `split` then `join`, the split remainder, and the first
cut token.

Fourteen mutations: identity filters never/always mark, inheritance reads the
destination flag, `replace` drops the flag or takes it from its first argument,
`format` never/always marks, ignores argument flags, ignores the format-text flag,
keeps the last instead of the first piece, `join` marks unconditionally, `slice`
and `indent` drop the flag, and the split tail loses the flag.

## Corrections found by the gate

Two of the first attempts were wrong and the gate proved it, which is why they
are recorded here rather than silently fixed:

1. `join` was marking when all elements were input. Native `join` builds a fresh
   string and never marks, so the rule was removed.
2. `split` was marking every token. Native marks only the tail; the cut tokens
   are dropped back to template text.

## Still open

- `all_parts_are_input` for `format` in the many-part sense is approximated by
  "first non-empty piece"; the current templates do not distinguish further.
- The typed-content direction, grammars/autoparser, model parsers,
  `use_jinja=false`, and the chat API's missing time injection remain native.
