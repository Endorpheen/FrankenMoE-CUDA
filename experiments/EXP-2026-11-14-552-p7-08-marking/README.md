# EXP552: input marking and the BOS/EOS strip (P7-08 slice 5, part 42)

## Hypothesis

`is_input` does not change the rendered bytes on its own, but `common/chat.cpp`
strips BOS/EOS from the first/last prompt part **only when that part is not
input**. With a template that renders `{{ bos_token }}` as literal text and the
user content next to it, the two paths must differ, and the strip is observable
in `prompt.str()`.

The oracle is `common_chat_template_direct_apply`, which takes
`autoparser::generation_params` with `add_bos`, `add_eos` and `mark_input`.
`common_chat_templates_apply` cannot be used for this: it copies `add_bos` from
the template object, which is false without a model.

## Port

| piece | rule |
|---|---|
| marking | a value carries bit 16 in the existing usage mask; context strings reachable from `messages` are marked, template literals are not |
| parts | `append_rendered` records the flag of the first and the last rendered part |
| strip | `chat_strip_affix` removes BOS from the start and EOS from the end only when the corresponding part is not input and the add flag is set |
| bases | `chat_bases` sets the arena row bases before marking, because the render resets them later |

## Gate

```
FG552_RUNTIME_MARK cases=593 values=1048 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=593 values=1048 failures=0 skipped=0
```

Six cases: literal BOS/EOS stripped; no add flags and nothing stripped; BOS only;
input starting with `<s>` kept; input ending with `</s>` kept; a literal `<s>`
inside the template stripped.

Mutations: marking disabled, first/last flag forced, strip ignoring the add flag
or the mark bit, one byte less removed, the copy starting at the wrong offset,
marking not recursing into containers, and object values not marked.

## Approximations recorded

1. The flag is set on the context subtree rather than per `from_json` call; the
   fixture marks the `messages` value, which is the same set the native side
   marks.
2. Filters do not propagate the flag yet. `upper`/`lower`/`strip`/`replace` and
   the rest create a new string with the flag cleared, so a template that filters
   user content before printing it would have that part treated as template text.
   The gate covers identity and literals only; this is the next marking step.

## Not ported

- `all_parts_are_input` for `format` (the one-to-many and many-to-one rules).
- The typed-content direction, grammars/autoparser, model parsers and
  `use_jinja=false`.
