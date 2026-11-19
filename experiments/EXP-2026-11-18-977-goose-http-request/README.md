# EXP-2026-11-18-977: HTTP request parsing, in Goose

Status: differential PASS, 566 checks against httplib's own validators. This is the
half of the transport that needs no socket.

## Hypothesis fixed before changes

EXP958 ported the response bytes. The request side had nothing: a listener would
have had to parse a request line and headers in C++ before Goose could route it.
If the parsing rules are exactly httplib's own `parse_request_line`
and `fields::*` validators - then porting them leaves the transport gap as a socket
and a loop, with no format work left on either side.

## What was done

`goose/server/http-request.goose`:

- `rq_is_token_char`, `rq_is_token` - RFC 9110 token: ASCII alphanumeric plus
  `!#$%&'*+-.^_`|~`;
- `rq_is_field_vchar`, `rq_is_field_content`, `rq_is_field_valid` - field-content,
  where an empty value is valid, otherwise it starts and ends with a visible
  character and may contain SP, HTAB and obs-text between;
- `rq_parse_line` - the request line **including CRLF**, with native's exact rules:
  exactly two spaces (other whitespace is not a separator), non-empty method,
  target and version, `sp1 != 0`, `sp2 != sp1 + 1`, `sp2 + 1 != end`, version
  `HTTP/1.1` or `HTTP/1.0`, target visible/obs-text only and no `#`;
- `rq_parse_header` - one header line including CRLF, split at the first colon, OWS
  trimmed from the value, then the same field-validity rule.

## Result

| check | result |
| --- | --- |
| gate | **566 checks, 0 failures** |
| token rule | all 256 single-byte strings, against `httplib::detail::fields::is_token` |
| field value | nine hostile values including `""`, `" "`, `"a\tb"`, a DEL byte |
| request line | 15 lines: valid forms, double space, trailing space, leading space, HTTP/2.0, HTAB separator, fragment, missing CRLF, LF only, three spaces, no method |
| header lines | seven lines including no colon, empty name, space in name, control byte in value |
| oracle | `httplib.cpp` linked, calling the real validators |

The request-line arm compares against a transcription of `parse_request_line`
(9081-9098), which is eight lines of rules and therefore transcribable exactly; the
span offsets are compared as slices of the input, so a shifted split fails.

## Not claimed

There is still no listener. What is missing on the transport is now only the socket,
the read loop and TLS - the request bytes and the response bytes are both decided in
Goose. P11-01's "no HTTP transport" line stands unchanged.

The module is in the canonical unit but nothing calls it: the shipping server still
parses with httplib. Wiring it needs a listener, which is what blocker 6 prevents.

No model and no server were started.

## Reproduction

    python3 tools/compose-goose.py OUT/unit.goose goose/server/http-request.goose
    <goose> check OUT/unit.goose
    <goose> --standalone -o OUT/runtime.c --header OUT/runtime.h OUT/unit.goose
    gcc-13 -O1 -fPIC -shared -Wl,-z,defs OUT/runtime.c -lm -o OUT/lib.so
    g++-13 -O1 -I OUT -Ivendor/flint/vendor/cpp-httplib \
        tests/gate.cpp tests/oracle.cpp <httplib.o> OUT/lib.so -lpthread -o gate
    OUT/gate
