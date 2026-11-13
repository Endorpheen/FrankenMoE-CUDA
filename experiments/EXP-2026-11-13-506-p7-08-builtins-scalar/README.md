# EXP506: scalar Jinja built-ins in Goose (P7-08 fifth slice, part 1)

The first part of the built-ins slice: `func_args` semantics and the int/float
tables. Strings, arrays, objects, bool/none/undefined and `global_builtins`
remain for the next parts.

## What was ported

`source/builtins.goose` (the EXP504/505 value model and JSON writer, plus):

| piece | rule |
|---|---|
| `func_args` | positional access with and without a default, keyword lookup, keyword-or-position fallback, argument count |
| `ensure_count` | `Function '<name>' expected between <min> and <max> arguments, got <n>` |
| `ensure_val` | `Function '<name>' expected value of type <mangled>, got <Type>` (the shared_ptr names the native tables pass) |
| int table | `default`, `abs`, `int`, `float`, `safe`, `string`, `tojson` |
| float table | `default`, `abs` (keeps `-0.0`), `int` (truncates), `float`, `safe`/`string` (Python `float_repr`), `tojson` |
| `tojson` | `indent`, `separators`, `sort_keys` (rejected as `NotImplemented`), `ensure_ascii` with the surrogate-pair escape |
| results | every result is serialized through the ported JSON writer and compared with native `value_to_json` |

Keyword arguments are supplied separately and are not positional, which is how
Jinja calls them; argument counts include them, as native `args.size()` does.

## Gate

`tests/builtins_fixture.cpp` invokes the native handler from
`get_builtins()` with `func_args` and compares the JSON of the result, or the
exact exception text, with the port. It also compares the full built-in name
list per type.

| corpus | cases |
|---|---|
| int table: every function, the `default` boolean branch, `abs` edges and wrong types, `tojson` with indent/separators/sort_keys, container arguments | 30 |
| float table: `abs` on `-0.0`, truncation toward zero, `float_repr` for `1.0`/`1e16`/`0.1`/`-0.0`/`nan`/`inf`, `tojson`, `sort_keys` | 19 |
| name lists for both tables | 2 |
| total | **51 cases, 100 values, 0 failures** |
| final gate | `-O2`, `cases=51 values=100 failures=0` |
| mutations | **14/14** (abs signs both types, truncation rounding/flooring, int-to-float type, float string JSON vs repr, default boolean branch and fallback, tojson indent/separator/sort_keys, count/type error text, name list) |

The mutation campaign takes 4 min 19 s on the optimized path. ASan+UBSan is
deferred: instrumenting the full generated module is hours of compile time and
does not affect this correctness conclusion.

## Build loop

`build_fixture.sh [mutate|final]` now separates the two modes:

| mode | generated C | fixture object |
|---|---|---|
| `mutate` | `gcc -O0` | reused while `builtins_fixture.cpp` is unchanged |
| `final` | `gcc -O2` | the gate that must pass before closing |

Measured before/after on the same machine:

| step | before | mutate (cached) |
|---|---|---|
| goose | 12.3 s | 12.1 s |
| gcc | 231.8 s (`-O2`) | 5.8 s (`-O0`) |
| fixture | 2.5 s | 0.004 s (cached) |
| link | 0.09 s | 0.08 s |
| run | 0.05 s | 0.05 s |
| **total** | **246.8 s** | **18.1 s** |

That is 13.7x for the mutation loop; the compiler was the bottleneck, not the
generator. The final `-O2` gate still costs ~242 s and is run once.

## Scope

P7-08 stays PARTIAL. The built-ins slice is half done: bool/none/undefined,
string, array/object tables and `global_builtins` are still native, then the
runtime and the chat layer.
