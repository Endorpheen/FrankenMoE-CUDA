# EXP-2026-11-18-992: A request body belongs to its own request

Status: real defect fixed, demonstrated by running the same gate against the modules
before and after. 38 274 checks, 0 failures.

## The bug

Nothing consumed a request body. `hl_serve_keep` answered the headers and then shifted
the remainder to the front of the buffer without knowing that the remainder began with
a body. The client's body therefore became the next request line.

Two symptoms, both silent:

- a POST with a body in the same write as its headers produced a wrong answer to the
  **next** request, which the client had sent correctly;
- `Content-Length` was never parsed, so `Content-Length: abc` was accepted and
  `Transfer-Encoding` was ignored while the connection stayed open on a body this
  listener cannot frame.

## What was done

`goose/server/http-request.goose` owns two new decisions:

- `rq_decimal` - a Content-Length is decimal digits and nothing else; `0` accept,
  `-1` reject, with an overflow stop rather than a wrap. "No body" and "a body I
  cannot measure" stay different values, because treating the second as the first is
  exactly the failure being fixed.
- `rq_body_rule` - RFC 9112 §6.1 as `httplib.cpp:10241-10243` applies it: a
  Transfer-Encoding closes the connection after the response. This listener cannot
  frame chunked bodies, so closing is also the only honest answer.

`goose/server/http-listen.goose` captures `Content-Length` and `Transfer-Encoding` in
the header scan it already runs, publishes a two-word contract to the caller
(`meta[0]` body length, `meta[1]` close flag), and `hl_serve_keep` consumes the body
before looking for the next request: bytes already in the buffer are skipped and the
tail is shifted, the rest is read and discarded through the existing fill primitive.
A short body - the client promised more than it sent - ends the loop rather than
framing bytes that are not a request.

The body is read, not stored: there is no consumer for it yet, and a buffer nobody
reads would be scaffolding.

## Evidence, before and after

The same gate against the modules at `HEAD` and against the fixed modules:

    pre-fix unit :  BAD checks=169 failures=5
    fixed unit   :  PASS checks=169 failures=0

The five pre-fix failures:

    both pipeline answers are 200, so the body became no request
    the drained body left both answers intact
    a Content-Length that is not a number is refused
    and the connection closes, because the body cannot be measured
    a Transfer-Encoding closes the connection after the answer

## A check that proved nothing, twice

The first version of the body checks asserted `served == 2` and the **absence** of
`400 Bad Request`. Both passed against the pre-fix unit, so they demonstrated nothing.
The body had produced a wrong answer that was not a 400, and an absence cannot see
that. Counting the `200 OK` answers made the check discriminate: pre-fix one of the
two answers is not 200.

This is the second time in three cycles that a check asserting an absence was the
weak one - EXP987's gate asked whether a header was present and never whether another
should be absent, and that is exactly how the preflight-headers defect survived.

## Result

| gate | checks | result |
| --- | --- | --- |
| http-listen (+20, 4 new scenarios) | 169 | PASS |
| whole suite | **38 274** | **0 failures** (9 tests, 9 gates) |

Scenarios: body in the same write as the headers followed by a pipelined GET; a
separate body drained before the next request; `Content-Length: abc` refused with the
connection closed; `Transfer-Encoding: chunked` answered with `Connection: close`.

## Not claimed

The body is discarded, so POST endpoints still answer as if the body were empty. A
body larger than the caller's buffer ends the connection instead of being streamed -
a bound, not a behaviour. No TLS.

No model was started and no benchmark was run.
