# P8-09 batch 1 — web/UI serving: static asset dispatch, MIME table, asset ETag

Base commit: `ec50a32d` (2026-11-15). Task: P8-09, "Port web/UI serving and
application integration". Scope of this batch: the parts of the embedded web UI
pipeline that are pure decisions and need neither a model, a socket, nor a
browser.

## Hypothesis

The native web UI is served by three text decisions that survive without a
server:

1. **which asset a request path gets and under which headers** — the
   embedded-asset branch of `server_http_context::init` in
   `upstream/llama.cpp/tools/server/server-http.cpp` (341-425): the per-route
   handlers, the gzip 415 gate, the ETag/304 validator, the isolation headers,
   the two Cache-Control literals and the four no-cache names;
2. **what content type an asset has, and which labels a complete UI bundle must
   satisfy** — `mime_from_ext`, `path_basename` and the eight `checks[]`
   matchers in `upstream/llama.cpp/tools/ui/embed.cpp` (20-40, 113-116,
   178-196);
3. **what the ETag of an asset is** — the FNV-1a hash and the
   `"\"0x%016" PRIx64 "\""` literal in the same generator (47-56, 265-267),
   which `server-http.cpp:370` copies verbatim into the response header.

If these three are decisions and not I/O, a Goose port must reproduce the
native bytes exactly: the same status, the same header set, the same
Cache-Control text, the same content type, the same ETag.

## Slices

| prefix | module | native source | exports |
|---|---|---|---|
| `web_` | `source/web-assets.goose` | server-http.cpp 341-425 | `web_route`, `web_serve`, `web_cache_text`, `web_isolation_of` |
| `mime_` | `source/mime-table.goose` | embed.cpp 20-40, 113-116, 178-196 | `mime_of`, `mime_text`, `mime_base`, `mime_required`, `mime_required_label` |
| `et_` | `source/asset-etag.goose` | embed.cpp 47-56, 265-267 | `et_hash`, `et_text` |

## Oracle

The oracle is the base-commit generator itself, not a paraphrase of it:

* `tests/make_assets.py` writes a deterministic 24-file synthetic bundle: the
  eight required labels plus one file per content-type branch plus the edge
  cases (`UPPER.HTML`, `dir.with.dot/file`, `trailing.`, `noext`,
  `notes.txt`, `_app/version.json`);
* `tests/build.sh` builds `tools/ui/embed.cpp` unmodified as
  `build/embed-native` and runs it on that bundle, producing
  `build/gen/ui.cpp` / `ui.h` — the real producer output that the server would
  link;
* `fixture_mime.cpp` and `fixture_etag.cpp` `#include "embed.cpp"` with its
  `main` renamed, so `mime_from_ext`, `path_basename` and `fnv_hash` are called
  as the generator defines them, and additionally compare against the fields of
  the generated `ui.cpp` table;
* `fixture_web.cpp` carries a statement-by-statement transcription of the
  serve chain (the lambdas are registered with httplib and are not callable from
  outside), cited line by line, and runs it against the real generated asset
  table.

Two fidelity errors were found and fixed because of that transcription being
written against the native order: the gzip gate runs **inside** a registered
handler, so an unknown path is 404 and not 415; and the four `no_cache_names`
use `serve_asset_nocache`, which sets **no ETag at all**, so their
`If-None-Match` is ignored. Both are now in the corpus as named cases
("404 survives without the gate", "no ETag, inm ignored").

## Gate results

| gate | cases | compared values | failures |
|---|---:|---:|---:|
| `FGWEB_WEB` | 51 | 294 | 0 |
| `FGWEB_MIME` | 73 | 156 | 0 |
| `FGWEB_ETAG` | 42 | 108 | 0 |

The same three gates pass at `-O1` (`results/gate-o1.log`) and at `-O0`
(`results/gate-o0.log`). `results/cases-web.log` is the per-request table of the
native decision: status, asset index, header mask, encoding, isolation and
Cache-Control.

## Mutations

Twelve mutations, two halves of six, two per slice per half, all run at `-O0`
against the regenerated module (`tests/mutate.py 1|2`):

| half | slice | mutations | caught |
|---|---|---:|---:|
| 1 | web | 2 | 2 |
| 1 | mime | 2 | 2 |
| 1 | etag | 2 | 2 |
| 2 | web | 2 | 2 |
| 2 | mime | 2 | 2 |
| 2 | etag | 2 | 2 |

12/12 caught, 0 survived, 0 broken. Logs: `results/mutate-1.log`,
`results/mutate-2.log`.

## What is deliberately not here

Still native, with the reason:

* `server-http.cpp:332-339` — `--path` mount point: httplib does the file I/O,
  the directory walk and the range responses; there is no owned decision.
* `vendor/cpp-httplib` range parsing (`parse_range_header`, `Content-Range`):
  vendored library behaviour, not llama code. Embedded assets are always served
  whole, so P8-09's "range requests" has no owned native branch.
* CORS (`server-http.cpp:51-54, 280-296`), percent decoding and query transport
  (`488-541`), the API-key/public-endpoint middleware (`188-235`): real owned
  decisions, scheduled as P8-09 batch 2 — they need `http-url-policy.goose`'s
  parsed-host surface for `origin_is_localhost`, which this batch does not
  touch.

## Layout

    source/{web-assets,mime-table,asset-etag}.goose   the port
    tests/build.sh                                    gate build (-O1 / -O0)
    tests/make_assets.py                              synthetic bundle + oracle
    tests/fixture_{web,mime,etag}.cpp                 differential gates
    tests/mutate.py                                   12 mutations, two halves
    results/                                          gate, mutation and hash logs

Generated files (`build/*.c`, `build/*.o`, `build/gen/*`, `build/assets/*`,
`build/gate-*`, `build/embed-native`) are reproducible from the tracked inputs
and are not to be committed.
