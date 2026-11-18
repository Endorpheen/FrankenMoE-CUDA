# EXP-2026-11-18-958: HTTP response bytes, in Goose

Status: differential PASS against cpp-httplib itself, 140 checks. This is the
formatting half of the transport; the socket half is still blocked.

## Hypothesis fixed before changes

The standalone runtime has HTTP *decisions* (routing, CORS, API key, query
parsing) but no way to produce the bytes of a response, so no transport can be
written. If the serialisation is ported, a listener needs only a socket write and
nothing else about the format.

Before writing it, the blocker for the socket half was measured instead of
assumed: a probe unit declaring `extern "socket"` compiles **only** while no
header declares `socket`. Force-including `sys/socket.h` reproduces the same
failure the reader owner hit with `open`:

    error: conflicting types for 'socket'; have 'int64_t(int64_t, int64_t, int64_t)'

That is recorded as blocker 6 in `docs/goose-migration/COMPILER-BLOCKERS.md`,
with the three workarounds that do not work (alias, `--defsym`, `objcopy`).

## What was done

`goose/server/http-response.goose` owns the formatting:

- `rs_reason` - the reason phrase for a status code, from the table httplib uses;
- `rs_status_line` - `"HTTP/1.1 <status> <reason>\r\n"`;
- `rs_header` / `rs_end_headers` - one header line, then the blank line;
- `rs_chunk` / `rs_chunk_end` - chunked framing, lower-case hex size.

The table was **extracted from `httplib.cpp` programmatically**, not typed by
hand, and the gate proved that mattered twice.

## Result

| check | result |
| --- | --- |
| `goose check` | ok, 69 specializations |
| gate | **140 checks, 0 failures** |
| status codes compared | 63, plus an unknown code |
| oracle | `httplib::status_message` linked from the vendored `httplib.cpp` |

Two real errors the gate caught, both in my first version:

1. **Unknown code.** I assumed an unknown status renders an empty phrase, so 599
   would leave a trailing space. httplib's switch ends with
   `default: case InternalServerError_500:`, so 599 is
   `"HTTP/1.1 599 Internal Server Error\r\n"`. The gate found 26 failures; the
   module now matches, and the test asserts the measured behaviour instead of my
   guess.
2. **Code 422.** I had written "Unprocessable Entity" from memory; httplib says
   "Unprocessable Content". Extracting the table from the source is what prevents
   this class of error, and this is why the table is generated rather than typed.

The final gate compares all 63 reason phrases, the unknown-code path, three
header lines, an empty header name (refused), a 5-byte chunk, a 4096-byte chunk
hex-framed as `1000`, the terminator, a complete chunked response assembled the
way a route assembles it, and a short buffer.

## Not claimed

There is still no listener: the socket, `accept` and the read loop remain blocked
by blocker 6, so `runtime/` has no HTTP transport and P11-01's "no HTTP transport"
line stands. This module makes the remaining work a socket and a loop, not a
format.

No model and no server were started; the gate is pure string comparison.

## Reproduction

    OUT=/home/end0/AI/LlamaServer/exp958-goose-http-response-20261118
    python3 tools/compose-goose.py "$OUT/unit.goose" goose/server/http-response.goose
    <goose> check "$OUT/unit.goose"
    <goose> --standalone -o "$OUT/runtime.c" --header "$OUT/runtime.h" "$OUT/unit.goose"
    gcc-13 -O1 -fPIC -shared -Wl,-z,defs "$OUT/runtime.c" -lm -o "$OUT/libgoose.so"
    g++-13 -O1 -std=c++17 -w -c vendor/flint/vendor/cpp-httplib/httplib.cpp -o "$OUT/httplib.o"
    g++-13 -O1 -I"$OUT" -Ivendor/flint/vendor/cpp-httplib \
        experiments/EXP-2026-11-18-958-goose-http-response/tests/{gate,oracle}.cpp \
        "$OUT"/httplib.o "$OUT"/libgoose.so -Wl,-rpath,"$OUT" -lpthread -o "$OUT/gate"
    "$OUT/gate"
