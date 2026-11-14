# EXP554: first-part and all-parts flags (P7-08 slice 5, part 44)

## Hypothesis

EXP553 tracked one input bit per value, which was enough for single-part strings
but not for concatenation and `format`, where the native runtime carries a part
list. The header comment promised a simple model, so the question was what the
code actually distinguishes.

## What the native code says

A string is a list of parts, each with its own `is_input` (`string.h`). Two
different questions are asked of that list:

- `common/chat.cpp` strips BOS/EOS from the **first prompt part**, which is the
  first non-empty rendered piece;
- `all_parts_are_input()` (`string.cpp`) is true only when **every** part is
  input, and `mark_input_based_on` marks the result only in that case.

The two are not the same value:

| expression | first prompt part | all parts input |
|---|---|---|
| `content` (input) | input | true |
| `content ~ 'b'` | input | false |
| `'' ~ content` | input (the empty part is skipped) | false (the empty part counts) |
| `content ~ ''` | input | false |
| `''` | none | false (one unmarked empty part) |

`format` is where this shows: literal pieces between placeholders are pushed with
`fmt_is_input = fmt.all_parts_are_input()`, and argument pieces keep their own
per-part flags.

## Port

A second flag bit (32) joins the existing first-part bit (16):

- `chat_mark_value` marks context strings with both bits;
- one-to-one transforms copy both bits (`inherit_input`);
- `replace`, `slice`, `indent` and the `split` tail use `mark_from_input`, which
  marks only when the source has the all-parts bit;
- `~` and `+` take the first-part flag from the first operand that has parts and
  the all-parts flag as the AND of both operands, so an empty operand zeroes the
  all flag;
- `format` records the first emitted piece (argument first-part flag, or the
  format string's all-parts flag for a literal piece) and ANDs the all flag over
  every emitted piece;
- `*` copies both bits, `join`/`tojson` stay unmarked.

## Gate

```
FG554_RUNTIME_PARTS cases=618 values=1098 failures=0 skipped=0
mutations 7/7 caught (range 0:7)
mutations 7/7 caught (range 7:14)
baseline after restore: rc=0 cases=618 values=1098 failures=0 skipped=0
```

Eleven new cases: `content ~ 'b'`, `'' ~ content`, `replace` on `content ~ 'x'`,
a format string with a literal tail, format literals using the all-parts flag,
both empty-operand directions through `replace`, format keeping and clearing the
all bit, and two chained filters (`replace|replace`, `strip|replace`).

Fourteen mutations: `mark_input_based_on` never/always marks, tests the wrong bit
or sets only one bit; `inherit_input` drops the all bit or every bit; context
marking sets only the first-part bit; concat takes the empty left part or ignores
a side's all flag; format's literals use the wrong bit, and the all flag is never
or always set or ignores argument flags.

## Corrections found by the gate

- The first implementation treated an empty string as "no parts", so `'' ~ x`
  did not constrain the all flag. The gate showed native zeroes it: the empty
  string has one empty part, it is only skipped when picking the first prompt
  part.
- `format` literal pieces were keyed off the format's first-part flag; native
  uses `all_parts_are_input()`, which differs when the format string itself is a
  concatenation.

## Still open

- Part lists are still approximated by two bits per value: a value whose parts
  alternate (input, literal, input) is represented by its first and its AND.
  Templates in the gate do not distinguish further.
- The typed-content direction, grammars/autoparser, model parsers,
  `use_jinja=false`, and the chat API's missing time injection remain native.
