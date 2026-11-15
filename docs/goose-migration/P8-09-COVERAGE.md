# P8-09 — web/UI serving and application integration

Status: **PARTIAL** — 3 of 3 slices accepted; CORS, query decoding, middleware and
the host wiring are open.

## Slices

| slice | content | exports |
|---|---|---|
| S1 embedded assets | gzip gate, ETag/304, isolation, two cache-control policies, four no-cache names | `web_*` |
| S2 MIME table | `mime_from_ext`, `path_basename`, the eight required bundle labels | `mime_*` |
| S3 asset ETag | FNV-1a over the asset and the ETag literal | `et_*` |

## Evidence

- Experiment: `experiments/EXP-2026-11-15-653-p8-09-web-serving`.
- Canon: `goose/server/{web-assets,mime-table,asset-etag}.goose`.
- Gates: 51/294 + 73/156 + 42/108 values, 0 failures, identical at -O0.
- Mutations: 12/12 (two per slice per half).
- The oracle is the real `embed.cpp` generator compiled into the gates, plus a
  line-by-line transcription of the `server-http.cpp` chain.

Two precision errors in the first model were found and fixed: the gzip gate sits
inside the registered handler (an unknown path is 404, not 415), and the four
no-cache names bypass ETag. Embedded assets have no range requests natively, so
there is nothing to port there.

## Additional slices (EXP656)

| slice | content | exports |
|---|---|---|
| S4 CORS | allow-origin branches, preflight, `origin_is_localhost` | `cors_*` |
| S5 query transport | path decode, `get_params`, query build/decode/encode | `qry_*` |
| S6 API key | frontend paths, public endpoints, middleware, 401 body | `key_*` |

Evidence: 194/622 + 133/287 + 1121/1157 values, 0 failures, identical at -O0;
mutations 36/36. The oracle links the real vendored httplib and builds the 401
body with the real nlohmann ordered_json.

## Remaining

Wire `web_serve` and the API-key middleware into `server-http.cpp`; a live server
run needs Igor's explicit approval.

## Boundaries

`--path` mount points go through the vendor httplib and stay native. The slices
own the decision, not the I/O.
