# P7-11 coverage — download, HF cache, HTTP, logging and process helpers

Updated: 2026-11-14. Native sources: `common/download.cpp`,
`common/hf-cache.cpp`, `common/http.h`, `common/log.cpp`, `common/console.cpp`,
`common/subproc.cpp` and `common/debug.cpp`.

The requirement owns five slices; `subproc` is a documented boundary, not a
slice. S1 is accepted; S2–S5 are open.

| slice | native scope | state |
|---|---|---|
| S1. HTTP URL and authority policy | `common_http_parse_url`, `format_host`, `is_loopback`, `proxy_bypass`, `show_masked_url` | **done (EXP578)** |
| S2. HF cache naming | `hf-cache.cpp` naming and path rules | open |
| S3. Download selection | `download.cpp` file selection and metadata | open |
| S4. Log policy | `log.cpp` level/filter policy, plus the `debug.cpp` filter/NaN-abort part | open |
| S5. Console editing | `console.cpp` line editing | open |
| Boundary. subprocess | `subproc.cpp` | not a slice |

## S1 — accepted

Source: `experiments/EXP-2026-11-14-578-p7-11-http-url/`; canonical file
`goose/server/http-url-policy.goose`.

The port covers scheme detection (lowercase http/https only), userinfo splitting,
path offsets with the invented `/`, bracketed IPv6 authorities and their status
codes, `std::stoi`-compatible port parsing and defaults, the full loopback rule
(localhost, `::1`, `0.0.0.0`, `*.localhost` but not bare `.localhost`, and four
1–3 digit octets of 127.0.0.0/8 with the native zero-shape quirks), NO_PROXY
matching and the masked-URL renderer.

Gate: 20 000 cases driven by one generator for both the native probe and the
Goose driver, diff = 0. Mutations 14/14 in two halves.

## Open boundaries

- `goose/server/prerouting.goose` still has its own `origin_is_localhost`;
  six corpus rows differ. Replacing it needs the CORS regression from EXP446 and
  is not part of S1.
- HF cache naming, download selection, logging and console editing stay native.
- `subproc.cpp` is explicitly outside the slices.
- The S1 gate runs through `goose run`; no network is touched.
