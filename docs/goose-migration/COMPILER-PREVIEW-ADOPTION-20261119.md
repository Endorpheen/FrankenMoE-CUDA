# Adoption of Fried Goose v0.1.9-preview "TrueWing": blocked by the export surface

Tested on 2026-11-19 against `/home/end0/AI/Fried-Goose/releases/fried-goose-v0.1.9-preview-truewing/bin/goose`
(`sha256 0ed4e169…aaca6`). Nothing was rebuilt and the released artifact `v0.10.0-exp1476` is untouched.

## What works

Plain functions typecheck:

```
$ goose --check t.goose          # fn f(x: i64) -> i64 { return x; } + fn main
checked 2 specializations, 5 types
```

## What does not

The runtime is a library of decisions exported to the C++ host. That surface is not in this subset -
every export form is refused:

| form | result |
|------|--------|
| `export fn f(x: i64) -> i64 { … }` | `error: expected top-level declaration` |
| `fn f(x: i64) -> i64 export { … }` | `error: expected '{', got 'export'` |
| `pub fn f(x: i64) -> i64 { … }` | `error: expected top-level declaration` |

The same three errors appear on the real modules, at the first door of each:

```
goose/sampling/sampler-chain-full.goose:693: error: expected top-level declaration   (export fn fg_dry_rep_limit_entry)
goose/model/gguf-reader.goose:62:            error: expected top-level declaration   (export fn gr_parse)
goose/json/json-parser.goose:977:            error: expected top-level declaration   (export fn json_parse)
```

The public `goose v2026.4` compiler, which CI pins, accepts these modules and composes the whole
unit, so this is not a defect of our sources: the preview subset is missing the export mechanism
the runtime depends on.

## Consequence

`runtime/compiler/current.json` stays on `goose-2026.3-string` for builds, and CI stays on the
pinned public `v2026.4`. Adopting the preview needs one of:

1. the export surface restored in the preview compiler (`export fn`, as in 2026.4); or
2. a defined way for a whole-program build to expose named entry points, which the host and the
   doors would then be generated from.

Until one exists, switching the pin would make every module in `goose/` fail to parse, so the pin
is not switched. The untested item is whether the compiler's own documentation defines an
equivalent mechanism under another name; the three forms above are the ones the runtime and the
host generator would use.
