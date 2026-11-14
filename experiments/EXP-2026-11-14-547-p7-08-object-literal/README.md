# EXP547: object literals and new-key assignment (P7-08 slice 5, part 37)

## Hypothesis

EXP545 recorded `ObjectLiteral` as an encoder gap. Checking the runtime showed it
was missing on that side too: `K_OBJECT` existed only as a node name, with no
evaluation. A literal is a pair list, and our objects need all keys contiguous
and all values contiguous, so the build has two phases: evaluate every pair in
order, then write all keys, then all values.

That held, and the new-key test exposed the design constraint that the previous
slice had only papered over.

## Rules captured

| piece | rule |
|---|---|
| literal | evaluate key then value for each pair in source order; up to 16 entries; then write keys, then values |
| existing key assignment | overwrite the value slot in place |
| new key on a named object | rebuild the object with one more pair and rebind the identifier |
| new key on an unnamed, non-empty object | explicit error: the key/value arrays must stay contiguous |
| new key on an empty object (`namespace()`) | in-place append, which keeps both arrays contiguous |

## Gate

```
FG547_RUNTIME_OBJLIT cases=532 values=928 failures=0 skipped=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=532 values=928 failures=0 skipped=0
```

Six literal cases: single key, two keys with `length`, direct `{'x': 5}['x']`,
empty literal, overwrite of an existing key, and adding a new key.

Mutations: entry cap, key/value lists swapped, key slot holding the value, the
key phase storing values, the value phase storing keys, the rebuild copying one
pair less, the appended key slot holding the value, the rebuild dropping the new
key or its value, the binding keeping the old object, the rebuild forgetting the
existing pairs, and the fallback append.

## Documented divergence

The native `value_object::insert` mutates the object in place, so every alias
observes a new key. Our rebuild rebinds only the assignment target: if another
variable referred to the same object, it keeps the old one. This is recorded as a
limitation rather than hidden; for template code that assigns through the
original name the behaviour is identical.

## Not ported

- Non-string literal keys are evaluated as values and compared structurally; the
  native `insert` additionally enforces hashability.
- `strftime_now`, the typed-content direction, grammars/autoparser, string
  marking/caps, model parsers and the legacy `use_jinja=false` path.
