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

## Update 2026-11-20: v0.2.0-preview-openwing, and it cannot replace the pinned compiler yet

The compiler agent reports `fried-goose-v0.2.0-preview-openwing`, sha256 `f7942c3613b4aaf1…`, with
`export fn` restored. **Verified here**, not taken on trust:

| capability | new compiler | pinned `2026.3-string` |
|---|---|---|
| `export fn` | **yes** - `export fn fg_add(a: i64, b: i64) -> i64` compiled | yes |
| `--header` | **yes** - emits `extern "C"` block, `goose_init`, `int64_t fg_add(int64_t, int64_t)` | yes |
| `-o out.c` | yes (45 KB for the sample) | yes |
| **`--standalone`** | **no** - the usage line lists `--tokens --parse --check --packed -o --emit-provenance` only | yes |

That last row is decisive for this repository: every build path here composes a **156-module unit** and
compiles it with `--standalone`. A compiler without it cannot build the unit, whatever else it improves.

So the decision recorded here is: **keep `2026.3-string` pinned for the current pipeline**, and treat the new
line as the target for a future flow that builds modules individually with `-o` + `--header` rather than as
one standalone unit. Adopting it is a pipeline migration with its own gates, not a version bump.

What the new line brings that is worth planning for: a clean C header with `extern "C"` and the exported
prototype (no hand-written declaration - the failure mode this repository hit when a bridge re-declared an
18-argument export), and a safety contract enforced on the export surface.

## The one module it could not build, and why it matters here

The compiler agent reports `goose/sampling/sampler-chain-full.goose` blocked: recursive argument broadening
of reference provenance. That is the module this repository found unused from production on 2026-11-19. Two
independent facts now agree that it is not a drop-in library: the unit-level analysis says nothing calls it,
and the compiler's safety contract refuses its recursion. Neither is a reason to weaken the contract.
