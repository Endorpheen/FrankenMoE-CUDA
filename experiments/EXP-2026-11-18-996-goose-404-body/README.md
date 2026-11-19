# EXP-2026-11-18-996: A 404 carries the native body

Status: the error handler's document is wired and measured on a live socket. 144
modules, 38 295 checks.

## What was wrong

Every non-200 answered with `Content-Length: 0`. Native does not: its error handler
sets a JSON document for a 404.

    server-http.cpp:145-156
      if (res.status == 404)
        {"error":{"message":"File Not Found","type":"not_found_error","code":404}}
        Content-Type: application/json; charset=utf-8

`goose/server/limits.goose` already owns that decision - `error_404` returns whether
the handler owns the body - and the module was in **no composed unit**. Same class as
EXP995's `shutdown-policy`: a decision present in the tree and unreachable, while the
listener answered with an empty body.

## What was done

- `limits.goose` joined the unit. Its private helpers are `lm_put` / `lm_st_i64`,
  because `put` and `st_i64` collided with other modules - the same reason
  `shutdown-policy` needed renaming.
- **It also lost its extern.** The module wrote through a host store
  (`extern "fg471_st_i64"`, `out: u64`). A Goose caller cannot take the address of a
  local array, so the call did not typecheck from the listener. The `out` parameter is
  an `i64[:]` now: a slice a Goose caller owns. The extern, the host binding and three
  lines of adapter debt are gone, which is strictly better than adding a binding.
- The listener asks `error_404` instead of writing a condition, and emits the document
  and its content type when the decision says so. A 200 is unchanged.

The document lives in one `const u8[:]`; the length comes from `HL_NOT_FOUND_DOC.len`,
so the header and the body cannot drift apart.

## Evidence

Live server, ephemeral port:

    $ printf 'GET /nope HTTP/1.1\r\nHost: x\r\n\r\n' | nc 127.0.0.1 $PORT
    HTTP/1.1 404 Not Found
    Content-Length: 74
    Content-Type: application/json; charset=utf-8

    {"error":{"message":"File Not Found","type":"not_found_error","code":404}}

`Content-Length: 74` matches the document byte for byte, and 74 was *measured* - the
first draft of the constant said 70 and would have truncated the document.

| check | result |
| --- | --- |
| http-listen, 7 new 404 checks | **183**, 0 failures |
| whole suite | **38 295**, 0 failures (10 tests) |
| unit | 144 modules, 1169 globals, 0 duplicates, 147 extern |
| adapter debt | unchanged by this change (a binding was removed, not added) |

## An assertion that was wrong, found by making it fail

The gate contained

    check(got.find("Content-Length: 0\r\n") != std::string::npos, "404 has no body");

It asserted the defect. It is now the opposite: a 404 must **not** claim an empty body.
That test was green for as long as the listener was wrong, which is the same failure
shape as EXP992's absence check - a test that encodes the current behaviour instead of
the required one.

## Not claimed

Other error codes keep an empty body, because native leaves them to whatever wrote the
response (`res->error()`), and the standalone has no handlers to write one yet.

`request-errors.goose` (exception class to error type) and `required-fields.goose` are
still outside the unit: both need a parsed request body or an exception path, neither of
which the standalone has.

No model was started, no benchmark was run.
