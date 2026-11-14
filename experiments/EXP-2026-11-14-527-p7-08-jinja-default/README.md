# EXP527: the default filter (P7-08 slice 5, part 17)

## Hypothesis

The chat layer needs a new harness before it can be gated (it links
common/chat.cpp and llama-chat.cpp, which the template fixture does not), so
this slice takes the largest remaining template-level gap that the chat
templates actually use: `default`. The native runtime defines it in every type
table except objects, where it is commented out with the note "cause issue with
gpt-oss". The Goose runtime already carried `h_default` from the EXP506 builtin
port, so only the filter route and the method tables were missing.

That held, with one correction from the gate: objects must NOT fall into the
filter branch, because the native error for `o|default(x)` is the unknown-filter
message for type Object.

## Native semantics (common/jinja/value.cpp, `default_value`)

| piece | rule |
|---|---|
| arguments | `default(input, default, boolean=false)`; the input is bound by the filter, so the call form passes `default` and optionally `boolean` |
| check off | the default is used only when the input is `undefined` or `none` |
| check on | the default is used when the input is falsy (empty string, zero, false, empty container, undefined, none) |
| `boolean` | a keyword or the second positional argument |
| object operand | no `default` entry: unknown-filter error |
| method form | `x.default(y)` works for int, float, bool, string and array |

## Plan

1. Route the `default` filter through the existing `h_default` semantics,
   excluding object operands, and honour the `boolean` keyword or positional.
2. Add `default` to the scalar, string and array method tables (not objects).
3. Initialise the filter argument array with `undefined` instead of value id 0,
   so an absent positional argument cannot be mistaken for a real value.
4. Fixture cases: undefined/none/empty/zero with and without the check, the
   keyword and positional boolean, the method form on four types, and the object
   rejection.
5. Gate, at least 12 mutations in two halves, docs, one commit.

## ACCEPT

- Gate green over the whole corpus.
- At least 12 mutations, all caught, baseline green after restore.
- The object operand keeps the native unknown-filter error.
- No new fixed-size buffers beyond the existing four-slot argument array.

## Not ported

- The chat layer itself: it needs a harness that links the chat sources and
  compares against `common_chat_templates_apply`; the template fixture cannot.
- String marking and caps: observable only in that chat layer.
- `strftime_now`: wall-clock dependent, no deterministic native input.
