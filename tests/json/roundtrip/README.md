# roundtrip

**Requirement:** P10-07 (replace or justify vendored executable dependencies) - the JSON parser
and dumper of `common_json`.
**Produced by:** `tools/json-roundtrip-check.sh`; entered the permanent tree on 2026-11-19.
**Tier:** fast - no model, no GPU. Needs the built library; skips (exit 77) without it.

## What it proves

The parser and the dumper cannot share one composed unit (they carry duplicate constants), so this
is the check that exercises both together through the shipped library: parse a text, dump the tree
compactly, require exactly the canonical bytes.

| input | expected dump |
|-------|---------------|
| `{"a":1,"b":[true,null]}` | identical |
| `{"z":1,"a":2}` | identical - key order is preserved |
| `[1,2,3]` | identical |
| `{"s":"x y","n":-1.5e3}` | `{"s":"x y","n":-1500.0}` - numbers normalise the way nlohmann normalises them |
| `null` | identical |

Key order is a promise `common_json` makes to its callers, which is why the expectation is the
input itself for those cases. The one case where it is not is a number that has to be normalised;
the first version of this table compared that against the input and failed on its own assumption.

## What it does not do

It does not compare against a second implementation. P10-07 still needs the adapter between the
Goose parser's parallel arrays and the `common_json` pimpl tree, plus the shadow comparison with
nlohmann at `common/json.cpp:205`. This test pins the current behaviour so that the adapter can be
judged against it.

## Run

```
bash tests/json/roundtrip/run.sh
```

Expected: `json roundtrip: cases=5 failures=0`, exit 0. The log records the library path, its
SHA-256 and the source revision.
