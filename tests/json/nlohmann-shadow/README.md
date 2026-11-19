# nlohmann-shadow

**Requirement:** P10-07 - the shadow comparison that must precede any swap of the JSON parser.
**Produced by:** the P10-07 state note of 2026-11-19; this is its executable half.
**Tier:** fast - one translation unit, no model, no GPU, seconds.

## What it proves

For fourteen texts, the Goose parser and dumper in the shipped library produce **exactly** the bytes
nlohmann's `ordered_json` produces for the same input. Both implementations sit in one binary:
nlohmann from the vendored header-only library, the Goose side through the shipped library.

That is what catches the two promises `common_json` makes to its callers - key order, and number
normalisation (`-1.5e3` printing as `-1500.0`).

Covered shapes: nested objects and arrays, key order that differs from alphabetical, unicode escapes
and an escaped quote, a 13-digit integer and a negative, floats in three spellings, `null`, empty
object and array, and a text with surrounding whitespace.

## What it does not prove

That the tree is built correctly from the arrays. That is the adapter, and it does not exist yet -
this test compares the two dumps, which is the part that can be compared today. It also does not
prove equality on invalid input: `json/door` covers refusal, this covers equality of valid parses.

## Run

```
bash tests/json/nlohmann-shadow/run.sh            # builds/flint-acc/bin
bash tests/json/nlohmann-shadow/run.sh <lib-dir>  # a release set
```

Expected: `json shadow: texts=14 compared=14 failures=0`, exit 0. Exit 77 without a library.
