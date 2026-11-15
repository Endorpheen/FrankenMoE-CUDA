# P10-07b — one nlohmann::json consumer behind the Goose parser

Base commit: `d22d3c4b` (the experiment itself lives outside the repository).
Requirement: `docs/goose-migration/tasks.json` P10-07, follow-up of
`experiments/EXP-2026-11-15-651-p10-07-vendored-deps`, whose inventory
classifies `nlohmann/json` as **replace**.

## 1. Hypothesis

The audit found exactly two translation units that include
`<nlohmann/json.hpp>`: `common/json.cpp` (the `common_json` pimpl, i.e. the
container every other consumer sees) and `tests/gguf-model-data.cpp` (a direct
consumer that reads the HuggingFace `/api/models` response). The container is a
data structure, not a JSON decision, and replacing it would switch every
consumer at once. This stage replaces the **one direct consumer** instead, and
proves parity against nlohmann itself:

> If the Goose parser `json_parse` is wrapped in an adapter that exposes the
> subset of the nlohmann API the consumer actually uses, then the consumer
> yields the same answer, the same error class and the same match list on a
> corpus that covers fields, types, nesting and malformed input.

## 2. Consumer and API surface

`tests/gguf-model-data.cpp`, `detect_gguf_filename()` (reference copy under
`reference/`, SHA-256 `ed9d2374…`). It parses the API body and walks
`siblings[].rfilename` to pick the GGUF file whose name carries the requested
quant. The reused API is exactly:

| nlohmann | used as | adapter implementation |
|---|---|---|
| `json::parse(first, last)` | parse a byte range, throw on error | `json_parse` + flat table, throws `parse_error` |
| `j.contains(key)` | object key test | linear scan of the node's child block |
| `j[key]` | object member | child id lookup, null view when absent |
| `v.is_array()` | kind test | `jk == 6` |
| `v.get<std::string>()` | typed read | `sbuf[js .. js+jl]`, throws on a wrong kind |
| `for (const auto & e : v)` | array walk | iterator over the child block |
| `v.size()` | count | `jn` |

Everything else in the function is `std::string`/`std::sort` and stays native.

## 3. What was added

| file | role |
|---|---|
| `source/json-parser.goose` | byte-identical copy of `goose/json/json-parser.goose` (`ee56f4e4…`, the same hash EXP-610 records) |
| `source/goose-json-adapter.h/.cpp` | the adapter: flat `json_parse` table -> the subset above |
| `source/hf-pick.h/.cpp` | the transcribed consumer body as one template instantiated for both carriers; the dispatch and the call counters |
| `tests/fixture.cpp` | differential gate |
| `tests/gen_cases.py`, `tests/cases/`, `tests/cases.tsv` | the 20-document corpus |
| `tests/build.sh` | OFF and ON builds, transcript diff, `nm` |
| `tests/mutate.py` | 12 mutations, two halves of six |
| `tests/anchors.py` | provenance: the transcribed lines still exist in the reference copy |
| `evidence/` | logs, dumps, hashes |

The native body is compiled in **both** configurations: `hf_pick_native()` is
always present, and in an ON build the gate also runs it in the same process as
the oracle. The flag only selects the dispatch, and the call counters
(`native_calls`, `goose_calls`) prove which path ran.

## 4. Gate

20 documents: success 10, parse error 3, format error 3, no match 3, type
error 1. They cover plain and split file lists, mmproj preference, missing and
wrongly typed `siblings`, empty array, no match, `\uXXXX`/quote/backslash/
newline/NUL escapes, raw UTF-8 names, duplicate keys, lowercase quant and
unordered keys, u64 max / negative / exponent numbers, deep nesting,
truncated and trailing-garbage input, empty input, an array root, and a match
in the last array slot.

| run | cases | failures | native vs goose | transcript |
|---|---:|---:|---:|---|
| OFF `-O1` | 20 | 0 | n/a | 20 lines |
| ON `-O1` | 20 | 0 | 0 | identical to OFF |
| OFF `-O0` | 20 | 0 | n/a | 20 lines |
| ON `-O0` | 20 | 0 | 0 | identical to OFF |

Compared per document: error code, chosen file, split prefix, and the full
sorted match list (80 fields over 20 documents, 0 differences).

Symbols (`evidence/nm-symbols.txt`):

| build | `goose_init` | `json_parse` | `hf_pick_goose` | `pick_with<goose_json::json>` | `pick_with<nlohmann::json>` |
|---|---:|---:|---:|---:|---:|
| OFF | 0 | 0 | 0 | 0 | 1 |
| ON | 1 | 1 | 1 | 1 | 1 |

## 5. Mutations

12/12, six per half, all detected by an in-process native-vs-goose diff:

- parser: duplicate-key policy, trailing garbage, `\u` UTF-8 encoding, string
  length, newline as whitespace, dropped last child;
- adapter: `is_array` on objects, `operator[](key)` ignoring the key, iterator
  offset, `get<string>` swallowing type errors, string offset, parse error
  turned into a null document.

## 6. What still uses nlohmann

1. `common/json.cpp` — `common_json` remains a nlohmann-backed pimpl. Every
   other consumer (server schema, models, chat, tools, the GGUF metadata path)
   reaches JSON through it, so this stage does not touch them by construction.
2. `tests/gguf-model-data.cpp` in the shipped tree is unchanged: the switch is
   staged here, not enabled in the canon.
3. The serializer side (`json-dump`) is out of scope; this consumer never dumps.
4. The adapter builds a host-side tree view over the flat table; that view is a
   data structure, not a Goose module.

## 7. Reproduce

```
bash tests/build.sh all            # OFF + ON at -O1, transcript diff, nm
bash tests/build.sh all mutate     # the same at -O0
python3 tests/mutate.py 1          # 6/6
python3 tests/mutate.py 2          # 6/6
python3 tests/anchors.py           # 9/9 anchors present
```

Needs `goose`, `gcc-13`, `g++-13`, the vendored `nlohmann/json.hpp`, and the
corpus. No network, no GPU, no model, no server, no llama.cpp build.
