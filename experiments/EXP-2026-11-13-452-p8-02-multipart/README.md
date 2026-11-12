# EXP452: multipart and path parameters in Goose (P8-02 fifth slice)

Form uploads and path parameters reach the handlers through two small
transformations. This experiment moves them into Goose.

## What was ported

`source/multipart.goose`, from `tools/server/server-http.cpp`
(`decode_path_component`, `get_params`, `server_http_context::post`):

1. **Path-component percent-decoding**: `%HH` is decoded only when both digits
   are hex, everything else is copied literally. There is no `+` -> space
   conversion, because that is query-string semantics and lives in httplib.
2. **Multipart form grouping**: the first occurrence of a text field becomes a
   JSON string, later occurrences of the same key append to an array, comparing
   raw key bytes. File parts are not part of that JSON object; they populate the
   `uploaded_file` map with their filename and content type.
3. **Parameter precedence**: a path parameter overrides a query parameter of the
   same name, and only the path value is percent-decoded.

The multipart **boundary parsing** stays with httplib: that is a vendored
external dependency, not owned code.

## Gate

| corpus | cases |
|---|---|
| hand-written decoding corpus (escapes, invalid escapes, `+`, `%00`, UTF-8, `%ff%fe`, lookalikes) | 54 |
| exhaustive `%XY` over 21 characters inside and just outside the hex ranges | 441 |
| exhaustive form grouping: 5 key shapes over field counts 0..6 | 19 531 |
| parameter precedence | 4 |
| total | **20 030 cases, 132 335 values, 0 failures** |

Independent checks: `check.py` re-decodes all 495 recorded inputs with Python's
`urllib.parse.unquote_to_bytes` (`FG452_DECODE_CHECK entries=495
python_unquote_match=yes`), and the form grouping is compared against a plain
`strcmp` reference, so neither arm reuses the Goose byte-offset logic.

ASan + UBSan: 0 failures, rc 0.

Mutations 6/6 caught: hex digits accept one extra character (24 failures),
decoded escape consumes two bytes (127), invalid escape is decoded anyway (360),
repeated form keys never group (41 955), key comparison ignores bytes (9 702),
path parameter loses to the query value (2).

Two of these mutations were initially missed and drove test additions rather
than being waived: the hex-range mutation was invisible until the exhaustive
`%XY` corpus existed, and the byte-comparison mutation needed a corrected
indentation anchor.

## What stays native

- httplib multipart boundary parsing, chunked body assembly and the socket
  transport;
- the `uploaded_file` container and the JSON document construction;
- handler binding and the routing table wiring (the table itself is EXP449).

P8-02 stays IN_PROGRESS on those items, plus streaming, GCP AIP routes, the
`/props` payload and value-level validation.

## Reproduce

    bash experiments/EXP-2026-11-13-452-p8-02-multipart/build_fixture.sh
    python3 experiments/EXP-2026-11-13-452-p8-02-multipart/mutate.py
