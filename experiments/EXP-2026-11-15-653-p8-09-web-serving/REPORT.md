# P8-09 batch 1 — report: web/UI serving slices

Base commit `ec50a32d`. Three slices, three differential gates, twelve
mutations, no model, no server, no GPU, no production file touched.

## 1. Inventory (what the native tree actually owns)

`tools/ui/embed.cpp` — the UI bundle generator (308 lines). It walks the
`dist/` tree, sorts names, computes an ETag per file and writes the C table the
server links. Owned text decisions: `mime_from_ext` (extension table and
octet-stream fallback), `path_basename`, the eight required-bundle labels, the
FNV-1a hash and the ETag literal. Not owned: directory walk, gzip staging,
`ui.cpp` emission, file reads.

`tools/server/server-http.cpp` (838 lines) — the HTTP surface. Owned decisions
that need no socket:

| lines | decision | this batch |
|---|---|---|
| 188-235 | frontend paths, public endpoints, `Bearer` key check | batch 2 |
| 280-296 | CORS echo / localhost-only / static origin, preflight | batch 2 |
| 341-425 | embedded asset serving: gzip 415 gate, ETag/304, isolation, Cache-Control, no-cache names | **S2** |
| 332-339 | `--path` mount point (httplib file I/O) | stays native |
| 488-541 | `decode_path_component`, `get_params`, `build_query_string` | batch 2 |
| 27-46 | request logger, `origin_is_localhost` | batch 2 |

Range requests: the only range code is in vendored cpp-httplib
(`parse_range_header`, `Content-Range`), used by the file mount point. Embedded
assets are served whole, so P8-09's range clause has no owned native branch and
nothing to port.

Chosen slices, all fully offline: static asset dispatch (S2), MIME and bundle
labels (S1), asset ETag (S3).

## 2. Interfaces

All exports take caller-owned buffers and return lengths; no pointer into static
storage crosses the boundary. C ABI: `sl_u8` = `{uint8_t*, int64_t}`,
`sl_i64` = `{int64_t*, int64_t}`, `goose_init(argc, argv)` once at start.

`source/mime-table.goose` (prefix `mime_`):

    mime_of(name)                        -> i64   content-type id 0..11
    mime_text(id, out)                   -> i64   bytes written, -1 on a bad id
    mime_base(name, out)                 -> i64   basename bytes (path_basename)
    mime_required(base)                  -> i64   bitmask of the 8 bundle labels
    mime_required_label(bit, out)        -> i64   label text ("bundle[hash].js"), -1 on a bad bit

`source/asset-etag.goose` (prefix `et_`):

    et_hash(bytes, out: i64[1])          -> i64   0; out[0] = FNV-1a 64 bit pattern
    et_text(hash, out)                   -> i64   20; '"0x' + 16 lower-case hex + '"'

`source/web-assets.goose` (prefix `web_`):

    web_route(path, prefix, names, off, cnt)                    -> i64  asset idx, -1 = no route
    web_serve(path, prefix, names, off, etags, eoff,
              use_gzip, accept, inm, out: i64[6])               -> i64  out[0] status
    web_cache_text(policy, out)                                 -> i64  Cache-Control bytes, -1 on a bad policy
    web_isolation_of(names, off, idx)                           -> i64  0/1

`web_serve` fills `out` as `[status, asset idx, header mask, encoding,
isolation, policy]`. Header mask bits, in the order native sets them:
`1` ETag, `2` COOP/COEP, `4` Cache-Control, `8` Content-Encoding, `16`
Content-Type. `status` is 200, 304, 404 or 415; `policy` is 0 immutable,
1 revalidate (index), 2 no-cache (`sw.js`, `manifest.webmanifest`,
`_app/version.json`, `build.json`). The asset table arrives as a packed name
blob plus `cnt + 1` offsets, and a parallel ETag blob plus offsets, so the count
is `off.len - 1` and the tables cannot disagree with themselves.

## 3. Exact integration step

1. Copy the three modules into the canonical tree:

       cp source/web-assets.goose  <repo>/goose/server/web-assets.goose
       cp source/mime-table.goose  <repo>/goose/server/mime-table.goose
       cp source/asset-etag.goose  <repo>/goose/server/asset-etag.goose

2. Append them to `MODULES` in `tools/build-main-runtime.sh`, after
   `goose/server/route-table.goose`. Composition was verified: the 95-module
   list plus these three typechecks as one unit (3489 specializations, no
   duplicate top-level name, no renamed helper needed).

3. Add three entries to `goose/MANIFEST.json` with `status: "ported"`, the
   experiment id, the module SHA-256 from `results/hashes.txt` and the gate line
   from section 5.

4. Wire the call site in `tools/server/server-http.cpp`. Build the two packed
   tables once in `server_http_context::init` from `llama_ui_get_assets()` (same
   order, `cnt + 1` offsets), keep `llama_ui_use_gzip()` as the `use_gzip` flag,
   then replace the per-route lambdas with one handler:

       const llama_ui_asset * a = llama_ui_find_asset(name);
       int64_t out[6];
       web_serve(sl_u8{(uint8_t*) path.data(), (int64_t) path.size()},
                 sl_u8{(uint8_t*) prefix.data(), (int64_t) prefix.size()},
                 names_blob, names_off, etags_blob, etags_off,
                 llama_ui_use_gzip() ? 1 : 0,
                 sl_u8{(uint8_t*) accept.data(), (int64_t) accept.size()},
                 sl_u8{(uint8_t*) inm.data(), (int64_t) inm.size()}, out);
       // out[0] status; out[2] the header mask; out[4] isolation;
       // Cache-Control bytes from web_cache_text(out[5], buf)
       // body from a->data / a->size, Content-Type from a->type

   The route table itself stays a Go map or a `web_route` call; the module is
   the single owner of the route decision, so the C++ side must not re-derive
   `prefix + "/" + name`.

5. Re-run the three gates from this directory (`bash tests/build.sh all`) after
   the rebase; they compile the same sources that were copied.

## 4. Compatibility, assumptions, conflicts

Compatibility:

* Preserves every owned native behaviour: the exact four status codes, the exact
  header set per status (a 304 keeps ETag and Content-Encoding but has no
  Cache-Control and no isolation headers), both Cache-Control literals, the
  content types, the ETag bytes.
* No allocation, no float, no I/O, no division or modulo in the port, so the
  Goose-vs-C division semantics document does not apply. The only wrapping
  arithmetic is the u64 FNV step, which the Goose runtime performs as
  `gs_mul_u64`, i.e. unsigned wrap like the native `uint64_t`.
* Exports are read-only over their inputs; the caller keeps ownership of the
  asset table.

Assumptions:

* the asset table is handed over in `embed.cpp`'s sorted order; the module never
  reorders or mutates it;
* `off`/`eoff` carry `cnt + 1` entries and the last one equals the blob length;
* `api_prefix` never ends with `/` (the native CLI accepts `/api`, not `/api/`);
* only the embedded-asset path is covered; `--path` (file mount) stays native;
* `use_gzip` is a per-build constant, as `llama_ui_use_gzip()` is.

Conflicts:

* `goose/server/route-table.goose` owns the API route ids and `web_` owns the
  static asset routes. They share no symbol and no table; both can be composed,
  which the unit check confirms.
* `goose/server/http-url-policy.goose` owns URL parsing. Batch 2's CORS slice
  needs `origin_is_localhost`, which is a host comparison on top of that
  module's parsed host, so the CORS module must take the host as an argument
  rather than re-parse the URL, or `http-url-policy` must export the host slice.
  This batch deliberately leaves that interface alone.
* `tools/compose-goose.py` renames colliding helper families per module; the
  three prefixes here (`web_`, `mime_`, `et_`) collide with nothing in the
  current 95-module list.

## 5. Evidence

    gate  web   FGWEB_WEB  cases=51 values=294 failures=0   (-O1 and -O0)
    gate  mime  FGWEB_MIME cases=73 values=156 failures=0   (-O1 and -O0)
    gate  etag  FGWEB_ETAG cases=42 values=108 failures=0   (-O1 and -O0)
    mutations 12/12 caught, 0 survived, 0 broken (halves 6/6, two per slice)

Logs: `results/gate-o1.log`, `results/gate-o0.log`,
`results/mutate-1.log`, `results/mutate-2.log`, `results/cases-web.log`.
Hashes of every input, of the generated oracle bundle and of the logs:
`results/hashes.txt`.

    goose        2026.3-string (toolchain/goose-2026.3-string/goose)
    compilers    gcc-13 / g++-13
    gate         -O1   mutations -O0
    native       upstream/llama.cpp/tools/ui/embed.cpp
                 sha256 c2c748620feeccf79fd7f6bae46ed563ffabba8451420a4f33ba6bd172d4e427
    native       upstream/llama.cpp/tools/server/server-http.cpp
                 sha256 fc9d307525d7db13fcee3d32e0d4587cbd2f3d93bcc70a86b2cedc16b336ce11
    port         source/web-assets.goose
                 sha256 0315674e23129694f19b99e28287ce747ee2bc40ca389107c05edd2b632f69c7
    port         source/mime-table.goose
                 sha256 59fbb6778ac6d1251c276b68e295bf54e2bdc7f849804b3fb61542e41d5f1bdb
    port         source/asset-etag.goose
                 sha256 9f74aeb283ebd86f2b99d47476da9a837f35b2a1309e2b379a3e8258efbdecce

## 6. Verdict and remainder

Verdict for this batch: ACCEPTED as far as a subagent can accept — three slices,
three green gates at both optimisation levels, twelve of twelve mutations caught,
no production file changed, no model or server started.

What P8-09 still needs before the requirement closes:

1. CORS decisions and `origin_is_localhost` (server-http.cpp 51-54, 280-296);
2. percent decoding of path components and query transport (488-541);
3. the API-key and public-endpoint middleware (188-235);
4. one recorded run of the real server serving the embedded UI (health, index,
   asset, 304, 415) — that is the liveness half, needs Igor's approval to start
   the binary;
5. the ledger update for P8-09 (`slices_total`, `slices_done`, `remaining`,
   `coverage_doc`) in `docs/goose-migration/tasks.json`.
