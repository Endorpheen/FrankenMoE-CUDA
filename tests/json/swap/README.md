# swap

**Requirement:** P10-07 - the swap of `common_json::parse` onto the Goose parser.
**Produced by:** the scenario built on 2026-11-19 when the adapter was proven.
**Tier:** fast - two translation units, no model, no GPU, a few seconds.

## What it proves, in three modes

1. **flag off** - `common_json::parse` still produces exactly nlohmann's bytes. The new path cannot
   have broken the old one, which is what a swap must show first.
2. **flag on, strict** - with `FG_JSON_GOOSE_PARSE=1 FG_JSON_GOOSE_REQUIRE=1` every parse must come
   from the Goose port: strict mode turns a decline into an error instead of a silent fallback, so a
   passing run is evidence that the port answered rather than that the fallback did. Seven texts,
   byte-identical to nlohmann.
3. **flag on, strict, no door** - the same run against a library without `json_parse` must refuse
   every text. This is the control: without it, mode 2 could pass for the wrong reason.

## Why strict mode exists

The swap falls back to nlohmann when the port declines, because a parse the native path accepts must
never be failed by the new one. That is right in production and useless as evidence, so the second
flag makes the fallback countable: `FG_JSON_GOOSE_REQUIRE=1` refuses to hide a decline.

## What it does not prove

That a production library built with the swap behaves the same under load. This test compiles
`json.cpp` and the adapter itself and links the shipped `libllama` for the door; the moment
`libllama-common` is rebuilt with the swap, the same three modes run against the built library and
that becomes the acceptance, not before.

## Run

```
bash tests/json/swap/run.sh            # builds/flint-acc/bin
bash tests/json/swap/run.sh <lib-dir>  # a release set
```
