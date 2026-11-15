# P8-09 batch 2 — CORS, query transport, API-key middleware

Offline port and differential verification of the three decisions that were
still native in `tools/server/server-http.cpp` after batch 1 (EXP-2026-11-15-653):
the CORS branch with `origin_is_localhost`, percent decoding with the query
transport, and the API-key middleware.

Base commit: `050354e9`. No model, no GPU, no server process, no network, and no
file of the shared tree is modified. Everything is built and run under this
directory.

| slice | module | prefix | gate key |
|---|---|---|---|
| S4 | `source/cors-policy.goose` | `cors_` | `FGCORS` |
| S5 | `source/query-transport.goose` | `qry_` | `FQRY` |
| S6 | `source/api-key-policy.goose` | `key_` | `FKEY` |

## Run

    bash tests/build.sh all           # three gates at -O1
    bash tests/build.sh all mutate    # three gates at -O0
    python3 tests/mutate.py 1         # 18 mutations: six per slice
    python3 tests/mutate.py 2         # the other eighteen

`build.sh` takes `[cors|qry|key|all] [final|mutate]`. It needs `goose` at
`/home/end0/AI/LlamaServer/toolchain/goose-2026.3-string/goose` (override with
`FG_GOOSE`), `gcc-13`/`g++-13` (override with `CC`/`CXX`) and the repository at
`/home/end0/AI/FrankenMoE-CUDA` (override with `FG_REPO`).

`mutate.py` expects `build/httplib.o`, which `build.sh` produces; run the gates
once before a mutation half. Builds are serial on purpose.

## Layout

    source/            the three Goose modules
    tests/build.sh     generate, compile, link and run the three gates
    tests/mutate.py    twelve mutations per slice, two halves of six
    tests/fixture_*.cpp  native arm plus the port arm, per slice
    results/           gate and mutation logs, SHA-256 of every input
    build/             generated C, objects and gate binaries (not committed)

## What the gates compare

Each fixture builds a native arm from the base commit itself:

* `fixture_cors.cpp` includes `common/http.h` and calls the real
  `common_http_parse_url`, then applies the transcribed pre-routing block
  (`server-http.cpp` 278-299) and `origin_is_localhost` (50-58).
* `fixture_qry.cpp` links the real `vendor/cpp-httplib/httplib.cpp`, so the query
  parse is `httplib::detail::parse_query_text`, the decoder is
  `httplib::decode_query_component` and the encoder is
  `httplib::encode_query_component`; only `decode_path_component` (a static
  helper) and the two small `get_params`/`build_query_string` loops are
  transcribed from 488-541.
* `fixture_key.cpp` transcribes the middleware from 206-251 over the real
  `frontend_paths`/`get_public_endpoints` shape, and builds the 401 body with
  `nlohmann::ordered_json` and the arguments `safe_json_to_str` uses, so the
  exported literal is checked against the library that produces it.

## Results

| gate | cases | values | failures (-O1) | failures (-O0) |
|---|---|---|---|---|
| cors | 194 | 622 | 0 | 0 |
| qry | 133 | 287 | 0 | 0 |
| key | 1121 | 1157 | 0 | 0 |

Mutations: 36 of 36 caught, 0 survived, 0 broken — 12 per slice, six per half.

Composition: the three modules plus one `import std;` typecheck as a single unit
(179 declarations, 117 specializations, 19 exports, no duplicate name) —
`results/compose.log`.

See `REPORT.md` for the interfaces, the exact integration step, the compatibility
argument, the boundaries and the module hashes.
