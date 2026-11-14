# EXP533: for-over-string and select-on-string caps probes (P7-08 slice 5, part 23)

## Correction to the previous card

EXP532 recorded two "runtime gaps": iterating a string in `for` and `select` on a
string. Reading the native code showed that **native rejects both too**:

- `for_statement::execute_impl` throws `Expected iterable or object type in for
  loop: got <Type>` for a string, exactly like our runtime;
- `select`/`selectattr` requires a value array and throws for a string.

The real difference was elsewhere: caps.cpp marks `array_access` on the for
iterable **before** the type check, and the caps harness treats a failed render
as `success = false` instead of an error. Our gate had been marking the template
as a failure and skipping the flag computation, so the two probe templates could
not be compared.

## What changed

| change | rule |
|---|---|
| `exec_for` | marks the iterable with the array-access bit before the kind dispatch, mirroring the native order, so a rejected string iteration still leaves the op mark |
| caps mode | `check_tpl` gained a mode that runs the render, tolerates a rejection, reads the content mask and reports `failed` instead of comparing text |
| content flags | computed as caps.cpp does: `typed = array_access`, `string = !failed && !(array_access && marker missing)` |

## Gate

```
FG533_RUNTIME_SEQ cases=404 values=780 failures=0
mutations 6/6 caught (range 0:6)
mutations 6/6 caught (range 6:12)
baseline after restore: rc=0 cases=404 values=780 failures=0
```

Five content cases now match `original_caps()`: plain content, `content[0]`,
`content|string`, `{% for c in content %}` (rejected render, array-access mark,
typed only) and `content|select(...)` (rejected render, no op, neither flag).

Mutations: `for` stops marking or marks the loop variable, `select` accepts
strings, string indexing stops marking, rendering sets the array bit, the content
mask returns zero / adds the array bit / drops the array bit, the key walk
matches any key or any length, and the walk returns the key mask or the object
mask.

## Not ported

- `supports_tools`/`supports_tool_calls` and their probe context.
- Iterating strings with `for` and `select` on strings remain errors, as they
  are natively.
- Typed content normalization, tools/grammars, the autoparser, string
  marking/caps, strftime_now and model parsers.
