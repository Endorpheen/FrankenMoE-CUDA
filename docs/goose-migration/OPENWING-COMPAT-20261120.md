# OpenWing vs 2026.3-string: what actually blocks the pipeline

Measured 2026-11-20 on the real composed unit (156 modules, 3 814 008 bytes) and on three minimal
reproductions. Every claim below has a command and an output; nothing here is inferred from a flag name.

Pinned: `/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose`
OpenWing: `/home/end0/AI/Fried-Goose/releases/fried-goose-v0.2.0-preview-openwing/bin/goose`
(sha256 `f7942c3613b4aaf1…`)

## Not a blocker: `--standalone`

The first version of this note called the absent `--standalone` an incompatibility. That was wrong, and the
source says why:

- `compiler/context.goose:897`: `ctx.library_build = compiler_header_path.len > 0;` - **`--header` selects
  library mode**;
- `compiler/emitter.goose:4359-4372`: the runtime, globals and math/bits/float runtimes are appended to
  **every** output, library or program;
- `compiler/emitter.goose:4432-4442`: library mode emits `void goose_init(int, char**)` and calls the
  module's `main` **from inside it**; program mode emits `int main`.

Verified end to end: `goose --header lib.h -o lib.c lib.goose` produces a header with an `extern "C"` block,
`goose_init` and `int64_t fg_add(int64_t, int64_t)`, and the object exports `T fg_add` and `T goose_init`.
That is exactly what the pair-building path needs, so a missing `--standalone` is not a blocker.

## Blocker 1: a float literal does not adapt to the other operand's `f32`

The specification (`docs/goose_spec.md:1890-1902`) says a float literal has **no committed type** and "adapts
to `f32` or `f64`, whichever its destination **or other operand** has".

Reproduction (`/tmp/repro/r6.goose`):

```goose
fn f() -> f32 {
    var pp = 0 as f32 - 0.875;
    return pp;
}
fn main() { let r = f(); }
```

| compiler | result |
|---|---|
| `2026.3-string` | `typechecked ok: 2 specialization(s)` |
| OpenWing | `r6.goose:3: error: incompatible types (type 3 kind 2 width 8 -> 1 kind 2 width 4)` at `return pp;` |

The literal is being committed to `f64`, the expression becomes `f64`, and the `f32` return rejects it. The
other operand is `f32`, so per the spec the literal should have adapted. Adding a type annotation
(`var pp: f32 = …`) makes both compilers accept it, which confirms the difference is in adaptation, not in
conversion rules.

On the real unit this is the first error after renaming identifiers:
`u2.goose:7610: error: incompatible types (type 1 kind 2 width 8 -> 56 kind 2 width 4)` at
`sx0 += w * pp * xv2;`, where `sx0`, `w`, `pp`, `xv2` are all `f32` by declaration.

## Blocker 2: `from` cannot be used as an identifier

`compiler/parser.goose:557`: `if tag == N_RETURN && take(ctx, "from")` consumes `from` after `return` as the
start of the `return from` construct (§7.9), then demands a name.

Reproduction (`/tmp/repro/r1_from.goose`):

```goose
fn find_from(from: i64) -> i64 {
    if from > 3 { return from; }
    return 0;
}
fn main() -> i64 { return find_from(5); }
```

| compiler | result |
|---|---|
| `2026.3-string` | checks (it has no `return from` construct) |
| OpenWing | `expected identifier` at `return from;` |

30 of the 156 unit modules use `from` as a name (474 occurrences in `goose/`). We can rename them, and will,
but the ambiguity is worth closing: `return from;` where `from` is a live variable is not decidable from the
token alone; a lookahead for an identifier after `from` would settle it.

## Blocker 3: `stdlib` is resolved relative to the working directory, not to the compiler

`compiler/main.goose:117`: `load_program(ctx, path, "stdlib")` - a relative path.

Reproduction:

```
mkdir /tmp/imp && cd /tmp/imp
printf 'import math;\nfn main() { let x = sqrt(4.0); }\n' > imp.goose
/path/to/openwing/bin/goose --check imp.goose      # error: cannot find import math.goose
cd /path/to/openwing && ./bin/goose --check /tmp/imp/imp.goose   # parses, no import error
```

The priced workaround is copying `stdlib/*.goose` next to the source; the durable fix is resolving `stdlib`
next to the executable (or an environment variable).

## What is still unverified, and what it would take

The unit cannot be typechecked past these two source-level issues, so further OpenWing differences may
exist behind them. The order that answers it cheaply:

1. fix Blocker 1 in the compiler (or confirm that explicit annotations are the intended migration, in which
   case we annotate and re-run);
2. we rename `from` in the 30 unit modules (mechanical, no semantic change);
3. re-run `--check` on the composed unit and iterate to green;
4. then the real gates: `-o` + `--header` on the unit, `gcc -c`, symbol comparison against the pinned
   compiler's output, and finally the Apex four-axis run - correctness, prefill, decode, VRAM - because a
   compiler change that costs speed is not a win.

Until step 3 is green, no claim is made about whether OpenWing can build the unit.
