# P10-07b — replacing nlohmann::json in one consumer (staged, differentially gated)

Date: 2026-11-15
Base commit of the repository: `d22d3c4b6d0f776c6c3a804eafcf29850258822a`
Work directory: `/tmp/opencode/sub-665/EXP-2026-11-15-665-p10-07b-json-consumer`
The repository was not modified and nothing was committed. No GPU, no model,
no server, no production build was started.

---

## 1. Goal and result

Goal: switch **one** consumer of `nlohmann::json` to the accepted Goose parser
behind a build flag, with a differential gate against nlohmann itself on a
corpus that exercises fields, types, nesting and errors.

Result: done for `tests/gguf-model-data.cpp::detect_gguf_filename()` — the only
direct `nlohmann::json` consumer in the tree besides the `common_json` pimpl.

| check | result |
|---|---|
| OFF build (`-DFG_GOOSE_JSON_REPLACE=OFF`, default) | built |
| ON build (`-DFG_GOOSE_JSON_REPLACE=ON`) | built |
| differential gate, 20 documents, OFF `-O1` | 20/20, 0 failures |
| differential gate, 20 documents, ON `-O1` | 20/20, 0 native-vs-goose differences |
| OFF vs ON transcript | byte-identical, 20 lines, SHA-256 `57823e88…` both |
| same at `-O0` | identical, 0 differences |
| `goose_init` / `json_parse` definitions | 1 / 1 in ON, 0 / 0 in OFF |
| native body (`pick_with<nlohmann::json>`) | present in both |
| Goose body (`pick_with<goose_json::json>`) | present in ON, absent in OFF |
| mutations | 12/12 (6 parser + 6 adapter), all detected |

## 2. Why this consumer

`docs/goose-migration/P10-07-COVERAGE.md` and EXP-2026-11-15-651 record the
inventory: `nlohmann/json` 3.12.0 is MIT, floating-pinned, and classified
**replace** because `goose/json/json-parser.goose` already reproduces
`common_json::parse`. The audit lists its consumers as `common/json.cpp`,
`tests/gguf-model-data.cpp`, linked by `llama-common`.

`common/json.cpp` is the pimpl that *owns* nlohmann for the whole tree:
changing it would switch every consumer at once, which the requirement
("one consumer") does not ask for. `tests/gguf-model-data.cpp` is a direct
consumer with clean JSON logic — parse a body, test a key, require an array,
walk it, read one string — and it is built by the production profile
(`LLAMA_BUILD_TESTS=ON` in `build/expert-tier-franken-cuda/CMakeCache.txt`,
EXP-651 finding 5).

## 3. What was replaced and what was added

The JSON half of `detect_gguf_filename()` (reference lines 457-514) is
transcribed into `source/hf-pick.cpp` as a single body:

```
bool/parse  -> J::parse(body.begin(), body.end())        (throw -> code 1)
contains    -> j.contains("siblings")                    (absent -> code 2)
is_array    -> j["siblings"].is_array()                  (false  -> code 2)
iterate     -> for (const auto & sibling : j["siblings"])
contains    -> sibling.contains("rfilename")             (absent -> skip)
get<string> -> sibling["rfilename"].template get<std::string>()  (throw -> code 4)
```

It is instantiated twice: `pick_with<nlohmann::json>` (always compiled, the
oracle) and `pick_with<goose_json::json>` (ON only, the dispatched path). The
one syntactic difference from the original is `.template` before `get<T>()`,
required because the body became a template; the semantics are unchanged. The
`std::string`, `std::toupper`, `std::sort` and split-prefix logic is copied
verbatim (nine anchors are checked by `tests/anchors.py` against the reference).

New host code:

- `source/goose-json-adapter.h/.cpp` — the seam. It calls `json_parse`, copies
  the flat table (`jk`, `jv`, `jd`, `js`, `jl`, `jf`, `jn`, `jchild`, `jkey`,
  `jkl`, `sbuf`) into a document view and implements only the API subset above.
  It makes no JSON decision: every parse rule (RFC 8259 grammar, number
  classification, escapes, duplicate-key policy, error class and position)
  stays inside the accepted Goose module and is not duplicated.
- `source/hf-pick.h/.cpp` — the transcribed body, the dispatch, the call
  counters.
- `source/json-parser.goose` — byte-identical copy of the canon module
  (`ee56f4e4…`, the same SHA-256 EXP-610 records), because Goose resolves
  imports by sibling file name.

The flag is `FG_GOOSE_JSON_REPLACE`, OFF by default: the canon path is
nlohmann, the new path is opt-in for this stage. The native body is compiled in
both builds, so no configuration can lose the reference implementation.

## 4. Corpus

20 documents (`tests/cases/`, one line per document in `tests/cases.tsv`):

| class | count | documents |
|---|---:|---|
| success | 10 | basic, split+mmproj, escapes, raw UTF-8, duplicate keys, lowercase quant/unordered keys, extreme numbers, deep nesting, last-slot match, tabs/CRLF |
| parse error | 3 | truncated object, trailing garbage, empty body |
| format error | 3 | no `siblings`, `siblings` is an object, array root |
| no match | 3 | empty array, wrong quant, `\u0000` in the name |
| type error | 1 | `rfilename` is a number |

Fields compared per document: `error`, `chosen`, `split_prefix`, `matches[]`
(full sorted list) — 80 values over the corpus, 0 differences.

## 5. Gate

```
bash tests/build.sh all
# run off rc=0: FG665_GATE mode=native cases=20 failures=0 native_vs_goose_diffs=0 native_calls=20 goose_calls=0
# run on  rc=0: FG665_GATE mode=goose  cases=20 failures=0 native_vs_goose_diffs=0 native_calls=20 goose_calls=20
# switch off/on transcript identical: yes (20 lines)
```

`native_calls=20` in the ON build is the oracle; `goose_calls=20` is the
dispatch, so an ON build cannot pass by silently keeping the native path. The
gate compares in-process and also across binaries (`dump-off.txt` vs
`dump-on.txt`, byte-identical).

Symbol evidence (`evidence/nm-symbols.txt`):

```
off goose_init=0 json_parse=0 hf_pick_goose=0 pick_goose=0 pick_nlohmann=1 hf_pick_native=1
on  goose_init=1 json_parse=1 hf_pick_goose=1 pick_goose=1 pick_nlohmann=1 hf_pick_native=1
```

## 6. Mutations

`python3 tests/mutate.py 1` and `... 2`, six each, rebuilt at `-O0`:

| # | target | mutation | detected by |
|---|---|---|---|
| P1 | parser | duplicate key appended instead of replaced | 09-dupkeys |
| P2 | parser | trailing garbage accepted | 14-trailing |
| P3 | parser | `\uXXXX` written as one byte, not UTF-8 | 07-escapes |
| P4 | parser | string length one short | 01-basic |
| P5 | parser | newline is not whitespace | 19-tabs |
| P6 | parser | last child of every container dropped | 01-basic |
| A1 | adapter | `is_array()` also true for objects | 04-siblings-object |
| A2 | adapter | `operator[](key)` returns the first child | 01-basic |
| A3 | adapter | array iterator skips the first element | 02-split |
| A4 | adapter | `get<string>` returns empty on a wrong type | 16-rfilename-number |
| A5 | adapter | string offset one byte late | 01-basic |
| A6 | adapter | parse error becomes a null document | 13-bad-json |

Half 1: 6/6. Half 2: 6/6. No mutation survived the gate.

## 7. SHA-256

Inputs and artefacts (`evidence/hashes.txt` is the full list):

| file | sha256 |
|---|---|
| `source/json-parser.goose` | `ee56f4e479dd05b65f57589ca4916767d923d13e73187087a95ab27416bfd70e` |
| `source/goose-json-adapter.h` | `54677ea23c16c2183507bb06bad5c423654fda7af6700e2d2326aa567630e122` |
| `source/goose-json-adapter.cpp` | `0986eaaf1c61fb63b8edd22fc7fdb57cca980428c0d478e6b421aba4f83c46dd` |
| `source/hf-pick.h` | `03e8e859bad2216a3c97361a13390b35ad2f66ce792b77e6a0cc0c7876d5e13b` |
| `source/hf-pick.cpp` | `372208e544fe4a020aecb69d167620bd7cb56370d6350d4021999ddc35035b6a` |
| `tests/fixture.cpp` | `e657d1a69fbb03dbbbd5ce3f80bdbf42499bc4a5750d7919a63e88424e27842c` |
| `tests/cases.tsv` | `4f2a7cafbbf68b4244feb983d90563a67e36a82739206099e3daca8d17237c7e` |
| `reference/gguf-model-data.cpp` | `ed9d2374393680e17ab8d528821e0991bc8092cbbc8704a450f0cf765ec8889a` |
| vendor `nlohmann/json.hpp` | `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` |
| canon `goose/json/json-parser.goose` | `ee56f4e479dd05b65f57589ca4916767d923d13e73187087a95ab27416bfd70e` |
| `evidence/dump-off.txt` | `57823e88a4835a4477aecfcbf468e5a5d725f1b1628292965edd5dbada3a1907` |
| `evidence/dump-on.txt` | `57823e88a4835a4477aecfcbf468e5a5d725f1b1628292965edd5dbada3a1907` |

Environment: `gcc/g++ 13.4.0`, goose `goose-2026.3-string`, kernel
`7.0.0-43-generic`, 32 cores.

## 8. Boundaries and remaining nlohmann

1. **The pimpl stays nlohmann.** `common/json.cpp` is unchanged; every other
   consumer (server schema, models, chat, tools, GGUF metadata) reaches JSON
   through `common_json` and is deliberately out of this stage.
2. **The shipped file is unchanged.** This is a staged trial in an isolated
   directory; enabling `FG_GOOSE_JSON_REPLACE` in the canon and removing the
   nlohmann link from `tests/gguf-model-data.cpp` is the lead's integration
   step, not this experiment's.
3. **Parse and navigate only.** The consumer never serializes, so `json-dump`
   is not exercised here; the dump path keeps nlohmann.
4. **The adapter is host code.** It converts the flat table into a document
   view; that view is a data structure, not a JSON decision. The corpus proves
   the observable behaviour, not that every nlohmann feature is covered: only
   the seven calls listed in §3 are implemented, and a future consumer with a
   wider API needs the adapter extended.
5. **Error text is not compared.** The gate compares the error class (parse,
   format, no match, type), not nlohmann's `what()` text; the original code
   prints `fprintf` messages that the gate replaces with codes.
6. **`get<std::string>()` on a non-string** ends the original function through
   an uncaught exception; the gate maps both throws to `err=4`, which is the
   same observable outcome but not the same unwinding path.
7. **Pin drift.** `nlohmann/json.hpp` is still the floating `releases/latest`
   download recorded as risk 1 in EXP-651; this stage does not change the pin.

## 9. Reproduce

```
cd /tmp/opencode/sub-665/EXP-2026-11-15-665-p10-07b-json-consumer
python3 tests/gen_cases.py          # 20 documents (already present)
bash tests/build.sh all             # OFF + ON at -O1, diff, nm
bash tests/build.sh all mutate      # the same at -O0
python3 tests/mutate.py 1           # 6/6
python3 tests/mutate.py 2           # 6/6
python3 tests/anchors.py            # 9/9
```

`source/json-parser-gen.c/.h` and every `build-*` directory are generated and
must not be committed; they are reproducible from `source/json-parser.goose`
plus `tests/build.sh`.
