# EXP-2026-11-19-1324: 12 of 261 ported modules reach the shipped library

## What was measured

`tools/check-module-coverage.py` reads every `goose/**/*.goose` outside the archive and staged copies, collects its
`export fn` names, and checks which of them are defined in the shipped libraries:

```
modules with all doors in the library:  12
modules with some doors:                 0
modules with no doors in the library:  249
shipped symbols:                      11805
```

The twelve that reach it are the ones this session and the accepted baseline actually depend on: the metrics module,
the sampler chain, the spec decisions and doors, the architecture registry, the tokenizer, detokenizer, unicode
tables and text, the tokenizer doors, the pretokenizer, and the config validator.

## Why the other 249 are not a defect

They are not shipping: the Jinja runtime, tool builders and streaming modules belong to paths this runtime does not
execute - the server builds prompts with its own chat templates, and the Jinja port is a library for another target.
Counting them as missing would be as wrong as counting them as done.

What the number does say is that "149 modules composed" is not coverage, and neither is "261 modules ported". Coverage
is the twelve, plus whatever else gets a call path.

## Why it belongs in the record now

Rule 33 asks for one growing build and for checks against the artefact. The GGUF reader turned out to be unconnected
in exactly the way the detokenizer was - ported, verified, called by nothing - and a number that answers "which
modules have doors in the binary" is what stops that being discovered one module at a time.

## Next step

Extend the tool to also report whether a door is **called**, not merely defined: a symbol that appears in no call
site is a port waiting for a path. That distinction is the one the migration keeps needing.
