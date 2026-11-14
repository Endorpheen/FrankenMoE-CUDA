# FIX — FG573 schema integration gate

Date: 2026-11-14
Base: commit `e1238277`
Work directory: `/tmp/opencode/sub-573/`
The repository was not touched; every change was made in `/tmp`. Builds ran one
at a time.

## 1. What was wrong

1. The fixture called `convert(...)` with 23 arguments, including `idx`. The
   integrated export is `json_schema_convert` and takes 22 arguments:
   `root, meta, props, vals, strs, sdir, koff, klen, toff, tlen, buf, st, opt,
   kvoff, kvlen, choff, chlen, sbuf, pwork, mprops, rref, out`. `idx` is no
   longer needed: the canonical `grammar_text` does the sorting.
2. The first run against the old `llama.cpp-flint-cuda` produced a wrong oracle
   (`{}` -> `value`, `items:{}` -> `value`). Base `e1238277` in
   `upstream/llama.cpp` gives the correct `object`/`item`. `build.sh` now
   defaults to `FG_LLAMA=$REPO/upstream/llama.cpp`.

After the signature was fixed the gate exposed six defects of the integrated
file itself:

| # | defect | location | symptom |
|---:|---|---|---|
| 1 | `app` returns the record end, but `ensure_primitive`/`space`/`dot` used it as a length | converter, `ensure_primitive`, start of `json_schema_convert`, `dot` | `tlen` was an end offset; NUL inside rules, `string0`/`string1` |
| 2 | pattern wrapper read 7 bytes from a 6-byte literal | pattern, `"\"\\\"\" ("` | `?` before the first literal, runtime out-of-bounds |
| 3 | literal was not flushed on break in a literal run | pattern | a repeated character produced `unsupported character`, fallback to `string` |
| 4 | `_not_strings` kept reject characters in one slot for all depths | trie | `also` lost `l`; `additional-k` did not match |
| 5 | allOf enum did not move the cursor past `(` | converter, enum intersection | rule of quotes instead of `("\"b\"" | "\"c\"")` |
| 6 | the fixture pushed the id after recursion | `tests/fixture.cpp`, `vals` | a nested `anyOf` consumed an id: `c-kv` was lost |

All six were fixed minimally; native behaviour was not changed.

## 2. Gate

Fixture: `tests/fixture.cpp`, 30 schemas from `cases.md`; oracle:
`json_schema_to_grammar` from `upstream/llama.cpp` at `e1238277`; comparison of
the full rule map.

```
$ bash tests/build.sh
FG573_SCHEMA cases=30 values=182 failures=0
```

`values=182` is the total number of oracle rules over the 30 schemas.

## 3. Mutations

`tests/mutate.py`, 12 mutations in two halves of 6; each rebuilds the converter
and reruns the fixture.

```
$ python3 tests/mutate.py --half first
first 6/6 caught
$ python3 tests/mutate.py --half second
second 6/6 caught
```

| # | mutation | result |
|---:|---|---|
| 0 | identical rule text is not reused | caught, 8 failures |
| 1 | primitive text length is an end offset | caught, 22 |
| 2 | space text is one byte too long | caught, 30 |
| 3 | pattern wrapper reads 7 of 6 bytes | caught, runtime out-of-bounds |
| 4 | literal is not flushed on break | caught, 3 |
| 5 | trie reject slot is shared across depths | caught, 1 |
| 6 | allOf enum takes the complement of the intersection | caught, 1 |
| 7 | allOf anyOf alternatives become required | caught, 1 |
| 8 | union loses `alternative-` | caught, 2 |
| 9 | tuple separator loses a byte | caught, 2 |
| 10 | time format uses `date-string` | caught, 1 |
| 11 | integer bounds are swapped | caught, 1 |

Total: **12/12**, 6/6 + 6/6.

## 4. `goose check` and exports

```
$ goose check source/format-grammar.goose
typechecked ok: 264 specialization(s), 0 struct/0 enum instance(s);
optimized -O1: 19 inlined, 0 base case(s), 84 folded, 686 propagated,
3 tail loop(s), 0 reassociated
```

Exports: canon plus 1 new, 5 total.

```
schema_build
grammar_text
conversion_failed
conversion_warning
json_schema_convert
```

## 5. Remaining work and risks

1. The integration file was copied into the repository as
   `source/format-grammar.goose`; the `/tmp` copy is no longer authoritative.
2. `format` and `prefixItems` are covered; `additionalProperties` covers all four
   modes; only the oracle limits described in REPORT5 remain.
3. Mutation 3 (pattern wrapper) is caught as a runtime error rather than a diff:
   a valid rejection, but the fixture must count it as a failure.
4. `values=182` depends on the native version; with another base the rule count
   can change while `failures` must stay 0.
5. `build.sh` uses `upstream/llama.cpp` and `build/expert-tier-franken-cuda` by
   default; set `FG_LLAMA` and `FG_BIN` for another base.

## 6. SHA-256

| file | SHA-256 |
|---|---|
| `source/format-grammar.goose` | `a5c66d60de35378b4378bfb5e65f13f94f5f89551aa33020bdd903c460dfa3f2` |
| `tests/fixture.cpp` | `0c8b397e8e783149fa1e9ada0c9a112973c630e9e9b2a2cd9875ba81b3fc71f8` |
| `tests/build.sh` | `88c47e4edd19ad0a93aada945f7dcb0b75d669819ec322b426fb65d1097ea973` |
| `tests/mutate.py` | `8084dac72261a1195adbeb1f66071431751f1543a72e8c415771a9b4283f46d9` |

`FIX.md` has no hash of its own: it would change when the hash is inserted.
