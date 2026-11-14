# EXP-2026-11-14-578 — HTTP URL and authority policy (P7-11 S1)

Native counterpart: `common/http.h` (header-only:
`common_http_parse_url`, `common_http_format_host`, `common_http_is_loopback`,
`common_http_proxy_bypass`, `common_http_show_masked_url`). Base commit:
`1adaccc7`.

## What this slice covers

`source/http-url-policy.goose` (450 lines, five exports, each a thin wrapper
over a private body because Goose cannot call an `export fn` from another
function):

| export | native | returns |
|---|---|---|
| `http_parse_url(url, off, len, port)` | `common_http_parse_url` | status 0/1/2/3/4 and offsets |
| `http_format_host(host, out)` | `common_http_format_host` | length |
| `http_is_loopback(host)` | `common_http_is_loopback` | 0/1 |
| `http_proxy_bypass(no_proxy, host)` | `common_http_proxy_bypass` | 0/1 |
| `http_masked_url(url, off, len, out)` | `common_http_show_masked_url` | length |

Rules reproduced from the native text: the first `://` selects the scheme and
only lowercase `http`/`https` are accepted; userinfo splits at the first `:` and
`@host` gives an empty user; the path starts at the first `/` and a missing
slash is represented as offset -1 with length 1; bracketed authorities keep
their inner colons, text after `]` that does not start with `:` is ignored and a
missing `]` is status 3; an unbracketed authority splits at the first `:`;
port text follows `std::stoi` (leading blanks, optional sign, stop at the first
non-digit, int range; empty port text takes 80 for http and 443 for https);
loopback covers `localhost`, `::1`, `0.0.0.0`, `*.localhost` (not the bare
`.localhost`) and exactly four 1–3 digit octets of 127.0.0.0/8; NO_PROXY is a
comma list with `*`, an optional leading dot, exact host or dot-preceded
subdomain, blank trimming and case folding, and entries with ports never match
a bare host.

## Gate

`tests/gen_corpus.py` emits the native probe (header-only, no llama, no socket)
and the Goose driver from the same index arithmetic. 20 000 cases over five URL
shapes are compared line by line:

```
20000 lines native, 20000 lines Goose, diff = 0
```

Status coverage: 0 → 6911, 1 → 4002, 2 → 2668, 3 → 4002, 4 → 2417; loopback
true 1508; bypass true 5043; masked credentials 2457; invented `/` 2061.
`tests/cases.tsv` is the hand-readable subset with the exact native lines.

## Mutations

`tests/mutate.py <lo> <hi>` edits the port, regenerates the driver, runs it with
`goose run` and compares with the native output.

```
python3 tests/mutate.py 0 7    # 7/7 caught
python3 tests/mutate.py 7 14   # 7/7 caught
```

## Recorded limits

- `origin_is_localhost` in `goose/server/prerouting.goose` still uses byte
  equality against `localhost` and `127.0.0.1`; six corpus rows differ from this
  policy. Replacing it is a separate integration step with the CORS regression.
- The gate runs `goose run` (TinyCC), not a full C build.
- The probe includes only `common/http.h`; no sockets and no network.
