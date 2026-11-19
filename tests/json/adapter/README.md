# adapter

**Requirement:** P10-07 - the adapter between the Goose parser's parallel arrays and the
`common_json` pimpl tree, which the requirement lists as the step before any swap.
**Produced by:** the P10-07 state note of 2026-11-19; this is the adapter and its gate. The adapter
now lives in `vendor/flint/common/json-adapter.cpp` and the test links that same translation unit,
so the code under test is the code the runtime would use.
**Tier:** fast - two translation units, no model, no GPU, a few seconds.

## What it proves

The full path works and matches the reference: parse a text with the Goose door, turn the flat
arrays into a `common_json` tree (`vendor/flint/common/json-adapter.cpp`), dump that tree with `common_json` itself, and
compare the bytes with nlohmann's ordered dump of the same text.

Result: `texts=11 compared=11 failures=0`, over nested objects and arrays, key order against
alphabetical, negative and 13-digit integers, floats in three spellings, `null`, and empty
containers.

The first run of this test **failed**, which is the reason it exists: the adapter read floats from
the wrong array (`jn` instead of `jd`) and every float came out as `0.0`. The dumper-only comparison
in `tests/json/nlohmann-shadow` could not have caught that - it exercises the Goose dumper, not the
tree bridge.

## What it does not prove

That the tree can be injected into `common_json`'s own parse path and the swap performed. The
adapter builds a tree; the consumer still calls nlohmann. The adapter targets `nlohmann::ordered_json`
directly, because that is what `common_json` holds internally - so the swap at
`common/json.cpp:205` is an assignment, not a conversion, and still needs its own build gate.

It also does not cover invalid input: `tests/json/door` covers refusal.

## Run

```
bash tests/json/adapter/run.sh            # builds/flint-acc/bin
bash tests/json/adapter/run.sh <lib-dir>  # a release set
```

Expected: `json adapter shadow: texts=11 compared=11 failures=0`, exit 0. Exit 77 without a library.
