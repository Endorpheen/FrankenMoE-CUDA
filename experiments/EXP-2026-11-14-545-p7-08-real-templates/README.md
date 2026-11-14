# EXP545: real chat templates and namespace assignment (P7-08 slice 5, part 35)

## Hypothesis

Every earlier chat case used a synthetic template. Gating
`common_chat_format_example` against real templates from `models/templates/`
would either confirm the ported context construction on real input or name the
missing rules. The example helper renders a fixed four-message conversation
(system/user/assistant/user) with a generation prompt.

It did both. Two templates render identically; four are blocked by AST nodes the
fixture encoder does not implement, and one of them also needed a runtime rule
that was genuinely missing.

## Result

| template | outcome |
|---|---|
| `google-gemma-2-2b-it.jinja` | **passes** (system role unsupported, so the system text is merged into the first user turn) |
| `llama-cpp-deepseek-r1.jinja` | **passes** after namespace assignment was added |
| `Mistral-Small-3.2-24B-Instruct-2506.jinja` | skipped: `SliceExpression` (`messages[1:]`) |
| `LFM2-8B-A1B.jinja` | skipped: `SliceExpression` |
| `google-gemma-4-31B-it.jinja` | skipped: `Ternary` |
| `HuggingFaceTB-SmolLM3-3B.jinja` | skipped: `Comment` |
| `GigaChat3-10B-A1B.jinja` | excluded: `common_chat_templates_init` aborts inside the native library |

## Rule added

`set <object>.<key> = value` (`namespace` member assignment) was rejected with
"Invalid LHS inside assignment expression". `exec_set` now handles a member LHS:
it evaluates the object and the key (static name or computed), rewrites the
value slot when the key exists, and appends a key/value pair when it does not.
Existing keys are the common case (`namespace(a=1)` then `set ns.a = 2`); the
append path is exercised by `namespace()` followed by `set ns.a = 7`.

## Gate

```
FG545_RUNTIME_REAL cases=507 values=878 failures=0 skipped=4
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=507 values=878 failures=0 skipped=4
```

Cases: the 502 of EXP544, two real-template examples, and three namespace
assignment templates (new key, existing key, two keys). `skipped=4` counts the
templates whose AST cannot be encoded; each prints its reason.

Mutations: member assignment writing the key, matching any key, never finding
the key, storing the value as a key, storing the key as a value, the branch
skipped, an empty static name, computed/static swapped, the length filter off by
one, the `if` body inverted, a one-item loop, and object member lookup missing.

## Recorded gaps (next slices)

1. `SliceExpression` — blocks Mistral-Small and LFM2.
2. `Ternary` — blocks gemma-4.
3. `Comment` — blocks SmolLM3.
4. `ObjectLiteral` in the fixture encoder — found while adding a `{...}` case.
5. `common_chat_templates_init` can abort (not throw) on some templates, so the
   harness must not assume every template is safe to load.

## Not ported

- The typed-content direction, grammars/autoparser, string marking/caps,
  `strftime_now`, model parsers and the legacy `use_jinja=false` path.
