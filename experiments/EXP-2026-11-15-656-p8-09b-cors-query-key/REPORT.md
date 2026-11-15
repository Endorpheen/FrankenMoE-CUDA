# P8-09 batch 2 — report: CORS, query transport, API-key middleware

Base commit `050354e9`. Three slices, three differential gates, thirty-six
mutations, no model, no GPU, no server, no production file touched.

Batch 1 (`EXP-2026-11-15-653`) closed the web-asset serving decision, the MIME
table and the asset ETag, and listed five items still open for P8-09. This batch
closes three of them: the CORS decision with `origin_is_localhost`, percent
decoding of path components plus the query transport, and the API-key
middleware. Items 4 and 5 of that list (a live server run and the ledger update)
stay open and are restated at the end.

## 1. Inventory (the native lines this batch owns)

`tools/server/server-http.cpp` at `050354e9`, SHA-256
`fc9d307525d7db13fcee3d32e0d4587cbd2f3d93bcc70a86b2cedc16b336ce11`:

| lines | decision | slice |
|---|---|---|
| 50-58 | `origin_is_localhost`: parse the Origin URL, compare the host with `localhost`, `127.0.0.1`, `::1`, catch → false | S4 |
| 278-291 | the three `Access-Control-Allow-Origin` branches (wildcard with credentials echoes, `localhost` echoes only a local origin and warns otherwise, everything else is a static value) | S4 |
| 292-299 | the `OPTIONS` preflight branch: credentials text, methods, headers, blank `text/html` body, handled early | S4 |
| 488-506 | `decode_path_component`: `%XX` with a local hex table, invalid pair keeps `%` | S5 |
| 512-525 | `get_params`: query params, then path params with the path decode, both into `std::map` | S5 |
| 531-541 | `build_query_string`: re-encode the httplib params in insertion order | S5 |
| — | the vendored `parse_query_text` / `decode_query_component` / `encode_query_component` the three helpers above delegate to | S5 |
| 187-204 | `frontend_paths` and `get_public_endpoints` | S6 |
| 206-251 | `middleware_validate_api_key`, including the 401 body | S6 |

Not owned and left native: the `--path` mount point, the "model not ready" 503
middleware (300-305 registers it before the key check but it is a separate
decision), header lookup and its case folding, the configuration parser that
fills `api_keys`, the warning log lines, the response object, TLS, sockets and
threads.

## 2. Interfaces

All exports take caller-owned buffers and return lengths or a status; no pointer
into static storage crosses the boundary. C ABI: `sl_u8` = `{uint8_t*, int64_t}`,
`sl_i64` = `{int64_t*, int64_t}`, `goose_init(argc, argv)` once at start. Every
out-parameter is a slice (`i64[:]`), never a fixed array: Goose passes a fixed
array by value, so a `i64[1]` parameter could not be written back.

Packed tables use the layout batch 1 already established: one blob plus
`count + 1` offsets, so a caller cannot read a length that disagrees with its own
offsets and `off.len - 1` is the count.

### `source/cors-policy.goose` (prefix `cors_`)

    cors_is_localhost(host, parsed)                              -> i64  1/0
    cors_allow_origin(origins, credentials, origin, origin_local,
                      out, res)                                  -> i64  value bytes, -1 = no header
    cors_is_preflight(method)                                    -> i64  1/0
    cors_credentials_text(credentials, out)                      -> i64  4 or 5 ("true"/"false")
    cors_preflight_type(out)                                     -> i64  9 ("text/html")

`res` is `i64[2]`: `res[0]` = 1 when the header is set, `res[1]` = 1 when native
logs `(CORS) skip non-localhost origin`. `origin_local` is
`cors_is_localhost(host, parsed)` for the same origin. `out` must hold
`max(origins.len, origin.len)` bytes; on overflow the call returns -1 and leaves
`res[0]` clear, which is a caller bug, not a policy outcome.

The module never parses a URL: `goose/server/http-url-policy.goose` owns
`common_http_parse_url`. The caller passes the parsed host and `parsed = 0` when
the native parse would have thrown, and that is exactly the native `catch`
branch.

### `source/query-transport.goose` (prefix `qry_`)

    qry_decode_path(src, out)                                    -> i64  bytes, -1 = buffer too small
    qry_decode_query(src, out)                                   -> i64  bytes (also '+' -> ' ')
    qry_encode_component(src, out)                               -> i64  bytes
    qry_parse(query, k_blob, k_off, v_blob, v_off, tok, max, st) -> i64  0 / -1
    qry_build(np, k_blob, k_off, v_blob, v_off, out)             -> i64  bytes
    qry_params(np, k_blob, k_off, v_blob, v_off,
               pnp, pk_blob, pk_off, pv_blob, pv_off,
               ok_blob, ok_off, ov_blob, ov_off, out_np)         -> i64  0 / -1

`qry_parse` appends: `st[0]` is the pair count already in the table on entry and
the new count on exit, and `k_off[0]`/`v_off[0]` are the bytes already used. That
is how the native code ends up with the URL query and then a form body in one
`Params` container (httplib calls `parse_query_text` twice, each with its own
raw-token cache). `tok` holds `2 * max` slots for the trimmed raw token ranges of
this call. `qry_params` takes the path pairs with **raw** values and applies the
path decode itself, so the caller never picks the wrong rule.

### `source/api-key-policy.goose` (prefix `key_`)

    key_is_public(path, paths, off)                              -> i64  1/0
    key_credential(auth, apikey, out)                            -> i64  bytes
    key_strip_bearer(value, out)                                 -> i64  bytes
    key_matches(keys, koff, candidate)                           -> i64  1/0
    key_match_credential(keys, koff, auth, apikey)               -> i64  1/0
    key_authorize(keys, koff, path, paths, poff, auth, apikey)   -> i64  1 = pass, 0 = 401
    key_error_body(out)                                          -> i64  80
    key_error_type(out)                                          -> i64  31

`paths`/`off` carry the frontend request paths (`"/"` first, then `"/" + name`
for every embedded asset); `/health` and `/v1/health` are owned by the module,
not by the table. `keys`/`koff` carry the configured API keys; an empty table is
the native "no key configured" case. The 401 body is exported as a literal so no
call site rebuilds it:

    {"error":{"message":"Invalid API Key","type":"authentication_error","code":401}}

## 3. Exact integration step

1. Copy the three modules into the canonical tree:

       cp source/cors-policy.goose      <repo>/goose/server/cors-policy.goose
       cp source/query-transport.goose  <repo>/goose/server/query-transport.goose
       cp source/api-key-policy.goose   <repo>/goose/server/api-key-policy.goose

2. Append them to `MODULES` in `tools/build-main-runtime.sh`, after
   `goose/server/web-assets.goose`. The prefixes `cors_`, `qry_` and `key_`
   collide with no module in the current list, and the three together typecheck
   as one unit (section 5). The whole-list check still belongs to
   `tools/compose-goose.py` on the extended list; run it before committing.

   Note for the integrator: while this batch ran, the shared tree carried
   uncommitted integration edits from another experiment
   (`EXP-2026-11-15-654-p10-08c-header-surface`, three `goose/misc/*` modules in
   `tools/build-main-runtime.sh`, `goose/MANIFEST.json`,
   `tools/sync-goose-tree.py`, `docs/goose-migration/tasks.json`). This batch
   did not create or touch those changes; the insertion point named above is
   "after `goose/server/web-assets.goose`", and the manifest entries must be
   appended to whatever state the ledger is in when the batch is integrated.

3. Add three entries to `goose/MANIFEST.json` with `status: "ported"`, the
   experiment id, the module SHA-256 and the gate line, both from section 5.

4. Wire the three call sites in `tools/server/server-http.cpp`.

   **CORS**, replacing 278-299 inside the pre-routing handler. The Origin host
   comes from `http-url-policy` (the port of `common_http_parse_url`):

       std::string origin = req.get_header_value("Origin");
       std::string host;
       int64_t     parsed = 1;
       try {
           host = common_http_parse_url(origin).host;   // http-url-policy export
       } catch (const std::exception &) {
           parsed = 0;                                  // the native catch
       }
       const int64_t local = cors_is_localhost(sl_u8{(uint8_t *) host.data(), (int64_t) host.size()}, parsed);

       static auto SL = [](const std::string & s) {
           return sl_u8{(uint8_t *) (void *) s.data(), (int64_t) s.size()};
       };
       static auto text = [](int64_t n, const uint8_t * buf) {
           return n < 0 ? std::string() : std::string((const char *) buf, (size_t) n);
       };

       uint8_t cbuf[8192];
       int64_t cres[2];
       const int64_t n = cors_allow_origin(
           SL(params.cors_origins), params.cors_credentials ? 1 : 0, SL(origin), local,
           sl_u8{cbuf, (int64_t) sizeof cbuf}, sl_i64{cres, 2});
       if (cres[0]) {
           res.set_header("Access-Control-Allow-Origin", text(n, cbuf));
       }
       if (cres[1]) {
           SRV_WRN("(CORS) skip non-localhost origin: %s\n", origin.c_str());
       }
       if (cors_is_preflight(SL(req.method))) {
           const int64_t cn = cors_credentials_text(params.cors_credentials ? 1 : 0,
                                                    sl_u8{cbuf, (int64_t) sizeof cbuf});
           res.set_header("Access-Control-Allow-Credentials", text(cn, cbuf));
           res.set_header("Access-Control-Allow-Methods",     params.cors_methods);
           res.set_header("Access-Control-Allow-Headers",     params.cors_headers);
           res.set_content("", "text/html");
           return httplib::Server::HandlerResponse::Handled;
       }

   **Query**, replacing 488-541:

       // one table per request; tokens, key bytes and value bytes are the query
       // text length in the worst case
       std::vector<uint8_t> kb(n), vb(n);
       std::vector<int64_t> ko(31, 0), vo(31, 0), tok(60, 0);
       int64_t st[1] = {0};
       qry_parse(query_sl, sl_u8{kb.data(), n}, sl_i64{ko.data(), 31},
                 sl_u8{vb.data(), n}, sl_i64{vo.data(), 31}, sl_i64{tok.data(), 60}, 30,
                 sl_i64{st, 1});

       // then qry_params with the route's path params (raw values), and
       // qry_build over the same table for the request's query string field

   **API key**, replacing 187-244:

       // once, at init: paths = "/" then "/" + a.name for every asset, packed
       const int64_t ok = key_authorize(
           sl_u8{keys_blob.data(), (int64_t) keys_blob.size()},
           sl_i64{keys_off.data(), (int64_t) keys_off.size()}, SL(req.path),
           sl_u8{paths_blob.data(), (int64_t) paths_blob.size()},
           sl_i64{paths_off.data(), (int64_t) paths_off.size()}, SL(auth), SL(apikey));
       if (!ok) {
           uint8_t bbuf[256], tbuf[64];
           const int64_t bn = key_error_body(sl_u8{bbuf, (int64_t) sizeof bbuf});
           const int64_t tn = key_error_type(sl_u8{tbuf, (int64_t) sizeof tbuf});
           res.status = 401;
           res.set_content(text(bn, bbuf), text(tn, tbuf));
           SRV_WRN("%s", "unauthorized: Invalid API Key\n");
           return false;
       }

   Keep the native order: CORS headers, then the preflight early return, then the
   server-state middleware, then the key check. The CORS module must not move
   behind the ready check.

5. Re-run the three gates from this directory (`bash tests/build.sh all`) after
   the rebase; they compile the same sources that were copied.

## 4. Compatibility, assumptions, conflicts

Compatibility:

* The exact native branch order and the exact literals: `"true"`, `"false"`,
  `"text/html"`, the three localhost spellings, `"Bearer "`, the 401 body and
  `application/json; charset=utf-8`.
* The wildcard-with-credentials branch sets the header even for an absent Origin
  (zero value bytes), the `localhost` branch sets it only for a non-empty local
  origin and warns on any other non-empty origin, an empty origin is silent.
* The query transport reproduces httplib exactly, including the parts that look
  like accidents: tokens are trimmed of spaces and tabs but the key and value
  inside a token are not re-trimmed; an exactly repeated raw token is dropped
  while two different spellings of the same decoded pair are both kept; an empty
  key is cached but not entered; the params map keeps the last assignment per
  key and sorts by unsigned byte order; a path param value is decoded with the
  path rule, so `+` stays a plus; the query string is rebuilt in insertion order
  with the RFC 3986 encoder (`+` for space, `%2B` for a plus, `!$'()*,;:@/?`
  unchanged, everything else upper-case `%XX`).
* The API-key middleware keeps the exact match, the Authorization-then-X-Api-Key
  fallback, the single case-sensitive `Bearer ` strip and the exact path set
  (`/health/` and `/Health` are not public).
* No allocation, no float, no I/O, no division and no modulo in the ports, so the
  Goose-vs-C division semantics document does not apply. Exports are read-only
  over their inputs.

Assumptions:

* the CORS caller parses the Origin with `http-url-policy` and maps a parse
  exception to `parsed = 0`; the module deliberately cannot re-parse;
* the frontend path table is `web-assets`' table with a leading `/`, built once
  at init, and `api_prefix` is not involved (the native sets are absolute);
* key values are byte strings, not trimmed, because the native vector holds the
  CLI values verbatim;
* the query table is per request and its buffers are sized from the query text;
  a longer body than the URL needs its own `qry_parse` call and a larger table.

Conflicts:

* `goose/server/http-url-policy.goose` owns URL parsing; this batch adds one
  consumer of its host slice and changes nothing in it. If that module is later
  rewritten to return a status class instead of the host string, the CORS call
  site is the only consumer to update.
* `goose/server/route-table.goose` owns the API route ids; `key_` owns only the
  public-path membership test, so an API route added to the route table still has
  to be listed as public if it should bypass the key.
* `goose/server/web-assets.goose` owns the asset names; `key_` consumes the same
  names as request paths. Neither re-derives the other's table.
* `web_`, `mime_`, `et_`, `http_`, `cors_`, `qry_` and `key_` do not collide.

## 5. Evidence

    gate  cors  FGCORS cases=194  values=622  failures=0   (-O1 and -O0)
    gate  qry   FQRY   cases=133  values=287  failures=0   (-O1 and -O0)
    gate  key   FKEY   cases=1121 values=1157 failures=0   (-O1 and -O0)

    mutations half 1  caught=18 survived=0 broken=0   (6 cors, 6 qry, 6 key)
    mutations half 2  caught=18 survived=0 broken=0   (6 cors, 6 qry, 6 key)

    compose  the three modules plus one `import std;` typecheck as one unit:
             179 top-level declarations, 117 specializations, 19 exports, no
             duplicate top-level name and no renamed helper (results/compose.log)

Logs: `results/gate-o1.log`, `results/gate-o0.log`, `results/mutate-1.log`,
`results/mutate-2.log`. SHA-256 of every input, of the generated C the gates
compiled and of the logs: `results/hashes.txt`.

    goose        2026.3-string (toolchain/goose-2026.3-string/goose)
    compilers    gcc-13 13.4.0 / g++-13 13.4.0
    gate         -O1   mutations -O0   fixtures -O2
    base commit  050354e9

    native  upstream/llama.cpp/tools/server/server-http.cpp
            fc9d307525d7db13fcee3d32e0d4587cbd2f3d93bcc70a86b2cedc16b336ce11
    native  upstream/llama.cpp/vendor/cpp-httplib/httplib.cpp
            c796420a111cd2c5f4a3efaa48f9dc6f5ad8086c39646b5e3921fa3e30e7e4ee
    native  upstream/llama.cpp/common/http.h
            7d9a690ba1f6efa5a518b3a0720fa9c4b682d2d5303b19cc8957ba083ad57343
    native  upstream/llama.cpp/vendor/nlohmann/json.hpp
            aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63

    port  source/cors-policy.goose
          d92abb48465ffadd640e6ea7b3219863f31966911912697c4baab8c5c330f5a9
    port  source/query-transport.goose
          6133979670ea29e815c05f0521b446f0c76d94bf11ab5edf9d53ee29d97a5825
    port  source/api-key-policy.goose
          3559df070128ae56bfdd73c62bf99ed6fc998616faa0f79ee5291f5d8803c3c9

The mutation table is in `tests/mutate.py`; each entry names the decision it
breaks in one line, which is also the record of what the gates are worth. The
thinnest catches are the three literal checks (401 message, 401 code, content
type) at one failing value each, which is the whole observable surface of a
constant, and the `parsed = 0` CORS case at three.

## 6. Verdict and remainder

Verdict for this batch: ACCEPTED as far as a subagent can accept — three slices,
three green gates at both optimisation levels, twelve of twelve mutations caught
per slice, no production file changed, no model or server started.

What P8-09 still needs before the requirement closes:

1. one recorded run of the real server serving the embedded UI (health, index,
   asset, 304, 415) and one CORS preflight, one percent-decoded path id and one
   401 — that is the liveness half and needs Igor's approval to start the binary;
2. the ledger update for P8-09 (`slices_total`, `slices_done`, `remaining`,
   `coverage_doc`) in `docs/goose-migration/tasks.json`, after this batch is
   integrated and the three modules are in `goose/MANIFEST.json`;
3. the composition check on the extended module list and the rebase of this
   experiment's gates onto whatever canon accepts, before the manifest entries
   are written. The canon moved during this run: another experiment's
   integration edits were uncommitted in the shared tree, which is why this batch
   quotes its base by commit and by the native file hash rather than by the
   current module count.
