# library-provenance

**Requirement:** rule 22 - accepted executable sets are immutable; a new build lives in its own
directory.
**Tier:** fast - a few `sha256sum` calls.

## Why it exists

On 2026-11-19 `builds/flint-acc/bin/libllama.so.0` changed at 19:18, after the 19:00 checkpoint, and
no script under `tools/` writes there. Its provenance is unknown. Four JSON tests had been passing
against that file all day, and against the released artefact they cannot pass at all: the released
library exports no `json_parse`.

A rule that says "do not overwrite accepted sets" cannot detect that by itself. This does: it prints
the working library's hash and date next to every release set and checkpoint, and says plainly when
the working file matches none of them.

## What it does not do

It does not fail on a mismatch. A working directory is allowed to hold a newer build - that is what a
working directory is for. What it refuses to do is let the mismatch stay invisible.

## Run

```
bash tests/regression/library-provenance/run.sh
```

Exit 0 always (77 when there is nothing to compare), with the table in `build/run.log`.
