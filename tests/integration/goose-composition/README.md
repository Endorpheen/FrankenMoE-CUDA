# goose-composition

**Requirement:** P6-10, P7-06, P8-09 and every stage that shares the composed unit - the check is
about the unit as a whole.
**Produced by:** `tools/check-composition.sh`, which owns the set list; this is its permanent,
logged entry in the test tree.
**Tier:** integration - no model, no GPU, needs the Goose compiler.

## What it proves

Nine canonical module sets still compose and typecheck together: the server set, the quant set,
the GGUF set, the CLI/chat set, the jinja-schema set, the application set, the core set, the core2
set and the full set. A module that stops fitting its neighbours - a duplicate global, a changed
signature, an import that no longer resolves - fails here in seconds, before any build.

It is not a duplicate of `tools/check.sh fast`: that level composes the single cumulative unit and
compiles it, and skips when its inputs are unchanged. This test composes the *individual* sets, so
it names which set broke instead of only that the unit did.

## Run

```
bash tests/integration/goose-composition/run.sh
```

Expected: `composition: 9 set(s) ok`, exit 0. The log records the compiler path, its SHA-256 and
the source revision, because a composition result is only meaningful for the compiler that ran it.

Exit 77 when no compiler is available.
